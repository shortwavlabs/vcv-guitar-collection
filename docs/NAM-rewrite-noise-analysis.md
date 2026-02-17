# NAM Rewrite Noise Analysis

> **Date**: 2026-02-17  
> **Status**: Root cause fixed, stabilization complete  
> **Symptom**: Extreme noise output from the NAM Player plugin even with silence/no audio input (full-scale noise visible on waveform display)

## Executive Summary

A comprehensive line-by-line comparison of the refactored `src/dsp/nam_rack/` code against the original `dep/NeuralAmpModelerCore/NAM/` identified **12 bugs** across 4 severity levels. During follow-up implementation and validation, the primary runtime noise issue was traced to a **sample-type ABI mismatch (`NAM_SAMPLE` float/double inconsistency across translation units)** and fixed. The plugin now loads and processes the affected WaveNet models correctly.

## 2026-02-17 Post-Fix Status (HIGH + MODERATE)

### HIGH Severity

- **Bug 5 (WaveNet JSON field mismatch)**: **Addressed**. Loader now supports both `layers`/`layer_arrays`, `groups`/`groups_input`, and fallback head detection.
- **Bug 6 (non-gated activation over full matrix)**: **Addressed**. Activation is applied only over active frames.
- **Bug 7 (`with_head` parsing difference)**: **Addressed**. Compatibility parsing supports both conventions.

### MODERATE Severity

- **Bug 8 (triple prewarm)**: **Addressed**. Redundant prewarm calls in `NamDSP::loadModel()` were removed.
- **Bug 9 (NaN/Inf protection)**: **Addressed (wrapper-level)**. `NamDSP` sanitizes non-finite values and clips extreme outputs in runtime processing.
- **Bug 12 (fast_tanh default mismatch)**: **Addressed**. Default now matches upstream behavior (`fast_tanh` disabled by default).
- **Bug 10 (naive matrix multiply)**: **Open optimization**. Not currently the source of incorrect output, but still a performance improvement opportunity.
- **Bug 11 (RingBuffer copy instead of zero-copy view)**: **Open optimization**. Functionally correct; can be optimized for CPU/memory bandwidth.

---

---

## CRITICAL Bugs (Directly Cause Noise / Incorrect Output)

### Bug 1: Real-Time Heap Allocation in Conv1D::process()

**Files**: `src/dsp/nam_rack/conv1d.cpp` (lines ~130-165)

The refactored `Conv1D::process()` allocates **two temporary `Matrix` objects** (each backed by `std::vector<float>`) **per kernel tap, per process call**. For a typical WaveNet with kernel_size=3 and ~30 layers, this means **~60 heap allocations per audio block** — inside the real-time audio thread.

**Refactored (BUGGY):**
```cpp
void Conv1D::process(const Matrix& input, int num_frames) {
    _inputBuffer.write(input, num_frames);
    // ...
    for (size_t k = 0; k < _weight.size(); k++) {
        // ALLOCATION #1: new std::vector<float> inside Matrix
        Matrix input_block;
        input_block.resize(_inputBuffer.getChannels(), num_frames);
        _inputBuffer.read(input_block, num_frames, lookback);

        // ALLOCATION #2: another std::vector<float> inside Matrix
        Matrix temp;
        temp.resize(_output.rows(), num_frames);
        Matrix::multiply(temp, _weight[k], input_block);
        // ...
    }
}
```

**Original (CORRECT):**
```cpp
void Conv1D::Process(const Eigen::MatrixXf& input, const int num_frames) {
    _input_buffer.Write(input, num_frames);
    _output.leftCols(num_frames).setZero();
    for (size_t k = 0; k < this->_weight.size(); k++) {
        // ZERO-COPY: Returns Eigen::Block view into ring buffer storage
        auto input_block = _input_buffer.Read(num_frames, lookback);
        // IN-PLACE: Accumulates directly into pre-allocated _output
        _output.leftCols(num_frames).noalias() += this->_weight[k] * input_block;
    }
    _input_buffer.Advance(num_frames);
}
```

**Impact**: Heap allocation in real-time audio threads causes unpredictable latency spikes when the allocator needs to acquire locks or request memory from the OS. This manifests as **audio glitches, clicks, and noise-like artifacts**. With 60+ allocations per block, glitches would be near-continuous.

**Fix**: Pre-allocate `input_block` and `temp` matrices in `setMaxBufferSize()` and reuse them. Better yet, have the ring buffer return a view/pointer rather than copying data.

---

### Bug 2: Buffer Overflow in Buffer::updateBuffers()

**Files**: `src/dsp/nam_rack/dsp.cpp` (lines ~108-113)

The refactored `updateBuffers()` writes input data at `mInputBufferOffset` without checking whether the write would exceed the buffer size. The original `_update_buffers_()` performs **three safety checks** that are all missing in the refactored version.

**Refactored (BUGGY):**
```cpp
void Buffer::updateBuffers(NAM_SAMPLE* input, int num_frames) {
    // NO overflow check!  NO dynamic resize!  NO pre-write rewind!
    for (int i = 0; i < num_frames; i++) {
        mInputBuffer[mInputBufferOffset + i] = static_cast<float>(input[i]);
    }
}
```

**Original (CORRECT):**
```cpp
void nam::Buffer::_update_buffers_(NAM_SAMPLE* input, const int num_frames) {
    // 1. Dynamic resize if buffer is too small
    const long minimum_input_buffer_size = (long)this->_receptive_field
        + _INPUT_BUFFER_SAFETY_FACTOR * num_frames;
    if ((long)this->_input_buffer.size() < minimum_input_buffer_size) {
        long new_buffer_size = 2;
        while (new_buffer_size < minimum_input_buffer_size)
            new_buffer_size *= 2;
        this->_input_buffer.resize(new_buffer_size);
        std::fill(this->_input_buffer.begin(), this->_input_buffer.end(), 0.0f);
    }

    // 2. Rewind BEFORE writing if buffer space is insufficient
    if (this->_input_buffer_offset + num_frames > (long)this->_input_buffer.size())
        this->_rewind_buffers_();

    // 3. Write data (now guaranteed to be safe)
    for (long i = this->_input_buffer_offset, j = 0; j < num_frames; i++, j++)
        this->_input_buffer[i] = input[j];

    // 4. Resize and zero output buffer
    this->_output_buffer.resize(num_frames);
    std::fill(this->_output_buffer.begin(), this->_output_buffer.end(), 0.0f);
}
```

**Impact**: Writing past the end of `mInputBuffer` causes **heap corruption**. Corrupted heap metadata can cause subsequent allocations to return overlapping memory regions, leading to data that appears as random noise when interpreted as audio samples. This is a **memory safety vulnerability** that produces undefined behavior.

**Affects**: ConvNet architecture (uses Buffer base class).

---

### Bug 3: WaveNet Gated Processing Ignores Model's Activation Function

**Files**: `src/dsp/nam_rack/wavenet.cpp` (lines ~65-80)

The refactored gated WaveNet path **hardcodes `std::tanh`** for the input activation and **hardcodes `std::exp`-based sigmoid** for the gating activation. It completely ignores the `mActivation` member variable that was set from the model's configuration.

**Refactored (BUGGY):**
```cpp
if (mGated) {
    for (int f = 0; f < num_frames; f++) {
        for (int c = 0; c < mBottleneck; c++) {
            mZ(c, f) = std::tanh(mZ(c, f));              // HARDCODED! Ignores mActivation
        }
        for (int c = 0; c < mBottleneck; c++) {
            float x = mZ(mBottleneck + c, f);
            mZ(mBottleneck + c, f) = 1.0f / (1.0f + std::exp(-x));  // HARDCODED sigmoid
        }
    }
}
```

**Original (CORRECT):**
```cpp
if (this->_gated) {
    for (int i = 0; i < num_frames; i++) {
        // Uses MODEL-SPECIFIED activation (could be Tanh, HardTanh, ReLU, etc.)
        this->_activation->apply(this->_z.block(0, i, bottleneck, 1));
        // Hardcoded Sigmoid for gating (this is correct per WaveNet architecture)
        activations::Activation::get_activation("Sigmoid")->apply(
            this->_z.block(bottleneck, i, bottleneck, 1));
    }
}
```

**Impact**: If a model specifies an activation other than "Tanh" (e.g., "ReLU", "HardTanh", "SiLU"), the refactored version produces completely wrong intermediate values. This causes the model to **output garbage** — the accumulated errors through residual connections produce noise-like output.

Even for models that DO use "Tanh", the refactored uses `std::tanh` while the original (with fast_tanh enabled) uses the polynomial approximation `fast_tanh`. While the difference is small (~0.001), it accumulates through dozens of layers and thousands of samples.

---

### Bug 4: Activation Name Mapping Mismatches

**Files**: `src/dsp/nam_rack/activations.h` (Activation::get() function)

The refactored activation lookup uses different string keys than the original, causing **silent fallthrough to identity activation** for certain model configurations.

| Model Config Value | Original Maps To | Refactored Maps To |
|---|---|---|
| `"Hardtanh"` | ActivationHardTanh ✓ | **Identity (no-op)** ✗ |
| `"Fasttanh"` | ActivationFastTanh ✓ | **Identity (no-op)** ✗ |
| `"SiLU"` | ActivationSwish ✓ | **Identity (no-op)** ✗ |
| `"Hardswish"` | ActivationHardSwish ✓ | **Identity (no-op)** ✗ |
| `"LeakyHardtanh"` | ActivationLeakyHardTanh ✓ | **Identity (no-op)** ✗ |
| `"PReLU"` | ActivationPReLU ✓ | **Identity (no-op)** ✗ |
| Unknown name | **nullptr** (would crash) | Identity (silent failure) |

**Impact**: Models using any of the unmatched activation names would have their activations silently replaced with identity (pass-through). Without the non-linearity that activations provide, the neural network becomes a simple linear transformation chain that **cannot model amplifier distortion** and is prone to producing divergent or oscillating output.

---

## HIGH Severity Bugs (Likely Incorrect Behavior)

### Bug 5: WaveNet Model Loader JSON Field Name Mismatches

**Files**: `src/dsp/nam_rack/model_loader.cpp` (createWaveNet function)

Multiple JSON field names in the WaveNet configuration parser don't match the standard NAM model format used by the original library.

| Purpose | Original Field Name | Refactored Field Name |
|---|---|---|
| Layer array config | `config["layers"]` | `config["layer_arrays"]` |
| Input groups | `layer_config["groups"]` | `arrJ["groups_input"]` |
| Head presence check | `!config["head"].is_null()` | `config["with_head"]` (boolean) |

**Impact**: If NAM model files use the original field names (which is the standard), WaveNet models would **fail to load entirely**. The plugin would fall back to passthrough mode (silence, not noise). However, if the model format was intentionally changed for the refactored version, this is a documentation issue rather than a bug.

---

### Bug 6: Non-Gated Activation Applied to Entire Matrix

**Files**: `src/dsp/nam_rack/wavenet.cpp` (Layer::process, non-gated path)

**Refactored:**
```cpp
mActivation->apply(mZ);  // Applies to ALL rows × ALL cols
```

**Original:**
```cpp
this->_activation->apply(this->_z.leftCols(num_frames));  // Only first num_frames cols
```

The `mZ` matrix is sized `(z_channels × maxBufferSize)`, but only the first `num_frames` columns contain valid data. The refactored version processes the entire matrix including stale data from previous process calls.

**Impact**: For stateless activations (Tanh, ReLU, Sigmoid), this is wasteful but not incorrect — stale columns are never read. For any future stateful activation, this would cause data leakage between frames. Current impact is **wasted CPU cycles** proportional to `(maxBufferSize / num_frames)`.

---

### Bug 7: WaveNet `with_head` Config Parsing Difference

**Files**: `src/dsp/nam_rack/model_loader.cpp`

Original determines head presence by checking if the JSON `"head"` field is null:
```cpp
const bool with_head = !config["head"].is_null();
```

Refactored looks for a separate boolean field:
```cpp
json_t* withHeadJ = json_object_get(config, "with_head");
with_head = json_boolean_value(withHeadJ);  // defaults to false
```

**Impact**: Low — both versions throw if `with_head` is true (head not implemented), and standard NAM models don't use heads. But this is a correctness issue for future model formats.

---

## MODERATE Severity Bugs (Performance / Robustness)

### Bug 8: Triple Prewarm on Model Load

**Files**: `src/dsp/nam_rack/model_loader.cpp`, `src/dsp/Nam.h`

The model is prewarmed three times during loading:

1. `createDSP()` calls `dsp->prewarm()` at the end
2. `NamDSP::loadModel()` calls `newModel->reset(...)` which internally calls `prewarm()`
3. `NamDSP::loadModel()` calls `newModel->prewarm()` again

For a WaveNet with ~8192 prewarm samples at buffer size 8192, this processes **~24,576 unnecessary silence samples** (~0.5 seconds of extra CPU time).

**Impact**: Slow model loading only. No effect on audio quality.

---

### Bug 9: Missing NaN/Inf Protection

**Files**: All processing files

The refactored code has no checks for numerical instability. The original also lacks explicit NaN checks, but its use of Eigen's optimized routines provides more predictable floating-point behavior. The naive loops in the refactored Matrix::multiply could accumulate errors differently.

**Impact**: If any intermediate value becomes NaN or Inf (e.g., from a degenerate model or numerical edge case), it would propagate through the entire network without detection, producing **noise-like output**.

---

### Bug 10: Naive Matrix Multiply vs BLAS/SIMD

**Files**: `src/dsp/nam_rack/matrix.h` (Matrix::multiply)

The custom `Matrix::multiply` is a naive triple-loop O(MKN) implementation. Eigen uses optimized BLAS routines with SIMD vectorization. For typical WaveNet dimensions (channels=16-32, bottleneck=8-16, num_frames=256):

- **Original (Eigen)**: Uses SSE/AVX instructions, ~10x faster than naive
- **Refactored**: Scalar operations only

**Impact**: Significantly higher CPU usage. May cause **audio buffer underruns** (dropouts/glitches) on systems that were previously running fine with the original Eigen-based code. VCV Rack's audio engine interprets buffer underruns as silence or repeated samples, which can appear as noise bursts.

---

### Bug 11: RingBuffer::read() Copies Data Instead of Returning a View

**Files**: `src/dsp/nam_rack/ring_buffer.cpp`

The original `RingBuffer::Read()` returns an `Eigen::Block` (zero-copy view into storage). The refactored `RingBuffer::read()` copies data to a new matrix.

**Impact**: Doubles the memory bandwidth for every convolution read operation. Combined with Bug 1 (allocation) and Bug 10 (naive multiply), this makes the refactored Conv1D roughly **20-50x slower** than the original.

---

### Bug 12: Default fast_tanh Behavior Difference

**Files**: `src/dsp/nam_rack/activations.h`

The refactored code defaults `g_useFastTanh = true`. The original defaults `using_fast_tanh = false`. While `NamPlayer.cpp` explicitly calls `enableFastTanh()`, this means the activation behavior differs if any other code path creates a model before the flag is set.

**Impact**: Minimal in practice since the plugin sets the flag at construction. But could cause inconsistent behavior in unit tests or other consumers.

---

## Root Cause Analysis (Updated)

The observed full-scale noise in runtime logs was ultimately consistent with an ABI/type mismatch in model processing buffers: **`NAM_SAMPLE` was not guaranteed to be the same type across all translation units**. Under certain build paths this could lead to float buffers being interpreted with double-based interfaces, producing deterministic huge finite values (e.g. `~3.68935e+19`) without immediate NaNs.

After enforcing a single sample type (`float`) in the NAM core interface and validating model load compatibility, the noise issue was resolved.

## Remaining Recommended Work

1. **Performance optimization (optional)**: Optimize `Matrix::multiply` for SIMD/BLAS-equivalent throughput.
2. **Performance optimization (optional)**: Introduce zero-copy/read-view path in `RingBuffer` for Conv1D reads.
3. **Regression safety (optional)**: Keep real-model stress tests and load diagnostics enabled in CI to catch future format/runtime regressions quickly.
