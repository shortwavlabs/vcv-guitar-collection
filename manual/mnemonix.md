# Mnemonix Guide

Mnemonix is Shortwav Labs' Rev_D MN3008-inspired BBD memory delay for VCV Rack. It is designed to feel like an analog pedal rather than a clean digital delay: repeats darken as delay time increases, feedback can bloom into self-oscillation, delay-time changes pitch-slew, and the chorus/vibrato circuit modulates the BBD clock.

Use Mnemonix anywhere you would patch a delay pedal, chorus, vibrato, dub feedback send, or clocky lo-fi memory effect.

## Signal Flow

Typical guitar rig:

```text
Guitar -> NAM Player -> Cabinet Simulator -> Mnemonix -> Output
```

Pedal-style front-of-amp patch:

```text
Guitar -> Mnemonix -> NAM Player -> Cabinet Simulator -> Output
```

Studio send patch:

```text
Dry guitar bus -> Mixer
              -> Mnemonix 100% wet -> Mixer
```

Putting Mnemonix after Cabinet Simulator gives a polished rack-delay feel. Putting it before NAM Player makes the repeats hit the amp model, which is closer to a pedalboard workflow.

## Main Controls

| Control | What it does |
| --- | --- |
| `Level` | Input drive into the modeled preamp and compander. Higher values add analog-style saturation and can push the delay harder. |
| `Blend` | Dry/wet mix. The sweep is level-compensated so the output does not collapse at dry settings. |
| `Feedback` | Repeat regeneration. High settings can self-oscillate, especially with short delay times. |
| `Delay` | BBD clock period, displayed in milliseconds. Moving it while audio is passing produces pitch bends. |
| `Depth` | Amount of chorus/vibrato modulation applied to the clock. |
| `Chorus/Vibrato` | Chorus is slower and subtler; vibrato is faster and more pitch-forward. |
| `LFO shape` | Triangle or square modulation. Square gives the Memory Boy-style jumpy modulation option. |
| `Delay range` | Normal preserves the Rev_D-style range; Long doubles the maximum delay time. |
| `Effect` | Engages or bypasses the effect. |

Normal range is approximately `32.8 ms` to `409.6 ms`. Long range extends the maximum to approximately `819.2 ms`.

## CV Inputs

Mnemonix provides CV inputs and attenuverters for the five continuous controls:

| CV input | Attenuverter | Behavior |
| --- | --- | --- |
| `Level CV` | `Level CV amount` | Modulates input drive. |
| `Blend CV` | `Blend CV amount` | Modulates dry/wet mix. |
| `Feedback CV` | `Feedback CV amount` | Modulates repeat regeneration. |
| `Delay CV` | `Delay CV amount` | Modulates the BBD clock/delay time. |
| `Depth CV` | `Depth CV amount` | Modulates chorus/vibrato depth. |

The switch inputs are gate-style controls:

| Input | Low | High |
| --- | --- | --- |
| `Chorus/Vibrato Gate` | Chorus | Vibrato |
| `LFO Shape Gate` | Triangle | Square |
| `Long Range Gate` | Normal range | Long range |
| `Engage Gate` | Bypassed | Engaged |
| `Tap Tempo Gate` | No tap | Rising edges set tap tempo |

Use small attenuverter amounts for `Delay CV` if you want musical pitch sweeps. Full-range delay modulation is intentionally dramatic.

## Outputs

| Output | Use |
| --- | --- |
| `Audio` | Main final output. |
| `Direct` | Unmodified direct signal, useful for parallel patches. |
| `Wet` | Wet delay path only. |
| `Level CV`, `Blend CV`, `Feedback CV`, `Delay CV`, `Depth CV` | Final post-CV control values as `0-10V`. |
| `Chorus/Vibrato Gate` | `0V` for chorus, `10V` for vibrato. |
| `LFO` | Bipolar selected LFO waveform, approximately `-5V..+5V`. |
| `Long Range Gate` | `0V` normal, `10V` long. |
| `Engage Gate` | `0V` bypassed, `10V` engaged. |
| `BBD clock / 64 gate` | Fast clock-derived timing utility. |
| `BBD clock / 512 gate` | Slower clock-derived timing utility. |
| `Compander envelope CV` | `0-10V` envelope from the modeled compander. |

The utility outputs make Mnemonix useful as a modulation source even when the audio path is patched traditionally.

## Context Menu

Right-click Mnemonix for deeper options.

### Artifact Profile

- `Cleaner BBD`: Lower noise and clock artifacts.
- `Rev_D authentic`: Default behavior.
- `Worn unit`: More noise, drift, and character.

### Bypass Behavior

- `Trails`: Bypassed audio passes through while the delay memory continues to evolve.
- `CPU mute`: Bypassed audio passes through and the delay engines reset/skip processing for lower CPU use.

### Timing

- `Free delay knob`: Front-panel `Delay` controls time directly.
- `Tap/sync delay`: Tap tempo and division determine delay time.

Available divisions:

```text
1/1, 1/2, 1/4., 1/4, 1/8., 1/8, 1/8T, 1/16
```

You can seed tap tempo from the menu (`60`, `90`, `120`, or `140 BPM`) or patch gates into `Tap Tempo Gate`.

### Stereo Mode

- `Mono pedal`: Faithful mono pedal behavior.
- `Wide chorus`: Expands mono input to two channels with slight delay and LFO offset.
- `Ping-pong offset`: Expands mono input to two channels with a more obvious offset for spacious repeats.

Stereo modes expand mono input to stereo while the effect is engaged. Polyphonic inputs are processed channel-by-channel.

### Advanced Calibration

Calibration trims are saved with the patch:

- `Input gain`
- `BBD bias`
- `Clock bleed`
- `Compander trim`
- `Noise amount`
- `Wet makeup`
- `Feedback headroom`

Start with the defaults. Use trims when you want a specific personality: cleaner repeats, a noisier worn unit, more aggressive feedback, or a different wet/dry balance.

### Clear Delay Memory

Clears the internal delay buffer. Use this after runaway feedback or before a performance cue where you need silence.

## Presets

Factory presets include:

- `00 Short Slapback`
- `01 Short Reverb Smear`
- `02 Classic Chorus Delay`
- `03 Wet Vibrato Voice`
- `04 Long Dark Repeats`
- `05 Almost Oscillating`
- `06 Wide Chorus`
- `07 Dub Feedback Throw`
- `08 Clock Whine Texture`
- `09 Clean Rack Delay`

Use these as starting points, then adjust `Level`, `Feedback`, and `Delay` for the source you are processing.

## Quick Recipes

### Slapback

```text
Delay: 80-130 ms
Blend: 20-35%
Feedback: 5-20%
Depth: 0-10%
```

Put it after Cabinet Simulator for a clean studio slap, or before NAM Player for a pedalboard slap.

### Chorus Delay

```text
Delay: 120-220 ms
Blend: 35-55%
Feedback: 20-45%
Depth: 35-70%
Mode: Chorus
Shape: Triangle
```

Try `Wide chorus` stereo mode for a larger mono-to-stereo sound.

### Vibrato Voice

```text
Blend: 100% wet
Feedback: 0-15%
Depth: 40-80%
Mode: Vibrato
```

Patch `Wet` or main `Audio` to a separate mixer channel for an unstable tape-like voice.

### Dub Feedback Throw

```text
Blend: 40-70%
Feedback: 70-95%
Delay: tempo-synced 1/4 or 1/8.
```

Use a limiter downstream. Patch a gate or manual button into `Engage Gate`, or automate `Feedback CV` for throws.

## Troubleshooting

### The delay is too dark

Shorten `Delay`, reduce `Feedback`, or choose `Cleaner BBD` from the artifact profile menu. Darkening is part of the BBD model and increases at longer delay times.

### Feedback runs away

Lower `Feedback`, use `Clear delay memory`, or switch to `CPU mute` bypass if you need an immediate reset on bypass.

### Delay modulation is too wild

Reduce `Depth`, reduce `Delay CV amount`, or use triangle LFO instead of square. BBD clock modulation is intentionally pitchy.

### It sounds noisy

Try `Cleaner BBD`, lower `Noise amount` or `Clock bleed` in Advanced calibration, and check that `Level` is not overdriving the input too hard.

### Tap sync does not seem active

Right-click Mnemonix and choose `Timing -> Tap/sync delay`, or send at least two rising edges to `Tap Tempo Gate`. The menu tempo seeds are useful when you want sync without patching a tap source.
