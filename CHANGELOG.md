# Changelog

All notable changes to Guitar Tools will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned Features
- Additional bundled NAM models
- Preset management system
- MIDI CC control for parameters
- Stereo IR support in Cabinet Simulator
- Extended EQ controls

---

## [2.0.0] - 2026-01-19

🎉 **Initial Release**

Guitar Tools brings professional guitar amp modeling and cabinet simulation to VCV Rack using Neural Amp Modeler technology and convolution-based IR processing.

### 🎸 NAM Player Module

**Neural Amp Modeling:**
- Load `.nam` model files for authentic amplifier, pedal, and preamp emulation
- Real-time neural network inference with automatic sample rate conversion
- Asynchronous model loading prevents audio dropouts

**30+ Bundled Models:**
- 5150/6505 series (high-gain)
- Ceriatone King Kong (British tones)
- V4 Countess (vintage warmth)
- Easy preset navigation with `<` / `>` buttons

**Signal Processing:**
- Input gain control: -24dB to +24dB
- Output level control: -24dB to +24dB
- Integrated noise gate with threshold, attack, release, and hold parameters
- Visual gate status indicator

**5-Band EQ:**
- Bass (120 Hz) - Shelving filter
- Depth (90 Hz) - Low-end control
- Middle (700 Hz) - Parametric filter
- Treble (2.5 kHz) - Parametric filter
- Presence (5 kHz) - High-end detail
- ±12dB range per band

**Visual Feedback:**
- Real-time output waveform display
- Multiple color schemes: Green, Blue, Amber, Red, Purple, White
- Model loading indicator
- Sample rate mismatch warning

### 🔊 Cabinet Simulator Module

**Dual IR Processing:**
- Load and blend two impulse responses simultaneously
- Smooth crossfading blend control
- Per-slot normalization option
- Visual indicators for loaded IRs

**Format Support:**
- WAV (16/24/32-bit PCM, 32-bit float)
- AIFF (16/24/32-bit)
- FLAC (16/24-bit)
- Automatic sample rate conversion

**Tone Shaping:**
- Lowpass filter: 1kHz - 20kHz (speaker resonance control)
- Highpass filter: 20Hz - 500Hz (cabinet thump control)
- Output level control: -24dB to +24dB

**Performance:**
- Efficient FFT-based convolution engine
- Optimized for long IRs (up to 10 seconds)
- Minimal CPU overhead

### 🛠️ Technical Features

**Audio Processing:**
- Integration with NeuralAmpModelerCore library
- Multi-threaded model and IR loading
- Block processing (128 samples) for efficiency
- SIMD-optimized DSP algorithms
- High-quality resampling for any sample rate

**Platform Support:**
- macOS: 10.15+ (Intel and Apple Silicon native)
- Windows: 10+ (64-bit)
- Linux: glibc 2.27+ (Ubuntu 20.04+)
- VCV Rack 2.6.0+ required

**Thread Safety:**
- Non-blocking file I/O
- Atomic state updates
- Safe parameter automation

### 📚 Documentation

Complete documentation suite included:
- Comprehensive quickstart guide
- Advanced usage and optimization guide
- Full API reference for developers
- FAQ and troubleshooting
- Real-world example patches
- Contributing guidelines

### 📊 Performance Benchmarks

**CPU Usage (Intel i7-9750H @ 48kHz, buffer 256):**
- NAM Player with Linear model: ~2-3% per voice
- NAM Player with LSTM standard: ~11-12% per voice
- NAM Player with WaveNet: ~15-20% per voice
- Cabinet Simulator (dual IR): ~2% per voice

**Memory Usage:**
- Base plugin: ~10 MB
- Per NAM model: 50-200 MB (varies by architecture)
- Per IR (2 seconds @ 48kHz): ~0.4 MB

**Latency:**
- Processing: <1ms (block size dependent)
- Model loading: <2 seconds (typical LSTM model)
- IR loading: <1 second (typical 2-second IR)

### 🎯 System Requirements

**Minimum:**
- VCV Rack 2.6.0+
- 4GB RAM
- Modern CPU with SSE2 support

**Recommended:**
- VCV Rack 2.6.0+
- 8GB+ RAM
- Modern CPU with AVX2 support
- SSD for fast model loading

---

## Known Issues

**Version 2.0.0:**

- Very large models (>500MB) may take longer to load on HDDs
  - **Workaround:** Use SSD storage for model files
- Polyphonic processing with >8 channels may be CPU-intensive on older systems
  - **Workaround:** Use lighter model architectures or reduce polyphony
- Some Linux distributions: File dialogs may not remember last directory
  - **Workaround:** Bookmark commonly used directories

See the [Issue Tracker](https://github.com/shortwavlabs/swv-guitar-collection/issues) for the complete list and to report new issues.

---

## Credits

**Lead Developer:**
- Stephane Pericat - Architecture, implementation, DSP

**Special Thanks:**
- Steven Atkinson - Neural Amp Modeler Core library
- VCV Rack community - Feature requests and feedback
- NAM community - Model contributions and support
- Beta testers - Testing and bug reports
- Model creators - Bundled amp models (see individual credits)

---

## Release Files

**Version 2.0.0 Downloads:**

- `swv-guitar-collection-2.0.0-mac-arm64.vcvplugin` (macOS Apple Silicon)
- `swv-guitar-collection-2.0.0-mac-x64.vcvplugin` (macOS Intel)
- `swv-guitar-collection-2.0.0-win-x64.vcvplugin` (Windows 64-bit)
- `swv-guitar-collection-2.0.0-lin-x64.vcvplugin` (Linux 64-bit)

SHA256 checksums will be provided with the release.

---

## Support & Feedback

**Get Help:**
- Documentation: [manual/](manual/)
- FAQ: [manual/faq.md](manual/faq.md)
- Issues: [GitHub Issues](https://github.com/shortwavlabs/swv-guitar-collection/issues)
- Discussions: [GitHub Discussions](https://github.com/shortwavlabs/swv-guitar-collection/discussions)
- Email: contact@shortwavlabs.com

**Support Development:**
- Ko-fi: https://ko-fi.com/shortwavlabs
- Star on GitHub: https://github.com/shortwavlabs/swv-guitar-collection

---

## License

Guitar Tools is free and open source software licensed under GPL-3.0-or-later.

Third-party components:
- Neural Amp Modeler Core: Apache 2.0 License
- Bundled NAM models: Various licenses (see individual model credits)

---

[unreleased]: https://github.com/shortwavlabs/swv-guitar-collection/compare/v2.0.0...HEAD
[2.0.0]: https://github.com/shortwavlabs/swv-guitar-collection/releases/tag/v2.0.0
