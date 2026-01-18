# Cabinet Simulator Module Architecture for VCV Rack

## Overview

This document describes the architecture for a stereo cabinet simulator module for VCV Rack 2.x. The module uses convolution-based impulse response (IR) loading to emulate guitar/bass cabinets, and is designed to be placed after the NAM Player in the signal chain.

## Module Concept

### Signal Flow

```
Guitar → NAM Player (Amp Sim) → Cabinet Simulator → Output
                                        ↓
                                 [IR A] + [IR B]
                                        ↓
                                   Blend A/B
                                        ↓
                                 Low/High Pass Filters
                                        ↓
                                   Output Level
```

### Core Features

1. **Dual IR Loading** - Load up to 2 impulse responses simultaneously
2. **IR Blending** - Crossfade/blend between IR A and IR B
3. **Tone Shaping** - High-pass and low-pass filters post-convolution
4. **IR Normalization** - Optional per-IR peak normalization
5. **Output Volume** - Master output level control

## Technical Architecture

### VCV Rack DSP Framework

VCV Rack provides a built-in FFT-based real-time convolver in `<dsp/fir.hpp>`:

```cpp
#include <dsp/fir.hpp>

struct rack::dsp::RealTimeConvolver {
    // Constructor: blockSize must be >=32 and a power of 2
    RealTimeConvolver(size_t blockSize);
    
    // Set the convolution kernel (impulse response)
    void setKernel(const float* kernel, size_t length);
    
    // Process a block of samples (input/output must be blockSize length)
    void processBlock(const float* input, float* output);
};
```

**Key characteristics:**
- Uses PFFFT (Pretty Fast FFT) library internally
- Overlap-add convolution method
- Block-based processing with configurable block size
- Latency equals the block size (in samples)

### FFT and Convolution Theory

The convolver implements the convolution theorem:

$$y * h = \mathcal{F}^{-1}\{\mathcal{F}\{y\} \cdot \mathcal{F}\{h\}\}$$

Where:
- $y$ is the input signal
- $h$ is the impulse response
- $\mathcal{F}$ is the FFT operation

For long IRs, the FFT-based approach is significantly more efficient than direct convolution:
- Direct convolution: $O(N)$ operations per sample
- FFT convolution: $O(\log N)$ average operations per sample

### Impulse Response File Formats

Cabinet IRs are typically distributed as WAV files:

| Format | Sample Rate | Bit Depth | Typical Length |
|--------|-------------|-----------|----------------|
| Standard | 44.1-48 kHz | 16-24 bit | 200-500 ms |
| High Quality | 96 kHz | 24-32 bit | 500-1000 ms |
| Ultra | 192 kHz | 32-bit float | 1000+ ms |

**Recommended IR constraints for real-time use:**
- Maximum length: ~1 second (48,000 samples at 48 kHz)
- Target length: 200-500 ms (10,000-24,000 samples)
- Longer IRs increase latency and CPU usage

### WAV File Loading

For loading WAV files, several approaches are available:

#### Option 1: dr_wav (Recommended)
A single-header C library, commonly used in VCV Rack plugins:

```cpp
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

struct WavFile {
    std::vector<float> samples;
    unsigned int sampleRate;
    unsigned int channels;
    
    bool load(const std::string& path) {
        drwav wav;
        if (!drwav_init_file(&wav, path.c_str(), nullptr)) {
            return false;
        }
        
        sampleRate = wav.sampleRate;
        channels = wav.channels;
        
        size_t totalSamples = wav.totalPCMFrameCount * wav.channels;
        samples.resize(totalSamples);
        drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, samples.data());
        
        drwav_uninit(&wav);
        return true;
    }
};
```

#### Option 2: libsndfile
More comprehensive but requires additional dependency.

#### Option 3: Custom WAV Parser
For simple mono/stereo WAV files only.

### Sample Rate Handling

IRs may have different sample rates than the VCV Rack engine. Options:

1. **Resample IR on load** (recommended) - Convert IR to engine sample rate
2. **Resample audio** - Process at IR rate, resample input/output
3. **Reject mismatched** - Only accept IRs matching engine rate

Using VCV Rack's built-in resampler:

```cpp
#include <dsp/resampler.hpp>

rack::dsp::SampleRateConverter<1> resampler;
resampler.setRates(irSampleRate, engineSampleRate);
resampler.setQuality(6); // 0-10 scale
```

## Module Specifications

### Module Width
- **12HP** (60.96mm) - Compact but functional

### Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| BLEND | 0.0 - 1.0 | 0.5 | Crossfade between IR A and IR B |
| LOWPASS | 1 kHz - 20 kHz | 20 kHz | Low-pass filter cutoff frequency |
| HIGHPASS | 20 Hz - 2 kHz | 20 Hz | High-pass filter cutoff frequency |
| OUTPUT | 0.0 - 2.0 | 1.0 | Output level (0-200%) |

### Inputs/Outputs

| Port | Type | Description |
|------|------|-------------|
| AUDIO_INPUT_L | Input | Left audio input |
| AUDIO_INPUT_R | Input | Right audio input (normaled to L) |
| AUDIO_OUTPUT_L | Output | Left audio output |
| AUDIO_OUTPUT_R | Output | Right audio output |

### Lights

| Light | Color | Description |
|-------|-------|-------------|
| IR_A_LOADED | Green | IR A is loaded |
| IR_B_LOADED | Green | IR B is loaded |
| PROCESSING | Blue | Audio is being processed |

### Context Menu Options

- Load IR A...
- Load IR B...
- Unload IR A
- Unload IR B
- Normalize IR A (toggle)
- Normalize IR B (toggle)
- Show IR A info (name, sample rate, length)
- Show IR B info (name, sample rate, length)

## Implementation Details

### Class Structure

```cpp
// src/CabSim.hpp
struct CabSim : Module {
    enum ParamId {
        BLEND_PARAM,
        LOWPASS_PARAM,
        HIGHPASS_PARAM,
        OUTPUT_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        AUDIO_INPUT_L,
        AUDIO_INPUT_R,
        INPUTS_LEN
    };
    enum OutputId {
        AUDIO_OUTPUT_L,
        AUDIO_OUTPUT_R,
        OUTPUTS_LEN
    };
    enum LightId {
        IR_A_LIGHT,
        IR_B_LIGHT,
        PROCESSING_LIGHT,
        LIGHTS_LEN
    };
    
    // Block size for convolution (power of 2, >= 32)
    static constexpr int BLOCK_SIZE = 256;
    
    // Dual convolver instances
    std::unique_ptr<rack::dsp::RealTimeConvolver> convolverA;
    std::unique_ptr<rack::dsp::RealTimeConvolver> convolverB;
    
    // IR metadata
    std::string irPathA, irPathB;
    bool normalizeA = false, normalizeB = false;
    
    // Post-convolution filters
    rack::dsp::TBiquadFilter<> lowpassL, lowpassR;
    rack::dsp::TBiquadFilter<> highpassL, highpassR;
    
    // Block buffers
    std::vector<float> inputBufferL, inputBufferR;
    std::vector<float> outputBufferL, outputBufferR;
    std::vector<float> tempBufferA, tempBufferB;
    int bufferPos = 0;
    
    // Sample rate
    float sampleRate = 48000.f;
    
    // Async loading
    std::thread loadThread;
    std::atomic<bool> isLoading{false};
    std::mutex convMutex;
    
    CabSim();
    ~CabSim();
    
    void process(const ProcessArgs& args) override;
    void onSampleRateChange(const SampleRateChangeEvent& e) override;
    
    void loadIR(int slot, const std::string& path);
    void unloadIR(int slot);
    
    json_t* dataToJson() override;
    void dataFromJson(json_t* rootJ) override;
};
```

### DSP Wrapper Class

```cpp
// src/dsp/CabSimDSP.h

struct IRLoader {
    std::vector<float> samples;
    unsigned int sampleRate = 0;
    unsigned int originalLength = 0;
    float peakLevel = 1.0f;
    std::string path;
    std::string name;
    
    bool load(const std::string& filePath);
    void resampleTo(unsigned int targetRate);
    void normalize();
    float getPeakLevel() const;
};

class CabSimDSP {
public:
    static constexpr int MAX_IR_LENGTH = 96000;  // 2 seconds at 48kHz
    static constexpr int BLOCK_SIZE = 256;
    
    CabSimDSP();
    ~CabSimDSP();
    
    // IR management
    bool loadIR(int slot, const std::string& path, float engineSampleRate);
    void unloadIR(int slot);
    bool isIRLoaded(int slot) const;
    std::string getIRName(int slot) const;
    std::string getIRPath(int slot) const;
    
    // Processing
    void setSampleRate(float rate);
    void process(float inputL, float inputR, 
                 float& outputL, float& outputR,
                 float blend, float lowpassFreq, float highpassFreq);
    
    // Normalization control
    void setNormalize(int slot, bool enabled);
    bool getNormalize(int slot) const;
    
private:
    std::unique_ptr<rack::dsp::RealTimeConvolver> convolvers[2];
    IRLoader irData[2];
    bool normalize[2] = {false, false};
    float sampleRate = 48000.f;
    
    // Block processing buffers
    std::vector<float> inputBlock;
    std::vector<float> outputBlockA;
    std::vector<float> outputBlockB;
    int blockPos = 0;
    
    // Output filters
    rack::dsp::TBiquadFilter<float> lpfL, lpfR;
    rack::dsp::TBiquadFilter<float> hpfL, hpfR;
    
    void updateFilters(float lpFreq, float hpFreq);
};
```

### Block Processing Strategy

Since VCV Rack calls `process()` sample-by-sample but the convolver processes blocks:

```cpp
void CabSim::process(const ProcessArgs& args) {
    // Get inputs
    float inputL = inputs[AUDIO_INPUT_L].getVoltage() / 5.f;
    float inputR = inputs[AUDIO_INPUT_R].isConnected() 
                   ? inputs[AUDIO_INPUT_R].getVoltage() / 5.f 
                   : inputL;
    
    // Accumulate into block buffer
    inputBufferL[bufferPos] = inputL;
    inputBufferR[bufferPos] = inputR;
    
    // Output from previous block
    float outputL = outputBufferL[bufferPos];
    float outputR = outputBufferR[bufferPos];
    
    bufferPos++;
    
    // Process when block is full
    if (bufferPos >= BLOCK_SIZE) {
        std::lock_guard<std::mutex> lock(convMutex);
        
        float blend = params[BLEND_PARAM].getValue();
        
        // Process through both convolvers
        if (convolverA) {
            convolverA->processBlock(inputBufferL.data(), tempBufferA.data());
        }
        if (convolverB) {
            convolverB->processBlock(inputBufferL.data(), tempBufferB.data());
        }
        
        // Blend results
        for (int i = 0; i < BLOCK_SIZE; i++) {
            float a = convolverA ? tempBufferA[i] : 0.f;
            float b = convolverB ? tempBufferB[i] : 0.f;
            outputBufferL[i] = a * (1.f - blend) + b * blend;
        }
        
        // Apply filters...
        
        bufferPos = 0;
    }
    
    // Apply output gain
    float outputGain = params[OUTPUT_PARAM].getValue();
    outputs[AUDIO_OUTPUT_L].setVoltage(outputL * outputGain * 5.f);
    outputs[AUDIO_OUTPUT_R].setVoltage(outputR * outputGain * 5.f);
}
```

### Filter Implementation

Post-convolution tone shaping using VCV Rack's built-in biquad filters:

```cpp
void updateFilters(float sampleRate, float lpFreq, float hpFreq) {
    // Lowpass filter
    float lpNorm = std::clamp(lpFreq / sampleRate, 0.001f, 0.499f);
    lpfL.setParameters(rack::dsp::TBiquadFilter<>::LOWPASS, lpNorm, 0.707f, 1.f);
    lpfR.setParameters(rack::dsp::TBiquadFilter<>::LOWPASS, lpNorm, 0.707f, 1.f);
    
    // Highpass filter
    float hpNorm = std::clamp(hpFreq / sampleRate, 0.001f, 0.499f);
    hpfL.setParameters(rack::dsp::TBiquadFilter<>::HIGHPASS, hpNorm, 0.707f, 1.f);
    hpfR.setParameters(rack::dsp::TBiquadFilter<>::HIGHPASS, hpNorm, 0.707f, 1.f);
}

float applyFilters(float sample, 
                   rack::dsp::TBiquadFilter<>& lpf,
                   rack::dsp::TBiquadFilter<>& hpf) {
    sample = hpf.process(sample);  // HPF first
    sample = lpf.process(sample);  // Then LPF
    return sample;
}
```

### IR Normalization

Peak normalization scales the IR so its maximum absolute value is 1.0:

```cpp
void IRLoader::normalize() {
    // Find peak
    float peak = 0.f;
    for (float s : samples) {
        peak = std::max(peak, std::abs(s));
    }
    
    if (peak > 0.f) {
        peakLevel = peak;
        float scale = 1.f / peak;
        for (float& s : samples) {
            s *= scale;
        }
    }
}
```

Benefits of normalization:
- Consistent output levels between different IRs
- Prevents unexpected volume jumps when switching IRs
- User can disable if they prefer original IR character

### Async IR Loading

IR loading should not block the audio thread:

```cpp
void CabSim::loadIR(int slot, const std::string& path) {
    if (isLoading) return;
    
    // Wait for previous load
    if (loadThread.joinable()) {
        loadThread.join();
    }
    
    isLoading = true;
    
    loadThread = std::thread([this, slot, path]() {
        try {
            IRLoader loader;
            if (loader.load(path)) {
                loader.resampleTo(static_cast<unsigned int>(sampleRate));
                
                if (normalize[slot]) {
                    loader.normalize();
                }
                
                // Create new convolver
                auto newConv = std::make_unique<rack::dsp::RealTimeConvolver>(BLOCK_SIZE);
                newConv->setKernel(loader.samples.data(), loader.samples.size());
                
                // Swap atomically
                {
                    std::lock_guard<std::mutex> lock(convMutex);
                    if (slot == 0) {
                        convolverA = std::move(newConv);
                        irPathA = path;
                    } else {
                        convolverB = std::move(newConv);
                        irPathB = path;
                    }
                }
            }
        } catch (const std::exception& e) {
            WARN("Failed to load IR: %s", e.what());
        }
        
        isLoading = false;
    });
}
```

### State Serialization

```cpp
json_t* CabSim::dataToJson() {
    json_t* rootJ = json_object();
    
    // IR paths
    json_object_set_new(rootJ, "irPathA", json_string(irPathA.c_str()));
    json_object_set_new(rootJ, "irPathB", json_string(irPathB.c_str()));
    
    // Normalization settings
    json_object_set_new(rootJ, "normalizeA", json_boolean(normalizeA));
    json_object_set_new(rootJ, "normalizeB", json_boolean(normalizeB));
    
    return rootJ;
}

void CabSim::dataFromJson(json_t* rootJ) {
    // Load IR paths
    json_t* pathAJ = json_object_get(rootJ, "irPathA");
    if (pathAJ && json_string_value(pathAJ)[0] != '\0') {
        loadIR(0, json_string_value(pathAJ));
    }
    
    json_t* pathBJ = json_object_get(rootJ, "irPathB");
    if (pathBJ && json_string_value(pathBJ)[0] != '\0') {
        loadIR(1, json_string_value(pathBJ));
    }
    
    // Normalization
    json_t* normAJ = json_object_get(rootJ, "normalizeA");
    if (normAJ) normalizeA = json_boolean_value(normAJ);
    
    json_t* normBJ = json_object_get(rootJ, "normalizeB");
    if (normBJ) normalizeB = json_boolean_value(normBJ);
}
```

## Performance Considerations

### CPU Usage

| Factor | Impact | Mitigation |
|--------|--------|------------|
| IR length | Linear increase | Limit to 1 second max |
| Block size | Smaller = more overhead | Use 256 or larger |
| Dual convolvers | 2x processing | Share input FFT if possible |
| Sample rate | Linear increase | IR length scales with rate |

### Latency

The convolver introduces latency equal to the block size:
- Block size 256 @ 48kHz = 5.33ms latency
- Block size 512 @ 48kHz = 10.67ms latency

For live playing, this latency is typically acceptable (comparable to physical speaker distance).

### Memory Usage

Each convolver allocates:
- Input FFT buffer: `blockSize * 2 * sizeof(float)`
- Kernel FFTs: `kernelBlocks * blockSize * 2 * sizeof(float)`
- Output tail: `blockSize * sizeof(float)`

For a 1-second IR at 48kHz with 256 block size:
- ~192 kernel blocks
- ~400 KB per convolver
- ~800 KB total for stereo dual-IR

## File Structure

```
src/
├── dsp/
│   ├── CabSimDSP.h          # DSP wrapper
│   └── dr_wav.h             # WAV loading library
├── CabSim.hpp               # Module header
├── CabSim.cpp               # Module implementation
├── plugin.hpp               # Updated with CabSim model
└── plugin.cpp               # Updated to add model

res/
├── CabSim.svg               # Light panel
└── CabSim-dark.svg          # Dark panel (optional)
```

## Dependencies

### Internal (VCV Rack SDK)
- `<dsp/fir.hpp>` - RealTimeConvolver
- `<dsp/filter.hpp>` - TBiquadFilter
- `<dsp/resampler.hpp>` - SampleRateConverter
- `<osdialog.h>` - File dialogs

### External (to be added)
- **dr_wav.h** - Single-header WAV loading
  - Source: https://github.com/mackron/dr_libs
  - License: Public Domain / MIT
  - Size: ~200 KB single header

## Testing Strategy

1. **Unit Tests**
   - IR loading with various sample rates
   - Normalization correctness
   - Filter response verification
   
2. **Integration Tests**
   - Load/unload cycling
   - Sample rate changes
   - Async loading under load
   
3. **Performance Tests**
   - CPU usage with max-length IRs
   - Memory leak detection
   - Latency measurement

## Future Enhancements

1. **Stereo IRs** - Support for stereo impulse responses
2. **IR Preview** - Waveform display of loaded IRs
3. **IR Browser** - Built-in browser for IR management
4. **Phase Alignment** - Align IRs for proper blending
5. **Preset IRs** - Bundle common cabinet IRs
6. **CV Control** - Blend parameter CV input

## References

- [VCV Rack DSP Documentation](https://vcvrack.com/manual/DSP)
- [PFFFT Library](https://bitbucket.org/jpommier/pffft/)
- [dr_libs (dr_wav)](https://github.com/mackron/dr_libs)
- [Convolution Reverb Theory](https://ccrma.stanford.edu/~jos/sasp/Convolution.html)
- [Audio EQ Cookbook](https://www.w3.org/2011/audio/audio-eq-cookbook.html)
