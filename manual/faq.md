# Frequently Asked Questions (FAQ)

Common questions and solutions for Guitar Tools.

## Table of Contents

- [General Questions](#general-questions)
- [Installation & Setup](#installation--setup)
- [NAM Player](#nam-player)
- [Cabinet Simulator](#cabinet-simulator)
- [Performance & Optimization](#performance--optimization)
- [Audio Quality](#audio-quality)
- [Compatibility](#compatibility)
- [Troubleshooting](#troubleshooting)
- [Advanced Topics](#advanced-topics)

---

## General Questions

### What is Guitar Tools?

Guitar Tools is a VCV Rack plugin that provides professional guitar amp modeling and cabinet simulation using Neural Amp Modeler (NAM) technology and convolution-based IR processing.

### Do I need real guitar amps to use this?

No! Guitar Tools includes 30+ bundled amp models covering classic and modern amplifiers. You can start making music immediately.

### Is this only for guitar?

While optimized for guitar, you can use Guitar Tools on any audio source: bass, synthesizers, drums, vocals, or anything else for creative processing.

### How much does it cost?

Guitar Tools is **free and open source** under GPL-3.0 license. You can use it for personal and commercial projects.

### Where can I get more models and IRs?

- **NAM Models**: [ToneHunt](https://tonehunt.org/), [NAM Community](https://github.com/sdatkinson/neural-amp-modeler)
- **Cabinet IRs**: Search for "free guitar cab IR" or purchase from OwnHammer, Celestion, 3 Sigma Audio

---

## Installation & Setup

### How do I install Guitar Tools?

**Option 1 (Recommended):** Through VCV Library
1. Open VCV Rack → Library
2. Search "Guitar Tools"
3. Click Subscribe

**Option 2:** Download `.vcvplugin` from [Releases](https://github.com/shortwavlabs/swv-guitar-collection/releases) and drag onto VCV Rack.

See [Installation Guide](../README.md#installation) for details.

### Where are the bundled models located?

Bundled models are in the plugin's installation directory:
- **macOS**: `~/Library/Application Support/Rack2/plugins-*/swv-guitar-collection/res/models/`
- **Windows**: `%LOCALAPPDATA%\Rack2\plugins-*\swv-guitar-collection\res\models\`
- **Linux**: `~/.local/share/Rack2/plugins-*/swv-guitar-collection/res/models/`

### Can I use my own NAM models?

Yes! Click the "LOAD" button on NAM Player and browse to any `.nam` file on your system.

### The plugin doesn't appear in VCV Rack after installation

**Try:**
1. Restart VCV Rack completely
2. Check Library → Subscriptions to ensure it's subscribed
3. Look for "Shortwav Labs" or "Guitar Tools" in the module browser
4. Check VCV Rack log for errors (Help → Open Log File)

---

## NAM Player

### What does "NAM" stand for?

**Neural Amp Modeler** - A machine learning technology that captures the behavior of guitar amplifiers, pedals, and other audio equipment.

### Why is there a yellow light next to the model name?

The yellow light indicates **sample rate mismatch**. The model was trained at a different sample rate (usually 48kHz) than your current VCV Rack rate. 

**Is this a problem?** No! The plugin automatically resamples with high quality. You may see a small CPU increase (~5%).

### How do I navigate through bundled models?

Use the **`<`** and **`>`** buttons to cycle through presets. Or click **`LOAD`** to browse all models.

### Can I automate parameters?

**VCV Rack Pro**: Yes, map parameters to DAW automation via "Map to host parameter"

**VCV Rack Standalone**: Use CV inputs for modulation (right-click parameters to enable CV input)

### What's the difference between model types (Linear, LSTM, WaveNet)?

| Type | Quality | CPU | Latency | Use Case |
|------|---------|-----|---------|----------|
| **Linear** | Good | Low | Lowest | Live performance, low-latency monitoring |
| **LSTM** | Excellent | Medium | Low | General recording and production |
| **WaveNet** | Best | High | Medium | Studio work, final mixes |

### My guitar sounds too quiet/loud

Adjust the **INPUT** knob:
- **Single-coil pickups**: Try +3dB to +6dB
- **Humbuckers**: Try 0dB to +3dB
- **Active pickups**: Try -3dB to 0dB

Watch the output waveform display - it should fill most of the display without clipping (flat-topping).

### The noise gate cuts off my notes

The **THRESHOLD** is set too high. Try:
1. Turn THRESHOLD knob fully left (off)
2. Slowly turn right until background noise disappears
3. Stop before it cuts note tails
4. Increase **RELEASE** time for longer sustain

### How do I get more gain/distortion?

1. **Increase INPUT gain** - Drives the model harder
2. **Choose high-gain model** - Try 5150/6505 models
3. **Add overdrive before NAM** - Stack drive pedals (VCV modules)
4. **Boost specific frequencies** - Use EQ to emphasize mids/treble

### Can I use multiple NAM Players in one patch?

Yes! Each instance runs independently. Great for:
- Dual amp setups (left/right)
- Layering different amp tones
- Separate processing for different instruments

**Note:** CPU usage multiplies per instance.

---

## Cabinet Simulator

### Do I need Cabinet Sim if I'm using NAM Player?

**Highly recommended!** NAM models emulate the amplifier only. Cabinet Sim adds the speaker cabinet character that completes the tone.

**Signal chain:** Guitar → NAM Player (amp) → Cabinet Sim (speaker) → Output

### What's the difference between IR slots A and B?

Both slots function identically. The two slots allow you to:
- **Blend different IRs** for complex tones
- **Quick A/B comparison** without reloading
- **Mix microphones** (e.g., SM57 + Ribbon)

### How do I blend two IRs?

1. Load IR into slot A
2. Load different IR into slot B
3. Use **BLEND** knob:
   - Fully left = 100% A
   - Center = 50/50 blend
   - Fully right = 100% B

### What IR file formats are supported?

- **WAV** (16/24/32-bit PCM, 32-bit float)
- **AIFF** (16/24/32-bit)
- **FLAC** (16/24-bit)

Mono or stereo, any sample rate (automatically resampled).

### How long should my IR files be?

**Ideal:** 1-2 seconds

**Why?** Cabinet resonance decays in ~1 second. Longer IRs:
- Waste CPU (processing silence)
- Increase memory usage
- Add unnecessary latency

**Tip:** Trim IRs in audio editor to remove excess silence.

### What does "Normalize" do?

Normalization scales the IR to 0dBFS peak level. This:
- **Prevents level jumps** when switching IRs
- **Makes blending easier** (matched levels)
- **Maintains consistent output**

**Enable via context menu** (right-click module → Enable Normalization for Slot A/B)

### Should I use lowpass and highpass filters?

**Yes, often!** These filters shape your cabinet tone:

**Lowpass:**
- Removes harsh high frequencies
- Simulates speaker roll-off
- Start at 10-12kHz, adjust to taste

**Highpass:**
- Tightens low end
- Reduces muddiness
- Start at 80-100Hz for guitar

**Tip:** Adjust by ear, especially when mixing with bass.

---

## Performance & Optimization

### VCV Rack is using too much CPU. How can I optimize?

**Quick fixes:**
1. **Increase buffer size**: Settings → Audio → Block size (512 or 1024)
2. **Use lighter models**: Browse for models with "lite" or "nano" in the name
3. **Reduce polyphony**: Limit to 1-4 voices
4. **Enable Eco Mode**: Right-click NAM Player → Eco Mode → On
5. **Lower sample rate**: 96kHz → 48kHz (Settings → Audio → Sample rate)

See [Performance Optimization](advanced-usage.md#performance-optimization) for detailed strategies.

### Which NAM models use the least CPU?

Generally:
- **Linear models**: Lowest CPU (~2-3%)
- **Nano/Lite LSTM**: Low-medium CPU (~5-8%)
- **Standard LSTM**: Medium CPU (~10-15%)
- **WaveNet**: Highest CPU (~15-20%)

Model filename often indicates architecture (e.g., `*_lite.nam`).

### Can I use this in real-time for live performance?

**Yes!** Tips for low latency:
1. Use **smaller buffer size** (128-256 samples)
2. Choose **lightweight models** (Linear, Nano LSTM)
3. **Disable unused effects** (bypass EQ if not needed)
4. Use **ASIO drivers** (Windows) for lowest latency
5. **Test thoroughly** before performance

**Expected latency:** 3-10ms total (interface + VCV + model)

### What is Eco Mode and when should I use it?

Eco Mode is a NAM Player context-menu option with two states:

- **Off**: Full processing quality (default)
- **On**: Lower CPU usage

Use **On** when running dense patches, higher sample rates, or live sets where CPU headroom is limited.

**How to enable:** Right-click **NAM Player** → **Eco Mode** → **On**

### How much RAM does Guitar Tools use?

**Base plugin:** ~10 MB  
**Per NAM model:** 50-200 MB (varies by architecture)  
**Per IR (2 seconds):** ~0.4 MB  

**Example setup:**
- NAM Player with standard model: ~120 MB
- Cabinet Sim with 2 IRs: ~1 MB
- **Total:** ~131 MB

---

## Audio Quality

### Does processing quality degrade at different sample rates?

No! The plugin automatically resamples with high-quality algorithms. You can run VCV Rack at:
- 44.1kHz, 48kHz, 88.2kHz, 96kHz, or higher

**Note:** Most NAM models are trained at 48kHz. Running at 48kHz avoids resampling overhead.

### I hear artifacts or digital distortion

**Possible causes:**

1. **Input clipping**: Lower INPUT gain
2. **Output clipping**: Lower OUTPUT gain
3. **Buffer underruns**: Increase buffer size
4. **Corrupted model**: Try reloading or different model
5. **CPU overload**: Reduce polyphony or effects

**Check:** Output waveform display should NOT show flat-topping.

### Is the tone the same as the original amp?

NAM models are **extremely accurate** when:
- Captured properly (good training data)
- Used with appropriate settings
- Paired with appropriate cabinet IRs

**Factors affecting accuracy:**
- Model quality (training time, architecture)
- Input level matching the original capture
- Sample rate consistency

High-quality NAM models are often **indistinguishable** from the original hardware in blind tests.

### Why does my tone sound thin or harsh?

**Common causes:**

1. **Missing cabinet simulation**: Always use Cabinet Sim after NAM Player
2. **Wrong IR choice**: Try different IRs (darker vs. brighter)
3. **Sample rate mismatch**: Not usually an issue, but verify
4. **Input too low**: Increase INPUT gain
5. **Need EQ adjustment**: Cut harsh frequencies (TREBLE, PRESENCE)

**Quick fix:** Load a classic 4x12 IR and reduce TREBLE by -3dB.

### Can I improve the quality beyond the default settings?

**Yes:**

1. **Use higher-quality models**: Look for "standard" or "HQ" versions
2. **Match sample rates**: Run VCV at 48kHz (model native rate)
3. **Use professional IRs**: Commercial IRs often have better phase coherence
4. **Minimize processing chain**: Fewer modules = less cumulative error
5. **Record at 24-bit**: Ensure adequate bit depth

---

## Compatibility

### What versions of VCV Rack are supported?

**Required:** VCV Rack 2.6.0 or later

**Tested on:**
- VCV Rack 2.6.0
- VCV Rack 2.6.3

**Older versions:** Not supported (may not work)

### Does this work with VCV Rack 1.x?

No. Guitar Tools requires VCV Rack 2.6.0+.

### Which operating systems are supported?

- **macOS**: 10.15 (Catalina) or later (Intel and Apple Silicon)
- **Windows**: 10 or later (64-bit)
- **Linux**: 64-bit, glibc 2.27+ (Ubuntu 20.04+, Fedora 30+, etc.)

### Does it work on Apple Silicon (M1/M2/M3)?

**Yes!** Native ARM64 support for optimal performance on Apple Silicon Macs.

### Can I use this in my DAW?

**Yes, with VCV Rack Pro:**
1. Insert VCV Rack Pro plugin in DAW
2. Load Guitar Tools patch inside VCV Rack
3. Route audio from DAW track through VCV

**Supported DAWs:** Any VST3/AU compatible DAW (Ableton, Logic, Reaper, etc.)

### Can I use this with other VCV Rack plugins?

Absolutely! Guitar Tools integrates seamlessly with:
- **Effects**: AS plugins, Vult, etc.
- **Sequencers**: Impromptu, etc.
- **Utilities**: VCV Fundamental, Bogaudio, etc.

**Common combinations:**
- Add reverb/delay after Cabinet Sim
- Add overdrive/compression before NAM Player
- Use mixers for parallel processing

---

## Troubleshooting

### No sound is coming out

**Checklist:**
1. ✅ Check cable connections (IN and OUT ports)
2. ✅ Verify model is loaded (green light on)
3. ✅ Check INPUT and OUTPUT knobs (not at zero)
4. ✅ Verify audio device selected (Settings → Audio)
5. ✅ Test with VCV VCO to isolate issue
6. ✅ Check gate isn't closing (gate light should be on)

### Model won't load / Green light doesn't turn on

**Troubleshooting:**
1. **Verify file is `.nam` format** (not .json, .wav, etc.)
2. **Check file isn't corrupted**: Try different model
3. **Test with bundled model**: Use `<` / `>` buttons
4. **Check permissions**: Ensure read access to file
5. **View log**: Help → Open Log File (look for error messages)

### Crackling, clicking, or popping sounds

**Solutions:**

1. **Increase buffer size**: 
   - Settings → Audio → Block size (try 512 or 1024)

2. **Reduce CPU load**:
   - Use lighter NAM model
   - Reduce polyphony
   - Close other applications

3. **Check input/output levels**:
   - Ensure no clipping (watch output display)
   - Lower INPUT or OUTPUT gains

4. **Update audio drivers**:
   - Especially important on Windows
   - Use ASIO drivers if possible

### High latency when monitoring

**Reduce latency:**

1. **Decrease buffer size**: Settings → Audio → Block size (128 or 256)
2. **Use direct monitoring**: Monitor through audio interface instead
3. **Optimize model choice**: Use Linear or Nano LSTM models
4. **Check audio interface**: Use native drivers (not generic USB audio)

**For recording:** Record both DI (dry) and processed signals, monitor DI with zero latency.

### Plugin crashes VCV Rack

**Immediate action:**
1. Note what you were doing (loading model, adjusting parameter, etc.)
2. Check crash log: Help → Open Log File
3. Report bug: [GitHub Issues](https://github.com/shortwavlabs/swv-guitar-collection/issues)

**Workaround:**
1. Restart VCV Rack
2. Load patch without problematic model
3. Update to latest plugin version

### Model loads but sounds wrong

**Possible issues:**

1. **Model file corrupted**: Re-download
2. **Wrong model type**: Verify it's a NAM model (not audio file)
3. **Extreme settings**: Reset INPUT/OUTPUT to 0dB
4. **Missing cabinet**: Always use Cabinet Sim with NAM Player
5. **Sample rate issue**: Try changing VCV rate to 48kHz

---

## Advanced Topics

### Can I create my own NAM models?

**Yes!** You can capture your own amplifiers, pedals, or unique signal chains.

**Process:**
1. Set up re-amping chain
2. Record training signal through your gear
3. Use [NAM Trainer](https://github.com/sdatkinson/neural-amp-modeler) to create model
4. Load in Guitar Tools

See [Custom Model Creation](advanced-usage.md#custom-model-creation) for detailed guide.

### Can I use CV to control parameters?

**Yes!** Right-click any parameter and enable CV input. Then connect CV sources:
- LFOs for rhythmic modulation
- Envelope followers for dynamic control
- Sequencers for stepped changes

**Examples:**
- Modulate BLEND for morphing between IRs
- Control EQ based on envelope for dynamic tone
- Automate INPUT gain for dynamics

### How does this compare to commercial amp sims?

**Advantages:**
- Free and open source
- Runs in modular VCV Rack environment
- Uses state-of-the-art NAM technology
- Capture your own gear
- Community model sharing

**Trade-offs:**
- Requires VCV Rack knowledge
- Manual cabinet IR loading (no built-in browser)
- Fewer included presets than commercial options

**Quality:** NAM technology rivals or exceeds commercial neural amp sims in accuracy.

### Can I contribute models or IRs?

**Absolutely!** See [CONTRIBUTING.md](../CONTRIBUTING.md) for guidelines.

**Requirements:**
- Legal rights to share the capture
- High-quality recordings
- Proper documentation
- Sample audio examples

### Is there MIDI support?

NAM Player and Cabinet Sim don't have built-in MIDI CC control, but you can:

**VCV Rack Pro:** Map parameters to DAW MIDI automation  
**VCV Rack Standalone:** Use VCV MIDI-CC module → CV to control parameters

### Can I use this for bass guitar?

**Yes!** Bass works great:
1. Use amp models designed for bass (or guitar amps for unique tones)
2. Load bass cabinet IRs (or create your own)
3. Adjust HIGHPASS lower (40-60Hz) for low-end extension
4. Consider disabling DEPTH EQ for full low end

### Does this work with extended range guitars (7/8/9 string)?

**Yes!** NAM models capture frequency response from DC to 20kHz+, handling extended range instruments without issues.

**Tips:**
- Use HIGHPASS filter aggressively (100-120Hz) for tight low end
- Cut DEPTH EQ (-3 to -6dB) to avoid mud
- Choose models with good low-frequency response

---

## Still Have Questions?

### Documentation

- **[Quickstart Guide](quickstart.md)** - Getting started
- **[Advanced Usage](advanced-usage.md)** - Deep dive into features
- **[API Reference](api-reference.md)** - Technical documentation

### Community

- **GitHub Issues**: [Report bugs or request features](https://github.com/shortwavlabs/swv-guitar-collection/issues)
- **GitHub Discussions**: [Ask questions](https://github.com/shortwavlabs/swv-guitar-collection/discussions)
- **VCV Forum**: [Share patches](https://community.vcvrack.com/)

### Contact

- **Email**: [contact@shortwavlabs.com](mailto:contact@shortwavlabs.com)
- **Support**: [Ko-fi](https://ko-fi.com/shortwavlabs)

---

**Can't find your question?** Open a [discussion](https://github.com/shortwavlabs/swv-guitar-collection/discussions) or send us an email!
