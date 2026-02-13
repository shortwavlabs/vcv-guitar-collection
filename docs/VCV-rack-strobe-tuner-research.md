# VCV Rack Strobe Tuner Plugin Research

## Scope

This document summarizes practical research for building a **strobe tuner module** in **VCV Rack 2.x**, with emphasis on:

1. Rack engine/plugin constraints that affect DSP and UI design.
2. Strobe-tuner signal-processing methods and tradeoffs.
3. A recommended architecture you can implement in this repository.

## Executive Summary

A robust VCV strobe tuner should be built as four decoupled stages:

1. **Real-time audio path (per sample):** pass audio through, update lightweight state only.
2. **Pitch analysis path (decimated):** run heavier F0 estimation every N samples using a ring buffer.
3. **Musical mapping:** convert F0 to note + cents against selected reference (A4 default 440 Hz).
4. **Strobe rendering:** animate drift from phase/frequency error, updated for UI frame rate.

Best practical approach for guitar-like signals in Rack:

- Use **MPM/NSDF** or **YIN** for coarse-robust F0.
- Add **short-term smoothing + confidence gating** to suppress octave jumps.
- Drive the strobe from **differential phase/frequency error** (`f_in - f_ref`), not from raw waveform drawing.

## 1. VCV Rack Constraints That Matter

### 1.1 Real-time safety inside `process()`

From Rack plugin docs:

- `process(const ProcessArgs& args)` runs once per sample per module instance.
- In practice, `process()` must avoid allocations, locks, and blocking I/O.
- Heavy work should be rate-limited or chunked (e.g., with `ClockDivider`).

Implementation implication:

- Keep per-sample work O(1).
- Run pitch estimation at a lower control rate (e.g., every 32-128 samples).

### 1.2 Engine lifecycle and sample-rate events

Rack exposes module lifecycle hooks (`onSampleRateChange()`, `onReset()`, `onRandomize()`, etc.).

Implementation implication:

- Recompute analysis window sizes, filter coefficients, and divider periods in `onSampleRateChange()`.
- Clear ring buffers and confidence state when rate changes.

### 1.3 Port and polyphony behavior

Rack `Port` API supports polyphony and explicit channel counts.

Implementation implication:

- Decide upfront whether tuner is:
  - **Mono-only** (simplest; analyze channel 0 only), or
  - **Selectable channel** tuner for poly signals.
- Always set output channels explicitly for pass-through (`outputs[OUT].setChannels(...)`).

### 1.4 Voltage conventions for tuning workflows

Rack voltage standards:

- Pitch CV follows **1 V/oct**.
- `0 V = C4`, with `f = 261.6256 * 2^V`.
- Gate/trigger conventions and unbounded-but-safe voltage ranges are defined in docs.

Implementation implication:

- If you add CV note targeting or calibration features, follow these standards exactly.

### 1.5 Useful Rack DSP helpers

Rack DSP namespace includes reusable primitives:

- `SchmittTrigger` (edge detection with hysteresis)
- `ClockDivider` (cheap periodic scheduling)
- Slew/filter helpers

Implementation implication:

- Use `ClockDivider` to schedule F0 analysis.
- Use `SchmittTrigger` only for UI/mode triggers, not for pitch estimation.

## 2. Strobe Tuner Theory for Plugin Design

### 2.1 What a strobe tuner displays

A strobe tuner visualizes **frequency error as drift**:

- In tune: pattern appears stationary.
- Sharp/flat: pattern drifts directionally.
- Drift speed is proportional to pitch error magnitude.

This behavior is core to Peterson-style strobe descriptions and modern virtual strobes.

### 2.2 Frequency and cents mapping

Key equations:

- `cents = 1200 * log2(f_in / f_ref)`
- `f_ref(note, A4) = A4 * 2^((midi - 69)/12)`

For strobe drift, a practical control signal is:

- `delta_f = f_in - f_ref`
- `phase_error += 2*pi*delta_f / sampleRate`

Inference from strobe behavior docs + DSP practice:

- Driving animation from `delta_f` (or cents mapped to angular speed) gives stable, intuitive drift independent of waveform shape.

## 3. Pitch Detection Methods: What Works Best Here

### 3.1 YIN (difference-function family)

YIN is widely used for monophonic F0 because it reduces common autocorrelation errors via a normalized difference and thresholding.

Pros:

- Strong accuracy on periodic, harmonic signals.
- Good octave-error resistance when tuned well.

Cons:

- Heavier than naive methods.
- Needs careful confidence threshold + smoothing.

### 3.2 McLeod Pitch Method (MPM / NSDF)

MPM ("A Smarter Way to Find Pitch") uses normalized square difference + peak picking.

Pros:

- Real-time friendly.
- Good robustness/latency tradeoff for instrument tuning.

Cons:

- Can still jump on noisy/transient input unless post-filtered.

### 3.3 Why not zero-crossing or plain FFT peak

- Zero-crossing is too fragile for distorted/inharmonic guitar content.
- Plain FFT-bin peak has limited low-latency precision unless phase/refinement is added.

### 3.4 Recommended estimator strategy

Recommended for this plugin:

1. Primary F0 via **MPM or YIN** on a rolling window.
2. Confidence score from periodicity metric.
3. Median + one-pole smoothing on cents/frequency.
4. Reject/hold updates when confidence drops below threshold.

Optional advanced upgrade:

- Add pYIN-style probabilistic tracking for better note continuity on weak signals.

## 4. Recommended Rack Module Architecture

### 4.1 Signal path split

- **Audio path:** Input to output pass-through (unity gain, optional gain trim).
- **Analysis path:** Tap input and push to ring buffer for F0 processing.

### 4.2 Preconditioning for guitar signals

Before F0 estimation:

- DC blocker / high-pass (~20-40 Hz).
- Optional gentle low-pass/band-limit (~1.5-2.5 kHz) to reduce high-harmonic confusion.
- Optional soft limiter for extreme peaks.

### 4.3 Windowing and update cadence

Typical starting point at 48 kHz:

- Analysis window: 2048-4096 samples.
- Hop/update: 64 samples (~750 Hz control updates).
- Target frequency range: ~70 Hz to ~1.2 kHz (guitar + harmonics).

Tradeoff:

- Larger windows improve low-E stability, but increase latency.

### 4.4 Strobe state model

Maintain stable engine-side state:

- `estimatedHz`
- `targetHz`
- `centsError`
- `confidence`
- `phaseError`
- `inTuneLatch`

UI reads atomically (or lock-free snapshot) and renders at frame rate.

## 5. Strobe Rendering Design in Rack

### 5.1 Practical visual model

Use a procedural band/stripe pattern where phase offset is driven by pitch error.

Example:

- Pattern coordinate `u` across display width.
- Brightness `b = 0.5 + 0.5 * sin(2*pi*stripes*u + phaseError)`.
- Drift sign from cents sign; speed from magnitude.

### 5.2 UX thresholds

Suggested defaults:

- `|cents| < 0.1`: "locked" (stationary + green indicator).
- `0.1-2 cents`: slow drift (fine tune zone).
- `>2 cents`: clearly visible drift.

### 5.3 Rack widget performance

For custom draw:

- Keep draw math simple (precompute where possible).
- Cache static visuals with `FramebufferWidget` when appropriate.
- Avoid allocations in draw loop.

### 5.4 Replicating a Classic Strobe Display with Rack Widgets

The classic hardware strobe look can be recreated cleanly in Rack by splitting the display into static and dynamic layers.

Recommended widget stack:

1. `widget::FramebufferWidget` for static art:
   - bezel, note label area, tick marks, center marker, ring boundaries.
   - redraw only when theme/size/state changes (`setDirty()`).
2. `widget::Widget` or `widget::OpaqueWidget` for dynamic strobe motion:
   - draw moving stripe/ring pattern every frame in `draw()`.
   - motion is driven only by phase/drift state from the module.
3. Optional top overlay widget:
   - lock indicator, cents text, confidence meter.

Minimal integration pattern in `ModuleWidget`:

```cpp
auto staticFb = new widget::FramebufferWidget;
staticFb->box.pos = mm2px(math::Vec(3.0, 20.0));
staticFb->box.size = mm2px(math::Vec(34.0, 38.0));
staticFb->addChild(createWidget<StrobeStaticMask>(math::Vec()));
addChild(staticFb);

auto dynamic = createWidget<StrobeDynamicDisplay>(staticFb->box.pos);
dynamic->box.size = staticFb->box.size;
dynamic->module = module;
addChild(dynamic);
```

Two visual models both map well to NanoVG:

1. Linear virtual strobe (simpler):
   - horizontal or vertical bars.
   - brightness pattern: `b(x) = 0.5 + 0.5 * sin(2*pi*k*x + phase)`.
2. Circular "mechanical disk" strobe (more classic):
   - compute `theta = atan2(y-cy, x-cx)` and radius `r`.
   - draw alternating angular sectors/rings whose phase offset is animated.

Module-side phase update (audio thread) should use only drift error:

```cpp
// deltaHz = estimatedHz - targetHz
phaseCycles += deltaHz * args.sampleTime;
phaseCycles -= std::floor(phaseCycles);   // wrap to [0, 1)
renderState.phaseCycles.store(phaseCycles, std::memory_order_relaxed);
renderState.cents.store(centsError, std::memory_order_relaxed);
```

Dynamic draw pattern (UI thread) for a linear classic strip:

```cpp
void StrobeDynamicDisplay::draw(const DrawArgs& args) {
    if (!module) return;
    float p = module->renderState.phaseCycles.load(std::memory_order_relaxed);

    nvgSave(args.vg);
    nvgScissor(args.vg, 0.f, 0.f, box.size.x, box.size.y);

    const float stripePitch = 10.0f;
    const float stripeWidth = 4.0f;
    float offset = p * stripePitch;
    for (float x = -stripePitch; x < box.size.x + stripePitch; x += stripePitch) {
        nvgBeginPath(args.vg);
        nvgRect(args.vg, x + offset, 0.f, stripeWidth, box.size.y);
        nvgFillColor(args.vg, nvgRGBA(240, 245, 220, 185));
        nvgFill(args.vg);
    }

    nvgResetScissor(args.vg);
    nvgRestore(args.vg);
}
```

How to make it look "classic" instead of generic:

- Use warm phosphor-like palette (`amber/green`) with subdued background.
- Add subtle radial vignette and thin tick marks in the static layer.
- Keep stripe contrast high near center and lower near edges (simulates optics).
- Snap to visual stillness in a tight lock window (for example, `|cents| < 0.1`).

Rack-specific drawing tips:

- Use `drawLayer()` only when you truly need multiple ordered passes.
- If static artwork changes (note name, theme), call `FramebufferWidget::setDirty()`.
- Keep dynamic drawing branchless and allocation-free.
- Use atomics or lock-free snapshots between module state and display widget.

## 6. Implementation Blueprint (C++ / Rack 2)

### 6.1 Module skeleton

Core controls/features to expose:

- `A4_REF` parameter (e.g., 430-450 Hz).
- Input select/channel select if polyphony is supported.
- Optional: target mode (auto-note detect vs fixed string/note target).

### 6.2 `process()` structure

```cpp
void StrobeTuner::process(const ProcessArgs& args) {
    // 1) I/O
    float in = inputs[IN_INPUT].getVoltage();
    outputs[THRU_OUTPUT].setVoltage(in);

    // 2) Feed analysis buffer
    float x = precondition(in);
    ring.push(x);

    // 3) Decimated heavy analysis
    if (analysisDivider.process()) {
        PitchResult r = estimator.estimate(ring.data(), sampleRate);
        if (r.confidence >= minConfidence) {
            updatePitchState(r, args.sampleRate);
        }
    }

    // 4) Lights
    lights[IN_TUNE_LIGHT].setBrightnessSmooth(inTune ? 1.f : 0.f, args.sampleTime);
}
```

### 6.3 State sharing with widget

- Store render state in atomics or a lock-free snapshot struct.
- Widget reads latest state in `draw()` and computes stripe motion.

### 6.4 Persistence

Store with JSON:

- A4 reference.
- UI mode.
- Calibration offsets.

## 7. Validation Plan

### 7.1 Deterministic tests

Generate synthetic tones:

- E2/A2/D3/G3/B3/E4 and offsets at ±0.1, ±0.5, ±1, ±5, ±20 cents.
- Verify reported cents error and lock behavior.

### 7.2 Real-world tests

Test sources:

- Clean DI guitar.
- Distorted guitar.
- Polyphonic/chord input (should gracefully refuse lock for monophonic mode).

### 7.3 Metrics to track

- Lock time after note onset.
- Cents jitter (steady-state).
- Octave error rate.
- CPU usage per module instance.

## 8. Risks and Mitigations

1. **Octave jumps on harmonic-rich input**
Mitigation: confidence gating + temporal smoothing + constrained search range.

2. **Low-E instability at short windows**
Mitigation: adaptive window size or longer window at low detected F0.

3. **UI jitter despite accurate F0**
Mitigation: decouple UI animation smoothing from raw estimator output.

4. **High CPU with many module instances**
Mitigation: decimate analysis, preallocate buffers, avoid per-frame heavy redraw.

## 9. Suggested Build Order

1. Implement pass-through module + coarse cents meter (no strobe).
2. Add robust estimator (MPM/YIN) + confidence gating.
3. Add strobe renderer driven by phase/frequency error.
4. Tune thresholds and smoothing from recorded guitar clips.
5. Add optional modes (fixed-string targets, poly channel select, calibration).

## 10. Sources

### VCV Rack docs/API (primary)

- Plugin Development Tutorial: https://vcvrack.com/manual/PluginDevelopmentTutorial
- Plugin Guide: https://vcvrack.com/manual/PluginGuide
- Voltage Standards: https://vcvrack.com/manual/VoltageStandards
- `rack::engine::Module` API: https://vcvrack.com/docs-v2/structrack_1_1engine_1_1Module
- `rack::engine::Port` API: https://vcvrack.com/docs-v2/structrack_1_1engine_1_1Port
- `rack::dsp` namespace reference: https://vcvrack.com/docs-v2/namespacerack_1_1dsp
- `SchmittTrigger` API: https://vcvrack.com/docs-v2/structrack_1_1dsp_1_1TSchmittTrigger
- DSP member index (`ClockDivider::process`, `setDivision`, etc.):
  - https://vcvrack.com/docs-v2/functions_p.html
  - https://vcvrack.com/docs-v2/functions_s.html

### Strobe tuner behavior references

- Peterson: "How Strobe Tuners Work": https://www.petersontuners.com/index.cfm?Category=13
- Peterson support article (virtual strobe explanation): https://help.petersontuners.com/article/40-how-does-a-strobe-tuner-work
- Tunable app strobe explainer: https://tunableapp.com/what-is-a-strobe-tuner

### Pitch detection references

- de Cheveigne & Kawahara (YIN, JASA 2002): https://pubmed.ncbi.nlm.nih.gov/12002874/
- McLeod & Wyvill (MPM / NSDF): https://www.researchgate.net/publication/230554927_A_smarter_way_to_find_pitch
- Mauch & Dixon (pYIN, ICASSP 2014): https://code.soundsoftware.ac.uk/projects/pyin

## 11. What Is Inferred vs Directly Stated

Directly documented:

- Rack lifecycle, module/process behavior, voltage standards, and core API contracts.
- Canonical strobe behavior (stationary when in tune; drift for error).
- YIN/MPM/pYIN algorithm characteristics from original literature and project pages.

Inference made in this document:

- The specific recommended architecture (decimated analysis + phase-driven strobe renderer) is a synthesis of Rack real-time constraints plus strobe and F0 literature.
