# Overdrive.cpp Performance Analysis

## Executive Summary

The `Overdrive.cpp` module processes audio samples through a DSP pipeline with oversampling. Several optimization opportunities exist that could significantly improve CPU usage, particularly at high sample rates and with multiple instances.

## Current Architecture

The module follows this processing flow:
1. Read parameters (potentially with CV modulation)
2. Apply smoothing to parameter changes
3. Update DSP engine parameters
4. Process audio through oversampled DSP chain
5. Update gate lights
6. Output audio

## Identified Performance Issues

### 1. Redundant Model Switching (High Impact)

**Location**: `Overdrive::process()`, line 35-37

```cpp
int modelInt = static_cast<int>(std::round(params[MODEL_PARAM].getValue()));
if (modelInt < 0) modelInt = 0;
if (modelInt > 2) modelInt = 2;
dsp.setModel(static_cast<OverdriveModel>(modelInt));
```

**Issue**: `dsp.setModel()` is called on every sample even when the model hasn't changed. While the implementation appears lightweight (just sets a variable), the parameter reading and clamping overhead is unnecessary.

**Impact**: Redundant parameter reads, integer conversion, and bounds checking every sample (~44,100-192,000 times/second).

**Recommendation**: Cache the current model value and only update when changed:
```cpp
int modelInt = static_cast<int>(std::round(params[MODEL_PARAM].getValue()));
if (modelInt < 0) modelInt = 0;
if (modelInt > 2) modelInt = 2;
if (modelInt != cachedModel) {
    dsp.setModel(static_cast<OverdriveModel>(modelInt));
    cachedModel = modelInt;
}
```

**Expected Improvement**: Minor (2-5% CPU savings)

---

### 2. Repeated Parameter Reading Pattern (Medium Impact)

**Location**: `Overdrive::process()`, lines 40-77

**Issue**: Each parameter is read, CV-modulated, and smoothed using identical code patterns. This creates:
- Redundant function calls (`params[...].getValue()`, `inputs[...].isConnected()`)
- Repeated CV rescaling calculations with constant ranges
- Multiple smoother operations

**Impact**: Code verbosity and minor function call overhead per parameter.

**Recommendation**: Consider parameter caching or use Rack's built-in parameter modulation system more efficiently. For critical parameters, cache values and only update on change.

**Expected Improvement**: Minor (1-3% CPU savings)

---

### 3. Light Updates Every Sample (Low Impact)

**Location**: `Overdrive::process()`, lines 91-100

```cpp
if (bypassed) {
    lights[GATE_GREEN_LIGHT].setBrightness(0.f);
    lights[GATE_RED_LIGHT].setBrightness(0.f);
} else if (dsp.isGateOpen()) {
    lights[GATE_GREEN_LIGHT].setBrightness(1.f);
    lights[GATE_RED_LIGHT].setBrightness(0.f);
} else {
    lights[GATE_GREEN_LIGHT].setBrightness(0.f);
    lights[GATE_RED_LIGHT].setBrightness(1.f);
}
```

**Issue**: Lights are updated every sample (44k-192k times/sec) but only need to change when the gate state changes.

**Impact**: Redundant light writes; gate state queries from DSP.

**Recommendation**: Track gate state and only update lights on change:
```cpp
bool gateOpen = dsp.isGateOpen();
if (gateOpen != cachedGateOpen || bypassed != cachedBypassed) {
    if (bypassed) {
        lights[GATE_GREEN_LIGHT].setBrightness(0.f);
        lights[GATE_RED_LIGHT].setBrightness(0.f);
    } else if (gateOpen) {
        lights[GATE_GREEN_LIGHT].setBrightness(1.f);
        lights[GATE_RED_LIGHT].setBrightness(0.f);
    } else {
        lights[GATE_GREEN_LIGHT].setBrightness(0.f);
        lights[GATE_RED_LIGHT].setBrightness(1.f);
    }
    cachedGateOpen = gateOpen;
    cachedBypassed = bypassed;
}
```

**Expected Improvement**: Negligible for CPU, but reduces light flicker and updates.

---

### 4. Integer Conversion Overhead (Low Impact)

**Location**: `Overdrive::process()`, lines 35, 62-68

**Issue**: `std::round()` is called every sample for model and attack parameters, even when CV inputs aren't connected.

**Impact**: Unnecessary floating-point to integer conversion overhead.

**Recommendation**: Cache integer parameter values when CV inputs are not connected:
```cpp
static int cachedAttack = -1;
int attack = static_cast<int>(std::round(params[ATTACK_PARAM].getValue()));
if (inputs[ATTACK_CV].isConnected()) {
    // ... CV processing ...
} else if (attack != cachedAttack) {
    dsp.setAttack(attack);
    cachedAttack = attack;
}
```

**Expected Improvement**: Very minor (<1% CPU savings)

---

### 5. Rescale Function Calls (Low Impact)

**Location**: Multiple locations throughout `process()`

**Issue**: `rescale()` is called with constant ranges (`-5.f, 5.f, 0.f, 1.f`) for CV processing. This involves:
- Division by range span (10.0)
- Multiplication and addition operations

**Impact**: Redundant arithmetic operations when CV is at the same value or when smoothing is active.

**Recommendation**: Precompute or inline the rescale operation for common ranges:
```cpp
// Precomputed: 1.0f / (5.0f - (-5.0f)) = 0.1f
constexpr float cvToParamScale = 0.1f;
float cv = inputs[DRIVE_CV].getVoltage();
drive = (cv + 5.0f) * cvToParamScale;
```

**Expected Improvement**: Very minor (<1% CPU savings)

---

### 6. DSP Hot Path Optimization (Medium-High Impact)

**Location**: `OverdriveDSP::process()`, lines 67-95

**Issue**: The switch statement is inside the oversampling loop:

```cpp
for (int i = 0; i < Oversampler::kFactor; ++i) {
    float sample = noiseGate.process(upsampled[i]);
    switch (currentModel) {
        case OverdriveModel::TS808:
        case OverdriveModel::TS9:
            // ... processing ...
            break;
        case OverdriveModel::DS1:
            // ... processing ...
            break;
    }
    processed[i] = sample;
}
```

This means the model comparison happens `kFactor` times per sample.

**Impact**: Branch prediction overhead inside inner loop; with 4x or 8x oversampling, this is significant.

**Recommendation**: Move the switch statement outside the loop:

```cpp
void process(float input) {
    float upsampled[Oversampler::kFactor];
    float processed[Oversampler::kFactor];

    oversampler.upsample(input, upsampled);

    switch (currentModel) {
        case OverdriveModel::TS808:
        case OverdriveModel::TS9:
            for (int i = 0; i < Oversampler::kFactor; ++i) {
                float sample = noiseGate.process(upsampled[i]);
                sample = inputBuffer.process(sample);
                sample = softClipper.process(sample);
                sample = tsTone.process(sample);
                sample = outputBuffer.process(sample);
                processed[i] = sample;
            }
            break;
        case OverdriveModel::DS1:
            for (int i = 0; i < Oversampler::kFactor; ++i) {
                float sample = noiseGate.process(upsampled[i]);
                sample = inputBuffer.process(sample);
                sample = transistorBooster.process(sample);
                sample = hardClipper.process(sample);
                sample = dsTone.process(sample);
                sample = outputBuffer.process(sample);
                processed[i] = sample;
            }
            break;
    }

    return oversampler.downsample(processed) * outputLevel;
}
```

**Expected Improvement**: Moderate (5-10% CPU savings due to better branch prediction)

---

### 7. Parameter Smoothing Overhead (Medium Impact)

**Location**: `Overdrive::process()`, lines 43, 50, 57, 77

**Issue**: Four separate `ExponentialFilter` instances are processing every sample, even when parameters aren't changing.

**Impact**: Each smoother performs multiplications and additions per sample.

**Recommendation**: Only run smoothers when values actually change, or use Rack's `ParamQuantity` smoothing system more efficiently. Consider merging smoothers into a single update loop.

**Expected Improvement**: Minor (1-2% CPU savings)

---

### 8. Inefficient Attack Parameter Processing (Low-Medium Impact)

**Location**: `Overdrive::process()`, lines 62-71

**Issue**: Attack parameter processing involves clamping and rescaling even when CV is not connected.

```cpp
int attack = static_cast<int>(std::round(params[ATTACK_PARAM].getValue()));
if (inputs[ATTACK_CV].isConnected()) {
    float cv = std::clamp(inputs[ATTACK_CV].getVoltage(), 0.f, 10.f);
    int cvPos = static_cast<int>(std::round(rescale(cv, 0.f, 10.f, 0.f, 5.f)));
    attack = std::clamp(attack + cvPos, 0, 5);
}
dsp.setAttack(attack);
```

**Impact**: `dsp.setAttack()` is called every sample regardless of whether the attack value changed.

**Recommendation**: Cache the attack value and only update when changed:
```cpp
int attack = static_cast<int>(std::round(params[ATTACK_PARAM].getValue()));
if (inputs[ATTACK_CV].isConnected()) {
    float cv = std::clamp(inputs[ATTACK_CV].getVoltage(), 0.f, 10.f);
    int cvPos = static_cast<int>(std::round(rescale(cv, 0.f, 10.f, 0.f, 5.f)));
    attack = std::clamp(attack + cvPos, 0, 5);
}
if (attack != cachedAttack) {
    dsp.setAttack(attack);
    cachedAttack = attack;
}
```

**Expected Improvement**: Minor (1-2% CPU savings)

---

## Optimization Priority Matrix

| Priority | Issue | Impact | Effort | Overall |
|----------|-------|--------|--------|---------|
| **High** | Switch statement in oversampling loop | Medium-High | Low | **★★★★★** |
| **Medium** | Redundant model switching | High | Low | **★★★★☆** |
| **Medium** | Attack parameter caching | Low-Medium | Low | **★★★☆☆** |
| **Low** | Light updates every sample | Low | Low | **★★☆☆☆** |
| **Low** | Parameter reading pattern | Minor | Medium | **★★☆☆☆** |
| **Low** | Integer conversion overhead | Very minor | Low | **★☆☆☆☆** |
| **Low** | Rescale function calls | Very minor | Low | **★☆☆☆☆** |
| **Low** | Smoother overhead | Minor | Medium | **★★☆☆☆** |

---

## Recommended Implementation Plan

### Phase 1: Quick Wins (High ROI)
1. Move switch statement outside oversampling loop in `OverdriveDSP::process()`
2. Cache model parameter in `Overdrive::process()`
3. Cache attack parameter in `Overdrive::process()`

**Expected CPU Savings**: 8-15%

### Phase 2: Light Optimization
4. Implement light update caching
5. Optimize integer conversions

**Expected CPU Savings**: 1-3% additional

### Phase 3: Code Quality Improvements
6. Refactor CV processing pattern
7. Consider smoother optimization strategies

**Expected CPU Savings**: 1-2% additional

---

## Benchmarking Recommendations

To measure the impact of these optimizations:

1. Use Rack's built-in CPU meter
2. Test at multiple sample rates (44.1kHz, 48kHz, 96kHz, 192kHz)
3. Test with:
   - No CV inputs connected (baseline)
   - All CV inputs modulated
   - Fast parameter changes
4. Measure with multiple instances (1, 4, 8, 16 modules)
5. Profile using instruments like `perf` on Linux or `Instruments` on macOS

---

## Conclusion

The `Overdrive.cpp` module is reasonably well-optimized but has several opportunities for improvement. The most impactful change is restructuring the DSP processing loop to move the model switch outside the oversampling loop, which should provide 5-10% CPU savings alone. Combined with parameter caching for model and attack parameters, we expect 8-15% total CPU reduction with minimal code changes.

Further optimizations in light updates and parameter processing patterns provide diminishing returns but may be worth implementing for code clarity and maintenance benefits.

**Overall Recommendation**: Implement Phase 1 optimizations first for maximum ROI. Phase 2 and 3 can be considered if performance needs exceed current capabilities.
