# Performance Improvement Proposals

> Analysis of `nam_rack/`, `Nam.h`, `NamPlayer.hpp`, and `NamPlayer.cpp`
>
> Date: 2025-02-17

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Critical – LSTM Hot Path](#1-critical--lstm-hot-path)
3. [Critical – Matrix Multiply Loop Order](#2-critical--matrix-multiply-loop-order)
4. [High – Enable SIMD Activations by Default](#3-high--enable-simd-activations-by-default)
5. [High – Conv1x1 Bias Initialization Hoisted Out of Frame Loop](#4-high--conv1x1-bias-initialization-hoisted-out-of-frame-loop)
6. [High – NoiseGate Per-Sample log10 and Division](#5-high--noisegate-per-sample-log10-and-division)
7. [High – fast_tanh Disabled by Default](#6-high--fast_tanh-disabled-by-default)
8. [Medium – WaveNet Layer Memory Copies](#7-medium--wavenet-layer-memory-copies)
9. [Medium – Linear Model Naive Dot Product](#8-medium--linear-model-naive-dot-product)
10. [Medium – RingBuffer Column-by-Column Memcpy](#9-medium--ringbuffer-column-by-column-memcpy)
11. [Medium – ConvNet Block Copy for BatchNorm](#10-medium--convnet-block-copy-for-batchnorm)
12. [Low – NamDSP::process Buffer Resize Guards](#11-low--namdspprocess-buffer-resize-guards)
13. [Low – NamPlayer Per-Sample Processing Overhead](#12-low--namplayer-per-sample-processing-overhead)
14. [Low – MatrixPool is Unused](#13-low--matrixpool-is-unused)
15. [Low – Activation Registry String Comparisons](#14-low--activation-registry-string-comparisons)
16. [Architectural Observations](#architectural-observations)
17. [Recommended Priority Order](#recommended-priority-order)
18. [Implementation Roadmap (PR Batches)](#implementation-roadmap-pr-batches)
19. [Benchmark & Acceptance Protocol](#benchmark--acceptance-protocol)

---

## Executive Summary

The codebase is well-structured with pre-allocated buffers, column-major layout, and hand-unrolled specialisations for common channel counts in `Conv1D`/`Conv1x1`. However, several optimisation opportunities remain — ranging from simple constant-folding fixes that yield measurable wins with minimal risk, to deeper SIMD/vectorisation work that could halve CPU cost for LSTM models.

The findings are categorised as **Critical**, **High**, **Medium**, and **Low** based on expected real-time audio thread impact.

---

## 1. Critical – LSTM Hot Path

**File:** `lstm.cpp` — `LSTMCell::process()`

The LSTM is the most CPU-intensive model architecture. Every sample goes through a full matrix-vector multiply (`4H × (I+H)` where `H` is typically 20-40) plus gate activations. This is the single hottest loop in the entire plugin.

### Issues

1. **Dot-product inner loop is scalar.** The `for (int j ...)` loop over `mInputSize + mHiddenSize` performs one multiply-add per iteration. This is trivially auto-vectorisable if the compiler can prove alignment, but the current `Vector` class stores data in a `std::vector<float>` whose `.data()` is not guaranteed to be 16-byte aligned.

2. **Gate computation splits into two loops.** The matrix-vector multiply and the gate application are separate loops over `mHiddenSize`, causing an extra pass over the IFGO buffer in L1 cache.

3. **Virtual call per-sample.** `LSTM::process()` calls `processSample()` in a loop. The single-sample design means the call overhead and branch mispredictions are multiplied by `num_frames`.

### Proposed Fixes

```
Priority: ★★★ Critical
Estimated Impact: 20-40% reduction in LSTM model CPU
Risk: Medium (SIMD requires platform guards)
```

- **A) Aligned allocation.** Replace `std::vector<float>` in `Vector`/`Matrix` with a custom allocator that guarantees 16- or 32-byte alignment. This lets the compiler auto-vectorise the inner loops with `-O2 -march=native`.

- **B) Fuse gate computation.** Merge the matrix-multiply loop and the gate-activation loop: compute each row of IFGO and immediately apply sigmoid/tanh once you have the full group of 4 values for a given hidden unit. This halves the memory traffic for `mIFGO`.

- **C) Block processing.** Accumulate N samples (e.g. 4–8) of input first, then batch the matrix-vector products using a small matrix-matrix multiply. This converts the bottleneck from latency-bound to throughput-bound and exposes SIMD width better.

- **D) Explicit SIMD for the dot product.** Use Rack's `float_4` (SSE) to process 4 output rows at a time in the inner product. The `mW` matrix is already contiguous per-row in column-major form, so loading 4 consecutive row elements is a single aligned load.

---

## 2. Critical – Matrix Multiply Loop Order

**File:** `matrix.h` — `Matrix::multiply(Matrix&, const Matrix&, const Matrix&)`

```cpp
for (int k = 0; k < K; k++) {
    for (int i = 0; i < M; i++) {
        const float a_ik = a(i, k);
        for (int j = 0; j < N; j++) {
            out_col[j * M + i] += a_ik * b_col[j * K + k];
        }
    }
}
```

The innermost `j` loop strides by `K` through `b` (cache-unfriendly for large N) and by `M` through `out` (also strided). While the comment says "optimized for column-major", the access pattern is sub-optimal.

### Proposed Fix

Re-order to the classic column-major-friendly `j-k-i` loop:

```cpp
for (int j = 0; j < N; j++) {
    float* out_col_j = out.col(j);
    std::memset(out_col_j, 0, M * sizeof(float));
    for (int k = 0; k < K; k++) {
        const float b_kj = b(k, j);          // one element
        const float* a_col_k = a.col(k);     // contiguous column
        for (int i = 0; i < M; i++) {
            out_col_j[i] += a_col_k[i] * b_kj; // stride-1 on both a and out
        }
    }
}
```

This makes the innermost loop stride-1 on both `a` and `out`, which is ideal for auto-vectorisation and cache behaviour.

```
Priority: ★★★ Critical
Estimated Impact: 2-5× speedup for matrix-matrix multiply (affects ConvNet head, WaveNet rechannel)
Risk: Low
```

---

## 3. High – Enable SIMD Activations by Default

**File:** `activations.h`

SIMD activation paths (`ActivationFastTanh`, `ActivationFastSigmoid`) are guarded behind `NAM_FORCE_SIMD_ACTIVATIONS` and `NAM_USE_SIMD`, which defaults to `0`. This means even when Rack's `float_4` is available, all activations run scalar.

### Proposed Fix

- On x86_64 (which all VCV Rack desktop targets are), Rack SDK guarantees SSE2. Enable `NAM_USE_SIMD` unconditionally when `__SSE2__` is defined, or better yet, replace the define gate with a runtime-available check:

```cpp
#if defined(__SSE2__) && __has_include(<simd/functions.hpp>)
    #include <simd/functions.hpp>
    #define NAM_USE_SIMD 1
#endif
```

- For ARM (Apple Silicon), Rack SDK also provides NEON-backed `float_4`. The same include guard works.

```
Priority: ★★☆ High
Estimated Impact: 15-25% for WaveNet/ConvNet models (activation is a significant fraction of per-layer cost)
Risk: Low (Rack SDK already abstracts SIMD portably)
```

---

## 4. High – Conv1x1 Bias Initialization Hoisted Out of Frame Loop

**File:** `conv1x1.cpp` — `Conv1x1::process()`

In the specialised `in_channels == 1`, `== 2`, and `== 4` paths, the bias copy loop runs inside the per-frame loop:

```cpp
for (int f = 0; f < num_frames; f++) {
    if (_doBias) {
        for (long oc = 0; oc < out_channels; oc++) {
            out_col[oc] = _bias(oc);
        }
    } else {
        std::memset(out_col, 0, ...);
    }
    // ... multiply-add ...
}
```

The `_doBias` branch is invariant across frames.

### Proposed Fix

Hoist the branch out of the frame loop and create two separate frame loops — one for bias, one for no-bias. Better yet, pre-fill the output buffer with bias values in a single memcpy pass before the frame loop:

```cpp
if (_doBias) {
    const float* bias_data = _bias.data();
    for (int f = 0; f < num_frames; f++) {
        std::memcpy(_output.col(f), bias_data, out_channels * sizeof(float));
    }
} else {
    for (int f = 0; f < num_frames; f++) {
        std::memset(_output.col(f), 0, out_channels * sizeof(float));
    }
}
// Then accumulate weights without branching
```

This eliminates a branch per frame in the tightest inner loops.

```
Priority: ★★☆ High
Estimated Impact: 5-10% for WaveNet models (Conv1x1 is called per-layer)
Risk: Very low
```

---

## 5. High – NoiseGate Per-Sample log10 and Division

**File:** `Nam.h` — `NoiseGate::process()`

Every sample computes:
```cpp
float envDb = 10.f * std::log10(envelope + 1e-10f);
holdCounter += 1.f / static_cast<float>(sampleRate);
```

`std::log10` is expensive (~20-40 cycles). The division by `sampleRate` is invariant.

### Proposed Fixes

- **Replace dB comparison with linear comparison.** Convert the thresholds to linear power at parameter-change time:
  ```cpp
  float openThresholdLinear = std::pow(10.f, threshold / 10.f);
  float closeThresholdLinear = std::pow(10.f, (threshold - hysteresis) / 10.f);
  ```
  Then compare `envelope` directly against these, eliminating `log10` entirely from the hot path.

- **Pre-compute `1.0f / sampleRate`** as `samplePeriod` in `recalculateCoefficients()` and use a simple add:
  ```cpp
  holdCounter += samplePeriod;
  ```

```
Priority: ★★☆ High
Estimated Impact: Eliminates ~20-40 cycles per sample in the noise gate path
Risk: Very low
```

---

## 6. High – fast_tanh Disabled by Default

**File:** `NamPlayer.cpp` (constructor), `activations.h`

```cpp
nam::activations::disableFastTanh();
```

The constructor explicitly disables `fast_tanh`, forcing all "Tanh" activations through `std::tanh()` (~40 cycles per call vs ~8 for the polynomial). The comment says "for numerical stability in deep residual NAM models".

### Analysis

The `fast_tanh` approximation has max error ~0.001 in `[-5, 5]`. For typical guitar amp models, this is well within perceptual tolerance. The "instability" concern likely came from an earlier implementation. Consider:

- **Making this configurable per-model** in the context menu (like Eco Mode).
- **Using the LUT-based activation** (`ActivationLUT::createTanhLUT()`) as a middle ground — 1024-entry LUT with linear interpolation gives ~0.0001 max error and is nearly as fast as the polynomial.

```
Priority: ★★☆ High
Estimated Impact: 30-50% reduction in activation cost for WaveNet/ConvNet models using Tanh
Risk: Low (perceptual difference is negligible)
```

---

## 7. Medium – WaveNet Layer Memory Copies

**File:** `wavenet.cpp` — `Layer::process()`, `LayerArray::processInner()`

Several per-frame `memcpy` operations copy data between matrices that could potentially be avoided:

1. **Residual add** (`Layer::process`, line `next_col[c] = in_col[c] + out1x1_col[c]`) writes to `mOutputNextLayer`. If the 1x1 output could be accumulated in-place on top of the input, the separate output matrix and copy could be eliminated.

2. **Head accumulation** in `LayerArray::processInner()` loops over all layers and adds skip outputs into `mHeadInputs` with a per-frame, per-channel loop. This is fine for small channel counts but could benefit from SIMD for larger bottleneck sizes (16, 32, etc.).

3. **Final layer copy** (`LayerArray::processInner()`) copies `mLayers.back().getOutputNextLayer()` into `mLayerOutputs` via per-frame memcpy. This could be eliminated by having the layer array return a reference to the last layer's output directly.

```
Priority: ★☆☆ Medium
Estimated Impact: 5-10% for WaveNet models
Risk: Low-Medium (requires careful pointer management)
```

---

## 8. Medium – Linear Model Naive Dot Product

**File:** `linear.cpp` — `Linear::process()`

The inner loop iterates backwards through `mInputBuffer` with a bounds check (`if (idx >= 0)`):

```cpp
for (int j = 0; j < mReceptiveField; j++) {
    const long idx = input_pos - j;
    if (idx >= 0) {
        sum += mWeights[j] * mInputBuffer[idx];
    }
}
```

### Proposed Fixes

- Remove the bounds check by guaranteeing the input buffer is always large enough (it already is — `Buffer` ensures `mInputBufferOffset >= mReceptiveField` after rewind). The check is always true after initialisation.
- Use `std::inner_product` or manual SIMD to vectorise the dot product.
- For large receptive fields (>256), consider FFT-based convolution via overlap-add.

```
Priority: ★☆☆ Medium
Estimated Impact: 10-30% for Linear models (uncommon in practice)
Risk: Low
```

---

## 9. Medium – RingBuffer Column-by-Column Memcpy

**File:** `ring_buffer.cpp` — `RingBuffer::write(const Matrix&, int)`, `RingBuffer::rewind()`

Both `write()` and `rewind()` copy one column at a time via `memcpy`. When `channels` (rows) is small (1-4, common in NAM), the per-call overhead of `memcpy` dominates.

### Proposed Fix

For small channel counts, unroll or use direct assignment instead of `memcpy`. For the `channels == 1` case (most common), the entire buffer is contiguous and a single `memcpy` suffices:

```cpp
if (channels == 1) {
    std::memcpy(m_storage.col(m_writePos), input.col(0),
                num_frames * sizeof(float));
} else {
    // existing per-column loop
}
```

```
Priority: ★☆☆ Medium
Estimated Impact: Minor, but removes overhead in a frequently-called path
Risk: Very low
```

---

## 10. Medium – ConvNet Block Copy for BatchNorm

**File:** `convnet.cpp` — `ConvNetBlock::process()`

When batch normalisation is enabled, the Conv1D output is copied into a scratch buffer before applying BatchNorm in-place:

```cpp
const size_t rowBytes = static_cast<size_t>(mOutput.rows()) * sizeof(float);
for (int f = 0; f < num_frames; f++) {
    std::memcpy(mOutput.col(f), conv_output.col(f), rowBytes);
}
mBatchNormLayer.process(mOutput, 0, num_frames);
```

### Proposed Fix

Apply BatchNorm directly to `conv_output` (it's already mutable via `mConv.getOutput()`). The Conv1D output buffer is owned by the Conv1D object and is overwritten every call, so mutating it in-place is safe. This eliminates an entire matrix copy per block.

```
Priority: ★☆☆ Medium
Estimated Impact: 5-10% for ConvNet models with batchnorm
Risk: Low
```

---

## 11. Low – NamDSP::process Buffer Resize Guards

**File:** `Nam.h` — `NamDSP::process()`

Multiple runtime checks like:
```cpp
if (numFrames > static_cast<int>(gatedInputBuffer.size())) {
    gatedInputBuffer.resize(numFrames, 0.f);
}
```

These guards protect against buffer overflows but shouldn't trigger in steady-state (buffers are pre-allocated for `MAX_BLOCK_SIZE`). However, the comparisons still execute every call.

### Proposed Fix

- Use `assert()` in debug builds and remove the runtime checks in release.
- Or, pre-allocate to `MAX_BLOCK_SIZE * MAX_RESAMPLE_RATIO` in the constructor (which is already done) and enforce `numFrames <= MAX_BLOCK_SIZE` at the call site.

```
Priority: ☆☆☆ Low
Estimated Impact: Negligible (branch prediction handles this well)
Risk: Very low
```

---

## 12. Low – NamPlayer Per-Sample Processing Overhead

**File:** `NamPlayer.cpp` — `NamPlayer::process()`

The module processes one sample at a time (VCV Rack's callback model), accumulating into a 128-sample block buffer. Each call:

1. Checks 3 atomic flags with `exchange()` / `load()`.
2. Reads params and CV inputs (11 params, potentially 11 CV inputs).
3. Computes input gain.
4. Checks `bufferPos == 0` for control-rate updates.

### Proposed Fixes

- **Param reading** is already correctly rate-limited to once per block (`bufferPos == 0`). Good.
- **Atomic flag checks** could be rate-limited to once per block as well (check them only when `bufferPos == 0`), since the loading thread won't complete mid-block. This removes 3 atomic operations per sample.
- **Input gain and CV** could be cached per-block rather than computed per-sample.

```
Priority: ☆☆☆ Low
Estimated Impact: Minor (~1-2% overall)
Risk: Very low
```

---

## 13. Low – MatrixPool is Unused

**File:** `matrix.h`

`MatrixPool` is defined but never used anywhere. Its `allocate()` method doesn't actually wire up the returned matrix's internal storage to the pool buffer (the comment says "This is a hack").

### Proposed Fix

Either remove `MatrixPool` to reduce dead code, or implement it properly and use it for the temporary matrices in WaveNet layers and ConvNet blocks to reduce allocation fragmentation.

```
Priority: ☆☆☆ Low
Estimated Impact: Code hygiene only
Risk: None
```

---

## 14. Low – Activation Registry String Comparisons

**File:** `activations.h` — `Activation::get()`

The factory method uses a chain of string comparisons. This is called during model loading (not in the hot path), so performance is not critical. However, it's called once per WaveNet layer during init.

### Proposed Fix

Use an `std::unordered_map<std::string, Activation*>` (the skeleton is already there in `getRegistry()` but unused). Not a priority.

```
Priority: ☆☆☆ Low
Estimated Impact: Negligible (init-time only)
Risk: None
```

---

## Architectural Observations

### What's Already Done Well

- **Pre-allocated buffers** throughout (`Conv1D`, `Conv1x1`, `RingBuffer`, `NamDSP`) — no heap allocation in the audio thread.
- **Column-major storage** matching the dominant access pattern (per-frame column iteration).
- **Hand-unrolled specialisations** for `in_channels == 1, 2, 4` in Conv1D/Conv1x1 — these cover the most common WaveNet configurations.
- **Block processing** at the `NamPlayer` level (128-sample blocks) amortises per-block overhead.
- **Eco mode** with 2× decimation for CPU-constrained systems.
- **Idle gate** that skips NAM processing when the noise gate has been closed for several blocks.
- **Control-rate parameter updates** (once per block, not per sample).
- **ToneStack bypass** when all bands are at 0 dB.

### Suggestions for Future Consideration

1. **Compile-time model specialisation.** If a small set of model topologies covers >90% of use cases (e.g., LSTM hidden_size=20 or 40), template-specialise those sizes to enable full loop unrolling and SIMD width matching.

2. **Thread-pinned model loading.** The async load thread currently creates a full `NamDSP` including prewarm (which can take hundreds of ms). Consider setting thread priority to below-normal to avoid audio glitches on systems with few cores.

3. **Double-buffered output.** The current design outputs samples from the *previous* block while accumulating the current block. This adds 128 samples of latency. If latency matters, consider processing smaller blocks (e.g. 32 samples) at the cost of slightly higher per-block overhead.

---

## Recommended Priority Order

| # | Item | Impact | Effort | Risk |
|---|------|--------|--------|------|
| 1 | LSTM SIMD dot product (§1-D) | ★★★ | Medium | Medium |
| 2 | Matrix multiply loop order (§2) | ★★★ | Low | Low |
| 3 | Enable fast_tanh / SIMD activations (§3, §6) | ★★☆ | Low | Low |
| 4 | NoiseGate linear threshold comparison (§5) | ★★☆ | Low | Very Low |
| 5 | Conv1x1 bias hoist (§4) | ★★☆ | Low | Very Low |
| 6 | ConvNet BatchNorm in-place (§10) | ★☆☆ | Low | Low |
| 7 | WaveNet layer copy elimination (§7) | ★☆☆ | Medium | Medium |
| 8 | Linear model dot product (§8) | ★☆☆ | Low | Low |
| 9 | RingBuffer single-copy optimisation (§9) | ★☆☆ | Low | Very Low |
| 10 | NamPlayer atomic flag rate-limiting (§12) | ☆☆☆ | Low | Very Low |

---

## Implementation Roadmap (PR Batches)

This roadmap is sequenced to deliver early CPU wins with minimal risk, while deferring more invasive refactors until after measurable baseline improvements are landed.

### PR 1 — Baseline + Measurement Harness (No Behavior Changes)

**Scope**

- Add a repeatable benchmark command/script for representative models (LSTM, WaveNet, ConvNet, Linear).
- Capture and persist baseline CPU timing metrics for release builds.
- Add a short developer doc describing how to run performance measurements.

**Acceptance Criteria**

- Benchmarks run locally with one command and complete without manual setup.
- Baseline results are committed in a machine-readable format (CSV/JSON/Markdown table).
- No output/audio behavior change.

---

### PR 2 — Matrix Multiply Loop Reorder (§2)

**Scope**

- Reorder `Matrix::multiply()` to a column-major friendly loop order (`j-k-i`) with stride-1 innermost traversal.
- Preserve existing API, dimensions, and numerics.

**Acceptance Criteria**

- Functional tests pass unchanged.
- Bitwise or near-bitwise output parity with prior implementation (allowing normal floating-point reordering tolerance).
- Benchmark shows measurable speedup in matrix-heavy paths (target: significant improvement; expected range 2-5× for isolated multiply microbenchmarks).

---

### PR 3 — NoiseGate Hot-Path Constants (§5)

**Scope**

- Remove per-sample `log10` by comparing against precomputed linear thresholds.
- Precompute `samplePeriod = 1.0f / sampleRate` and eliminate per-sample division.

**Acceptance Criteria**

- Gate open/close behavior matches prior semantics within tolerance.
- No regressions in idle-gate interaction.
- Benchmark shows reduced per-sample overhead in gate-enabled scenarios.

---

### PR 4 — Conv1x1 Bias Hoist (§4)

**Scope**

- Hoist `_doBias` branch out of frame loops in `Conv1x1::process()` for specialized channel paths.
- Keep existing specialized kernels (`1/2/4` in-channels) intact.

**Acceptance Criteria**

- Output parity for `_doBias=true/false` across all specialized and generic paths.
- No additional allocations or audio-thread synchronization.
- Benchmark shows reduced branch/loop overhead in Conv1x1-heavy models.

---

### PR 5 — ConvNet BatchNorm In-Place (§10)

**Scope**

- Apply BatchNorm directly to mutable Conv1D output where safe.
- Remove intermediate block copy into scratch output matrix.

**Acceptance Criteria**

- ConvNet outputs remain numerically equivalent within floating-point tolerance.
- BatchNorm-enabled models show measurable CPU reduction.
- No aliasing/lifetime bugs under repeated block processing.

---

### PR 6 — SIMD Activations Default-On (§3)

**Scope**

- Enable SIMD activation path by default when Rack SIMD headers/types are available.
- Keep a compile-time fallback to scalar path for unsupported builds.

**Acceptance Criteria**

- Builds pass on x86_64 and Apple Silicon.
- Runtime behavior remains stable with no activation mismatch beyond expected FP tolerance.
- Benchmark shows improved activation throughput for WaveNet/ConvNet models.

---

### PR 7 — fast_tanh Runtime Policy (§6)

**Scope**

- Replace unconditional constructor-level `disableFastTanh()` with a runtime policy (default-on fast path plus opt-out).
- Expose a conservative fallback switch (global or per-model) for problematic models.

**Acceptance Criteria**

- Default path improves throughput on Tanh-heavy models.
- Audio quality checks pass defined thresholds (see protocol below).
- Users can disable fast path without restart/regression.

---

### PR 8 — LSTM SIMD Dot Product (§1-D)

**Scope**

- Implement explicit SIMD dot-product kernel for LSTM IFGO rows using Rack SIMD abstractions.
- Keep scalar fallback path and guard all SIMD code by feature macros.

**Acceptance Criteria**

- LSTM model outputs are numerically stable and free of denorm/NaN regressions.
- End-to-end CPU reduction for LSTM models reaches target band (expected 20-40% from combined LSTM optimizations).
- Code remains readable with clear scalar/SIMD separation.

---

### PR 9 — Medium/Low Follow-Ups (§7, §8, §9, §12)

**Scope**

- WaveNet copy eliminations (safe cases).
- Linear model dot-product cleanup/vectorization.
- RingBuffer small-channel copy optimization.
- NamPlayer atomic flag checks reduced to control rate.

**Acceptance Criteria**

- No correctness regressions.
- Aggregate CPU win is measurable in mixed model test set.
- Any refactors are isolated and easy to bisect/revert.

---

## Benchmark & Acceptance Protocol

### Benchmark Configuration

- Build type: Release (`-O2` or project default release flags).
- Sample rates: 44.1kHz and 48kHz.
- Block size: current production path (128) plus optional stress case (32).
- Warmup: discard first 10s of processing.
- Measurement window: at least 30s steady-state per model.
- Model set: at minimum one representative model each for LSTM, WaveNet, ConvNet, Linear.

### Reported Metrics

- Mean CPU time per audio block.
- P95 CPU time per audio block.
- Relative speedup vs baseline (`(baseline - new) / baseline`).
- Peak block time outliers count (XRuns-risk proxy).

### Audio Correctness Gates

- Compare old/new outputs on fixed test inputs (sine sweep + guitar DI snippet + silence).
- Track max absolute error and RMS error.
- For non-bit-exact changes, require error thresholds documented per PR.
- Verify no NaN/Inf generation and no denormal performance collapse.

### Rollout/Guardrails

- Land PRs in roadmap order unless a dependency requires reorder.
- Keep each optimization behind straightforward revert boundaries (small, focused commits/PRs).
- For SIMD and fast approximation changes, keep scalar-safe fallback available.

---

## Measured Results (After PR1–PR8)

### Run Context

- Script: `./run_performance_benchmarks.sh`
- Current baseline artifact: `docs/perf/perf-baseline-20260217-200058.csv`
- Previous comparable baseline: `docs/perf/perf-baseline-20260217-193555.csv`
- Model coverage currently available in `res/models/`: WaveNet only (no bundled LSTM/ConvNet/Linear yet)

### WaveNet Benchmark Delta

| Sample Rate | Previous Mean (us/block) | Current Mean (us/block) | Improvement |
|---|---:|---:|---:|
| 44.1kHz | 592.704 | 487.461 | 17.8% |
| 48kHz | 535.193 | 432.998 | 19.1% |

### P95 Delta

| Sample Rate | Previous P95 (us/block) | Current P95 (us/block) | Improvement |
|---|---:|---:|---:|
| 44.1kHz | 627.000 | 517.000 | 17.5% |
| 48kHz | 577.000 | 461.000 | 20.1% |

### Notes

- One intermediate run (`docs/perf/perf-baseline-20260217-200036.csv`) showed a transient 48kHz max-block spike (`2197us`) likely caused by scheduler/system jitter.
- A confirmation run (`docs/perf/perf-baseline-20260217-200058.csv`) did not reproduce the spike (`max 573us` at 48kHz, `outlier_blocks = 0`).
- Results indicate meaningful aggregate gains from early roadmap items (matrix, gate, conv paths, activation policy, and LSTM SIMD preparation), with remaining upside expected as non-WaveNet architectures are benchmarked.
