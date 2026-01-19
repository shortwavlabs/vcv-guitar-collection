# Performance Optimization Report - SWV Guitar Collection

**Date:** 2026-01-18
**Version:** 1.0
**Status:** Initial Analysis

## Executive Summary

This report provides a comprehensive performance analysis of the SWV Guitar Collection VCV Rack plugin. The codebase demonstrates solid engineering practices with proper use of smart pointers, pre-allocated buffers, and background threading for resource loading. However, several optimization opportunities have been identified that could improve CPU efficiency, reduce latency, and enhance real-time performance.

### Key Findings
- **Hot Path Memory Allocations:** Critical issue in [NamDSP::process()](src/dsp/Nam.h#L351)
- **Mutex Contention:** Locking in audio processing threads ([NamPlayer.cpp:102](src/NamPlayer.cpp#L102), [CabSim.cpp:61](src/CabSim.cpp#L61))
- **Filter Coefficient Updates:** Recalculated every sample in [CabSimDSP::process()](src/dsp/CabSimDSP.h#L177)
- **Missing SIMD:** No vectorized operations for DSP math
- **Compiler Flags:** Link Time Optimization not enabled

### Performance Impact Summary
| Optimization | Estimated Impact | Complexity |
|-------------|------------------|------------|
| Remove hot path allocations | High | Low |
| Reduce mutex contention | Medium | Medium |
| Add SIMD vectorization | High | High |
| Enable LTO | Medium | Low |
| Cache filter coefficients | Low | Low |

---

## 1. Critical Performance Issues

### 1.1 Hot Path Memory Allocation ⚠️ CRITICAL

**Location:** [src/dsp/Nam.h:351](src/dsp/Nam.h#L351)
**Severity:** High
**Impact:** CPU spikes, potential audio glitches

```cpp
void process(const float* input, float* output, int numFrames) {
    // ...
    std::vector<float> gatedInput(numFrames);  // ⚠️ Allocated EVERY call!
    for (int i = 0; i < numFrames; i++) {
        gatedInput[i] = noiseGate.process(input[i]);
    }
    // ...
}
```

**Problem:** A `std::vector` is allocated on every call to `process()`, which runs thousands of times per second. This causes:
- Heap allocation overhead in the audio hot path
- Memory fragmentation
- Cache misses
- Potential audio dropouts

**Solution:** Pre-allocate a fixed-size buffer in the constructor:

```cpp
class NamDSP {
private:
    static constexpr int MAX_BLOCK_SIZE = 2048;
    std::vector<float> gatedInputBuffer;  // Pre-allocated in constructor

public:
    NamDSP() {
        gatedInputBuffer.resize(MAX_BLOCK_SIZE, 0.f);
        // ... other initialization
    }

    void process(const float* input, float* output, int numFrames) {
        // Use pre-allocated buffer
        for (int i = 0; i < numFrames; i++) {
            gatedInputBuffer[i] = noiseGate.process(input[i]);
        }
        // Use gatedInputBuffer.data() instead of gatedInput.data()
    }
};
```

**Expected Improvement:** 5-10% CPU reduction, reduced latency variance

---

### 1.2 Mutex Contention in Audio Thread

**Location:** [src/NamPlayer.cpp:102](src/NamPlayer.cpp#L102), [src/CabSim.cpp:61](src/CabSim.cpp#L61)
**Severity:** Medium
**Impact:** Audio thread blocking, potential xruns

```cpp
// NamPlayer.cpp:101-107
if (bufferPos >= BLOCK_SIZE) {
    std::lock_guard<std::mutex> lock(dspMutex);  // ⚠️ Blocks audio thread
    if (namDsp && namDsp->isModelLoaded()) {
        namDsp->process(inputBuffer.data(), outputBuffer.data(), BLOCK_SIZE);
    }
    bufferPos = 0;
}
```

**Problem:** The audio thread acquires a mutex on every block processing, which can block if:
- A model is being loaded in the background
- Another thread is accessing the DSP object

**Solution:** Use lock-free atomic pointer swap for DSP objects:

```cpp
class NamPlayer {
private:
    std::unique_ptr<NamDSP> namDsp;
    std::unique_ptr<NamDSP> pendingDsp;  // New model being loaded
    std::atomic<bool> hasPending{false};
    // Remove dspMutex

public:
    void process(const ProcessArgs& args) {
        // Check for pending swap
        if (hasPending.load(std::memory_order_acquire)) {
            // Swap without blocking
            std::swap(namDsp, pendingDsp);
            hasPending.store(false, std::memory_order_release);
        }

        // Process without lock
        if (namDsp && namDsp->isModelLoaded()) {
            namDsp->process(inputBuffer.data(), outputBuffer.data(), BLOCK_SIZE);
        }
    }

    void loadModel(const std::string& path) {
        loadThread = std::thread([this, path]() {
            auto newDsp = std::make_unique<NamDSP>();
            if (newDsp->loadModel(path)) {
                pendingDsp = std::move(newDsp);
                hasPending.store(true, std::memory_order_release);
            }
        });
    }
};
```

**Expected Improvement:** Reduced audio thread blocking, fewer xruns

---

### 1.3 Filter Coefficient Recalculation

**Location:** [src/dsp/CabSimDSP.h:177-200](src/dsp/CabSimDSP.h#L177)
**Severity:** Low
**Impact:** Wasted CPU cycles

```cpp
float process(float input, float blend, float lowpassFreq, float highpassFreq) {
    updateFilters(lowpassFreq, highpassFreq);  // ⚠️ Called every sample!

    // Accumulate into input buffer
    inputBuffer[bufferPos] = input;
    // ...
}
```

**Problem:** Filter coefficients are recalculated on every sample, even though the filter frequencies rarely change. The `updateFilters()` method does have change detection, but the function call overhead still occurs.

**Solution:** Move update logic to parameter change handler:

```cpp
class CabSimDSP {
private:
    float cachedLpFreq = -1.f;
    float cachedHpFreq = -1.f;

public:
    float process(float input, float blend, float lowpassFreq, float highpassFreq) {
        // Only check if changed (avoid function call overhead)
        if (lowpassFreq != cachedLpFreq) {
            updateFilters(lowpassFreq, cachedHpFreq);
            cachedLpFreq = lowpassFreq;
        }
        if (highpassFreq != cachedHpFreq) {
            updateFilters(cachedLpFreq, highpassFreq);
            cachedHpFreq = highpassFreq;
        }

        // Processing...
    }
};
```

**Alternative:** Update filters only when parameters actually change in the module's `process()` method.

**Expected Improvement:** 1-2% CPU reduction

---

## 2. Memory Optimization Opportunities

### 2.1 Large Buffer Allocations

**Location:** [src/dsp/Nam.h:263-266](src/dsp/Nam.h#L263)
**Severity:** Medium
**Impact:** High memory usage per instance

```cpp
NamDSP() {
    // Pre-allocate buffers
    resampleInBuffer.resize(MAX_BLOCK_SIZE * MAX_RESAMPLE_RATIO, 0.f);    // 32KB
    resampleOutBuffer.resize(MAX_BLOCK_SIZE * MAX_RESAMPLE_RATIO, 0.f);   // 32KB
    modelInputBuffer.resize(MAX_BLOCK_SIZE * MAX_RESAMPLE_RATIO, 0.0);    // 64KB (double)
    modelOutputBuffer.resize(MAX_BLOCK_SIZE * MAX_RESAMPLE_RATIO, 0.0);   // 64KB (double)
    // Total: ~192KB per instance
}
```

**Problem:** Each NamDSP instance allocates 192KB for buffers that are rarely used at full capacity.

**Solutions:**
1. **Reduce MAX_RESAMPLE_RATIO** from 8 to 4 (most common ratios are 1-4x)
2. **Use dynamic allocation** for large buffers only when needed
3. **Share buffers** between instances using a memory pool

**Quick Win:**
```cpp
static constexpr int MAX_RESAMPLE_RATIO = 4;  // Down from 8
// Saves ~96KB per instance
```

---

### 2.2 BiquadFilter Memory Layout

**Location:** [src/dsp/Nam.h:24-83](src/dsp/Nam.h#L24)
**Severity:** Low
**Impact:** Cache efficiency

```cpp
struct BiquadFilter {
    float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;  // Coefficients
    float z1 = 0.f, z2 = 0.f;  // State
};
```

**Problem:** State variables (z1, z2) are separated from coefficients, potentially causing cache misses during processing.

**Solution:** Use structure of arrays (SOA) or ensure contiguous memory:

```cpp
struct BiquadFilter {
    // Group state variables for better cache locality
    struct State {
        float z1 = 0.f;
        float z2 = 0.f;
    } state;

    struct Coeffs {
        float b0 = 1.f, b1 = 0.f, b2 = 0.f;
        float a1 = 0.f, a2 = 0.f;
    } coeffs;

    void process(float* in, float* out, int frames) {
        // Better cache locality
    }
};
```

---

## 3. CPU Optimization Opportunities

### 3.1 Missing SIMD Vectorization

**Location:** [src/dsp/Nam.h:75-80](src/dsp/Nam.h#L75), [src/dsp/Nam.h:224-230](src/dsp/Nam.h#L224)
**Severity:** Medium
**Impact:** 2-4x potential speedup for filter operations

```cpp
// Current: Scalar processing
float process(float in) {
    float out = b0 * in + z1;
    z1 = b1 * in - a1 * out + z2;
    z2 = b2 * in - a2 * out;
    return out;
}

// Tone stack processes 5 filters per sample
float process(float sample) {
    sample = depth.process(sample);    // Scalar
    sample = bass.process(sample);     // Scalar
    sample = middle.process(sample);   // Scalar
    sample = treble.process(sample);   // Scalar
    sample = presence.process(sample); // Scalar
    return sample;
}
```

**Problem:** All filter processing is scalar, missing SIMD optimization opportunities.

**Solution:** Implement vectorized biquad processing using SSE/AVX:

```cpp
#ifdef __SSE2__
#include <emmintrin.h>

void BiquadFilter::processBlock(float* in, float* out, int frames) {
    __m128 b0v = _mm_set1_ps(b0);
    __m128 b1v = _mm_set1_ps(b1);
    __m128 b2v = _mm_set1_ps(b2);
    __m128 a1v = _mm_set1_ps(a1);
    __m128 a2v = _mm_set1_ps(a2);

    for (int i = 0; i < frames; i += 4) {
        __m128 in = _mm_loadu_ps(&input[i]);
        __m128 z1v = _mm_set1_ps(state.z1);
        __m128 z2v = _mm_set1_ps(state.z2);

        __m128 out = _mm_mul_ps(in, b0v);
        out = _mm_add_ps(out, z1v);
        // ... full implementation

        _mm_storeu_ps(&output[i], out);

        // Update state (take last sample)
        state.z1 = output[i+3];
    }
}
#endif
```

**Expected Improvement:** 2-3x speedup for tone stack processing

---

### 3.2 Resampling Quality vs Performance

**Location:** [src/dsp/Nam.h:269-270](src/dsp/Nam.h#L269)
**Severity:** Low
**Impact:** Trade-off between quality and CPU

```cpp
NamDSP() {
    srcIn.setQuality(6);  // Speex quality 6 (0-10 scale)
    srcOut.setQuality(6);
}
```

**Problem:** Quality 6 is good for offline processing but may be overkill for real-time.

**Solutions:**
1. **Make quality configurable** via module parameter
2. **Use lower quality** (4) for real-time, higher for rendering
3. **Quality scaling** based on CPU load

**Quick Configuration:**
```cpp
// For better performance
srcIn.setQuality(4);  // ~20% faster with minimal quality loss
srcOut.setQuality(4);
```

---

## 4. Build System Optimizations

### 4.1 Link Time Optimization (LTO)

**Location:** [Makefile](Makefile)
**Severity:** Medium
**Impact:** 5-15% performance improvement

**Current:** No LTO flags
**Recommended:** Add LTO support

```makefile
# Add to Makefile after line 24
ifeq ($(shell uname -s),Darwin)
    EXTRA_FLAGS := -mmacosx-version-min=10.15 -std=c++17
    LDFLAGS += -flto
    FLAGS += -flto -O3
else
    EXTRA_FLAGS := -std=c++17
    LDFLAGS += -flto
    FLAGS += -flto -O3
endif
```

**Benefits:**
- Cross-module inlining
- Better register allocation
- Dead code elimination
- Interprocedural optimizations

**Note:** VCV Rack SDK may already set some optimizations; verify compatibility.

---

### 4.2 Architecture-Specific Optimizations

**Location:** VCV Rack SDK handles this via [dep/Rack-SDK/compile.mk](dep/Rack-SDK/compile.mk)

**Current Settings:**
```makefile
FLAGS += -O3 -funsafe-math-optimizations -fno-omit-frame-pointer
```

**Observations:**
- ✅ O3 optimization enabled
- ✅ Unsafe math optimizations (good for audio)
- ⚠️ Frame pointer preserved (slight performance cost)
- ⚠️ No `-march=native` (limited to Nehalem/ARMv8)

**Considerations:**
- `-fno-omit-frame-pointer` aids debugging but costs ~2-5% performance
- Could add `-ffast-math` for more aggressive floating-point optimizations
- `-march=native` would optimize for build machine but reduce portability

---

## 5. Algorithmic Optimizations

### 5.1 Noise Gate Envelope Follower

**Location:** [src/dsp/Nam.h:134-172](src/dsp/Nam.h#L134)
**Severity:** Low
**Impact:** Minor CPU savings

```cpp
float process(float sample) {
    float rectified = sample * sample;
    float envCoeff = rectified > envelope ? envAttack : envRelease;
    envelope += envCoeff * (rectified - envelope);

    float envDb = 10.f * std::log10(envelope + 1e-10f);  // ⚠️ Expensive!
    // ...
}
```

**Problem:** `std::log10()` is called on every sample, which is expensive.

**Solutions:**
1. **Use lookup table** for log conversion
2. **Convert threshold to linear** and avoid log domain entirely
3. **Update less frequently** (every N samples)

**Lookup Table Approach:**
```cpp
struct NoiseGate {
    static constexpr int TABLE_SIZE = 1024;
    std::array<float, TABLE_SIZE> logTable;

    NoiseGate() {
        // Pre-compute log table
        for (int i = 0; i < TABLE_SIZE; i++) {
            float x = static_cast<float>(i) / TABLE_SIZE;
            logTable[i] = 10.f * std::log10(x + 1e-10f);
        }
    }

    float process(float sample) {
        float rectified = sample * sample;
        float envCoeff = rectified > envelope ? envAttack : envRelease;
        envelope += envCoeff * (rectified - envelope);

        // Fast lookup with interpolation
        int idx = static_cast<int>(envelope * TABLE_SIZE);
        idx = std::clamp(idx, 0, TABLE_SIZE - 1);
        float envDb = logTable[idx];
        // ...
    }
};
```

---

### 5.2 Tanh Fast Path

**Location:** [src/NamPlayer.cpp:36](src/NamPlayer.cpp#L36)
**Current:** ✅ Already enabled

```cpp
// Enable fast tanh for better performance
nam::activations::Activation::enable_fast_tanh();
```

**Status:** Good! This is already implemented and provides significant speedup for neural network activation functions.

---

## 6. I/O and File Loading

### 6.1 IR Loading Performance

**Location:** [src/dsp/IRLoader.h](src/dsp/IRLoader.h)
**Severity:** Low
**Impact:** Load time, not real-time performance

**Current Implementation:**
- Synchronous file reading
- High-quality resampling (quality 8)
- Entire file loaded into memory

**Optimizations:**
1. **Memory-mapped files** for very large IRs
2. **Lower resampling quality** for file loading (use quality 4 instead of 8)
3. **Background loading** already implemented ✅

**Status:** Background loading is well-implemented. File I/O is not in the real-time path, so optimizations here have lower priority.

---

## 7. Recommended Implementation Priority

### Phase 1: Quick Wins (Low-Hanging Fruit)
1. ✅ **Remove hot path allocation** - [NamDSP::process()](src/dsp/Nam.h#L351)
   - Effort: 1 hour
   - Impact: High
   - Risk: Low

2. ✅ **Enable LTO** - [Makefile](Makefile)
   - Effort: 15 minutes
   - Impact: Medium
   - Risk: Low

3. ✅ **Reduce buffer sizes** - [NamDSP constructor](src/dsp/Nam.h#L261)
   - Effort: 5 minutes
   - Impact: Medium
   - Risk: Low

### Phase 2: Medium-Term Improvements
4. ✅ **Optimize filter coefficient updates** - [CabSimDSP](src/dsp/CabSimDSP.h#L281)
   - Effort: 2-3 hours
   - Impact: Low-Medium
   - Risk: Low

5. ✅ **Reduce mutex contention** - [NamPlayer](src/NamPlayer.cpp#L102), [CabSim](src/CabSim.cpp#L61)
   - Effort: 1 day
   - Impact: Medium
   - Risk: Medium (requires careful testing)

6. ✅ **Add SIMD vectorization** - [BiquadFilter](src/dsp/Nam.h#L75)
   - Effort: 2-3 days
   - Impact: High
   - Risk: Medium (requires SIMD fallbacks)

### Phase 3: Advanced Optimizations
7. ✅ **Implement log lookup table** - [NoiseGate](src/dsp/Nam.h#L134)
   - Effort: 1 day
   - Impact: Low
   - Risk: Low

8. ✅ **Memory pool for buffers**
   - Effort: 2-3 days
   - Impact: Medium
   - Risk: Medium

---

## 8. Performance Monitoring Recommendations

### 8.1 Add Performance Counters

**Recommended:** Add CPU monitoring to detect bottlenecks:

```cpp
class NamPlayer {
private:
    float cpuTime = 0.f;
    int blockCount = 0;

public:
    void process(const ProcessArgs& args) {
        auto start = std::chrono::high_resolution_clock::now();

        // ... existing processing ...

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> diff = end - start;
        cpuTime += diff.count();
        blockCount++;

        if (blockCount >= 100) {
            float avgCpu = cpuTime / blockCount;
            float blockDuration = BLOCK_SIZE / args.sampleRate;
            float cpuPercent = (avgCpu / blockDuration) * 100.f;

            DEBUG("Average CPU: %.2f%%", cpuPercent);

            cpuTime = 0.f;
            blockCount = 0;
        }
    }
};
```

### 8.2 Benchmark Test Suite

**Create:** `src/tests/performance_tests.cpp`

```cpp
#include "../src/dsp/Nam.h"
#include <benchmark/benchmark.h>

static void BM_NoiseGate(benchmark::State& state) {
    NoiseGate gate;
    gate.setParameters(-60.f, 0.5f, 100.f, 50.f);

    for (auto _ : state) {
        float sample = 0.1f;
        benchmark::DoNotOptimize(gate.process(sample));
    }
}
BENCHMARK(BM_NoiseGate);

static void BM_ToneStack(benchmark::State& state) {
    ToneStack tone;
    tone.setParameters(0.f, 0.f, 0.f, 0.f, 0.f);

    for (auto _ : state) {
        float sample = 0.1f;
        benchmark::DoNotOptimize(tone.process(sample));
    }
}
BENCHMARK(BM_ToneStack);
```

---

## 9. Platform-Specific Considerations

### 9.1 macOS
- ✅ Already uses `-mmacosx-version-min=10.15`
- Consider adding `-march=haswell` for SSE4.2/AVX2 support
- Profile with Instruments.app for cache analysis

### 9.2 Windows
- Ensure `/arch:AVX2` is set for modern CPUs
- Use VTune or Visual Studio profiler
- Consider SIMD intrinsics with fallbacks

### 9.3 Linux
- Use `perf` for profiling
- Check CPU affinity and NUMA effects
- Profile with `valgrind --tool=callgrind`

---

## 10. Testing and Validation

### Performance Regression Tests
1. **Baseline measurements** before any changes
2. **Automated benchmarks** for critical paths
3. **CPU usage monitoring** under load
4. **Audio quality validation** (especially for resampling changes)

### Validation Checklist
- [ ] No audio dropouts at 48kHz/96kHz
- [ ] CPU usage < 50% on mid-range CPU
- [ ] No audible artifacts from optimizations
- [ ] Sample rate conversion remains accurate
- [ ] Memory usage within expected bounds
- [ ] Thread safety maintained

---

## 11. Conclusion

The SWV Guitar Collection codebase is well-architected with good practices for audio plugin development. The identified optimizations focus on eliminating real-time processing bottlenecks while maintaining code clarity and audio quality.

**Highest Priority Items:**
1. Remove hot path memory allocation in `NamDSP::process()` - Quick win with high impact
2. Enable Link Time Optimization - Easy compiler flag change
3. Reduce buffer sizes - Simple constant change
4. Optimize mutex usage - Improves real-time reliability

**Estimated Overall Performance Gain:** 15-25% CPU reduction with Phase 1 and 2 optimizations, potentially 40-50% with SIMD implementation in Phase 3.

---

## Appendix A: File Reference Summary

| File | Lines | Key Issues |
|------|-------|------------|
| [src/dsp/Nam.h](src/dsp/Nam.h) | 351, 75-80, 224-230 | Hot path allocation, no SIMD |
| [src/dsp/CabSimDSP.h](src/dsp/CabSimDSP.h) | 177-200, 281-294 | Filter recalculation |
| [src/NamPlayer.cpp](src/NamPlayer.cpp) | 102 | Mutex contention |
| [src/CabSim.cpp](src/CabSim.cpp) | 61 | Mutex contention |
| [Makefile](Makefile) | 22-24 | Missing LTO flags |

## Appendix B: Performance Metrics Reference

**Current Performance Characteristics:**
- Block size: 128 samples (2.67ms @ 48kHz)
- Buffer allocations: ~192KB per NamDSP instance
- Processing: Double precision (NAM) ↔ Float (Rack)
- Resampling quality: 6 (Speex)
- Compiler: O3 with unsafe math optimizations

**Target Performance Goals:**
- Reduce CPU usage by 20-30%
- Eliminate real-time allocations
- Maintain < 5ms latency
- Support 96kHz operation
- Zero audio dropouts under load

---

**Document Version:** 1.0
**Next Review:** After implementation of Phase 1 optimizations
**Contact:** shortwavlabs
