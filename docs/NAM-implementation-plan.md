# NAM Module Implementation Plan

## Overview

This document provides a step-by-step implementation plan for creating a Neural Amp Modeler module for VCV Rack 2.6.x and up.

### Target Platform

- **Primary Development:** macOS ARM64 (Apple Silicon)
- **Supported Platforms:** macOS, Windows, Linux (via GitHub Actions CI)
- **VCV Rack Version:** 2.6.x and up

---

## Phase 1: Project Setup (1-2 hours)

### 1.1 Add Dependencies

```bash
# Navigate to project root
cd /Users/shortwavlabs/Workspace/shortwavlabs/swv-guitar-collection

# Add NeuralAmpModelerCore as submodule
git submodule add https://github.com/sdatkinson/NeuralAmpModelerCore dep/NeuralAmpModelerCore

# Add Eigen (header-only, from GitLab)
git submodule add https://gitlab.com/libeigen/eigen.git dep/eigen

# Pin to stable versions
cd dep/NeuralAmpModelerCore && git checkout main && cd ../..
cd dep/eigen && git checkout 3.4.0 && cd ../..

# Initialize submodules (for fresh clones)
git submodule update --init --recursive
```

### 1.2 Update Makefile

Update the project Makefile with NAM configuration:

```makefile
# If RACK_DIR is not defined when calling the Makefile, default to two directories above
RACK_DIR ?= dep/Rack-SDK

# NAM Core paths
NAM_DIR := dep/NeuralAmpModelerCore
EIGEN_DIR := dep/eigen

# Include paths
FLAGS += -I$(NAM_DIR)
FLAGS += -I$(EIGEN_DIR)
FLAGS += -I$(NAM_DIR)/Dependencies/nlohmann

# C++17 required for NAM
CXXFLAGS += -std=c++17

# Eigen configuration - use aligned allocation for performance
# We use proper EIGEN_MAKE_ALIGNED_OPERATOR_NEW in classes containing Eigen types
# Fallback: uncomment these lines if alignment issues occur
# FLAGS += -DEIGEN_MAX_ALIGN_BYTES=0
# FLAGS += -DEIGEN_DONT_VECTORIZE

# NAM source files
NAM_SOURCES := $(NAM_DIR)/NAM/activations.cpp
NAM_SOURCES += $(NAM_DIR)/NAM/convnet.cpp
NAM_SOURCES += $(NAM_DIR)/NAM/dsp.cpp
NAM_SOURCES += $(NAM_DIR)/NAM/get_dsp.cpp
NAM_SOURCES += $(NAM_DIR)/NAM/lstm.cpp
NAM_SOURCES += $(NAM_DIR)/NAM/wavenet.cpp
NAM_SOURCES += $(NAM_DIR)/NAM/util.cpp

SOURCES += $(NAM_SOURCES)

# Plugin sources
SOURCES += $(wildcard src/*.cpp)

# Distribution files
DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += $(wildcard presets)

include $(RACK_DIR)/plugin.mk
```

### 1.3 Verify Build

```bash
make clean
make -j$(nproc)
```

---

## Phase 2: Core Module Implementation (4-6 hours)

### 2.1 Create DSP Abstraction Layer

**File:** `src/dsp/Nam.h`

This header contains the NAM DSP wrapper with resampling support and proper Eigen alignment.

```cpp
#pragma once

#include "NAM/dsp.h"
#include "NAM/get_dsp.h"
#include "NAM/activations.h"
#include <samplerate.h>

#include <memory>
#include <vector>
#include <string>
#include <Eigen/Dense>

/**
 * NamDSP - Wrapper for Neural Amp Modeler DSP with resampling support
 * 
 * Handles:
 * - Model loading and management
 * - Sample rate conversion (engine rate <-> model rate)
 * - Proper Eigen memory alignment
 * - Thread-safe model swapping
 * - Passthrough when no model loaded
 * - CPU usage monitoring
 * - Noise gate (before NAM processing)
 * - 5-band tone stack (Bass, Middle, Treble, Presence, Depth)
 */
class NamDSP {
public:
    // Eigen alignment macro for classes containing Eigen types
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    
    static constexpr int BLOCK_SIZE = 128;
    static constexpr int MAX_RESAMPLE_RATIO = 8;  // Support up to 8x resampling
    
    NamDSP();
    ~NamDSP();
    
    // Model management
    bool loadModel(const std::string& path);
    void unloadModel();
    bool isModelLoaded() const { return model != nullptr; }
    double getModelSampleRate() const;
    std::string getModelPath() const { return modelPath; }
    std::string getModelName() const;  // Returns filename without extension
    
    // Processing
    void setSampleRate(double rate);
    void process(const float* input, float* output, int numFrames);
    void reset();
    
    // Noise Gate - values in dB/ms
    void setNoiseGate(float thresholdDb, float attackMs, float releaseMs, float holdMs);
    bool isGateOpen() const { return noiseGate.isOpen; }  // For LED indicator
    
    // Tone Stack (5-band EQ) - values in dB (-12 to +12)
    void setToneStack(float bass, float middle, float treble, float presence, float depth);
    
    // Monitoring
    float getCpuLoad() const { return cpuLoad; }  // 0.0 to 1.0
    bool isSampleRateMismatched() const { return std::abs(engineSampleRate - modelSampleRate) > 1.0; }
    
private:
    std::unique_ptr<nam::DSP> model;
    std::string modelPath;
    
    // Resampling state
    SRC_STATE* srcIn = nullptr;
    SRC_STATE* srcOut = nullptr;
    double engineSampleRate = 48000.0;
    double modelSampleRate = 48000.0;
    
    // Noise Gate (before NAM)
    NoiseGate noiseGate;
    
    // Tone Stack (5-band EQ using biquad filters, after NAM)
    ToneStack toneStack;
    
    // CPU monitoring
    float cpuLoad = 0.f;
    
    // Pre-allocated buffers for resampling
    std::vector<float> resampleInBuffer;
    std::vector<float> resampleOutBuffer;
    std::vector<double> modelInputBuffer;
    std::vector<double> modelOutputBuffer;
    
    void initResampling();
    void cleanupResampling();
};

/**
 * NoiseGate - Hysteresis-based noise gate for guitar
 * 
 * Uses RMS envelope detection with hysteresis to prevent
 * chattering on decaying notes. Placed before NAM processing
 * since distortion amplifies noise.
 */
struct NoiseGate {
    // Parameters
    float threshold = -50.f;    // dB
    float attack = 0.0005f;     // seconds
    float release = 0.1f;       // seconds  
    float hold = 0.05f;         // seconds
    float hysteresis = 6.f;     // dB
    
    // State
    float envelope = 0.f;
    float gain = 0.f;
    float holdCounter = 0.f;
    bool isOpen = false;
    
    // Coefficients
    float envAttack, envRelease;
    float gainAttack, gainRelease;
    double sampleRate = 48000.0;
    
    void setSampleRate(double sr);
    void setParameters(float threshDb, float attackMs, float releaseMs, float holdMs);
    float process(float sample);
    void reset();
};

/**
 * ToneStack - 5-band EQ for guitar amp tone shaping
 * 
 * Placed after NAM processing to shape output tone:
 * - Bass (Low Shelf, 100 Hz)
 * - Middle (Peaking, 650 Hz)  
 * - Treble (High Shelf, 3.2 kHz)
 * - Presence (Peaking, 3.5 kHz)
 * - Depth (Peaking, 80 Hz)
 */
struct ToneStack {
    BiquadFilter bass;      // Low shelf at 100 Hz
    BiquadFilter middle;    // Peaking at 650 Hz
    BiquadFilter treble;    // High shelf at 3.2 kHz
    BiquadFilter presence;  // Peaking at 3.5 kHz
    BiquadFilter depth;     // Peaking at 80 Hz
    
    void setSampleRate(double sr);
    void setParameters(float bassDb, float midDb, float trebleDb, float presenceDb, float depthDb);
    float process(float sample);
    void reset();
};

/**
 * BiquadFilter - Second-order IIR filter
 */
struct BiquadFilter {
    // Coefficients
    float b0, b1, b2, a1, a2;
    // State
    float z1 = 0.f, z2 = 0.f;
    
    void setLowShelf(double sr, double freq, double q, double gainDb);
    void setHighShelf(double sr, double freq, double q, double gainDb);
    void setPeaking(double sr, double freq, double q, double gainDb);
    float process(float in);
    void reset() { z1 = z2 = 0.f; }
};
```

### 2.2 Create Module Header

**File:** `src/NamPlayer.hpp`

```cpp
#pragma once

#include "plugin.hpp"
#include "dsp/Nam.h"

#include <memory>
#include <atomic>
#include <thread>
#include <mutex>

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
    std::mutex dspMutex;
    
    // Async loading
    std::thread loadThread;
    std::atomic<bool> isLoading{false};
    std::atomic<bool> loadSuccess{false};
    
    // Audio buffers (pre-allocated)
    std::vector<float> inputBuffer;
    std::vector<float> outputBuffer;
    int bufferPos = 0;
    
    // Sample rate
    double currentSampleRate = 48000.0;
    
    // CPU monitoring (for display)
    float cpuLoad = 0.f;
    
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
```

### 2.3 Create Module Implementation

**File:** `src/NamPlayer.cpp`

```cpp
#include "NamPlayer.hpp"
#include <osdialog.h>

NamPlayer::NamPlayer() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    
    configParam(INPUT_PARAM, 0.f, 2.f, 1.f, "Input Level");
    configParam(OUTPUT_PARAM, 0.f, 2.f, 1.f, "Output Level");
    
    configInput(AUDIO_INPUT, "Audio");
    configOutput(AUDIO_OUTPUT, "Audio");
    
    // Initialize DSP wrapper
    namDsp = std::make_unique<NamDSP>();
    
    // Pre-allocate buffers
    inputBuffer.resize(BLOCK_SIZE, 0.f);
    outputBuffer.resize(BLOCK_SIZE, 0.f);
    
    // Enable fast tanh for better performance
    nam::activations::Activation::enable_fast_tanh();
}

NamPlayer::~NamPlayer() {
    // Wait for any loading thread to finish
    if (loadThread.joinable()) {
        loadThread.join();
    }
}

void NamPlayer::process(const ProcessArgs& args) {
    // Get input with gain (line-level normalization: ±5V -> ±1.0)
    float inputGain = params[INPUT_PARAM].getValue();
    float input = inputs[AUDIO_INPUT].getVoltage() / 5.f * inputGain;
    
    // Update noise gate parameters from knobs
    if (namDsp) {
        float threshold = params[GATE_THRESHOLD_PARAM].getValue();  // -80 to 0 dB
        float attack = params[GATE_ATTACK_PARAM].getValue();        // 0.1 to 10 ms
        float release = params[GATE_RELEASE_PARAM].getValue();      // 10 to 500 ms
        float hold = params[GATE_HOLD_PARAM].getValue();            // 10 to 500 ms
        namDsp->setNoiseGate(threshold, attack, release, hold);
    }
    
    // Update tone stack parameters from knobs (convert 0-1 to -12 to +12 dB)
    if (namDsp) {
        float bass = (params[BASS_PARAM].getValue() - 0.5f) * 24.f;      // -12 to +12 dB
        float middle = (params[MIDDLE_PARAM].getValue() - 0.5f) * 24.f;
        float treble = (params[TREBLE_PARAM].getValue() - 0.5f) * 24.f;
        float presence = (params[PRESENCE_PARAM].getValue() - 0.5f) * 24.f;
        float depth = (params[DEPTH_PARAM].getValue() - 0.5f) * 24.f;
        namDsp->setToneStack(bass, middle, treble, presence, depth);
    }
    
    // Check if model is loaded - passthrough if not
    if (!namDsp || !namDsp->isModelLoaded() || isLoading) {
        // Passthrough: output = input (with gain applied)
        float outputGain = params[OUTPUT_PARAM].getValue();
        outputs[AUDIO_OUTPUT].setVoltage(input * outputGain * 5.f);
        
        lights[MODEL_LIGHT].setBrightness(isLoading ? 0.5f : 0.f);
        lights[SAMPLE_RATE_LIGHT].setBrightness(0.f);
        lights[GATE_LIGHT].setBrightness(0.f);
        return;
    }
    
    // Model loaded indicator
    lights[MODEL_LIGHT].setBrightness(1.f);
    
    // Sample rate mismatch indicator
    lights[SAMPLE_RATE_LIGHT].setBrightness(namDsp->isSampleRateMismatched() ? 1.f : 0.f);
    
    // Gate activity indicator
    lights[GATE_LIGHT].setBrightness(namDsp->isGateOpen() ? 1.f : 0.2f);
    
    // Update CPU load for display
    cpuLoad = namDsp->getCpuLoad();
    
    // Accumulate input
    inputBuffer[bufferPos] = input;
    
    // Get output from previous block
    float output = outputBuffer[bufferPos];
    
    // Advance position
    bufferPos++;
    
    // Process when buffer is full (Gate -> NAM -> Tone Stack applied internally)
    if (bufferPos >= BLOCK_SIZE) {
        std::lock_guard<std::mutex> lock(dspMutex);
        if (namDsp && namDsp->isModelLoaded()) {
            namDsp->process(inputBuffer.data(), outputBuffer.data(), BLOCK_SIZE);
        }
        bufferPos = 0;
    }
    
    // Apply output gain and send (scale back to ±5V)
    float outputGain = params[OUTPUT_PARAM].getValue();
    outputs[AUDIO_OUTPUT].setVoltage(output * outputGain * 5.f);
}

void NamPlayer::onSampleRateChange(const SampleRateChangeEvent& e) {
    currentSampleRate = e.sampleRate;
    
    std::lock_guard<std::mutex> lock(dspMutex);
    if (namDsp) {
        namDsp->setSampleRate(currentSampleRate);
    }
}

void NamPlayer::loadModel(const std::string& path) {
    // Don't start new load if already loading
    if (isLoading) {
        return;
    }
    
    // Wait for previous load to finish
    if (loadThread.joinable()) {
        loadThread.join();
    }
    
    isLoading = true;
    loadSuccess = false;
    
    loadThread = std::thread([this, path]() {
        try {
            auto newDsp = std::make_unique<NamDSP>();
            
            if (newDsp->loadModel(path)) {
                // Initialize with current sample rate
                newDsp->setSampleRate(currentSampleRate);
                
                // Swap DSP
                {
                    std::lock_guard<std::mutex> lock(dspMutex);
                    namDsp = std::move(newDsp);
                }
                
                loadSuccess = true;
            }
        } catch (const std::exception& e) {
            WARN("Failed to load NAM model: %s", e.what());
            loadSuccess = false;
        }
        
        isLoading = false;
    });
}

void NamPlayer::unloadModel() {
    std::lock_guard<std::mutex> lock(dspMutex);
    if (namDsp) {
        namDsp->unloadModel();
    }
}

std::string NamPlayer::getModelPath() const {
    if (namDsp) {
        return namDsp->getModelPath();
    }
    return "";
}

json_t* NamPlayer::dataToJson() {
    json_t* rootJ = json_object();
    std::string path = getModelPath();
    json_object_set_new(rootJ, "modelPath", json_string(path.c_str()));
    return rootJ;
}

void NamPlayer::dataFromJson(json_t* rootJ) {
    json_t* pathJ = json_object_get(rootJ, "modelPath");
    if (pathJ) {
        std::string path = json_string_value(pathJ);
        if (!path.empty()) {
            loadModel(path);
        }
    }
}
```

### 2.4 Create Module Widget (in NamPlayer.cpp)

Add to `src/NamPlayer.cpp`:

```cpp
// Widget implementation
NamPlayerWidget::NamPlayerWidget(NamPlayer* module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/NamPlayer.svg")));

    // Module is 21HP = 106.68mm wide
    // Panel based on SWV_21HP_PANEL.svg template
    
    // Screws (4 corners)
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    // Input/Output gain knobs (large, top corners)
    addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15, 30)), module, NamPlayer::INPUT_PARAM));
    addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(91, 30)), module, NamPlayer::OUTPUT_PARAM));

    // Noise Gate knobs (small, second row)
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(15, 50)), module, NamPlayer::GATE_THRESHOLD_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(34, 50)), module, NamPlayer::GATE_ATTACK_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(53, 50)), module, NamPlayer::GATE_RELEASE_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(72, 50)), module, NamPlayer::GATE_HOLD_PARAM));

    // Tone Stack knobs (small, third row - 5 knobs evenly spaced)
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(15, 70)), module, NamPlayer::BASS_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(34, 70)), module, NamPlayer::MIDDLE_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(53, 70)), module, NamPlayer::TREBLE_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(72, 70)), module, NamPlayer::PRESENCE_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(91, 70)), module, NamPlayer::DEPTH_PARAM));

    // Mono inputs/outputs (bottom)
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15, 100)), module, NamPlayer::AUDIO_INPUT));
    addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(91, 100)), module, NamPlayer::AUDIO_OUTPUT));

    // Lights (top center area)
    addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(45, 20)), module, NamPlayer::MODEL_LIGHT));
    addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(53, 20)), module, NamPlayer::SAMPLE_RATE_LIGHT));
    addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(61, 20)), module, NamPlayer::GATE_LIGHT));
    
    // Custom displays (see custom widget implementations):
    // - ModelNameDisplay: shows loaded model name (center top)
    // - CpuMeter: shows CPU usage percentage
}

void NamPlayerWidget::appendContextMenu(Menu* menu) {
    NamPlayer* module = dynamic_cast<NamPlayer*>(this->module);
    if (!module) return;

    menu->addChild(new MenuSeparator());
    
    // Submenu for bundled models (from res/models/)
    menu->addChild(createSubmenuItem("Bundled Models", "", [=](Menu* submenu) {
        std::string modelsDir = asset::plugin(pluginInstance, "res/models");
        std::vector<std::string> modelFiles = system::getEntries(modelsDir);
        
        for (const std::string& file : modelFiles) {
            if (system::getExtension(file) == ".nam") {
                std::string name = system::getStem(file);
                submenu->addChild(createMenuItem(name, "", [=]() {
                    module->loadModel(file);
                }));
            }
        }
    }));
    
    // File picker for custom models
    menu->addChild(createMenuItem("Load Custom Model...", "", [=]() {
        char* path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, 
            osdialog_filters_parse("NAM Models:nam"));
        if (path) {
            module->loadModel(path);
            free(path);
        }
    }));

    menu->addChild(createMenuItem("Unload Model", "", [=]() {
        module->unloadModel();
    }, !module->namDsp || !module->namDsp->isModelLoaded()));

    std::string modelName = module->getModelName();
    if (!modelName.empty()) {
        menu->addChild(new MenuSeparator());
        menu->addChild(createMenuLabel("Model: " + modelName));
        if (module->isSampleRateMismatched()) {
            menu->addChild(createMenuLabel("⚠ Sample rate mismatch (resampling active)"));
        }
    }
}

Model* modelNamPlayer = createModel<NamPlayer, NamPlayerWidget>("NamPlayer");
```

### 2.5 Register Module

**File:** `src/plugin.cpp` (update)

```cpp
#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
    pluginInstance = p;

    p->addModel(modelNamPlayer);
}
```

**File:** `src/plugin.hpp` (update)

```cpp
#pragma once
#include <rack.hpp>

using namespace rack;

extern Plugin* pluginInstance;

extern Model* modelNamPlayer;
```

### 2.6 Update plugin.json

```json
{
  "slug": "swv-guitar-collection",
  "name": "SWV Guitar Collection",
  "version": "2.0.0",
  "license": "GPL-3.0-or-later",
  "brand": "Shortwav Labs",
  "author": "Stephane Pericat",
  "authorEmail": "",
  "authorUrl": "",
  "pluginUrl": "",
  "manualUrl": "",
  "sourceUrl": "https://github.com/shortwavlabs/swv-guitar-collection",
  "donateUrl": "",
  "changelogUrl": "",
  "minRackVersion": "2.6.0",
  "modules": [
    {
      "slug": "NamPlayer",
      "name": "NAM Player",
      "description": "Neural Amp Modeler player for guitar amp simulation",
      "tags": ["Effect", "Distortion", "Hardware Clone"]
    }
  ]
}
```

---

## Phase 3: Panel Design (1-2 hours)

### 3.1 Create SVG Panel

Create `res/NamPlayer.svg` with:
- Module width: 21HP (106.68mm)
- Standard Rack panel dimensions
- Labels for INPUT/OUTPUT knobs
- Model indicator light
- Input/Output port labels
- Space for model name display (future)

Recommended tools:
- Inkscape
- Adobe Illustrator
- Or use VCV Rack panel template

### 3.2 Panel Specifications

```
Width: 21HP = 106.68mm
Height: 128.5mm (standard 3U)
Template: Based on SWV_21HP_PANEL.svg

Elements (positioned for 21HP):

Top Section (y=20mm):
- Model indicator light: x=45mm (green)
- Sample rate indicator: x=53mm (yellow) 
- Gate activity light: x=61mm (green)
- Model name display: center area, y=10-18mm

Gain Section (y=30mm):
- INPUT gain knob: x=15mm (large)
- OUTPUT gain knob: x=91mm (large)

Noise Gate Section (y=50mm) - 4 knobs:
- THRESHOLD knob: x=15mm (small) - -80 to 0 dB
- ATTACK knob: x=34mm (small) - 0.1 to 10 ms
- RELEASE knob: x=53mm (small) - 10 to 500 ms
- HOLD knob: x=72mm (small) - 10 to 500 ms

Tone Stack Section (y=70mm) - 5 knobs:
- BASS knob: x=15mm (small) - Low shelf 100Hz
- MIDDLE knob: x=34mm (small) - Peaking 650Hz
- TREBLE knob: x=53mm (small) - High shelf 3.2kHz  
- PRESENCE knob: x=72mm (small) - Peaking 3.5kHz
- DEPTH knob: x=91mm (small) - Peaking 80Hz

Display Section (y=85mm):
- CPU meter: center area

I/O Section (y=100mm):
- Audio In port (mono): x=15mm
- Audio Out port (mono): x=91mm

Labels:
- "INPUT" / "OUTPUT" above gain knobs
- "GATE" section label
- "THRESH" "ATK" "REL" "HOLD" above gate knobs
- "TONE" section label  
- "BASS" "MID" "TREB" "PRES" "DEPTH" above tone knobs
- "IN" / "OUT" below ports
- "NAM PLAYER" at top
```

---

## Phase 4: Sample Rate Conversion (Integrated)

### 4.1 Resampling in NamDSP

Resampling is implemented from the start in the `NamDSP` wrapper class (`src/dsp/Nam.h`).

The wrapper handles:
- Automatic detection of model sample rate vs engine sample rate
- Upsampling input to model rate when needed
- Downsampling output back to engine rate
- Pre-allocated buffers to avoid audio thread allocations

**Implementation in `src/dsp/Nam.h`:**

```cpp
void NamDSP::process(const float* input, float* output, int numFrames) {
    if (!model) return;
    
    // Check if resampling is needed
    double ratio = modelSampleRate / engineSampleRate;
    
    if (std::abs(ratio - 1.0) < 0.001) {
        // No resampling needed - direct processing
        for (int i = 0; i < numFrames; i++) {
            modelInputBuffer[i] = static_cast<double>(input[i]);
        }
        model->process(modelInputBuffer.data(), modelOutputBuffer.data(), numFrames);
        for (int i = 0; i < numFrames; i++) {
            output[i] = static_cast<float>(modelOutputBuffer[i]);
        }
    } else {
        // Resampling required
        int resampledFrames = static_cast<int>(numFrames * ratio) + 1;
        
        // Upsample input to model rate
        SRC_DATA srcData;
        srcData.data_in = input;
        srcData.input_frames = numFrames;
        srcData.data_out = resampleInBuffer.data();
        srcData.output_frames = resampledFrames;
        srcData.src_ratio = ratio;
        src_process(srcIn, &srcData);
        
        // Convert to double and process
        for (int i = 0; i < srcData.output_frames_gen; i++) {
            modelInputBuffer[i] = static_cast<double>(resampleInBuffer[i]);
        }
        model->process(modelInputBuffer.data(), modelOutputBuffer.data(), srcData.output_frames_gen);
        
        // Convert back to float
        for (int i = 0; i < srcData.output_frames_gen; i++) {
            resampleOutBuffer[i] = static_cast<float>(modelOutputBuffer[i]);
        }
        
        // Downsample output to engine rate
        SRC_DATA srcDataOut;
        srcDataOut.data_in = resampleOutBuffer.data();
        srcDataOut.input_frames = srcData.output_frames_gen;
        srcDataOut.data_out = output;
        srcDataOut.output_frames = numFrames;
        srcDataOut.src_ratio = 1.0 / ratio;
        src_process(srcOut, &srcDataOut);
    }
}
```

---

## Phase 5: Testing (2-3 hours)

### 5.1 Build Verification

```bash
# Clean build
make clean && make -j$(nproc)

# Check for undefined symbols
nm -u plugin.dylib | grep -E "nam|Eigen"  # Should be empty
```

### 5.2 Functional Testing

1. Load plugin in VCV Rack
2. Add NAMPlayer module to patch
3. Right-click → Load NAM Model
4. Connect audio source → NAMPlayer → audio output
5. Verify audio processing
6. Save and reload patch (verify model path persistence)

### 5.3 Stress Testing

1. Run at various sample rates (44.1k, 48k, 96k, 192k)
2. Add multiple NAMPlayer instances
3. Monitor CPU usage
4. Test model loading during playback (no dropouts)

### 5.4 Platform Testing

Test on all target platforms:
- macOS (Intel and Apple Silicon if possible)
- Windows (via MinGW)
- Linux

---

## Phase 6: Documentation (1-2 hours)

### 6.1 Update README

- Installation instructions
- Usage guide
- Model compatibility notes
- Performance recommendations

### 6.2 User Documentation

- How to load models
- Parameter descriptions
- Troubleshooting guide

---

## Timeline Summary

| Phase | Task | Duration |
|-------|------|----------|
| 1 | Project Setup | 1-2 hours |
| 2 | Core Implementation | 4-6 hours |
| 3 | Panel Design | 1-2 hours |
| 4 | Sample Rate Conversion | 2-3 hours |
| 5 | Testing | 2-3 hours |
| 6 | Documentation | 1-2 hours |
| **Total** | | **11-18 hours** |

---

## Future Enhancements

### Version 2.1
- [ ] Tone stack (bass/mid/treble EQ)
- [ ] Noise gate
- [ ] Model browser (list models in folder)
- [ ] Model presets

### Version 2.2
- [ ] Stereo processing
- [ ] CV control of parameters
- [ ] Model blending (crossfade between models)

### Version 2.3
- [ ] Cab simulation (IR loader)
- [ ] Model training info display
- [ ] Undo/redo for model loading

---

## Quick Start Commands

```bash
# Clone with submodules
git clone --recursive https://github.com/shortwavlabs/swv-guitar-collection.git

# Or init submodules after clone
git submodule update --init --recursive

# Build
make -j$(nproc)

# Install to VCV Rack plugins folder
make install

# Run VCV Rack (macOS)
open /Applications/VCV\ Rack\ 2\ Free.app
```
