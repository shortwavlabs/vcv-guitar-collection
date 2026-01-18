# NAM Module Risk Assessment

## Overview

This document assesses technical risks associated with integrating Neural Amp Modeler into a VCV Rack 2.x plugin, with particular focus on performance, stability, and maintainability.

## Risk Matrix

| Risk | Severity | Likelihood | Mitigation Difficulty |
|------|----------|------------|----------------------|
| Real-time audio dropouts | High | Medium | Medium |
| Eigen memory alignment | High | Medium | Low |
| Sample rate mismatch | Medium | High | Low |
| Model loading blocking audio | High | Medium | Low |
| CPU usage too high | High | Medium | Medium |
| Cross-platform compatibility | Medium | Medium | Medium |

---

## 1. Real-Time Audio Performance

### Risk Description

NAM's neural network processing is computationally intensive. In VCV Rack's real-time audio context, any processing that takes longer than the audio buffer period will cause audio dropouts (clicks, pops, silence).

### Technical Details

- **VCV Rack audio callback:** Process is called per sample at engine sample rate
- **NAM processing:** Designed for block-based processing
- **Neural network complexity:** WaveNet models can have 10+ layers with dilated convolutions

### Measured Performance (Reference)

From NeuralAmpModelerPlugin benchmarks:
- WaveNet (typical): 2-8% CPU on modern desktop @ 48kHz/128 samples
- LSTM: 1-4% CPU
- Feather models: <1% CPU

### Mitigation Strategies

1. **Use fast tanh approximation:**
   ```cpp
   nam::activations::Activation::enable_fast_tanh();
   ```
   This reduces CPU usage by 20-30% with minimal quality loss.

2. **Optimize block size:**
   - Larger blocks = more efficient (better cache utilization)
   - Trade-off: increased latency

3. **Model selection:**
   - Recommend "Feather" models for lower CPU
   - Warn users about heavy models

4. **CPU limiting:**
   - Implement CPU meter/warning in module
   - Option to bypass processing if overloaded

### Residual Risk: **MEDIUM**

Modern CPUs handle NAM well, but users with older hardware or running many instances may experience issues.

---

## 2. Eigen Memory Alignment

### Risk Description

Eigen uses SIMD operations that require specific memory alignment (16 or 32 bytes). Misaligned memory can cause:
- Crashes (SIGBUS/SIGSEGV on some platforms)
- Severe performance degradation
- Undefined behavior

### Technical Details

From NeuralAmpModelerCore Issue #67:
> "The Eigen library requires careful handling of memory alignment when using SIMD instructions."

VCV Rack's compiler flags (`-march=nehalem`) enable SSE4.2, which has alignment requirements.

### Manifestation

- Random crashes during processing
- Crashes only on certain models
- Works in Debug, crashes in Release (different optimization levels)

### Mitigation Strategies

**Option A: Disable Eigen Vectorization (Safe but Slower)**

```makefile
FLAGS += -DEIGEN_MAX_ALIGN_BYTES=0 -DEIGEN_DONT_VECTORIZE
```

Performance impact: ~20-40% slower neural network inference.

**Option B: Ensure Proper Alignment (Performant but Complex)**

1. Use `EIGEN_MAKE_ALIGNED_OPERATOR_NEW` macro in classes containing Eigen types
2. Use `Eigen::aligned_allocator` for STL containers of Eigen types
3. Verify alignment at runtime in debug builds

```cpp
struct MyClass {
    Eigen::MatrixXd matrix;
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};
```

**Option C: Use C++17 Aligned Allocation**

With C++17, `new` respects alignment requirements by default if `alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__`.

### Recommendation

Start with **Option A** (disabled vectorization) for stability. Profile performance, and if acceptable, ship that. If performance is problematic, carefully implement Option B with extensive testing.

### Residual Risk: **LOW** (with Option A) / **MEDIUM** (with Option B)

---

## 3. Sample Rate Handling

### Risk Description

NAM models are trained at specific sample rates (typically 48kHz). Running at different sample rates causes:
- Tonal changes (EQ shift)
- Aliasing artifacts
- Incorrect transient response

### Technical Details

- VCV Rack default: 44.1kHz
- VCV Rack range: 22.05kHz to 768kHz
- NAM models: Usually 48kHz, sometimes 44.1kHz

### Mitigation Strategies

1. **Built-in Resampling (Recommended):**
   
   Use `ResamplingNAM` wrapper from NeuralAmpModelerPlugin:
   ```cpp
   class ResamplingNAM {
       // Wraps nam::DSP with sample rate conversion
       void process(float* input, float* output, int numFrames, 
                    double inputSampleRate);
   };
   ```

2. **Use VCV Rack's libsamplerate:**
   
   The SDK includes libsamplerate (`samplerate.h`):
   ```cpp
   SRC_STATE* src = src_new(SRC_SINC_MEDIUM_QUALITY, 1, &error);
   ```

3. **Document Model Requirements:**
   
   Display expected sample rate in module UI, warn if mismatched.

### Residual Risk: **LOW** (with resampling implemented)

---

## 4. Model Loading Thread Safety

### Risk Description

Loading NAM models involves:
- File I/O (disk access)
- JSON parsing
- Memory allocation for weights
- Neural network initialization

All of these are **blocking operations** that can take 100ms - 2s. If done on the audio thread, this causes severe audio dropouts.

### Mitigation Strategies

1. **Async Loading:**
   ```cpp
   std::thread loadThread([this, path]() {
       auto newModel = nam::get_dsp(path);
       // Swap atomically after initialization
   });
   ```

2. **Double Buffering:**
   - Keep current model active while loading new one
   - Atomic swap when ready

3. **Prewarming:**
   ```cpp
   newModel->prewarm();  // Run with silence to initialize internal state
   ```
   Do this on the loading thread before swapping.

### Residual Risk: **LOW** (with async loading)

---

## 5. Memory Allocation in Audio Thread

### Risk Description

Memory allocation (`new`, `malloc`) in the audio processing path can cause:
- Unpredictable latency (heap lock contention)
- Priority inversion with other threads
- Audio dropouts

### Technical Details

NAM library is designed for real-time safety. From their tests:
```cpp
// From test_dsp.cpp
TEST_CASE("No allocations during process") {
    // Verifies that nam::DSP::process() does not allocate
}
```

However, this relies on proper pre-allocation:
- Call `SetMaxBufferSize()` before processing
- Call `Reset()` when sample rate changes
- Don't resize vectors during processing

### Mitigation Strategies

1. **Pre-allocate all buffers:**
   ```cpp
   inputBuffer.resize(MAX_BLOCK_SIZE);
   outputBuffer.resize(MAX_BLOCK_SIZE);
   namModel->SetMaxBufferSize(MAX_BLOCK_SIZE);
   ```

2. **Avoid dynamic allocation in process():**
   - No `std::vector::push_back()`
   - No `std::string` operations
   - No `new` or `std::make_unique`

3. **Verify with tools:**
   - Use Allocator hooks in debug builds
   - Valgrind/AddressSanitizer testing

### Residual Risk: **LOW** (NAM is already real-time safe)

---

## 6. Cross-Platform Compatibility

### Risk Description

VCV Rack supports Windows, macOS, and Linux. NAM/Eigen must compile and run correctly on all platforms.

### Platform-Specific Issues

| Platform | Issue | Mitigation |
|----------|-------|------------|
| Windows | MSVC vs MinGW differences | Use MinGW (VCV Rack standard) |
| macOS | ARM64 vs x86_64 | Universal binary or separate builds |
| macOS | libc++ vs libstdc++ | Use libc++ (default on macOS) |
| Linux | Various glibc versions | Link statically or use older glibc |
| All | Filesystem paths | Use VCV Rack's `system::*` helpers |

### Mitigation Strategies

1. **Use VCV Rack's abstraction layers:**
   ```cpp
   #include "system.hpp"
   std::string path = system::join(assetDir, "models", filename);
   ```

2. **Test on all platforms in CI:**
   - GitHub Actions with Windows/macOS/Linux runners

3. **Static linking where possible:**
   - Avoid runtime dependencies on system libraries

### Residual Risk: **MEDIUM** (requires CI infrastructure)

---

## 7. NAMCore API Stability

### Risk Description

NeuralAmpModelerCore is actively developed. API changes could break compilation or cause runtime issues after updates.

### Historical Changes (v0.1 → v0.3)

- Namespace changes
- Method signature changes
- New model architecture support

### Mitigation Strategies

1. **Pin to specific version:**
   ```bash
   cd dep/NeuralAmpModelerCore
   git checkout v0.3.0  # Pin to release tag
   ```

2. **Wrapper layer:**
   Create a thin wrapper that isolates NAM API:
   ```cpp
   class NAMWrapper {
   public:
       bool loadModel(const std::string& path);
       void process(float* in, float* out, int n);
   private:
       std::unique_ptr<nam::DSP> model;
   };
   ```
   
   API changes only require updating the wrapper.

3. **Semantic versioning awareness:**
   - Track NAMCore releases
   - Test before updating

### Residual Risk: **LOW** (with version pinning)

---

## 8. Legal/Licensing

### Risk Description

Ensure license compatibility:
- **VCV Rack SDK:** GPL-3.0
- **This plugin:** GPL-3.0-or-later
- **NeuralAmpModelerCore:** MIT License
- **Eigen:** MPL2/BSD3/GPL (triple-licensed)

### Analysis

- MIT is compatible with GPL
- Eigen's MPL2 is compatible with GPL
- All licenses allow commercial use

### Residual Risk: **NONE** (licenses are compatible)

---

## Summary Recommendations

### Immediate Actions (Before Development)

1. ✅ Add `NeuralAmpModelerCore` as git submodule
2. ✅ Configure Eigen with `EIGEN_MAX_ALIGN_BYTES=0` initially
3. ✅ Implement async model loading from the start

### Development Best Practices

1. Pre-allocate all buffers during initialization
2. Use `enable_fast_tanh()` by default
3. Implement sample rate conversion with `libsamplerate`
4. Add CPU usage monitoring to module

### Testing Requirements

1. Test on all three platforms (Windows/macOS/Linux)
2. Test with various sample rates (44.1k, 48k, 96k, 192k)
3. Test with various block sizes
4. Stress test with multiple instances
5. Test model loading during playback (no dropouts)

### Performance Targets

| Metric | Target | Acceptable |
|--------|--------|------------|
| Single instance CPU @ 48kHz | <5% | <10% |
| Model load time | <1s | <3s |
| Processing latency | <5ms | <10ms |
| Audio dropouts during load | 0 | 0 |

---

## Feasibility Conclusion

**Overall Feasibility: HIGH**

The integration of NAM into VCV Rack 2.x is technically feasible with manageable risks. The primary concerns are:

1. **Eigen alignment** - Mitigated by disabling vectorization (with acceptable performance trade-off)
2. **CPU performance** - NAM is well-optimized; modern CPUs handle it easily
3. **Real-time safety** - NAM is designed for real-time; requires careful initialization

The NeuralAmpModelerPlugin (for DAWs) serves as a proven reference implementation, demonstrating that the core library works well in real-time audio contexts.

**Recommended approach:** Start with conservative settings (no Eigen vectorization, 128-sample blocks, async loading), get a working implementation, then optimize based on profiling data.
