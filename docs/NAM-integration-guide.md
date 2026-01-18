# Neural Amp Modeler Integration Guide for VCV Rack

## Overview

This document outlines how to integrate the NeuralAmpModelerCore library and its dependencies into this VCV Rack 2.x plugin.

## Dependencies

### 1. NeuralAmpModelerCore

**Repository:** https://github.com/sdatkinson/NeuralAmpModelerCore

The core library provides:
- Neural network architectures (WaveNet, LSTM, ConvNet, Linear)
- Model loading from `.nam` files (JSON format with weights)
- DSP processing interface

**Key files:**
- `NAM/dsp.h` - Base DSP class and utilities
- `NAM/get_dsp.h` - Model factory functions
- `NAM/wavenet.h` - WaveNet architecture (most common)
- `NAM/lstm.h` - LSTM architecture
- `NAM/convnet.h` - ConvNet architecture

### 2. Eigen

**Version:** 3.x (header-only library)

Eigen is used extensively for matrix operations in the neural network layers. It's a header-only library which simplifies integration.

**Integration approach:**
- Include Eigen as a header-only dependency in `dep/` folder
- No linking required

### 3. nlohmann/json

**Version:** 3.x (header-only library)

Used for parsing `.nam` model files which are JSON formatted.

**Integration approach:**
- Single header file (`json.hpp`)
- Already bundled with NeuralAmpModelerCore

## Recommended Integration Strategy

### Option A: Git Submodule (Recommended)

```bash
# Add NeuralAmpModelerCore as a submodule
git submodule add https://github.com/sdatkinson/NeuralAmpModelerCore dep/NeuralAmpModelerCore

# Add Eigen as a submodule (or download headers)
git submodule add https://gitlab.com/libeigen/eigen.git dep/eigen
```

**Advantages:**
- Easy to update by pulling latest changes
- Version control of exact dependency versions
- Clean separation from plugin code

**Disadvantages:**
- Users cloning must use `--recursive`
- Submodule management overhead

### Option B: Copy Source Files

Copy the following from NeuralAmpModelerCore:
- `NAM/` directory (all `.cpp` and `.h` files)
- `Dependencies/eigen/` (header files)
- `Dependencies/nlohmann/json.hpp`

**Advantages:**
- Self-contained plugin
- No submodule complexity

**Disadvantages:**
- Manual updates required
- Harder to track upstream changes

## Makefile Integration

Modify the project `Makefile` to include NAM sources:

```makefile
# If RACK_DIR is not defined when calling the Makefile, default to two directories above
RACK_DIR ?= dep/Rack-SDK

# NAM Core integration
NAM_DIR := dep/NeuralAmpModelerCore
EIGEN_DIR := dep/eigen

# Add include paths
FLAGS += -I$(NAM_DIR)
FLAGS += -I$(EIGEN_DIR)
FLAGS += -I$(NAM_DIR)/Dependencies/nlohmann

# C++ standard (NAM requires C++17 or later)
CXXFLAGS += -std=c++17

# Eigen alignment options
# Using aligned allocation for performance (EIGEN_MAKE_ALIGNED_OPERATOR_NEW in classes)
# Fallback: uncomment these lines if alignment issues occur on specific platforms
# FLAGS += -DEIGEN_MAX_ALIGN_BYTES=0 -DEIGEN_DONT_VECTORIZE

# NAM source files
NAM_SOURCES := $(wildcard $(NAM_DIR)/NAM/*.cpp)
SOURCES += $(NAM_SOURCES)

# Add plugin sources
SOURCES += $(wildcard src/*.cpp)

# Add files to the ZIP package when running `make dist`
DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)

# Include the Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk
```

## Directory Structure

Recommended project layout after integration:

```
swv-guitar-collection/
├── dep/
│   ├── Rack-SDK/
│   ├── NeuralAmpModelerCore/    # Submodule
│   │   ├── NAM/
│   │   │   ├── dsp.cpp
│   │   │   ├── dsp.h
│   │   │   ├── wavenet.cpp
│   │   │   ├── wavenet.h
│   │   │   ├── lstm.cpp
│   │   │   ├── lstm.h
│   │   │   ├── convnet.cpp
│   │   │   ├── convnet.h
│   │   │   ├── get_dsp.cpp
│   │   │   ├── get_dsp.h
│   │   │   └── ...
│   │   └── Dependencies/
│   │       └── nlohmann/
│   │           └── json.hpp
│   └── eigen/                    # Submodule or copied headers
│       └── Eigen/
│           ├── Dense
│           ├── Core
│           └── ...
├── docs/
├── res/
│   ├── NamPlayer.svg            # Module panel (21HP, based on SWV_21HP_PANEL.svg)
│   └── models/                   # All bundled NAM models from pelennor2170/NAM_models
│       └── *.nam
├── src/
│   ├── dsp/
│   │   └── Nam.h                 # NAM DSP abstraction (resampling, noise gate, tone stack)
│   ├── plugin.cpp
│   ├── plugin.hpp
│   ├── NamPlayer.hpp            # NAM module header
│   └── NamPlayer.cpp            # NAM module implementation + widget
├── Makefile
└── plugin.json
```

## Bundled Models

The plugin ships with all NAM models from:
https://github.com/pelennor2170/NAM_models

These are placed in `res/models/` and included in the distribution.

**Model Loading:**
- **Submenu:** Right-click menu lists all bundled models for quick selection
- **File picker:** "Load Custom Model..." for user's own `.nam` files
- **Empty state:** When no model is loaded, module acts as passthrough

**Note:** Full model collection bundled initially; may be trimmed if distribution size becomes an issue.

## Updating Dependencies

### With Submodules

```bash
# Update NeuralAmpModelerCore to latest
cd dep/NeuralAmpModelerCore
git pull origin main
cd ../..
git add dep/NeuralAmpModelerCore
git commit -m "Update NeuralAmpModelerCore"

# Update Eigen
cd dep/eigen
git pull origin master
cd ../..
git add dep/eigen
git commit -m "Update Eigen"
```

### With Copied Sources

1. Download new release from GitHub
2. Replace files in `dep/` directory
3. Test thoroughly for API changes
4. Update version tracking in README

## Sample Preprocessor Configuration

When including NAM headers in your module:

```cpp
// In src/dsp/Nam.h - the DSP abstraction layer

// Eigen must be included with proper alignment support
#include <Eigen/Dense>

#include "NAM/dsp.h"
#include "NAM/get_dsp.h"

// For faster tanh approximation (recommended for real-time)
#include "NAM/activations.h"

// Classes containing Eigen types must use this macro
class NamDSP {
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW  // Required for proper Eigen alignment
    
    // ... class members
};

// In initialization:
nam::activations::Activation::enable_fast_tanh();
```

### Eigen Alignment Strategy

We use the aligned allocation approach (EIGEN_MAKE_ALIGNED_OPERATOR_NEW) for performance.
If alignment issues occur on specific platforms, add these flags to the Makefile:

```makefile
# Fallback: disable vectorization if alignment issues occur
FLAGS += -DEIGEN_MAX_ALIGN_BYTES=0 -DEIGEN_DONT_VECTORIZE
```

## Build Verification

After integration, verify:

1. **Compilation succeeds:**
   ```bash
   make clean && make
   ```

2. **No undefined symbols:**
   ```bash
   # On macOS
   nm -u plugin.dylib | grep nam
   
   # On Linux
   nm -u plugin.so | grep nam
   ```

3. **Model loading works:**
   - Load a test `.nam` file
   - Verify no runtime errors

## Platform-Specific Notes

### Target Platform

- **Primary Development:** macOS ARM64 (Apple Silicon)
- **CI/CD:** GitHub Actions builds for macOS, Windows, Linux
- **VCV Rack Version:** 2.6.x and up

### macOS (Primary)
- Use `libc++` (already configured in NAMCore CMakeLists)
- ARM64 (Apple Silicon) is the primary development target
- Universal binary support handled via GitHub Actions CI

### Linux
- Link with `stdc++fs` for filesystem operations if needed
- Use `-static-libstdc++ -static-libgcc` for distribution

### Windows
- Define `NOMINMAX` and `WIN32_LEAN_AND_MEAN`
- May need to handle different MSVC versions
- Built via GitHub Actions CI with MinGW
