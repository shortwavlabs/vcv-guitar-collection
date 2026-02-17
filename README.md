# Guitar Tools for VCV Rack

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Version](https://img.shields.io/badge/version-2.0.0-green.svg)](https://github.com/shortwavlabs/swv-guitar-collection/releases)
[![VCV Rack](https://img.shields.io/badge/VCV%20Rack-2.6+-orange.svg)](https://vcvrack.com/)

A professional guitar processing plugin collection for VCV Rack 2, featuring state-of-the-art neural amp modeling and convolution-based cabinet simulation. Transform your virtual guitar rig with authentic amp tones and speaker cabinet responses.

## 🎸 Overview

**Guitar Tools** by Shortwav Labs brings professional guitar amp and cabinet modeling to VCV Rack using cutting-edge technology:

- **NAM Player**: Real-time neural network inference for highly accurate guitar amplifier, pedal, and preamp emulation using [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler) technology
- **Cabinet Simulator**: Dual-slot convolution-based cabinet simulator with impulse response (IR) loading, blending, and advanced tone shaping

Whether you're creating guitar-driven patches, processing recorded guitars, or building complete virtual guitar rigs, Guitar Tools provides the essential building blocks for authentic guitar tone.

## ✨ Key Features

### NAM Player Module
- **Neural Amp Modeling**: Load `.nam` model files for authentic amplifier, pedal, and preamp emulation
- **Extensive Model Library**: 30+ bundled preset models covering classic and modern amplifiers
- **Real-time Processing**: Optimized neural network inference with automatic sample rate conversion
- **Integrated Signal Chain**:
  - Precision input gain control (-24dB to +24dB)
  - Adjustable noise gate with threshold, attack, release, and hold parameters
  - 5-band EQ (Bass, Middle, Treble, Presence, Depth)
  - Output level control (-24dB to +24dB)
- **Visual Feedback**: Output waveform display with customizable colors
- **Easy Model Management**: Browse bundled presets or load custom models

### Cabinet Simulator Module
- **Dual IR Slots**: Load and blend two impulse responses simultaneously
- **Flexible Routing**: Mix between IR A and IR B with smooth crossfading
- **Tone Shaping Filters**: Dedicated lowpass and highpass filters for speaker voicing
- **Automatic Normalization**: Optional normalization for consistent levels
- **Wide Format Support**: Loads WAV, AIFF, and FLAC impulse responses
- **Efficient Convolution**: Optimized FFT-based convolution engine

## 📦 Installation

1. Go to [VCV Rack's Library](https://library.vcvrack.com/)
3. Search for "Guitar Tools" or "Shortwav Labs"
4. Click "Subscribe" to install

### Option 3: Build from Source

See the [Building from Source](#building-from-source) section below for detailed instructions.

## 🚀 Quick Start

### Using NAM Player

1. Add **NAM Player** to your patch from the module browser
2. Connect your guitar/audio source to the `IN` input
3. Open the contextual menu to browse bundled models
4. Adjust `INPUT` gain to drive the amp model (watch for clipping)
5. Use the noise gate knob to reduce background noise
6. Shape your tone with the 5-band EQ
7. Set `OUTPUT` level to taste
8. Connect `OUT` to your mixer or Cabinet Simulator

### Using Cabinet Simulator

1. Add **Cabinet Simulator** to your patch (typically after NAM Player)
2. Right-click the module to load impulse responses into slot A and/or B
3. Use the `BLEND` knob to mix between the two IRs
4. Adjust `LOWPASS` and `HIGHPASS` filters for tone shaping
5. Set `OUTPUT` level for proper gain staging

For detailed instructions, see the [Quickstart Guide](manual/quickstart.md).

## 📚 Documentation

Comprehensive documentation is available in the [manual](manual/) directory:

- **[Quickstart Guide](manual/quickstart.md)** - Get up and running quickly
- **[Advanced Usage](manual/advanced-usage.md)** - Performance optimization, best practices, and advanced techniques
- **[API Reference](manual/api-reference.md)** - Complete technical documentation for developers
- **[FAQ](manual/faq.md)** - Common questions and troubleshooting
- **[Examples](manual/examples/)** - Real-world patch examples and use cases

## 🎛️ Module Reference

### NAM Player

**Inputs & Outputs:**
- `IN` - Audio input (mono)
- `OUT` - Processed audio output (mono)

**Controls:**
- `INPUT` - Input gain (-24dB to +24dB)
- `OUTPUT` - Output level (-24dB to +24dB)
- Noise Gate: `THRESHOLD`, `ATTACK`, `RELEASE`, `HOLD`
- EQ: `BASS`, `MIDDLE`, `TREBLE`, `PRESENCE`, `DEPTH`

**Indicators:**
- Green light: Model loaded successfully
- Yellow light: Sample rate mismatch (automatic conversion active)
- Gate light: Shows gate open/close status

### Cabinet Simulator

**Inputs & Outputs:**
- `IN` - Audio input (mono)
- `OUT` - Processed audio output (mono)

**Controls:**
- `BLEND` - Mix between IR A (left) and IR B (right)
- `LOWPASS` - High-frequency roll-off (speaker resonance)
- `HIGHPASS` - Low-frequency roll-off (cabinet thump)
- `OUTPUT` - Output level

**Context Menu:**
- Load IR to slot A/B
- Unload IR from slot A/B
- Enable/disable normalization per slot

## 🔧 System Requirements

- **VCV Rack**: Version 2.6.0 or later
- **Operating Systems**:
  - macOS 10.15 (Catalina) or later (Intel and Apple Silicon)
  - Windows 10 or later (64-bit)
  - Linux (64-bit, glibc 2.27+)
- **CPU**: Modern x86_64 or ARM64 processor with SIMD support
- **RAM**: Base plugin ~10MB + ~50-200MB per loaded NAM model (varies by model complexity)
- **Storage**: ~2GB for bundled models and IRs

### Performance Notes

- NAM Player: CPU usage varies by model complexity (typically 3-15% per voice on modern CPUs)
- Cabinet Simulator: Minimal CPU overhead with optimized FFT convolution (~1-2% per voice)
- Both modules support polyphonic operation when used in polyphonic patches

## 🏗️ Building from Source

### Prerequisites

**All Platforms:**
- Git
- VCV Rack SDK 2.6.x

**Platform-specific:**
- **macOS**: Xcode Command Line Tools (`xcode-select --install`)
- **Windows**: MSYS2 with MinGW-w64 or Visual Studio Build Tools
- **Linux**: GCC 9+ or Clang 10+, make, standard development libraries

### Build Steps

1. **Clone the repository:**
   ```bash
   git clone --recursive https://github.com/shortwavlabs/swv-guitar-collection.git
   cd swv-guitar-collection
   ```

   If you already cloned without `--recursive`:
   ```bash
   git submodule update --init --recursive
   ```

2. **Install dependencies and SDK:**
   ```bash
   ./install.sh
   ```

3. **Build the plugin:**
   ```bash
   make -j$(nproc)           # Linux
   make -j$(sysctl -n hw.ncpu)  # macOS
   make -j%NUMBER_OF_PROCESSORS%  # Windows (MSYS2)
   ```

4. **Install to VCV Rack:**
   ```bash
   make install
   ```

### Running Tests

Unit tests are available to verify the DSP components:

```bash
# Run basic tests
./run_tests.sh

# Run tests with code coverage analysis
./run_tests_with_coverage.sh
```

The coverage script will display per-file and overall coverage statistics after running all tests.

For more detailed build instructions and troubleshooting, see [CONTRIBUTING.md](CONTRIBUTING.md).

## 🤝 Contributing

We welcome contributions! Whether it's bug reports, feature requests, documentation improvements, or code contributions, please see our [Contributing Guidelines](CONTRIBUTING.md).

## 📜 License

This plugin is licensed under the GNU General Public License v3.0 or later. See [LICENSE.md](LICENSE.md) for details.

**Third-party Components:**
- Bundled NAM models - Licensed under their respective terms (see model metadata)

## 🙏 Acknowledgments

- Steven Atkinson for [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler)
- VCV Rack community for feedback and testing
- Model creators for the bundled presets

## 💬 Support

- **Issues**: [GitHub Issue Tracker](https://github.com/shortwavlabs/swv-guitar-collection/issues)
- **Email**: contact@shortwavlabs.com
- **Donations**: [Ko-fi](https://ko-fi.com/shortwavlabs)

## 📝 Changelog

See [CHANGELOG.md](CHANGELOG.md) for version history and release notes.

---

**Made with ❤️ by [Shortwav Labs](https://shortwavlabs.com)**

5. **Launch VCV Rack** and find "Guitar Tools" in the module browser.

#### Troubleshooting Build Issues

- **Missing Rack SDK errors**: Ensure `dep/Rack-SDK` is present and your include paths point to `dep/Rack-SDK/include` and `dep/Rack-SDK/dep/include`.
- **C++17 errors**: This plugin requires C++17. Ensure your compiler supports it (GCC 9+, Clang 10+, MSVC 2019+).
- **Symbol not found errors**: Run `make clean && make` to rebuild from scratch.

## Bundled Models

This plugin includes a curated collection of `.nam` models in the `res/models/` directory, featuring captures from various contributors:

- **George B** - Ceriatone King Kong series
- **Helga B** - 5150, 6505+, JSX, and more
- **Tim R** - JCM, Splawn, Magnatone, Fender series
- **Tudor N** - Various drive pedals and Suhr captures
- **And more...**

See [res/models/README.md](res/models/README.md) for full credits and model descriptions.

## Loading Custom Models

1. Click the `Load custom model` option in the contextual menu
2. Navigate to your `.nam` file
3. Select and open the file
4. The model name will appear in the menu when loaded successfully

NAM models can be found at:
- [ToneHunt](https://tonehunt.org/) - Community model repository
- Create your own using [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler)

## Technical Details

### Dependencies

- **nam_rack** (in-tree) - Core NAM inference engine
- **VCV Rack SDK** 2.6.x - Plugin framework

### Audio Processing

- Models run internally at 48kHz sample rate
- Automatic sample rate conversion using VCV Rack's Speex-based resampler
- Processing latency: ~1ms at 48kHz (depends on model architecture)
- Supported model architectures: WaveNet, LSTM, ConvNet

## License

This plugin is licensed under the [GPL-3.0 License](LICENSE).

The bundled NAM models retain their original licenses as specified by their creators.

## Credits

- **Neural Amp Modeler** by Steven Atkinson ([@sdatkinson](https://github.com/sdatkinson))
- **VCV Rack** by VCV/Andrew Belt
- **Model Contributors**: See bundled models section above

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

For model contributions, please ensure you have the rights to distribute the captures and include appropriate attribution.