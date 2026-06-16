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
        SHAPE_PARAM,
        ENGAGE_PARAM,
        LONG_PARAM,
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
        SHAPE_CV_INPUT,
        ENGAGE_CV_INPUT,
        LONG_CV_INPUT,
        TAP_CV_INPUT,
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
        SHAPE_LFO_OUTPUT,
        ENGAGE_GATE_OUTPUT,
        LONG_GATE_OUTPUT,
        CLOCK_GATE_OUTPUT,
        CLOCK_DIV_OUTPUT,
        ENVELOPE_CV_OUTPUT,
        OUTPUTS_LEN
    };

    enum BypassBehavior {
        BYPASS_TRAILS = 0,
        BYPASS_CPU_MUTE = 1
    };

    enum TimingMode {
        TIMING_FREE = 0,
        TIMING_SYNC = 1
    };

    enum SyncDivision {
        SYNC_WHOLE = 0,
        SYNC_HALF,
        SYNC_DOTTED_QUARTER,
        SYNC_QUARTER,
        SYNC_DOTTED_EIGHTH,
        SYNC_EIGHTH,
        SYNC_EIGHTH_TRIPLET,
        SYNC_SIXTEENTH,
        SYNC_DIVISIONS_LEN
    };

    enum StereoMode {
        STEREO_MONO = 0,
        STEREO_WIDE,
        STEREO_PING_PONG
    };

    enum LightId {
        OVERLOAD_LIGHT,
        STATUS_LIGHT,
        VIBRATO_LIGHT,
        SQUARE_LIGHT,
        LONG_LIGHT,
        LIGHTS_LEN
    };

    std::array<MnemonixDSP, 16> engines;
    float currentSampleRate = 0.f;
    bool sampleRateInitialized = false;
    int artifactProfile = MnemonixDSP::ARTIFACT_AUTHENTIC;
    int bypassBehavior = BYPASS_TRAILS;
    int timingMode = TIMING_FREE;
    int syncDivision = SYNC_QUARTER;
    int stereoMode = STEREO_MONO;
    float tapTempoSeconds = 0.5f;
    float tapTimerSeconds = 0.f;
    bool previousEngaged = true;
    dsp::SchmittTrigger tapTrigger;

    float inputGainTrim = 1.f;
    float bbdBiasTrim = 1.f;
    float clockBleedTrim = 1.f;
    float companderTrim = 1.f;
    float noiseTrim = 1.f;
    float wetMakeupTrim = 1.f;
    float feedbackHeadroomTrim = 1.f;

    Mnemonix();

    void process(const ProcessArgs& args) override;
    void onSampleRateChange(const SampleRateChangeEvent& e) override;
    json_t* dataToJson() override;
    void dataFromJson(json_t* rootJ) override;

    void resetEngines();
    void setArtifactProfile(int profile);
    void setEngineSampleRate(float sampleRate);
    float getSyncedDelayNorm(bool longDelay) const;
};

struct MnemonixWidget : ModuleWidget {
    MnemonixWidget(Mnemonix* module);
    void appendContextMenu(Menu* menu) override;
};
