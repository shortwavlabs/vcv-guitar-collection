# Example: Mnemonix Memory Delay

A practical analog-style delay, chorus, and feedback patch using Mnemonix.

## Overview

This patch adds Mnemonix to a basic guitar rig:

```text
Guitar -> Audio Input -> NAM Player -> Cabinet Simulator -> Mnemonix -> Audio Output
```

**Time to setup:** 10 minutes  
**Difficulty:** Beginner to intermediate  
**CPU Usage:** Low-medium

---

## Required Modules

1. **VCV Audio** (from Core)
2. **NAM Player** (Guitar Tools)
3. **Cabinet Simulator** (Guitar Tools)
4. **Mnemonix** (Guitar Tools)
5. Optional: **Limiter** for high-feedback experiments

---

## Basic Delay Setup

### 1. Patch the Audio Path

```text
Audio-8 OUTPUT 1 -> NAM Player IN
NAM Player OUT -> Cabinet Simulator IN
Cabinet Simulator OUT -> Mnemonix Audio
Mnemonix Audio -> Audio-8 INPUT 1
```

Use this order for a polished rack delay. For pedalboard behavior, place Mnemonix before NAM Player.

### 2. Start with Safe Settings

```text
Level: 55%
Blend: 30%
Feedback: 20%
Delay: 100 ms
Depth: 0%
Mode: Chorus
Shape: Triangle
Range: Normal
Effect: Engaged
```

Play a few short phrases and adjust `Blend` until repeats sit behind the dry tone.

### 3. Set Feedback

- `10-25%`: slapback and single repeats
- `25-55%`: classic delay
- `55-80%`: long repeats and dub echoes
- `80%+`: self-oscillation territory

Put a limiter after Mnemonix before exploring very high feedback.

---

## Chorus Delay Variation

Use Mnemonix as a modulated memory delay:

```text
Blend: 45%
Feedback: 35%
Delay: 150-220 ms
Depth: 50%
Mode: Chorus
Shape: Triangle
Stereo mode: Wide chorus
```

Right-click Mnemonix, open **Stereo mode**, and choose **Wide chorus**. A mono guitar input becomes a stereo output while the effect is engaged.

---

## Wet Vibrato Variation

Use Mnemonix as a pitch-modulated voice:

```text
Blend: 100%
Feedback: 0-15%
Delay: 80-160 ms
Depth: 60%
Mode: Vibrato
Shape: Triangle
```

Patch `Wet` to a separate mixer channel if you want to blend the vibrato voice manually.

---

## Tap-Synced Dub Throw

### 1. Enable Sync

1. Right-click Mnemonix.
2. Open **Timing**.
3. Select **Tap/sync delay**.
4. Choose `1/4` or `1/8.`.
5. Use a menu tempo seed, or patch a trigger/button into `Tap Tempo Gate`.

### 2. Patch Feedback Performance Control

```text
Manual CV / expression -> Feedback CV
```

Turn up the `Feedback CV amount` attenuverter until the performance control can push the repeats near oscillation.

### 3. Safety

```text
Mnemonix -> Limiter -> Output
```

Use **Clear delay memory** from the context menu after heavy feedback moments.

---

## Using Utility Outputs

Mnemonix can also drive modulation:

```text
Mnemonix LFO -> Cabinet Simulator Blend CV
Mnemonix Compander envelope CV -> NAM Player Input CV
Mnemonix BBD clock /512 gate -> Clocked modulation or sequencer logic
```

The clock outputs follow the internal BBD clock, so they change with `Delay`, `Depth`, tap sync, and LFO modulation.

---

## Suggested Presets

Start from these factory presets:

- `00 Short Slapback`
- `02 Classic Chorus Delay`
- `05 Almost Oscillating`
- `06 Wide Chorus`
- `07 Dub Feedback Throw`
- `09 Clean Rack Delay`

Load a preset, then adjust `Level` for the source and `Blend` for the patch context.

---

## Troubleshooting

### Repeats are too loud or too quiet

Adjust `Blend` first, then use context-menu **Advanced calibration -> Wet makeup** only if you need a different saved calibration.

### Delay is too dark

Shorten `Delay`, choose **Artifact profile -> Cleaner BBD**, or reduce `Noise amount` / `Clock bleed`.

### Feedback will not stop

Lower `Feedback`, bypass with **CPU mute**, or use **Clear delay memory**.

### Modulation feels too jumpy

Switch `LFO Shape` to Triangle and reduce `Depth`.
