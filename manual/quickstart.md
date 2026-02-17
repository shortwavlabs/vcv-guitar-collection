# Quickstart Guide

Get up and running with Guitar Tools in minutes.

## Table of Contents

- [Introduction](#introduction)
- [Installation](#installation)
- [Basic Setup](#basic-setup)
  - [Your First Guitar Patch](#your-first-guitar-patch)
  - [Understanding Signal Flow](#understanding-signal-flow)
- [NAM Player Module](#nam-player-module)
  - [Loading Models](#loading-models)
  - [Dialing in Your Tone](#dialing-in-your-tone)
  - [Using the Noise Gate](#using-the-noise-gate)
   - [Eco Mode (CPU Savings)](#eco-mode-cpu-savings)
  - [Tone Shaping with EQ](#tone-shaping-with-eq)
- [Cabinet Simulator Module](#cabinet-simulator-module)
  - [Loading Impulse Responses](#loading-impulse-responses)
  - [Blending IRs](#blending-irs)
  - [Tone Shaping](#tone-shaping)
- [Common Use Cases](#common-use-cases)
- [Next Steps](#next-steps)

---

## Introduction

**Guitar Tools** brings professional guitar amp modeling and cabinet simulation to VCV Rack. This guide will walk you through creating your first guitar processing patch in under 5 minutes.

**What You'll Learn:**
- How to set up a basic guitar processing chain
- How to load and use amp models
- How to add cabinet simulation
- How to shape your tone
- How to reduce CPU with Eco Mode
- Common patching techniques

---

## Installation

### Quick Install

1. **Open VCV Rack** and navigate to Library
2. **Search** for "Guitar Tools" or "Shortwav Labs"
3. **Click Subscribe** to install
4. **Restart** VCV Rack if prompted

The plugin includes 30+ bundled amp models and several cabinet impulse responses to get you started.

### Alternative: Manual Installation

Download the `.vcvplugin` file from [GitHub Releases](https://github.com/shortwavlabs/swv-guitar-collection/releases) and drag it onto VCV Rack.

---

## Basic Setup

### Your First Guitar Patch

Let's create a simple but powerful guitar processing chain.

#### Step 1: Add the Modules

1. **Right-click** in an empty area of your patch
2. Type "**NAM Player**" and add it to your patch
3. Type "**Cabinet Simulator**" and add it next to NAM Player
4. Add **VCV Audio** module (under Core) for input/output

**Your patch should look like:**
```
[Audio In] → [NAM Player] → [Cabinet Simulator] → [Audio Out]
```

#### Step 2: Connect the Audio Path

1. Connect your **Audio input** (from audio interface) to **NAM Player IN**
2. Connect **NAM Player OUT** to **Cabinet Simulator IN**
3. Connect **Cabinet Simulator OUT** to your **Audio output**

#### Step 3: Load an Amp Model

1. **Right-click** NAM Player
2. Try one of the bundled models:
   - **"Helga B 5150 BlockLetter - NoBoost.nam"** - Classic high-gain rock tone
   - **"George B Ceriatone King Kong Channel 1 60s mode.nam"** - Vintage British crunch
3. The green light illuminates when the model is loaded

#### Step 4: Load a Cabinet IR

1. **Right-click** Cabinet Simulator
2. Select **"Load IR to slot A"**
3. Browse to the included IRs (in the plugin's `res/` folder)
4. Load any `.wav` IR file

**🎸 You're ready to play!** Plug in your guitar and adjust the input gain on NAM Player.

### Understanding Signal Flow

```
Guitar → Audio Interface → VCV Rack
                              ↓
                         NAM Player
                    (Amp Simulation)
                              ↓
                     Cabinet Simulator
                    (Speaker Emulation)
                              ↓
                         Audio Output
```

**Key Concepts:**
- **NAM Player** = Your amplifier
- **Cabinet Simulator** = Your speaker cabinet + microphone
- **Signal chain order matters** (amp before cab, just like real gear)

---

## NAM Player Module

The NAM Player is your virtual guitar amplifier, pedal, or preamp.

### Loading Models

#### Method 1: Browse Bundled Presets

Available in the contextual menu as "Bundled models".

**Popular Models:**
- **5150 / 6505 Series**: High-gain metal tones
- **King Kong (Bluesbreaker clone)**: British blues/rock
- **V4 Countess**: Vintage all-tube warmth

#### Method 2: Load Custom Models

1. **Right-click** NAM Player, and click "Load model"
2. Navigate to your `.nam` model files
3. Select a model to load

**Where to Find Models:**
- [Neural Amp Modeler Model Collection](https://tonehunt.org/)
- Create your own with [NAM Trainer](https://github.com/sdatkinson/neural-amp-modeler)

### Dialing in Your Tone

#### Input Gain

The **`INPUT`** knob controls how hard you hit the amp.

```
Low Gain (-12dB to 0dB)    → Clean to edge-of-breakup tones
Medium Gain (0dB to +6dB)  → Classic overdrive
High Gain (+6dB to +12dB)  → Saturated, compressed tones
Max Gain (+12dB to +24dB)  → Extreme saturation
```

**Tips:**
- Start at **0dB** and adjust to taste
- Watch your **output waveform** - it should fill most of the display without clipping
- More gain ≠ better tone; find the sweet spot for your model

#### Output Level

The **`OUTPUT`** knob sets your final volume.

**Gain Staging:**
1. Set INPUT for desired tone
2. Adjust OUTPUT to match the rest of your patch
3. Use VCV Rack's VU meters to check levels (aim for -6dB to -3dB peaks)

### Using the Noise Gate

The built-in noise gate reduces unwanted hum and noise when you're not playing.

#### Quick Setup

1. **`THRESHOLD`**: Set just above your guitar's noise floor
   - Turn fully left (off) to start
   - Slowly turn right until noise disappears between notes
   - Go too far and you'll cut off note tails

2. **`ATTACK`**: How quickly the gate opens
   - Fast (1-3ms): Punchy, percussive
   - Medium (5-10ms): Natural
   - Slow (20ms+): Smooth fade-in

3. **`RELEASE`**: How quickly the gate closes
   - Fast (50-100ms): Tight, controlled
   - Medium (100-300ms): Natural decay
   - Slow (500ms+): Long sustain

4. **`HOLD`**: Time before gate starts closing
   - Short (0-10ms): Quick response
   - Medium (10-50ms): Balanced
   - Long (100ms+): Sustains longer

#### Typical Settings

**For High-Gain:**
```
THRESHOLD: -60dB
ATTACK: 2ms
RELEASE: 150ms
HOLD: 20ms
```

**For Clean Tones:**
```
THRESHOLD: -70dB
ATTACK: 5ms
RELEASE: 300ms
HOLD: 50ms
```

**Visual Feedback:** The gate light shows when the gate is open (signal passing).

### Eco Mode (CPU Savings)

NAM Player includes an **Eco Mode** option to reduce CPU usage while keeping the same core workflow.

#### Eco Mode States

- **Off**: Full processing quality (default)
- **On**: Lower CPU usage for heavier patches and live sets

#### How to Enable

1. **Right-click** NAM Player
2. Open **Eco Mode**
3. Select **On**

When enabled, the module shows an **ECO ON** badge so you can quickly see which instances are optimized.

**When to use it:**
- Large patches with multiple NAM Player modules
- Lower-latency live setups where CPU headroom is tight
- Projects running at higher sample rates

**Tip:** Start with Eco Mode Off while dialing in tones, then switch to On if you need extra headroom.

### Tone Shaping with EQ

The 5-band EQ lets you sculpt your tone after the amp model.

#### The Five Bands

| Band | Frequency | Use For |
|------|-----------|---------|
| **BASS** | 120 Hz | Fullness, body, low-end weight |
| **DEPTH** | 90 Hz | Deep low end, thump |
| **MIDDLE** | 700 Hz | Presence, cut-through-the-mix |
| **TREBLE** | 2.5 kHz | Bite, definition, string clarity |
| **PRESENCE** | 5 kHz | Air, sparkle, high-end detail |

#### Common EQ Settings

**Metal / High-Gain:**
```
BASS: -3dB (tight, controlled low end)
DEPTH: -6dB (reduce mud)
MIDDLE: +2dB (cut through mix)
TREBLE: +3dB (string definition)
PRESENCE: +4dB (air and clarity)
```

**Blues / Classic Rock:**
```
BASS: +2dB (full, warm)
DEPTH: 0dB (natural)
MIDDLE: +4dB (vocal quality)
TREBLE: 0dB (smooth)
PRESENCE: -2dB (less harsh)
```

**Clean / Jazz:**
```
BASS: +1dB (round, warm)
DEPTH: +2dB (full bottom)
MIDDLE: -2dB (smooth)
TREBLE: -3dB (mellow)
PRESENCE: -4dB (dark, warm)
```

**General Tips:**
- Start with all knobs at **0dB** (center)
- Make small adjustments (**±2-3dB** at a time)
- Cut rather than boost when possible
- Use your ears, not your eyes!

### CV Modulation

Every parameter on the NAM Player has a dedicated CV input for modulation and automation.

#### Using CV Inputs

**How it Works:**
- Each knob has a corresponding CV input jack below or next to it
- CV inputs accept **±5V** signals (standard in VCV Rack)
- When a CV cable is connected, it **replaces** the knob value
- No attenuverters needed - CV signals auto-scale to full parameter range

**Available CV Inputs:**
- **Input/Output Gain**: Dynamic volume control
- **Noise Gate** (Threshold, Attack, Release, Hold): Automated gating
- **5-Band EQ** (Bass, Middle, Treble, Presence, Depth): Dynamic tone shaping

**Example Uses:**

*Sidechain Gate:*
```
[Kick Drum Envelope] → CV GATE THRESHOLD → NAM Player
```
Opens the gate based on kick drum hits for rhythmic gating.

*Dynamic EQ:*
```
[LFO] → CV TREBLE → NAM Player
```
Modulate treble for tremolo-like brightness changes.

*Expression Control:*
```
[MIDI-CV Module] → CV INPUT → NAM Player
```
Control input gain with a MIDI expression pedal.

**Tips:**
- CV inputs are audio-rate for smooth, responsive modulation
- Combine multiple CV sources for complex automation
- Disconnecting CV cable returns control to the knob

---

## Cabinet Simulator Module

The Cabinet Simulator adds speaker cabinet character to your amp tone.

### Loading Impulse Responses

#### What is an Impulse Response (IR)?

An IR is a recording of how a speaker cabinet colors sound. It captures:
- Speaker type (e.g., Celestion Vintage 30, Jensen C12N)
- Cabinet construction (open-back, closed-back, 1x12, 4x12)
- Microphone position and type
- Room acoustics

#### Loading Your First IR

1. **Right-click** the Cabinet Simulator module
2. Select **"Load IR to slot A"**
3. Navigate to IR files (`.wav`, `.aiff`, or `.flac`)
4. Select an IR

**The green light** next to "A" indicates an IR is loaded.

#### Where to Find IRs

**Free IR Sources:**
- [Kalthallen Cabs](https://www.kalthallen-cabs.com/) - Free IR library
- [God's Cab](https://wilkinson-audio.com/collections/gods-cab) - High-quality free IRs
- Search for "free guitar cab IR" online

**Commercial IRs:**
- OwnHammer
- Celestion
- 3 Sigma Audio

### Blending IRs

One of Cabinet Simulator's most powerful features is **dual IR blending**.

#### Why Blend IRs?

- **Mix different microphones**: Combine bright (condenser) and warm (ribbon) mics
- **Layer speakers**: Blend Vintage 30 + Greenback for complex tone
- **Create space**: Mix close-mic + room-mic IRs
- **Experimental tones**: Combine contrasting cabinets

#### How to Blend

1. Load IR into **slot A**
2. Load different IR into **slot B**
3. Use the **`BLEND`** knob:
   - **Fully left**: 100% IR A
   - **Center**: 50% A + 50% B
   - **Fully right**: 100% IR B

**The lights** show which IRs are active.

#### Blend Examples

**Dual Microphone Setup:**
```
Slot A: Close SM57 (bright, present)
Slot B: Ribbon mic (warm, smooth)
Blend: 60% A / 40% B
Result: Balanced, professional studio tone
```

**Speaker Blend:**
```
Slot A: Celestion Vintage 30 (aggressive mids)
Slot B: Celestion Greenback (vintage warmth)
Blend: 50% / 50%
Result: Complex, rich cabinet tone
```

### Tone Shaping

#### Lowpass Filter

The **`LOWPASS`** knob rolls off high frequencies.

**Use Cases:**
- **12kHz-20kHz**: Minimal filtering (natural)
- **8kHz-12kHz**: Reduce harshness
- **5kHz-8kHz**: Darker, more vintage tone
- **2kHz-5kHz**: Extremely dark (special effects)

**Tip:** Start at 20kHz (fully right) and turn left until harshness disappears.

#### Highpass Filter

The **`HIGHPASS`** knob removes low frequencies.

**Use Cases:**
- **20Hz-50Hz**: Minimal filtering (full-range)
- **60Hz-100Hz**: Tighten low end (metal, palm mutes)
- **100Hz-200Hz**: Reduce boominess
- **200Hz+**: Telephone/radio effect

**Tip:** Use highpass to make room for bass and kick drum in a mix.

#### Typical Filter Settings

**Modern Metal:**
```
LOWPASS: 10kHz (reduce ice-pick highs)
HIGHPASS: 100Hz (tight, focused low end)
```

**Classic Rock:**
```
LOWPASS: 8kHz (vintage warmth)
HIGHPASS: 60Hz (natural fullness)
```

**Djent / Progressive:**
```
LOWPASS: 12kHz (clarity and definition)
HIGHPASS: 120Hz (extremely tight low end)
```

#### Output Level

The **`OUTPUT`** knob controls final cabinet volume.

**Best Practice:** Set for unity gain (input level ≈ output level) unless intentionally boosting/cutting.

### CV Modulation

The Cabinet Simulator includes **dedicated CV inputs for all parameters**, enabling dynamic control over IR blending and tone shaping.

#### Available CV Inputs

- **Blend CV**: Morph between IR A and IR B
- **Low-Pass Cutoff CV**: Dynamic brightness control
- **High-Pass Cutoff CV**: Automated low-end filtering
- **Output Level CV**: Volume automation

#### Using CV Inputs

**How it Works:**
- Each knob has a corresponding CV input jack
- CV inputs accept **±5V** signals (standard in VCV Rack)
- When a CV cable is connected, it **replaces** the knob value
- No attenuverters needed - CV signals auto-scale to full parameter range

**Example Uses:**

*Rhythmic IR Morphing:*
```
[LFO] → CV BLEND → Cabinet Simulator
```
Blend sweeps between two different cabinets rhythmically.

*Dynamic Filter Sweep:*
```
[Envelope Follower] → CV LOWPASS → Cabinet Simulator
```
Lowpass filter follows playing dynamics - harder playing = brighter tone.

*Automated Blend Changes:*
```
[Sequencer] → CV BLEND → Cabinet Simulator
```
Switch between IRs by song section (verse uses IR A, chorus uses IR B).

**Tips:**
- Use slow LFOs on blend for subtle movement between cabinets
- Automate filters to create wah-like effects
- CV inputs are audio-rate for smooth modulation

---

## Common Use Cases

### Use Case 1: Direct Recording Setup

**Goal:** Record guitar directly into your DAW through VCV Rack.

**Patch:**
```
Audio In → NAM Player → Cabinet Simulator → Audio Out → DAW
```

**Settings:**
- Input gain: Match your pickups (+3dB for singles, 0dB for humbuckers)
- Gate: Threshold at -60dB for high-gain, off for clean
- EQ: Shape tone as needed
- Cabinet: Load appropriate IR for your style

### Use Case 2: Re-Amping

**Goal:** Process pre-recorded DI guitar tracks.

**Patch:**
```
DAW Track → Audio In → NAM Player → Cabinet Simulator → Audio Out → New Track
```

**Tips:**
- Match input level to original recording
- Disable gate if already applied
- Experiment with different amp models
- Try various cabinet IRs

### Use Case 3: Creative Effects Processing

**Goal:** Use Guitar Tools on synths or drums for unique tones.

**Patch:**
```
VCV Modules → NAM Player → [Effects] → Cabinet Simulator → Output
```

**Try This:**
- Run drums through NAM Player for saturation and grit
- Process synth pads through cabinet sim for lo-fi texture
- Use gate creatively for rhythmic gating effects

### Use Case 4: Building a Pedal Board

**Goal:** Create a complete guitar rig with pedals before the amp.

**Patch:**
```
Audio In → [Overdrive] → [Delay] → NAM Player → Cabinet Simulator → Output
                                      ↑
                                   [Modulation]
```

**Suggested Modules:**
- Overdrive: VCV Fundamental Saturator
- Delay: Vult Debriatus
- Modulation: AS Phaser, Chorus

**Signal Chain Order:**
1. **Before NAM Player**: Overdrive, fuzz, compression, wah
2. **After NAM Player**: Modulation, delay, reverb
3. **Cabinet Sim last**: Always

---

## Next Steps

### Explore More

- **Browse All Models**: Try each bundled NAM model to find your favorites
- **Create Presets**: Save your favorite amp/cab combinations as VCV patches
- **Experiment with EQ**: Develop your ear for tone shaping
- **Try IR Blending**: Create unique cabinet tones

### Advanced Topics

Ready to dive deeper? Check out:

- **[Advanced Usage Guide](advanced-usage.md)**: Performance optimization, custom models, and pro techniques
- **[API Reference](api-reference.md)**: Technical documentation for developers
- **[FAQ](faq.md)**: Troubleshooting and common questions
- **[Examples](examples/)**: Real-world patches and setups

### Get More Models

**NAM Model Sources:**
- [ToneHunt](https://tonehunt.org/) - Community model library
- [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler) - Create your own

**IR Sources:**
- See [Loading Impulse Responses](#loading-impulse-responses) section above

### Join the Community

- **Report Issues**: [GitHub Issues](https://github.com/shortwavlabs/swv-guitar-collection/issues)
- **Share Patches**: Post your creations in VCV Rack forums
- **Support Development**: [Ko-fi](https://ko-fi.com/shortwavlabs)

---

## Quick Reference

### NAM Player Controls

| Control | Function | Range |
|---------|----------|-------|
| INPUT | Input gain | -24dB to +24dB |
| OUTPUT | Output level | -24dB to +24dB |
| GATE THRESHOLD | Noise gate threshold | -80dB to 0dB |
| GATE ATTACK | Gate opening time | 0.1ms to 100ms |
| GATE RELEASE | Gate closing time | 10ms to 1000ms |
| GATE HOLD | Gate hold time | 0ms to 500ms |
| BASS | Bass EQ (120 Hz) | -12dB to +12dB |
| MIDDLE | Mid EQ (700 Hz) | -12dB to +12dB |
| TREBLE | Treble EQ (2.5 kHz) | -12dB to +12dB |
| PRESENCE | Presence EQ (5 kHz) | -12dB to +12dB |
| DEPTH | Depth EQ (90 Hz) | -12dB to +12dB |

### Cabinet Simulator Controls

| Control | Function | Range |
|---------|----------|-------|
| BLEND | IR A/B mix | 0 (A) to 1 (B) |
| LOWPASS | High-frequency rolloff | 1kHz to 20kHz |
| HIGHPASS | Low-frequency rolloff | 20Hz to 500Hz |
| OUTPUT | Output level | -24dB to +24dB |

---

**Happy patching! 🎸**

For help, see the [FAQ](faq.md) or contact [contact@shortwavlabs.com](mailto:contact@shortwavlabs.com).
