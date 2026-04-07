# NeuralAmpModelerCore Rewrite Plan for VCV Rack

## Executive Summary

This document outlines a comprehensive plan to rewrite the NeuralAmpModelerCore (NAM) library for optimal integration with VCV Rack, targeting C++11 compatibility and macOS 10.9 support while removing the Eigen dependency.

## Goals

1. **C++11 Compatibility**: Ensure all code works with C++11 standard
2. **macOS 10.9 Support**: Remove dependencies on newer OS features
3. **Zero External Dependencies**: Remove Eigen and nlohmann/json - only depend on VCV Rack SDK
4. **Performance Target**: Max 10% CPU usage at typical buffer sizes
5. **Drop-in Replacement**: Direct replacement for current `src/dsp/Nam.h` - no parallel implementation

---

## Current Architecture Analysis

### Dependencies

| Dependency | Version | Purpose | C++11 Compatible | Action |
|------------|---------|---------|------------------|--------|
| Eigen | 3.3+ | Matrix operations | Yes | **Remove** - use custom + Rack SIMD |
| nlohmann/json | single-header | JSON parsing | Yes | **Remove** - use Jansson from Rack SDK |
| std::filesystem | C++17 | File path handling | **No** | **Replace** with `rack::system` |

**Goal**: Zero external dependencies except VCV Rack SDK.

### VCV Rack SDK Utilities (Available to Leverage)

The Rack SDK provides many utilities we can use instead of implementing from scratch:

#### Math Functions (`rack::math`)
```cpp
#include <math.hpp>

// Clamp (replaces std::clamp)
float clamped = rack::math::clamp(x, 0.f, 1.f);
int clampedInt = rack::math::clamp(x, 0, 10);

// Other utilities
rack::math::rescale(x, xMin, xMax, yMin, yMax);  // Linear rescaling
rack::math::crossfade(a, b, p);                   // Linear interpolation
rack::math::isNear(a, b, epsilon);               // Float comparison
```

#### SIMD Support (`rack::simd`)
```cpp
#include <simd/functions.hpp>

// 4-wide SIMD float vector
rack::simd::float_4 a = 1.f;
rack::simd::float_4 b = {1.f, 2.f, 3.f, 4.f};

// SIMD math operations (4x faster than scalar)
auto s = rack::simd::sin(a);
auto c = rack::simd::cos(a);
auto e = rack::simd::exp(a);
auto l = rack::simd::log(a);
auto t = rack::simd::tan(a);
auto clamped = rack::simd::clamp(a, 0.f, 1.f);

// Load/store from arrays
rack::simd::float_4 vec = rack::simd::float_4::load(ptr);
vec.store(outputPtr);
```

#### DSP Utilities (`rack::dsp`)
```cpp
#include <dsp/filter.hpp>      // Filters
#include <dsp/ringbuffer.hpp>  // Ring buffers
#include <dsp/resampler.hpp>   // Sample rate conversion
#include <dsp/common.hpp>      // DSP utilities

// Biquad filter (could replace BiquadFilter in Nam.h)
rack::dsp::BiquadFilter filter;
filter.setParameters(rack::dsp::BiquadFilter::LOWPASS, freq/sr, Q, gain);

// Ring buffers (for temporal memory in NAM)
rack::dsp::DoubleRingBuffer<float, 1024> buffer;
buffer.push(x);
float* data = buffer.startData();

// Sample rate conversion (already used in Nam.h)
rack::dsp::SampleRateConverter<1> src;
src.setRates(inRate, outRate);
src.process(in, inStride, &inFrames, out, outStride, &outFrames);

// Conversion utilities
float db = rack::dsp::amplitudeToDb(amp);
float amp = rack::dsp::dbToAmplitude(db);
```

#### File System (`rack::system`)
```cpp
#include <system.hpp>

// File operations (C++11 compatible, replaces std::filesystem)
if (rack::system::isFile(path)) { ... }
std::string dir = rack::system::getDirectory(path);
std::string filename = rack::system::getFilename(path);
std::string ext = rack::system::getExtension(path);
std::string stem = rack::system::getStem(path);
std::string fullPath = rack::system::join(dir, filename);

// Read file into buffer
std::vector<uint8_t> data = rack::system::readFile(path);
```

#### String Utilities (`rack::string`)
```cpp
#include <string.hpp>

std::string upper = rack::string::uppercase(s);
std::string trimmed = rack::string::trim(s);
bool match = rack::string::startsWith(s, prefix);
std::vector<std::string> parts = rack::string::split(s, ",");
```

### Supported Model Architectures

1. **Linear** - Simple FIR filter (impulse response)
2. **ConvNet** - Dilated 1D convolutions with batch normalization
3. **LSTM** - Multi-layer Long Short-Term Memory network
4. **WaveNet** - Dilated convolutions with gated activations (**most common in res/models**)

**Priority**: Implement WaveNet first as it's the dominant architecture in the model collection.

### Key Eigen Operations Used

```cpp
// Matrix multiplication
Eigen::MatrixXf result = weight * input;

// Colwise bias addition
result.colwise() += bias;

// Block/slice access
auto block = matrix.block(row, col, rows, cols);
auto leftCols = matrix.leftCols(n);
auto middleCols = matrix.middleCols(start, n);

// Elementwise operations
result = input.cwiseProduct(scale);

// Vector operations
Eigen::VectorXf vec;
vec(Eigen::placeholders::lastN(n));  // Last n elements
```

### Critical Interface (must preserve)

```cpp
namespace nam {
class DSP {
public:
    virtual void process(NAM_SAMPLE* input, NAM_SAMPLE* output, const int num_frames);
    virtual void prewarm();
    virtual void Reset(const double sampleRate, const int maxBufferSize);
    double GetExpectedSampleRate() const;
    bool HasLoudness() const;
    double GetLoudness() const;
};

std::unique_ptr<DSP> get_dsp(const std::string& config_filename);
}
```

---

## Rewrite Strategy

### Phase 1: Matrix Library Replacement

Create a lightweight matrix library optimized for NAM's specific usage patterns.

#### 1.1 Design Goals for Matrix Library

- **Header-only** for easy integration
- **No dynamic allocation** during audio processing
- **Column-major** storage (compatible with NAM's access patterns)
- **Minimal API** covering only what NAM needs
- **Cache-friendly** for real-time audio

#### 1.2 Required Operations

```cpp
// Core matrix class
class Matrix {
    float* data;
    int rows, cols;

    // Access
    float& operator()(int row, int col);
    float* col(int c);              // Column pointer
    void setZero();
    void resize(int rows, int cols);

    // Operations
    static void multiply(Matrix& out, const Matrix& a, const Matrix& b);
    static void add_colwise(Matrix& out, const Vector& v);
    void leftCols(int n);           // Returns span
    void middleCols(int start, int n);
    void middleRows(int start, int n);
    void block(int row, int col, int rows, int cols);
};

class Vector {
    float* data;
    int size;

    float& operator()(int i);
    void resize(int n);
    void setZero();
};
```

#### 1.3 Memory Management Strategy

```cpp
// Pre-allocated buffer pool
class MatrixPool {
    std::vector<float> buffer;
    int offset = 0;

public:
    Matrix allocate(int rows, int cols) {
        int size = rows * cols;
        Matrix m(buffer.data() + offset, rows, cols);
        offset += size;
        return m;
    }

    void reset() { offset = 0; }
};
```

### Phase 2: C++11 Compatibility Fixes

#### 2.1 File System - Use rack::system

**Current (C++17):**
```cpp
#include <filesystem>
std::filesystem::path path;
```

**Replacement using Rack SDK:**
```cpp
#include <system.hpp>

// Already available in Rack SDK - no implementation needed
if (rack::system::isFile(path)) { ... }
std::string dir = rack::system::getDirectory(path);
std::string filename = rack::system::getFilename(path);
std::vector<uint8_t> data = rack::system::readFile(path);
```

#### 2.2 Clamp - Use rack::math::clamp

**Current (C++17):**
```cpp
#include <algorithm>
auto clamped = std::clamp(value, min, max);
```

**Replacement using Rack SDK:**
```cpp
#include <math.hpp>

// Already available - no implementation needed
float clamped = rack::math::clamp(value, min, max);
int clampedInt = rack::math::clamp(intValue, minInt, maxInt);
```

#### 2.3 Language Feature Audit

| Feature | C++ Standard | Used in NAM | Action |
|---------|--------------|-------------|--------|
| `auto` | C++11 | Yes | Keep |
| `nullptr` | C++11 | Yes | Keep |
| Range-based for | C++11 | Yes | Keep |
| Lambda | C++11 | Yes | Keep |
| `std::unique_ptr` | C++11 | Yes | Keep |
| `std::function` | C++11 | Yes | Keep |
| `std::unordered_map` | C++11 | Yes | Keep |
| `override`/`final` | C++11 | Yes | Keep |
| `std::clamp` | C++17 | Yes | **Use `rack::math::clamp`** |
| `std::filesystem` | C++17 | Yes | **Use `rack::system`** |

#### 2.4 JSON Parsing - Use Jansson from Rack SDK

**No custom implementation needed!** The Rack SDK includes **Jansson** (v2.12), a mature C JSON library.

**Location**: `dep/Rack-SDK/dep/include/jansson.h`

**Usage example for .nam files**:
```cpp
#include <jansson.h>

// Load .nam file
json_error_t error;
json_t* root = json_load_file(model_path.c_str(), 0, &error);
if (!root) {
    // Handle error: error.text, error.line
}

// Get architecture
json_t* arch = json_object_get(root, "architecture");
const char* arch_str = json_string_value(arch);  // "WaveNet", "ConvNet", etc.

// Get nested config
json_t* config = json_object_get(root, "config");
json_t* channels = json_object_get(config, "channels");
int num_channels = json_integer_value(channels);

// Get dilations array
json_t* dilations = json_object_get(config, "dilations");
size_t num_dilations = json_array_size(dilations);
for (size_t i = 0; i < num_dilations; i++) {
    int d = json_integer_value(json_array_get(dilations, i));
}

// Get weights (large float arrays)
json_t* weights = json_object_get(root, "weights");
json_t* input_weight = json_object_get(weights, "_input_layer.weight");
size_t weight_size = json_array_size(input_weight);
std::vector<float> weight_vec(weight_size);
for (size_t i = 0; i < weight_size; i++) {
    weight_vec[i] = json_number_value(json_array_get(input_weight, i));
}

// Cleanup (reference counting)
json_decref(root);
```

**Key Jansson functions**:
| Function | Purpose |
|----------|---------|
| `json_load_file()` | Parse JSON from file |
| `json_loads()` | Parse JSON from string |
| `json_object_get()` | Get value by key |
| `json_array_get()` | Get array element by index |
| `json_array_size()` | Get array length |
| `json_string_value()` | Get string value |
| `json_integer_value()` | Get integer value |
| `json_number_value()` | Get float/double value |
| `json_is_object()`, `json_is_array()`, etc. | Type checking |
| `json_decref()` | Decrement refcount (cleanup) |

**Benefits**:
- Zero implementation work
- Well-tested, production-ready library
- C++11 compatible (C library)
- Already linked in Rack SDK

### Phase 3: Architecture Reimplementation

#### 3.1 Linear Model (Simplest)

**Purpose**: Simple FIR filter / impulse response

**Implementation**:
```cpp
class Linear : public Buffer {
    std::vector<float> _weight;
    float _bias;

    void process(float* input, float* output, int num_frames) override {
        for (int i = 0; i < num_frames; i++) {
            float sum = _bias;
            for (size_t j = 0; j < _weight.size(); j++) {
                sum += _weight[j] * get_input_sample(i - j);
            }
            output[i] = sum;
        }
    }
};
```

**Eigen removal complexity**: Low - direct dot product

#### 3.2 ConvNet Architecture

**Components**:
- `Conv1D` - Dilated 1D convolution
- `BatchNorm` - Element-wise affine transform
- Activation functions

**Implementation approach**:
```cpp
class Conv1D {
    std::vector<Matrix> _weight;  // Per kernel position
    Vector _bias;
    int _dilation;
    int _num_groups;
    RingBuffer _input_buffer;
    Matrix _output;

    void process(const Matrix& input, int num_frames) {
        _input_buffer.write(input, num_frames);
        _output.setZero();

        for (size_t k = 0; k < _weight.size(); k++) {
            int offset = _dilation * (k + 1 - _weight.size());
            auto input_block = _input_buffer.read(num_frames, -offset);
            matrix_multiply_accumulate(_output, _weight[k], input_block);
        }

        if (_bias.size() > 0) {
            add_colwise(_output, _bias);
        }
    }
};
```

**Eigen removal complexity**: Medium - matrix multiplication and block access

#### 3.3 LSTM Architecture

**Components**:
- `LSTMCell` - Single LSTM layer
- Multi-layer stacking
- Output projection

**Implementation approach**:
```cpp
class LSTMCell {
    Matrix _w;   // Weight matrix (input+hidden -> 4*hidden)
    Vector _b;   // Bias
    Vector _xh;  // Concatenated input and hidden state
    Vector _ifgo; // Gate outputs
    Vector _c;   // Cell state

    void process(const Vector& x) {
        // Concatenate input and hidden
        copy(_xh.data(), x.data(), input_size);
        copy(_xh.data() + input_size, _hidden.data(), hidden_size);

        // Matrix-vector multiply: ifgo = w * xh + b
        matrix_vector_multiply(_ifgo, _w, _xh);
        vector_add(_ifgo, _b);

        // Apply activations and update state
        // i = sigmoid(ifgo[0:hid])
        // f = sigmoid(ifgo[hid:2*hid])
        // g = tanh(ifgo[2*hid:3*hid])
        // o = sigmoid(ifgo[3*hid:4*hid])
        // c = f * c_prev + i * g
        // h = o * tanh(c)
    }
};
```

**Eigen removal complexity**: Medium - mostly vector operations

#### 3.4 WaveNet Architecture (Most Complex)

**Components**:
- `_Layer` - Dilated conv + gated activation + 1x1 conv
- `_LayerArray` - Stack of layers with same configuration
- `WaveNet` - Main model with multiple layer arrays

**Implementation approach**:
```cpp
class Layer {
    Conv1D _conv;
    Conv1x1 _input_mixin;
    Conv1x1 _1x1;
    Activation* _activation;
    bool _gated;

    Matrix _z;                  // Internal state
    Matrix _output_next_layer;  // Residual output
    Matrix _output_head;        // Skip output

    void process(const Matrix& input, const Matrix& condition, int num_frames) {
        // 1. Dilated convolution
        _conv.process(input, num_frames);
        auto& conv_out = _conv.get_output();

        // 2. Input mixin (conditioning)
        _input_mixin.process(condition, num_frames);
        auto& mixin_out = _input_mixin.get_output();

        // 3. Add and apply gated activation
        for (int i = 0; i < num_frames; i++) {
            for (int c = 0; c < _bottleneck; c++) {
                float val = conv_out(c, i) + mixin_out(c, i);
                if (_gated) {
                    float gate = conv_out(c + _bottleneck, i) + mixin_out(c + _bottleneck, i);
                    _z(c, i) = fast_tanh(val) * fast_sigmoid(gate);
                } else {
                    _z(c, i) = _activation->apply(val);
                }
            }
        }

        // 4. 1x1 convolution
        _1x1.process(_z, num_frames);

        // 5. Residual connection
        for (int i = 0; i < num_frames; i++) {
            for (int c = 0; c < channels; c++) {
                _output_next_layer(c, i) = input(c, i) + _1x1.get_output()(c, i);
            }
        }
    }
};
```

**Eigen removal complexity**: High - complex matrix operations with multiple layers

### Phase 4: Ring Buffer Implementation

The ring buffer is critical for handling temporal dependencies in dilated convolutions.

```cpp
class RingBuffer {
    Matrix _storage;
    long _write_pos = 0;
    long _max_lookback = 0;
    int _max_buffer_size = 0;

public:
    void reset(int channels, int max_buffer_size) {
        int storage_size = 2 * _max_lookback + max_buffer_size;
        _storage.resize(channels, storage_size);
        _storage.setZero();
        _write_pos = _max_lookback;  // Start after lookback area
    }

    void write(const Matrix& input, int num_frames) {
        if (needs_rewind(num_frames)) {
            rewind();
        }
        // Copy input to storage at _write_pos
        for (int i = 0; i < num_frames; i++) {
            for (int c = 0; c < _storage.rows(); c++) {
                _storage(c, _write_pos + i) = input(c, i);
            }
        }
    }

    MatrixSpan read(int num_frames, long lookback = 0) {
        long start = _write_pos - lookback;
        return MatrixSpan(_storage, 0, start, _storage.rows(), num_frames);
    }

    void advance(int num_frames) {
        _write_pos += num_frames;
    }

private:
    void rewind() {
        // Copy tail to head for continuity
        for (long i = 0; i < _max_lookback; i++) {
            for (int c = 0; c < _storage.rows(); c++) {
                _storage(c, i) = _storage(c, _write_pos - _max_lookback + i);
            }
        }
        _write_pos = _max_lookback;
    }
};
```

### Phase 5: Activation Functions

Most activation functions are already simple inline functions. The main change is updating the `Activation::apply()` methods to work with raw pointers instead of Eigen types.

```cpp
class Activation {
public:
    virtual void apply(float* data, long size) = 0;
    virtual ~Activation() = default;
};

class ActivationTanh : public Activation {
public:
    void apply(float* data, long size) override {
        for (long i = 0; i < size; i++) {
            data[i] = std::tanh(data[i]);
        }
    }
};

class ActivationFastTanh : public Activation {
public:
    void apply(float* data, long size) override {
        for (long i = 0; i < size; i++) {
            data[i] = fast_tanh(data[i]);
        }
    }
};
```

---

## File Structure

```
src/dsp/nam/
├── nam_dsp.h              # Main DSP interface (drop-in for nam::DSP)
├── nam_dsp.cpp
├── matrix.h               # Lightweight matrix library (header-only)
├── ring_buffer.h          # Ring buffer for temporal processing
├── ring_buffer.cpp
├── activations.h          # Activation functions (header-only)
├── conv1d.h               # 1D convolution layer
├── conv1d.cpp
├── conv1x1.h              # 1x1 convolution layer
├── conv1x1.cpp
├── linear.h               # Linear model (FIR filter)
├── linear.cpp
├── convnet.h              # ConvNet architecture
├── convnet.cpp
├── lstm.h                 # LSTM architecture
├── lstm.cpp
├── wavenet.h              # WaveNet architecture
├── wavenet.cpp
├── model_loader.h         # Model loading from JSON (uses Jansson)
└── model_loader.cpp
```

### Dependencies from Rack SDK

We leverage these Rack SDK components (no implementation needed):

| Rack SDK Include | What We Use |
|-----------------|-------------|
| `<math.hpp>` | `rack::math::clamp()`, `rescale()`, `crossfade()` |
| `<simd/functions.hpp>` | `simd::float_4`, SIMD math (sin, cos, tan, exp, etc.) |
| `<dsp/ringbuffer.hpp>` | Ring buffer implementations (optional, may use custom) |
| `<dsp/resampler.hpp>` | `SampleRateConverter<1>` (already used) |
| `<system.hpp>` | File system: `isFile()`, `readFile()`, `getFilename()`, etc. |
| `<string.hpp>` | `trim()`, `split()`, `startsWith()`, etc. |

### What We Must Implement

| Component | Why |
|-----------|-----|
| `matrix.h` | Custom matrix ops for NAM's specific access patterns |
| `ring_buffer.h/cpp` | Custom ring buffer for dilated convolution lookback |
| `activations.h` | Fast approximations + Rack SIMD integration |
| Neural net layers | Conv1D, Conv1x1, LSTM cells, WaveNet layers |

### What We Get From Rack SDK (No Implementation Needed)

| Component | Source |
|-----------|--------|
| JSON parsing | Jansson library (`dep/jansson.h`) |
| Math utilities | `rack::math::clamp()`, `rescale()`, etc. |
| SIMD support | `rack::simd::float_4` and math functions |
| File system | `rack::system::isFile()`, `readFile()`, etc. |
| Sample rate conversion | `rack::dsp::SampleRateConverter` |

---

## Implementation Order

### Stage 1: Foundation

1. **matrix.h** - Basic matrix operations (header-only)
   - Allocation/deallocation
   - Element access
   - Zero/copy operations
   - Matrix multiplication with SIMD optimization
   - Use `rack::simd::float_4` for 4-wide SIMD operations

2. **ring_buffer.h/cpp** - Ring buffer implementation
   - Write/read operations
   - Rewind logic for dilated convolution
   - Consider using `rack::dsp::DoubleRingBuffer` as reference

3. **activations.h** - Activation functions (header-only)
   - Port existing fast approximations
   - Integrate with `rack::simd` for SIMD versions
   - `fast_tanh`, `fast_sigmoid`, etc.

### Stage 2: Neural Network Layers

5. **conv1d.h/cpp** - 1D convolution
   - Weight storage
   - Dilated convolution
   - Grouped convolution support
   - SIMD optimization where applicable

6. **conv1x1.h/cpp** - 1x1 convolution
   - Matrix multiplication wrapper
   - SIMD-optimized

### Stage 3: Model Architectures

7. **linear.h/cpp** - Linear model (simplest)
   - FIR filter implementation
   - Test with simple models

8. **convnet.h/cpp** - ConvNet architecture
   - Block implementation
   - Batch normalization
   - Head implementation

9. **lstm.h/cpp** - LSTM architecture
   - LSTM cell
   - Multi-layer stacking

10. **wavenet.h/cpp** - WaveNet architecture
    - Layer implementation
    - Layer array
    - Full model

### Stage 4: Integration

11. **model_loader.h/cpp** - Model loading
    - JSON parsing with Jansson (from Rack SDK)
    - Factory pattern
    - Version checking
    - Use `rack::system` for file operations

12. **nam_dsp.h/cpp** - Main interface
    - Direct replacement for current NAM includes
    - Update Nam.h to use new implementation

### Rack SDK Integration Points

Throughout all stages, leverage Rack SDK:
- Use `rack::math::clamp()` instead of custom implementation
- Use `rack::system::readFile()` for loading .nam files
- Use `rack::simd::float_4` for SIMD activation functions
- Use `rack::dsp::SampleRateConverter<1>` for resampling (already in use)

---

## Performance Optimizations

**Target: Max 10% CPU usage** - This requires aggressive optimization.

### SIMD Support via Rack SDK

Rack SDK provides built-in SIMD support that we can leverage:

```cpp
#include <simd/functions.hpp>

// Process 4 floats at once (4x speedup on aligned data)
void apply_activation_simd(float* data, int size) {
    int i = 0;
    // Process 4 at a time
    for (; i + 3 < size; i += 4) {
        rack::simd::float_4 v = rack::simd::float_4::load(data + i);
        v = rack::simd::tanh(v);  // SIMD tanh
        v.store(data + i);
    }
    // Handle remaining elements
    for (; i < size; i++) {
        data[i] = std::tanh(data[i]);
    }
}

// SIMD matrix operations
void matrix_multiply_simd(float* C, const float* A, const float* B, int M, int K, int N) {
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j += 4) {
            rack::simd::float_4 sum = 0.f;
            for (int k = 0; k < K; k++) {
                rack::simd::float_4 b = rack::simd::float_4::load(&B[k * N + j]);
                sum += A[i * K + k] * b;
            }
            sum.store(&C[i * N + j]);
        }
    }
}
```

### Matrix Multiplication Strategies

**Option 1: Blocked scalar** (cache-friendly, C++11):
```cpp
void matrix_multiply_blocked(float* C, const float* A, const float* B,
                             int M, int K, int N) {
    const int BLOCK = 32;
    memset(C, 0, M * N * sizeof(float));

    for (int i0 = 0; i0 < M; i0 += BLOCK) {
        for (int j0 = 0; j0 < N; j0 += BLOCK) {
            for (int k0 = 0; k0 < K; k0 += BLOCK) {
                int imax = std::min(i0 + BLOCK, M);
                int jmax = std::min(j0 + BLOCK, N);
                int kmax = std::min(k0 + BLOCK, K);

                for (int i = i0; i < imax; i++) {
                    for (int k = k0; k < kmax; k++) {
                        float a = A[i * K + k];
                        for (int j = j0; j < jmax; j++) {
                            C[i * N + j] += a * B[k * N + j];
                        }
                    }
                }
            }
        }
    }
}
```

**Option 2: SIMD-enhanced** (use Rack's simd::float_4):
- Process 4 elements per iteration
- Already provided by Rack SDK
- Works on x86 (SSE) and ARM (NEON via SIMDe)

### Memory Allocation

- Pre-allocate all buffers during model initialization
- No allocations during `process()` calls
- Use memory pools for temporary matrices

### Activation Functions

- Use fast approximations (already in place):
  - `fast_tanh()` - polynomial approximation
  - `fast_sigmoid()` - based on fast_tanh
- Consider LUT (lookup table) for further optimization

---

## Testing Strategy

### Test File Location

The test suite for the rewritten NAM library lives in a **separate file** from the existing plugin tests:

- **Test file**: `src/tests/test_nam_rewrite.cpp`
- **Existing tests**: `src/tests/test_swv_guitar_collection.cpp` (plugin tests, unchanged)

This separation ensures:
1. NAM-specific tests can be run independently
2. Tests don't interfere with existing plugin test suite
3. Clear organization between DSP library tests and module tests

### Test Framework

Follow the existing test pattern from `test_swv_guitar_collection.cpp`:

```cpp
// test_nam_rewrite.cpp
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <dsp/nam/nam_dsp.h>  // New NAM implementation

namespace test_nam {

struct TestContext {
    const char* current_test = "";
    int passed = 0;
    int failed = 0;

    void assertTrue(bool condition, const char* msg = "") {
        if (condition) {
            passed++;
        } else {
            failed++;
            std::cerr << "FAIL [" << current_test << "]: " << msg << std::endl;
        }
    }

    void assertNear(float a, float b, float epsilon = 1e-5f, const char* msg = "") {
        assertTrue(std::fabs(a - b) < epsilon, msg);
    }

    void assertNearArray(const float* a, const float* b, int size, float epsilon = 1e-5f) {
        for (int i = 0; i < size; i++) {
            if (std::fabs(a[i] - b[i]) >= epsilon) {
                char buf[256];
                snprintf(buf, sizeof(buf), "Mismatch at index %d: %f vs %f", i, a[i], b[i]);
                assertTrue(false, buf);
                return;
            }
        }
        assertTrue(true);
    }
};

} // namespace test_nam

int main() {
    test_nam::TestContext ctx;

    // Run test suites
    test_matrix(ctx);
    test_ring_buffer(ctx);
    test_json_parser(ctx);
    test_activations(ctx);
    test_conv1d(ctx);
    test_conv1x1(ctx);
    test_linear(ctx);
    test_convnet(ctx);
    test_lstm(ctx);
    test_wavenet(ctx);
    test_model_loader(ctx);
    test_comparison_vs_original(ctx);
    test_performance(ctx);

    // Report
    std::cout << "\n=== NAM Rewrite Test Results ===" << std::endl;
    std::cout << "Passed: " << ctx.passed << std::endl;
    std::cout << "Failed: " << ctx.failed << std::endl;

    return ctx.failed > 0 ? 1 : 0;
}
```

### Test Categories

#### 1. Matrix Operations (`test_matrix`)

```cpp
void test_matrix(TestContext& ctx) {
    ctx.current_test = "matrix";

    // Test 1.1: Basic allocation and access
    {
        nam::Matrix m;
        m.resize(4, 8);
        ctx.assertTrue(m.rows() == 4, "Matrix rows");
        ctx.assertTrue(m.cols() == 8, "Matrix cols");

        m(2, 3) = 1.5f;
        ctx.assertNear(m(2, 3), 1.5f, 1e-6f, "Matrix element access");
    }

    // Test 1.2: setZero
    {
        nam::Matrix m;
        m.resize(3, 3);
        m.setZero();
        bool all_zero = true;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                if (m(i, j) != 0.f) all_zero = false;
        ctx.assertTrue(all_zero, "Matrix setZero");
    }

    // Test 1.3: Matrix multiplication (small)
    {
        nam::Matrix a, b, c;
        a.resize(2, 3);
        b.resize(3, 2);
        c.resize(2, 2);

        // [[1,2,3],[4,5,6]] * [[1,2],[3,4],[5,6]] = [[22,28],[49,64]]
        a(0,0)=1; a(0,1)=2; a(0,2)=3;
        a(1,0)=4; a(1,1)=5; a(1,2)=6;
        b(0,0)=1; b(0,1)=2;
        b(1,0)=3; b(1,1)=4;
        b(2,0)=5; b(2,1)=6;

        nam::Matrix::multiply(c, a, b);

        ctx.assertNear(c(0,0), 22.f, "Matmul result [0,0]");
        ctx.assertNear(c(0,1), 28.f, "Matmul result [0,1]");
        ctx.assertNear(c(1,0), 49.f, "Matmul result [1,0]");
        ctx.assertNear(c(1,1), 64.f, "Matmul result [1,1]");
    }

    // Test 1.4: Matrix multiplication (larger, SIMD path)
    {
        nam::Matrix a, b, c;
        a.resize(16, 32);
        b.resize(32, 16);
        c.resize(16, 16);

        // Fill with known values
        for (int i = 0; i < 16; i++)
            for (int k = 0; k < 32; k++)
                a(i,k) = (float)(i * k) / 100.f;
        for (int k = 0; k < 32; k++)
            for (int j = 0; j < 16; j++)
                b(k,j) = (float)(k + j) / 100.f;

        nam::Matrix::multiply(c, a, b);

        // Verify a few elements manually
        float expected = 0.f;
        for (int k = 0; k < 32; k++)
            expected += a(5, k) * b(k, 7);
        ctx.assertNear(c(5, 7), expected, 1e-4f, "Matmul larger [5,7]");
    }

    // Test 1.5: Colwise addition
    {
        nam::Matrix m;
        nam::Vector v;
        m.resize(3, 4);
        v.resize(3);

        m.setZero();
        v(0) = 1.f; v(1) = 2.f; v(2) = 3.f;

        nam::Matrix::add_colwise(m, v);

        for (int j = 0; j < 4; j++) {
            ctx.assertNear(m(0, j), 1.f, "Colwise add [0,*]");
            ctx.assertNear(m(1, j), 2.f, "Colwise add [1,*]");
            ctx.assertNear(m(2, j), 3.f, "Colwise add [2,*]");
        }
    }

    // Test 1.6: Block/span access
    {
        nam::Matrix m;
        m.resize(6, 6);
        for (int i = 0; i < 6; i++)
            for (int j = 0; j < 6; j++)
                m(i,j) = (float)(i * 10 + j);

        nam::MatrixSpan left = m.leftCols(2);
        ctx.assertTrue(left.cols() == 2, "leftCols size");
        ctx.assertNear(left(0, 0), 0.f, "leftCols [0,0]");
        ctx.assertNear(left(3, 1), 31.f, "leftCols [3,1]");

        nam::MatrixSpan middle = m.middleCols(2, 2);
        ctx.assertTrue(middle.cols() == 2, "middleCols size");
        ctx.assertNear(middle(1, 0), 12.f, "middleCols [1,0]");

        nam::MatrixSpan block = m.block(2, 2, 2, 2);
        ctx.assertTrue(block.rows() == 2 && block.cols() == 2, "block size");
        ctx.assertNear(block(0, 0), 22.f, "block [0,0]");
        ctx.assertNear(block(1, 1), 33.f, "block [1,1]");
    }
}
```

#### 2. Ring Buffer (`test_ring_buffer`)

```cpp
void test_ring_buffer(TestContext& ctx) {
    ctx.current_test = "ring_buffer";

    // Test 2.1: Basic write/read
    {
        nam::RingBuffer rb;
        rb.reset(2, 256, 100);  // 2 channels, max buffer 256, max lookback 100

        float input[2 * 4] = {1,2, 3,4, 5,6, 7,8};  // 4 frames, 2 channels
        rb.write(input, 4);
        rb.advance(4);

        float output[2 * 4];
        rb.read(output, 4, 0);  // No lookback

        ctx.assertNearArray(input, output, 8, "RingBuffer basic write/read");
    }

    // Test 2.2: Lookback
    {
        nam::RingBuffer rb;
        rb.reset(1, 256, 100);

        float input[6] = {1, 2, 3, 4, 5, 6};
        rb.write(input, 6);
        rb.advance(6);

        float output[3];
        rb.read(output, 3, 3);  // Read 3 frames, lookback 3

        // Should get {1, 2, 3} (what was written 3 frames ago)
        ctx.assertNear(output[0], 1.f, "Lookback [0]");
        ctx.assertNear(output[1], 2.f, "Lookback [1]");
        ctx.assertNear(output[2], 3.f, "Lookback [2]");
    }

    // Test 2.3: Rewind behavior
    {
        nam::RingBuffer rb;
        rb.reset(1, 256, 10);  // Small lookback to force rewind

        // Write enough to trigger multiple rewinds
        for (int batch = 0; batch < 20; batch++) {
            float input[64];
            for (int i = 0; i < 64; i++)
                input[i] = (float)(batch * 64 + i);
            rb.write(input, 64);
            rb.advance(64);
        }

        // Verify lookback still works after rewinds
        float output[10];
        rb.read(output, 10, 10);

        // Values should be consistent with position
        bool consistent = true;
        for (int i = 0; i < 10; i++) {
            // Should match values written 10 frames before current position
            float expected = (float)(20 * 64 - 10 + i);
            if (std::fabs(output[i] - expected) > 1e-4f)
                consistent = false;
        }
        ctx.assertTrue(consistent, "RingBuffer rewind consistency");
    }

    // Test 2.4: Multi-channel
    {
        nam::RingBuffer rb;
        rb.reset(4, 256, 50);

        float input[4 * 8];  // 8 frames, 4 channels
        for (int i = 0; i < 8; i++) {
            for (int c = 0; c < 4; c++) {
                input[i * 4 + c] = (float)(c * 100 + i);
            }
        }
        rb.write(input, 8);
        rb.advance(8);

        float output[4 * 8];
        rb.read(output, 8, 0);

        ctx.assertNearArray(input, output, 32, "RingBuffer multi-channel");
    }
}
```

#### 3. JSON Parsing with Jansson (`test_json_parser`)

```cpp
#include <jansson.h>

void test_json_parser(TestContext& ctx) {
    ctx.current_test = "json_parser";

    // Test 3.1: Simple object with Jansson
    {
        const char* json = R"({"name": "test", "value": 42, "active": true})";
        json_error_t error;
        json_t* root = json_loads(json, 0, &error);
        ctx.assertTrue(root != nullptr, "JSON parse success");

        json_t* name = json_object_get(root, "name");
        ctx.assertTrue(json_is_string(name), "JSON 'name' is string");
        ctx.assertTrue(std::string(json_string_value(name)) == "test", "JSON 'name' value");

        json_t* value = json_object_get(root, "value");
        ctx.assertTrue(json_is_integer(value), "JSON 'value' is integer");
        ctx.assertTrue(json_integer_value(value) == 42, "JSON 'value' value");

        json_decref(root);
    }

    // Test 3.2: Nested objects
    {
        const char* json = R"({
            "config": {
                "channels": 16,
                "kernel_size": 3
            }
        })";
        json_error_t error;
        json_t* root = json_loads(json, 0, &error);

        json_t* config = json_object_get(root, "config");
        ctx.assertTrue(json_is_object(config), "JSON nested is object");

        json_t* channels = json_object_get(config, "channels");
        ctx.assertNear((float)json_integer_value(channels), 16.f, "Nested value");

        json_decref(root);
    }

    // Test 3.3: Arrays
    {
        const char* json = R"({"dilations": [1, 2, 4, 8, 16, 32, 64]})";
        json_error_t error;
        json_t* root = json_loads(json, 0, &error);

        json_t* dilations = json_object_get(root, "dilations");
        ctx.assertTrue(json_is_array(dilations), "JSON array type");
        ctx.assertTrue(json_array_size(dilations) == 7, "JSON array size");
        ctx.assertTrue(json_integer_value(json_array_get(dilations, 0)) == 1, "JSON array [0]");
        ctx.assertTrue(json_integer_value(json_array_get(dilations, 6)) == 64, "JSON array [6]");

        json_decref(root);
    }

    // Test 3.4: Float arrays (weight loading)
    {
        const char* json = R"({"weights": [0.1, 0.2, 0.3, -0.4, 0.5]})";
        json_error_t error;
        json_t* root = json_loads(json, 0, &error);

        json_t* weights = json_object_get(root, "weights");
        ctx.assertTrue(json_array_size(weights) == 5, "Float array size");
        ctx.assertNear((float)json_number_value(json_array_get(weights, 0)), 0.1f, "Float array [0]");
        ctx.assertNear((float)json_number_value(json_array_get(weights, 3)), -0.4f, "Float array [3]");

        json_decref(root);
    }

    // Test 3.5: Parse actual .nam file structure
    {
        const char* json = R"({
            "version": "0.5.4",
            "architecture": "WaveNet",
            "config": {
                "input_size": 1,
                "condition_size": 0,
                "head_size": 8,
                "channels": 16,
                "kernel_size": 3
            },
            "sample_rate": 48000,
            "metadata": {
                "loudness": -20.5
            }
        })";
        json_error_t error;
        json_t* root = json_loads(json, 0, &error);

        json_t* arch = json_object_get(root, "architecture");
        ctx.assertTrue(std::string(json_string_value(arch)) == "WaveNet", "Architecture parse");

        json_t* config = json_object_get(root, "config");
        json_t* head_size = json_object_get(config, "head_size");
        ctx.assertNear((float)json_integer_value(head_size), 8.f, "Config nested parse");

        json_t* sample_rate = json_object_get(root, "sample_rate");
        ctx.assertNear((float)json_integer_value(sample_rate), 48000.f, "Sample rate parse");

        json_t* metadata = json_object_get(root, "metadata");
        json_t* loudness = json_object_get(metadata, "loudness");
        ctx.assertNear((float)json_number_value(loudness), -20.5f, "Metadata nested parse");

        json_decref(root);
    }
}
```

#### 4. Activation Functions (`test_activations`)

```cpp
void test_activations(TestContext& ctx) {
    ctx.current_test = "activations";

    // Test 4.1: Tanh vs fast_tanh accuracy
    {
        for (float x = -5.f; x <= 5.f; x += 0.5f) {
            float ref = std::tanh(x);
            float fast = nam::fast_tanh(x);
            ctx.assertNear(fast, ref, 0.01f, "fast_tanh accuracy");  // 1% tolerance
        }
    }

    // Test 4.2: Sigmoid vs fast_sigmoid accuracy
    {
        for (float x = -5.f; x <= 5.f; x += 0.5f) {
            float ref = 1.f / (1.f + std::exp(-x));
            float fast = nam::fast_sigmoid(x);
            ctx.assertNear(fast, ref, 0.01f, "fast_sigmoid accuracy");
        }
    }

    // Test 4.3: SIMD activation
    {
        float data[8] = {-1.f, -0.5f, 0.f, 0.5f, 1.f, 1.5f, 2.f, 2.5f};
        float expected[8];
        for (int i = 0; i < 8; i++)
            expected[i] = std::tanh(data[i]);

        nam::apply_tanh_simd(data, 8);

        for (int i = 0; i < 8; i++) {
            ctx.assertNear(data[i], expected[i], 0.01f, "SIMD tanh accuracy");
        }
    }

    // Test 4.4: Activation class interface
    {
        nam::Activation* tanh_act = nam::createActivation("Tanh");
        nam::Activation* fast_tanh_act = nam::createActivation("FastTanh");

        float data[4] = {1.f, 2.f, 3.f, 4.f};
        tanh_act->apply(data, 4);

        ctx.assertNear(data[0], std::tanh(1.f), 1e-6f, "Tanh activation [0]");
        ctx.assertNear(data[1], std::tanh(2.f), 1e-6f, "Tanh activation [1]");

        delete tanh_act;
        delete fast_tanh_act;
    }
}
```

#### 5. Conv1D Layer (`test_conv1d`)

```cpp
void test_conv1d(TestContext& ctx) {
    ctx.current_test = "conv1d";

    // Test 5.1: Basic convolution (no dilation)
    {
        nam::Conv1D conv;
        // 1 input channel, 2 output channels, kernel size 3, dilation 1
        conv.init(1, 2, 3, 1, 256);

        // Set simple weights
        // Output[0] = input*1 + input[-1]*2 + input[-2]*3 + bias[0]
        // Output[1] = input*0.5 + input[-1]*0.5 + input[-2]*0.5 + bias[1]
        std::vector<float> weights = {
            1.f, 2.f, 3.f,      // kernel for output channel 0
            0.5f, 0.5f, 0.5f    // kernel for output channel 1
        };
        conv.setWeights(weights, {0.1f, -0.1f});

        float input[6] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f};  // 6 frames
        conv.process(input, 6);

        const float* output = conv.getOutput();
        // Verify first output sample (with zero-padding for lookback)
        // Out[0][0] = 1*1 + 0*2 + 0*3 + 0.1 = 1.1
        // Out[1][0] = 1*0.5 + 0*0.5 + 0*0.5 - 0.1 = 0.4
        ctx.assertNear(output[0], 1.1f, 1e-4f, "Conv1D output[0]");
        ctx.assertNear(output[1], 0.4f, 1e-4f, "Conv1D output[1]");
    }

    // Test 5.2: Dilated convolution
    {
        nam::Conv1D conv;
        conv.init(1, 1, 3, 2, 256);  // dilation = 2
        conv.setWeights({1.f, 1.f, 1.f}, {0.f});  // Sum kernel

        // Need enough history for dilation
        float input[16];
        for (int i = 0; i < 16; i++) input[i] = (float)i;

        conv.process(input, 16);
        const float* output = conv.getOutput();

        // With dilation 2, kernel looks at t, t-2, t-4
        // Output[8] = input[8] + input[6] + input[4] = 8 + 6 + 4 = 18
        ctx.assertNear(output[8], 18.f, 1e-4f, "Dilated Conv1D at frame 8");
    }
}
```

#### 6. Conv1x1 Layer (`test_conv1x1`)

```cpp
void test_conv1x1(TestContext& ctx) {
    ctx.current_test = "conv1x1";

    // Test 6.1: Basic 1x1 convolution (matrix multiply)
    {
        nam::Conv1x1 conv;
        conv.init(2, 3, 256);  // 2 input channels, 3 output channels

        // Weight matrix: 3x2
        std::vector<float> weights = {
            1.f, 0.f,   // out[0] = in[0]
            0.f, 1.f,   // out[1] = in[1]
            1.f, 1.f    // out[2] = in[0] + in[1]
        };
        conv.setWeights(weights, {0.f, 0.f, 0.f});

        float input[2 * 4] = {1.f, 2.f,  3.f, 4.f,  5.f, 6.f,  7.f, 8.f};  // 4 frames, 2 channels
        conv.process(input, 4);

        const float* output = conv.getOutput();
        // Frame 0: in = [1, 2], out = [1, 2, 3]
        ctx.assertNear(output[0], 1.f, "Conv1x1 frame 0, channel 0");
        ctx.assertNear(output[1], 2.f, "Conv1x1 frame 0, channel 1");
        ctx.assertNear(output[2], 3.f, "Conv1x1 frame 0, channel 2");

        // Frame 1: in = [3, 4], out = [3, 4, 7]
        ctx.assertNear(output[3], 3.f, "Conv1x1 frame 1, channel 0");
        ctx.assertNear(output[4], 4.f, "Conv1x1 frame 1, channel 1");
        ctx.assertNear(output[5], 7.f, "Conv1x1 frame 1, channel 2");
    }
}
```

#### 7. Linear Model (`test_linear`)

```cpp
void test_linear(TestContext& ctx) {
    ctx.current_test = "linear";

    // Test 7.1: Simple FIR
    {
        nam::Linear model;
        model.init({0.5f, 0.3f, 0.2f}, 0.1f);  // weights, bias

        float input[5] = {1.f, 2.f, 3.f, 4.f, 5.f};
        float output[5];

        model.process(input, output, 5);

        // Output[0] = bias + w[0]*in[0] + w[1]*0 + w[2]*0 = 0.1 + 0.5*1 = 0.6
        // Output[2] = bias + w[0]*in[2] + w[1]*in[1] + w[2]*in[0] = 0.1 + 0.5*3 + 0.3*2 + 0.2*1 = 2.4
        ctx.assertNear(output[0], 0.6f, 1e-4f, "Linear output[0]");
        ctx.assertNear(output[2], 2.4f, 1e-4f, "Linear output[2]");
    }
}
```

#### 8-10. Architecture Tests (ConvNet, LSTM, WaveNet)

```cpp
void test_convnet(TestContext& ctx) {
    ctx.current_test = "convnet";
    // Load a small ConvNet model and verify output matches expected
    // Test file: res/models/test_convnet.nam (create if needed)
}

void test_lstm(TestContext& ctx) {
    ctx.current_test = "lstm";
    // Test LSTM cell state management
    // Test multi-layer stacking
    // Load a small LSTM model and verify output
}

void test_wavenet(TestContext& ctx) {
    ctx.current_test = "wavenet";

    // Test 10.1: Single layer
    {
        // Verify layer residual connection
        // Verify gated activation: tanh(x) * sigmoid(gate)
        // Verify skip output
    }

    // Test 10.2: Full WaveNet
    {
        nam::WaveNet model;
        // Load config and weights for a minimal WaveNet
        // Process audio and verify output shape
    }
}
```

#### 11. Model Loader (`test_model_loader`)

```cpp
void test_model_loader(TestContext& ctx) {
    ctx.current_test = "model_loader";

    // Test 11.1: Load all models in res/models
    {
        std::vector<std::string> models = list_files("res/models", ".nam");
        int loaded = 0;
        int failed = 0;

        for (const auto& path : models) {
            try {
                auto dsp = nam::get_dsp(path);
                if (dsp) {
                    loaded++;
                } else {
                    failed++;
                    std::cerr << "Failed to load: " << path << std::endl;
                }
            } catch (const std::exception& e) {
                failed++;
                std::cerr << "Exception loading " << path << ": " << e.what() << std::endl;
            }
        }

        ctx.assertTrue(failed == 0, "All models loaded successfully");
        std::cout << "Loaded " << loaded << " models from res/models" << std::endl;
    }

    // Test 11.2: Architecture detection
    {
        // Load specific models and verify architecture is detected correctly
    }

    // Test 11.3: Metadata extraction
    {
        // Verify loudness, sample_rate, etc. are extracted correctly
    }
}
```

#### 12. Comparison vs Original (`test_comparison_vs_original`)

```cpp
void test_comparison_vs_original(TestContext& ctx) {
    ctx.current_test = "comparison";

    // Test 12.1: Output matches original NAM implementation
    // This requires both implementations to be available temporarily
    {
        std::vector<std::string> test_models = {
            "res/models/amelton_v1_0R1R1R.nam",
            "res/models/aim.getM.nam",
            // ... select a diverse set of models
        };

        for (const auto& path : test_models) {
            // Load with original NAM
            auto original = nam_original::get_dsp(path);
            // Load with rewritten NAM
            auto rewritten = nam::get_dsp(path);

            // Generate test input
            const int block_size = 256;
            std::vector<float> input(block_size);
            for (int i = 0; i < block_size; i++)
                input[i] = 0.5f * std::sin(2 * M_PI * 440 * i / 48000);

            std::vector<float> output_orig(block_size);
            std::vector<float> output_new(block_size);

            // Process
            original->process(input.data(), output_orig.data(), block_size);
            rewritten->process(input.data(), output_new.data(), block_size);

            // Compare
            float max_diff = 0.f;
            for (int i = 0; i < block_size; i++) {
                float diff = std::fabs(output_orig[i] - output_new[i]);
                max_diff = std::max(max_diff, diff);
            }

            char msg[256];
            snprintf(msg, sizeof(msg), "Output match for %s (max diff: %f)",
                     path.c_str(), max_diff);
            ctx.assertNear(max_diff, 0.f, 1e-4f, msg);
        }
    }
}
```

#### 13. Performance Benchmarks (`test_performance`)

```cpp
void test_performance(TestContext& ctx) {
    ctx.current_test = "performance";

    // Test 13.1: CPU usage benchmark (target: max 10%)
    {
        std::vector<std::string> models = list_files("res/models", ".nam");

        const int block_size = 256;
        const int sample_rate = 48000;
        const int duration_ms = 5000;
        const int num_blocks = (duration_ms * sample_rate) / (block_size * 1000);

        std::vector<float> input(block_size, 0.5f);
        std::vector<float> output(block_size);

        int passed = 0;
        int failed = 0;
        float max_cpu = 0.f;

        for (const auto& path : models) {
            auto dsp = nam::get_dsp(path);
            if (!dsp) continue;

            auto start = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < num_blocks; i++) {
                dsp->process(input.data(), output.data(), block_size);
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

            float cpu_percent = (float)elapsed_ms / duration_ms * 100.0f;
            max_cpu = std::max(max_cpu, cpu_percent);

            if (cpu_percent <= 10.0f) {
                passed++;
            } else {
                failed++;
                std::cerr << "Performance FAIL [" << path << "]: " << cpu_percent << "%" << std::endl;
            }
        }

        std::cout << "Performance: " << passed << " passed, " << failed << " failed" << std::endl;
        std::cout << "Max CPU: " << max_cpu << "%" << std::endl;

        ctx.assertTrue(failed == 0, "All models meet 10% CPU target");
    }

    // Test 13.2: Latency measurement
    {
        // Measure worst-case single block processing time
    }

    // Test 13.3: Memory usage
    {
        // Verify no allocations during process() calls
    }
}
```

### Test Models Available

The project includes **200+ .nam models** in `res/models/` covering:
- Various amplifiers (5150, Marshall, Mesa, Fender, etc.)
- Different architectures (WaveNet, ConvNet, LSTM)
- Range of model complexities (lightweight to heavy)

Use these for validation throughout development.

### Running Tests

```bash
# Build test executable
make test_nam_rewrite

# Run all tests
./test_nam_rewrite

# Run specific test category
./test_nam_rewrite --filter=matrix
./test_nam_rewrite --filter=performance

# Verbose output
./test_nam_rewrite --verbose
```

### Integration Tests

1. **Load all res/models/*.nam files**
   - Verify all load without errors
   - Check sample rate detection
   - Validate metadata extraction

2. **Real-time performance**
   - CPU usage measurement (target: max 10%)
   - Latency verification
   - Memory usage profiling

3. **Edge cases**
   - Empty input
   - Very long sequences
   - Rapid model switching

### Performance Benchmarks

```cpp
// Benchmark framework
void benchmark_model(const std::string& model_path, int duration_ms = 5000) {
    auto model = nam::get_dsp(model_path);

    const int block_size = 256;
    const int sample_rate = 48000;
    const int num_blocks = (duration_ms * sample_rate) / (block_size * 1000);

    std::vector<float> input(block_size, 0.5f);
    std::vector<float> output(block_size);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_blocks; i++) {
        model->process(input.data(), output.data(), block_size);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    float cpu_percent = (float)elapsed_ms / duration_ms * 100.0f;

    printf("Model: %s\n", model_path.c_str());
    printf("  CPU: %.1f%%\n", cpu_percent);
    printf("  Status: %s\n", cpu_percent <= 10.0f ? "PASS" : "FAIL");
}
```

---

## Risks and Mitigations

### Risk 1: Performance Regression

**Impact**: High - VCV Rack requires real-time processing

**Mitigation**:
- Profile before and after
- Optimize hot paths (matrix multiply)
- Consider SIMD optimization

### Risk 2: Numerical Accuracy

**Impact**: Medium - Audio quality depends on precision

**Mitigation**:
- Use float (not double) consistently
- Compare outputs with tolerance
- Test with edge-case inputs

### Risk 3: Model Compatibility

**Impact**: High - Must load existing .nam files

**Mitigation**:
- Test with diverse model collection
- Maintain JSON parsing compatibility
- Version checking

### Risk 4: macOS 10.9 Issues

**Impact**: Low - Legacy platform

**Mitigation**:
- Avoid C++14/17 features
- Test on older compiler if possible
- Use standard library features conservatively

---

## Success Criteria

1. **Functional**
   - All 200+ models in `res/models/` load correctly
   - Audio output matches original within tolerance (1e-5)
   - All architectures supported (Linear, ConvNet, LSTM, WaveNet)

2. **Performance**
   - **Max 10% CPU usage** at 48kHz with 256-sample buffers
   - No audio dropouts at typical buffer sizes (64-512 samples)
   - Memory usage similar to or better than original

3. **Compatibility**
   - C++11 compilation successful on all platforms
   - macOS 10.9 deployment target met
   - **Zero external dependencies** (only VCV Rack SDK)

4. **Integration**
   - **Direct replacement** - delete old NAM, add new files
   - No changes required to NamPlayer module code
   - Existing patches load correctly

---

## Build System Changes

After the NAM rewrite is complete, the following changes must be made to the build system.

### Makefile Changes

**Current Makefile** (before rewrite):
```makefile
# If RACK_DIR is not defined when calling the Makefile, default to Rack-SDK in dep
RACK_DIR ?= dep/Rack-SDK

# NAM Core paths
NAM_DIR := dep/NeuralAmpModelerCore

# Include paths - use NAM's bundled Eigen (3.4 pre-release with placeholders::lastN support)
FLAGS += -I$(NAM_DIR)
FLAGS += -I$(NAM_DIR)/Dependencies/eigen
FLAGS += -I$(NAM_DIR)/Dependencies/nlohmann

# Platform-specific flags
# NAM requires macOS 10.15+ for std::filesystem and C++17
ifeq ($(shell uname -s),Darwin)
    EXTRA_FLAGS := -mmacosx-version-min=10.15 -std=c++17
else
    EXTRA_FLAGS := -std=c++17
endif

# NAM source files
NAM_SOURCES := $(NAM_DIR)/NAM/activations.cpp
NAM_SOURCES += $(NAM_DIR)/NAM/conv1d.cpp
NAM_SOURCES += $(NAM_DIR)/NAM/convnet.cpp
NAM_SOURCES += $(NAM_DIR)/NAM/dsp.cpp
NAM_SOURCES += $(NAM_DIR)/NAM/get_dsp.cpp
NAM_SOURCES += $(NAM_DIR)/NAM/lstm.cpp
NAM_SOURCES += $(NAM_DIR)/NAM/ring_buffer.cpp
NAM_SOURCES += $(NAM_DIR)/NAM/util.cpp
NAM_SOURCES += $(NAM_DIR)/NAM/wavenet.cpp

SOURCES += $(NAM_SOURCES)
```

**Updated Makefile** (after rewrite):
```makefile
# If RACK_DIR is not defined when calling the Makefile, default to Rack-SDK in dep
RACK_DIR ?= dep/Rack-SDK

# Rewritten NAM implementation (no external dependencies)
# All NAM sources are now in src/dsp/nam/
NAM_SOURCES := $(wildcard src/dsp/nam/*.cpp)

SOURCES += $(NAM_SOURCES)

# Platform-specific flags
# C++11 compatible, macOS 10.9+ support
ifeq ($(shell uname -s),Darwin)
    EXTRA_FLAGS := -mmacosx-version-min=10.9 -std=c++11
else
    EXTRA_FLAGS := -std=c++11
endif
```

**Key changes**:
1. Remove `NAM_DIR` variable and related include paths
2. Remove Eigen and nlohmann include paths
3. Change from C++17 to C++11
4. Change macOS minimum from 10.15 to 10.9
5. Update NAM_SOURCES to point to new `src/dsp/nam/` directory
6. No external dependency includes needed

### GitHub Workflow Changes

The workflow in `.github/workflows/build_and_publish.yml` will need minor updates:

**Current workflow** runs:
```yaml
- name: Run Tests (with coverage reporting)
  shell: bash
  run: |
    chmod +x ./run_tests_with_coverage.sh
    ./run_tests_with_coverage.sh
```

**Updated considerations**:

1. **Test file location**: The new test file `src/tests/test_nam_rewrite.cpp` should be integrated into the existing test runner or run separately.

2. **Update run_tests_with_coverage.sh** to include:
```bash
# Build and run NAM rewrite tests
echo "Building NAM rewrite tests..."
$CXX -std=c++11 -I$RACK_DIR/include \
    src/tests/test_nam_rewrite.cpp \
    src/dsp/nam/*.cpp \
    -o test_nam_rewrite

echo "Running NAM rewrite tests..."
./test_nam_rewrite
```

3. **Coverage considerations**: If using coverage reporting, ensure the new sources are included:
```bash
# Coverage for NAM rewrite sources
gcov src/dsp/nam/*.cpp
```

### Dependency Cleanup

After the rewrite is complete, these dependencies can be removed:

1. **Delete submodule**:
```bash
git submodule deinit -f dep/NeuralAmpModelerCore
rm -rf .git/modules/dep/NeuralAmpModelerCore
git rm -f dep/NeuralAmpModelerCore
```

2. **Update .gitmodules** (if NeuralAmpModelerCore is listed):
```ini
# Remove this section:
[submodule "dep/NeuralAmpModelerCore"]
    path = dep/NeuralAmpModelerCore
    url = https://github.com/sdatkinson/NeuralAmpModelerCore.git
```

3. **Keep Rack SDK submodule** (required for VCV Rack plugin development):
```ini
[submodule "dep/Rack-SDK"]
    path = dep/Rack-SDK
    url = https://github.com/VCVRack/Rack-SDK.git
```

### File Structure After Rewrite

```
swv-guitar-collection/
├── dep/
│   └── Rack-SDK/          # Keep - VCV Rack SDK (includes Jansson for JSON)
├── src/
│   ├── dsp/
│   │   ├── nam/           # NEW - Rewritten NAM implementation
│   │   │   ├── nam_dsp.h
│   │   │   ├── nam_dsp.cpp
│   │   │   ├── matrix.h
│   │   │   ├── ring_buffer.h
│   │   │   ├── ring_buffer.cpp
│   │   │   ├── activations.h
│   │   │   ├── conv1d.h
│   │   │   ├── conv1d.cpp
│   │   │   ├── conv1x1.h
│   │   │   ├── conv1x1.cpp
│   │   │   ├── linear.h
│   │   │   ├── linear.cpp
│   │   │   ├── convnet.h
│   │   │   ├── convnet.cpp
│   │   │   ├── lstm.h
│   │   │   ├── lstm.cpp
│   │   │   ├── wavenet.h
│   │   │   ├── wavenet.cpp
│   │   │   ├── model_loader.h
│   │   │   └── model_loader.cpp
│   │   └── Nam.h          # Updated to use new nam/ implementation
│   └── tests/
│       ├── test_swv_guitar_collection.cpp  # Existing plugin tests
│       └── test_nam_rewrite.cpp            # NEW - NAM rewrite tests
├── Makefile               # Updated
└── .github/workflows/
    └── build_and_publish.yml  # Minor updates if needed
```

### Migration Checklist

- [ ] Create `src/dsp/nam/` directory
- [ ] Implement all NAM rewrite files
- [ ] Update `src/dsp/Nam.h` to include from `nam/` subdirectory
- [ ] Update Makefile:
  - [ ] Remove Eigen/nlohmann include paths
  - [ ] Change C++17 to C++11
  - [ ] Change macOS 10.15 to 10.9
  - [ ] Update NAM_SOURCES path
- [ ] Create `src/tests/test_nam_rewrite.cpp`
- [ ] Update `run_tests_with_coverage.sh` if needed
- [ ] Verify all 200+ models load correctly
- [ ] Verify performance meets 10% CPU target
- [ ] Remove `dep/NeuralAmpModelerCore` submodule
- [ ] Update .gitmodules
- [ ] Test build on all platforms (Linux, Windows, macOS x64, macOS ARM)
- [ ] Merge to main branch

---

## Appendix D: Rack SDK Reference

### Headers to Include

```cpp
// Math utilities
#include <math.hpp>  // rack::math::clamp, rescale, crossfade

// SIMD support
#include <simd/functions.hpp>  // simd::float_4, sin, cos, tan, exp, etc.

// DSP utilities
#include <dsp/common.hpp>      // Frame, sinc, amplitudeToDb, dbToAmplitude
#include <dsp/filter.hpp>      // BiquadFilter, IIRFilter, etc.
#include <dsp/ringbuffer.hpp>  // RingBuffer, DoubleRingBuffer
#include <dsp/resampler.hpp>   // SampleRateConverter

// File system
#include <system.hpp>  // isFile, readFile, getFilename, etc.

// String utilities
#include <string.hpp>  // trim, split, startsWith, etc.
```

### BiquadFilter Consideration

The current `Nam.h` implements a custom `BiquadFilter` struct for the tone stack. Rack SDK provides `rack::dsp::BiquadFilter` which supports:
- LOWPASS, HIGHPASS
- LOWSHELF, HIGHSHELF
- PEAK, NOTCH, BANDPASS

**Option**: Consider replacing custom `BiquadFilter` with Rack's implementation:
```cpp
// Current custom implementation
struct BiquadFilter { ... };

// Could use Rack SDK instead
#include <dsp/filter.hpp>
rack::dsp::BiquadFilter bass;
bass.setParameters(rack::dsp::BiquadFilter::LOWSHELF, freq/sr, Q, gainDb);
```

This would reduce code and ensure consistency with Rack conventions.

### Key SIMD Patterns

```cpp
// Load 4 floats from array
rack::simd::float_4 v = rack::simd::float_4::load(ptr);

// SIMD tanh (4x faster)
v = rack::simd::tan(v);

// Store back to array
v.store(ptr);

// Conditional operations
rack::simd::float_4 mask = (v > 0.f);  // All 1s where true
v = rack::simd::ifelse(mask, v, -v);   // Absolute value

// Horizontal operations (across vector elements)
float sum = v.s[0] + v.s[1] + v.s[2] + v.s[3];
```

---

## Appendix A: Key Files to Reference

### Current Implementation

- `dep/NeuralAmpModelerCore/NAM/dsp.h` - Main DSP interface
- `dep/NeuralAmpModelerCore/NAM/get_dsp.cpp` - Model loading
- `dep/NeuralAmpModelerCore/NAM/conv1d.h/cpp` - Conv1D implementation
- `dep/NeuralAmpModelerCore/NAM/convnet.h/cpp` - ConvNet architecture
- `dep/NeuralAmpModelerCore/NAM/lstm.h/cpp` - LSTM architecture
- `dep/NeuralAmpModelerCore/NAM/wavenet.h/cpp` - WaveNet architecture
- `dep/NeuralAmpModelerCore/NAM/ring_buffer.h/cpp` - Ring buffer
- `dep/NeuralAmpModelerCore/NAM/activations.h/cpp` - Activation functions

### Integration Points

- `src/dsp/Nam.h` - Current wrapper (uses `nam::get_dsp()`, `nam::DSP`)

---

## Appendix B: JSON Model Format

The minimal JSON parser must handle this structure:

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
    "dilations": [1, 2, 4, 8, 16, 32, 64],
    "head_bias": true
  },
  "weights": {
    "_input_layer.weight": [...],
    "_input_layer.bias": [...],
    // ... more weights (large float arrays)
  },
  "metadata": {
    "loudness": -20.0,
    "input_level_dbu": 18.3,
    "output_level_dbu": 12.3
  },
  "sample_rate": 48000
}
```

**Required JSON features**:
- String, number, boolean, null types
- Nested objects
- Arrays of numbers (especially large float arrays for weights)
- No need for: escaped unicode, scientific notation, comments

---

## Appendix C: Eigen Operations to Replace

| Eigen Operation | Replacement |
|-----------------|-------------|
| `MatrixXf m(rows, cols)` | `Matrix m; m.resize(rows, cols)` |
| `m.setZero()` | `m.setZero()` |
| `m(i, j)` | `m(i, j)` |
| `m.leftCols(n)` | `MatrixSpan(m, 0, 0, m.rows(), n)` |
| `m.middleCols(start, n)` | `MatrixSpan(m, 0, start, m.rows(), n)` |
| `m.middleRows(start, n)` | `MatrixSpan(m, start, 0, n, m.cols())` |
| `m.block(r, c, rows, cols)` | `MatrixSpan(m, r, c, rows, cols)` |
| `m.colwise() += v` | `add_colwise(m, v)` |
| `a * b` (matrix multiply) | `matrix_multiply(c, a, b)` |
| `a.noalias() += b * c` | `matrix_multiply_accumulate(a, b, c)` |
| `VectorXf v(size)` | `Vector v; v.resize(size)` |
| `v(Eigen::placeholders::lastN(n))` | `v.span(v.size() - n, n)` |

---

## Next Steps

1. ~~Review and approve this plan~~ ✓
2. Create feature branch for rewrite
3. Implement Stage 1 (Foundation)
   - matrix.h, ring_buffer
   - activations.h
   - (JSON parsing via Jansson - no implementation needed)
4. Test with sample models from res/models
5. Implement Stage 2 (Layers)
   - conv1d, conv1x1
6. Implement Stage 3 (Architectures)
   - linear, convnet, lstm, wavenet
7. Implement Stage 4 (Integration)
   - model_loader (uses Jansson for JSON)
   - nam_dsp
   - Update Nam.h to use new implementation
8. Performance testing - verify 10% CPU target
9. Load test all 200+ models in res/models
10. Direct replacement - remove old NAM dependency
