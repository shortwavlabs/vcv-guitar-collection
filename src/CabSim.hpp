#pragma once

#include "plugin.hpp"
#include "dsp/CabSimDSP.h"

#include <memory>
#include <atomic>
#include <thread>
#include <mutex>

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
    std::mutex dspMutex;
    
    // Async loading
    std::thread loadThread;
    std::atomic<bool> isLoading{false};
    std::atomic<int> loadingSlot{-1};
    
    // State for serialization
    std::string irPathA;
    std::string irPathB;
    bool normalizeA = false;
    bool normalizeB = false;
    
    // Sample rate
    float currentSampleRate = 48000.f;
    
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
