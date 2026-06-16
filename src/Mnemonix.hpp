#pragma once

#include "plugin.hpp"
#include "dsp/MnemonixDSP.h"

#include <array>

struct Mnemonix : Module {
    enum ParamId {
        LEVEL_PARAM,
        BLEND_PARAM,
        FEEDBACK_PARAM,
        DELAY_PARAM,
        DEPTH_PARAM,
        MODE_PARAM,
        ENGAGE_PARAM,
        LEVEL_CV_ATT_PARAM,
        BLEND_CV_ATT_PARAM,
        FEEDBACK_CV_ATT_PARAM,
        DELAY_CV_ATT_PARAM,
        DEPTH_CV_ATT_PARAM,
        PARAMS_LEN
    };

    enum InputId {
        AUDIO_INPUT,
        LEVEL_CV_INPUT,
        BLEND_CV_INPUT,
        FEEDBACK_CV_INPUT,
        DELAY_CV_INPUT,
        DEPTH_CV_INPUT,
        MODE_CV_INPUT,
        ENGAGE_CV_INPUT,
        INPUTS_LEN
    };

    enum OutputId {
        AUDIO_OUTPUT,
        DIRECT_OUTPUT,
        WET_OUTPUT,
        LEVEL_CV_OUTPUT,
        BLEND_CV_OUTPUT,
        FEEDBACK_CV_OUTPUT,
        DELAY_CV_OUTPUT,
        DEPTH_CV_OUTPUT,
        MODE_GATE_OUTPUT,
        ENGAGE_GATE_OUTPUT,
        OUTPUTS_LEN
    };

    enum LightId {
        OVERLOAD_LIGHT,
        STATUS_LIGHT,
        VIBRATO_LIGHT,
        LIGHTS_LEN
    };

    std::array<MnemonixDSP, 16> engines;
    float currentSampleRate = 48000.f;
    int artifactProfile = MnemonixDSP::ARTIFACT_AUTHENTIC;

    Mnemonix();

    void process(const ProcessArgs& args) override;
    void onSampleRateChange(const SampleRateChangeEvent& e) override;
    json_t* dataToJson() override;
    void dataFromJson(json_t* rootJ) override;

    void resetEngines();
    void setArtifactProfile(int profile);
};

struct MnemonixWidget : ModuleWidget {
    MnemonixWidget(Mnemonix* module);
    void appendContextMenu(Menu* menu) override;
};

