# API Reference

Complete technical reference for Guitar Tools plugin developers.

## Table of Contents

- [Overview](#overview)
- [Module Classes](#module-classes)
  - [NamPlayer Module](#namplayer-module)
  - [CabSim Module](#cabsim-module)
- [DSP Classes](#dsp-classes)
  - [NamDSP](#namdsp)
  - [CabSimDSP](#cabsimdsp)
- [Widget Classes](#widget-classes)
- [Utility Functions](#utility-functions)
- [Constants and Enumerations](#constants-and-enumerations)

---

## Overview

The Guitar Tools plugin is built on VCV Rack's Module API and integrates with the Neural Amp Modeler Core library. This reference documents the public API for both module usage and extension development.

**Plugin Architecture:**
```
plugin.cpp/hpp       - Plugin initialization and model registration
├── NamPlayer.cpp/hpp    - NAM Player module implementation
├── CabSim.cpp/hpp       - Cabinet Simulator module implementation
└── dsp/
    ├── Nam.h            - NAM DSP wrapper
    ├── CabSimDSP.h      - Cabinet simulation DSP
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
    AUDIO_INPUT,  // Mono audio input
    INPUTS_LEN
};

enum OutputId {
    AUDIO_OUTPUT,  // Mono audio output
    OUTPUTS_LEN
};
```

#### Lights

```cpp
enum LightId {
    MODEL_LIGHT,         // Model loaded indicator (green)
    SAMPLE_RATE_LIGHT,   // Sample rate mismatch warning (yellow)
    GATE_LIGHT,          // Noise gate open indicator
    LIGHTS_LEN
};
```

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
    AUDIO_INPUT,  // Mono audio input
    INPUTS_LEN
};

enum OutputId {
    AUDIO_OUTPUT,  // Mono audio output
    OUTPUTS_LEN
};
```

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
"tags": ["Effect", "Distortion", "Equalizer", "Hardware Clone"]
```

### Audio Processing

```cpp
constexpr int BLOCK_SIZE = 128;           // Audio processing block size
constexpr int DISPLAY_BUFFER_SIZE = 512;  // Waveform display buffer
constexpr double DEFAULT_SAMPLE_RATE = 48000.0;  // NAM models default rate
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

Both `NamPlayer` and `CabSim` modules use asynchronous loading to prevent audio dropouts:

1. **Load Request**: User action triggers background thread
2. **File Reading**: I/O performed off audio thread
3. **Atomic Swap**: DSP object swapped on audio thread during `process()`

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

- **Version**: Latest from `sdatkinson/NeuralAmpModelerCore`
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
