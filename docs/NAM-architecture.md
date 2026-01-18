# NAM Module Architecture for VCV Rack

## Overview

This document describes the architecture for integrating Neural Amp Modeler into a VCV Rack 2.x module, bridging the NAM DSP library with VCV Rack's plugin framework.

## NeuralAmpModelerCore Architecture

### Class Hierarchy

```
nam::DSP (base class)
├── nam::Linear          - Simple linear model
├── nam::LSTM           - Long Short-Term Memory network
├── nam::ConvNet        - Convolutional neural network
└── nam::wavenet::WaveNet - WaveNet architecture (most common)
```

All derived classes share the common DSP interface:

```cpp
namespace nam {
class DSP {
public:
    // Main processing function - process a block of samples
    virtual void process(NAM_SAMPLE* input, NAM_SAMPLE* output, const int num_frames);
    
    // Set maximum expected buffer size (call before processing)
    void SetMaxBufferSize(const int maxBufferSize);
    
    // Get expected sample rate for this model
    double GetExpectedSampleRate() const;
    
    // Whether model has input/output loudness metadata
    bool HasLoudness() const;
    double GetLoudness() const;
    
    // Reset internal state
    void Reset(const double sampleRate, const int maxBufferSize);
    
    // Prewarm the model with silence
    void prewarm();
};
}
```

### Sample Type

NAM uses a configurable sample type:

```cpp
// Default: double precision
using NAM_SAMPLE = double;

// With NAM_SAMPLE_FLOAT defined: single precision
// #define NAM_SAMPLE_FLOAT
// using NAM_SAMPLE = float;
```

VCV Rack uses `float` for audio. Conversion will be needed if using default NAM configuration.

### Model Loading

```cpp
#include "NAM/get_dsp.h"

// Load model from file path
std::unique_ptr<nam::DSP> model = nam::get_dsp(filepath);

// Check if loading succeeded
if (model == nullptr) {
    // Handle error
}

// Get model metadata
double expectedSampleRate = model->GetExpectedSampleRate();
```

## VCV Rack Module Architecture

### Target Platform

- **Primary:** VCV Rack 2.6.x and up
- **Development Focus:** macOS ARM64 (Apple Silicon)
- **Supported:** macOS, Windows, Linux (via GitHub Actions CI)

### Module Specifications

- **Module Width:** 21HP (106.68mm)
- **I/O:** Mono input, mono output (guitar amps are inherently mono)
- **Input Normalization:** Line-level (±5V → ±1.0), user adjusts via input gain
- **Empty State:** Passthrough (audio passes through unprocessed when no model loaded)
- **Panel Displays:**
  - Model name display
  - Sample rate mismatch indicator (when engine rate ≠ model rate)
  - CPU usage meter

### File Structure

```
src/
├── dsp/
│   └── Nam.h              # NAM DSP abstraction layer
├── NamPlayer.hpp          # Module header
├── NamPlayer.cpp          # Module implementation
├── plugin.hpp
└── plugin.cpp
```

### Module Structure

```cpp
// src/NamPlayer.hpp
struct NamPlayer : rack::Module {
    enum ParamId {
        INPUT_LEVEL_PARAM,
        OUTPUT_LEVEL_PARAM,
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
        MODEL_LOADED_LIGHT,
        LIGHTS_LEN
    };
    
    // NAM wrapper (from src/dsp/Nam.h)
    std::unique_ptr<NamDSP> namDsp;
    std::string modelPath;
    
    // Audio processing state
    std::vector<float> inputBuffer;
    std::vector<float> outputBuffer;
    int bufferIndex = 0;
    
    NamPlayer();
    void process(const ProcessArgs& args) override;
    void loadModel(const std::string& path);
    json_t* dataToJson() override;
    void dataFromJson(json_t* rootJ) override;
};
```

### Processing Strategy

VCV Rack calls `process()` once per sample, while NAM processes blocks. Two main approaches:

#### Approach 1: Sample-by-Sample with Internal Buffering

```cpp
void NAMPlayerModule::process(const ProcessArgs& args) {
    float input = inputs[AUDIO_INPUT].getVoltage() / 5.0f;  // Normalize to ±1
    
    if (namModel) {
        inputBuffer[bufferIndex] = static_cast<NAM_SAMPLE>(input);
        float output = static_cast<float>(outputBuffer[bufferIndex]);
        bufferIndex++;
        
        if (bufferIndex >= BLOCK_SIZE) {
            namModel->process(inputBuffer.data(), outputBuffer.data(), BLOCK_SIZE);
            bufferIndex = 0;
        }
        
        outputs[AUDIO_OUTPUT].setVoltage(output * 5.0f);
    }
}
```

**Note:** This introduces latency of `BLOCK_SIZE` samples.

#### Approach 2: Overlap-Add for Lower Latency

Use circular buffers with overlap to reduce perceived latency:

```cpp
class CircularBuffer {
    std::vector<NAM_SAMPLE> buffer;
    int writePos = 0;
    int readPos = 0;
    // ...
};
```

### Sample Rate Handling

NAM models expect specific sample rates (typically 48kHz). Resampling is implemented from the start using libsamplerate (included in VCV Rack SDK).

```cpp
// Resampling is handled in the NamDSP wrapper (src/dsp/Nam.h)
// Uses VCV Rack SDK's libsamplerate: dep/include/samplerate.h

#include "samplerate.h"

// In NamDSP class:
class NamDSP {
    SRC_STATE* srcIn = nullptr;   // Upsample to model rate
    SRC_STATE* srcOut = nullptr;  // Downsample from model rate
    double modelSampleRate = 48000.0;
    double engineSampleRate = 48000.0;
    
    void initResampling() {
        int error;
        srcIn = src_new(SRC_SINC_MEDIUM_QUALITY, 1, &error);
        srcOut = src_new(SRC_SINC_MEDIUM_QUALITY, 1, &error);
    }
    
    void updateSampleRates(double engineRate, double modelRate) {
        engineSampleRate = engineRate;
        modelSampleRate = modelRate;
        // Recalculate resampling ratios
    }
};
```

## Data Flow Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                     VCV Rack Engine                             │
│                    (44.1/48/96 kHz)                             │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                   NAMPlayerModule::process()                    │
│                                                                 │
│  1. Get input voltage (±5V) → normalize to ±1.0                │
│  2. Convert float → NAM_SAMPLE (double)                        │
│  3. Accumulate in input buffer                                 │
│  4. When buffer full: call namModel->process()                 │
│  5. Read from output buffer                                    │
│  6. Convert NAM_SAMPLE → float                                 │
│  7. Scale to voltage (±5V) → output                            │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      nam::DSP::process()                        │
│                                                                 │
│  Input buffer  ──────────────────────────────────►  Output buffer│
│  (BLOCK_SIZE samples)                              (BLOCK_SIZE) │
│                                                                 │
│  Internal stages (e.g., WaveNet):                              │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐    │
│  │ Input    │──►│ Dilated  │──►│ Dilated  │──►│ Head     │    │
│  │ Conv     │   │ Conv 1   │   │ Conv N   │   │ Recomb   │    │
│  └──────────┘   └──────────┘   └──────────┘   └──────────┘    │
└─────────────────────────────────────────────────────────────────┘
```

## Model File Format

NAM model files (`.nam`) are JSON with this structure:

```json
{
  "version": "0.5.4",
  "architecture": "WaveNet",
  "config": {
    "input_size": 1,
    "condition_size": 0,
    "head_size": 8,
    "channels": 16,
    "kernel_size": 3,
    "dilations": [1, 2, 4, 8, 16, 32, 64, 1, 2, 4, 8, 16, 32, 64],
    "head_bias": true
  },
  "weights": {
    "_input_layer.weight": [...],
    "_input_layer.bias": [...],
    // ... more layer weights
  },
  "metadata": {
    "name": "Model Name",
    "author": "Author Name",
    "date": "2024-01-01",
    "samplerate": 48000
  }
}
```

## State Management

### Serialization (Saving/Loading Patches)

```cpp
json_t* NAMPlayerModule::dataToJson() {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "modelPath", json_string(modelPath.c_str()));
    return rootJ;
}

void NAMPlayerModule::dataFromJson(json_t* rootJ) {
    json_t* pathJ = json_object_get(rootJ, "modelPath");
    if (pathJ) {
        std::string path = json_string_value(pathJ);
        loadModel(path);
    }
}
```

### Thread Safety

Model loading should happen on a separate thread to avoid audio dropouts:

```cpp
std::thread loadThread;
std::atomic<bool> modelLoading{false};
std::mutex modelMutex;

void loadModelAsync(const std::string& path) {
    if (loadThread.joinable()) {
        loadThread.join();
    }
    
    modelLoading = true;
    loadThread = std::thread([this, path]() {
        auto newModel = nam::get_dsp(path);
        if (newModel) {
            newModel->Reset(sampleRate, BLOCK_SIZE);
            newModel->prewarm();
            
            std::lock_guard<std::mutex> lock(modelMutex);
            namModel = std::move(newModel);
            modelPath = path;
        }
        modelLoading = false;
    });
}
```

## Module Widget

```cpp
struct NamPlayerWidget : ModuleWidget {
    NamPlayerWidget(NamPlayer* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/NamPlayer.svg")));
        
        // Module is 21HP = 106.68mm wide
        // Panel based on SWV_21HP_PANEL.svg template
        
        // Screws (4 corners)
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        
        // Knobs - spread across 21HP panel
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(26.67, 45)), module, NamPlayer::INPUT_LEVEL_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(80.01, 45)), module, NamPlayer::OUTPUT_LEVEL_PARAM));
        
        // Mono ports
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(26.67, 110)), module, NamPlayer::AUDIO_INPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(80.01, 110)), module, NamPlayer::AUDIO_OUTPUT));
        
        // Model load indicator (centered)
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(53.34, 25)), module, NamPlayer::MODEL_LOADED_LIGHT));
        
        // Custom displays (implemented as custom widgets):
        // - Model name display (center area)
        // - Sample rate mismatch indicator
        // - CPU usage meter
    }
    
    void appendContextMenu(Menu* menu) override {
        NamPlayer* module = dynamic_cast<NamPlayer*>(this->module);
        if (!module) return;
        
        menu->addChild(new MenuSeparator());
        
        // Submenu for bundled models
        menu->addChild(createSubmenuItem("Bundled Models", "", [=](Menu* submenu) {
            // Populate with models from res/models/
            for (const auto& modelFile : getBundledModels()) {
                submenu->addChild(createMenuItem(modelFile.name, "", [=]() {
                    module->loadModel(modelFile.path);
                }));
            }
        }));
        
        // File picker for custom models
        menu->addChild(createMenuItem("Load Custom Model...", "", [=]() {
            char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, nullptr);
            if (path) {
                module->loadModel(path);
                free(path);
            }
        }));
        
        // Unload option
        menu->addChild(createMenuItem("Unload Model", "", [=]() {
            module->unloadModel();
        }, !module->namDsp || !module->namDsp->isModelLoaded()));
    }
};

// Note: Widget implementation is in NamPlayer.cpp
```

## Recommended Block Sizes

| Block Size | Latency @ 48kHz | Use Case |
|------------|-----------------|----------|
| 32 samples | 0.67 ms | Minimum latency, higher CPU |
| 64 samples | 1.33 ms | Good balance |
| 128 samples | 2.67 ms | Default, efficient |
| 256 samples | 5.33 ms | Lower CPU, noticeable latency |

Most users won't notice latency under 5ms, so 128-256 samples is recommended for efficiency.

## Bundled NAM Models

The plugin ships with all models from https://github.com/pelennor2170/NAM_models

These models are included in the `res/models/` directory and can be loaded via:
- **Submenu:** Right-click context menu lists all bundled models for quick access
- **File picker:** "Load Custom Model..." option for user's own `.nam` files

Note: The full model collection is bundled initially; this may be trimmed down later if distribution size or performance becomes an issue.
