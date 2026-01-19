# Example: Basic Guitar Rig

A simple, effective guitar processing setup perfect for beginners.

## Overview

This patch demonstrates the fundamental guitar signal chain:
```
Guitar → Audio Input → NAM Player → Cabinet Simulator → Audio Output
```

**Time to setup:** 5 minutes  
**Difficulty:** Beginner  
**CPU Usage:** Low-Medium (~15%)

---

## Required Modules

1. **VCV Audio** (from Core)
   - For audio input and output
2. **NAM Player** (Guitar Tools)
   - Amplifier simulation
3. **Cabinet Simulator** (Guitar Tools)
   - Speaker cabinet emulation

---

## Step-by-Step Setup

### 1. Add Modules to Patch

1. Right-click empty area
2. Add **Audio-8** (from VCV Audio)
3. Add **NAM Player** (from Guitar Tools / Shortwav Labs)
4. Add **Cabinet Simulator** (from Guitar Tools / Shortwav Labs)

**Layout:** Arrange left to right in signal flow order.

### 2. Connect Audio Path

**Connections:**
```
Audio-8 OUTPUT 1 → NAM Player IN
NAM Player OUT → Cabinet Simulator IN
Cabinet Simulator OUT → Audio-8 INPUT 1
```

**Tip:** Use different cable colors to track signal flow visually.

### 3. Configure Audio Interface

**In Audio-8 module:**
1. **Device**: Select your audio interface
2. **Sample rate**: 48000 Hz (recommended)
3. **Block size**: 256 samples (good balance)

**Hardware setup:**
- Guitar → Audio interface input 1
- Audio interface outputs → Monitors/headphones

### 4. Load Amp Model

**On NAM Player:**
1. Click **`LOAD`** button
2. Try a bundled model:
   - **For clean tones**: "George B Ceriatone King Kong Channel 1 60s mode.nam"
   - **For crunch**: "Helga B 5150 BlockLetter - NoBoost.nam"
   - **For high-gain**: "Helga B 6505+ Red ch - MXR Drive.nam"

**Verify:** Green light should illuminate when model loads.

### 5. Load Cabinet IR

**On Cabinet Simulator:**
1. Right-click module
2. Select **"Load IR to slot A"**
3. Browse to plugin directory:
   - Look in `res/irs/` if included
   - Or use your own IR collection

**Recommendation:** Start with a classic 4x12 cabinet IR.

### 6. Set Initial Levels

**NAM Player:**
- **INPUT**: Start at 0dB (12 o'clock)
- **OUTPUT**: Start at 0dB (12 o'clock)
- **GATE THRESHOLD**: Fully left (off) for now
- **All EQ knobs**: 0dB (center position)

**Cabinet Simulator:**
- **BLEND**: If using single IR, position doesn't matter
- **LOWPASS**: Fully right (20kHz - no filtering)
- **HIGHPASS**: Fully left (20Hz - no filtering)
- **OUTPUT**: 0dB (center)

---

## Dialing In Your Tone

### Step 1: Set Input Gain

1. **Play guitar** at your typical dynamics
2. **Watch output waveform** on NAM Player
3. **Adjust INPUT** so waveform fills ~70% of display
   - Too low: Weak tone, noise
   - Too high: Clipping, distortion

**Typical settings:**
- Single-coils: +3 to +6dB
- Humbuckers: 0 to +3dB
- Active pickups: -3 to 0dB

### Step 2: Add Noise Gate (if needed)

**For high-gain only:**

1. **Turn GATE THRESHOLD** from left to right slowly
2. **Stop** when background noise disappears between notes
3. **Fine-tune** to avoid cutting note sustain

**Typical setting:** -60dB for high-gain, -70dB for medium gain

**Advanced (optional):**
- **ATTACK**: 2ms (fast response)
- **RELEASE**: 150ms (natural decay)
- **HOLD**: 20ms (slight sustain)

### Step 3: Shape with EQ

Start with all knobs at center (0dB), then adjust:

**For more clarity:**
- **TREBLE**: +2 to +4dB
- **PRESENCE**: +2 to +3dB

**For more warmth:**
- **BASS**: +1 to +3dB
- **MIDDLE**: +1 to +2dB

**For tighter low end:**
- **DEPTH**: -3 to -6dB

**For less harsh tone:**
- **TREBLE**: -2 to -4dB
- **PRESENCE**: -2 to -3dB

### Step 4: Fine-Tune Cabinet

**Lowpass filter** (reduce harsh highs):
- Start at 20kHz (far right)
- Turn left until harshness disappears
- Typical: 8kHz-12kHz

**Highpass filter** (tighten low end):
- Start at 20Hz (far left)
- Turn right for tighter bass
- Typical: 80Hz-100Hz

### Step 5: Set Final Output Level

Adjust **Cabinet Sim OUTPUT** to:
- Match other tracks in your mix
- Avoid clipping audio interface
- Leave headroom (-6dB to -3dB peak)

**Tip:** Use VCV VU meters or interface meters to check levels.

---

## Recommended Settings by Style

### Clean / Jazz

**NAM Player:**
- Model: "King Kong Channel 1 60s mode"
- INPUT: +2dB
- Gate: Off
- BASS: +1dB, MIDDLE: 0dB, TREBLE: -2dB
- PRESENCE: -3dB, DEPTH: 0dB
- OUTPUT: 0dB

**Cabinet Sim:**
- IR: Open-back 1x12 or 2x12
- LOWPASS: 10kHz
- HIGHPASS: 60Hz
- OUTPUT: 0dB

**Tone:** Warm, round, natural

---

### Blues / Classic Rock

**NAM Player:**
- Model: "King Kong Channel 2 70s" or "5150 NoBoost"
- INPUT: +4dB
- Gate: -65dB
- BASS: +2dB, MIDDLE: +3dB, TREBLE: +1dB
- PRESENCE: 0dB, DEPTH: -2dB
- OUTPUT: 0dB

**Cabinet Sim:**
- IR: Greenback 4x12
- LOWPASS: 9kHz
- HIGHPASS: 80Hz
- OUTPUT: 0dB

**Tone:** Crunchy, vocal, vintage

---

### Modern Metal

**NAM Player:**
- Model: "6505+ Red ch - MXR Drive"
- INPUT: +6dB
- Gate: -60dB, ATTACK: 2ms, RELEASE: 120ms
- BASS: -2dB, MIDDLE: +2dB, TREBLE: +3dB
- PRESENCE: +4dB, DEPTH: -6dB
- OUTPUT: -2dB

**Cabinet Sim:**
- IR: V30 4x12, close mic
- LOWPASS: 11kHz
- HIGHPASS: 100Hz
- OUTPUT: 0dB

**Tone:** Tight, aggressive, defined

---

## Common Issues & Solutions

### "My tone sounds thin"

**Likely cause:** Missing cabinet simulation or wrong IR

**Fix:**
1. Ensure Cabinet Sim is connected after NAM Player
2. Try different IR (darker cabinet)
3. Increase BASS and reduce TREBLE

---

### "I hear crackling or pops"

**Likely cause:** CPU overload or buffer too small

**Fix:**
1. Increase Audio-8 block size to 512 or 1024
2. Choose lighter NAM model (Linear or Nano)
3. Close other applications

---

### "Too much noise/hum"

**Likely cause:** Guitar pickup noise or gain too high

**Fix:**
1. Enable and adjust noise gate
2. Lower INPUT gain slightly
3. Move away from electromagnetic interference
4. Consider guitar with noise-cancelling pickups

---

### "Sounds distorted or harsh"

**Likely cause:** Clipping or too much gain

**Fix:**
1. Lower INPUT gain
2. Check output waveform - shouldn't flat-top
3. Reduce TREBLE and PRESENCE
4. Lower Cabinet Sim LOWPASS frequency

---

## Saving Your Patch

1. **File → Save As**
2. Name: "My Basic Guitar Rig.vcv"
3. Save to Documents/Rack2/patches/

**Tip:** Save variations with different amp models as separate patches for quick recall.

---

## Next Steps

Once comfortable with this basic rig:

1. **Experiment with models**: Try all 30+ bundled amps
2. **Try different IRs**: Each cabinet has unique character
3. **Add effects**: Insert modules before or after amp
4. **Read Advanced Guide**: Learn optimization and pro techniques

### Suggested Additions

**Before NAM Player:**
- Overdrive/boost pedals (VCV Saturator)
- Wah (external VCV module)
- Compressor

**After Cabinet Sim:**
- Delay (VCV Delay)
- Reverb (VCV Reverb or external)
- Modulation (Chorus, Phaser, Flanger)

---

## Example Variations

### Variation 1: Dual Cabinet Blend

**Modification:**
1. Load different IR into Cabinet Sim slot B
2. Use BLEND knob to mix (try 50/50)

**Result:** More complex, layered cabinet tone

---

### Variation 2: EQ Before Amp

**Modification:**
1. Add VCV EQ module before NAM Player
2. Boost mids for more drive
3. Cut bass for tighter tone

**Result:** Additional tonal control, different distortion character

---

### Variation 3: Parallel Compression

**Modification:**
1. Split signal after Cabinet Sim
2. Add heavy compression to one path
3. Mix back together (70% dry, 30% compressed)

**Result:** Punchy, controlled dynamics

---

## Resources

- **More Models**: [ToneHunt](https://tonehunt.org/)
- **Free IRs**: [Kalthallen Cabs](https://www.kalthallen-cabs.com/)
- **Forum**: [VCV Rack Community](https://community.vcvrack.com/)

---

**Questions?** See [FAQ](../faq.md) or [contact support](mailto:contact@shortwavlabs.com).

**Ready for more?** Check out [High-Gain Metal Example](high-gain-metal.md) or [Dual Amp Setup](dual-amp-setup.md).
