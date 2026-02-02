# Overdrive Module - Detailed Implementation Plan

**Project:** SWV Guitar Collection - Overdrive Module  
**Target:** VCV Rack Plugin  
**Panel Size:** 8HP  
**Status:** Planning Phase  
**Date:** February 2026

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Architecture Overview](#architecture-overview)
3. [File Structure](#file-structure)
4. [Implementation Phases](#implementation-phases)
5. [Phase 1: Core DSP Components](#phase-1-core-dsp-components)
6. [Phase 2: VCV Rack Module Integration](#phase-2-vcv-rack-module-integration)
7. [Phase 3: Panel and UI](#phase-3-panel-and-ui)
8. [Phase 4: Testing](#phase-4-testing)
9. [Phase 5: Optimization](#phase-5-optimization)
10. [Dependencies](#dependencies)
11. [Performance Targets](#performance-targets)
12. [Testing Strategy](#testing-strategy)
13. [Milestone Checklist](#milestone-checklist)

---

## Project Overview

### Goals

Implement a faithful digital recreation of three classic overdrive pedals:
- **Ibanez Tubescreamer TS-808** (1979-1980)
- **Ibanez Tubescreamer TS-9** (1981-present)
- **Boss DS-1 Distortion** (1978-present)

### Key Features

- 3-way model selector switch
- Component-level circuit modeling
- Attack control (6-position step knob)
- Single-knob noise gate
- CV modulation for all parameters
- 4x oversampling with polyphase filters
- Mono processing only (no polyphony)

### Constraints

- **Panel Size:** 8HP (40.64mm width)
- **Processing:** Mono only
- **CPU Target:** < 5% on typical hardware (Intel i5/Ryzen 5)
- **Latency:** < 2ms at 48kHz with oversampling
- **Testing:** Unit tests only (no hardware validation)

---

## Architecture Overview

### Signal Flow

```
Input → Oversampling (4x Upsample) → Noise Gate → Model-Specific Chain → Oversampling (4x Downsample) → Output

Model-Specific Chains:
TS-808/TS-9: Input Buffer → Soft Clipper (w/ Attack) → Tone Stack → Output Buffer
DS-1:        Input Buffer → Transistor Booster → Hard Clipper → Tone Stack → Output Buffer
```

### DSP Processing Chain

1. **Upsampling:** 4x using polyphase FIR filters
2. **Noise Gate:** RMS envelope detection with fixed parameters (from Nam.h)
3. **Model Processing:**
   - **Tubescreamer:** Emitter follower → Soft clipper with feedback loop attack capacitor → Active tone control → Output stage
   - **DS-1:** Emitter follower → Asymmetric transistor booster → Hard clipper → Big Muff-style tone stack → Output stage
4. **Downsampling:** 4x using polyphase FIR filters

### Component Hierarchy

```
OverdriveModule (VCV Rack Module)
    └── OverdriveDSP (Main DSP Engine)
        ├── Oversampler (Polyphase FIR)
        ├── SingleKnobNoiseGate (Wrapper around NoiseGate from Nam.h)
        ├── SoftClipper (TS-808/TS-9)
        │   ├── EmitterFollower (Input Buffer)
        │   ├── AttackCapacitorSwitch (6-position)
        │   ├── DiodeClipperFeedback (Soft clipping)
        │   └── OutputBuffer (Model-specific resistors)
        ├── HardClipper (DS-1)
        │   ├── EmitterFollower (Input Buffer)
        │   ├── TransistorBooster (Asymmetric)
        │   ├── DiodeClipperShunt (Hard clipping)
        │   └── OutputBuffer
        ├── TubeScreamerTone (Active tone control)
        └── DS1Tone (Big Muff-style)
```

---

## File Structure

### Directory Layout

```
src/
├── Overdrive.hpp                      # Module declaration (VCV Rack interface)
├── Overdrive.cpp                      # Module implementation (process loop, CV)
├── OverdriveWidget.cpp                # Panel widget and UI components
├── dsp/
│   ├── OverdriveDSP.h                 # Main DSP engine
│   ├── OverdriveDSP.cpp               # Implementation
│   ├── Oversampler.h                  # Polyphase oversampling
│   ├── Oversampler.cpp                # Implementation
│   ├── SingleKnobNoiseGate.h          # Noise gate wrapper
│   ├── SingleKnobNoiseGate.cpp        # Implementation
│   ├── SoftClipper.h                  # TS soft clipping stage
│   ├── SoftClipper.cpp                # Implementation
│   ├── HardClipper.h                  # DS-1 hard clipping stage
│   ├── HardClipper.cpp                # Implementation
│   ├── ToneStack.h                    # Tone control implementations
│   ├── ToneStack.cpp                  # Implementation
│   ├── TransistorStage.h              # BJT models (buffers, booster)
│   └── TransistorStage.cpp            # Implementation
│
res/
├── Overdrive.svg                      # Panel placeholder (8HP)
└── components/
    └── (reuse existing component graphics from CabSim)

test/
├── test_OverdriveDSP.cpp              # DSP engine tests
├── test_Oversampler.cpp               # Oversampling tests
├── test_SoftClipper.cpp               # Soft clipper tests
├── test_HardClipper.cpp               # Hard clipper tests
├── test_ToneStack.cpp                 # Tone stack tests
├── test_TransistorStage.cpp           # Transistor model tests
└── test_NoiseGate.cpp                 # Noise gate tests

docs/
├── Overdrive-implementation-guide.md  # This comprehensive guide
└── Overdrive-implementation-plan.md   # This document
```

---

## Implementation Phases

### Phase 1: Core DSP Components (Week 1-2)

**Goal:** Build and test all DSP components in isolation

**Tasks:**
1. Implement oversampler with polyphase filters
2. Implement noise gate wrapper
3. Implement soft clipper (Tubescreamer)
4. Implement hard clipper (DS-1)
5. Implement tone stacks
6. Implement transistor stages
7. Write unit tests for each component

**Deliverables:**
- All DSP header/source files
- Passing unit tests for each component
- Documented component APIs

### Phase 2: VCV Rack Module Integration (Week 2-3)

**Goal:** Integrate DSP into VCV Rack module

**Tasks:**
1. Create module skeleton
2. Integrate OverdriveDSP engine
3. Implement parameter mapping
4. Implement CV modulation (with Attack quantization)
5. Add model switching logic
6. Test in VCV Rack environment

**Deliverables:**
- Functioning VCV Rack module
- All parameters controllable
- Model switching working
- CV modulation functional

### Phase 3: Panel and UI (Week 3)

**Goal:** Create placeholder panel and polish UI

**Tasks:**
1. Design 8HP panel layout
2. Create SVG placeholder
3. Implement widget
4. Add knobs and switches
5. Add input/output jacks
6. Test UI responsiveness

**Deliverables:**
- 8HP panel SVG
- Fully functional UI
- Proper parameter tooltips

### Phase 4: Testing (Week 4)

**Goal:** Comprehensive unit testing

**Tasks:**
1. Write unit tests for all DSP components
2. Test model switching
3. Test CV modulation
4. Test edge cases and bounds
5. Frequency response validation
6. Harmonic content analysis

**Deliverables:**
- Complete test suite
- Test coverage report
- Performance benchmarks

### Phase 5: Optimization (Week 4-5)

**Goal:** Optimize for CPU efficiency

**Tasks:**
1. Profile CPU usage
2. Optimize hot paths
3. SIMD optimization (if needed)
4. Memory allocation optimization
5. Final performance validation

**Deliverables:**
- CPU usage < 5% target
- Profiling reports
- Optimization notes

---

## Phase 1: Core DSP Components

### Task 1.1: Polyphase Oversampler

**File:** `src/dsp/Oversampler.h`, `src/dsp/Oversampler.cpp`

**Purpose:** Efficient 4x oversampling/downsampling to minimize aliasing

**Design:**

```cpp
class Oversampler {
public:
    Oversampler();
    void setSampleRate(double sr);
    void setOversamplingFactor(int factor);  // Default: 4x
    
    // Process one input sample, returns 4 upsampled samples
    void upsample(float input, float output[4]);
    
    // Process 4 samples, returns one downsampled sample
    float downsample(float input[4]);
    
    void reset();

private:
    int factor = 4;
    double sampleRate = 48000.0;
    
    // Polyphase filter banks
    std::vector<std::vector<float>> upsampleFilters;
    std::vector<std::vector<float>> downsampleFilters;
    
    // State buffers
    std::vector<float> upsampleState;
    std::vector<float> downsampleState;
    
    void designFilters();
};
```

**Implementation Details:**
- Use polyphase decomposition for efficiency
- FIR filter design: Kaiser window, cutoff at 0.45 * Nyquist
- Filter length: 32 taps per phase (balance quality vs CPU)
- Direct-form FIR implementation (cache-friendly)

**Test Cases:**
- DC signal preservation
- Nyquist frequency attenuation
- Impulse response
- Frequency sweep (20Hz-20kHz at original SR)
- Phase linearity

**Reference:** [VCV Rack's src/dsp/resampler.cpp](https://github.com/VCVRack/Rack/blob/v2/src/dsp/resampler.cpp)

---

### Task 1.2: Single-Knob Noise Gate

**File:** `src/dsp/SingleKnobNoiseGate.h`, `src/dsp/SingleKnobNoiseGate.cpp`

**Purpose:** Simple threshold-only noise gate wrapping existing NoiseGate

**Design:**

```cpp
class SingleKnobNoiseGate {
public:
    SingleKnobNoiseGate();
    
    void setSampleRate(double sr);
    void setThreshold(float knobValue);  // 0-1, maps to -20 to -80 dB
    
    float process(float input);
    bool isOpen() const;
    void reset();

private:
    NoiseGate gate;  // From Nam.h
    
    // Fixed parameters
    static constexpr float ATTACK_MS = 1.0f;
    static constexpr float RELEASE_MS = 100.0f;
    static constexpr float HOLD_MS = 50.0f;
    static constexpr float HYSTERESIS_DB = 6.0f;
    
    void updateThreshold(float value);
};
```

**Implementation Details:**
- Threshold mapping: `thresholdDb = -20.0f - value * 60.0f`
- Fixed timing optimized for guitar
- Reuse NoiseGate from `src/dsp/Nam.h` (lines 92-180)

**Test Cases:**
- Threshold mapping accuracy
- Gate opens/closes at correct levels
- Hysteresis prevents chattering
- Attack/release timing

---

### Task 1.3: Soft Clipper (Tubescreamer)

**File:** `src/dsp/SoftClipper.h`, `src/dsp/SoftClipper.cpp`

**Purpose:** TS-808/TS-9 soft clipping stage with attack capacitor switching

**Design:**

```cpp
class SoftClipper {
public:
    SoftClipper();
    
    void setSampleRate(double sr);
    void setDrive(float drive);        // 0-1, maps to 0-500K pot
    void setAttackPosition(int pos);   // 0-5, capacitor selection
    void setModel(OverdriveModel model); // TS808 or TS9
    
    float process(float input);
    void reset();

private:
    double sampleRate = 48000.0;
    float drive = 0.5f;
    int attackPosition = 3;  // Default: 47nF (standard TS)
    OverdriveModel model = OverdriveModel::TS808;
    
    // Tubescreamer circuit components
    static constexpr float R4 = 4700.0f;     // 4.7K to ground
    static constexpr float R6 = 51000.0f;    // 51K series
    static constexpr float C4 = 51e-12f;     // 51pF LPF
    static constexpr float VF_DIODE = 1.0f;  // MA150 forward voltage
    
    // Attack capacitor values (6 positions)
    static constexpr float ATTACK_CAPS[6] = {
        470e-9f, 220e-9f, 100e-9f, 47e-9f, 22e-9f, 10e-9f
    };
    
    // Filter states
    float hpState = 0.0f;  // High-pass (feedback loop)
    float lpState = 0.0f;  // Low-pass (across diodes)
    
    // Output buffer state (model-specific)
    float outputRB = 100.0f;   // TS-808: 100Ω, TS-9: 470Ω
    float outputRC = 10000.0f; // TS-808: 10K, TS-9: 100K
    
    float softClipDiode(float x);
};
```

**Implementation Details:**
- Gain: `G = 1 + (R6 + drive*500K) / R4`
- High-pass in feedback: `fc = 1 / (2π * R4 * C_attack)`
- Soft clipping: Piecewise diode approximation
- Low-pass across diodes: Frequency-dependent on gain
- Output stage: Simple resistive divider (model-specific)

**Test Cases:**
- Gain range (12x to 118x)
- Soft clipping symmetry
- Attack capacitor frequency response
- TS-808 vs TS-9 output impedance
- Harmonic content analysis

---

### Task 1.4: Hard Clipper (DS-1)

**File:** `src/dsp/HardClipper.h`, `src/dsp/HardClipper.cpp`

**Purpose:** DS-1 hard clipping with transistor booster

**Design:**

```cpp
class HardClipper {
public:
    HardClipper();
    
    void setSampleRate(double sr);
    void setDrive(float drive);  // 0-1, maps to 0-100K pot
    
    float process(float input);
    void reset();

private:
    double sampleRate = 48000.0;
    float drive = 0.5f;
    
    // Op-amp stage components
    static constexpr float R13 = 4700.0f;    // 4.7K to ground
    static constexpr float C8 = 0.47e-6f;    // 0.47uF HPF
    static constexpr float C10 = 0.01e-6f;   // 0.01uF (parallel diodes)
    static constexpr float R14 = 2200.0f;    // 2.2K output
    static constexpr float VF_DIODE = 0.7f;  // 1N4148 forward voltage
    
    // Filter states
    float hpState = 0.0f;  // High-pass (72 Hz)
    float lpState = 0.0f;  // Low-pass (7.2 kHz)
    
    float hardClipDiode(float x);
};
```

**Implementation Details:**
- Gain: `G = 1 + drive*100K / R13` (1x to 22.3x)
- High-pass: `fc = 72 Hz` (R13, C8)
- Hard clipping: `clamp(x, -0.7, 0.7)`
- Low-pass: `fc = 7.2 kHz` (R14, C10)

**Test Cases:**
- Gain range (1x to 22x)
- Hard clipping accuracy
- Frequency response (HPF + LPF)
- Harmonic content (asymmetric distortion)

---

### Task 1.5: Tone Stacks

**File:** `src/dsp/ToneStack.h`, `src/dsp/ToneStack.cpp`

**Purpose:** TS active tone control and DS-1 Big Muff-style tone

**Design:**

```cpp
// Tubescreamer Active Tone Control
class TubeScreamerTone {
public:
    TubeScreamerTone();
    
    void setSampleRate(double sr);
    void setTone(float tone);  // 0 (bass) to 1 (treble)
    
    float process(float input);
    void reset();

private:
    double sampleRate = 48000.0;
    float tone = 0.5f;
    
    // Component values
    static constexpr float R7 = 1000.0f;   // 1K main LPF
    static constexpr float C5 = 0.22e-6f;  // 0.22uF main LPF
    static constexpr float R8 = 220.0f;    // 220Ω
    static constexpr float C6 = 0.22e-6f;  // 0.22uF
    
    // Filter states
    float mainLpfState = 0.0f;
    float toneHpfState = 0.0f;
};

// DS-1 Big Muff-Style Tone
class DS1Tone {
public:
    DS1Tone();
    
    void setSampleRate(double sr);
    void setTone(float tone);  // 0 (bass) to 1 (treble)
    
    float process(float input);
    void reset();

private:
    double sampleRate = 48000.0;
    float tone = 0.5f;
    
    // Component values
    static constexpr float R16 = 6800.0f;   // 6.8K LPF
    static constexpr float C12 = 0.1e-6f;   // 0.1uF
    static constexpr float R17 = 6800.0f;   // 6.8K HPF
    static constexpr float C11 = 0.022e-6f; // 0.022uF
    
    // Filter states
    float lpfState = 0.0f;
    float hpfState = 0.0f;
};
```

**Implementation Details:**
- **TS Tone:** Active bandpass, blends bass/treble
  - Main LPF: `fc = 723 Hz`
  - Treble boost: HPF at ~3.2 kHz
- **DS-1 Tone:** Passive crossfade between LPF/HPF
  - LPF: `fc = 234 Hz`
  - HPF: `fc = 1063 Hz`

**Test Cases:**
- Frequency response at tone=0, 0.5, 1
- Mid-scoop characteristics (DS-1)
- Bass/treble boost range

---

### Task 1.6: Transistor Stages

**File:** `src/dsp/TransistorStage.h`, `src/dsp/TransistorStage.cpp`

**Purpose:** BJT models for buffers and DS-1 booster

**Design:**

```cpp
// Simple emitter follower (unity gain buffer)
class EmitterFollower {
public:
    EmitterFollower();
    
    void setSampleRate(double sr);
    float process(float input);
    void reset();

private:
    double sampleRate = 48000.0;
    
    // Model parameters (2SC1815/2SC2240)
    static constexpr float VBE_DROP = 0.65f;  // Base-emitter voltage
    static constexpr float VCC = 9.0f;        // Supply voltage
    static constexpr float BIAS = 4.5f;       // DC bias
    
    // DC blocking
    float dcBlockState = 0.0f;
};

// DS-1 Transistor Booster (common emitter)
class TransistorBooster {
public:
    TransistorBooster();
    
    void setSampleRate(double sr);
    float process(float input);
    void reset();

private:
    double sampleRate = 48000.0;
    
    // Circuit components
    static constexpr float R_COLLECTOR = 10000.0f;  // 10K
    static constexpr float R_EMITTER = 22.0f;       // 22Ω
    static constexpr float VCC = 9.0f;
    
    // Filter states (input coupling, output DC block)
    float inputHpfState = 0.0f;
    float outputDcBlockState = 0.0f;
    
    float asymmetricClip(float x);
};
```

**Implementation Details:**
- **Emitter Follower:** Simple VBE drop model
- **Transistor Booster:** Asymmetric gain stage
  - Gain: ~35 dB (56x theoretical)
  - Positive swing: Soft saturation (tanh)
  - Negative swing: Harder cutoff (transistor off)
  - Input HPF: `fc = 33 Hz`

**Test Cases:**
- Unity gain for emitter follower
- Asymmetric clipping (booster)
- Gain accuracy (~35 dB)
- Even-order harmonics

---

### Task 1.7: Main DSP Engine

**File:** `src/dsp/OverdriveDSP.h`, `src/dsp/OverdriveDSP.cpp`

**Purpose:** Integrate all components into unified DSP engine

**Design:**

```cpp
enum class OverdriveModel {
    TS808,
    TS9,
    DS1
};

class OverdriveDSP {
public:
    OverdriveDSP();
    ~OverdriveDSP() = default;
    
    // Configuration
    void setSampleRate(double sr);
    void setModel(OverdriveModel model);
    OverdriveModel getModel() const { return currentModel; }
    
    // Parameters
    void setDrive(float drive);        // 0-1
    void setTone(float tone);          // 0-1
    void setLevel(float level);        // 0-1
    void setAttack(int position);      // 0-5
    void setGate(float threshold);     // 0-1
    
    // Processing
    float process(float input);
    void reset();
    
    // Monitoring
    bool isGateOpen() const;

private:
    OverdriveModel currentModel = OverdriveModel::TS808;
    double sampleRate = 48000.0;
    
    // DSP Components
    Oversampler oversampler;
    SingleKnobNoiseGate noiseGate;
    
    // Model-specific chains
    EmitterFollower inputBuffer;
    SoftClipper softClipper;           // TS-808/TS-9
    HardClipper hardClipper;           // DS-1
    TransistorBooster transistorBooster; // DS-1 only
    TubeScreamerTone tsTone;
    DS1Tone dsTone;
    EmitterFollower outputBuffer;
    
    // Output
    float outputLevel = 1.0f;
    
    void updateModelComponents();
};
```

**Implementation Details:**

```cpp
float OverdriveDSP::process(float input) {
    // Upsample to 4x
    float upsampled[4];
    oversampler.upsample(input, upsampled);
    
    // Process each oversampled sample
    float downsampled[4];
    for (int i = 0; i < 4; i++) {
        float sample = upsampled[i];
        
        // Noise gate (before distortion)
        sample = noiseGate.process(sample);
        
        // Model-specific processing
        switch (currentModel) {
            case OverdriveModel::TS808:
            case OverdriveModel::TS9:
                sample = inputBuffer.process(sample);
                sample = softClipper.process(sample);
                sample = tsTone.process(sample);
                sample = outputBuffer.process(sample);
                break;
                
            case OverdriveModel::DS1:
                sample = inputBuffer.process(sample);
                sample = transistorBooster.process(sample);
                sample = hardClipper.process(sample);
                sample = dsTone.process(sample);
                sample = outputBuffer.process(sample);
                break;
        }
        
        downsampled[i] = sample;
    }
    
    // Downsample to original rate
    float output = oversampler.downsample(downsampled);
    
    // Apply output level
    return output * outputLevel;
}
```

**Test Cases:**
- Model switching seamless
- All parameters applied correctly
- Signal chain integrity
- No DC offset
- No buffer overflow

---

## Phase 2: VCV Rack Module Integration

### Task 2.1: Module Skeleton

**File:** `src/Overdrive.hpp`

**Design:**

```cpp
#pragma once
#include "rack.hpp"
#include "dsp/OverdriveDSP.h"

using namespace rack;

struct Overdrive : Module {
    enum ParamId {
        MODEL_PARAM,     // 3-way switch: 0=TS808, 1=TS9, 2=DS1
        DRIVE_PARAM,     // 0-1
        TONE_PARAM,      // 0-1
        LEVEL_PARAM,     // 0-1
        ATTACK_PARAM,    // 0-5 (step knob)
        GATE_PARAM,      // 0-1
        PARAMS_LEN
    };
    
    enum InputId {
        AUDIO_INPUT,
        DRIVE_CV,
        TONE_CV,
        LEVEL_CV,
        ATTACK_CV,       // Quantized to 0-5
        GATE_CV,
        INPUTS_LEN
    };
    
    enum OutputId {
        AUDIO_OUTPUT,
        OUTPUTS_LEN
    };
    
    enum LightId {
        GATE_LIGHT,      // Green when gate is open
        LIGHTS_LEN
    };
    
    Overdrive();
    void process(const ProcessArgs& args) override;
    void onSampleRateChange() override;
    void onReset() override;
    
    json_t* dataToJson() override;
    void dataFromJson(json_t* rootJ) override;

private:
    OverdriveDSP dsp;
    
    // CV smoothing
    dsp::ExponentialSmoother driveSmoother;
    dsp::ExponentialSmoother toneSmoother;
    dsp::ExponentialSmoother levelSmoother;
    dsp::ExponentialSmoother gateSmoother;
};
```

---

### Task 2.2: Process Loop

**File:** `src/Overdrive.cpp`

**Implementation:**

```cpp
void Overdrive::process(const ProcessArgs& args) {
    // Get model selection
    int modelInt = static_cast<int>(params[MODEL_PARAM].getValue());
    OverdriveModel model = static_cast<OverdriveModel>(modelInt);
    dsp.setModel(model);
    
    // Get parameters with CV modulation
    float drive = params[DRIVE_PARAM].getValue();
    if (inputs[DRIVE_CV].isConnected()) {
        drive += inputs[DRIVE_CV].getVoltage() / 10.f;
        drive = clamp(drive, 0.f, 1.f);
    }
    drive = driveSmoother.process(args.sampleTime, drive);
    dsp.setDrive(drive);
    
    float tone = params[TONE_PARAM].getValue();
    if (inputs[TONE_CV].isConnected()) {
        tone += inputs[TONE_CV].getVoltage() / 10.f;
        tone = clamp(tone, 0.f, 1.f);
    }
    tone = toneSmoother.process(args.sampleTime, tone);
    dsp.setTone(tone);
    
    float level = params[LEVEL_PARAM].getValue();
    if (inputs[LEVEL_CV].isConnected()) {
        level += inputs[LEVEL_CV].getVoltage() / 10.f;
        level = clamp(level, 0.f, 1.f);
    }
    level = levelSmoother.process(args.sampleTime, level);
    dsp.setLevel(level);
    
    // Attack with quantization (using VCV Rack's rescale method)
    int attack = static_cast<int>(params[ATTACK_PARAM].getValue());
    if (inputs[ATTACK_CV].isConnected()) {
        float cv = clamp(inputs[ATTACK_CV].getVoltage(), 0.f, 10.f);
        int cvPos = static_cast<int>(std::round(rescale(cv, 0.f, 10.f, 0.f, 5.f)));
        attack = clamp(attack + cvPos, 0, 5);
    }
    dsp.setAttack(attack);
    
    // Gate threshold
    float gate = params[GATE_PARAM].getValue();
    if (inputs[GATE_CV].isConnected()) {
        gate += inputs[GATE_CV].getVoltage() / 10.f;
        gate = clamp(gate, 0.f, 1.f);
    }
    gate = gateSmoother.process(args.sampleTime, gate);
    dsp.setGate(gate);
    
    // Process audio
    float input = inputs[AUDIO_INPUT].getVoltage();
    float output = dsp.process(input);
    outputs[AUDIO_OUTPUT].setVoltage(output);
    
    // Update gate LED
    lights[GATE_LIGHT].setBrightness(dsp.isGateOpen() ? 1.f : 0.f);
}
```

---

### Task 2.3: Widget Implementation

**File:** `src/OverdriveWidget.cpp`

**Design:**

```cpp
struct OverdriveWidget : ModuleWidget {
    OverdriveWidget(Overdrive* module) {
        setModule(module);
        setPanel(createPanel(
            asset::plugin(pluginInstance, "res/Overdrive.svg")
        ));
        
        // Screws (optional, 8HP corners)
        addChild(createWidget<ScrewSilver>(Vec(0, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 15, 0)));
        addChild(createWidget<ScrewSilver>(Vec(0, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 15, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        
        // Model selector (3-way switch) - Top
        addParam(createParamCentered<CKSSThree>(
            mm2px(Vec(20.32, 20.0)),  // 8HP center = 20.32mm
            module,
            Overdrive::MODEL_PARAM
        ));
        
        // Knobs (reuse existing components from CabSim)
        addParam(createParamCentered<RoundBlackKnob>(
            mm2px(Vec(20.32, 35.0)), module, Overdrive::DRIVE_PARAM
        ));
        
        addParam(createParamCentered<RoundBlackKnob>(
            mm2px(Vec(20.32, 50.0)), module, Overdrive::TONE_PARAM
        ));
        
        addParam(createParamCentered<RoundBlackKnob>(
            mm2px(Vec(20.32, 65.0)), module, Overdrive::LEVEL_PARAM
        ));
        
        // Attack: Step knob (6 positions)
        addParam(createParamCentered<RoundBlackSnapKnob>(
            mm2px(Vec(20.32, 80.0)), module, Overdrive::ATTACK_PARAM
        ));
        
        addParam(createParamCentered<RoundSmallBlackKnob>(
            mm2px(Vec(20.32, 95.0)), module, Overdrive::GATE_PARAM
        ));
        
        // Audio I/O
        addInput(createInputCentered<PJ301MPort>(
            mm2px(Vec(10.0, 110.0)), module, Overdrive::AUDIO_INPUT
        ));
        
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(30.64, 110.0)), module, Overdrive::AUDIO_OUTPUT
        ));
        
        // CV inputs (small jacks, clustered at bottom)
        float cvStartY = 100.0;
        float cvSpacing = 8.0;
        
        addInput(createInputCentered<PJ301MPort>(
            mm2px(Vec(7.0, cvStartY)), module, Overdrive::DRIVE_CV
        ));
        addInput(createInputCentered<PJ301MPort>(
            mm2px(Vec(7.0, cvStartY + cvSpacing)), module, Overdrive::TONE_CV
        ));
        addInput(createInputCentered<PJ301MPort>(
            mm2px(Vec(7.0, cvStartY + cvSpacing*2)), module, Overdrive::LEVEL_CV
        ));
        addInput(createInputCentered<PJ301MPort>(
            mm2px(Vec(33.64, cvStartY)), module, Overdrive::ATTACK_CV
        ));
        addInput(createInputCentered<PJ301MPort>(
            mm2px(Vec(33.64, cvStartY + cvSpacing)), module, Overdrive::GATE_CV
        ));
        
        // Gate LED
        addChild(createLightCentered<SmallLight<GreenLight>>(
            mm2px(Vec(20.32, 95.0 + 5.0)), module, Overdrive::GATE_LIGHT
        ));
    }
};

Model* modelOverdrive = createModel<Overdrive, OverdriveWidget>("Overdrive");
```

---

## Phase 3: Panel and UI

### Task 3.1: Panel SVG

**File:** `res/Overdrive.svg`

**Requirements:**
- 8HP width (40.64mm)
- Simple placeholder design
- Labels for all controls
- Similar style to CabSim module

**Layout:**
```
Top: Logo/name
Model switch labels: 808 | 9 | DS1
Knob labels: DRIVE, TONE, LEVEL, ATTACK, GATE
Jack labels: IN, OUT
CV labels: D, T, L, A, G
```

**Implementation:**
- Use Inkscape or Adobe Illustrator
- Export as optimized SVG
- Test in VCV Rack for alignment

---

## Phase 4: Testing

### Task 4.1: Unit Test Suite

**Test Files:**
- `test/test_Oversampler.cpp`
- `test/test_NoiseGate.cpp`
- `test/test_SoftClipper.cpp`
- `test/test_HardClipper.cpp`
- `test/test_ToneStack.cpp`
- `test/test_TransistorStage.cpp`
- `test/test_OverdriveDSP.cpp`

**Test Framework:** Google Test (gtest)

**Test Categories:**

1. **Component Tests:**
   - Parameter bounds
   - State reset
   - Sample rate changes
   - Edge cases (0, 1, inf, -inf, NaN)

2. **Signal Integrity:**
   - DC offset
   - Gain accuracy
   - Frequency response
   - Harmonic content

3. **Integration Tests:**
   - Model switching
   - CV modulation
   - Attack quantization
   - Gate behavior

**Coverage Target:** > 90% line coverage

---

### Task 4.2: Frequency Response Validation

**Tool:** Custom test harness with FFT

**Tests:**
- Sweep 20Hz-20kHz at base sample rate
- Measure response at each model/parameter setting
- Verify filter corner frequencies
- Check for aliasing artifacts

**Validation Criteria:**
- HPF cutoffs within ±10% of design
- LPF cutoffs within ±10% of design
- Stopband attenuation > 40dB
- Passband ripple < 1dB

---

### Task 4.3: Harmonic Distortion Analysis

**Tool:** FFT analysis on distorted signals

**Tests:**
- 440Hz sine wave at various drive levels
- Measure THD (Total Harmonic Distortion)
- Analyze harmonic spectrum (2f, 3f, 4f...)
- Compare TS vs DS-1 characteristics

**Expected Results:**
- **TS-808/TS-9:** Predominantly odd-order harmonics (soft clipping)
- **DS-1:** Strong even-order harmonics (asymmetric booster) + odd (hard clipping)

---

## Phase 5: Optimization

### Task 5.1: CPU Profiling

**Tools:**
- VCV Rack's built-in CPU meter
- Instruments (macOS)
- perf (Linux)
- Visual Studio Profiler (Windows)

**Profile:**
- Baseline performance
- Identify hotspots
- Measure per-component cost

**Target:** < 5% CPU on Intel i5 @ 48kHz

---

### Task 5.2: Optimization Strategies

**Priorities:**

1. **Oversampling:** Biggest CPU cost
   - Optimize polyphase filter loops
   - Consider SSE/SIMD intrinsics
   - Cache-friendly memory access

2. **Clipping Functions:**
   - Approximate tanh with polynomial (Pade)
   - LUT for diode curves (if needed)

3. **Filters:**
   - Direct-form I for 1st-order (best cache)
   - Minimize state variables

4. **Memory:**
   - Stack allocation only (no heap in process loop)
   - Align filter states to cache lines

**SIMD Considerations:**
- 4x oversampling naturally fits SSE (4 floats)
- Use `_mm_*` intrinsics if needed
- Fallback to scalar for portability

---

## Dependencies

### External Libraries

1. **VCV Rack SDK:** v2.x (latest stable)
2. **Google Test:** For unit testing
3. **Existing Code:**
   - `src/dsp/Nam.h` - NoiseGate class (lines 92-180)
   - CabSim panel graphics for component reuse

### Build System

- Makefile (VCV Rack standard)
- C++17 standard
- Compiler: GCC/Clang (Linux/macOS), MSVC (Windows)

---

## Performance Targets

### CPU Usage

| Sample Rate | Target CPU | Max CPU |
|-------------|------------|---------|
| 44.1 kHz    | 3%         | 5%      |
| 48 kHz      | 3.5%       | 5.5%    |
| 96 kHz      | 7%         | 10%     |

### Latency

- **Oversampling:** 4x introduces ~32 samples latency (0.67ms @ 48kHz)
- **Target:** < 2ms total latency
- **Acceptable:** < 5ms

### Memory

- **Static allocation:** < 1MB per instance
- **No heap allocation in process loop**

---

## Testing Strategy

### Unit Test Coverage

**Target:** > 90% line coverage

**Test Levels:**

1. **Component-level:**
   - Each DSP class in isolation
   - Mock dependencies
   - Test all public methods

2. **Integration:**
   - OverdriveDSP with all components
   - Model switching
   - Parameter interactions

3. **Edge Cases:**
   - Zero input
   - Maximum input (clipping)
   - NaN/Inf handling
   - Parameter extremes

### Test Execution

```bash
# Build tests
make tests

# Run all tests
./test/run_all_tests

# Run with coverage
./test/run_tests_with_coverage.sh

# Generate coverage report
gcov -r src/dsp/*.cpp
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

---

## Milestone Checklist

### Phase 1: Core DSP Components ✓

- [ ] Oversampler implemented and tested
- [ ] SingleKnobNoiseGate wrapper complete
- [ ] SoftClipper (TS) implemented and tested
- [ ] HardClipper (DS-1) implemented and tested
- [ ] Tone stacks implemented and tested
- [ ] Transistor stages implemented and tested
- [ ] OverdriveDSP engine integrated
- [ ] All unit tests passing
- [ ] Component documentation complete

### Phase 2: VCV Rack Module ✓

- [ ] Module skeleton created
- [ ] Process loop implemented
- [ ] CV modulation working
- [ ] Attack CV quantization verified
- [ ] Model switching functional
- [ ] Audio I/O tested
- [ ] Module compiles in VCV Rack
- [ ] Manual testing complete

### Phase 3: Panel and UI ✓

- [ ] Panel SVG designed (8HP)
- [ ] Widget implemented
- [ ] All controls positioned
- [ ] Labels and graphics complete
- [ ] UI responsive and intuitive
- [ ] Component graphics reused from CabSim

### Phase 4: Testing ✓

- [ ] Unit test suite complete
- [ ] Frequency response validated
- [ ] Harmonic content analyzed
- [ ] Edge cases covered
- [ ] Coverage > 90%
- [ ] All tests passing

### Phase 5: Optimization ✓

- [ ] CPU profiling complete
- [ ] Hotspots identified
- [ ] Optimizations implemented
- [ ] CPU usage < 5% target met
- [ ] Memory usage validated
- [ ] Performance documented

### Final Release ✓

- [ ] All phases complete
- [ ] Documentation finalized
- [ ] Changelog updated
- [ ] Git tagged (v1.0.0)
- [ ] Ready for distribution

---

## Notes and Considerations

### Design Decisions

1. **Mono-only:** Simplifies implementation, reduces CPU
2. **Attack Quantization:** Matches hardware rotary switch behavior
3. **Fixed Gate Parameters:** Simplifies UI, optimized for guitar
4. **4x Oversampling:** Balance between quality and CPU

### Future Enhancements (Post-v1.0)

- Polyphonic support (if CPU allows)
- Additional models (TS-10, MT-2, etc.)
- Preset system
- Stereo processing option
- Advanced anti-aliasing (ADAA)
- Neural amp modeling integration

### Known Limitations

- No true bypass (digital domain)
- Simplified transistor models (behavioral, not circuit-accurate)
- Fixed oversampling rate (no user control)
- No built-in cab simulation (use CabSim module)

---

**Document Version:** 1.0  
**Last Updated:** February 2026  
**Author:** Shortwav Labs  
**Status:** Ready for Implementation
