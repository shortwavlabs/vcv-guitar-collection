#pragma once

#include "plugin.hpp"
#include "dsp/StrobeTunerDSP.h"

#include <atomic>

struct StrobeTunerDisplay;

struct StrobeTuner : Module {
    enum ParamId {
        A4_REF_PARAM,
        SMOOTHING_PARAM,
        SENSITIVITY_PARAM,
        PARAMS_LEN
    };

    enum InputId {
        AUDIO_INPUT,
        A4_CV_INPUT,
        INPUTS_LEN
    };

    enum OutputId {
        THRU_OUTPUT,
        NOTE_OUTPUT,
        ERROR_OUTPUT,
        OUTPUTS_LEN
    };

    enum LightId {
        FLAT_LIGHT,
        IN_TUNE_LIGHT,
        SHARP_LIGHT,
        SIGNAL_LIGHT,
        LIGHTS_LEN
    };

    StrobeTuner();

    void process(const ProcessArgs& args) override;
    void onSampleRateChange(const SampleRateChangeEvent& e) override;
    void onReset(const ResetEvent& e) override;

    // Shared with UI widget (atomics for lock-free access).
    std::atomic<float> uiPhaseCycles{0.f};
    std::atomic<float> uiCents{0.f};
    std::atomic<float> uiConfidence{0.f};
    std::atomic<float> uiFrequencyHz{0.f};
    std::atomic<int> uiMidiNote{69};
    std::atomic<bool> uiPitchValid{false};

private:
    StrobeTunerDSP pitchDetector;

    float phaseCycles = 0.f;
    float lastDeltaHz = 0.f;
    float centsError = 0.f;
    float confidence = 0.f;
    int midiNote = 69;
    bool pitchValid = false;

    float cachedSmoothing = -1.f;
    float cachedConfidenceThreshold = -1.f;
    float cachedMinRms = -1.f;
};

struct StrobeTunerWidget : ModuleWidget {
    StrobeTunerWidget(StrobeTuner* module);
};

struct StrobeTunerDisplay : OpaqueWidget {
    StrobeTuner* module = nullptr;

    void draw(const DrawArgs& args) override;

private:
    void drawBackground(const DrawArgs& args);
    void drawStripes(const DrawArgs& args);
    void drawReadout(const DrawArgs& args);
};
