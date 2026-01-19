#pragma once

#include "plugin.hpp"
#include "dsp/Nam.h"

#include <memory>
#include <atomic>
#include <thread>

struct NamPlayer : Module {
    enum ParamId {
        INPUT_PARAM,
        OUTPUT_PARAM,
        // Noise Gate
        GATE_THRESHOLD_PARAM,
        GATE_ATTACK_PARAM,
        GATE_RELEASE_PARAM,
        GATE_HOLD_PARAM,
        // Tone Stack (5-band EQ)
        BASS_PARAM,
        MIDDLE_PARAM,
        TREBLE_PARAM,
        PRESENCE_PARAM,
        DEPTH_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        AUDIO_INPUT,  // Mono input
        INPUTS_LEN
    };
    enum OutputId {
        AUDIO_OUTPUT,  // Mono output
        OUTPUTS_LEN
    };
    enum LightId {
        MODEL_LIGHT,
        SAMPLE_RATE_LIGHT,  // Indicates sample rate mismatch
        GATE_LIGHT,         // Shows when gate is open
        LIGHTS_LEN
    };

    // Constants
    static constexpr int BLOCK_SIZE = 128;
    
    // NAM DSP wrapper (handles resampling and tone stack internally)
    std::unique_ptr<NamDSP> namDsp;
    std::unique_ptr<NamDSP> pendingDsp;
    std::atomic<bool> hasPendingDsp{false};
    std::atomic<bool> hasPendingUnload{false};
    
    // Async loading
    std::thread loadThread;
    std::atomic<bool> isLoading{false};
    std::atomic<bool> loadSuccess{false};
    
    // Audio buffers (pre-allocated)
    std::vector<float> inputBuffer;
    std::vector<float> outputBuffer;
    int bufferPos = 0;
    
    // Sample rate
    std::atomic<double> currentSampleRate{48000.0};
    std::atomic<double> pendingSampleRate{48000.0};
    std::atomic<bool> hasPendingSampleRate{false};
    
    NamPlayer();
    ~NamPlayer();
    
    void process(const ProcessArgs& args) override;
    void onSampleRateChange(const SampleRateChangeEvent& e) override;
    
    void loadModel(const std::string& path);
    void unloadModel();
    std::string getModelPath() const;
    std::string getModelName() const;
    bool isSampleRateMismatched() const;
    
    json_t* dataToJson() override;
    void dataFromJson(json_t* rootJ) override;
};

struct NamPlayerWidget : ModuleWidget {
    NamPlayerWidget(NamPlayer* module);
    void appendContextMenu(Menu* menu) override;
};
