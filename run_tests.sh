#!/usr/bin/env bash
set -e

echo "Compiling tests..."

# Allow overriding compiler via CXX, default to g++, fall back to clang++ if needed.
if [ -z "$CXX" ]; then
  if command -v g++ >/dev/null 2>&1; then
    CXX="g++"
  elif command -v clang++ >/dev/null 2>&1; then
    CXX="clang++"
  else
    echo "Error: No suitable C++ compiler found (g++ or clang++ required)." >&2
    exit 1
  fi
fi

# Optional: use ./build if it exists, otherwise current directory.
OUT_DIR="."
if [ -d "./build" ]; then
  OUT_DIR="./build"
fi

OUT_BIN="${OUT_DIR}/build_test_swv_guitar_collection"

# nam_rack source files
NAM_RACK_SOURCES="src/dsp/nam_rack/ring_buffer.cpp"
NAM_RACK_SOURCES="$NAM_RACK_SOURCES src/dsp/nam_rack/conv1d.cpp"
NAM_RACK_SOURCES="$NAM_RACK_SOURCES src/dsp/nam_rack/conv1x1.cpp"
NAM_RACK_SOURCES="$NAM_RACK_SOURCES src/dsp/nam_rack/dsp.cpp"
NAM_RACK_SOURCES="$NAM_RACK_SOURCES src/dsp/nam_rack/linear.cpp"
NAM_RACK_SOURCES="$NAM_RACK_SOURCES src/dsp/nam_rack/convnet.cpp"
NAM_RACK_SOURCES="$NAM_RACK_SOURCES src/dsp/nam_rack/wavenet.cpp"
NAM_RACK_SOURCES="$NAM_RACK_SOURCES src/dsp/nam_rack/lstm.cpp"
NAM_RACK_SOURCES="$NAM_RACK_SOURCES src/dsp/nam_rack/model_loader.cpp"

# Detect OS for platform-specific linking
UNAME_S=$(uname -s)

# Compile with VCV Rack SDK includes
if [[ "$UNAME_S" == "MINGW"* || "$UNAME_S" == "MSYS"* ]]; then
  # Windows: link to libRack.dll.a in SDK root
  "$CXX" -std=c++11 -O2 -Wall \
    -Isrc \
    -Idep/Rack-SDK/include \
    -Idep/Rack-SDK/dep/include \
    -DSHORTWAV_DSP_RUN_TESTS \
    -D_USE_MATH_DEFINES \
    -o "$OUT_BIN" \
    src/tests/test_swv_guitar_collection.cpp \
    $NAM_RACK_SOURCES \
    dep/Rack-SDK/libRack.dll.a
else
  # macOS/Linux: use -L and -l flags
  "$CXX" -std=c++11 -O2 -Wall \
    -Isrc \
    -Idep/Rack-SDK/include \
    -Idep/Rack-SDK/dep/include \
    -DSHORTWAV_DSP_RUN_TESTS \
    -D_USE_MATH_DEFINES \
    -o "$OUT_BIN" \
    src/tests/test_swv_guitar_collection.cpp \
    $NAM_RACK_SOURCES \
    -Ldep/Rack-SDK \
    -lRack \
    -Wl,-rpath,@executable_path/../dep/Rack-SDK
fi

echo "Running tests..."
if [[ "$UNAME_S" == "Darwin" ]]; then
  export DYLD_LIBRARY_PATH="$(pwd)/dep/Rack-SDK:$DYLD_LIBRARY_PATH"
elif [[ "$UNAME_S" == "Linux" ]]; then
  export LD_LIBRARY_PATH="$(pwd)/dep/Rack-SDK:$LD_LIBRARY_PATH"
elif [[ "$UNAME_S" == "MINGW"* || "$UNAME_S" == "MSYS"* ]]; then
  # Windows: add Rack SDK to PATH for DLL loading
  export PATH="$(pwd)/dep/Rack-SDK:$PATH"
fi
if ! "$OUT_BIN"; then
  EXIT_CODE=$?
  echo "Plugin tests failed with exit code $EXIT_CODE"
  exit $EXIT_CODE
fi

echo "Tests passed."
exit 0
