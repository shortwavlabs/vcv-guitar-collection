#include "StrobeTuner.hpp"

#include <cmath>
#include <cstdio>

namespace {
constexpr float kStrobeLockCents = 0.1f;

const char* kNoteNames[12] = {
    "C", "C#", "D", "D#", "E", "F",
    "F#", "G", "G#", "A", "A#", "B"
};

std::string formatMidiNote(int midiNote) {
    const int noteIndex = ((midiNote % 12) + 12) % 12;
    const int octave = midiNote / 12 - 1;

    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%s%d", kNoteNames[noteIndex], octave);
    return std::string(buffer);
}
} // namespace

StrobeTuner::StrobeTuner() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

    configParam(A4_REF_PARAM, 430.f, 450.f, 440.f, "A4 Reference", " Hz");
    configParam(SMOOTHING_PARAM, 0.f, 1.f, 0.7f, "Pitch Smoothing");
    configParam(SENSITIVITY_PARAM, 0.f, 1.f, 0.55f, "Tracking Sensitivity");

    configInput(AUDIO_INPUT, "Audio");
    configInput(A4_CV_INPUT, "A4 Reference CV");

    configOutput(THRU_OUTPUT, "Thru");
    configOutput(NOTE_OUTPUT, "Nearest Note (1V/Oct)");
    configOutput(ERROR_OUTPUT, "Tuning Error (10 cents/V)");

    configBypass(AUDIO_INPUT, THRU_OUTPUT);
}

void StrobeTuner::process(const ProcessArgs& args) {
    const float inputVoltage = inputs[AUDIO_INPUT].getVoltage();
    outputs[THRU_OUTPUT].setVoltage(inputVoltage);

    float a4Hz = params[A4_REF_PARAM].getValue();
    if (inputs[A4_CV_INPUT].isConnected()) {
        a4Hz += swv::compat::clamp(inputs[A4_CV_INPUT].getVoltage(), -5.f, 5.f);
    }
    a4Hz = swv::compat::clamp(a4Hz, 430.f, 450.f);

    const float smoothingParam = params[SMOOTHING_PARAM].getValue();
    const float smoothingCoeff = rescale(smoothingParam, 0.f, 1.f, 0.55f, 0.96f);
    if (std::fabs(smoothingCoeff - cachedSmoothing) > 1.0e-4f) {
        pitchDetector.setSmoothing(smoothingCoeff);
        cachedSmoothing = smoothingCoeff;
    }

    const float sensitivityParam = params[SENSITIVITY_PARAM].getValue();
    const float confidenceThreshold = rescale(sensitivityParam, 0.f, 1.f, 0.55f, 0.9f);
    if (std::fabs(confidenceThreshold - cachedConfidenceThreshold) > 1.0e-4f) {
        pitchDetector.setConfidenceThreshold(confidenceThreshold);
        cachedConfidenceThreshold = confidenceThreshold;
    }

    const float minRms = rescale(sensitivityParam, 0.f, 1.f, 0.0035f, 0.0010f);
    if (std::fabs(minRms - cachedMinRms) > 1.0e-6f) {
        pitchDetector.setMinRms(minRms);
        cachedMinRms = minRms;
    }

    StrobeTunerDSP::PitchResult analysisResult;
    const bool hasNewAnalysis = pitchDetector.processSample(inputVoltage / 5.f, analysisResult);
    if (hasNewAnalysis) {
        if (analysisResult.valid) {
            const float estimatedHz = (analysisResult.smoothedFrequencyHz > 0.f)
                ? analysisResult.smoothedFrequencyHz
                : analysisResult.frequencyHz;

            const float midiFloat = StrobeTunerDSP::frequencyToMidi(estimatedHz, a4Hz);
            midiNote = static_cast<int>(std::round(midiFloat));
            const float targetHz = StrobeTunerDSP::midiToFrequency(midiNote, a4Hz);
            centsError = StrobeTunerDSP::centsDifference(estimatedHz, targetHz);
            centsError = swv::compat::clamp(centsError, -50.f, 50.f);

            lastDeltaHz = estimatedHz - targetHz;
            confidence = analysisResult.confidence;
            pitchValid = true;

            uiFrequencyHz.store(estimatedHz, std::memory_order_relaxed);
            uiMidiNote.store(midiNote, std::memory_order_relaxed);
        } else {
            pitchValid = false;
            confidence = analysisResult.confidence;
            lastDeltaHz *= 0.95f;
            centsError *= 0.92f;
        }
    } else if (!pitchValid) {
        // Keep stale states from drifting forever when analysis drops out.
        lastDeltaHz *= 0.995f;
        centsError *= 0.995f;
    }

    phaseCycles += lastDeltaHz * args.sampleTime;
    phaseCycles -= std::floor(phaseCycles);
    if (phaseCycles < 0.f) {
        phaseCycles += 1.f;
    }

    if (pitchValid) {
        outputs[NOTE_OUTPUT].setVoltage(StrobeTunerDSP::midiToRackPitchVoltage(midiNote));
        outputs[ERROR_OUTPUT].setVoltage(swv::compat::clamp(centsError / 10.f, -5.f, 5.f));
    } else {
        outputs[NOTE_OUTPUT].setVoltage(0.f);
        outputs[ERROR_OUTPUT].setVoltage(0.f);
    }

    const float absCents = std::fabs(centsError);
    const bool inTune = pitchValid && absCents <= kStrobeLockCents;

    const float flatBrightness = (pitchValid && centsError < -kStrobeLockCents)
        ? std::min(1.f, absCents / 8.f)
        : 0.f;
    const float sharpBrightness = (pitchValid && centsError > kStrobeLockCents)
        ? std::min(1.f, absCents / 8.f)
        : 0.f;
    const float tuneBrightness = inTune ? 1.f : 0.f;
    const float signalBrightness = pitchValid ? swv::compat::clamp(confidence, 0.f, 1.f) : 0.f;

    lights[FLAT_LIGHT].setBrightnessSmooth(flatBrightness, args.sampleTime * 20.f);
    lights[IN_TUNE_LIGHT].setBrightnessSmooth(tuneBrightness, args.sampleTime * 20.f);
    lights[SHARP_LIGHT].setBrightnessSmooth(sharpBrightness, args.sampleTime * 20.f);
    lights[SIGNAL_LIGHT].setBrightnessSmooth(signalBrightness, args.sampleTime * 20.f);

    uiPhaseCycles.store(phaseCycles, std::memory_order_relaxed);
    uiCents.store(centsError, std::memory_order_relaxed);
    uiConfidence.store(confidence, std::memory_order_relaxed);
    uiPitchValid.store(pitchValid, std::memory_order_relaxed);
}

void StrobeTuner::onSampleRateChange(const SampleRateChangeEvent& e) {
    pitchDetector.setSampleRate(e.sampleRate);
}

void StrobeTuner::onReset(const ResetEvent& e) {
    (void)e;
    pitchDetector.reset();

    phaseCycles = 0.f;
    lastDeltaHz = 0.f;
    centsError = 0.f;
    confidence = 0.f;
    midiNote = 69;
    pitchValid = false;

    uiPhaseCycles.store(0.f, std::memory_order_relaxed);
    uiCents.store(0.f, std::memory_order_relaxed);
    uiConfidence.store(0.f, std::memory_order_relaxed);
    uiFrequencyHz.store(0.f, std::memory_order_relaxed);
    uiMidiNote.store(69, std::memory_order_relaxed);
    uiPitchValid.store(false, std::memory_order_relaxed);
}

StrobeTunerWidget::StrobeTunerWidget(StrobeTuner* module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/STROBE_TUNER_PANEL.svg")));

    const float centerX = box.size.x * 0.5f;

    addChild(createWidget<ScrewSilver>(Vec(0, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    auto* display = new StrobeTunerDisplay();
    display->module = module;
    display->box.pos = Vec(9.f, 34.f);
    display->box.size = Vec(box.size.x - 18.f, 168.f);
    addChild(display);

    addChild(createLightCentered<MediumLight<BlueLight>>(Vec(centerX - 25.f, 214.f), module, StrobeTuner::FLAT_LIGHT));
    addChild(createLightCentered<MediumLight<GreenLight>>(Vec(centerX, 214.f), module, StrobeTuner::IN_TUNE_LIGHT));
    addChild(createLightCentered<MediumLight<RedLight>>(Vec(centerX + 25.f, 214.f), module, StrobeTuner::SHARP_LIGHT));
    addChild(createLightCentered<SmallLight<YellowLight>>(Vec(centerX + 43.f, 214.f), module, StrobeTuner::SIGNAL_LIGHT));

    addParam(createParamCentered<RoundBlackKnob>(Vec(centerX - 30.f, 252.f), module, StrobeTuner::A4_REF_PARAM));
    addParam(createParamCentered<RoundBlackKnob>(Vec(centerX + 30.f, 252.f), module, StrobeTuner::SMOOTHING_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerX, 288.f), module, StrobeTuner::SENSITIVITY_PARAM));

    addInput(createInputCentered<PJ301MPort>(Vec(24.f, 326.f), module, StrobeTuner::AUDIO_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(box.size.x - 24.f, 326.f), module, StrobeTuner::A4_CV_INPUT));

    addOutput(createOutputCentered<PJ301MPort>(Vec(24.f, 360.f), module, StrobeTuner::THRU_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(centerX, 360.f), module, StrobeTuner::NOTE_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(box.size.x - 24.f, 360.f), module, StrobeTuner::ERROR_OUTPUT));
}

void StrobeTunerDisplay::draw(const DrawArgs& args) {
    drawBackground(args);
    drawStripes(args);
    drawReadout(args);
}

void StrobeTunerDisplay::drawBackground(const DrawArgs& args) {
    const float w = box.size.x;
    const float h = box.size.y;
    const float cx = w * 0.5f;
    const float cy = h * 0.62f;
    const float outerRadius = std::min(w, h) * 0.47f;
    const float innerRadius = outerRadius - 24.f;

    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0.f, 0.f, w, h, 4.f);
    nvgFillColor(args.vg, nvgRGB(10, 10, 10));
    nvgFill(args.vg);

    NVGpaint glass = nvgLinearGradient(
        args.vg,
        0.f, 0.f,
        0.f, h,
        nvgRGBA(74, 30, 14, 255),
        nvgRGBA(30, 11, 8, 255));
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 1.0f, 1.0f, w - 2.f, h - 2.f, 4.f);
    nvgFillPaint(args.vg, glass);
    nvgFill(args.vg);

    NVGpaint vignette = nvgRadialGradient(
        args.vg,
        cx, cy,
        innerRadius * 0.2f, outerRadius * 1.2f,
        nvgRGBA(255, 150, 90, 20),
        nvgRGBA(0, 0, 0, 140));
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0.f, 0.f, w, h);
    nvgFillPaint(args.vg, vignette);
    nvgFill(args.vg);

    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0.5f, 0.5f, w - 1.f, h - 1.f, 4.f);
    nvgStrokeWidth(args.vg, 1.1f);
    nvgStrokeColor(args.vg, nvgRGBA(124, 78, 50, 230));
    nvgStroke(args.vg);

    // Arc guides for circular strobe wheel.
    nvgBeginPath(args.vg);
    nvgArc(args.vg, cx, cy, outerRadius, -2.85f, -0.29f, NVG_CW);
    nvgArc(args.vg, cx, cy, innerRadius, -0.29f, -2.85f, NVG_CCW);
    nvgClosePath(args.vg);
    nvgStrokeColor(args.vg, nvgRGBA(210, 130, 84, 80));
    nvgStrokeWidth(args.vg, 0.9f);
    nvgStroke(args.vg);

    for (int i = 0; i < 11; ++i) {
        const float t = static_cast<float>(i) / 10.f;
        const float theta = -2.75f + t * 2.35f;
        const float x0 = cx + std::cos(theta) * (innerRadius - 1.f);
        const float y0 = cy + std::sin(theta) * (innerRadius - 1.f);
        const float x1 = cx + std::cos(theta) * (outerRadius + 1.f);
        const float y1 = cy + std::sin(theta) * (outerRadius + 1.f);

        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, x0, y0);
        nvgLineTo(args.vg, x1, y1);
        nvgStrokeColor(args.vg, nvgRGBA(225, 160, 112, (i % 2 == 0) ? 95 : 62));
        nvgStrokeWidth(args.vg, (i % 2 == 0) ? 1.0f : 0.7f);
        nvgStroke(args.vg);
    }
}

void StrobeTunerDisplay::drawStripes(const DrawArgs& args) {
    if (!module) {
        return;
    }

    const bool valid = module->uiPitchValid.load(std::memory_order_relaxed);
    const float phaseCycles = module->uiPhaseCycles.load(std::memory_order_relaxed);
    const float cents = module->uiCents.load(std::memory_order_relaxed);
    const float confidence = swv::compat::clamp(module->uiConfidence.load(std::memory_order_relaxed), 0.f, 1.f);

    const float w = box.size.x;
    const float h = box.size.y;
    const float cx = w * 0.5f;
    const float cy = h * 0.62f;
    const float ringRadius = std::min(w, h) * 0.43f;
    const float absCents = std::fabs(cents);
    const bool locked = valid && absCents <= kStrobeLockCents;
    const float spinAngle = phaseCycles * 2.f * static_cast<float>(M_PI);

    nvgSave(args.vg);
    nvgScissor(args.vg, 2.f, 18.f, w - 4.f, h - 44.f);

    const int padCount = 18;
    const int barsPerPad = 7;
    const float padWidth = 18.f;
    const float padHeight = 20.f;
    const float baseAlpha = valid ? (90.f + 135.f * confidence) : 28.f;
    const int baseR = locked ? 160 : 255;
    const int baseG = locked ? 255 : 178;
    const int baseB = locked ? 132 : 95;
    const float startAngle = -2.95f;
    const float sweepAngle = 2.70f;

    for (int i = 0; i < padCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(padCount - 1);
        const float theta = startAngle + t * sweepAngle + spinAngle;
        const float x = cx + std::cos(theta) * ringRadius;
        const float y = cy + std::sin(theta) * ringRadius;

        nvgSave(args.vg);
        nvgTranslate(args.vg, x, y);
        nvgRotate(args.vg, theta + static_cast<float>(M_PI) * 0.5f);

        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, -padWidth * 0.5f, -padHeight * 0.5f, padWidth, padHeight, 1.8f);
        nvgFillColor(args.vg, nvgRGBA(52, 24, 14, static_cast<unsigned char>(baseAlpha * 0.35f)));
        nvgFill(args.vg);

        for (int b = 0; b < barsPerPad; ++b) {
            const float barT = static_cast<float>(b) / static_cast<float>(barsPerPad - 1);
            const float barX = -padWidth * 0.5f + 1.5f + barT * (padWidth - 3.0f);
            const float barAlpha = baseAlpha * (0.55f + 0.45f * std::sin(spinAngle * 1.8f + i * 0.32f + b * 0.41f));
            nvgBeginPath(args.vg);
            nvgRect(args.vg, barX, -padHeight * 0.42f, 1.0f, padHeight * 0.84f);
            nvgFillColor(args.vg, nvgRGBA(baseR, baseG, baseB, static_cast<unsigned char>(swv::compat::clamp(barAlpha, 0.f, 255.f))));
            nvgFill(args.vg);
        }

        nvgRestore(args.vg);
    }

    NVGpaint edgeShade = nvgLinearGradient(
        args.vg,
        0.f, 18.f,
        0.f, h - 24.f,
        nvgRGBA(0, 0, 0, 120),
        nvgRGBA(0, 0, 0, 30));
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0.f, 18.f, w, h - 42.f);
    nvgFillPaint(args.vg, edgeShade);
    nvgFill(args.vg);

    nvgResetScissor(args.vg);
    nvgRestore(args.vg);
}

void StrobeTunerDisplay::drawReadout(const DrawArgs& args) {
    const float w = box.size.x;
    const float h = box.size.y;

    std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
    if (!font) {
        return;
    }

    nvgFontFaceId(args.vg, font->handle);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

    bool valid = false;
    float cents = 0.f;
    float confidence = 0.f;
    float frequency = 0.f;
    int midi = 69;
    if (module) {
        valid = module->uiPitchValid.load(std::memory_order_relaxed);
        cents = module->uiCents.load(std::memory_order_relaxed);
        confidence = module->uiConfidence.load(std::memory_order_relaxed);
        frequency = module->uiFrequencyHz.load(std::memory_order_relaxed);
        midi = module->uiMidiNote.load(std::memory_order_relaxed);
    }

    const float noteY = h * 0.62f;

    nvgFontSize(args.vg, 42.f);
    nvgFillColor(args.vg, valid ? nvgRGB(245, 178, 122) : nvgRGB(145, 110, 90));
    std::string noteText = valid ? formatMidiNote(midi) : "--";
    nvgText(args.vg, w * 0.5f, noteY, noteText.c_str(), nullptr);

    char centsText[32];
    if (valid) {
        std::snprintf(centsText, sizeof(centsText), "%+.2f c", static_cast<double>(cents));
    } else {
        std::snprintf(centsText, sizeof(centsText), "--.- c");
    }
    nvgFontSize(args.vg, 14.f);
    nvgFillColor(args.vg, nvgRGB(225, 170, 130));
    nvgText(args.vg, w * 0.5f, noteY + 28.f, centsText, nullptr);

    char freqText[32];
    if (valid) {
        std::snprintf(freqText, sizeof(freqText), "%.2f Hz", static_cast<double>(frequency));
    } else {
        std::snprintf(freqText, sizeof(freqText), "--.-- Hz");
    }
    nvgFontSize(args.vg, 12.f);
    nvgFillColor(args.vg, nvgRGB(170, 125, 95));
    nvgText(args.vg, w * 0.5f, h - 14.f, freqText, nullptr);

    // Small confidence meter (left-bottom), inspired by clip-on hardware indicators.
    const float meterX = 10.f;
    const float meterY = h - 21.f;
    const float meterW = 23.f;
    const float meterH = 8.f;

    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, meterX, meterY, meterW, meterH, 1.5f);
    nvgStrokeColor(args.vg, nvgRGBA(210, 150, 110, 190));
    nvgStrokeWidth(args.vg, 0.9f);
    nvgStroke(args.vg);

    int bars = static_cast<int>(std::round(swv::compat::clamp(confidence, 0.f, 1.f) * 4.f));
    for (int i = 0; i < 4; ++i) {
        const float bx = meterX + 2.f + i * 5.f;
        nvgBeginPath(args.vg);
        nvgRect(args.vg, bx, meterY + 1.8f, 3.4f, meterH - 3.6f);
        if (i < bars && valid) {
            nvgFillColor(args.vg, nvgRGBA(246, 178, 122, 205));
        } else {
            nvgFillColor(args.vg, nvgRGBA(92, 56, 40, 165));
        }
        nvgFill(args.vg);
    }
}

Model* modelStrobeTuner = createModel<StrobeTuner, StrobeTunerWidget>("StrobeTuner");
