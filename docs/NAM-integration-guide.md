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

# Eigen alignment options (see Risk Assessment for details)
# Option 1: Safe but slower - disable vectorization
# FLAGS += -DEIGEN_MAX_ALIGN_BYTES=0 -DEIGEN_DONT_VECTORIZE

# Option 2: Performance mode - require proper alignment
# No additional flags needed, but use EIGEN_MAKE_ALIGNED_OPERATOR_NEW in classes

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
│   └── NAMPlayer.svg            # Module panel
├── src/
│   ├── plugin.cpp
│   ├── plugin.hpp
│   └── NAMPlayer.cpp            # NAM module implementation
├── Makefile
└── plugin.json
```

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
// Before including NAM headers, configure Eigen if needed
// #define EIGEN_MAX_ALIGN_BYTES 0
// #define EIGEN_DONT_VECTORIZE

#include "NAM/dsp.h"
#include "NAM/get_dsp.h"

// For faster tanh approximation (recommended for real-time)
#include "NAM/activations.h"

// In initialization:
nam::activations::Activation::enable_fast_tanh();
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

### macOS
- Use `libc++` (already configured in NAMCore CMakeLists)
- Universal binary support may require separate ARM64 and x86_64 builds

### Linux
- Link with `stdc++fs` for filesystem operations
- Use `-static-libstdc++ -static-libgcc` for distribution

### Windows
- Define `NOMINMAX` and `WIN32_LEAN_AND_MEAN`
- May need to handle different MSVC versions
