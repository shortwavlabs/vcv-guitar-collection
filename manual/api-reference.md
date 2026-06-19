# API Reference

Complete technical reference for Guitar Tools plugin developers.

## Table of Contents

- [Overview](#overview)
- [Module Classes](#module-classes)
  - [NamPlayer Module](#namplayer-module)
  - [NamFxLoopExpander Module](#namfxloopexpander-module)
  - [CabSim Module](#cabsim-module)
  - [Mnemonix Module](#mnemonix-module)
- [DSP Classes](#dsp-classes)
  - [NamDSP](#namdsp)
  - [CabSimDSP](#cabsimdsp)
  - [MnemonixDSP](#mnemonixdsp)
- [Widget Classes](#widget-classes)
- [Utility Functions](#utility-functions)
- [Constants and Enumerations](#constants-and-enumerations)

---

## Overview

The Guitar Tools plugin is built on VCV Rack's Module API and uses an in-tree NAM DSP implementation (`nam_rack`). This reference documents the public API for both module usage and extension development.

**Plugin Architecture:**
```
plugin.cpp/hpp       - Plugin initialization and model registration
├── NamPlayer.cpp/hpp    - NAM Player module implementation
├── NamFxLoopExpander.cpp/hpp - NAM Player send/return expander implementation
├── CabSim.cpp/hpp       - Cabinet Simulator module implementation
├── Mnemonix.cpp/hpp     - Mnemonix BBD delay module implementation
└── dsp/
    ├── Nam.h            - NAM DSP wrapper
    ├── CabSimDSP.h      - Cabinet simulation DSP
    ├── MnemonixDSP.h    - BBD delay, compander, clock, and modulation DSP
    ├── NoiseGate.h      - Noise gate implementation
    └── ToneStack.h      - 5-band EQ implementation
```

---

## Module Classes

### NamPlayer Module

Neural Amp Modeler player module for real-time guitar amp simulation.

#### Class Declaration

```cpp
struct NamPlayer : Module {
    // Module implementation
};
```

**Header File:** `src/NamPlayer.hpp`

#### Parameters

```cpp
enum ParamId {
    INPUT_PARAM,           // Input gain
    OUTPUT_PARAM,          // Output level
    GATE_THRESHOLD_PARAM,  // Noise gate threshold
    GATE_ATTACK_PARAM,     // Gate attack time
    GATE_RELEASE_PARAM,    // Gate release time
    GATE_HOLD_PARAM,       // Gate hold time
    BASS_PARAM,            // Bass EQ
    MIDDLE_PARAM,          // Mid EQ
    TREBLE_PARAM,          // Treble EQ
    PRESENCE_PARAM,        // Presence EQ
    DEPTH_PARAM,           // Depth EQ
    PARAMS_LEN
};
```

**Parameter Ranges:**

| Parameter | Min | Max | Default | Unit |
|-----------|-----|-----|---------|------|
| INPUT_PARAM | -24.0 | +24.0 | 0.0 | dB |
| OUTPUT_PARAM | -24.0 | +24.0 | 0.0 | dB |
| GATE_THRESHOLD_PARAM | -80.0 | 0.0 | -80.0 | dB |
| GATE_ATTACK_PARAM | 0.1 | 100.0 | 1.0 | ms |
| GATE_RELEASE_PARAM | 10.0 | 1000.0 | 100.0 | ms |
| GATE_HOLD_PARAM | 0.0 | 500.0 | 10.0 | ms |
| BASS_PARAM | -12.0 | +12.0 | 0.0 | dB |
| MIDDLE_PARAM | -12.0 | +12.0 | 0.0 | dB |
| TREBLE_PARAM | -12.0 | +12.0 | 0.0 | dB |
| PRESENCE_PARAM | -12.0 | +12.0 | 0.0 | dB |
| DEPTH_PARAM | -12.0 | +12.0 | 0.0 | dB |

#### Inputs & Outputs

```cpp
enum InputId {
    AUDIO_INPUT,              // Mono audio input
    // CV inputs for all parameters
    CV_INPUT_INPUT,           // Input Level CV
    CV_INPUT_OUTPUT,          // Output Level CV
    CV_GATE_THRESHOLD_INPUT,  // Gate Threshold CV
    CV_GATE_ATTACK_INPUT,     // Gate Attack CV
    CV_GATE_RELEASE_INPUT,    // Gate Release CV
    CV_GATE_HOLD_INPUT,       // Gate Hold CV
    CV_BASS_INPUT,            // Bass CV
    CV_MIDDLE_INPUT,          // Middle CV
    CV_TREBLE_INPUT,          // Treble CV
    CV_PRESENCE_INPUT,        // Presence CV
    CV_DEPTH_INPUT,           // Depth CV
    INPUTS_LEN
};

enum OutputId {
    AUDIO_OUTPUT,  // Mono audio output
    OUTPUTS_LEN
};
```

**CV Input Behavior:**
- All CV inputs accept ±5V signals
- CV signals automatically rescale to the full parameter range
- When a CV input is connected, it **replaces** the knob value (no attenuverter needed)
- CV inputs are sampled at audio rate for smooth modulation
- Disconnected CV inputs allow knob control

#### Lights

```cpp
enum LightId {
    MODEL_LIGHT,         // Model loaded indicator (green)
    SAMPLE_RATE_LIGHT,   // Sample rate mismatch warning (yellow)
    GATE_LIGHT,          // Noise gate open indicator
    LIGHTS_LEN
};
```

#### Context Menu Options

`NamPlayer` exposes an **Eco Mode** context-menu setting:

- **Off**: Full processing quality (default)
- **On**: Reduced CPU usage

`NamPlayer` also exposes a **Use Fast Tanh** context-menu toggle:

- **Enabled (default)**: Uses fast tanh approximation for activation paths.
- **Disabled**: Uses exact tanh behavior.

The module persists these states in patch data, with backward compatibility for older patches that stored a legacy boolean eco flag.

#### Public Methods

##### `NamPlayer()`
Constructor. Initializes the module with default parameters and allocates DSP buffers.

```cpp
NamPlayer();
```

##### `~NamPlayer()`
Destructor. Cleans up DSP resources and joins any pending load threads.

```cpp
~NamPlayer();
```

##### `void process(const ProcessArgs& args)`
Main audio processing callback. Called by VCV Rack for each audio buffer.

```cpp
void process(const ProcessArgs& args) override;
```

**Parameters:**
- `args` - Processing arguments containing sample rate and block size

**Processing Flow:**
1. Apply input gain
2. Run noise gate
3. Process through NAM model
4. Apply 5-band EQ (tone stack)
5. Apply output gain
6. Update display buffer

##### `void loadModel(const std::string& path)`
Asynchronously loads a NAM model from disk.

```cpp
void loadModel(const std::string& path);
```

**Parameters:**
- `path` - Absolute file path to `.nam` model file

**Behavior:**
- Spawns background thread for loading
- Updates `isLoading` flag
- Sets `loadSuccess` on completion
- Thread-safe: model swapped on audio thread

**Example:**
```cpp
module->loadModel("/path/to/amp_model.nam");
```

##### `void unloadModel()`
Unloads the currently loaded NAM model.

```cpp
void unloadModel();
```

**Behavior:**
- Sets `hasPendingUnload` flag
- Model cleared on next audio processing cycle
- Thread-safe operation

##### `std::string getModelPath() const`
Returns the file path of the currently loaded model.

```cpp
std::string getModelPath() const;
```

**Returns:** Absolute path to loaded model, or empty string if no model loaded

##### `std::string getModelName() const`
Returns the display name of the currently loaded model.

```cpp
std::string getModelName() const;
```

**Returns:** Model filename without path and extension

##### `bool isSampleRateMismatched() const`
Checks if the model's expected sample rate differs from the current rate.

```cpp
bool isSampleRateMismatched() const;
```

**Returns:** `true` if automatic resampling is active, `false` otherwise

**Note:** NAM models are typically captured at 48kHz. When running VCV Rack at other sample rates, automatic resampling occurs with minimal quality loss.

##### `WaveformColor getWaveformColor() const`
Gets the current waveform display color preset.

```cpp
WaveformColor getWaveformColor() const;
```

**Returns:** Current color enum value

##### `void setWaveformColor(WaveformColor color)`
Sets the waveform display color preset.

```cpp
void setWaveformColor(WaveformColor color);
```

**Parameters:**
- `color` - Color preset from `WaveformColor` enum

##### `void onSampleRateChange(const SampleRateChangeEvent& e)`
Handles sample rate changes from VCV Rack.

```cpp
void onSampleRateChange(const SampleRateChangeEvent& e) override;
```

**Parameters:**
- `e` - Event containing new sample rate

**Behavior:**
- Triggers DSP reinitialization
- Updates resampler state
- Thread-safe: applied on audio thread

#### Member Variables

##### `std::unique_ptr<NamDSP> namDsp`
Primary NAM DSP processor instance.

##### `std::vector<float> inputBuffer`
Pre-allocated input buffer for block processing.

**Size:** `BLOCK_SIZE` (128 samples)

##### `std::vector<float> outputBuffer`
Pre-allocated output buffer for block processing.

**Size:** `BLOCK_SIZE` (128 samples)

##### `std::vector<float> displayBuffer`
Ring buffer for waveform visualization.

**Size:** `DISPLAY_BUFFER_SIZE` (512 samples)

#### Constants

```cpp
static constexpr int BLOCK_SIZE = 128;
static constexpr int DISPLAY_BUFFER_SIZE = 512;
```

#### Waveform Color Enumeration

```cpp
enum class WaveformColor {
    Green = 0,
    BabyBlue,
    Amber,
    Red,
    Purple,
    White,
    NUM_COLORS
};
```

---

### NamFxLoopExpander Module

4HP send/return expander for NAM Player. It is not a standalone audio processor; it communicates with a NAM Player immediately on its left through Rack's expander message buffers.

#### Class Declaration

```cpp
struct NamFxLoopExpander : Module {
    // Module implementation
};
```

**Header File:** `src/NamFxLoopExpander.hpp`

#### Parameters

```cpp
enum ParamId {
    BLEND_PARAM,  // Dry/wet blend
    PARAMS_LEN
};
```

| Parameter | Min | Max | Default | Unit |
|-----------|-----|-----|---------|------|
| BLEND_PARAM | 0.0 | 1.0 | 1.0 | percent display |

#### Inputs & Outputs

```cpp
enum InputId {
    RETURN_INPUT,
    INPUTS_LEN
};

enum OutputId {
    SEND_OUTPUT,
    OUTPUTS_LEN
};
```

`SEND_OUTPUT` carries NAM Player's post-amp/post-output signal when linked. `RETURN_INPUT` is blended back into NAM Player's final output path. If `RETURN_INPUT` is unpatched, the expander normals to the dry signal.

#### Lights

```cpp
enum LightId {
    LINK_LIGHT,
    LIGHTS_LEN
};
```

`LINK_LIGHT` is green when the expander is immediately connected to NAM Player on its left.

#### Processing Flow

1. Check for NAM Player on the left expander side.
2. Read the latest dry voltage from NAM Player.
3. Send that voltage to `SEND_OUTPUT`.
4. Blend dry voltage with `RETURN_INPUT` using `BLEND_PARAM`.
5. Send the blended voltage back to NAM Player.

---

### CabSim Module

Convolution-based cabinet simulator with dual IR slots.

#### Class Declaration

```cpp
struct CabSim : Module {
    // Module implementation
};
```

**Header File:** `src/CabSim.hpp`

#### Parameters

```cpp
enum ParamId {
    BLEND_PARAM,      // Mix between IR A and IR B
    LOWPASS_PARAM,    // Lowpass filter cutoff
    HIGHPASS_PARAM,   // Highpass filter cutoff
    OUTPUT_PARAM,     // Output level
    PARAMS_LEN
};
```

**Parameter Ranges:**

| Parameter | Min | Max | Default | Unit |
|-----------|-----|-----|---------|------|
| BLEND_PARAM | 0.0 | 1.0 | 0.5 | normalized |
| LOWPASS_PARAM | 1000.0 | 20000.0 | 20000.0 | Hz |
| HIGHPASS_PARAM | 20.0 | 500.0 | 20.0 | Hz |
| OUTPUT_PARAM | -24.0 | +24.0 | 0.0 | dB |

#### Inputs & Outputs

```cpp
enum InputId {
    AUDIO_INPUT,         // Mono audio input
    // CV inputs for all parameters
    CV_BLEND_INPUT,      // Blend CV
    CV_LOWPASS_INPUT,    // Low-Pass Cutoff CV
    CV_HIGHPASS_INPUT,   // High-Pass Cutoff CV
    CV_OUTPUT_INPUT,     // Output Level CV
    INPUTS_LEN
};

enum OutputId {
    AUDIO_OUTPUT,  // Mono audio output
    OUTPUTS_LEN
};
```

**CV Input Behavior:**
- All CV inputs accept ±5V signals
- CV signals automatically rescale to the full parameter range
- When a CV input is connected, it **replaces** the knob value (no attenuverter needed)
- CV inputs are sampled at audio rate for smooth modulation
- Disconnected CV inputs allow knob control

#### Lights

```cpp
enum LightId {
    IR_A_LIGHT,  // IR A loaded indicator
    IR_B_LIGHT,  // IR B loaded indicator
    LIGHTS_LEN
};
```

#### Public Methods

##### `CabSim()`
Constructor. Initializes the cabinet simulator module.

```cpp
CabSim();
```

##### `~CabSim()`
Destructor. Cleans up DSP resources and joins any pending load threads.

```cpp
~CabSim();
```

##### `void process(const ProcessArgs& args)`
Main audio processing callback.

```cpp
void process(const ProcessArgs& args) override;
```

**Processing Flow:**
1. Apply input to convolution engine
2. Blend between IR A and IR B based on BLEND parameter
3. Apply highpass and lowpass filters
4. Apply output gain

##### `void loadIR(int slot, const std::string& path)`
Asynchronously loads an impulse response file.

```cpp
void loadIR(int slot, const std::string& path);
```

**Parameters:**
- `slot` - IR slot index (0 for A, 1 for B)
- `path` - Absolute file path to IR file (WAV, AIFF, or FLAC)

**Supported Formats:**
- WAV (16/24/32-bit PCM, 32-bit float)
- AIFF (16/24/32-bit PCM)
- FLAC (16/24-bit)

**Sample Rates:** Automatic resampling to match VCV Rack rate

**Example:**
```cpp
module->loadIR(0, "/path/to/cabinet_ir.wav");
```

##### `void unloadIR(int slot)`
Unloads an impulse response from a slot.

```cpp
void unloadIR(int slot);
```

**Parameters:**
- `slot` - IR slot index (0 for A, 1 for B)

##### `void setNormalize(int slot, bool enabled)`
Enables or disables automatic gain normalization for an IR slot.

```cpp
void setNormalize(int slot, bool enabled);
```

**Parameters:**
- `slot` - IR slot index (0 for A, 1 for B)
- `enabled` - `true` to enable normalization, `false` to disable

**Behavior:**
- When enabled, normalizes IR to 0dBFS peak
- Prevents level jumps when switching between IRs
- Applied during IR loading

##### `bool getNormalize(int slot) const`
Gets the normalization state for an IR slot.

```cpp
bool getNormalize(int slot) const;
```

**Parameters:**
- `slot` - IR slot index (0 for A, 1 for B)

**Returns:** `true` if normalization is enabled, `false` otherwise

##### `std::string getIRPath(int slot) const`
Returns the file path of the loaded IR.

```cpp
std::string getIRPath(int slot) const;
```

**Parameters:**
- `slot` - IR slot index (0 for A, 1 for B)

**Returns:** Absolute path to loaded IR, or empty string if slot is empty

##### `std::string getIRName(int slot) const`
Returns the display name of the loaded IR.

```cpp
std::string getIRName(int slot) const;
```

**Parameters:**
- `slot` - IR slot index (0 for A, 1 for B)

**Returns:** IR filename without path and extension

##### `bool isIRLoaded(int slot) const`
Checks if an IR is loaded in a slot.

```cpp
bool isIRLoaded(int slot) const;
```

**Parameters:**
- `slot` - IR slot index (0 for A, 1 for B)

**Returns:** `true` if IR is loaded, `false` otherwise

##### `void onSampleRateChange(const SampleRateChangeEvent& e)`
Handles sample rate changes from VCV Rack.

```cpp
void onSampleRateChange(const SampleRateChangeEvent& e) override;
```

**Parameters:**
- `e` - Event containing new sample rate

**Behavior:**
- Reinitializes convolution engine
- Reloads IRs with new sample rate
- Updates filter coefficients

##### `json_t* dataToJson()`
Serializes module state for patch saving.

```cpp
json_t* dataToJson() override;
```

**Returns:** JSON object containing IR paths and normalization settings

##### `void dataFromJson(json_t* rootJ)`
Deserializes module state from patch.

```cpp
void dataFromJson(json_t* rootJ) override;
```

**Parameters:**
- `rootJ` - JSON object containing saved state

#### Member Variables

##### `std::unique_ptr<CabSimDSP> cabSimDsp`
Cabinet simulation DSP processor instance.

##### `std::string irPathA`, `irPathB`
File paths for loaded IRs (for serialization).

##### `bool normalizeA`, `normalizeB`
Normalization enable flags per slot.

---

### Mnemonix Module

Rev_D MN3008-inspired BBD memory delay with chorus/vibrato modulation, tap/sync timing, stereo expansion, calibration trims, and modular utility outputs.

#### Class Declaration

```cpp
struct Mnemonix : Module {
    // Module implementation
};
```

**Header File:** `src/Mnemonix.hpp`

#### Parameters

```cpp
enum ParamId {
    LEVEL_PARAM,            // Input drive
    BLEND_PARAM,            // Dry/wet mix
    FEEDBACK_PARAM,         // Repeat feedback
    DELAY_PARAM,            // BBD clock/delay time
    DEPTH_PARAM,            // Modulation depth
    MODE_PARAM,             // Chorus/Vibrato switch
    SHAPE_PARAM,            // Triangle/Square LFO switch
    ENGAGE_PARAM,           // Effect engage switch
    LONG_PARAM,             // Normal/Long delay range switch
    LEVEL_CV_ATT_PARAM,     // Level CV attenuverter
    BLEND_CV_ATT_PARAM,     // Blend CV attenuverter
    FEEDBACK_CV_ATT_PARAM,  // Feedback CV attenuverter
    DELAY_CV_ATT_PARAM,     // Delay CV attenuverter
    DEPTH_CV_ATT_PARAM,     // Depth CV attenuverter
    PARAMS_LEN
};
```

**Parameter Ranges:**

| Parameter | Min | Max | Default | Unit |
|-----------|-----|-----|---------|------|
| LEVEL_PARAM | 0.0 | 1.0 | 0.55 | normalized |
| BLEND_PARAM | 0.0 | 1.0 | 0.5 | normalized |
| FEEDBACK_PARAM | 0.0 | 1.0 | 0.25 | normalized |
| DELAY_PARAM | 0.0 | 1.0 | 0.45 | displayed as ms |
| DEPTH_PARAM | 0.0 | 1.0 | 0.2 | normalized |
| MODE_PARAM | 0.0 | 1.0 | 0.0 | Chorus/Vibrato |
| SHAPE_PARAM | 0.0 | 1.0 | 0.0 | Triangle/Square |
| ENGAGE_PARAM | 0.0 | 1.0 | 1.0 | Bypassed/Engaged |
| LONG_PARAM | 0.0 | 1.0 | 0.0 | Normal/Long |
| `*_CV_ATT_PARAM` | -1.0 | 1.0 | 0.0 | attenuverter |

Normal delay range maps to approximately `32.8-409.6 ms`; Long range extends to approximately `819.2 ms`.

#### Inputs & Outputs

```cpp
enum InputId {
    AUDIO_INPUT,
    LEVEL_CV_INPUT,
    BLEND_CV_INPUT,
    FEEDBACK_CV_INPUT,
    DELAY_CV_INPUT,
    DEPTH_CV_INPUT,
    MODE_CV_INPUT,
    SHAPE_CV_INPUT,
    ENGAGE_CV_INPUT,
    LONG_CV_INPUT,
    TAP_CV_INPUT,
    INPUTS_LEN
};

enum OutputId {
    AUDIO_OUTPUT,
    DIRECT_OUTPUT,
    WET_OUTPUT,
    LEVEL_CV_OUTPUT,
    BLEND_CV_OUTPUT,
    FEEDBACK_CV_OUTPUT,
    DELAY_CV_OUTPUT,
    DEPTH_CV_OUTPUT,
    MODE_GATE_OUTPUT,
    SHAPE_LFO_OUTPUT,
    ENGAGE_GATE_OUTPUT,
    LONG_GATE_OUTPUT,
    CLOCK_GATE_OUTPUT,
    CLOCK_DIV_OUTPUT,
    ENVELOPE_CV_OUTPUT,
    OUTPUTS_LEN
};
```

**CV behavior:**
- Continuous CV inputs are summed with the knob through their attenuverter and clamped to `0.0..1.0`.
- Switch gate inputs override their front-panel switch when connected.
- `Tap Tempo Gate` uses rising edges to set tap tempo and enters sync mode.
- Continuous control outputs emit final post-CV control values as `0-10V`.
- `SHAPE_LFO_OUTPUT` emits the selected LFO waveform as approximately `-5V..+5V`.

#### Context Menu State

```cpp
enum BypassBehavior {
    BYPASS_TRAILS = 0,
    BYPASS_CPU_MUTE = 1
};

enum TimingMode {
    TIMING_FREE = 0,
    TIMING_SYNC = 1
};

enum SyncDivision {
    SYNC_WHOLE = 0,
    SYNC_HALF,
    SYNC_DOTTED_QUARTER,
    SYNC_QUARTER,
    SYNC_DOTTED_EIGHTH,
    SYNC_EIGHTH,
    SYNC_EIGHTH_TRIPLET,
    SYNC_SIXTEENTH,
    SYNC_DIVISIONS_LEN
};

enum StereoMode {
    STEREO_MONO = 0,
    STEREO_WIDE,
    STEREO_PING_PONG
};
```

The context menu also stores `artifactProfile`, tap tempo, and Advanced calibration trims:

```cpp
float inputGainTrim;
float bbdBiasTrim;
float clockBleedTrim;
float companderTrim;
float noiseTrim;
float wetMakeupTrim;
float feedbackHeadroomTrim;
```

#### Public Methods

##### `Mnemonix()`
Constructor. Configures parameters, CV inputs, outputs, bypass behavior, and default DSP state.

##### `void process(const ProcessArgs& args)`
Main audio processing callback. Handles sample-rate updates, tap tempo, CV resolution, bypass behavior, polyphony, stereo expansion, DSP processing, and utility outputs.

##### `void onSampleRateChange(const SampleRateChangeEvent& e)`
Updates all per-channel DSP engines when VCV Rack sample rate changes.

##### `json_t* dataToJson()` / `void dataFromJson(json_t* rootJ)`
Serializes and restores artifact profile, bypass behavior, timing mode, sync division, stereo mode, tap tempo, and calibration trims.

##### `void resetEngines()`
Clears all per-channel delay memories and DSP state.

##### `void setArtifactProfile(int profile)`
Sets Cleaner BBD, Rev_D authentic, or Worn unit artifact behavior.

##### `void setEngineSampleRate(float sampleRate)`
Applies a validated sample rate to all `MnemonixDSP` engines.

##### `float getSyncedDelayNorm(bool longDelay) const`
Maps current tap tempo and sync division into the normalized nonlinear delay control.

#### Member Variables

##### `std::array<MnemonixDSP, 16> engines`
One independent DSP engine per polyphonic channel.

##### `int artifactProfile`, `bypassBehavior`, `timingMode`, `syncDivision`, `stereoMode`
Patch-saveable context menu state.

##### `dsp::SchmittTrigger tapTrigger`
Rising-edge detector for tap tempo input.

---

## DSP Classes

### NamDSP

Wrapper class for Neural Amp Modeler Core DSP processing with integrated resampling and tone stack.

**Header File:** `src/dsp/Nam.h`

#### Methods

##### `void process(float* input, float* output, int numSamples)`
Processes audio through the NAM model.

```cpp
void process(float* input, float* output, int numSamples);
```

**Parameters:**
- `input` - Input audio buffer
- `output` - Output audio buffer
- `numSamples` - Number of samples to process

**Requirements:**
- Buffers must be pre-allocated
- Input and output buffers can be the same (in-place processing)

##### `void setSampleRate(double sampleRate)`
Sets the DSP sample rate and initializes resampling.

```cpp
void setSampleRate(double sampleRate);
```

**Parameters:**
- `sampleRate` - Target sample rate in Hz

**Behavior:**
- Configures resampler for NAM model's expected rate (typically 48kHz)
- Reinitializes internal buffers

##### `void setInputGain(float gainDb)`
Sets input gain in decibels.

```cpp
void setInputGain(float gainDb);
```

**Parameters:**
- `gainDb` - Gain in dB (-24.0 to +24.0)

##### `void setOutputGain(float gainDb)`
Sets output gain in decibels.

```cpp
void setOutputGain(float gainDb);
```

**Parameters:**
- `gainDb` - Gain in dB (-24.0 to +24.0)

##### `void setToneStack(float bass, float middle, float treble, float presence, float depth)`
Configures the 5-band EQ (tone stack).

```cpp
void setToneStack(float bass, float middle, float treble, float presence, float depth);
```

**Parameters:**
- `bass` - Bass EQ in dB (-12.0 to +12.0)
- `middle` - Mid EQ in dB (-12.0 to +12.0)
- `treble` - Treble EQ in dB (-12.0 to +12.0)
- `presence` - Presence EQ in dB (-12.0 to +12.0)
- `depth` - Depth EQ in dB (-12.0 to +12.0)

**Frequency Bands:**
- Bass: 120 Hz (shelving filter)
- Middle: 700 Hz (peaking filter)
- Treble: 2.5 kHz (peaking filter)
- Presence: 5 kHz (peaking filter)
- Depth: 90 Hz (peaking filter)

##### `void reset()`
Resets DSP state (clears buffers and history).

```cpp
void reset();
```

**Use Cases:**
- When loading a new model
- To clear audio artifacts
- When changing sample rate

---

### CabSimDSP

Convolution-based cabinet simulation DSP.

**Header File:** `src/dsp/CabSimDSP.h`

#### Methods

##### `void process(float* input, float* output, int numSamples)`
Processes audio through convolution engine.

```cpp
void process(float* input, float* output, int numSamples);
```

**Parameters:**
- `input` - Input audio buffer
- `output` - Output audio buffer
- `numSamples` - Number of samples to process

##### `void loadIR(int slot, const std::vector<float>& irSamples)`
Loads impulse response samples into a slot.

```cpp
void loadIR(int slot, const std::vector<float>& irSamples);
```

**Parameters:**
- `slot` - IR slot index (0 or 1)
- `irSamples` - Impulse response sample data

**Performance Note:** Uses FFT-based convolution for efficiency with long IRs.

##### `void setBlend(float blend)`
Sets the mix between IR slots.

```cpp
void setBlend(float blend);
```

**Parameters:**
- `blend` - Blend amount (0.0 = 100% A, 1.0 = 100% B)

**Curve:** Equal-power crossfading for smooth transitions

##### `void setLowpass(float frequency)`
Sets lowpass filter cutoff frequency.

```cpp
void setLowpass(float frequency);
```

**Parameters:**
- `frequency` - Cutoff in Hz (1000.0 to 20000.0)

**Filter Type:** 2nd-order Butterworth lowpass

##### `void setHighpass(float frequency)`
Sets highpass filter cutoff frequency.

```cpp
void setHighpass(float frequency);
```

**Parameters:**
- `frequency` - Cutoff in Hz (20.0 to 500.0)

**Filter Type:** 2nd-order Butterworth highpass

##### `void setSampleRate(float sampleRate)`
Sets the DSP sample rate.

```cpp
void setSampleRate(float sampleRate);
```

**Parameters:**
- `sampleRate` - Sample rate in Hz

**Behavior:**
- Reinitializes convolution buffers
- Updates filter coefficients

---

### MnemonixDSP

Topology-aware BBD delay DSP used by the Mnemonix module.

**Header File:** `src/dsp/MnemonixDSP.h`

#### Data Structures

```cpp
struct Params {
    float level;
    float blend;
    float feedback;
    float delay;
    float depth;
    float delayOffset;
    float lfoPhaseOffset;
    float inputGainTrim;
    float bbdBiasTrim;
    float clockBleedTrim;
    float companderTrim;
    float noiseTrim;
    float wetMakeupTrim;
    float feedbackHeadroomTrim;
    bool vibrato;
    bool squareLfo;
    bool longDelay;
    bool engaged;
    int artifactProfile;
};

struct Result {
    float output;
    float direct;
    float wet;
    float overload;
    float clockHz;
    float delaySeconds;
    float lfo;
    float clockGate;
    float clockDivGate;
    float envelope;
};
```

#### Artifact Profiles

```cpp
enum ArtifactProfile {
    ARTIFACT_CLEAN = 0,
    ARTIFACT_AUTHENTIC = 1,
    ARTIFACT_WORN = 2
};
```

#### Methods

##### `void setSampleRate(float rate)`
Sets DSP sample rate and recalculates smoothing, filter, compander, and op-amp coefficients. Invalid or very low rates fall back to `DEFAULT_SAMPLE_RATE`.

##### `void reset()`
Clears delay memory, feedback state, LFO/clock state, filter history, compander state, and cached control calculations.

##### `Result process(float input, const Params& rawParams)`
Processes one normalized audio sample through the modeled input stage, compander, BBD chips, clocking path, filters, feedback path, wet/dry mix, and utility outputs.

##### `float getSampleRate() const`
Returns the current DSP sample rate.

##### `static float clockPeriodUsForDelay(float delayNorm, bool longDelay = false)`
Maps normalized delay control to the nonlinear BBD clock period. Normal range is `8 us .. 100 us`; Long range extends the maximum to `200 us`.

##### `static float delaySecondsForDelay(float delayNorm, bool longDelay = false)`
Returns nominal BBD delay time for a normalized delay control.

##### `static float delayNormForDelayMilliseconds(float delayMs, bool longDelay)`
Maps a displayed millisecond value back to normalized delay control.

##### `static float lfoRateHzForDelay(float delayNorm, bool vibrato)`
Returns delay-dependent chorus/vibrato LFO rate.

#### Implementation Notes

- Fixed-size delay memory avoids allocation in the audio path.
- One `MnemonixDSP` instance is used per polyphonic channel.
- Sample-rate changes are handled through `setSampleRate()` from the Rack module.
- The BBD path is intentionally clock-limited and artifact-prone; it is not a clean interpolated digital delay.

---

## Widget Classes

### NamPlayerWidget

UI widget for the NAM Player module.

**Header File:** Defined in `src/NamPlayer.cpp`

#### Methods

##### `void appendContextMenu(Menu* menu)`
Adds custom menu items to the context menu.

```cpp
void appendContextMenu(Menu* menu) override;
```

**Menu Items:**
- Waveform color selection
- Model information
- Performance statistics

---

### CabSimWidget

UI widget for the Cabinet Simulator module.

**Header File:** Defined in `src/CabSim.cpp`

#### Methods

##### `void appendContextMenu(Menu* menu)`
Adds custom menu items to the context menu.

```cpp
void appendContextMenu(Menu* menu) override;
```

**Menu Items:**
- Load IR to slot A/B
- Unload IR from slot A/B
- Enable/disable normalization per slot

---

### NamFxLoopExpanderWidget

UI widget for the NAM FX Loop expander.

**Header File:** Defined in `src/NamFxLoopExpander.cpp`

The widget provides one `DRY/WET` knob, `SEND` output, `RETURN` input, and a green `LINK` light.

---

### MnemonixWidget

UI widget for the Mnemonix module.

**Header File:** Defined in `src/Mnemonix.cpp`

#### Methods

##### `void appendContextMenu(Menu* menu)`
Adds custom menu items to the context menu.

```cpp
void appendContextMenu(Menu* menu) override;
```

**Menu Items:**
- Artifact profile
- Bypass behavior
- Timing mode, sync division, and tap tempo seed
- Stereo mode
- Advanced calibration trims
- Clear delay memory

---

## Utility Functions

### File I/O

#### `std::vector<float> loadWavFile(const std::string& path, float& sampleRate)`
Loads a WAV file and returns sample data.

**Parameters:**
- `path` - Absolute file path
- `sampleRate` - Output: sample rate of loaded file

**Returns:** Vector of audio samples (mono or stereo interleaved)

**Error Handling:** Returns empty vector on failure

---

## Constants and Enumerations

### Module Tags

```cpp
// From plugin.json
NAM Player: ["Effect", "Distortion", "Equalizer"]
NAM FX Loop: ["Expander", "Effect"]
Cabinet Simulator: ["Effect", "Equalizer"]
Mnemonix: ["Delay", "Effect", "Chorus"]
```

### Audio Processing

```cpp
constexpr int BLOCK_SIZE = 128;           // Audio processing block size
constexpr int DISPLAY_BUFFER_SIZE = 512;  // Waveform display buffer
constexpr double DEFAULT_SAMPLE_RATE = 48000.0;  // NAM models default rate
```

### Mnemonix Delay Targets

```cpp
Normal clock period: 8 us .. 100 us
Long clock period:   8 us .. 200 us
Normal delay range:  ~32.8 ms .. ~409.6 ms
Long delay range:    ~32.8 ms .. ~819.2 ms
```

### Parameter Limits

```cpp
constexpr float MIN_GAIN_DB = -24.0f;
constexpr float MAX_GAIN_DB = +24.0f;
constexpr float MIN_GATE_THRESHOLD_DB = -80.0f;
constexpr float MAX_GATE_THRESHOLD_DB = 0.0f;
constexpr float MIN_EQ_DB = -12.0f;
constexpr float MAX_EQ_DB = +12.0f;
```

---

## Thread Safety

### Asynchronous Operations

`NamPlayer` and `CabSim` use asynchronous loading to prevent audio dropouts:

1. **Load Request**: User action triggers background thread
2. **File Reading**: I/O performed off audio thread
3. **Atomic Swap**: DSP object swapped on audio thread during `process()`

`Mnemonix` does not load external files. Its DSP state is fixed-size and runs directly in the audio callback, with sample-rate changes propagated through `setEngineSampleRate()`.

**Thread-Safe Methods:**
- `loadModel()` / `loadIR()`
- `unloadModel()` / `unloadIR()`
- `onSampleRateChange()`

**Audio Thread Only:**
- `process()`
- All parameter updates

### Best Practices

```cpp
// Good: Load model asynchronously
module->loadModel("/path/to/model.nam");

// Bad: Don't block audio thread with I/O
// NAM models are loaded in background automatically

// Good: Check loading state
if (module->isLoading) {
    // Show loading indicator
}

// Good: Safe parameter updates (audio thread)
void process(const ProcessArgs& args) override {
    float inputGain = params[INPUT_PARAM].getValue();
    // Use immediately in processing
}
```

---

## Version Compatibility

### VCV Rack API

- **Target Version**: 2.6.0+
- **API Level**: VCV Rack v2 API

### Neural Amp Modeler Core

- **Version**: In-tree `nam_rack` implementation (no external NAM core dependency)
- **NAM Format**: Compatible with `.nam` v1.x format

### Breaking Changes

**v2.0.0:**
- Added 5-band EQ to NAM Player
- Added Cabinet Simulator module
- Unified parameter ranges

**v1.x → v2.0:**
- Model paths may need updating
- JSON format changed (automatic migration)

---

## Performance Considerations

### CPU Usage

**NAM Player:**
- Model complexity: 3-15% CPU per voice (typical)
- Simple models (Linear): ~1-3% CPU
- LSTM models: ~5-10% CPU
- WaveNet models: ~10-15% CPU

**Cabinet Simulator:**
- FFT convolution: ~1-2% CPU per voice
- Independent of IR length (optimized)

### Memory Usage

**NAM Player:**
- Base: ~10MB
- Per model: 50-200MB (varies by architecture)

**Cabinet Simulator:**
- Base: ~5MB
- Per IR (48kHz, 2 seconds): ~0.4MB
- FFT buffers: Depends on IR length

### Optimization Tips

1. **Use appropriate model complexity** for your needs
2. **Keep IR lengths reasonable** (typically 0.5-2 seconds)
3. **Avoid frequent model loading** during playback
4. **Use block processing** (default behavior)
5. **Enable normalization** to avoid level adjustments

---

## Examples

### Loading a Model Programmatically

```cpp
// Get module instance
NamPlayer* module = dynamic_cast<NamPlayer*>(moduleWidget->getModule());
if (module) {
    // Load custom model
    module->loadModel("/Users/me/nam_models/my_amp.nam");
    
    // Set input gain
    module->params[NamPlayer::INPUT_PARAM].setValue(3.0f);  // +3dB
    
    // Configure noise gate
    module->params[NamPlayer::GATE_THRESHOLD_PARAM].setValue(-60.0f);
    module->params[NamPlayer::GATE_ATTACK_PARAM].setValue(2.0f);
    module->params[NamPlayer::GATE_RELEASE_PARAM].setValue(100.0f);
}
```

### Creating a Custom Widget

```cpp
struct MyNamPlayerWidget : ModuleWidget {
    MyNamPlayerWidget(NamPlayer* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/MyPanel.svg")));
        
        // Add parameters
        addParam(createParamCentered<RoundLargeBlackKnob>(
            Vec(100, 150),
            module,
            NamPlayer::INPUT_PARAM
        ));
        
        // Add input/output ports
        addInput(createInputCentered<PJ301MPort>(
            Vec(50, 300),
            module,
            NamPlayer::AUDIO_INPUT
        ));
        
        addOutput(createOutputCentered<PJ301MPort>(
            Vec(150, 300),
            module,
            NamPlayer::AUDIO_OUTPUT
        ));
    }
};
```

---

## See Also

- [Quickstart Guide](quickstart.md) - Get started quickly
- [Advanced Usage](advanced-usage.md) - Performance optimization and best practices
- [Examples](examples/) - Real-world usage examples
- [FAQ](faq.md) - Common questions and troubleshooting

---

**Last Updated:** v2.0.0 (January 2026)
