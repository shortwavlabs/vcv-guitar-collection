# Cabinet Simulator Implementation Plan

## Overview

This document outlines the step-by-step implementation plan for the Cabinet Simulator (CabSim) module. The plan is broken into phases to allow for incremental development and testing.

## Phase 1: Project Setup and Dependencies

### 1.1 Add dr_wav Dependency

**Task:** Download and integrate the dr_wav single-header library for WAV file loading.

**Files to create:**
- `src/dsp/dr_wav.h`

**Source:** https://raw.githubusercontent.com/mackron/dr_libs/master/dr_wav.h

**Implementation Notes:**
- Only include `#define DR_WAV_IMPLEMENTATION` in ONE .cpp file
- The header provides both WAV reading and writing capabilities
- We only need the reading functionality

### 1.2 Update Build Configuration

**Files to modify:**
- `Makefile` (if needed for additional include paths)
- `plugin.json` (add new module entry)

**plugin.json addition:**
```json
{
  "slug": "CabSim",
  "name": "Cabinet Simulator",
  "description": "Convolution-based cabinet simulator with dual IR loading, blending, and tone shaping",
  "tags": ["Effect", "Equalizer", "Hardware Clone"]
}
```

## Phase 2: Core DSP Implementation

### 2.1 Create IR Loader Class

**File:** `src/dsp/IRLoader.h`

**Responsibilities:**
- Load WAV files using dr_wav
- Support mono and stereo files (convert stereo to mono)
- Handle different bit depths (16, 24, 32-bit, float)
- Support different sample rates
- Resample IR to match engine sample rate
- Provide peak normalization

**API:**
```cpp
class IRLoader {
public:
    bool load(const std::string& path);
    bool resampleTo(float targetSampleRate);
    void normalize();
    void reset();
    
    const std::vector<float>& getSamples() const;
    size_t getLength() const;
    float getOriginalSampleRate() const;
    float getPeakLevel() const;
    std::string getPath() const;
    std::string getName() const;  // Filename without extension
    bool isLoaded() const;
};
```

### 2.2 Create CabSim DSP Wrapper

**File:** `src/dsp/CabSimDSP.h`

**Responsibilities:**
- Manage two RealTimeConvolver instances
- Handle block-based processing accumulation
- Implement IR blending
- Implement high-pass and low-pass filters
- Manage sample rate changes

**Key Implementation Details:**

1. **Block Accumulation:**
   - VCV calls `process()` per-sample
   - Accumulate samples until block is full
   - Process block through convolvers
   - Output from previous block while accumulating

2. **Dual Convolver Processing:**
   - Process input through both convolvers
   - Blend outputs based on blend parameter
   - Handle cases where only one IR is loaded

3. **Filter Processing:**
   - Apply HPF first, then LPF
   - Update filter coefficients when cutoff changes
   - Process sample-by-sample (after block output)

### 2.3 Implement Sample Rate Conversion

**Using VCV Rack's SampleRateConverter:**
```cpp
void IRLoader::resampleTo(float targetSampleRate) {
    if (std::abs(originalSampleRate - targetSampleRate) < 1.0f) {
        return;  // No resampling needed
    }
    
    rack::dsp::SampleRateConverter<1> src;
    src.setRates(originalSampleRate, targetSampleRate);
    src.setQuality(8);  // High quality for IR resampling
    
    // Calculate output size
    size_t outputSize = (samples.size() * targetSampleRate / originalSampleRate) + 100;
    std::vector<float> resampled(outputSize);
    
    int inFrames = samples.size();
    int outFrames = outputSize;
    src.process(samples.data(), 1, &inFrames, resampled.data(), 1, &outFrames);
    
    resampled.resize(outFrames);
    samples = std::move(resampled);
    originalSampleRate = targetSampleRate;
}
```

## Phase 3: Module Implementation

### 3.1 Create Module Header

**File:** `src/CabSim.hpp`

**Parameters:**
```cpp
enum ParamId {
    BLEND_PARAM,      // 0.0 = 100% IR A, 1.0 = 100% IR B
    LOWPASS_PARAM,    // Frequency in Hz, displayed logarithmically
    HIGHPASS_PARAM,   // Frequency in Hz, displayed logarithmically
    OUTPUT_PARAM,     // Linear gain 0.0 - 2.0
    PARAMS_LEN
};
```

**Inputs/Outputs:**
```cpp
enum InputId {
    AUDIO_INPUT,
    INPUTS_LEN
};

enum OutputId {
    AUDIO_OUTPUT,
    OUTPUTS_LEN
};
```

**Lights:**
```cpp
enum LightId {
    IR_A_LIGHT,        // Green when IR A loaded
    IR_B_LIGHT,        // Green when IR B loaded  
    PROCESSING_LIGHT,  // Blue when processing (optional)
    LIGHTS_LEN
};
```

### 3.2 Create Module Implementation

**File:** `src/CabSim.cpp`

**Constructor:**
```cpp
CabSim::CabSim() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    
    // Blend: 0 = IR A only (or dry), 1 = IR B only (or wet)
    // When only one IR loaded, acts as wet/dry mix
    configParam(BLEND_PARAM, 0.f, 1.f, 0.5f, "IR Blend", "%", 0.f, 100.f);
    
    // Lowpass: 1kHz to 20kHz, log scale (2-pole, 12 dB/oct)
    configParam(LOWPASS_PARAM, 0.f, 1.f, 1.f, "Low-Pass Cutoff", " Hz", 
                std::pow(20000.f / 1000.f, 1.f), 1000.f);
    
    // Highpass: 20Hz to 2kHz, log scale (2-pole, 12 dB/oct)
    configParam(HIGHPASS_PARAM, 0.f, 1.f, 0.f, "High-Pass Cutoff", " Hz",
                std::pow(2000.f / 20.f, 1.f), 20.f);
    
    // Output level: 0-200%
    configParam(OUTPUT_PARAM, 0.f, 2.f, 1.f, "Output Level", "%", 0.f, 100.f);
    
    configInput(AUDIO_INPUT, "Audio");
    configOutput(AUDIO_OUTPUT, "Audio");
    
    configBypass(AUDIO_INPUT, AUDIO_OUTPUT);
    
    // Initialize DSP
    cabSimDsp = std::make_unique<CabSimDSP>();
}
```

**Process Method:**
```cpp
void CabSim::process(const ProcessArgs& args) {
    // Get parameters
    float blend = params[BLEND_PARAM].getValue();
    
    // Convert log params to Hz
    float lpFreq = 1000.f * std::pow(20.f, params[LOWPASS_PARAM].getValue());
    float hpFreq = 20.f * std::pow(100.f, params[HIGHPASS_PARAM].getValue());
    
    float outputGain = params[OUTPUT_PARAM].getValue();
    
    // Get mono input (normalized to ±1.0)
    float input = inputs[AUDIO_INPUT].getVoltage() / 5.f;
    
    // Process through DSP
    // Note: when only one IR loaded, blend acts as wet/dry mix
    float output = cabSimDsp->process(input, blend, lpFreq, hpFreq, args.sampleRate);
    
    // Apply output gain and convert back to ±5V
    outputs[AUDIO_OUTPUT].setVoltage(output * outputGain * 5.f);
    
    // Update lights
    lights[IR_A_LIGHT].setBrightness(cabSimDsp->isIRLoaded(0) ? 1.f : 0.f);
    lights[IR_B_LIGHT].setBrightness(cabSimDsp->isIRLoaded(1) ? 1.f : 0.f);
}
```

### 3.3 Implement Async IR Loading

**Thread-safe loading pattern:**
```cpp
void CabSim::loadIR(int slot, const std::string& path) {
    if (isLoading.exchange(true)) {
        return;  // Already loading
    }
    
    if (loadThread.joinable()) {
        loadThread.join();
    }
    
    loadThread = std::thread([this, slot, path]() {
        auto loader = std::make_unique<IRLoader>();
        
        if (loader->load(path)) {
            // Resample to current engine rate
            loader->resampleTo(currentSampleRate);
            
            // Apply normalization if enabled
            if (slot == 0 && normalizeA) {
                loader->normalize();
            } else if (slot == 1 && normalizeB) {
                loader->normalize();
            }
            
            // Update DSP (thread-safe swap)
            {
                std::lock_guard<std::mutex> lock(dspMutex);
                cabSimDsp->setIR(slot, std::move(loader));
            }
            
            // Update path for serialization
            if (slot == 0) {
                irPathA = path;
            } else {
                irPathB = path;
            }
        }
        
        isLoading = false;
    });
}
```

### 3.4 Implement State Serialization

```cpp
json_t* CabSim::dataToJson() {
    json_t* rootJ = json_object();
    
    // Save IR paths (relative paths where possible)
    if (!irPathA.empty()) {
        json_object_set_new(rootJ, "irPathA", json_string(irPathA.c_str()));
    }
    if (!irPathB.empty()) {
        json_object_set_new(rootJ, "irPathB", json_string(irPathB.c_str()));
    }
    
    // Save normalization settings
    json_object_set_new(rootJ, "normalizeA", json_boolean(normalizeA));
    json_object_set_new(rootJ, "normalizeB", json_boolean(normalizeB));
    
    return rootJ;
}

void CabSim::dataFromJson(json_t* rootJ) {
    // Load normalization settings first
    json_t* normAJ = json_object_get(rootJ, "normalizeA");
    if (normAJ) normalizeA = json_boolean_value(normAJ);
    
    json_t* normBJ = json_object_get(rootJ, "normalizeB");
    if (normBJ) normalizeB = json_boolean_value(normBJ);
    
    // Then load IRs (they will use normalization settings)
    json_t* pathAJ = json_object_get(rootJ, "irPathA");
    if (pathAJ) {
        std::string path = json_string_value(pathAJ);
        if (!path.empty() && rack::system::exists(path)) {
            loadIR(0, path);
        }
    }
    
    json_t* pathBJ = json_object_get(rootJ, "irPathB");
    if (pathBJ) {
        std::string path = json_string_value(pathBJ);
        if (!path.empty() && rack::system::exists(path)) {
            loadIR(1, path);
        }
    }
}
```

## Phase 4: Widget Implementation

### 4.1 Create Panel SVG

**File:** `res/CabSim.svg`

**Panel Layout (12HP = 60.96mm):**
```
+------------------+
|   CABINET SIM    |
|                  |
|  [IR A]  [IR B]  |   <- LED indicators
|                  |
|    [ BLEND ]     |   <- Large knob
|                  |
| [LPF]    [HPF]   |   <- Medium knobs
|                  |
|   [ OUTPUT ]     |   <- Medium knob
|                  |
|  IN L    OUT L   |   <- Jacks
|  IN R    OUT R   |
+------------------+
```

### 4.2 Create Widget Implementation

**File:** `src/CabSim.cpp` (widget section)

```cpp
struct CabSimWidget : ModuleWidget {
    CabSimWidget(CabSim* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/CabSim.svg")));
        
        // Screws
        addChild(createWidget<ScrewSilver>(Vec(0, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 15, 0)));
        addChild(createWidget<ScrewSilver>(Vec(0, 365)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 15, 365)));
        
        // Lights for IR status
        addChild(createLightCentered<SmallLight<GreenLight>>(
            mm2px(Vec(10, 25)), module, CabSim::IR_A_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(
            mm2px(Vec(30, 25)), module, CabSim::IR_B_LIGHT));
        
        // Blend knob (large)
        addParam(createParamCentered<RoundBigBlackKnob>(
            mm2px(Vec(20, 45)), module, CabSim::BLEND_PARAM));
        
        // Filter knobs
        addParam(createParamCentered<RoundBlackKnob>(
            mm2px(Vec(10, 70)), module, CabSim::LOWPASS_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(
            mm2px(Vec(30, 70)), module, CabSim::HIGHPASS_PARAM));
        
        // Output knob
        addParam(createParamCentered<RoundBlackKnob>(
            mm2px(Vec(20, 95)), module, CabSim::OUTPUT_PARAM));
        
        // Input jack (mono)
        addInput(createInputCentered<PJ301MPort>(
            mm2px(Vec(10, 115)), module, CabSim::AUDIO_INPUT));
        
        // Output jack (mono)
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(30, 115)), module, CabSim::AUDIO_OUTPUT));
    }
    
    void appendContextMenu(Menu* menu) override;
};
```

### 4.3 Implement Context Menu

```cpp
void CabSimWidget::appendContextMenu(Menu* menu) {
    CabSim* module = getModule<CabSim>();
    if (!module) return;
    
    menu->addChild(new MenuSeparator);
    menu->addChild(createMenuLabel("Impulse Responses"));
    
    // Load IR A
    menu->addChild(createMenuItem("Load IR A...", "",
        [=]() {
            char* path = osdialog_file(OSDIALOG_OPEN, 
                NULL, NULL, 
                osdialog_filters_parse("WAV files:wav"));
            if (path) {
                module->loadIR(0, path);
                free(path);
            }
        }
    ));
    
    // Load IR B
    menu->addChild(createMenuItem("Load IR B...", "",
        [=]() {
            char* path = osdialog_file(OSDIALOG_OPEN,
                NULL, NULL,
                osdialog_filters_parse("WAV files:wav"));
            if (path) {
                module->loadIR(1, path);
                free(path);
            }
        }
    ));
    
    menu->addChild(new MenuSeparator);
    
    // Unload options
    menu->addChild(createMenuItem("Unload IR A", "",
        [=]() { module->unloadIR(0); },
        !module->cabSimDsp->isIRLoaded(0)
    ));
    
    menu->addChild(createMenuItem("Unload IR B", "",
        [=]() { module->unloadIR(1); },
        !module->cabSimDsp->isIRLoaded(1)
    ));
    
    menu->addChild(new MenuSeparator);
    
    // Normalization toggles
    menu->addChild(createBoolPtrMenuItem("Normalize IR A", "", 
        &module->normalizeA));
    menu->addChild(createBoolPtrMenuItem("Normalize IR B", "", 
        &module->normalizeB));
    
    menu->addChild(new MenuSeparator);
    
    // IR Info
    if (module->cabSimDsp->isIRLoaded(0)) {
        menu->addChild(createMenuLabel(
            "IR A: " + module->cabSimDsp->getIRName(0)));
    }
    if (module->cabSimDsp->isIRLoaded(1)) {
        menu->addChild(createMenuLabel(
            "IR B: " + module->cabSimDsp->getIRName(1)));
    }
}
```

## Phase 5: Plugin Registration

### 5.1 Update plugin.hpp

```cpp
// Add at end of file
extern Model* modelCabSim;
```

### 5.2 Update plugin.cpp

```cpp
// Add model
Model* modelCabSim = createModel<CabSim, CabSimWidget>("CabSim");

void init(Plugin* p) {
    pluginInstance = p;
    
    p->addModel(modelNamPlayer);
    p->addModel(modelCabSim);  // Add new model
}
```

## Phase 6: Testing

### 6.1 Manual Testing Checklist

- [ ] Module loads without crash
- [ ] Empty state passes audio through (bypass)
- [ ] IR A loads correctly
- [ ] IR B loads correctly
- [ ] Blend knob crossfades between IRs
- [ ] Lowpass filter affects tone
- [ ] Highpass filter affects tone
- [ ] Output level works
- [ ] Sample rate change doesn't crash
- [ ] State saves/loads correctly
- [ ] Async loading doesn't cause audio dropouts
- [ ] Normalization affects level correctly
- [ ] Context menu items all work
- [ ] Lights indicate IR status correctly

### 6.2 Performance Testing

- [ ] CPU usage with 0 IRs loaded
- [ ] CPU usage with 1 IR (500ms) loaded  
- [ ] CPU usage with 2 IRs (500ms each) loaded
- [ ] CPU usage with maximum length IRs (1s each)
- [ ] Memory usage stable over time
- [ ] No memory leaks after load/unload cycles

### 6.3 Compatibility Testing

- [ ] Test with various IR sample rates (44.1k, 48k, 96k)
- [ ] Test with various bit depths (16, 24, 32-bit)
- [ ] Test with stereo IRs (should convert to mono)
- [ ] Test with very short IRs (<100 samples)
- [ ] Test with very long IRs (>48000 samples)

## Timeline Estimate

| Phase | Tasks | Estimated Time |
|-------|-------|----------------|
| Phase 1 | Setup | 1-2 hours |
| Phase 2 | DSP | 4-6 hours |
| Phase 3 | Module | 4-6 hours |
| Phase 4 | Widget | 2-3 hours |
| Phase 5 | Registration | 30 min |
| Phase 6 | Testing | 2-4 hours |

**Total: 14-22 hours**

## Risk Mitigation

### Risk 1: IR Loading Performance
**Mitigation:** Async loading with progress indicator, limit max IR length

### Risk 2: Sample Rate Conversion Quality
**Mitigation:** Use high-quality resampler settings, validate output

### Risk 3: Thread Safety
**Mitigation:** Use mutex for DSP state access, atomic flags for status

### Risk 4: Memory Usage
**Mitigation:** Limit max IR length, release memory on unload

### Risk 5: Convolver Latency
**Mitigation:** Document latency, consider smaller block sizes for lower latency option
