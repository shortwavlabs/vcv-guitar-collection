#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

# Allow overriding compiler via CXX, default to g++, fall back to clang++.
if [ -z "${CXX:-}" ]; then
  if command -v g++ >/dev/null 2>&1; then
    CXX="g++"
  elif command -v clang++ >/dev/null 2>&1; then
    CXX="clang++"
  else
    echo "Error: No suitable C++ compiler found (g++ or clang++ required)." >&2
    exit 1
  fi
fi

OUT_DIR="./build"
mkdir -p "$OUT_DIR"
mkdir -p "./docs/perf"

OUT_BIN="${OUT_DIR}/benchmark_nam_models"

BLOCK_SIZE="${BLOCK_SIZE:-128}"
WARMUP_BLOCKS="${WARMUP_BLOCKS:-250}"
MEASURE_BLOCKS="${MEASURE_BLOCKS:-1200}"
SAMPLE_RATES="${SAMPLE_RATES:-44100,48000}"
MODELS_DIR="${MODELS_DIR:-res/models}"

LATEST_JSON="docs/perf/perf-baseline-latest.json"
LATEST_CSV="docs/perf/perf-baseline-latest.csv"
STAMP="$(date -u +"%Y%m%d-%H%M%S")"
ARCHIVE_JSON="docs/perf/perf-baseline-${STAMP}.json"
ARCHIVE_CSV="docs/perf/perf-baseline-${STAMP}.csv"

NAM_RACK_SOURCES=(
  src/dsp/nam_rack/ring_buffer.cpp
  src/dsp/nam_rack/conv1d.cpp
  src/dsp/nam_rack/conv1x1.cpp
  src/dsp/nam_rack/dsp.cpp
  src/dsp/nam_rack/linear.cpp
  src/dsp/nam_rack/convnet.cpp
  src/dsp/nam_rack/wavenet.cpp
  src/dsp/nam_rack/lstm.cpp
  src/dsp/nam_rack/model_loader.cpp
)

UNAME_S="$(uname -s)"

echo "Compiling performance benchmark harness..."

if [[ "$UNAME_S" == "MINGW"* || "$UNAME_S" == "MSYS"* ]]; then
  "$CXX" -std=c++17 -O2 -Wall \
    -Isrc \
    -Idep/Rack-SDK/include \
    -Idep/Rack-SDK/dep/include \
    -DSHORTWAV_DSP_RUN_TESTS \
    -D_USE_MATH_DEFINES \
    -o "$OUT_BIN" \
    src/tests/benchmark_nam_models.cpp \
    "${NAM_RACK_SOURCES[@]}" \
    dep/Rack-SDK/libRack.dll.a
else
  "$CXX" -std=c++17 -O2 -Wall \
    -Isrc \
    -Idep/Rack-SDK/include \
    -Idep/Rack-SDK/dep/include \
    -DSHORTWAV_DSP_RUN_TESTS \
    -D_USE_MATH_DEFINES \
    -o "$OUT_BIN" \
    src/tests/benchmark_nam_models.cpp \
    "${NAM_RACK_SOURCES[@]}" \
    -Ldep/Rack-SDK \
    -lRack \
    -Wl,-rpath,@executable_path/../dep/Rack-SDK
fi

if [[ "$UNAME_S" == "Darwin" ]]; then
  export DYLD_LIBRARY_PATH="$ROOT_DIR/dep/Rack-SDK:${DYLD_LIBRARY_PATH:-}"
elif [[ "$UNAME_S" == "Linux" ]]; then
  export LD_LIBRARY_PATH="$ROOT_DIR/dep/Rack-SDK:${LD_LIBRARY_PATH:-}"
elif [[ "$UNAME_S" == "MINGW"* || "$UNAME_S" == "MSYS"* ]]; then
  export PATH="$ROOT_DIR/dep/Rack-SDK:${PATH:-}"
fi

echo "Running performance benchmarks..."
"$OUT_BIN" \
  --models-dir "$MODELS_DIR" \
  --sample-rates "$SAMPLE_RATES" \
  --block-size "$BLOCK_SIZE" \
  --warmup-blocks "$WARMUP_BLOCKS" \
  --measure-blocks "$MEASURE_BLOCKS" \
  --output-json "$LATEST_JSON" \
  --output-csv "$LATEST_CSV"

cp "$LATEST_JSON" "$ARCHIVE_JSON"
cp "$LATEST_CSV" "$ARCHIVE_CSV"

echo ""
echo "Performance benchmark complete."
echo "Latest JSON:   $LATEST_JSON"
echo "Latest CSV:    $LATEST_CSV"
echo "Archive JSON:  $ARCHIVE_JSON"
echo "Archive CSV:   $ARCHIVE_CSV"
