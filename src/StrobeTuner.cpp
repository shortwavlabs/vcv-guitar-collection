#include "StrobeTuner.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr float kStrobeLockCents = 0.1f;
constexpr float kTrackInsetX = 10.f;
constexpr float kTrackTop = 53.f;
constexpr float kTrackHeight = 12.f;
constexpr float kTrackGap = 4.f;
constexpr int kTrackCount = 5;
constexpr float kTrackAreaHeight = kTrackCount * kTrackHeight + (kTrackCount - 1) * kTrackGap;
constexpr float kTrackArcDepth = 2.4f;
constexpr float kScaleY = 44.f;
constexpr float kBottomReadoutY = 151.f;

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

float strobeTrackArcOffset(float normalizedX, float depth = kTrackArcDepth) {
    const float x = normalizedX * 2.f - 1.f;
    return depth * (1.f - x * x);
}

void strobeTrackPath(NVGcontext* vg, float x, float y, float width, float height, float depth = kTrackArcDepth) {
    const float c1 = x + width * 0.28f;
    const float c2 = x + width * 0.72f;
    const float x2 = x + width;

    nvgBeginPath(vg);
    nvgMoveTo(vg, x, y);
    nvgBezierTo(vg, c1, y + depth, c2, y + depth, x2, y);
    nvgLineTo(vg, x2, y + height);
    nvgBezierTo(vg, c2, y + height + depth, c1, y + height + depth, x, y + height);
    nvgClosePath(vg);
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
    const float trackW = w - 2.f * kTrackInsetX;

    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0.f, 0.f, w, h, 5.f);
    nvgFillColor(args.vg, nvgRGB(5, 6, 5));
    nvgFill(args.vg);

    NVGpaint bezel = nvgLinearGradient(
        args.vg,
        0.f, 0.f,
        0.f, h,
        nvgRGBA(52, 57, 46, 255),
        nvgRGBA(15, 20, 15, 255));
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 1.f, 1.f, w - 2.f, h - 2.f, 4.5f);
    nvgFillPaint(args.vg, bezel);
    nvgFill(args.vg);

    NVGpaint glass = nvgLinearGradient(
        args.vg,
        0.f, 4.f,
        0.f, h - 4.f,
        nvgRGBA(33, 41, 34, 255),
        nvgRGBA(9, 12, 10, 255));
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 5.f, 5.f, w - 10.f, h - 10.f, 3.5f);
    nvgFillPaint(args.vg, glass);
    nvgFill(args.vg);

    NVGpaint warmGlass = nvgRadialGradient(
        args.vg,
        w * 0.5f, kTrackTop + kTrackAreaHeight * 0.45f,
        8.f, w * 0.72f,
        nvgRGBA(247, 150, 82, 34),
        nvgRGBA(0, 0, 0, 122));
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 5.f, 5.f, w - 10.f, h - 10.f, 3.5f);
    nvgFillPaint(args.vg, warmGlass);
    nvgFill(args.vg);

    // Recessed strobe aperture.
    const float apertureY = kTrackTop - 6.f;
    const float apertureH = kTrackAreaHeight + 12.f;
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 7.f, apertureY, w - 14.f, apertureH, 2.5f);
    nvgFillColor(args.vg, nvgRGBA(3, 4, 3, 210));
    nvgFill(args.vg);

    for (int i = 0; i < kTrackCount; ++i) {
        const float y = kTrackTop + i * (kTrackHeight + kTrackGap);
        NVGpaint trough = nvgLinearGradient(
            args.vg,
            0.f, y,
            0.f, y + kTrackHeight,
            nvgRGBA(23, 14, 9, 235),
            nvgRGBA(7, 5, 4, 235));
        strobeTrackPath(args.vg, kTrackInsetX, y, trackW, kTrackHeight);
        nvgFillPaint(args.vg, trough);
        nvgFill(args.vg);

        strobeTrackPath(args.vg, kTrackInsetX + 0.5f, y + 0.5f, trackW - 1.f, kTrackHeight - 1.f);
        nvgStrokeColor(args.vg, nvgRGBA(180, 105, 58, 58));
        nvgStrokeWidth(args.vg, 0.7f);
        nvgStroke(args.vg);
    }

    const float centerX = w * 0.5f;
    nvgBeginPath(args.vg);
    nvgRect(args.vg, centerX - 0.45f, apertureY - 2.f, 0.9f, apertureH + 4.f);
    nvgFillColor(args.vg, nvgRGBA(255, 223, 180, 56));
    nvgFill(args.vg);

    for (int i = 0; i <= 10; ++i) {
        const float t = static_cast<float>(i) / 10.f;
        const float x = kTrackInsetX + t * trackW;
        const bool major = (i == 0 || i == 5 || i == 10);
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, x, kScaleY + (major ? 2.f : 4.f));
        nvgLineTo(args.vg, x, kScaleY + 8.f);
        nvgStrokeColor(args.vg, nvgRGBA(220, 160, 105, major ? 118 : 64));
        nvgStrokeWidth(args.vg, major ? 0.9f : 0.55f);
        nvgStroke(args.vg);
    }

    NVGpaint bevel = nvgLinearGradient(
        args.vg,
        0.f, 0.f,
        0.f, h,
        nvgRGBA(255, 255, 255, 44),
        nvgRGBA(0, 0, 0, 135));
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 5.5f, 5.5f, w - 11.f, h - 11.f, 3.f);
    nvgStrokeWidth(args.vg, 0.8f);
    nvgStrokeColor(args.vg, nvgRGBA(210, 170, 130, 95));
    nvgStroke(args.vg);

    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 6.f, 6.f, w - 12.f, h - 12.f, 3.f);
    nvgFillPaint(args.vg, bevel);
    nvgFill(args.vg);
}

void StrobeTunerDisplay::drawStripes(const DrawArgs& args) {
    bool valid = false;
    float phaseCycles = 0.f;
    float cents = 0.f;
    float confidence = 0.f;
    if (module) {
        valid = module->uiPitchValid.load(std::memory_order_relaxed);
        phaseCycles = module->uiPhaseCycles.load(std::memory_order_relaxed);
        cents = module->uiCents.load(std::memory_order_relaxed);
        confidence = swv::compat::clamp(module->uiConfidence.load(std::memory_order_relaxed), 0.f, 1.f);
    }

    const float w = box.size.x;
    const float h = box.size.y;
    const float trackW = w - 2.f * kTrackInsetX;
    const float centerX = w * 0.5f;
    const float absCents = std::fabs(cents);
    const bool locked = valid && absCents <= kStrobeLockCents;
    const float glow = valid ? (0.38f + 0.62f * confidence) : 0.16f;
    const int stripeR = locked ? 160 : 255;
    const int stripeG = locked ? 255 : 180;
    const int stripeB = locked ? 142 : 86;

    nvgSave(args.vg);
    nvgScissor(args.vg, kTrackInsetX, kTrackTop - 1.f, trackW, kTrackAreaHeight + kTrackArcDepth + 2.f);

    const float periods[kTrackCount] = {10.8f, 9.4f, 8.3f, 7.2f, 6.3f};
    for (int row = 0; row < kTrackCount; ++row) {
        const float y = kTrackTop + row * (kTrackHeight + kTrackGap);
        const float period = periods[row];
        const float stripeW = period * 0.44f;
        const float rowPhase = static_cast<float>(row) * 0.173f;
        float offset = std::fmod((phaseCycles + rowPhase) * period, period);
        if (offset < 0.f) {
            offset += period;
        }

        for (float x = kTrackInsetX - period * 2.f + offset; x < kTrackInsetX + trackW + period; x += period) {
            const float alpha = swv::compat::clamp((valid ? 168.f : 76.f) * glow, 18.f, 228.f);
            const float stripeCenterX = x + stripeW * 0.5f;
            const float normalizedX = swv::compat::clamp((stripeCenterX - kTrackInsetX) / trackW, 0.f, 1.f);
            const float stripeY = y + strobeTrackArcOffset(normalizedX);
            NVGpaint stripe = nvgLinearGradient(
                args.vg,
                x, stripeY,
                x, stripeY + kTrackHeight,
                nvgRGBA(stripeR, stripeG, stripeB, static_cast<unsigned char>(alpha)),
                nvgRGBA(stripeR, stripeG, stripeB, static_cast<unsigned char>(alpha * 0.35f)));

            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, x, stripeY + 1.2f, stripeW, kTrackHeight - 2.4f, 1.0f);
            nvgFillPaint(args.vg, stripe);
            nvgFill(args.vg);

            nvgBeginPath(args.vg);
            nvgRect(args.vg, x + stripeW * 0.18f, stripeY + 1.8f, std::max(0.8f, stripeW * 0.18f), kTrackHeight - 3.6f);
            nvgFillColor(args.vg, nvgRGBA(255, 244, 205, static_cast<unsigned char>(alpha * 0.46f)));
            nvgFill(args.vg);
        }
    }

    nvgResetScissor(args.vg);

    const float hairlineAlpha = locked ? 220.f : 150.f;
    nvgBeginPath(args.vg);
    nvgRect(args.vg, centerX - 0.6f, kTrackTop - 8.f, 1.2f, kTrackAreaHeight + 16.f);
    nvgFillColor(args.vg, locked
        ? nvgRGBA(155, 255, 166, static_cast<unsigned char>(hairlineAlpha))
        : nvgRGBA(255, 204, 140, static_cast<unsigned char>(hairlineAlpha)));
    nvgFill(args.vg);

    nvgBeginPath(args.vg);
    nvgCircle(args.vg, centerX, kTrackTop + kTrackAreaHeight * 0.5f, locked ? 4.2f : 3.2f);
    nvgFillColor(args.vg, locked
        ? nvgRGBA(128, 255, 150, 85)
        : nvgRGBA(255, 172, 96, 48));
    nvgFill(args.vg);

    NVGpaint edgeShade = nvgBoxGradient(
        args.vg,
        7.f, kTrackTop - 6.f,
        w - 14.f, kTrackAreaHeight + 12.f,
        2.f, 15.f,
        nvgRGBA(0, 0, 0, 0),
        nvgRGBA(0, 0, 0, 165));
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 7.f, kTrackTop - 6.f, w - 14.f, kTrackAreaHeight + 12.f, 2.5f);
    nvgFillPaint(args.vg, edgeShade);
    nvgFill(args.vg);

    const float pointerT = valid ? swv::compat::clamp((cents + 50.f) / 100.f, 0.f, 1.f) : 0.5f;
    const float pointerX = kTrackInsetX + pointerT * trackW;
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, pointerX, kScaleY + 9.f);
    nvgLineTo(args.vg, pointerX - 3.5f, kScaleY + 14.f);
    nvgLineTo(args.vg, pointerX + 3.5f, kScaleY + 14.f);
    nvgClosePath(args.vg);
    nvgFillColor(args.vg, locked
        ? nvgRGBA(140, 255, 160, 218)
        : nvgRGBA(255, 184, 98, valid ? 212 : 80));
    nvgFill(args.vg);

    NVGpaint reflection = nvgLinearGradient(
        args.vg,
        0.f, 8.f,
        0.f, h * 0.54f,
        nvgRGBA(255, 255, 255, 45),
        nvgRGBA(255, 255, 255, 0));
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 8.f, 7.f, w - 16.f, h * 0.34f, 2.5f);
    nvgFillPaint(args.vg, reflection);
    nvgFill(args.vg);

    nvgRestore(args.vg);
}

void StrobeTunerDisplay::drawReadout(const DrawArgs& args) {
    const float w = box.size.x;

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

    const float absCents = std::fabs(cents);
    const bool locked = valid && absCents <= kStrobeLockCents;

    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFontSize(args.vg, 30.f);
    nvgFillColor(args.vg, valid
        ? (locked ? nvgRGB(180, 255, 178) : nvgRGB(255, 190, 118))
        : nvgRGB(116, 95, 78));
    std::string noteText = valid ? formatMidiNote(midi) : "--";
    nvgText(args.vg, w * 0.5f, 23.f, noteText.c_str(), nullptr);

    char centsText[32];
    if (valid) {
        std::snprintf(centsText, sizeof(centsText), "%+.2fc", static_cast<double>(cents));
    } else {
        std::snprintf(centsText, sizeof(centsText), "--.-c");
    }
    nvgFontSize(args.vg, 11.f);
    nvgFillColor(args.vg, valid ? nvgRGB(231, 175, 119) : nvgRGB(112, 89, 72));
    nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgText(args.vg, w - 12.f, 24.f, centsText, nullptr);

    nvgFontSize(args.vg, 7.f);
    nvgFillColor(args.vg, nvgRGBA(218, 160, 104, 165));
    nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(args.vg, kTrackInsetX, kScaleY - 3.f, "FLAT", nullptr);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(args.vg, w * 0.5f, kScaleY - 3.f, "0", nullptr);
    nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgText(args.vg, w - kTrackInsetX, kScaleY - 3.f, "SHARP", nullptr);

    char freqText[32];
    if (valid) {
        std::snprintf(freqText, sizeof(freqText), "%.2f Hz", static_cast<double>(frequency));
    } else {
        std::snprintf(freqText, sizeof(freqText), "--.-- Hz");
    }
    nvgFontSize(args.vg, 10.f);
    nvgFillColor(args.vg, valid ? nvgRGB(170, 128, 91) : nvgRGB(91, 74, 62));
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(args.vg, w * 0.5f, kBottomReadoutY, freqText, nullptr);

    const char* statusText = "NO SIGNAL";
    if (valid) {
        if (locked) {
            statusText = "LOCK";
        } else {
            statusText = (cents < 0.f) ? "FLAT" : "SHARP";
        }
    }
    nvgFontSize(args.vg, 7.5f);
    nvgFillColor(args.vg, locked
        ? nvgRGBA(158, 255, 168, 210)
        : nvgRGBA(225, 155, 95, valid ? 188 : 92));
    nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgText(args.vg, w - 10.f, kBottomReadoutY, statusText, nullptr);

    // Small confidence meter, styled like hardware signal-strength lamps.
    const float meterX = 10.f;
    const float meterY = kBottomReadoutY - 4.f;
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
