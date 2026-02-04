#pragma once

#include "plugin.hpp"
#include "dsp/OverdriveDSP.h"

#include <dsp/digital.hpp>

struct Overdrive : Module {
    enum ParamId {
        MODEL_PARAM,
        DRIVE_PARAM,
        TONE_PARAM,
        LEVEL_PARAM,
        ATTACK_PARAM,
        GATE_PARAM,
        BYPASS_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        AUDIO_INPUT,
        DRIVE_CV,
        TONE_CV,
        LEVEL_CV,
        ATTACK_CV,
        GATE_CV,
        INPUTS_LEN
    };
    enum OutputId {
        AUDIO_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        GATE_GREEN_LIGHT,
        GATE_RED_LIGHT,
        LIGHTS_LEN
    };

    Overdrive();

    void process(const ProcessArgs& args) override;
    void onSampleRateChange(const SampleRateChangeEvent& e) override;
    void onReset(const ResetEvent& e) override;
    json_t* dataToJson() override;
    void dataFromJson(json_t* rootJ) override;

private:
    OverdriveDSP dsp;

    // CV smoothing
    dsp::ExponentialFilter driveSmoother;
    dsp::ExponentialFilter toneSmoother;
    dsp::ExponentialFilter levelSmoother;
    dsp::ExponentialFilter gateSmoother;

    dsp::SchmittTrigger bypassTrigger;
    bool isBypassed = false;
};

struct OverdriveWidget : ModuleWidget {
    OverdriveWidget(Overdrive* module);
};
