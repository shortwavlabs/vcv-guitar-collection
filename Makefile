# If RACK_DIR is not defined when calling the Makefile, default to Rack-SDK in dep
RACK_DIR ?= dep/Rack-SDK

# Enable Link Time Optimization for performance
FLAGS += -flto
LDFLAGS += -flto

# Careful about linking to shared libraries, since you can't assume much about the user's environment and library search path.
# Static libraries are fine, but they should be added to this plugin's build system.
LDFLAGS +=

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
