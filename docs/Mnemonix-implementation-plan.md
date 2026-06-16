# Mnemonix VCV Rack Module Implementation Plan

## Goal

Create `Mnemonix`, a VCV Rack v2 module that faithfully recreates the attached Deluxe Memory Man Rev_D MN3008 schematic with chorus/vibrato. Treat the schematic, not a generic "classic Memory Man" memory-delay idea, as the implementation authority.

The goal is not just "a delay with modulation." The module should preserve the Rev_D circuit personality: level-dependent drive, compander behavior, dark clock-limited repeats, BBD noise and clock artifacts, feedback that can bloom into oscillation, delay-time pitch slews, and the fixed-rate chorus/vibrato modulation path. Authenticity is preferred over cleanliness by default.

Use a topology-aware DSP model rather than a generic clean delay. Keep all DSP code independent of Rack headers, with the Rack `Module` acting only as the voltage/control wrapper.

## Source Observations

### Manual

- Unit is the XO Deluxe Memory Man reissue and is described as an exact reissue of the original circuit with true bypass added.
- Power is 24 VDC / 100 mA.
- User controls are `Level`, `Blend`, `Feedback`, `Delay`, `Depth`, `Chorus/Vibrato`, and bypass footswitch.
- `Level` should be set so the overload LED blinks on loud notes but does not stay bright; overdriving the unit intentionally causes distortion.
- `Direct Out` always carries the unmodified signal.
- `Blend` mixes direct and delayed signals, with center position intended as equal direct/delay level.
- `Feedback` creates repeats; high settings produce runaway oscillation. High feedback with short delay produces a reverb-like effect.
- Changing `Delay` while playing produces pitch-shift effects, especially with feedback.
- `Depth` controls delay-time modulation amount.
- `Chorus` is slower, `Vibrato` is faster. Vibrato is described as a Doppler/pitch-shift effect in the delayed signal.

### Schematic

Schematic identity:

- `DELUXE MEMORY MAN (MN3008 VERSION)`
- Drawing `EC2002`, `REV_D`
- Date `12-May-2005`
- New Sensor Corporation

Important circuit blocks:

- Input/direct/bypass network: `J1` input, `J2` direct out, `J3` output, relay/footswitch true-bypass section.
- Audio op amps: multiple `4558` stages (`U1`, `U2`, `U3`, `U8`, `U10`) for input gain, filtering, LFO, wet/dry mix, and output recovery.
- Compander: `U9 NE570`.
- BBD delay line: four `MN3008` chips (`U4`, `U5`, `U6`, `U7`) with multiple bias, gain, and balance trims.
- Clock: `U11 CD4047`, controlled by the `Delay` pot and modulated by the chorus/vibrato LFO.
- BBD clock annotation: `8uS (P4 @ CCW)` and `100uS (P4 @ CW)`.
- Delay pot: `P4 100K 10%`.
- Depth pot: `P5 100K`.
- Level pot: `P1 1M/LOG`.
- Blend pot: `P2 10K`.
- Feedback pot: `P3 10K/LOG`.
- Chorus/vibrato switch: `S2`; associated capacitors include `C37 0.47/50V` and `C38 2.2uF/NP`, giving the faster/slower rate split.
- Power rails include a regulated negative rail (`U12 LM7915`, schematic rail `A = -15V`) plus local bias/reference nodes. Model these as signal headroom and bias behavior, not literal Eurorack power rails.

With four MN3008 devices, the delay has 8192 BBD stages. If the schematic's `8uS` to `100uS` annotation is the CD4047 clock period, the first implementation target is:

```text
clock frequency: 125 kHz down to 10 kHz
BBD delay:       8192 / (2 * clockHz)
delay range:     about 32.8 ms to 409.6 ms
```

Treat this as the starting calibration target and verify by ear/scope if a hardware reference or recorded test set becomes available.

## Naming and Release Note

The public module name is `Mnemonix`. For legal hygiene, avoid shipping exact Electro-Harmonix branding or "Deluxe Memory Man" as the public module name unless there is explicit permission. Keep source comments and manual text framed around circuit inspiration and compatibility notes rather than implying endorsement.

Visual design should follow Shortwav Labs branding rather than imitating the original pedal enclosure. The panel can hint at BBD/analog-memory behavior, but it should sit naturally beside the existing Shortwav Labs Rack modules.

## Proposed Rack Module Surface

### Parameters

- `Level`: input drive into the modeled preamp/compander. Log taper like `P1`.
- `Blend`: dry/wet mix. Center equals nominal unity dry and wet, matching the manual.
- `Feedback`: wet feedback return amount. Log taper like `P3`; allow self-oscillation above roughly 75 percent.
- `Delay`: BBD clock/delay time. Map to the CD4047 clock period, not directly to a clean delay time.
- `Depth`: LFO modulation depth into the clock-control path, matching `P5`.
- `Chorus/Vibrato`: two-position switch matching `S2`.
- `LFO Shape`: `Triangle/Square` switch inspired by the Memory Boy modulation option.
- `Range`: `Normal/Long` switch. Normal preserves the Rev_D schematic range; Long doubles the maximum clock period for extended delay.
- `Bypass`: optional Rack button mirroring the footswitch; also call `configBypass(AUDIO_INPUT, AUDIO_OUTPUT)`.

### Trims / Context Menu

Expose circuit calibration as context-menu items or hidden trim parameters, not as primary front-panel controls:

- BBD bias trim amount.
- BBD balance / clock feedthrough cancellation.
- Compander calibration.
- Input gain trim.
- Feedback headroom trim.
- Noise/artifact amount: ideal, nominal, worn.
- Delay calibration: schematic clock range vs extended range.

### Inputs

- `Audio In`.
- `Level CV` with attenuverter.
- `Delay CV` with attenuverter.
- `Feedback CV` with attenuverter.
- `Blend CV` with attenuverter.
- `Depth CV` with attenuverter.
- `Chorus/Vibrato Gate/CV` input for switching modes.
- `LFO Shape Gate/CV` input for triangle/square modulation.
- `Range Gate/CV` input for normal/long delay range.
- `Bypass Gate/CV`.

### Outputs

- `Audio Out`: final pedal output.
- `Direct Out`: unmodified direct signal, matching `J2`.
- `Wet Out`: useful in Rack patches while still keeping the normal output faithful.
- `Level CV Out`: final post-CV level control value.
- `Blend CV Out`: final post-CV blend control value.
- `Feedback CV Out`: final post-CV feedback control value.
- `Delay CV Out`: final post-CV delay/clock control value.
- `Depth CV Out`: final post-CV modulation-depth control value.
- `Chorus/Vibrato Gate Out`: low for chorus, high for vibrato.
- `LFO Out`: bipolar `-5V..+5V` LFO waveform using the selected triangle or square shape.
- `Range Gate Out`: low for normal range, high for long range.
- `Bypass Gate Out`: low for bypassed, high for engaged.

Control outputs should make the module patchable as a modulation source without changing the faithful audio path. Use `0-10V` for continuous user controls and gate-style outputs for switch states.

### Lights

- `Overload`: driven from the modeled input/preamp level, calibrated so it flickers on loud peaks.
- `Status`: on when effect is engaged.
- Optional subtle `Clock/Mod` indicator if it does not clutter the panel.

## Circuit-to-DSP Mapping

| Schematic area | Circuit role | DSP plan |
| --- | --- | --- |
| `J1`, `J2`, `J3`, `K1`, `S1` | Input, direct out, true bypass/output switching | Rack input/output wrapper, `configBypass`, direct output path always available |
| `P1`, `U1A`, `U2B`, nearby RC network | Input gain, impedance, pre-emphasis/color, overload region | Log input gain, highpass/lowpass tone shaping, 4558-style soft saturation, overload detector |
| `U9 NE570` | Companding around the noisy BBD path | Envelope-controlled compressor before BBD and expander after BBD; model breathing and recovery timing |
| `U2A` and RC network before `U4` | BBD drive filtering and bias | Anti-alias/pre-emphasis active filters, BBD bias offset, drive trim |
| `U4`-`U7 MN3008` | Four-chip bucket-brigade delay line | 8192-stage BBD model with variable clock, sample/hold stepping, charge loss, saturation, clock feedthrough, delay-time pitch slews |
| `U11 CD4047`, `P4`, `C40`, `C41`, `C42`, `R69` | Two-phase clock generator | Clock-period model from schematic range; modulate clock frequency from LFO rather than modulating a clean delay read index |
| `U10A`, `U10B`, `P5`, `S2`, `C37`, `C38` | Chorus/vibrato LFO | Fixed-rate analog LFO model; chorus uses slower capacitor path, vibrato faster. Depth scales clock modulation |
| `U8`, `U3`, post-BBD RC networks | Reconstruction filtering, gain recovery, de-emphasis | Cascaded active lowpass/highpass filters with component-derived cutoff targets and 4558 saturation |
| `P3` feedback network | Repeat feedback and self-oscillation | Feed post-reconstruction wet signal back before/around compander input; include nonlinear limiting and tone loss per repeat |
| `P2`, output stages | Wet/dry blend and final output | Equal-power or calibrated linear dry/wet blend, output recovery coloration, final finite guard |

## DSP Architecture

Create these files:

- `src/Mnemonix.hpp`
- `src/Mnemonix.cpp`
- `src/dsp/MnemonixDSP.h`
- `src/dsp/BbdDelayLine.h`
- `src/dsp/Ne570Compander.h`
- `src/dsp/AnalogFilterBank.h`
- `src/dsp/AnalogLfo.h`
- `src/dsp/OpAmpStage.h`
- `src/tests/test_memory_mna_deluxe.cpp`

Keep `src/dsp/*` free of `rack.hpp`. Use plain C++ types and pass normalized audio samples in the range approximately `-1.0 .. +1.0`.

### Per-Channel State

Support Rack polyphony by storing one DSP engine per channel:

```cpp
MnemonixDSP engines[16];
```

The module wrapper should:

1. Determine the input channel count.
2. Convert Rack audio volts to normalized audio with `/ 5.f`.
3. Combine knobs and CV with clamping/smoothing.
4. Call the per-channel DSP engine.
5. Convert normalized output back to Rack voltage with `* 5.f`.
6. Guard outputs against NaN/Inf.

### Processing Flow

```text
input
  -> direct tap
  -> level/preamp RC filters
  -> 4558 input saturation and overload detector
  -> dry path to blend
  -> NE570 compressor
  -> pre-BBD anti-alias / pre-emphasis filters
  -> BBD delay line, clocked by CD4047 + LFO
  -> post-BBD reconstruction / de-emphasis filters
  -> NE570 expander
  -> feedback tap, with nonlinear feedback limiting
  -> wet/dry blend
  -> output stage coloration
  -> output
```

## Modeling Details

### Delay and Clock

- Model `Delay` as a CD4047 clock-period control.
- Start with clock period range `8 us .. 100 us`.
- Convert to BBD delay by `8192 / (2 * clockHz)`.
- `Range: Long` doubles the maximum clock period to `200 us`, extending the maximum nominal delay from about `409.6 ms` to about `819.2 ms`.
- Smooth the clock-control voltage but do not smooth away audible pitch slews; delay-time changes should glide and pitch-shift.
- Add a context-menu calibration mode if later tests show the physical unit reaches a wider maximum delay.

### BBD Behavior

Implement the BBD line in stages:

1. Baseline variable delay line with high-quality interpolation for stability.
2. Clocked sample/hold behavior so high delay settings sound grainier and darker.
3. Per-stage or per-chip charge loss approximation: tiny one-pole droop and level loss per pass.
4. BBD input/output saturation around the bias point.
5. Clock feedthrough/noise sidebands, reduced by balance trim and post filters.
6. Delay-dependent hiss that increases as clock frequency drops.

Do not make the BBD path full-bandwidth. Its bandwidth should collapse as the clock slows, with the post-filter bank doing much of the audible darkening.

### Compander

The NE570 is essential to the pedal feel. Implement a simplified but musical model:

- Full-wave rectifier envelope follower.
- Attack/release coefficients derived from schematic RC neighborhoods where practical; otherwise start with fast attack and medium release, then calibrate.
- Compressor before the BBD that reduces BBD overload and noise.
- Expander after the BBD that restores dynamics and creates subtle breathing.
- Saturate the sidechain/gain computer so extreme feedback does not behave like a sterile digital gain block.

### Op Amp and Headroom

Use light 4558-style nonlinear stages at the same places the circuit has gain recovery:

- Input stage.
- BBD drive stage.
- Reconstruction/recovery stage.
- Output stage.
- Feedback return.

Start with a cheap asymmetric `tanh`/soft clip model with bias offset and frequency-dependent drive. If profiling later shows this is too generic, replace with a small waveshaper table or a calibrated polynomial.

### Filters

Use component-derived active filter approximations rather than generic tone controls:

- Convert each major RC/4558 filter cluster into first-order or biquad sections.
- Use bilinear transform coefficients and recalculate only when sample rate or clock-tracked cutoff changes.
- Group filters into named stages: `inputColor`, `preBbd`, `postBbdA`, `postBbdB`, `outputColor`.
- Add denormal prevention in filter states.

The post-BBD filters should be the main reason repeats lose top end. The clock-tracked bandwidth should become especially obvious at long delay settings.

### Chorus/Vibrato

- The LFO is fixed-rate with depth control, not a fully user-variable modulation oscillator.
- LFO shape is switchable between triangle and square. Triangle is the default pedal-style sweep; square intentionally creates stepped clock changes like the Memory Boy option.
- Use two rate modes matching the switch:
  - `Chorus`: slow mode, start around `0.3-0.4 Hz`.
  - `Vibrato`: faster mode, start around `1.4-1.8 Hz`.
- Preserve the schematic capacitor ratio (`2.2uF / 0.47uF`, about 4.7x) when setting the initial rates.
- Modulate the BBD clock period/frequency, not the final delay output directly.
- Blend determines whether the result is chorus-like dry+wet or vibrato-like mostly wet, matching how the pedal is used.

### Feedback

- Feed back the filtered/expanded wet signal at the same conceptual point as the circuit, before BBD drive coloration.
- Let self-oscillation happen.
- Protect the DSP with internal soft saturation and finite guards, not hard clipping that kills the analog runaway.
- Feedback tone should darken and smear with every repeat.

### Noise and Imperfections

Add optional artifact layers after the clean path is stable:

- BBD hiss, increasing with longer delay/lower clock.
- Low-level clock whine at the current clock and related components, filtered by the reconstruction bank.
- Compander pumping/breathing.
- Slight LFO asymmetry and rate drift.
- Tiny channel-to-channel trim variation for polyphonic channels.

The default artifact profile should be authentically noisy and alive rather than sanitized. The context menu may offer cleaner or more worn calibration profiles, but the default should feel like a well-calibrated analog unit.

## Calibration Sources

- Primary calibration source: attached Rev_D MN3008 schematic and manual.
- Secondary sanity reference: available smaller Memory Boy unit. Use it only for broad EHX analog-delay family behaviors such as compander feel, overload response, delay-knob pitch slews, and modulation texture.
- Do not tune away from the Rev_D schematic just to match the Memory Boy; it is a useful feel reference, not the target circuit.

## Implementation Phases

### Phase 1: Module Skeleton

- Add module declaration in `plugin.hpp`.
- Register model in `plugin.cpp`.
- Add `plugin.json` module entry.
- Create `Mnemonix` Rack wrapper and Shortwav Labs-branded panel SVG.
- Add controls, per-control CV inputs, per-control CV outputs, lights, context menu, serialization for calibration/artifact mode.
- Add bypass and direct-out routing.

### Phase 2: Clean Circuit Topology

- Implement `MnemonixDSP` with normalized audio processing.
- Add level, blend, delay, feedback, depth, and mode parameters.
- Build the delay-time mapping from the CD4047/MN3008 calculation.
- Implement clean feedback delay with delay-time pitch slews.
- Add basic input/output gain staging and overload LED calculation.

### Phase 3: Filters and Tone Loss

- Implement component-inspired input, pre-BBD, post-BBD, and output filter banks.
- Make BBD bandwidth track clock frequency.
- Validate impulse and frequency responses at short, middle, and long delay settings.
- Tune repeat darkness before adding heavy nonlinear artifacts.

### Phase 4: NE570 Compander

- Add pre-BBD compressor and post-BBD expander.
- Tune sidechain attack/release and gain law.
- Validate that low-level repeats stay audible while long-delay noise remains pedal-like.

### Phase 5: BBD Artifacts

- Add clocked sample/hold behavior.
- Add charge loss/droop, BBD headroom, and bias trim.
- Add delay-dependent noise and clock feedthrough.
- Add balance trim to reduce clock feedthrough.

### Phase 6: Chorus/Vibrato

- Implement analog LFO with `Chorus` and `Vibrato` rate modes.
- Modulate clock control.
- Tune depth range so high depth produces recognizable warble without breaking delay stability.
- Verify manual behaviors: rich chorus at short delay/center blend/high feedback/high depth; vibrato pitch shift on delayed signal.

### Phase 7: Calibration and Presets

- Create presets:
  - Short slap.
  - Reverb-like short feedback.
  - Classic chorus delay.
  - Vibrato wet.
  - Runaway oscillation.
  - Long dark repeats.
- Tune the default state to a sane pedal-like starting point.
- Make the default calibration authentic/noisy rather than clean.
- Add manual docs for Rack voltage/CV behavior.

### Phase 8: Tests and Performance

- Add standalone DSP tests under `src/tests`.
- Run at 44.1, 48, 96, and 192 kHz.
- Test all parameter extremes and modulation extremes.
- Confirm no allocations in `process()`.
- Benchmark with 1, 4, 8, and 16 polyphonic channels.

## Test Plan

Unit tests:

- Delay mapping: `Delay` min/max produces expected clock period and approximate delay time.
- Delay-time changes remain finite and create continuous output.
- Bypass returns input to output.
- Direct out equals input regardless of effect state.
- Each user control has a working CV input and corresponding control output.
- Feedback remains finite during self-oscillation.
- Filter coefficients are finite across all sample rates.
- Compander does not produce NaN/Inf for silence, DC, impulses, or extreme input.
- Polyphony keeps channel states independent.

Audio characterization:

- Impulse response at short/mid/long delay.
- Frequency response through wet path at short/mid/long clock settings.
- Noise floor versus delay time.
- Overload LED threshold versus input level.
- Feedback decay and onset of oscillation.
- Chorus/vibrato LFO rate and depth.
- Pitch slew when moving `Delay` while audio and feedback are active.

Manual verification patches:

- Blend center gives roughly equal dry and wet.
- Direct out is always dry.
- High feedback with short delay creates reverb-like smear.
- High feedback can run away.
- Chorus mode is slower than vibrato mode.
- Vibrato mode becomes obvious on mostly wet output.

## Risks and Open Questions

- The schematic is detailed, but exact active-filter transfer functions should be derived carefully from the component network before final tuning.
- The manual does not publish exact delay range or modulation rates; the schematic clock annotation gives a strong initial target, but final values should be calibrated against hardware recordings if possible.
- The available Memory Boy is not the target circuit; use it only to avoid obviously un-EHX-like behavior.
- A literal component-level circuit solver would be expensive and unnecessary for Rack polyphony. A topology-aware block model is the recommended first implementation.
- The public module name should avoid protected brand/product names unless permission is obtained.
- Authentic BBD clock artifacts can become annoying quickly; expose artifact amount/calibration in the context menu, but keep the default closer to a real calibrated analog unit than a clean digital delay.

## Definition of Done

- The module builds in the existing Rack plugin.
- DSP code is isolated from Rack SDK headers.
- Short and long delay settings have clearly different bandwidth/noise behavior.
- Delay knob movement produces analog-style pitch slews.
- Feedback can self-oscillate musically without numerical failure.
- Chorus/vibrato switch changes modulation rate and feel.
- Overload and status LEDs match the manual's operating guidance.
- Direct out remains dry.
- Every front-panel user control has CV input and control-voltage output support.
- The panel uses Shortwav Labs branding.
- Tests pass at common Rack sample rates and with polyphonic input.
