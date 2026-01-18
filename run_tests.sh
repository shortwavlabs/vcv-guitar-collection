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

"$CXX" -std=c++17 -O2 -Wall -Isrc -DSHORTWAV_DSP_RUN_TESTS -o "$OUT_BIN" src/tests/test_swv_guitar_collection.cpp

echo "Running tests..."
if "$OUT_BIN"; then
  echo "Tests passed."
  exit 0
else
  echo "Tests failed."
  exit $?
fi