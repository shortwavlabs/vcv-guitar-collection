# If RACK_DIR is not defined when calling the Makefile, default to Rack-SDK in dep
RACK_DIR ?= dep/Rack-SDK

# NAM Core paths
# NAM_DIR := dep/NeuralAmpModelerCore

# Include paths - use NAM's bundled Eigen (3.4 pre-release with placeholders::lastN support)
# FLAGS += -I$(NAM_DIR)
# FLAGS += -I$(NAM_DIR)/Dependencies/eigen
# FLAGS += -I$(NAM_DIR)/Dependencies/nlohmann

# Eigen configuration - use aligned allocation for performance
# We use proper EIGEN_MAKE_ALIGNED_OPERATOR_NEW in classes containing Eigen types
# Fallback: uncomment these lines if alignment issues occur
# FLAGS += -DEIGEN_MAX_ALIGN_BYTES=0
# FLAGS += -DEIGEN_DONT_VECTORIZE

# Platform-specific flags
# NAM requires macOS 10.15+ for std::filesystem and C++17
# Only set macOS version flag on macOS builds
# ifeq ($(shell uname -s),Darwin)
#     EXTRA_FLAGS := -mmacosx-version-min=10.15 -std=c++17
# else
#     EXTRA_FLAGS := -std=c++17
# endif

# Enable Link Time Optimization for performance
FLAGS += -flto
LDFLAGS += -flto

# Careful about linking to shared libraries, since you can't assume much about the user's environment and library search path.
# Static libraries are fine, but they should be added to this plugin's build system.
LDFLAGS +=

# NAM source files
# NAM_SOURCES := $(NAM_DIR)/NAM/activations.cpp
# NAM_SOURCES += $(NAM_DIR)/NAM/conv1d.cpp
# NAM_SOURCES += $(NAM_DIR)/NAM/convnet.cpp
# NAM_SOURCES += $(NAM_DIR)/NAM/dsp.cpp
# NAM_SOURCES += $(NAM_DIR)/NAM/get_dsp.cpp
# NAM_SOURCES += $(NAM_DIR)/NAM/lstm.cpp
# NAM_SOURCES += $(NAM_DIR)/NAM/ring_buffer.cpp
# NAM_SOURCES += $(NAM_DIR)/NAM/util.cpp
# NAM_SOURCES += $(NAM_DIR)/NAM/wavenet.cpp

# SOURCES += $(NAM_SOURCES)

# Plugin sources
SOURCES += $(wildcard src/*.cpp)
SOURCES += $(wildcard src/dsp/nam_rack/*.cpp)

# Add files to the ZIP package when running `make dist`
# The compiled plugin and "plugin.json" are automatically added.
DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += $(wildcard presets)

# Include the Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk
