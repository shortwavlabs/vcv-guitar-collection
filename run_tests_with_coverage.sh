#!/usr/bin/env bash
set -e

echo "========================================="
echo "Running Tests with Code Coverage"
echo "========================================="
echo ""

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

# Check if gcov or llvm-cov is available
GCOV_TOOL=""
if command -v gcov >/dev/null 2>&1; then
  GCOV_TOOL="gcov"
elif command -v llvm-cov >/dev/null 2>&1; then
  GCOV_TOOL="llvm-cov gcov"
else
  echo "Warning: gcov or llvm-cov not found. Coverage reporting will be limited." >&2
fi

# Create coverage directory
COV_DIR="./coverage"
mkdir -p "$COV_DIR"

# Clean previous coverage data
rm -f "$COV_DIR"/*.gcda "$COV_DIR"/*.gcno "$COV_DIR"/*.gcov
rm -f ./*.gcda ./*.gcno

OUT_DIR="$COV_DIR"
OUT_BIN="${OUT_DIR}/test_with_coverage"

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

# Coverage flags for GCC/Clang
COVERAGE_FLAGS="--coverage -fprofile-arcs -ftest-coverage -O0 -g"

echo "Compiling tests with coverage instrumentation..."

# Compile with VCV Rack SDK includes and coverage flags
if [[ "$UNAME_S" == "MINGW"* || "$UNAME_S" == "MSYS"* ]]; then
  # Windows: link to libRack.dll.a in SDK root
  "$CXX" -std=c++11 -Wall $COVERAGE_FLAGS \
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
  "$CXX" -std=c++11 -Wall $COVERAGE_FLAGS \
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
    -Wl,-rpath,@executable_path/../../dep/Rack-SDK
fi

echo "Compilation successful."
echo ""
echo "Running tests..."
echo ""

# Set library path and run tests
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
  echo ""
  echo "Plugin tests failed with exit code $EXIT_CODE"
  exit $EXIT_CODE
fi

echo ""
echo "========================================="
echo "Running NAM Rack Tests"
echo "========================================="
echo ""

NAM_RACK_BIN="${OUT_DIR}/test_nam_rack_coverage"

# Compile nam_rack tests
"$CXX" -std=c++11 -Wall $COVERAGE_FLAGS \
  -Isrc \
  -Idep/Rack-SDK/include \
  -Idep/Rack-SDK/dep/include \
  -o "$NAM_RACK_BIN" \
  src/tests/test_nam_rack.cpp \
  src/dsp/nam_rack/ring_buffer.cpp \
  src/dsp/nam_rack/conv1d.cpp \
  src/dsp/nam_rack/conv1x1.cpp \
  src/dsp/nam_rack/dsp.cpp \
  src/dsp/nam_rack/linear.cpp \
  src/dsp/nam_rack/convnet.cpp \
  src/dsp/nam_rack/wavenet.cpp \
  src/dsp/nam_rack/lstm.cpp \
  src/dsp/nam_rack/model_loader.cpp \
  -Ldep/Rack-SDK \
  -lRack \
  -ldl \
  -lm \
  -lpthread \
  -Wl,-rpath,@executable_path/../../dep/Rack-SDK

if ! "$NAM_RACK_BIN"; then
  EXIT_CODE=$?
  echo ""
  echo "NAM Rack tests failed with exit code $EXIT_CODE"
  exit $EXIT_CODE
fi

echo ""
echo "========================================="
echo "Generating Coverage Report"
echo "========================================="
echo ""

# Move coverage files to coverage directory
mv ./*.gcda "$COV_DIR/" 2>/dev/null || true
mv ./*.gcno "$COV_DIR/" 2>/dev/null || true

# Generate coverage for the main test file
$GCOV_TOOL -o "$COV_DIR" -l "$COV_DIR"/test_with_coverage-test_swv_guitar_collection.gcno > /dev/null 2>&1 || true

# Generate coverage for nam_rack test file
$GCOV_TOOL -o "$COV_DIR" -l "$COV_DIR"/test_nam_rack_coverage-test_nam_rack.gcno > /dev/null 2>&1 || true

# Move generated .gcov files to coverage directory
mv ./*.gcov "$COV_DIR/" 2>/dev/null || true

if [ -z "$GCOV_TOOL" ]; then
  echo "Coverage tools not available. Skipping detailed coverage report."
  exit 0
fi

echo "Coverage Summary:"
echo "-----------------"
echo ""

# Parse .gcov files and display summary
TOTAL_LINES=0
COVERED_LINES=0

# Look for header files in our source tree
for gcov_file in "$COV_DIR"/*.h.gcov; do
  if [ -f "$gcov_file" ]; then
    filename=$(basename "$gcov_file" .gcov)
    
    # Extract just the filename after ## if present
    display_name=$(echo "$filename" | sed 's/.*##//')
    
    # Only process files from our src/dsp directory
    if ! grep -q "Source:.*src/dsp/" "$gcov_file" 2>/dev/null; then
      continue
    fi
    
    # Count executed lines (lines starting with a number > 0)
    executed=$(grep -E "^[ ]*[1-9][0-9]*:" "$gcov_file" | wc -l | tr -d ' ')
    # Count non-executed lines (lines starting with ####)
    not_executed=$(grep -E "^[ ]*####:" "$gcov_file" | wc -l | tr -d ' ')
    
    total=$((executed + not_executed))
    
    if [ "$total" -gt 0 ]; then
      percentage=$(awk "BEGIN {printf \"%.1f\", ($executed / $total) * 100}")
      printf "%-30s %5d / %5d lines  (%5.1f%%)\n" "$display_name" "$executed" "$total" "$percentage"
      
      TOTAL_LINES=$((TOTAL_LINES + total))
      COVERED_LINES=$((COVERED_LINES + executed))
    fi
  fi
done

echo ""
echo "-----------------"

if [ "$TOTAL_LINES" -gt 0 ]; then
  OVERALL_PERCENTAGE=$(awk "BEGIN {printf \"%.1f\", ($COVERED_LINES / $TOTAL_LINES) * 100}")
  printf "%-30s %5d / %5d lines  (%5.1f%%)\n" "OVERALL COVERAGE" "$COVERED_LINES" "$TOTAL_LINES" "$OVERALL_PERCENTAGE"
else
  echo "No coverage data generated."
fi

echo ""
echo "Detailed coverage reports saved in: $COV_DIR/"
echo ""
echo "To view detailed line-by-line coverage, examine the .gcov files:"
echo "  cat $COV_DIR/*.gcov | less"
echo ""

exit 0
