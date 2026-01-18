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

# NAM paths
NAM_DIR="dep/NeuralAmpModelerCore"

# NAM source files needed for tests
NAM_SOURCES="$NAM_DIR/NAM/activations.cpp"
NAM_SOURCES="$NAM_SOURCES $NAM_DIR/NAM/conv1d.cpp"
NAM_SOURCES="$NAM_SOURCES $NAM_DIR/NAM/convnet.cpp"
NAM_SOURCES="$NAM_SOURCES $NAM_DIR/NAM/dsp.cpp"
NAM_SOURCES="$NAM_SOURCES $NAM_DIR/NAM/get_dsp.cpp"
NAM_SOURCES="$NAM_SOURCES $NAM_DIR/NAM/lstm.cpp"
NAM_SOURCES="$NAM_SOURCES $NAM_DIR/NAM/ring_buffer.cpp"
NAM_SOURCES="$NAM_SOURCES $NAM_DIR/NAM/util.cpp"
NAM_SOURCES="$NAM_SOURCES $NAM_DIR/NAM/wavenet.cpp"

# Detect OS for platform-specific linking
UNAME_S=$(uname -s)

# Compile with NAM includes and VCV Rack SDK includes
if [[ "$UNAME_S" == "MINGW"* || "$UNAME_S" == "MSYS"* ]]; then
  # Windows: link to libRack.dll.a in SDK root
  "$CXX" -std=c++17 -O2 -Wall \
    -Isrc \
    -I"$NAM_DIR" \
    -I"$NAM_DIR/Dependencies/eigen" \
    -I"$NAM_DIR/Dependencies/nlohmann" \
    -Idep/Rack-SDK/include \
    -Idep/Rack-SDK/dep/include \
    -DSHORTWAV_DSP_RUN_TESTS \
    -D_USE_MATH_DEFINES \
    -o "$OUT_BIN" \
    src/tests/test_swv_guitar_collection.cpp \
    $NAM_SOURCES \
    dep/Rack-SDK/libRack.dll.a
else
  # macOS/Linux: use -L and -l flags
  "$CXX" -std=c++17 -O2 -Wall \
    -Isrc \
    -I"$NAM_DIR" \
    -I"$NAM_DIR/Dependencies/eigen" \
    -I"$NAM_DIR/Dependencies/nlohmann" \
    -Idep/Rack-SDK/include \
    -Idep/Rack-SDK/dep/include \
    -DSHORTWAV_DSP_RUN_TESTS \
    -D_USE_MATH_DEFINES \
    -o "$OUT_BIN" \
    src/tests/test_swv_guitar_collection.cpp \
    $NAM_SOURCES \
    -Ldep/Rack-SDK \
    -lRack \
    -Wl,-rpath,@executable_path/../dep/Rack-SDK
fi

echo "Running tests..."
if [[ "$UNAME_S" == "Darwin" ]]; then
  export DYLD_LIBRARY_PATH="$(pwd)/dep/Rack-SDK:$DYLD_LIBRARY_PATH"
fi
if "$OUT_BIN"; then
  echo "Tests passed."
  exit 0
else
  EXIT_CODE=$?
  echo "Tests failed with exit code $EXIT_CODE"
  exit $EXIT_CODE
fi