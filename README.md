# VCV Guitar Collection

A VCV Rack 2 plugin featuring neural amp modeling capabilities using the [Neural Amp Modeler (NAM)](https://github.com/sdatkinson/neural-amp-modeler) technology.

## Modules

### NAM Player

A neural amp modeler player that loads `.nam` model files to emulate guitar amplifiers, pedals, and other audio gear with stunning accuracy.

**Features:**
- Load any NAM-compatible `.nam` model file
- Browse and select from bundled preset models
- Real-time neural network inference
- Input gain control (-24dB to +24dB)
- Output level control (-24dB to +24dB)
- Built-in noise gate with adjustable threshold
- Automatic sample rate conversion (models run at 48kHz internally)
- Visual feedback for model loading status

**Inputs/Outputs:**
- `IN` - Audio input (mono)
- `OUT` - Processed audio output (mono)

**Controls:**
- `INPUT` knob - Adjust input gain before processing
- `OUTPUT` knob - Adjust output level after processing  
- `GATE` knob - Noise gate threshold (off when fully counter-clockwise)
- `LOAD` button - Open file browser to load a `.nam` model
- `<` / `>` buttons - Navigate through bundled preset models
- Model display - Shows the currently loaded model name

## System Requirements

- **VCV Rack**: Version 2.6.x or later
- **Operating System**: 
  - macOS 10.15 (Catalina) or later (Intel and Apple Silicon)
  - Windows 10 or later (64-bit)
  - Linux (64-bit, glibc 2.27+)
- **CPU**: Modern x86_64 or ARM64 processor with SIMD support
- **RAM**: Additional ~50-200MB per loaded NAM model (varies by model complexity)

## Installation

### From Release Package

1. Download the latest `.vcvplugin` file for your platform from the [Releases](../../releases) page
2. Double-click the downloaded file, or drag it onto the VCV Rack window
3. Restart VCV Rack if prompted

### From Source

#### Prerequisites

- **macOS**: Xcode Command Line Tools (`xcode-select --install`)
- **Windows**: MSYS2 with MinGW-w64, or Visual Studio Build Tools
- **Linux**: GCC 9+ or Clang 10+, make, and standard development libraries

All platforms require:
- Git (with submodule support)
- VCV Rack SDK 2.6.x (automatically downloaded during build)

#### Build Instructions

1. **Clone the repository with submodules:**
   ```bash
   git clone --recursive https://github.com/shortwavlabs/swv-guitar-collection.git
   cd swv-guitar-collection
   ```

   If you already cloned without `--recursive`, initialize submodules:
   ```bash
   git submodule update --init --recursive
   ```

2. **Download the VCV Rack SDK** (if not already present):
   ```bash
   # The Makefile expects the SDK at dep/Rack-SDK
   # Download from https://vcvrack.com/downloads/ and extract to dep/Rack-SDK
   # Or use the helper script:
   ./install.sh
   ```

3. **Build the plugin:**
   ```bash
   make -j$(nproc)   # Linux
   make -j$(sysctl -n hw.ncpu)  # macOS
   make -j%NUMBER_OF_PROCESSORS%  # Windows (MSYS2)
   ```

4. **Install to VCV Rack:**
   ```bash
   make install
   ```

   This copies the plugin to your VCV Rack plugins directory:
   - macOS: `~/Library/Application Support/Rack2/plugins-mac-arm64/` or `plugins-mac-x64/`
   - Windows: `%LOCALAPPDATA%/Rack2/plugins-win-x64/`
   - Linux: `~/.local/share/Rack2/plugins-lin-x64/`

5. **Launch VCV Rack** and find "SWV Guitar Collection" in the module browser.

#### Troubleshooting Build Issues

- **Missing Eigen errors**: The NeuralAmpModelerCore submodule bundles Eigen. Ensure submodules are initialized.
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

1. Click the `LOAD` button on the NAM Player module
2. Navigate to your `.nam` file
3. Select and open the file
4. The model name will appear in the display when loaded successfully

NAM models can be found at:
- [ToneHunt](https://tonehunt.org/) - Community model repository
- [NAM Discord](https://discord.gg/enV3wSDBcf) - Community sharing
- Create your own using [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler)

## Technical Details

### Dependencies

- **NeuralAmpModelerCore** v0.3.0 - Core NAM inference engine
- **Eigen** 3.4 (bundled with NAM Core) - Linear algebra library
- **nlohmann/json** (bundled with NAM Core) - JSON parsing
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