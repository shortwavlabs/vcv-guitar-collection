# AGENTS.md

Guide for AI agents working in the **swv-guitar-collection** repository - a VCV Rack 2 plugin for guitar amp modeling.

## Project Overview

This is a **VCV Rack 2** plugin called "Guitar Tools" that provides:
- **NAM Player**: Neural network-based guitar amp simulation using NeuralAmpModeler technology
- **Cabinet Simulator**: Convolution-based cabinet simulation with dual IR slots

**Language**: C++17
**License**: GPL-3.0-or-later
**Target**: VCV Rack 2.6.0+

## Essential Commands

```bash
# Build the plugin
make -j$(sysctl -n hw.ncpu)  # macOS
make -j$(nproc)              # Linux

# Build distribution package
./build.sh

# Clean build artifacts
./clean.sh

# Install to VCV Rack (after build)
./install.sh
# Or: make install

# Run unit tests
./run_tests.sh

# Run tests with code coverage
./run_tests_with_coverage.sh
```

## Project Structure

```
swv-guitar-collection/
├── src/
│   ├── plugin.cpp/hpp          # Plugin registration
│   ├── NamPlayer.cpp/hpp       # NAM Player module
│   ├── CabSim.cpp/hpp          # Cabinet Simulator module
│   ├── dsp/
│   │   ├── Nam.h               # NAM DSP wrapper (noise gate, tone stack, resampling)
│   │   ├── CabSimDSP.h         # Cabinet simulation DSP
│   │   ├── IRLoader.h          # Impulse response loader
│   │   ├── WavFile.h           # WAV file reader
│   │   └── nam/                # (planned) Rewritten NAM implementation
│   └── tests/
│       └── test_swv_guitar_collection.cpp
├── dep/
│   ├── Rack-SDK/               # VCV Rack SDK submodule
│   └── NeuralAmpModelerCore/   # NAM library submodule
├── res/
│   ├── models/                 # Bundled .nam model files
│   ├── NAM_PANEL.svg           # Panel graphics
│   └── CABSIM_PANEL.svg
├── docs/                       # Architecture docs
├── manual/                     # User documentation
├── Makefile                    # Build configuration
├── plugin.json                 # Plugin metadata
└── build.sh, clean.sh, install.sh, run_tests.sh
```

## Code Conventions

### Naming

```cpp
// Classes/structs: PascalCase
class NamPlayer : public Module { };
struct BiquadFilter { };

// Functions/methods: camelCase
void loadModel(const std::string& path);
float process(float sample);

// Variables: camelCase
int bufferSize = 128;
std::unique_ptr<NamDSP> namDsp;

// Constants: UPPER_SNAKE_CASE
static constexpr int BLOCK_SIZE = 128;
static constexpr float DEFAULT_THRESHOLD = -60.f;

// Enum values: UPPER_CASE
enum ParamId {
    INPUT_PARAM,
    OUTPUT_PARAM,
    PARAMS_LEN
};

// Private member variables: trailing underscore (optional)
int privateValue_;
```

### Formatting

- **Indentation**: 4 spaces (no tabs)
- **Braces**: Same line for functions and control flow
- **Header guards**: Use `#pragma once`
- **One statement per line**

```cpp
void process(const ProcessArgs& args) {
    if (condition) {
        doSomething();
    }
}
```

### Memory Management

- Use `std::unique_ptr` and `std::make_unique<>` (no raw new/delete)
- Pre-allocate buffers during initialization
- No allocations in audio processing (`process()` methods)

### Thread Safety

- Use `std::atomic<>` for cross-thread communication
- Audio thread must never block
- Model loading happens on background threads

## Key Dependencies

| Dependency | Location | Purpose |
|------------|----------|---------|
| VCV Rack SDK | `dep/Rack-SDK/` | Plugin framework |
| NeuralAmpModelerCore | `dep/NeuralAmpModelerCore/` | Neural network inference |
| Eigen | bundled with NAM | Matrix operations |
| nlohmann/json | bundled with NAM | JSON parsing |

## Module Development Pattern

Each module follows this structure:

```cpp
// ModuleName.hpp
#pragma once
#include "plugin.hpp"

struct ModuleName : Module {
    enum ParamId { ... };
    enum InputId { ... };
    enum OutputId { ... };
    enum LightId { ... };

    // Member variables
    std::unique_ptr<DSPWrapper> dsp;

    ModuleName();
    ~ModuleName();

    void process(const ProcessArgs& args) override;
    void onSampleRateChange(const SampleRateChangeEvent& e) override;

    json_t* dataToJson() override;
    void dataFromJson(json_t* rootJ) override;
};

struct ModuleNameWidget : ModuleWidget {
    ModuleNameWidget(ModuleName* module);
    void appendContextMenu(Menu* menu) override;
};
```

## DSP Components

### NamDSP (src/dsp/Nam.h)

The main DSP wrapper handles:
1. **Noise Gate** - Before NAM processing (hysteresis-based)
2. **NAM Model** - Neural network inference
3. **Tone Stack** - 5-band EQ after NAM (Depth, Bass, Middle, Treble, Presence)
4. **Sample Rate Conversion** - Automatic resampling to model's expected rate

Key classes:
- `BiquadFilter` - IIR filter for tone stack
- `NoiseGate` - Hysteresis-based gate with RMS detection
- `ToneStack` - 5-band parametric EQ
- `NamDSP` - Main wrapper coordinating all components

### CabSimDSP (src/dsp/CabSimDSP.h)

Dual-slot cabinet simulation:
- Two IR slots with crossfade blending
- Lowpass/highpass filters for tone shaping
- FFT-based convolution
- Optional normalization per slot

## Testing

Tests are in `src/tests/test_swv_guitar_collection.cpp`.

Test pattern:
```cpp
void test_feature(TestContext &ctx) {
    std::printf("Testing feature...\n");

    // Setup
    DSPComponent dsp;
    dsp.setSampleRate(48000.0);

    // Test
    T_ASSERT(ctx, condition);
    T_ASSERT_NEAR(ctx, actual, expected, tolerance);
}
```

Run tests:
```bash
./run_tests.sh              # Basic run
./run_tests_with_coverage.sh  # With coverage report
```

## Important Gotchas

### Eigen Memory Alignment

Classes containing Eigen types must use the alignment macro:
```cpp
class NamDSP {
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    // ...
};
```

### Sample Rate Handling

- NAM models expect 48kHz
- Automatic resampling via `rack::dsp::SampleRateConverter`
- `onSampleRateChange()` must propagate rate to DSP components

### Model Loading

- Async loading on background thread
- Use `std::atomic` flags for state (`isLoading`, `hasPendingDsp`)
- Swap pointers atomically in `process()`

### Platform Differences

```cpp
// macOS needs special handling
ifeq ($(shell uname -s),Darwin)
    EXTRA_FLAGS := -mmacosx-version-min=10.15 -std=c++17
else
    EXTRA_FLAGS := -std=c++17
endif
```

## File Loading

NAM models (`.nam`) and IR files (`.wav`, `.aiff`, `.flac`) are loaded via:
- VCV Rack's file dialogs
- `rack::system` utilities for path operations
- Custom WAV/AIFF parser in `src/dsp/WavFile.h`

## Documentation

- User manual: `manual/` directory
- Architecture docs: `docs/` directory
- NAM rewrite plan: `docs/NAM-rewrite-plan.md` (planned C++11 port)

## Debugging

```cpp
// Use VCV Rack's logging
DEBUG("Loading model: %s", path.c_str());
INFO("Model loaded successfully");
WARN("Sample rate mismatch");
```

Run VCV Rack in dev mode for extra logging:
```bash
/path/to/Rack -d
```

## Makefile Configuration

The Makefile uses VCV Rack's plugin build system:
- `RACK_DIR` - Path to Rack SDK (defaults to `dep/Rack-SDK`)
- `SOURCES` - Source files to compile
- `DISTRIBUTABLES` - Files to include in distribution

NAM Core sources are manually listed and compiled with the plugin.

## When Adding New Features

1. Update `plugin.json` for new modules
2. Create panel SVG in `res/`
3. Follow existing patterns in `NamPlayer.cpp` or `CabSim.cpp`
4. Add tests to `test_swv_guitar_collection.cpp`
5. Update documentation in `manual/`
6. Run `./run_tests.sh` before committing
