# NAM Module Implementation Plan

## Overview

This document provides a step-by-step implementation plan for creating a Neural Amp Modeler module for VCV Rack 2.x.

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

# Eigen configuration - disable vectorization for stability
FLAGS += -DEIGEN_MAX_ALIGN_BYTES=0
FLAGS += -DEIGEN_DONT_VECTORIZE

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

### 2.1 Create Module Header

**File:** `src/NAMPlayer.hpp`

```cpp
#pragma once

#include "plugin.hpp"
#include "NAM/dsp.h"
#include "NAM/get_dsp.h"
#include "NAM/activations.h"

#include <memory>
#include <atomic>
#include <thread>
#include <mutex>

struct NAMPlayer : Module {
    enum ParamId {
        INPUT_PARAM,
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
        MODEL_LIGHT,
        LIGHTS_LEN
    };

    // Constants
    static constexpr int BLOCK_SIZE = 128;
    
    // NAM model
    std::unique_ptr<nam::DSP> model;
    std::string modelPath;
    std::mutex modelMutex;
    
    // Async loading
    std::thread loadThread;
    std::atomic<bool> isLoading{false};
    std::atomic<bool> loadSuccess{false};
    
    // Audio buffers (pre-allocated)
    std::vector<double> inputBuffer;
    std::vector<double> outputBuffer;
    int bufferPos = 0;
    
    // Sample rate handling
    double currentSampleRate = 48000.0;
    double modelSampleRate = 48000.0;
    
    NAMPlayer();
    ~NAMPlayer();
    
    void process(const ProcessArgs& args) override;
    void onSampleRateChange(const SampleRateChangeEvent& e) override;
    
    void loadModel(const std::string& path);
    void unloadModel();
    
    json_t* dataToJson() override;
    void dataFromJson(json_t* rootJ) override;
};
```

### 2.2 Create Module Implementation

**File:** `src/NAMPlayer.cpp`

```cpp
#include "NAMPlayer.hpp"

NAMPlayer::NAMPlayer() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    
    configParam(INPUT_PARAM, 0.f, 2.f, 1.f, "Input Level");
    configParam(OUTPUT_PARAM, 0.f, 2.f, 1.f, "Output Level");
    
    configInput(AUDIO_INPUT, "Audio");
    configOutput(AUDIO_OUTPUT, "Audio");
    
    // Pre-allocate buffers
    inputBuffer.resize(BLOCK_SIZE, 0.0);
    outputBuffer.resize(BLOCK_SIZE, 0.0);
    
    // Enable fast tanh for better performance
    nam::activations::Activation::enable_fast_tanh();
}

NAMPlayer::~NAMPlayer() {
    // Wait for any loading thread to finish
    if (loadThread.joinable()) {
        loadThread.join();
    }
}

void NAMPlayer::process(const ProcessArgs& args) {
    // Check if model is loaded
    if (!model || isLoading) {
        outputs[AUDIO_OUTPUT].setVoltage(0.f);
        lights[MODEL_LIGHT].setBrightness(isLoading ? 0.5f : 0.f);
        return;
    }
    
    // Model loaded indicator
    lights[MODEL_LIGHT].setBrightness(1.f);
    
    // Get input with gain
    float inputGain = params[INPUT_PARAM].getValue();
    float input = inputs[AUDIO_INPUT].getVoltage() / 5.f * inputGain;
    
    // Accumulate input
    inputBuffer[bufferPos] = static_cast<double>(input);
    
    // Get output from previous block
    float output = static_cast<float>(outputBuffer[bufferPos]);
    
    // Advance position
    bufferPos++;
    
    // Process when buffer is full
    if (bufferPos >= BLOCK_SIZE) {
        std::lock_guard<std::mutex> lock(modelMutex);
        if (model) {
            model->process(inputBuffer.data(), outputBuffer.data(), BLOCK_SIZE);
        }
        bufferPos = 0;
    }
    
    // Apply output gain and send
    float outputGain = params[OUTPUT_PARAM].getValue();
    outputs[AUDIO_OUTPUT].setVoltage(output * outputGain * 5.f);
}

void NAMPlayer::onSampleRateChange(const SampleRateChangeEvent& e) {
    currentSampleRate = e.sampleRate;
    
    std::lock_guard<std::mutex> lock(modelMutex);
    if (model) {
        model->Reset(currentSampleRate, BLOCK_SIZE);
    }
}

void NAMPlayer::loadModel(const std::string& path) {
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
            auto newModel = nam::get_dsp(path);
            
            if (newModel) {
                // Initialize model
                newModel->Reset(currentSampleRate, BLOCK_SIZE);
                newModel->prewarm();
                
                // Swap model
                {
                    std::lock_guard<std::mutex> lock(modelMutex);
                    model = std::move(newModel);
                    modelPath = path;
                    modelSampleRate = model->GetExpectedSampleRate();
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

void NAMPlayer::unloadModel() {
    std::lock_guard<std::mutex> lock(modelMutex);
    model.reset();
    modelPath.clear();
}

json_t* NAMPlayer::dataToJson() {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "modelPath", json_string(modelPath.c_str()));
    return rootJ;
}

void NAMPlayer::dataFromJson(json_t* rootJ) {
    json_t* pathJ = json_object_get(rootJ, "modelPath");
    if (pathJ) {
        std::string path = json_string_value(pathJ);
        if (!path.empty()) {
            loadModel(path);
        }
    }
}
```

### 2.3 Create Module Widget

**File:** `src/NAMPlayerWidget.cpp`

```cpp
#include "NAMPlayer.hpp"

struct NAMPlayerWidget : ModuleWidget {
    NAMPlayerWidget(NAMPlayer* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/NAMPlayer.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Parameters
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.16, 46.0)), module, NAMPlayer::INPUT_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.16, 71.0)), module, NAMPlayer::OUTPUT_PARAM));

        // Inputs
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.16, 96.0)), module, NAMPlayer::AUDIO_INPUT));

        // Outputs
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10.16, 116.0)), module, NAMPlayer::AUDIO_OUTPUT));

        // Lights
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(10.16, 21.0)), module, NAMPlayer::MODEL_LIGHT));
    }

    void appendContextMenu(Menu* menu) override {
        NAMPlayer* module = dynamic_cast<NAMPlayer*>(this->module);
        if (!module) return;

        menu->addChild(new MenuSeparator());
        
        menu->addChild(createMenuItem("Load NAM Model...", "", [=]() {
            char* path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, 
                osdialog_filters_parse("NAM Models:nam"));
            if (path) {
                module->loadModel(path);
                free(path);
            }
        }));

        menu->addChild(createMenuItem("Unload Model", "", [=]() {
            module->unloadModel();
        }, !module->model));

        if (!module->modelPath.empty()) {
            menu->addChild(new MenuSeparator());
            menu->addChild(createMenuLabel("Model: " + system::getFilename(module->modelPath)));
        }
    }
};

Model* modelNAMPlayer = createModel<NAMPlayer, NAMPlayerWidget>("NAMPlayer");
```

### 2.4 Register Module

**File:** `src/plugin.cpp` (update)

```cpp
#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
    pluginInstance = p;

    p->addModel(modelNAMPlayer);
}
```

**File:** `src/plugin.hpp` (update)

```cpp
#pragma once
#include <rack.hpp>

using namespace rack;

extern Plugin* pluginInstance;

extern Model* modelNAMPlayer;
```

### 2.5 Update plugin.json

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
  "modules": [
    {
      "slug": "NAMPlayer",
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

Create `res/NAMPlayer.svg` with:
- Module width: 4HP (20.32mm)
- Standard Rack panel dimensions
- Labels for INPUT/OUTPUT knobs
- Model indicator light
- Input/Output port labels

Recommended tools:
- Inkscape
- Adobe Illustrator
- Or use VCV Rack panel template

### 3.2 Panel Specifications

```
Width: 4HP = 20.32mm
Height: 128.5mm (standard 3U)

Elements (from top to bottom):
- Model indicator light: y=21mm
- INPUT knob: y=46mm  
- OUTPUT knob: y=71mm
- Audio In port: y=96mm
- Audio Out port: y=116mm
```

---

## Phase 4: Sample Rate Conversion (2-3 hours)

### 4.1 Add Resampling Support

If models require specific sample rates, implement resampling using VCV's libsamplerate:

**File:** `src/Resampler.hpp`

```cpp
#pragma once
#include <samplerate.h>
#include <vector>

class Resampler {
public:
    Resampler() = default;
    ~Resampler();
    
    void init(double srcRate, double dstRate, int quality = SRC_SINC_MEDIUM_QUALITY);
    void reset();
    void process(const float* in, float* out, int numFrames, int& outFrames);
    
private:
    SRC_STATE* state = nullptr;
    double ratio = 1.0;
    std::vector<float> buffer;
};
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
