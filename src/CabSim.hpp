#pragma once

#include "plugin.hpp"
#include "dsp/CabSimDSP.h"

#include <memory>
#include <atomic>
#include <thread>

struct CabSim : Module {
    enum ParamId {
        BLEND_PARAM,
        LOWPASS_PARAM,
        HIGHPASS_PARAM,
        OUTPUT_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        AUDIO_INPUT,
        // CV inputs for all parameters
        CV_BLEND_INPUT,
        CV_LOWPASS_INPUT,
        CV_HIGHPASS_INPUT,
        CV_OUTPUT_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        AUDIO_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        IR_A_LIGHT,
        IR_B_LIGHT,
        LIGHTS_LEN
    };

    // DSP
    std::unique_ptr<CabSimDSP> cabSimDsp;
    
    // Async loading
    std::thread loadThread;
    std::atomic<bool> isLoading{false};
    std::atomic<int> loadingSlot{-1};

    // Pending IR updates (applied on audio thread)
    std::vector<float> pendingIrSamples[2];
    std::string pendingIrPath[2];
    std::string pendingIrName[2];
    std::atomic<bool> hasPendingIr[2];
    std::atomic<bool> hasPendingIrUnload[2];

    // Pending sample rate updates (applied on audio thread)
    std::atomic<float> pendingSampleRate{48000.f};
    std::atomic<bool> hasPendingSampleRate{false};
    
    // State for serialization
    std::string irPathA;
    std::string irPathB;
    bool normalizeA = false;
    bool normalizeB = false;
    
    // Sample rate
    std::atomic<float> currentSampleRate{48000.f};

    // Filter cache (avoid per-sample updates)
    float lastLpFreq = -1.f;
    float lastHpFreq = -1.f;
    
    CabSim();
    ~CabSim();
    
    void process(const ProcessArgs& args) override;
    void onSampleRateChange(const SampleRateChangeEvent& e) override;
    
    void loadIR(int slot, const std::string& path);
    void unloadIR(int slot);
    void setNormalize(int slot, bool enabled);
    bool getNormalize(int slot) const;
    std::string getIRPath(int slot) const;
    std::string getIRName(int slot) const;
    bool isIRLoaded(int slot) const;
    
    json_t* dataToJson() override;
    void dataFromJson(json_t* rootJ) override;
};

struct CabSimWidget : ModuleWidget {
    CabSimWidget(CabSim* module);
    void appendContextMenu(Menu* menu) override;
};
