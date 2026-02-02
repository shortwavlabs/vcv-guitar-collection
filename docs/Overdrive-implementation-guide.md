# Overdrive Module Implementation Guide

## Table of Contents
1. [Overview](#overview)
2. [Circuit Analysis](#circuit-analysis)
3. [Digital Modeling Approaches](#digital-modeling-approaches)
4. [VCV Rack Implementation](#vcv-rack-implementation)
5. [Component-Level Modeling](#component-level-modeling)
6. [Attack Filter Implementation](#attack-filter-implementation)
7. [Noise Gate Integration](#noise-gate-integration)
8. [Code Structure](#code-structure)
9. [Testing and Validation](#testing-and-validation)
10. [References](#references)

---

## Overview

This document provides a comprehensive guide for implementing a faithful digital recreation of three classic overdrive pedals in VCV Rack:

- **Ibanez Tubescreamer TS-808** (1979-1980)
- **Ibanez Tubescreamer TS-9** (1981-present)
- **Boss DS-1 Distortion** (1978-present)

The module will feature:
- 3-way switch for model selection
- Faithful component-level circuit modeling
- Attack knob (6-position step knob: 0 = 72Hz HPF (Deep), 1-5 = HPF at increasing frequencies)
- One-knob noise gate (threshold control only, other parameters fixed)
- 8HP panel layout (similar to CabSim module)
- Mono processing only (no polyphony)
- Sophisticated oversampling with polyphase filters for minimal aliasing and CPU usage

---

## Circuit Analysis

### 1. Ibanez Tubescreamer TS-808

**Circuit Topology:**

```
Input → [Input Buffer] → [Clipping Amp] → [Tone Control] → [Volume] → [Output Buffer]
                    ↓                ↓
              JFET Bypass      Diode Clipper
```

**Key Components:**

| Component | Value | Purpose |
|-----------|-------|---------|
| Op-Amp | JRC4558D | Dual op-amp for gain stages |
| Clipping Diodes | MA150 × 2 | Soft clipping in feedback loop |
| Input Buffer | 2SC1815 (β=350) | Emitter follower, unity gain |
| R1 | 1KΩ | Input series protection |
| R2 | 510KΩ | Base bias resistor |
| R3 | 10KΩ | Emitter resistor |
| R4 | 4.7KΩ | Feedback to ground (sets gain) |
| R5 | 10KΩ | Op-amp + input bias |
| R6 | 51KΩ | Feedback series resistor |
| Distortion Pot | 500KΩ (log) | Variable gain control |
| C1 | 0.02µF | Input coupling cap |
| C2 | 1µF (NP) | Stage coupling cap |
| C3 | 0.047µF | High-pass filter in feedback |
| C4 | 51pF | Low-pass filter across diodes |
| C5 | 0.22µF | Main low-pass filter |
| C6 | 0.22µF | Tone control capacitor |
| R7 | 1KΩ | Main low-pass resistor |
| R8 | 220Ω | Tone control series resistor |
| Tone Pot | 20KΩ (G taper) | Tone control |
| Volume Pot | 100KΩ (audio) | Output level |
| RB | 100Ω | Output series resistor |
| RC | 10KΩ | Output shunt resistor |

**Clipping Stage Details:**

```
Gain formula: Gv = 1 + (R6 + R_DISTORTION) / R4

- Maximum gain (Distortion=0): Gv_max = 1 + (51K + 500K) / 4.7K = 118 (41dB)
- Minimum gain (Distortion=max): Gv_min = 1 + 51K / 4.7K = 12 (21dB)
```

**Frequency Response:**

- **High-pass filter** (R4, C3): fc = 720 Hz
- **Low-pass filter** (C4 across diodes): fc = 5.6K - 61.2K (varies with distortion)
- **Main LPF** (R7, C5): fc = 723 Hz
- **Tone control**: Active bandpass filter

**Output Stage:**

```
Output impedance: Z_out ≈ 1.2KΩ
- RB = 100Ω (TS-808) or 470Ω (TS-9)
- RC = 10KΩ (TS-808) or 100KΩ (TS-9)
```

### 2. Ibanez Tubescreamer TS-9

**Key Differences from TS-808:**

| Component | TS-808 | TS-9 |
|-----------|--------|------|
| RB (Series Output) | 100Ω | 470Ω |
| RC (Shunt to Ground) | 10KΩ | 100KΩ |
| Output Impedance | 1.2KΩ | Higher |

**Important:** The clipping stage, tone control, and all other circuitry are **identical** between TS-808 and TS-9. The only significant circuit difference is the two output resistors.

### 3. Boss DS-1 Distortion

**Circuit Topology:**

```
Input → [Input Buffer] → [Transistor Booster] → [Op-Amp Stage] → [Tone] → [Level] → [Output Buffer]
                                         ↓                    ↓
                                 Asymmetric Clip       Hard Clip Diodes
```

**Key Components:**

| Stage | Component | Value | Purpose |
|-------|-----------|-------|---------|
| **Input Buffer** | Q1 | 2SC2240 | Emitter follower |
| | R1 | 1KΩ | Input protection |
| | R2 | 470KΩ | Base bias |
| | R3 | 10KΩ | Emitter resistor |
| | C1 | 47nF | Input coupling |
| **Transistor Booster** | Q2 | 2SC2240 | Common emitter amp |
| | R4, R5 | 100KΩ | Bias resistors |
| | R7 | 470KΩ | Shunt feedback |
| | R8 | 10KΩ | Collector resistor |
| | R9 | 22Ω | Emitter resistor |
| | C2 | 0.47µF | HPF coupling |
| | C3 | 47nF | HPF coupling |
| | C4 | 250pF | Miller feedback |
| **Op-Amp Stage** | U1 | NJM2904L | Dual op-amp |
| | R10 | 47KΩ | + input bias |
| | R13 | 4.7KΩ | Feedback to ground |
| | R14 | 2.2KΩ | Output resistor |
| | VR1 (Dist) | 100KΩ | Gain control |
| | C5 | 68nF | Coupling cap |
| | C7 | 100pF | HF reduction |
| | C8 | 0.47µF | HPF in feedback |
| | C10 | 0.01µF | Parallel with diodes |
| | D4, D5 | 1N4148 × 2 | Hard clipping to ground |
| **Tone Stage** | R16 | 6.8KΩ | LPF resistor |
| | R17 | 6.8KΩ | HPF resistor |
| | C11 | 0.022µF | HPF capacitor |
| | C12 | 0.1µF | LPF capacitor |
| | VR3 | 20KΩ | Tone control |
| **Level** | VR2 | 100KΩ | Output level |

**Gain Calculations:**

```
Transistor Booster: Gv ≈ 56 (35dB)
Op-Amp Stage: Gv_max = 1 + 100K/4.7K = 22.3 (26.5dB)
Total gain: 35dB + 26.5dB = 61.5dB
```

**Frequency Response:**

- **Transistor Booster HPF**: fc = 33 Hz (C3, R5)
- **Op-Amp HPF**: fc = 72 Hz (C8, R13)
- **Output LPF**: fc ≈ 7.2 KHz (R14, C10)
- **Tone Stack**: Mid-scoop at ~500 Hz

**Clipping Characteristics:**

- **Transistor stage**: Asymmetric soft clipping (even-order harmonics)
- **Op-amp stage**: Symmetric hard clipping to ±0.7V

---

## Digital Modeling Approaches

### Whitebox Modeling (Physics-Based)

#### 1. Wave Digital Filters (WDF)

Wave Digital Filters provide a circuit-theoretic approach where each analog component is mapped to a digital wave port.

**Key Resources:**
- [jatinchowdhury18/WaveDigitalFilters](https://github.com/jatinchowdhury18/WaveDigitalFilters) - Advanced C++ WDF library
- [AndrewBelt/WDFplusplus](https://github.com/AndrewBelt/WDFplusplus) - Easy-to-use WDF collection
- [chowdsp_wdf](https://www.researchgate.net/publication/364689194_chowdsp_wdf_An_Advanced_C_Library_for_Wave_Digital_Circuit_Modelling) - Research paper

**Diode Clipper Implementation:**

```cpp
// Basic WDF diode clipper concept
class DiodeClipperWDF {
    // R-type series resistance
    // C-type shunt capacitor
    // Nonlinear diode element using Shockley equation

    float process(float input) {
        // Incident wave computation
        // Scattering with diode nonlinearity
        // Reflected wave computation
    }
};
```

#### 2. Circuit Analysis Derivation

Based on [David Yeh's PhD thesis](https://ccrma.stanford.edu/~dtyeh/papers/DavidYehThesissinglesided.pdf), circuits can be derived directly from schematics:

1. Analyze signal flow through the circuit
2. Model each stage with appropriate differential equations
3. Solve nonlinear equations using Newton-Raphson iteration

**Tubescreamer Clipping Stage:**

```
Non-inverting op-amp with diode feedback:
- State variables: op-amp output, capacitor states
- Nonlinearity: diode I-V characteristic in feedback loop
- Solution: Iterative solver at each sample
```

#### 3. Simplified Whitebox Models

For real-time performance, simplified models capture essential behavior:

```cpp
// Simplified Tubescreamer-style soft clipper
struct TubeScreamerClipper {
    float process(float x, float drive) {
        // Pre-emphasis high-pass (720 Hz)
        x = hpf_high.process(x);

        // Soft clipping with diode approximation
        float y = tanh_approx(x * drive);

        // De-emphasis low-pass (5.6K - 61K Hz)
        y = lpf_diode.process(y);

        return y;
    }
};
```

### Blackbox Modeling

#### Neural Network Approaches

- **Differentiable black-box/gray-box modeling** (2025 research)
- Neural ODEs for circuit modeling
- Training on captured impulse responses or tone sweeps

#### WaveShaping Functions

Static nonlinearities with appropriate filtering:

```cpp
// Various waveshaping functions
inline float soft_clip_tanh(float x) {
    return std::tanh(x);
}

inline float hard_clip(float x, float threshold = 0.7f) {
    return std::clamp(x, -threshold, threshold);
}

inline float variable_clip(float x, float k) {
    // k >= 1, where k=1 is soft, k->inf is hard
    if (std::abs(x) < 1.0f/k)
        return x;
    return std::copysign(1.0f - 1.0f/(k*std::abs(x) + k - 1.0f), x);
}
```

### Hybrid Approach (Recommended)

Combine whitebox circuit topology with efficient DSP implementations:

1. **Stage-level modeling**: Model each circuit block separately
2. **Component-level accuracy**: Use measured component values
3. **Efficient nonlinearities**: Use optimized approximations
4. **Frequency-dependent behavior**: Capture filtering at each stage

---

## VCV Rack Implementation

### Panel Design

**Layout: 8HP (40.64mm width)**

The module uses a compact 8HP panel layout similar to the CabSim module in the collection:

```
┌─────────────────┐
│   OVERDRIVE     │  
├─────────────────┤
│   [3-WAY SW]    │  Model selector (TS-808/TS-9/DS-1)
│                 │
│   ○ DRIVE       │  Drive/Distortion knob
│   ○ TONE        │  Tone control knob
│   ○ LEVEL       │  Output level knob
│   ◇ ATTACK      │  6-position step knob
│   ○ GATE        │  Noise gate threshold
│                 │
│   ● INPUT       │  Audio input jack
│   ● OUTPUT      │  Audio output jack
│                 │
│   ● DRIVE CV    │  CV input for drive
│   ● TONE CV     │  CV input for tone
│   ● LEVEL CV    │  CV input for level
│   ● ATTACK CV   │  CV input for attack (quantized)
│   ● GATE CV     │  CV input for gate threshold
└─────────────────┘
```

**Note:** A placeholder panel SVG will be created during implementation. Final graphic design will be completed separately.

### Module Structure

```cpp
struct OverdriveModule : Module {
    enum ParamId {
        // Model selection (3-way switch)
        MODEL_PARAM,          // 0 = TS-808, 1 = TS-9, 2 = DS-1

        // Standard controls
        DRIVE_PARAM,
        TONE_PARAM,
        LEVEL_PARAM,

        // Additional features
        ATTACK_PARAM,         // 6-position step knob (0-5)
        GATE_PARAM,           // Threshold-only noise gate

        PARAMS_LEN
    };

    enum InputId {
        AUDIO_INPUT,
        DRIVE_CV,
        TONE_CV,
        LEVEL_CV,
        ATTACK_CV,        // Quantized to discrete positions 0-5
        GATE_CV,
        INPUTS_LEN
    };

    enum OutputId {
        AUDIO_OUTPUT,
        OUTPUTS_LEN
    };

    // DSP instances
    std::unique_ptr<OverdriveDSP> odDsp;
    
    // Note: Module is MONO only, no polyphony support
};
```

### 3-Way Switch Implementation

Using VCV Rack's `CKSSThree` component:

```cpp
// In widget
addParam(createParamCentered<CKSSThree>(
    mm2px(Vec(10.0, 20.0)),
    module,
    OverdriveModule::MODEL_PARAM
));

// Model switching logic
void process(const ProcessArgs& args) override {
    int model = static_cast<int>(params[MODEL_PARAM].getValue());

    switch (model) {
        case 0: // TS-808
            odDsp->setModel(OverdriveModel::TS808);
            break;
        case 1: // TS-9
            odDsp->setModel(OverdriveModel::TS9);
            break;
        case 2: // DS-1
            odDsp->setModel(OverdriveModel::DS1);
            break;
    }
}
```

---

## Component-Level Modeling

### 1. Transistor & Buffer Models (New)

To capture the subtle coloration of the original pedals, we utilize a unified BJT (Bipolar Junction Transistor) model for both the input/output buffers and the DS-1 booster stage.

#### BJT Model (Ebers-Moll Approximation)

```cpp
struct BJT {
    // Transistor parameters (2SC1815 / 2SC2240)
    float Is = 1e-14f;    // Saturation current
    float Vt = 0.026f;    // Thermal voltage
    float beta = 350.0f;  // Current gain (hFE)

    // Compute collector current Ic based on Vbe
    float getIc(float Vbe) {
        return Is * (std::exp(Vbe / Vt) - 1.0f);
    }
};
```

#### Emitter Follower (Input/Output Buffers)

Used in TS-808/TS-9 (2SC1815) and DS-1 (2SC2240) for impedance matching. Note that real emitter followers are not perfectly linear; they exhibit slight asymmetric compression and a DC offset drop (~0.6V).

```cpp
struct EmitterFollower {
    float R_load = 10000.0f; // Emitter resistor
    float V_bias = 4.5f;     // DC bias voltage
    float V_cc = 9.0f;

    // Simplified iterative solution or approximation
    float process(float input) {
        // V_in is AC coupled + Bias
        float V_base = input + V_bias;
        
        // Emitter follows base minus diode drop, but depends on load current
        // V_out = V_base - V_be(Ic)
        // Iterative refinement for accuracy or use detailed equation
        float V_out_approx = V_base - 0.65f; 
        
        // Hard limits at rails
        return std::clamp(V_out_approx, 0.0f, V_cc);
    }
};
```

#### Common Emitter Amplifier (DS-1 Booster)

The DS-1's distinctive sound comes from this high-gain (~35dB) pre-amplification stage which drives the op-amp hard.

```cpp
struct TransistorBooster {
    // Component values from DS-1 schematic
    float R_collector = 10000.0f; // 10k
    float R_emitter = 22.0f;      // 22 ohms
    float V_cc = 9.0f;
    float bias = 4.5f;

    // State for DC blocking caps
    float hp_state = 0.0f;

    float process(float input) {
        // 1. High-pass filtering (input coupling)
        // ... (HPF implementation) ...
        
        // 2. Transistor Gain Stage
        // Asymmetric amplification:
        // - Positive swings saturate against rail
        // - Negative swings cutoff (transistor turns off)
        
        // Simplified behavioral model of Common Emitter with Emitter Degeneration:
        float gain = R_collector / R_emitter; // Approx 450x theoretical, limited by open loop
        
        // Nonlinear transfer function simulation
        float driving_signal = input * gain;
        
        // Soft asymmetry
        if (driving_signal > 0) {
             driving_signal = std::tanh(driving_signal); // Compresses
        } else {
             driving_signal = std::tanh(driving_signal * 1.2f); // Slightly harder cutoff
        }

        return driving_signal * 10.0f; // Scale to match 35dB target
    }
};
```

### 2. Diode Clipper Models

#### Soft Clipping (Tubescreamer)

**Circuit:** Diodes in feedback loop of non-inverting op-amp

```cpp
struct SoftClipper {
    // ... (previous SoftClipper code) ...


#### Hard Clipping (Boss DS-1)

**Circuit:** Diodes shunting output to ground

```cpp
struct HardClipper {
    // State
    float z1 = 0.0f;
    float sampleRate = 48000.0f;

    // Component values
    float R13 = 4700.0f;     // 4.7K to ground
    float C8 = 0.47e-6f;     // 0.47uF (HPF)
    float C10 = 0.01e-6f;    // 0.01uF (parallel with diodes)
    float R14 = 2200.0f;     // 2.2K output
    float Vf = 0.7f;         // Silicon diode forward voltage

    float hpfs1 = 0.0f;
    float lpfs1 = 0.0f;

    void setSampleRate(double sr) {
        sampleRate = sr;
    }

    float process(float input, float drive) {
        // Op-amp gain
        float R_drive = drive * 100000.0f;  // 0-100K
        float gain = 1.0f + R_drive / R13;

        float amplified = input * gain;

        // High-pass filter (R13, C8) - fc = 72 Hz
        float wc_hp = 2.0f * M_PI * 72.0f / sampleRate;
        float alpha_hp = wc_hp / (1.0f + wc_hp);
        float hp_out = alpha_hp * (amplified - hpfs1);
        hpfs1 += alpha_hp * (amplified - hpfs1);

        // Hard clipping (diodes to ground)
        float clipped = std::clamp(hp_out, -Vf, Vf);

        // Low-pass filter (R14, C10) - fc = 7.2 KHz (when diodes not conducting)
        // This is frequency-dependent due to diode loading
        float wc_lp = 2.0f * M_PI * 7200.0f / sampleRate;
        float alpha_lp = wc_lp / (1.0f + wc_lp);
        float output = alpha_lp * clipped + (1.0f - alpha_lp) * lpfs1;
        lpfs1 = output;

        return output;
    }
};
```

### 2. Tone Stack Models

#### Tubescreamer Active Tone Control

```cpp
struct TubeScreamerTone {
    float R7 = 1000.0f;      // 1K (main LPF)
    float C5 = 0.22e-6f;     // 0.22uF (main LPF)
    float R8 = 220.0f;       // 220 ohm
    float C6 = 0.22e-6f;     // 0.22uF
    float tone = 0.5f;       // 0-1 (bass to treble)

    float mainLpfState = 0.0f;
    float toneState = 0.0f;
    float sampleRate = 48000.0f;

    void setSampleRate(double sr) {
        sampleRate = sr;
    }

    float process(float input, float toneParam) {
        tone = toneParam;

        // Main low-pass filter (R7, C5) - fc = 723 Hz
        float wc_main = 2.0f * M_PI * 723.0f / sampleRate;
        float alpha_main = wc_main / (1.0f + wc_main);
        float mainOut = alpha_main * mainLpfState + (1.0f - alpha_main) * input;
        mainLpfState = mainOut;

        // Active tone control
        // Interpolate between bass and treble responses
        float trebleGain = tone * 2.0f;      // 0 to 2
        float bassGain = (1.0f - tone) * 2.0f; // 0 to 2

        // Simple approximation: blend filtered versions
        // For full accuracy, use the op-amp circuit analysis

        // High-pass for treble boost
        float wc_hp = 2.0f * M_PI * 3200.0f / sampleRate;
        float alpha_hp = wc_hp / (1.0f + wc_hp);
        float treble = mainOut + alpha_hp * (mainOut - toneState);
        toneState = treble;

        return mainOut * bassGain + treble * trebleGain;
    }
};
```

#### Boss DS-1 Tone Stack (Big Muff Style)

```cpp
struct DS1Tone {
    float R16 = 6800.0f;     // 6.8K (LPF)
    float C12 = 0.1e-6f;     // 0.1uF
    float R17 = 6800.0f;     // 6.8K (HPF)
    float C11 = 0.022e-6f;   // 0.022uF

    float lpfState = 0.0f;
    float hpfState = 0.0f;
    float sampleRate = 48000.0f;

    void setSampleRate(double sr) {
        sampleRate = sr;
    }

    float process(float input, float toneParam) {
        // tone: 0 = bass, 1 = treble

        // Low-pass filter (R16, C12) - fc = 234 Hz
        float wc_lp = 2.0f * M_PI * 234.0f / sampleRate;
        float alpha_lp = wc_lp / (1.0f + wc_lp);
        float lowOut = alpha_lp * lpfState + (1.0f - alpha_lp) * input;
        lpfState = lowOut;

        // High-pass filter (C11, R17) - fc = 1063 Hz
        float wc_hp = 2.0f * M_PI * 1063.0f / sampleRate;
        float alpha_hp = wc_hp / (1.0f + wc_hp);
        float highOut = alpha_hp * (input - hpfState);
        hpfState += alpha_hp * (input - hpfState);

        // Blend based on tone control
        float wet = toneParam;  // 0 = all low, 1 = all high
        return lowOut * (1.0f - wet) + highOut * wet;
    }
};
```

### 3. Complete Model Classes

```cpp
enum class OverdriveModel {
    TS808,
    TS9,
    DS1
};

class OverdriveDSP {
public:
    OverdriveDSP() {
        // Initialize all stages
    }

    void setModel(OverdriveModel model) {
        currentModel = model;
        updateComponentValues();
    }

    void setDrive(float drive) {
        // drive: 0-1, mapped to appropriate range
        switch (currentModel) {
            case OverdriveModel::TS808:
            case OverdriveModel::TS9:
                tsClipper.setDrive(drive);
                break;
            case OverdriveModel::DS1:
                dsClipper.setDrive(drive);
                break;
        }
    }

    void setTone(float tone) {
        // tone: 0-1
        switch (currentModel) {
            case OverdriveModel::TS808:
            case OverdriveModel::TS9:
                tsTone.setTone(tone);
                break;
            case OverdriveModel::DS1:
                dsTone.setTone(tone);
                break;
        }
    }

    void setLevel(float level) {
        outputGain = level;
    }

    float process(float input) {
        float output = input;

        switch (currentModel) {
            case OverdriveModel::TS808:
            case OverdriveModel::TS9:
                // Input buffer (emitter follower - unity gain)
                output = inputBuffer.process(output);

                // Clipping stage
                output = tsClipper.process(output);

                // Tone control
                output = tsTone.process(output);

                // Output buffer (with model-specific resistor values)
                output = outputBuffer.process(output, currentModel);
                break;

            case OverdriveModel::DS1:
                // Input buffer
                output = inputBuffer.process(output);

                // Transistor booster (asymmetric clipping)
                output = transistorBooster.process(output);

                // Op-amp hard clipping stage
                output = dsClipper.process(output);

                // Tone control
                output = dsTone.process(output);

                // Output buffer
                output = outputBuffer.process(output, currentModel);
                break;
        }

        return output * outputGain;
    }

    void setSampleRate(double sr) {
        sampleRate = sr;
        // Update all sample rates in sub-components
    }

private:
    OverdriveModel currentModel = OverdriveModel::TS808;
    double sampleRate = 48000.0;
    float outputGain = 1.0f;

    // Tubescreamer components
    SoftClipper tsClipper;
    TubeScreamerTone tsTone;

    // DS-1 components
    HardClipper dsClipper;
    DS1Tone dsTone;
    TransistorBooster transistorBooster;

    // Shared
    InputBuffer inputBuffer;
    OutputBuffer outputBuffer;

    void updateComponentValues() {
        switch (currentModel) {
            case OverdriveModel::TS808:
                // TS-808 values
                outputBuffer.setResistors(100.0f, 10000.0f);
                break;
            case OverdriveModel::TS9:
                // TS-9 values
                outputBuffer.setResistors(470.0f, 100000.0f);
                break;
            case OverdriveModel::DS1:
                // DS-1 values
                outputBuffer.setResistors(0.0f, 0.0f);
                break;
        }
    }
};
```

---

## Attack Filter Implementation

The attack knob is based on the **Horizon Devices Precision Drive** design. It is a **6-position step knob** (values 0-5) that controls the feedback loop capacitor in the clipping stage, shaping how much bass/low-mid frequencies are affected by the distortion.

### Design Rationale

**Sources:**
- [Horizon Devices User's Guide](https://horizondevices.com/pages/users-guide-pd) - Official documentation
- [DIYstompboxes Forum - Precision Drive Schematic Discussion](https://www.diystompboxes.com/smfforum/index.php?topic=126289.0) - Technical analysis
- [PedalPCB Dwarven Hammer Build Docs](https://docs.pedalpcb.com/project/PedalPCB-DwarvenHammer.pdf) - DIY clone documentation

### How It Works

The Precision Drive is essentially a Tube Screamer with a **6-way rotary switch** that changes the capacitor value in the op-amp feedback loop. This differs from a simple high-pass filter approach:

- **Left (counter-clockwise):** Larger capacitor = more lower-mids punch, old-school thick tone
- **Right (clockwise):** Smaller capacitor = tighter bass response, more defined and pick-y modern tone

### Circuit Implementation

The Attack switch controls capacitor C3 in the Tubescreamer's clipping stage feedback loop. With a 4.7KΩ resistor to ground (R4):

```
Corner frequency: fc = 1 / (2π × R4 × C3)
```

### Capacitor Value Mapping

| Position | Capacitor Value | Corner Frequency (fc) | Description |
|----------|-----------------|----------------------|-------------|
| 0 | 470 nF | ~72 Hz | Maximum bass, thickest low-end |
| 1 | 220 nF | ~153 Hz | Warm, full-range low-mids |
| 2 | 100 nF | ~338 Hz | Balanced, classic TS response |
| 3 | 47 nF | ~720 Hz | Standard Tubescreamer (original C3 value) |
| 4 | 22 nF | ~1,540 Hz | Tight, focused midrange |
| 5 | 10 nF | ~3,386 Hz | Tightest, most defined/pick-y |

**Note:** Position 3 (47nF) is the original Tubescreamer value.

### Filter Design

```cpp
// Feedback loop capacitor switch (Precision Drive style)
struct AttackCapacitorSwitch {
    double sampleRate = 48000.0;
    int position = 3;  // Default to standard TS value (47nF)

    // Tubescreamer feedback loop components
    float R4 = 4700.0f;  // 4.7K to ground
    std::vector<float> capValues = {
        470e-9f,  // Position 0: ~72 Hz (Deep Bass)
        220e-9f,  // Position 1: ~153 Hz
        100e-9f,  // Position 2: ~338 Hz
        47e-9f,   // Position 3: ~720 Hz (Standard TS)
        22e-9f,   // Position 4: ~1.5 kHz
        10e-9f    // Position 5: ~3.4 kHz
    };

    // Filter state (high-pass in feedback loop)
    float hpState = 0.0f;

    void setSampleRate(double sr) {
        sampleRate = sr;
    }

    void setPosition(int pos) {
        position = std::clamp(pos, 0, 5);
    }

    // Calculate corner frequency for current position
    float getCornerFrequency() const {
        float C = capValues[position];
        return 1.0f / (2.0f * M_PI * R4 * C);
    }

    // Process as high-pass filter in feedback loop
    float process(float input, float feedbackResistance) {
        float C = capValues[position];
        float R_total = R4 + feedbackResistance;

        // High-pass filter coefficient
        float wc = 2.0f * M_PI * R4 * C / (float)sampleRate;
        float alpha = wc / (1.0f + wc);

        hpState = alpha * (input - hpfs1);
        hpfs1 = hpState;

        return input - hpState;  // High-pass effect
    }

private:
    float hpfs1 = 0.0f;
};
```

### Integration in Soft Clipper

The Attack capacitor is integrated into the Tubescreamer soft clipping stage:

```cpp
struct SoftClipperWithAttack {
    // Standard Tubescreamer components
    float R4 = 4700.0f;      // 4.7K to ground
    float R6 = 51000.0f;     // 51K series resistor
    float C4 = 51e-12f;      // 51pF (LPF across diodes)
    float Vf = 1.0f;         // Diode forward voltage

    // Attack switch capacitor
    std::vector<float> attackCaps = {
        470e-9f, 220e-9f, 100e-9f, 47e-9f, 22e-9f, 10e-9f
    };
    int attackPosition = 3;  // Default to standard TS

    // Filter states
    float hpState = 0.0f;
    float lpState = 0.0f;
    double sampleRate = 48000.0;

    void setSampleRate(double sr) {
        sampleRate = sr;
    }

    void setAttackPosition(int pos) {
        attackPosition = std::clamp(pos, 0, 5);
    }

    float process(float input, float drive) {
        // Calculate effective resistance
        float R_dist = drive * 500000.0f;  // 0-500K
        float R_feedback = R6 + R_dist;

        // Gain calculation
        float gain = 1.0f + R_feedback / R4;

        // Get attack capacitor for current position
        float C_attack = attackCaps[attackPosition];

        // High-pass filter in feedback loop (R4, C_attack)
        float wc_hp = 2.0f * M_PI * R4 * C_attack / sampleRate;
        float alpha_hp = wc_hp / (1.0f + wc_hp);
        float hp_out = alpha_hp * (input - hpState);
        hpState = hp_out + alpha_hp * (input - hpState);

        // Apply gain to high-pass filtered signal
        float amplified = hp_out * gain;

        // Diode clipping (soft, in feedback)
        float clipped = softClipDiode(amplified, Vf);

        // Low-pass filter (C4 across diodes)
        float fc_lp = 1.0f / (2.0f * M_PI * (R_feedback * C4));
        float wc_lp = 2.0f * M_PI * fc_lp / sampleRate;
        float alpha_lp = wc_lp / (1.0f + wc_lp);
        float output = alpha_lp * clipped + (1.0f - alpha_lp) * lpState;
        lpState = output;

        return output;
    }

private:
    float softClipDiode(float x, float threshold) {
        if (std::abs(x) < threshold) {
            return x;
        }
        float sign = (x >= 0.0f) ? 1.0f : -1.0f;
        float excess = std::abs(x) - threshold;
        return sign * (threshold + excess * 0.1f);
    }
};
```

### Signal Chain Integration

The Attack capacitor is **part of the clipping stage** (not a separate filter before):

```
Input → [Noise Gate] → [Clipping Stage with Attack Cap] → [Tone] → [Output]
```

```cpp
float OverdriveDSP::process(float input) {
    // Apply noise gate first
    float gated = singleKnobGate.process(input);

    // Apply clipping (with attack capacitor in feedback loop)
    float clipped = tsClipper.process(gated, drive, attackPosition);

    // Continue with tone control...
    return tsTone.process(clipped) * outputGain;
}
```

### Parameter Mapping

```cpp
// Attack knob: discrete step knob with 6 positions (0-5)
void OverdriveDSP::setAttack(int position) {
    // position: 0-5 (from step knob)
    // Maps to capacitor values in feedback loop

    tsClipper.setAttackPosition(position);
}
```

### VCV Rack Parameter Configuration

```cpp
// In module constructor
enum ParamId {
    // ... other params
    ATTACK_PARAM,  // Step knob: 0-5
    // ...
};

void configParams() {
    // Attack: 6-position step knob
    configParam(ATTACK_PARAM, 0.f, 5.f, 3.f, "Attack");
    // Default to position 3 (47nF - original Tubescreamer)
}

// In process()
int attackPos = static_cast<int>(params[ATTACK_PARAM].getValue());

// Apply CV modulation (quantized to discrete steps using VCV Rack's rescale)
if (inputs[ATTACK_CV].isConnected()) {
    float cv = clamp(inputs[ATTACK_CV].getVoltage(), 0.f, 10.f);
    int cvPos = static_cast<int>(std::round(rescale(cv, 0.f, 10.f, 0.f, 5.f)));
    attackPos = clamp(attackPos + cvPos, 0, 5);
}

odDsp->setAttack(attackPos);
```

### Character by Position Summary

| Position | Capacitor | Corner Fc | Sound Character |
|----------|-----------|----------|-----------------|
| 0 | 470 nF | 72 Hz | Thick, full-range, doom-y low-end |
| 1 | 220 nF | 153 Hz | Warm, punchy low-mids |
| 2 | 100 nF | 338 Hz | Balanced, classic overdrive |
| 3 | 47 nF | 720 Hz | Original Tubescreamer |
| 4 | 22 nF | 1,540 Hz | Tight, focused midrange |
| 5 | 10 nF | 3,386 Hz | Modern, djent, super defined |

---

## Noise Gate Integration

The existing `NoiseGate` class from [src/dsp/Nam.h](src/dsp/Nam.h:92-180) is reused with a simplified single-knob interface that controls **only the threshold**. All other parameters are fixed to optimized values for guitar use.

### Research: Horizon Devices Precision Drive Noise Gate

**Original Precision Drive Noise Gate:**

The Horizon Devices Precision Drive includes a built-in noise gate that operates differently from traditional threshold-based gates:

| Feature | Precision Drive | Our Implementation |
|---------|-----------------|-------------------|
| **Control** | Gate knob adjusts response speed/sensitivity | Threshold knob (-20 to -80 dB) |
| **Threshold** | Fixed internally (not user-adjustable) | User-adjustable via knob |
| **Design** | Proprietary circuit (schematic not released) | RMS envelope with hysteresis |
| **Behavior** | Creates chopped, gated palm-mute tones | Traditional noise gating |

**Key Findings from Research:**

1. **No Public Schematic**: The original Precision Drive noise gate circuit is proprietary. No official schematic has been released ([DIYstompboxes Forum](https://www.diystompboxes.com/smfforum/index.php?topic=126289.0))

2. **Dwarven Hammer Omission**: The PedalPCB Dwarven Hammer (Precision Drive clone) intentionally **excludes** the noise gate circuit from the PCB design ([PedalPCB Forum](https://forum.pedalpcb.com/threads/any-interesting-or-cool-mods-to-the-dwarven-hammer-pcb-precision-drive-clone.18812/))

3. **Alternative Approaches**: Forum discussions suggest using:
   - JSX noise gate circuit as alternative
   - THAT 2181/2180 VCA-based designs (used in ISP Decimator)
   - Traditional RMS envelope detection (our chosen approach)

**Implementation Decision:**

Given the lack of publicly available documentation for the Precision Drive's proprietary noise gate, this implementation uses a **traditional threshold-based noise gate** with:
- RMS envelope detection (proven, well-understood technique)
- Hysteresis to prevent chattering
- Fixed timing parameters optimized for guitar
- Single-knob threshold control for simplicity

This approach provides effective noise reduction without attempting to reverse-engineer the proprietary Precision Drive circuit.

### Existing NoiseGate Features

```cpp
struct NoiseGate {
    // Parameters
    float threshold = -60.f;    // dB (-80 to 0)
    float attack = 0.0005f;     // seconds
    float release = 0.1f;       // seconds
    float hold = 0.05f;         // seconds
    float hysteresis = 6.f;     // dB

    // State
    float envelope = 0.f;
    float gain = 0.f;
    float holdCounter = 0.f;
    bool isOpen = false;

    // Methods
    float process(float sample);
    void reset();
    void setSampleRate(double sr);
    void setParameters(float threshDb, float attackMs, float releaseMs, float holdMs);
};
```

### Single-Knob Threshold Control

The gate knob controls only the threshold parameter. All other parameters are fixed to sensible defaults for guitar applications:

```cpp
struct SingleKnobNoiseGate {
    NoiseGate gate;

    // ===== FIXED PARAMETERS =====
    // These are hardcoded to optimal values for guitar
    static constexpr float FIXED_ATTACK_MS = 1.0f;      // 1 ms (fast attack)
    static constexpr float FIXED_RELEASE_MS = 100.0f;   // 100 ms (natural decay)
    static constexpr float FIXED_HOLD_MS = 50.0f;       // 50 ms (prevent chatter)
    static constexpr float FIXED_HYSTERESIS_DB = 6.0f;  // 6 dB (stable operation)

    void setSampleRate(double sr) {
        gate.setSampleRate(sr);
        // Initialize with fixed parameters and default threshold
        updateThreshold(0.5f);  // Start at middle position
    }

    // Set threshold from normalized knob value (0-1)
    // Maps to threshold range: -20 dB (open) to -80 dB (heavy gating)
    void setKnob(float value) {
        updateThreshold(value);
    }

    float process(float sample) {
        return gate.process(sample);
    }

    bool isOpen() const { return gate.isOpen; }
    void reset() { gate.reset(); }

private:
    void updateThreshold(float value) {
        // value: 0 (open/light gating) to 1 (closed/heavy gating)
        // threshold: -20 dB to -80 dB
        float thresholdDb = -20.0f - value * 60.0f;

        // Update gate with fixed parameters + variable threshold
        gate.setParameters(
            thresholdDb,
            FIXED_ATTACK_MS,
            FIXED_RELEASE_MS,
            FIXED_HOLD_MS
        );
    }
};
```

### Threshold Mapping Table

| Knob Position (0-1) | Threshold dB | Description |
|---------------------|--------------|-------------|
| 0.0 | -20 dB | Minimal gating, barely closed |
| 0.25 | -35 dB | Light noise reduction |
| 0.5 | -50 dB | Moderate gating |
| 0.75 | -65 dB | Heavy gating |
| 1.0 | -80 dB | Maximum noise suppression |

### Fixed Parameter Justification

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| Attack | 1 ms | Fast enough to catch transients, slow enough to avoid clicks |
| Release | 100 ms | Natural decay for guitar, prevents "pumping" |
| Hold | 50 ms | Prevents gate from chattering on decaying notes |
| Hysteresis | 6 dB | Ensures stable operation near threshold |

### Placement in Signal Chain

Place the noise gate **after the attack filter** but **before the clipping stage**:

```
Input → [Attack HPF] → [Noise Gate] → [Clipping Stage] → [Tone] → [Output]
```

This order ensures:
1. Attack filter shapes the signal first
2. Noise gate removes low-level noise before distortion amplification
3. Clipping stage operates on gated signal (noise isn't further amplified)

### VCV Rack Parameter Configuration

```cpp
// In module constructor
enum ParamId {
    // ... other params
    GATE_PARAM,  // Threshold-only control: 0-1
    // ...
};

void configParams() {
    // Gate: single knob controlling threshold only
    configParam(GATE_PARAM, 0.f, 1.f, 0.f, "Gate Threshold", " dB", 0.f, 60.f, 20.f);
    // Display shows: -20 dB at 0.0, -80 dB at 1.0
}

// In process()
float gateValue = params[GATE_PARAM].getValue();
singleKnobGate.setKnob(gateValue);
```

---

## Code Structure

### File Organization

```
src/
├── Overdrive.hpp              # Module declaration
├── Overdrive.cpp              # Module implementation
├── dsp/
│   ├── OverdriveDSP.h         # DSP engine
│   ├── DiodeClipper.h         # Clipping stages
│   ├── ToneStack.h            # Tone controls
│   └── TransistorStage.h      # Transistor booster (DS-1)
```

### Header File Structure (OverdriveDSP.h)

```cpp
#pragma once

#include <cmath>
#include <algorithm>
#include "dsp/Nam.h"  // For NoiseGate

// Forward declarations
class SoftClipper;
class HardClipper;
class TubeScreamerTone;
class DS1Tone;
class OnePoleHPF;

enum class OverdriveModel {
    TS808,
    TS9,
    DS1
};

/**
 * OverdriveDSP - Digital emulation of classic overdrive pedals
 *
 * Models:
 * - TS-808: Original Tubescreamer with soft clipping
 * - TS-9: Modified output stage (470Ω/100K vs 100Ω/10K)
 * - DS-1: Hard clipping with transistor booster
 *
 * Signal Chain:
 * Input → Noise Gate → Model-specific chain (w/ Attack Cap) → Tone → Output
 *
 * Additional Features:
 * - Attack: 6-position step knob (0-5), controls feedback capacitor
 * - Gate: Single-knob threshold control, other parameters fixed
 */
class OverdriveDSP {
public:
    OverdriveDSP();
    ~OverdriveDSP() = default;

    // Model management
    void setModel(OverdriveModel model);
    OverdriveModel getModel() const { return currentModel; }

    // Sample rate
    void setSampleRate(double sr);

    // Parameters
    void setDrive(float drive);   // 0-1 normalized
    void setTone(float tone);     // 0-1 normalized
    void setLevel(float level);   // 0-1 normalized
    void setAttack(int position); // 0-5 (step knob position)
    void setGate(float value);    // 0-1 normalized, maps to -20 to -80 dB

    // Processing
    float process(float input);
    void reset();

    // Monitoring
    bool isGateOpen() const { return singleKnobGate.isOpen(); }

private:
    OverdriveModel currentModel = OverdriveModel::TS808;
    double sampleRate = 48000.0;

    // Component instances
    std::unique_ptr<SoftClipper> softClipper;
    std::unique_ptr<HardClipper> hardClipper;
    std::unique_ptr<TubeScreamerTone> tsTone;
    std::unique_ptr<DS1Tone> dsTone;
    std::unique_ptr<TransistorBooster> transistorBooster;
    std::unique_ptr<OnePoleHPF> attackFilter;
    std::unique_ptr<SingleKnobNoiseGate> singleKnobGate;

    // Output state
    float outputGain = 1.0f;
};
```

---

## Testing and Validation

### Unit Test Strategy

**Scope:** Unit tests only (no hardware validation or integration testing)

**Test Coverage:**
- DSP component behavior (clippers, tone stacks, filters)
- Model switching logic
- Parameter bounds and edge cases
- Oversampling accuracy
- Attack parameter quantization
- Noise gate threshold mapping

### Unit Test Structure

```cpp
// test/OverdriveTest.cpp
#include <gtest/gtest.h>
#include "../src/dsp/OverdriveDSP.h"

class OverdriveTest : public ::testing::Test {
protected:
    OverdriveDSP od;
    double sampleRate = 48000.0;

    void SetUp() override {
        od.setSampleRate(sampleRate);
    }
};

TEST_F(OverdriveTest, TS808_SoftClipping) {
    od.setModel(OverdriveModel::TS808);
    od.setDrive(0.5f);
    od.setTone(0.5f);
    od.setLevel(0.5f);

    // Test that output is within expected bounds
    float input = 0.1f;
    float output = od.process(input);
    EXPECT_GE(output, -1.5f);  // Some overshoot allowed
    EXPECT_LE(output, 1.5f);
}

TEST_F(OverdriveTest, DS1_HardClipping) {
    od.setModel(OverdriveModel::DS1);
    od.setDrive(1.0f);

    // High input should hard clip at ~0.7V
    float input = 1.0f;
    float output = od.process(input);
    EXPECT_NEAR(output, 0.7f, 0.1f);
}

TEST_F(OverdriveTest, AttackFilter) {
    od.setAttack(5);  // Maximum attack (position 5 = 1000 Hz HPF)

    // Low frequency should be attenuated
    float lowFreq = 50.0f;
    float amplitude = od.process(std::sin(2.0f * M_PI * lowFreq / sampleRate));
    EXPECT_LT(amplitude, 0.5f);  // Should be attenuated
}

TEST_F(OverdriveTest, AttackDeepBass) {
    od.setAttack(0);  // Position 0 = 470nF (72Hz)

    // Sub-bass should be slightly attenuated, but guitar range passes
    float lowFreq = 40.0f;
    float input = std::sin(2.0f * M_PI * lowFreq / sampleRate);
    float output = od.process(input);
    // Expect some attenuation at 40Hz vs 82Hz (Low E)
}

TEST_F(OverdriveTest, NoiseGate) {
    od.setGate(0.8f);  // Heavy gating

    // Low level signal should be gated
    float noise = 0.001f;
    float gated = od.process(noise);
    EXPECT_LT(gated, noise);  // Should be attenuated
}
```

### Validation Against Real Pedals

For accurate validation, compare:

1. **Frequency Response**: Use swept sine wave measurement
2. **Harmonic Content**: FFT analysis of distorted signal
3. **Dynamic Response**: Step response, attack/decay
4. **Clipping Characteristics**: Waveform shape analysis

#### Test Signals

```cpp
// Frequency sweep
for (float freq = 20.0f; freq <= 20000.0f; freq *= 1.01f) {
    float input = 0.1f * std::sin(2.0f * M_PI * freq * t);
    float output = od.process(input);
    // Record input/output for frequency response analysis
}

// Harmonic distortion test
float fundamental = 440.0f;
float input = 0.2f * std::sin(2.0f * M_PI * fundamental * t);
float output = od.process(input);
// FFT to analyze harmonic content: 2f, 3f, 4f, etc.
```

---

## References

### Circuit Analysis Resources

- [ElectroSmash - Tube Screamer Circuit Analysis](https://www.electrosmash.com/tube-screamer-analysis) - Complete schematic analysis with calculations
- [ElectroSmash - Boss DS-1 Distortion Analysis](https://www.electrosmash.com/boss-ds1-analysis) - Comprehensive DS-1 breakdown
- [Geofex - Tube Screamer Technology](http://www.geofex.com/article_folders/tstech/tsxtech.htm) - Classic reference by R.G. Keen

### Digital Modeling Papers

- [David Yeh - Digital Implementation of Musical Distortion Circuits (Stanford CCRMA)](https://ccrma.stanford.edu/~dtyeh/papers/DavidYehThesissinglesided.pdf) - Foundational whitebox modeling thesis (101 citations)
- [An Improved and Generalized Diode Clipper Model for Wave Digital Filters](https://www.researchgate.net/publication/299514713_An_Improved_and_Generalized_Diode_Clipper_Model_for_Wave_Digital_Filters) - WDF diode modeling
- [Differentiable black-box and gray-box modeling](https://www.frontiersin.org/journals/signal-processing/articles/10.3389/frsip.2025.1580395/full) - Modern neural approaches (2025)

### WDF Implementation Libraries

- [jatinchowdhury18/WaveDigitalFilters](https://github.com/jatinchowdhury18/WaveDigitalFilters) - Advanced C++ WDF library
- [AndrewBelt/WDFplusplus](https://github.com/AndrewBelt/WDFplusplus) - Easy-to-use WDF collection
- [chowdsp_wdf - An Advanced C++ Library](https://www.researchgate.net/publication/364689194_chowdsp_wdf_An_Advanced_C_Library_for_Wave_Digital_Circuit_Modelling) - Research paper

### DSP and Waveshaping

- [JUCE DSP Tutorial - Waveshaping](https://juce.com/tutorials/tutorial_dsp_convolution/) - Comprehensive waveshaping guide
- [MusicDSP - Variable-Hardness Clipping](https://www.musicdsp.org/en/latest/Effects/104-variable-hardness-clipping-function.html) - Adjustable clipping algorithms
- [Baltic Lab - Diode Clipping Algorithm](https://baltic-lab.com/2023/08/dsp-diode-clipping-algorithm-for-overdrive-and-distortion-effects/) - Practical diode implementation
- [Stanford CCRMA - Antiderivative Anti-Aliasing](https://ccrma.stanford.edu/~jatin/Notebooks/adaa.html) - Advanced anti-aliasing techniques

### VCV Rack Resources

- [VCV Rack DSP Manual](https://vcvrack.com/manual/DSP) - Official DSP documentation
- [ChowDSP-VCV-Secrets](https://github.com/Chowdhury-DSP/ChowDSP-VCV-Secrets) - Guitar-related VCV modules
- [StudioSixPlusOne VCV Modules](https://github.com/StudioSixPlusOne/rack-modules) - Example implementations

### Noise Gate References

- [PedalPCB Forum - Dwarven Hammer Mods](https://forum.pedalpcb.com/threads/any-interesting-or-cool-mods-to-the-dwarven-hammer-pcb-precision-drive-clone.18812/) - Confirmation that noise gate was omitted from Dwarven Hammer PCB
- [DIYstompboxes Forum - Precision Drive Schematic](https://www.diystompboxes.com/smfforum/index.php?topic=126289.0) - Discussion on unavailable Precision Drive schematic
- [THAT Corporation 2181 Series Datasheet](https://www.thatcorp.com/datashts/THAT_2181-Series_Datasheet.pdf) - VCA chips used in professional noise gates (ISP Decimator)
- [Freestompboxes - Box of Metal Noise Gate](https://www.freestompboxes.org/viewtopic.php?t=30473) - Noise gate circuit discussions
- [Horizon Devices Precision Drive Page](https://horizondevices.com/pages/precision-drive) - Official product page
- [PedalPCB Dwarven Hammer Build Docs](https://docs.pedalpcb.com/project/PedalPCB-DwarvenHammer.pdf) - DIY clone documentation

### Component Datasheets

- **JRC4558D** - Dual op-amp used in original Tubescreamers
- **2SC1815** - Input/output buffer transistor (TS)
- **2SC2240** - Low-noise transistor (DS-1)
- **MA150/1N4148** - Clipping diodes

---

## Implementation Priority

### Phase 1: Core Overdrive Models
1. Implement basic soft clipper (TS-808/TS-9)
2. Implement hard clipper (DS-1)
3. Basic tone controls for each model
4. 3-way model switching
5. Sophisticated 4x oversampling using polyphase filters (minimal CPU, minimal aliasing)

### Phase 2: Enhanced Features
6. Attack capacitor switching (integrated into clipping stage)
7. One-knob noise gate integration (reuse NoiseGate from Nam.h)
8. Output stage variations (TS-808 vs TS-9)
9. Separate high-pass filter implementation for attack control

### Phase 3: Polish and Optimization
10. Component-level accuracy refinement
11. CV modulation for all parameters (Attack CV quantized to discrete steps)
12. Unit tests for all DSP components
13. Panel placeholder SVG (8HP layout)
14. Performance profiling and optimization

---

*Document Version: 1.0*
*Last Updated: 2025*
*Author: Shortwav Labs*
