#include "Mnemonix.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

float finiteClamp(float value, float minValue, float maxValue) {
    if (!std::isfinite(value)) {
        value = 0.f;
    }
    return clamp(value, minValue, maxValue);
}

float applyCv(float knob, float cv, float amount) {
    return finiteClamp(knob + (cv / 5.f) * amount, 0.f, 1.f);
}

float outputControlVoltage(float value) {
    return finiteClamp(value, 0.f, 1.f) * 10.f;
}

bool isUsableSampleRate(float sampleRate) {
    return std::isfinite(sampleRate) && sampleRate >= 8000.f;
}

float sanitizeControl(float value, float fallback, float minValue, float maxValue) {
    if (!std::isfinite(value)) {
        value = fallback;
    }
    return finiteClamp(value, minValue, maxValue);
}

int clampInt(json_int_t value, int minValue, int maxValue) {
    return static_cast<int>(std::max(static_cast<json_int_t>(minValue), std::min(static_cast<json_int_t>(maxValue), value)));
}

float syncDivisionMultiplier(int division) {
    switch (division) {
        case Mnemonix::SYNC_WHOLE: return 4.f;
        case Mnemonix::SYNC_HALF: return 2.f;
        case Mnemonix::SYNC_DOTTED_QUARTER: return 1.5f;
        case Mnemonix::SYNC_DOTTED_EIGHTH: return 0.75f;
        case Mnemonix::SYNC_EIGHTH: return 0.5f;
        case Mnemonix::SYNC_EIGHTH_TRIPLET: return 1.f / 3.f;
        case Mnemonix::SYNC_SIXTEENTH: return 0.25f;
        case Mnemonix::SYNC_QUARTER:
        default: return 1.f;
    }
}

const char* syncDivisionName(int division) {
    switch (division) {
        case Mnemonix::SYNC_WHOLE: return "1/1";
        case Mnemonix::SYNC_HALF: return "1/2";
        case Mnemonix::SYNC_DOTTED_QUARTER: return "1/4.";
        case Mnemonix::SYNC_DOTTED_EIGHTH: return "1/8.";
        case Mnemonix::SYNC_EIGHTH: return "1/8";
        case Mnemonix::SYNC_EIGHTH_TRIPLET: return "1/8T";
        case Mnemonix::SYNC_SIXTEENTH: return "1/16";
        case Mnemonix::SYNC_QUARTER:
        default: return "1/4";
    }
}

const char* stereoModeName(int mode) {
    switch (mode) {
        case Mnemonix::STEREO_WIDE: return "Wide";
        case Mnemonix::STEREO_PING_PONG: return "Ping-pong";
        case Mnemonix::STEREO_MONO:
        default: return "Mono";
    }
}

struct DelayTimeQuantity : engine::ParamQuantity {
    float getDisplayValue() override {
        bool longDelay = false;
        if (module) {
            Mnemonix* mnemonix = static_cast<Mnemonix*>(module);
            longDelay = mnemonix->params[Mnemonix::LONG_PARAM].getValue() >= 0.5f;
        }
        return MnemonixDSP::delaySecondsForDelay(getValue(), longDelay) * 1000.f;
    }

    void setDisplayValue(float displayValue) override {
        bool longDelay = false;
        if (module) {
            Mnemonix* mnemonix = static_cast<Mnemonix*>(module);
            longDelay = mnemonix->params[Mnemonix::LONG_PARAM].getValue() >= 0.5f;
        }
        setImmediateValue(MnemonixDSP::delayNormForDelayMilliseconds(displayValue, longDelay));
    }

    std::string getDisplayValueString() override {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.1f", getDisplayValue());
        return buffer;
    }
};

} // namespace

Mnemonix::Mnemonix() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, 0);

    configParam(LEVEL_PARAM, 0.f, 1.f, 0.55f, "Level", "%", 0.f, 100.f);
    configParam(BLEND_PARAM, 0.f, 1.f, 0.5f, "Blend", "%", 0.f, 100.f);
    configParam(FEEDBACK_PARAM, 0.f, 1.f, 0.25f, "Feedback", "%", 0.f, 100.f);
    configParam<DelayTimeQuantity>(DELAY_PARAM, 0.f, 1.f, 0.45f, "Delay", " ms");
    configParam(DEPTH_PARAM, 0.f, 1.f, 0.2f, "Depth", "%", 0.f, 100.f);
    configSwitch(MODE_PARAM, 0.f, 1.f, 0.f, "Chorus/Vibrato", {"Chorus", "Vibrato"});
    configSwitch(SHAPE_PARAM, 0.f, 1.f, 0.f, "LFO shape", {"Triangle", "Square"});
    configSwitch(ENGAGE_PARAM, 0.f, 1.f, 1.f, "Effect", {"Bypassed", "Engaged"});
    configSwitch(LONG_PARAM, 0.f, 1.f, 0.f, "Delay range", {"Normal", "Long"});

    configParam(LEVEL_CV_ATT_PARAM, -1.f, 1.f, 0.f, "Level CV amount", "%", 0.f, 100.f);
    configParam(BLEND_CV_ATT_PARAM, -1.f, 1.f, 0.f, "Blend CV amount", "%", 0.f, 100.f);
    configParam(FEEDBACK_CV_ATT_PARAM, -1.f, 1.f, 0.f, "Feedback CV amount", "%", 0.f, 100.f);
    configParam(DELAY_CV_ATT_PARAM, -1.f, 1.f, 0.f, "Delay CV amount", "%", 0.f, 100.f);
    configParam(DEPTH_CV_ATT_PARAM, -1.f, 1.f, 0.f, "Depth CV amount", "%", 0.f, 100.f);

    configInput(AUDIO_INPUT, "Audio");
    configInput(LEVEL_CV_INPUT, "Level CV");
    configInput(BLEND_CV_INPUT, "Blend CV");
    configInput(FEEDBACK_CV_INPUT, "Feedback CV");
    configInput(DELAY_CV_INPUT, "Delay CV");
    configInput(DEPTH_CV_INPUT, "Depth CV");
    configInput(MODE_CV_INPUT, "Chorus/Vibrato Gate");
    configInput(SHAPE_CV_INPUT, "LFO Shape Gate");
    configInput(ENGAGE_CV_INPUT, "Engage Gate");
    configInput(LONG_CV_INPUT, "Long Range Gate");
    configInput(TAP_CV_INPUT, "Tap Tempo Gate");

    configOutput(AUDIO_OUTPUT, "Audio");
    configOutput(DIRECT_OUTPUT, "Direct");
    configOutput(WET_OUTPUT, "Wet");
    configOutput(LEVEL_CV_OUTPUT, "Level CV");
    configOutput(BLEND_CV_OUTPUT, "Blend CV");
    configOutput(FEEDBACK_CV_OUTPUT, "Feedback CV");
    configOutput(DELAY_CV_OUTPUT, "Delay CV");
    configOutput(DEPTH_CV_OUTPUT, "Depth CV");
    configOutput(MODE_GATE_OUTPUT, "Chorus/Vibrato Gate");
    configOutput(SHAPE_LFO_OUTPUT, "LFO");
    configOutput(ENGAGE_GATE_OUTPUT, "Engage Gate");
    configOutput(LONG_GATE_OUTPUT, "Long Range Gate");
    configOutput(CLOCK_GATE_OUTPUT, "BBD clock / 64 gate");
    configOutput(CLOCK_DIV_OUTPUT, "BBD clock / 512 gate");
    configOutput(ENVELOPE_CV_OUTPUT, "Compander envelope CV");

    configBypass(AUDIO_INPUT, AUDIO_OUTPUT);
}

void Mnemonix::process(const ProcessArgs& args) {
    setEngineSampleRate(args.sampleRate);

    tapTimerSeconds = std::min(tapTimerSeconds + args.sampleTime, 8.f);
    if (inputs[TAP_CV_INPUT].isConnected() && tapTrigger.process(inputs[TAP_CV_INPUT].getVoltage(), 0.1f, 2.f)) {
        if (tapTimerSeconds >= 0.08f && tapTimerSeconds <= 4.f) {
            tapTempoSeconds = tapTimerSeconds;
            timingMode = TIMING_SYNC;
        }
        tapTimerSeconds = 0.f;
    }

    MnemonixDSP::Params dspParams;
    dspParams.vibrato = inputs[MODE_CV_INPUT].isConnected()
        ? inputs[MODE_CV_INPUT].getVoltage() >= 1.f
        : params[MODE_PARAM].getValue() >= 0.5f;
    dspParams.squareLfo = inputs[SHAPE_CV_INPUT].isConnected()
        ? inputs[SHAPE_CV_INPUT].getVoltage() >= 1.f
        : params[SHAPE_PARAM].getValue() >= 0.5f;
    dspParams.longDelay = inputs[LONG_CV_INPUT].isConnected()
        ? inputs[LONG_CV_INPUT].getVoltage() >= 1.f
        : params[LONG_PARAM].getValue() >= 0.5f;
    dspParams.engaged = inputs[ENGAGE_CV_INPUT].isConnected()
        ? inputs[ENGAGE_CV_INPUT].getVoltage() >= 1.f
        : params[ENGAGE_PARAM].getValue() >= 0.5f;

    dspParams.level = applyCv(params[LEVEL_PARAM].getValue(),
        inputs[LEVEL_CV_INPUT].isConnected() ? inputs[LEVEL_CV_INPUT].getVoltage() : 0.f,
        params[LEVEL_CV_ATT_PARAM].getValue());
    dspParams.blend = applyCv(params[BLEND_PARAM].getValue(),
        inputs[BLEND_CV_INPUT].isConnected() ? inputs[BLEND_CV_INPUT].getVoltage() : 0.f,
        params[BLEND_CV_ATT_PARAM].getValue());
    dspParams.feedback = applyCv(params[FEEDBACK_PARAM].getValue(),
        inputs[FEEDBACK_CV_INPUT].isConnected() ? inputs[FEEDBACK_CV_INPUT].getVoltage() : 0.f,
        params[FEEDBACK_CV_ATT_PARAM].getValue());
    const float delayCvOffset = inputs[DELAY_CV_INPUT].isConnected()
        ? (inputs[DELAY_CV_INPUT].getVoltage() / 5.f) * params[DELAY_CV_ATT_PARAM].getValue()
        : 0.f;
    const float baseDelay = timingMode == TIMING_SYNC
        ? getSyncedDelayNorm(dspParams.longDelay)
        : params[DELAY_PARAM].getValue();
    dspParams.delay = finiteClamp(baseDelay + delayCvOffset, 0.f, 1.f);
    dspParams.depth = applyCv(params[DEPTH_PARAM].getValue(),
        inputs[DEPTH_CV_INPUT].isConnected() ? inputs[DEPTH_CV_INPUT].getVoltage() : 0.f,
        params[DEPTH_CV_ATT_PARAM].getValue());
    dspParams.artifactProfile = artifactProfile;
    dspParams.inputGainTrim = inputGainTrim;
    dspParams.bbdBiasTrim = bbdBiasTrim;
    dspParams.clockBleedTrim = clockBleedTrim;
    dspParams.companderTrim = companderTrim;
    dspParams.noiseTrim = noiseTrim;
    dspParams.wetMakeupTrim = wetMakeupTrim;
    dspParams.feedbackHeadroomTrim = feedbackHeadroomTrim;

    const int inputChannels = std::max(1, inputs[AUDIO_INPUT].getChannels());
    const bool expandMonoToStereo = inputChannels == 1 && stereoMode != STEREO_MONO && dspParams.engaged;
    const int channels = expandMonoToStereo ? 2 : inputChannels;
    outputs[AUDIO_OUTPUT].setChannels(channels);
    if (outputs[DIRECT_OUTPUT].isConnected()) {
        outputs[DIRECT_OUTPUT].setChannels(channels);
    }
    if (outputs[WET_OUTPUT].isConnected()) {
        outputs[WET_OUTPUT].setChannels(channels);
    }

    float lfoOutput = 0.f;
    float clockGate = 0.f;
    float clockDivGate = 0.f;
    float envelope = 0.f;

    const bool cpuMutedBypass = !dspParams.engaged && bypassBehavior == BYPASS_CPU_MUTE;
    if (cpuMutedBypass && previousEngaged) {
        resetEngines();
    }
    previousEngaged = dspParams.engaged;

    if (cpuMutedBypass) {
        for (int c = 0; c < inputChannels; c += 4) {
            simd::float_4 inVoltage = inputs[AUDIO_INPUT].getPolyVoltageSimd<simd::float_4>(c);
            inVoltage = simd::clamp(inVoltage, -12.f, 12.f);
            outputs[AUDIO_OUTPUT].setVoltageSimd(inVoltage, c);
            if (outputs[DIRECT_OUTPUT].isConnected()) {
                outputs[DIRECT_OUTPUT].setVoltageSimd(inVoltage, c);
            }
            if (outputs[WET_OUTPUT].isConnected()) {
                outputs[WET_OUTPUT].setVoltageSimd(simd::float_4(0.f), c);
            }
        }
        outputs[AUDIO_OUTPUT].setChannels(inputChannels);
        if (outputs[DIRECT_OUTPUT].isConnected()) {
            outputs[DIRECT_OUTPUT].setChannels(inputChannels);
        }
        if (outputs[WET_OUTPUT].isConnected()) {
            outputs[WET_OUTPUT].setChannels(inputChannels);
        }
    }

    for (int c = 0; c < channels; ++c) {
        if (cpuMutedBypass) {
            break;
        }

        MnemonixDSP::Params channelParams = dspParams;
        if (expandMonoToStereo && c == 1) {
            if (stereoMode == STEREO_WIDE) {
                channelParams.delayOffset = 0.018f;
                channelParams.lfoPhaseOffset = 0.25f;
                channelParams.depth = finiteClamp(channelParams.depth * 1.08f, 0.f, 1.f);
            }
            else if (stereoMode == STEREO_PING_PONG) {
                channelParams.delayOffset = 0.085f;
                channelParams.lfoPhaseOffset = 0.5f;
                channelParams.feedback = finiteClamp(channelParams.feedback * 0.92f, 0.f, 1.f);
            }
        }

        const int inputChannel = expandMonoToStereo ? 0 : c;
        const float in = inputs[AUDIO_INPUT].getPolyVoltage(inputChannel) / 5.f;
        const MnemonixDSP::Result result = engines[c].process(in, channelParams);

        const float out = std::isfinite(result.output) ? result.output : 0.f;
        const float direct = std::isfinite(result.direct) ? result.direct : 0.f;
        const float wet = std::isfinite(result.wet) ? result.wet : 0.f;

        outputs[AUDIO_OUTPUT].setVoltage(finiteClamp(out * 5.f, -12.f, 12.f), c);
        if (outputs[DIRECT_OUTPUT].isConnected()) {
            outputs[DIRECT_OUTPUT].setVoltage(finiteClamp(direct * 5.f, -12.f, 12.f), c);
        }
        if (outputs[WET_OUTPUT].isConnected()) {
            outputs[WET_OUTPUT].setVoltage(finiteClamp(wet * 5.f, -12.f, 12.f), c);
        }
        if (c == 0 && std::isfinite(result.lfo)) {
            lfoOutput = result.lfo;
            clockGate = result.clockGate;
            clockDivGate = result.clockDivGate;
            envelope = result.envelope;
        }
    }

    if (outputs[LEVEL_CV_OUTPUT].isConnected()) outputs[LEVEL_CV_OUTPUT].setVoltage(outputControlVoltage(dspParams.level));
    if (outputs[BLEND_CV_OUTPUT].isConnected()) outputs[BLEND_CV_OUTPUT].setVoltage(outputControlVoltage(dspParams.blend));
    if (outputs[FEEDBACK_CV_OUTPUT].isConnected()) outputs[FEEDBACK_CV_OUTPUT].setVoltage(outputControlVoltage(dspParams.feedback));
    if (outputs[DELAY_CV_OUTPUT].isConnected()) outputs[DELAY_CV_OUTPUT].setVoltage(outputControlVoltage(dspParams.delay));
    if (outputs[DEPTH_CV_OUTPUT].isConnected()) outputs[DEPTH_CV_OUTPUT].setVoltage(outputControlVoltage(dspParams.depth));
    if (outputs[MODE_GATE_OUTPUT].isConnected()) outputs[MODE_GATE_OUTPUT].setVoltage(dspParams.vibrato ? 10.f : 0.f);
    if (outputs[SHAPE_LFO_OUTPUT].isConnected()) outputs[SHAPE_LFO_OUTPUT].setVoltage(finiteClamp(lfoOutput * 5.f, -5.f, 5.f));
    if (outputs[ENGAGE_GATE_OUTPUT].isConnected()) outputs[ENGAGE_GATE_OUTPUT].setVoltage(dspParams.engaged ? 10.f : 0.f);
    if (outputs[LONG_GATE_OUTPUT].isConnected()) outputs[LONG_GATE_OUTPUT].setVoltage(dspParams.longDelay ? 10.f : 0.f);
    if (outputs[CLOCK_GATE_OUTPUT].isConnected()) outputs[CLOCK_GATE_OUTPUT].setVoltage(clockGate >= 0.5f ? 10.f : 0.f);
    if (outputs[CLOCK_DIV_OUTPUT].isConnected()) outputs[CLOCK_DIV_OUTPUT].setVoltage(clockDivGate >= 0.5f ? 10.f : 0.f);
    if (outputs[ENVELOPE_CV_OUTPUT].isConnected()) outputs[ENVELOPE_CV_OUTPUT].setVoltage(outputControlVoltage(envelope));

}

void Mnemonix::onSampleRateChange(const SampleRateChangeEvent& e) {
    setEngineSampleRate(e.sampleRate);
}

json_t* Mnemonix::dataToJson() {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "artifactProfile", json_integer(artifactProfile));
    json_object_set_new(rootJ, "bypassBehavior", json_integer(bypassBehavior));
    json_object_set_new(rootJ, "timingMode", json_integer(timingMode));
    json_object_set_new(rootJ, "syncDivision", json_integer(syncDivision));
    json_object_set_new(rootJ, "stereoMode", json_integer(stereoMode));
    json_object_set_new(rootJ, "tapTempoSeconds", json_real(tapTempoSeconds));
    json_object_set_new(rootJ, "inputGainTrim", json_real(inputGainTrim));
    json_object_set_new(rootJ, "bbdBiasTrim", json_real(bbdBiasTrim));
    json_object_set_new(rootJ, "clockBleedTrim", json_real(clockBleedTrim));
    json_object_set_new(rootJ, "companderTrim", json_real(companderTrim));
    json_object_set_new(rootJ, "noiseTrim", json_real(noiseTrim));
    json_object_set_new(rootJ, "wetMakeupTrim", json_real(wetMakeupTrim));
    json_object_set_new(rootJ, "feedbackHeadroomTrim", json_real(feedbackHeadroomTrim));
    return rootJ;
}

void Mnemonix::dataFromJson(json_t* rootJ) {
    json_t* artifactJ = json_object_get(rootJ, "artifactProfile");
    if (artifactJ && json_is_integer(artifactJ)) {
        setArtifactProfile(static_cast<int>(json_integer_value(artifactJ)));
    }
    json_t* bypassJ = json_object_get(rootJ, "bypassBehavior");
    if (bypassJ && json_is_integer(bypassJ)) {
        bypassBehavior = clampInt(json_integer_value(bypassJ), BYPASS_TRAILS, BYPASS_CPU_MUTE);
    }
    json_t* timingJ = json_object_get(rootJ, "timingMode");
    if (timingJ && json_is_integer(timingJ)) {
        timingMode = clampInt(json_integer_value(timingJ), TIMING_FREE, TIMING_SYNC);
    }
    json_t* divisionJ = json_object_get(rootJ, "syncDivision");
    if (divisionJ && json_is_integer(divisionJ)) {
        syncDivision = clampInt(json_integer_value(divisionJ), 0, SYNC_DIVISIONS_LEN - 1);
    }
    json_t* stereoJ = json_object_get(rootJ, "stereoMode");
    if (stereoJ && json_is_integer(stereoJ)) {
        stereoMode = clampInt(json_integer_value(stereoJ), STEREO_MONO, STEREO_PING_PONG);
    }
    json_t* tapJ = json_object_get(rootJ, "tapTempoSeconds");
    if (tapJ && json_is_number(tapJ)) {
        tapTempoSeconds = sanitizeControl(json_number_value(tapJ), 0.5f, 0.08f, 4.f);
    }
    json_t* inputGainJ = json_object_get(rootJ, "inputGainTrim");
    if (inputGainJ && json_is_number(inputGainJ)) {
        inputGainTrim = sanitizeControl(json_number_value(inputGainJ), 1.f, 0.5f, 1.5f);
    }
    json_t* biasJ = json_object_get(rootJ, "bbdBiasTrim");
    if (biasJ && json_is_number(biasJ)) {
        bbdBiasTrim = sanitizeControl(json_number_value(biasJ), 1.f, 0.f, 2.f);
    }
    json_t* bleedJ = json_object_get(rootJ, "clockBleedTrim");
    if (bleedJ && json_is_number(bleedJ)) {
        clockBleedTrim = sanitizeControl(json_number_value(bleedJ), 1.f, 0.f, 2.5f);
    }
    json_t* companderJ = json_object_get(rootJ, "companderTrim");
    if (companderJ && json_is_number(companderJ)) {
        companderTrim = sanitizeControl(json_number_value(companderJ), 1.f, 0.65f, 1.65f);
    }
    json_t* noiseJ = json_object_get(rootJ, "noiseTrim");
    if (noiseJ && json_is_number(noiseJ)) {
        noiseTrim = sanitizeControl(json_number_value(noiseJ), 1.f, 0.f, 2.5f);
    }
    json_t* wetJ = json_object_get(rootJ, "wetMakeupTrim");
    if (wetJ && json_is_number(wetJ)) {
        wetMakeupTrim = sanitizeControl(json_number_value(wetJ), 1.f, 0.5f, 1.5f);
    }
    json_t* headroomJ = json_object_get(rootJ, "feedbackHeadroomTrim");
    if (headroomJ && json_is_number(headroomJ)) {
        feedbackHeadroomTrim = sanitizeControl(json_number_value(headroomJ), 1.f, 0.6f, 1.8f);
    }
}

void Mnemonix::resetEngines() {
    for (auto& engine : engines) {
        engine.reset();
    }
}

void Mnemonix::setArtifactProfile(int profile) {
    if (profile < MnemonixDSP::ARTIFACT_CLEAN || profile > MnemonixDSP::ARTIFACT_WORN) {
        profile = MnemonixDSP::ARTIFACT_AUTHENTIC;
    }
    artifactProfile = profile;
}

void Mnemonix::setEngineSampleRate(float sampleRate) {
    if (!isUsableSampleRate(sampleRate)) {
        return;
    }

    if (sampleRateInitialized && std::fabs(sampleRate - currentSampleRate) < 0.01f) {
        return;
    }

    currentSampleRate = sampleRate;
    sampleRateInitialized = true;
    for (auto& engine : engines) {
        engine.setSampleRate(currentSampleRate);
    }
}

float Mnemonix::getSyncedDelayNorm(bool longDelay) const {
    const float delayMs = tapTempoSeconds * syncDivisionMultiplier(syncDivision) * 1000.f;
    return MnemonixDSP::delayNormForDelayMilliseconds(delayMs, longDelay);
}

MnemonixWidget::MnemonixWidget(Mnemonix* module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/MNEMONIX_PANEL.svg")));

    addChild(createWidget<ScrewSilver>(Vec(0, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    const float xs[] = {47.f, 106.f, 165.f, 224.f, 283.f};
    addParam(createParamCentered<Davies1900hLargeBlackKnob>(Vec(xs[0], 55), module, Mnemonix::LEVEL_PARAM));
    addParam(createParamCentered<Davies1900hLargeBlackKnob>(Vec(xs[1], 55), module, Mnemonix::BLEND_PARAM));
    addParam(createParamCentered<Davies1900hLargeBlackKnob>(Vec(xs[2], 55), module, Mnemonix::FEEDBACK_PARAM));
    addParam(createParamCentered<Davies1900hLargeBlackKnob>(Vec(xs[3], 55), module, Mnemonix::DELAY_PARAM));
    addParam(createParamCentered<Davies1900hLargeBlackKnob>(Vec(xs[4], 55), module, Mnemonix::DEPTH_PARAM));

    addParam(createParamCentered<Trimpot>(Vec(xs[0], 125), module, Mnemonix::LEVEL_CV_ATT_PARAM));
    addParam(createParamCentered<Trimpot>(Vec(xs[1], 125), module, Mnemonix::BLEND_CV_ATT_PARAM));
    addParam(createParamCentered<Trimpot>(Vec(xs[2], 125), module, Mnemonix::FEEDBACK_CV_ATT_PARAM));
    addParam(createParamCentered<Trimpot>(Vec(xs[3], 125), module, Mnemonix::DELAY_CV_ATT_PARAM));
    addParam(createParamCentered<Trimpot>(Vec(xs[4], 125), module, Mnemonix::DEPTH_CV_ATT_PARAM));

    addInput(createInputCentered<PJ301MPort>(Vec(xs[0], 156), module, Mnemonix::LEVEL_CV_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(xs[1], 156), module, Mnemonix::BLEND_CV_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(xs[2], 156), module, Mnemonix::FEEDBACK_CV_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(xs[3], 156), module, Mnemonix::DELAY_CV_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(xs[4], 156), module, Mnemonix::DEPTH_CV_INPUT));

    addParam(createParamCentered<CKSS>(Vec(37, 215), module, Mnemonix::MODE_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(73, 215), module, Mnemonix::MODE_CV_INPUT));
    addParam(createParamCentered<CKSS>(Vec(111, 215), module, Mnemonix::SHAPE_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(147, 215), module, Mnemonix::SHAPE_CV_INPUT));
    addParam(createParamCentered<CKSS>(Vec(185, 215), module, Mnemonix::LONG_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(221, 215), module, Mnemonix::LONG_CV_INPUT));
    addParam(createParamCentered<CKSS>(Vec(259, 215), module, Mnemonix::ENGAGE_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(295, 215), module, Mnemonix::ENGAGE_CV_INPUT));

    const float outXs[] = {32.f, 65.f, 98.f, 132.f, 165.f, 198.f, 232.f, 265.f, 298.f};
    addOutput(createOutputCentered<PJ301MPort>(Vec(outXs[0], 276), module, Mnemonix::LEVEL_CV_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(outXs[1], 276), module, Mnemonix::BLEND_CV_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(outXs[2], 276), module, Mnemonix::FEEDBACK_CV_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(outXs[3], 276), module, Mnemonix::DELAY_CV_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(outXs[4], 276), module, Mnemonix::DEPTH_CV_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(outXs[5], 276), module, Mnemonix::MODE_GATE_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(outXs[6], 276), module, Mnemonix::SHAPE_LFO_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(outXs[7], 276), module, Mnemonix::LONG_GATE_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(outXs[8], 276), module, Mnemonix::ENGAGE_GATE_OUTPUT));

    addInput(createInputCentered<PJ301MPort>(Vec(32, 320), module, Mnemonix::AUDIO_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(70, 320), module, Mnemonix::TAP_CV_INPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(108, 320), module, Mnemonix::DIRECT_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(146, 320), module, Mnemonix::WET_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(184, 320), module, Mnemonix::CLOCK_GATE_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(222, 320), module, Mnemonix::CLOCK_DIV_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(260, 320), module, Mnemonix::ENVELOPE_CV_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(298, 320), module, Mnemonix::AUDIO_OUTPUT));
}

void MnemonixWidget::appendContextMenu(Menu* menu) {
    Mnemonix* mnemonix = dynamic_cast<Mnemonix*>(module);
    if (!mnemonix) {
        return;
    }

    menu->addChild(new MenuSeparator());
    menu->addChild(createMenuLabel("Mnemonix Calibration"));

    struct ProfileOption {
        int value;
        const char* label;
    };

    const ProfileOption profiles[] = {
        {MnemonixDSP::ARTIFACT_CLEAN, "Cleaner BBD"},
        {MnemonixDSP::ARTIFACT_AUTHENTIC, "Rev_D authentic"},
        {MnemonixDSP::ARTIFACT_WORN, "Worn unit"}
    };

    menu->addChild(createSubmenuItem("Artifact profile", "", [=](Menu* submenu) {
        for (const auto& profile : profiles) {
            submenu->addChild(createMenuItem(profile.label,
                mnemonix->artifactProfile == profile.value ? "*" : "",
                [=]() { mnemonix->setArtifactProfile(profile.value); }));
        }
    }));

    menu->addChild(createSubmenuItem("Bypass behavior", mnemonix->bypassBehavior == Mnemonix::BYPASS_CPU_MUTE ? "CPU mute" : "Trails", [=](Menu* submenu) {
        submenu->addChild(createMenuItem("Trails", mnemonix->bypassBehavior == Mnemonix::BYPASS_TRAILS ? "*" : "", [=]() {
            mnemonix->bypassBehavior = Mnemonix::BYPASS_TRAILS;
        }));
        submenu->addChild(createMenuItem("CPU mute", mnemonix->bypassBehavior == Mnemonix::BYPASS_CPU_MUTE ? "*" : "", [=]() {
            mnemonix->bypassBehavior = Mnemonix::BYPASS_CPU_MUTE;
        }));
    }));

    menu->addChild(createSubmenuItem("Timing", mnemonix->timingMode == Mnemonix::TIMING_SYNC ? "Sync" : "Free", [=](Menu* submenu) {
        submenu->addChild(createMenuItem("Free delay knob", mnemonix->timingMode == Mnemonix::TIMING_FREE ? "*" : "", [=]() {
            mnemonix->timingMode = Mnemonix::TIMING_FREE;
        }));
        submenu->addChild(createMenuItem("Tap/sync delay", mnemonix->timingMode == Mnemonix::TIMING_SYNC ? "*" : "", [=]() {
            mnemonix->timingMode = Mnemonix::TIMING_SYNC;
        }));

        submenu->addChild(new MenuSeparator());
        submenu->addChild(createMenuLabel("Division"));
        for (int i = 0; i < Mnemonix::SYNC_DIVISIONS_LEN; ++i) {
            submenu->addChild(createMenuItem(syncDivisionName(i), mnemonix->syncDivision == i ? "*" : "", [=]() {
                mnemonix->syncDivision = i;
                mnemonix->timingMode = Mnemonix::TIMING_SYNC;
            }));
        }

        submenu->addChild(new MenuSeparator());
        submenu->addChild(createMenuLabel("Tap tempo seed"));
        struct TempoOption {
            float bpm;
            const char* label;
        };
        const TempoOption tempos[] = {
            {60.f, "60 BPM"},
            {90.f, "90 BPM"},
            {120.f, "120 BPM"},
            {140.f, "140 BPM"}
        };
        for (const auto& tempo : tempos) {
            submenu->addChild(createMenuItem(tempo.label, "", [=]() {
                mnemonix->tapTempoSeconds = 60.f / tempo.bpm;
                mnemonix->timingMode = Mnemonix::TIMING_SYNC;
            }));
        }
    }));

    menu->addChild(createSubmenuItem("Stereo mode", stereoModeName(mnemonix->stereoMode), [=](Menu* submenu) {
        struct StereoOption {
            int value;
            const char* label;
        };
        const StereoOption modes[] = {
            {Mnemonix::STEREO_MONO, "Mono pedal"},
            {Mnemonix::STEREO_WIDE, "Wide chorus"},
            {Mnemonix::STEREO_PING_PONG, "Ping-pong offset"}
        };
        for (const auto& mode : modes) {
            submenu->addChild(createMenuItem(mode.label, mnemonix->stereoMode == mode.value ? "*" : "", [=]() {
                mnemonix->stereoMode = mode.value;
            }));
        }
    }));

    menu->addChild(createSubmenuItem("Advanced calibration", "", [=](Menu* submenu) {
        struct TrimOption {
            float value;
            const char* label;
        };
        const TrimOption trimOptions[] = {
            {0.f, "Off"},
            {0.5f, "50%"},
            {0.75f, "75%"},
            {1.f, "100%"},
            {1.25f, "125%"},
            {1.5f, "150%"},
            {2.f, "200%"}
        };
        auto addTrim = [=](Menu* target, const char* name, float* trim, float minValue, float maxValue) {
            target->addChild(createSubmenuItem(name, "", [=](Menu* trimMenu) {
                for (const auto& option : trimOptions) {
                    if (option.value < minValue || option.value > maxValue) {
                        continue;
                    }
                    trimMenu->addChild(createMenuItem(option.label, std::fabs(*trim - option.value) < 0.001f ? "*" : "", [=]() {
                        *trim = option.value;
                    }));
                }
            }));
        };

        addTrim(submenu, "Input gain", &mnemonix->inputGainTrim, 0.5f, 1.5f);
        addTrim(submenu, "BBD bias", &mnemonix->bbdBiasTrim, 0.f, 2.f);
        addTrim(submenu, "Clock bleed", &mnemonix->clockBleedTrim, 0.f, 2.f);
        addTrim(submenu, "Compander trim", &mnemonix->companderTrim, 0.75f, 1.5f);
        addTrim(submenu, "Noise amount", &mnemonix->noiseTrim, 0.f, 2.f);
        addTrim(submenu, "Wet makeup", &mnemonix->wetMakeupTrim, 0.5f, 1.5f);
        addTrim(submenu, "Feedback headroom", &mnemonix->feedbackHeadroomTrim, 0.75f, 1.5f);

        submenu->addChild(new MenuSeparator());
        submenu->addChild(createMenuItem("Reset calibration trims", "", [=]() {
            mnemonix->inputGainTrim = 1.f;
            mnemonix->bbdBiasTrim = 1.f;
            mnemonix->clockBleedTrim = 1.f;
            mnemonix->companderTrim = 1.f;
            mnemonix->noiseTrim = 1.f;
            mnemonix->wetMakeupTrim = 1.f;
            mnemonix->feedbackHeadroomTrim = 1.f;
        }));
    }));

    menu->addChild(createMenuItem("Clear delay memory", "", [=]() {
        mnemonix->resetEngines();
    }));
}

Model* modelMnemonix = createModel<Mnemonix, MnemonixWidget>("Mnemonix");
