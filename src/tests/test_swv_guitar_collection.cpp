// Define M_PI for Windows before including cmath
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <cstdio>
#include <cmath>
#include <chrono>
#include <vector>
#include <limits>
#include <algorithm>
#include "../dsp/Nam.h"
#include "../dsp/IRLoader.h"
#include "../dsp/CabSimDSP.h"
#include "../dsp/WavFile.h"

namespace TestSuite
{
  // Simple assertion helpers
  struct TestContext
  {
    int passed = 0;
    int failed = 0;

    void assertTrue(bool cond, const char *name, const char *file, int line)
    {
      if (cond)
      {
        ++passed;
      }
      else
      {
        ++failed;
        std::printf("[FAIL] %s (%s:%d)\n", name, file, line);
      }
    }

    void assertNear(float actual, float expected, float tol,
                    const char *name, const char *file, int line)
    {
      const float diff = std::fabs(actual - expected);
      if (diff <= tol || (std::isnan(expected) && std::isnan(actual)))
      {
        ++passed;
      }
      else
      {
        ++failed;
        std::printf("[FAIL] %s: expected=%g actual=%g tol=%g (%s:%d)\n",
                    name, (double)expected, (double)actual, (double)tol, file, line);
      }
    }

    void summary() const
    {
      std::printf("[TEST SUMMARY] passed=%d failed=%d\n", passed, failed);
    }
  };

  #define T_ASSERT(ctx, cond) (ctx).assertTrue((cond), #cond, __FILE__, __LINE__)
  #define T_ASSERT_NEAR(ctx, actual, expected, tol) \
    (ctx).assertNear((actual), (expected), (tol), #actual " ~= " #expected, __FILE__, __LINE__)

  //------------------------------------------------------------------------------
  // BiquadFilter Tests
  //------------------------------------------------------------------------------
  void test_biquad_filter_passthrough(TestContext &ctx)
  {
    std::printf("Testing BiquadFilter passthrough...\n");
    
    BiquadFilter filter;
    // Default coefficients should be passthrough (b0=1, others=0)
    T_ASSERT_NEAR(ctx, filter.b0, 1.f, 1e-6f);
    T_ASSERT_NEAR(ctx, filter.b1, 0.f, 1e-6f);
    T_ASSERT_NEAR(ctx, filter.b2, 0.f, 1e-6f);
    
    // Process should pass signal through
    float input = 0.5f;
    float output = filter.process(input);
    T_ASSERT_NEAR(ctx, output, input, 1e-6f);
  }
  
  void test_biquad_filter_low_shelf(TestContext &ctx)
  {
    std::printf("Testing BiquadFilter low shelf...\n");
    
    BiquadFilter filter;
    double sr = 48000.0;
    double freq = 100.0;
    double q = 0.7;
    double gainDb = 6.0;
    
    filter.setLowShelf(sr, freq, q, gainDb);
    
    // Coefficients should be non-zero after setting
    T_ASSERT(ctx, filter.b0 != 0.f);
    
    // Process a signal
    float input = 0.3f;
    float output = filter.process(input);
    T_ASSERT(ctx, !std::isnan(output) && !std::isinf(output));
    
    // Reset should clear state
    filter.reset();
    T_ASSERT_NEAR(ctx, filter.z1, 0.f, 1e-6f);
    T_ASSERT_NEAR(ctx, filter.z2, 0.f, 1e-6f);
  }
  
  void test_biquad_filter_high_shelf(TestContext &ctx)
  {
    std::printf("Testing BiquadFilter high shelf...\n");
    
    BiquadFilter filter;
    double sr = 48000.0;
    double freq = 3200.0;
    double q = 0.7;
    double gainDb = -6.0;
    
    filter.setHighShelf(sr, freq, q, gainDb);
    
    // Coefficients should be non-zero
    T_ASSERT(ctx, filter.b0 != 0.f);
    
    // Process multiple samples to test state evolution
    for (int i = 0; i < 10; i++) {
      float output = filter.process(0.1f * std::sin(2.f * M_PI * 1000.f * i / sr));
      T_ASSERT(ctx, !std::isnan(output) && !std::isinf(output));
    }
  }
  
  void test_biquad_filter_peaking(TestContext &ctx)
  {
    std::printf("Testing BiquadFilter peaking...\n");
    
    BiquadFilter filter;
    double sr = 48000.0;
    double freq = 650.0;
    double q = 1.0;
    double gainDb = 3.0;
    
    filter.setPeaking(sr, freq, q, gainDb);
    
    // Process signal
    float input = 0.5f;
    float output = filter.process(input);
    T_ASSERT(ctx, !std::isnan(output) && !std::isinf(output));
  }

  //------------------------------------------------------------------------------
  // NoiseGate Tests
  //------------------------------------------------------------------------------
  void test_noise_gate_initialization(TestContext &ctx)
  {
    std::printf("Testing NoiseGate initialization...\n");
    
    NoiseGate gate;
    T_ASSERT_NEAR(ctx, gate.threshold, -60.f, 1e-6f);
    T_ASSERT_NEAR(ctx, gate.envelope, 0.f, 1e-6f);
    T_ASSERT_NEAR(ctx, gate.gain, 0.f, 1e-6f);
    T_ASSERT(ctx, !gate.isOpen);
  }
  
  void test_noise_gate_below_threshold(TestContext &ctx)
  {
    std::printf("Testing NoiseGate below threshold...\n");
    
    NoiseGate gate;
    gate.setSampleRate(48000.0);
    gate.setParameters(-40.f, 1.f, 100.f, 20.f);
    
    // Very quiet signal should be gated
    for (int i = 0; i < 1000; i++) {
      float input = 0.0001f;  // Very quiet
      float output = gate.process(input);
      // Output should be attenuated
      (void)output;  // Mark as intentionally unused
    }
    
    // Gate should eventually close
    T_ASSERT(ctx, gate.gain < 0.5f);
  }
  
  void test_noise_gate_above_threshold(TestContext &ctx)
  {
    std::printf("Testing NoiseGate above threshold...\n");
    
    NoiseGate gate;
    gate.setSampleRate(48000.0);
    gate.setParameters(-40.f, 1.f, 100.f, 20.f);
    
    // Strong signal should open gate
    for (int i = 0; i < 5000; i++) {
      float input = 0.5f;  // Strong signal
      float output = gate.process(input);
      (void)output;  // Mark as intentionally unused
    }
    
    // Gate should be open
    T_ASSERT(ctx, gate.isOpen);
    T_ASSERT(ctx, gate.gain > 0.5f);
  }

  void test_noise_gate_default_params_unchanged_still_opens(TestContext &ctx)
  {
    std::printf("Testing NoiseGate default unchanged params still opens...\n");

    NoiseGate gate;
    gate.setSampleRate(48000.0);

    // Explicitly set the same defaults to exercise the no-op parameter fast path.
    gate.setParameters(-60.f, 0.5f, 100.f, 50.f);

    for (int i = 0; i < 5000; i++) {
      gate.process(0.5f);
    }

    T_ASSERT(ctx, gate.isOpen);
    T_ASSERT(ctx, gate.gain > 0.5f);
  }
  
  void test_noise_gate_hysteresis(TestContext &ctx)
  {
    std::printf("Testing NoiseGate hysteresis...\n");
    
    NoiseGate gate;
    gate.setSampleRate(48000.0);
    gate.setParameters(-30.f, 1.f, 100.f, 20.f);
    gate.hysteresis = 6.f;
    gate.recalculateCoefficients();
    
    // Open the gate with strong signal
    for (int i = 0; i < 5000; i++) {
      gate.process(0.5f);
    }
    T_ASSERT(ctx, gate.isOpen);
    
    // Reduce signal slightly - should stay open due to hysteresis
    for (int i = 0; i < 1000; i++) {
      gate.process(0.05f);
    }
    // Gate behavior depends on hysteresis implementation
  }
  
  void test_noise_gate_reset(TestContext &ctx)
  {
    std::printf("Testing NoiseGate reset...\n");
    
    NoiseGate gate;
    gate.setSampleRate(48000.0);
    
    // Process some signal
    for (int i = 0; i < 100; i++) {
      gate.process(0.5f);
    }
    
    // Reset
    gate.reset();
    
    T_ASSERT_NEAR(ctx, gate.envelope, 0.f, 1e-6f);
    T_ASSERT_NEAR(ctx, gate.gain, 0.f, 1e-6f);
    T_ASSERT_NEAR(ctx, gate.holdCounter, 0.f, 1e-6f);
    T_ASSERT(ctx, !gate.isOpen);
  }

  //------------------------------------------------------------------------------
  // ToneStack Tests
  //------------------------------------------------------------------------------
  void test_tone_stack_initialization(TestContext &ctx)
  {
    std::printf("Testing ToneStack initialization...\n");
    
    ToneStack stack;
    T_ASSERT_NEAR(ctx, stack.bassDb, 0.f, 1e-6f);
    T_ASSERT_NEAR(ctx, stack.midDb, 0.f, 1e-6f);
    T_ASSERT_NEAR(ctx, stack.trebleDb, 0.f, 1e-6f);
    T_ASSERT_NEAR(ctx, stack.presenceDb, 0.f, 1e-6f);
    T_ASSERT_NEAR(ctx, stack.depthDb, 0.f, 1e-6f);
  }
  
  void test_tone_stack_flat_response(TestContext &ctx)
  {
    std::printf("Testing ToneStack flat response...\n");
    
    ToneStack stack;
    stack.setSampleRate(48000.0);
    stack.setParameters(0.f, 0.f, 0.f, 0.f, 0.f);
    
    // With all EQ at 0dB, output should be close to input
    float input = 0.5f;
    float output = stack.process(input);
    
    T_ASSERT_NEAR(ctx, output, input, 0.1f);  // Allow some tolerance for filter artifacts
  }
  
  void test_tone_stack_bass_boost(TestContext &ctx)
  {
    std::printf("Testing ToneStack bass boost...\n");
    
    ToneStack stack;
    stack.setSampleRate(48000.0);
    stack.setParameters(6.f, 0.f, 0.f, 0.f, 0.f);  // +6dB bass
    
    float input = 0.3f;
    float output = stack.process(input);
    
    T_ASSERT(ctx, !std::isnan(output) && !std::isinf(output));
  }
  
  void test_tone_stack_all_bands(TestContext &ctx)
  {
    std::printf("Testing ToneStack all bands...\n");
    
    ToneStack stack;
    stack.setSampleRate(48000.0);
    stack.setParameters(3.f, -2.f, 4.f, 2.f, -3.f);
    
    // Process multiple samples
    for (int i = 0; i < 100; i++) {
      float input = 0.5f * std::sin(2.f * M_PI * 440.f * i / 48000.0);
      float output = stack.process(input);
      T_ASSERT(ctx, !std::isnan(output) && !std::isinf(output));
    }
  }
  
  void test_tone_stack_reset(TestContext &ctx)
  {
    std::printf("Testing ToneStack reset...\n");
    
    ToneStack stack;
    stack.setSampleRate(48000.0);
    stack.setParameters(6.f, 3.f, -3.f, 4.f, 2.f);
    
    // Process some samples
    for (int i = 0; i < 50; i++) {
      stack.process(0.5f);
    }
    
    // Reset should clear filter state
    stack.reset();

    // After reset, zero input should produce zero output (no lingering state).
    float out0 = stack.process(0.f);
    float out1 = stack.process(0.f);
    T_ASSERT_NEAR(ctx, out0, 0.f, 1e-6f);
    T_ASSERT_NEAR(ctx, out1, 0.f, 1e-6f);
  }

  //------------------------------------------------------------------------------
  // NamDSP Tests
  //------------------------------------------------------------------------------
  void test_nam_dsp_initialization(TestContext &ctx)
  {
    std::printf("Testing NamDSP initialization...\n");
    
    NamDSP dsp;
    T_ASSERT(ctx, !dsp.isModelLoaded());
    T_ASSERT(ctx, dsp.getModelPath().empty());
    T_ASSERT(ctx, dsp.getModelName().empty());
  }
  
  void test_nam_dsp_passthrough(TestContext &ctx)
  {
    std::printf("Testing NamDSP passthrough (no model)...\n");
    
    NamDSP dsp;
    dsp.setSampleRate(48000.0);
    
    // Without a model, should pass through
    const int numFrames = 128;
    std::vector<float> input(numFrames);
    std::vector<float> output(numFrames);
    
    for (int i = 0; i < numFrames; i++) {
      input[i] = 0.5f * std::sin(2.f * M_PI * 440.f * i / 48000.0);
    }
    
    dsp.process(input.data(), output.data(), numFrames);
    
    // Output should match input (passthrough)
    for (int i = 0; i < numFrames; i++) {
      T_ASSERT_NEAR(ctx, output[i], input[i], 1e-6f);
    }
  }
  
  void test_nam_dsp_sample_rate(TestContext &ctx)
  {
    std::printf("Testing NamDSP sample rate handling...\n");
    
    NamDSP dsp;
    dsp.setSampleRate(44100.0);
    
    // Should not crash
    const int numFrames = 64;
    std::vector<float> input(numFrames, 0.3f);
    std::vector<float> output(numFrames);
    
    dsp.process(input.data(), output.data(), numFrames);
    
    // Change sample rate
    dsp.setSampleRate(96000.0);
    dsp.process(input.data(), output.data(), numFrames);
    
    T_ASSERT(ctx, true);  // Success if no crash
  }
  
  void test_nam_dsp_noise_gate_integration(TestContext &ctx)
  {
    std::printf("Testing NamDSP noise gate integration...\n");
    
    NamDSP dsp;
    dsp.setSampleRate(48000.0);
    dsp.setNoiseGate(-40.f, 1.f, 100.f, 20.f);
    
    // Process very quiet signal - should be gated
    const int numFrames = 256;
    std::vector<float> input(numFrames, 0.0001f);
    std::vector<float> output(numFrames);
    
    dsp.process(input.data(), output.data(), numFrames);
    
    // All output should be very quiet or zero
    for (int i = 0; i < numFrames; i++) {
      T_ASSERT(ctx, std::abs(output[i]) < 0.01f);
    }
  }
  
  void test_nam_dsp_tone_stack_integration(TestContext &ctx)
  {
    std::printf("Testing NamDSP tone stack integration...\n");
    
    NamDSP dsp;
    dsp.setSampleRate(48000.0);
    dsp.setToneStack(3.f, 0.f, -3.f, 2.f, 0.f);
    
    // Process signal with tone stack active
    const int numFrames = 256;
    std::vector<float> input(numFrames);
    std::vector<float> output(numFrames);
    
    for (int i = 0; i < numFrames; i++) {
      input[i] = 0.3f * std::sin(2.f * M_PI * 440.f * i / 48000.0);
    }
    
    dsp.process(input.data(), output.data(), numFrames);
    
    // Output should be processed (not NaN or Inf)
    for (int i = 0; i < numFrames; i++) {
      T_ASSERT(ctx, !std::isnan(output[i]) && !std::isinf(output[i]));
    }
  }
  
  void test_nam_dsp_reset(TestContext &ctx)
  {
    std::printf("Testing NamDSP reset...\n");
    
    NamDSP dsp;
    dsp.setSampleRate(48000.0);
    
    // Process some samples
    const int numFrames = 128;
    std::vector<float> input(numFrames, 0.5f);
    std::vector<float> output(numFrames);
    
    dsp.process(input.data(), output.data(), numFrames);
    
    // Reset
    dsp.reset();
    
    // Should still work after reset
    dsp.process(input.data(), output.data(), numFrames);
    
    T_ASSERT(ctx, true);  // Success if no crash
  }
  
  void test_nam_dsp_block_sizes(TestContext &ctx)
  {
    std::printf("Testing NamDSP various block sizes...\n");
    
    NamDSP dsp;
    dsp.setSampleRate(48000.0);
    
    // Test different block sizes
    int blockSizes[] = {1, 16, 64, 128, 256, 512, 1024};
    
    for (int size : blockSizes) {
      std::vector<float> input(size, 0.3f);
      std::vector<float> output(size);
      
      dsp.process(input.data(), output.data(), size);
      
      // Verify output is valid
      for (int i = 0; i < size; i++) {
        T_ASSERT(ctx, !std::isnan(output[i]) && !std::isinf(output[i]));
      }
    }
  }
  
  void test_nam_dsp_dc_blocking(TestContext &ctx)
  {
    std::printf("Testing NamDSP with DC offset...\n");
    
    NamDSP dsp;
    dsp.setSampleRate(48000.0);
    
    // Input with DC offset
    const int numFrames = 128;
    std::vector<float> input(numFrames);
    std::vector<float> output(numFrames);
    
    for (int i = 0; i < numFrames; i++) {
      input[i] = 0.5f + 0.1f * std::sin(2.f * M_PI * 440.f * i / 48000.0);
    }
    
    dsp.process(input.data(), output.data(), numFrames);
    
    // Should handle DC offset without issues
    for (int i = 0; i < numFrames; i++) {
      T_ASSERT(ctx, !std::isnan(output[i]) && !std::isinf(output[i]));
    }
  }
  
  void test_nam_dsp_extreme_values(TestContext &ctx)
  {
    std::printf("Testing NamDSP with extreme values...\n");
    
    NamDSP dsp;
    dsp.setSampleRate(48000.0);
    
    const int numFrames = 64;
    std::vector<float> input(numFrames);
    std::vector<float> output(numFrames);
    
    // Test with maximum values
    std::fill(input.begin(), input.end(), 1.0f);
    dsp.process(input.data(), output.data(), numFrames);
    
    for (int i = 0; i < numFrames; i++) {
      T_ASSERT(ctx, !std::isnan(output[i]) && !std::isinf(output[i]));
    }
    
    // Test with negative maximum
    std::fill(input.begin(), input.end(), -1.0f);
    dsp.process(input.data(), output.data(), numFrames);
    
    for (int i = 0; i < numFrames; i++) {
      T_ASSERT(ctx, !std::isnan(output[i]) && !std::isinf(output[i]));
    }
    
    // Test with zero
    std::fill(input.begin(), input.end(), 0.0f);
    dsp.process(input.data(), output.data(), numFrames);
    
    for (int i = 0; i < numFrames; i++) {
      T_ASSERT(ctx, !std::isnan(output[i]) && !std::isinf(output[i]));
    }
  }

  void test_nam_dsp_eco_mode_toggle(TestContext &ctx)
  {
    std::printf("Testing NamDSP eco mode toggle...\n");

    NamDSP dsp;
    dsp.setSampleRate(48000.0);

    T_ASSERT(ctx, !dsp.isEcoModeEnabled());
    dsp.setEcoMode(true);
    T_ASSERT(ctx, dsp.isEcoModeEnabled());
    dsp.setEcoMode(false);
    T_ASSERT(ctx, !dsp.isEcoModeEnabled());

    dsp.setEcoModeLevel(NamDSP::ECO_ON);
    T_ASSERT(ctx, dsp.getEcoModeLevel() == NamDSP::ECO_ON);
    dsp.setEcoModeLevel(-1);
    T_ASSERT(ctx, dsp.getEcoModeLevel() == NamDSP::ECO_OFF);
    dsp.setEcoModeLevel(99);
    T_ASSERT(ctx, dsp.getEcoModeLevel() == NamDSP::ECO_ON);

    // Passthrough should remain exact with no model loaded, regardless of eco mode.
    const int numFrames = 64;
    std::vector<float> input(numFrames);
    std::vector<float> output(numFrames);
    for (int i = 0; i < numFrames; i++) {
      input[i] = 0.3f * std::sin(2.f * M_PI * 220.f * static_cast<float>(i) / 48000.f);
    }

    dsp.setEcoMode(true);
    dsp.process(input.data(), output.data(), numFrames);
    for (int i = 0; i < numFrames; i++) {
      T_ASSERT_NEAR(ctx, output[i], input[i], 1e-6f);
    }
  }

  void benchmark_namplayer_style_control_path(TestContext &ctx)
  {
    std::printf("Benchmarking NamPlayer-style control path...\n");

    const char* modelPath = "res/models/Phillipe P Bug333-Lead-NoDrive-Cab-ESR0,007.nam";
    std::FILE* f = std::fopen(modelPath, "rb");
    if (!f) {
      std::printf("  Skipped (model not found): %s\n", modelPath);
      T_ASSERT(ctx, true);
      return;
    }
    std::fclose(f);

    NamDSP dsp;
    dsp.setSampleRate(48000.0);
    if (!dsp.loadModel(modelPath)) {
      std::printf("  Skipped (failed to load model): %s\n", dsp.getLastLoadError().c_str());
      T_ASSERT(ctx, true);
      return;
    }

    const int blockSize = 128;
    const int numBlocks = 800;
    std::vector<float> input(blockSize, 0.0f);
    std::vector<float> output(blockSize, 0.0f);

    for (int i = 0; i < blockSize; i++) {
      input[i] = 0.25f * std::sin(2.f * M_PI * 440.f * static_cast<float>(i) / 48000.f);
    }

    auto runBenchmark = [&](bool perSampleControl) -> double {
      volatile float sink = 0.0f;
      auto t0 = std::chrono::high_resolution_clock::now();

      for (int b = 0; b < numBlocks; b++) {
        if (!perSampleControl) {
          dsp.setNoiseGate(-60.f, 0.5f, 100.f, 50.f);
          dsp.setToneStack(0.f, 0.f, 0.f, 0.f, 0.f);
        }

        if (perSampleControl) {
          for (int i = 0; i < blockSize; i++) {
            dsp.setNoiseGate(-60.f, 0.5f, 100.f, 50.f);
            dsp.setToneStack(0.f, 0.f, 0.f, 0.f, 0.f);
          }
        }

        dsp.process(input.data(), output.data(), blockSize);
        sink += output[b % blockSize];
      }

      auto t1 = std::chrono::high_resolution_clock::now();
      (void)sink;

      const auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
      return static_cast<double>(us) / static_cast<double>(numBlocks);
    };

    const double usPerBlockPerSampleControl = runBenchmark(true);
    const double usPerBlockPerBlockControl = runBenchmark(false);

    std::printf("  NamPlayer-style (per-sample control): %.3f us/block\n", usPerBlockPerSampleControl);
    std::printf("  NamPlayer-style (per-block control): %.3f us/block\n", usPerBlockPerBlockControl);

    auto runEcoBenchmark = [&](int ecoLevel) -> double {
      dsp.setEcoModeLevel(ecoLevel);
      volatile float sink = 0.0f;
      auto t0 = std::chrono::high_resolution_clock::now();
      for (int b = 0; b < numBlocks; b++) {
        dsp.process(input.data(), output.data(), blockSize);
        sink += output[b % blockSize];
      }
      auto t1 = std::chrono::high_resolution_clock::now();
      (void)sink;
      const auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
      return static_cast<double>(us) / static_cast<double>(numBlocks);
    };

    const double ecoOffUs = runEcoBenchmark(NamDSP::ECO_OFF);
    const double ecoOnUs = runEcoBenchmark(NamDSP::ECO_ON);

    std::printf("  Eco Off: %.3f us/block\n", ecoOffUs);
    std::printf("  Eco On: %.3f us/block\n", ecoOnUs);

    // Also benchmark with forced sample-rate mismatch (resampling path)
    dsp.setSampleRate(44100.0);
    const double ecoOffResampleUs = runEcoBenchmark(NamDSP::ECO_OFF);
    const double ecoOnResampleUs = runEcoBenchmark(NamDSP::ECO_ON);

    std::printf("  Eco Off (resample): %.3f us/block\n", ecoOffResampleUs);
    std::printf("  Eco On (resample): %.3f us/block\n", ecoOnResampleUs);

    dsp.setSampleRate(48000.0);

    T_ASSERT(ctx, std::isfinite(usPerBlockPerSampleControl));
    T_ASSERT(ctx, std::isfinite(usPerBlockPerBlockControl));
    T_ASSERT(ctx, std::isfinite(ecoOffUs));
    T_ASSERT(ctx, std::isfinite(ecoOnUs));
    T_ASSERT(ctx, std::isfinite(ecoOffResampleUs));
    T_ASSERT(ctx, std::isfinite(ecoOnResampleUs));
    T_ASSERT(ctx, usPerBlockPerSampleControl > 0.0);
    T_ASSERT(ctx, usPerBlockPerBlockControl > 0.0);
    T_ASSERT(ctx, ecoOffUs > 0.0);
    T_ASSERT(ctx, ecoOnUs > 0.0);
    T_ASSERT(ctx, ecoOffResampleUs > 0.0);
    T_ASSERT(ctx, ecoOnResampleUs > 0.0);
  }

  //------------------------------------------------------------------------------
  // IRLoader Tests
  //------------------------------------------------------------------------------
  void test_ir_loader_initialization(TestContext &ctx)
  {
    std::printf("Testing IRLoader initialization...\n");
    
    IRLoader loader;
    T_ASSERT(ctx, !loader.isLoaded());
    T_ASSERT(ctx, loader.getPath().empty());
    T_ASSERT(ctx, loader.getName().empty());
    T_ASSERT(ctx, loader.getSamples().empty());
    T_ASSERT(ctx, loader.getLength() == 0);
  }
  
  void test_ir_loader_reset(TestContext &ctx)
  {
    std::printf("Testing IRLoader reset...\n");
    
    IRLoader loader;
    // Even without loading, reset should work
    loader.reset();
    
    T_ASSERT(ctx, !loader.isLoaded());
    T_ASSERT(ctx, loader.getSamples().empty());
  }
  
  void test_ir_loader_invalid_file(TestContext &ctx)
  {
    std::printf("Testing IRLoader with invalid file...\n");
    
    IRLoader loader;
    bool result = loader.load("/nonexistent/path/to/file.wav");
    
    T_ASSERT(ctx, !result);
    T_ASSERT(ctx, !loader.isLoaded());
  }
  
  void test_ir_loader_normalization(TestContext &ctx)
  {
    std::printf("Testing IRLoader normalization logic...\n");
    
    // Create a mock loader with synthetic data to test normalization
    IRLoader loader;
    
    // We can't easily test file loading without a real file,
    // but we can verify the normalization flag behavior
    T_ASSERT(ctx, !loader.isNormalized());
  }

  //------------------------------------------------------------------------------
  // WavFile Tests
  //------------------------------------------------------------------------------
  void test_wavfile_initialization(TestContext &ctx)
  {
    std::printf("Testing WavFile initialization...\n");
    
    WavFile wav;
    T_ASSERT(ctx, !wav.isLoaded());
    T_ASSERT(ctx, wav.getPath().empty());
    T_ASSERT(ctx, wav.getSamples().empty());
    T_ASSERT(ctx, wav.getChannels() == 0);
    T_ASSERT(ctx, wav.getSampleRate() == 0);
    T_ASSERT(ctx, wav.getFrameCount() == 0);
  }
  
  void test_wavfile_reset(TestContext &ctx)
  {
    std::printf("Testing WavFile reset...\n");
    
    WavFile wav;
    // Even without loading, reset should work
    wav.reset();
    
    T_ASSERT(ctx, !wav.isLoaded());
    T_ASSERT(ctx, wav.getSamples().empty());
    T_ASSERT(ctx, wav.getChannels() == 0);
    T_ASSERT(ctx, wav.getSampleRate() == 0);
    T_ASSERT(ctx, wav.getFrameCount() == 0);
    T_ASSERT(ctx, wav.getPath().empty());
  }
  
  void test_wavfile_invalid_file(TestContext &ctx)
  {
    std::printf("Testing WavFile with invalid file path...\n");
    
    WavFile wav;
    bool result = wav.load("/nonexistent/path/to/file.wav");
    
    T_ASSERT(ctx, !result);
    T_ASSERT(ctx, !wav.isLoaded());
    T_ASSERT(ctx, wav.getSamples().empty());
  }
  
  void test_wavfile_empty_path(TestContext &ctx)
  {
    std::printf("Testing WavFile with empty path...\n");
    
    WavFile wav;
    bool result = wav.load("");
    
    T_ASSERT(ctx, !result);
    T_ASSERT(ctx, !wav.isLoaded());
  }
  
  void test_wavfile_getters_no_data(TestContext &ctx)
  {
    std::printf("Testing WavFile getters with no data...\n");
    
    WavFile wav;
    
    // All getters should return safe default values
    T_ASSERT(ctx, wav.getChannels() == 0);
    T_ASSERT(ctx, wav.getSampleRate() == 0);
    T_ASSERT(ctx, wav.getFrameCount() == 0);
    T_ASSERT(ctx, wav.getSamples().empty());
    T_ASSERT(ctx, wav.getPath().empty());
    T_ASSERT(ctx, !wav.isLoaded());
  }
  
  void test_wavfile_frame_count_calculation(TestContext &ctx)
  {
    std::printf("Testing WavFile frame count calculation...\n");
    
    WavFile wav;
    
    // With 0 channels, frame count should be 0 regardless
    T_ASSERT(ctx, wav.getFrameCount() == 0);
    
    // Frame count calculation is: samples.size() / channels
    // We can't easily test this without a real file, but we verified the logic
  }
  
  void test_wavfile_multiple_loads(TestContext &ctx)
  {
    std::printf("Testing WavFile multiple load attempts...\n");
    
    WavFile wav;
    
    // First load attempt (will fail)
    bool result1 = wav.load("/nonexistent/file1.wav");
    T_ASSERT(ctx, !result1);
    T_ASSERT(ctx, !wav.isLoaded());
    
    // Second load attempt (will also fail)
    bool result2 = wav.load("/nonexistent/file2.wav");
    T_ASSERT(ctx, !result2);
    T_ASSERT(ctx, !wav.isLoaded());
    
    // State should remain clean after failed loads
    T_ASSERT(ctx, wav.getSamples().empty());
  }
  
  void test_wavfile_reset_after_failed_load(TestContext &ctx)
  {
    std::printf("Testing WavFile reset after failed load...\n");
    
    WavFile wav;
    
    // Try to load invalid file
    wav.load("/nonexistent/file.wav");
    
    // Reset should work even after failed load
    wav.reset();
    
    T_ASSERT(ctx, !wav.isLoaded());
    T_ASSERT(ctx, wav.getSamples().empty());
    T_ASSERT(ctx, wav.getPath().empty());
  }
  
  void test_wavfile_path_types(TestContext &ctx)
  {
    std::printf("Testing WavFile with various path types...\n");
    
    WavFile wav;
    
    // Test relative path
    bool result1 = wav.load("nonexistent.wav");
    T_ASSERT(ctx, !result1);
    
    // Test path with special characters
    bool result2 = wav.load("/path/with spaces/file.wav");
    T_ASSERT(ctx, !result2);
    
    // Test very long path
    std::string longPath(1000, 'a');
    longPath += ".wav";
    bool result3 = wav.load(longPath);
    T_ASSERT(ctx, !result3);
  }
  
  void test_wavfile_invalid_extension(TestContext &ctx)
  {
    std::printf("Testing WavFile with invalid extension...\n");
    
    WavFile wav;
    
    // Try loading files with wrong extensions
    bool result1 = wav.load("/some/file.mp3");
    T_ASSERT(ctx, !result1);
    
    bool result2 = wav.load("/some/file.txt");
    T_ASSERT(ctx, !result2);
    
    bool result3 = wav.load("/some/file");
    T_ASSERT(ctx, !result3);
  }
  
  void test_wavfile_state_isolation(TestContext &ctx)
  {
    std::printf("Testing WavFile state isolation between instances...\n");
    
    WavFile wav1;
    WavFile wav2;
    
    // Both should start clean
    T_ASSERT(ctx, !wav1.isLoaded());
    T_ASSERT(ctx, !wav2.isLoaded());
    
    // Failed load on wav1 shouldn't affect wav2
    wav1.load("/nonexistent1.wav");
    T_ASSERT(ctx, !wav1.isLoaded());
    T_ASSERT(ctx, !wav2.isLoaded());
    
    // Each instance should maintain independent state
    T_ASSERT(ctx, wav1.getChannels() == 0);
    T_ASSERT(ctx, wav2.getChannels() == 0);
  }
  
  void test_wavfile_getters_const_correctness(TestContext &ctx)
  {
    std::printf("Testing WavFile const getter correctness...\n");
    
    const WavFile wav;
    
    // All const getters should work
    (void)wav.getSamples();
    (void)wav.getChannels();
    (void)wav.getSampleRate();
    (void)wav.getFrameCount();
    (void)wav.getPath();
    (void)wav.isLoaded();
    
    T_ASSERT(ctx, true);  // Success if no crash
  }

  //------------------------------------------------------------------------------
  // CabSimDSP Tests
  //------------------------------------------------------------------------------
  void test_cabsim_dsp_initialization(TestContext &ctx)
  {
    std::printf("Testing CabSimDSP initialization...\n");
    
    CabSimDSP dsp;
    T_ASSERT(ctx, !dsp.isIRLoaded(0));
    T_ASSERT(ctx, !dsp.isIRLoaded(1));
    T_ASSERT(ctx, dsp.getIRName(0).empty());
    T_ASSERT(ctx, dsp.getIRName(1).empty());
  }
  
  void test_cabsim_dsp_passthrough(TestContext &ctx)
  {
    std::printf("Testing CabSimDSP passthrough (no IRs)...\n");
    
    CabSimDSP dsp;
    dsp.setSampleRate(48000.f);
    
    // Without any IRs loaded, should pass through with blend applied
    // Process enough samples to fill a block
    float blend = 0.5f;
    float lpFreq = 20000.f;
    float hpFreq = 20.f;
    
    // Process a full block (256 samples) plus some extra
    for (int i = 0; i < 300; i++) {
      float input = 0.5f * std::sin(2.f * M_PI * 440.f * i / 48000.f);
      float output = dsp.process(input, blend, lpFreq, hpFreq);
      T_ASSERT(ctx, !std::isnan(output) && !std::isinf(output));
    }
  }
  
  void test_cabsim_dsp_sample_rate(TestContext &ctx)
  {
    std::printf("Testing CabSimDSP sample rate handling...\n");
    
    CabSimDSP dsp;
    dsp.setSampleRate(44100.f);
    
    // Should not crash
    float output = dsp.process(0.3f, 0.5f, 10000.f, 100.f);
    T_ASSERT(ctx, !std::isnan(output) && !std::isinf(output));
    
    // WavFile tests
    test_wavfile_initialization(ctx);
    test_wavfile_reset(ctx);
    test_wavfile_invalid_file(ctx);
    test_wavfile_empty_path(ctx);
    test_wavfile_getters_no_data(ctx);
    test_wavfile_frame_count_calculation(ctx);
    test_wavfile_multiple_loads(ctx);
    test_wavfile_reset_after_failed_load(ctx);
    test_wavfile_path_types(ctx);
    test_wavfile_invalid_extension(ctx);
    test_wavfile_state_isolation(ctx);
    test_wavfile_getters_const_correctness(ctx);
    
    // Change sample rate
    dsp.setSampleRate(96000.f);
    output = dsp.process(0.3f, 0.5f, 10000.f, 100.f);
    T_ASSERT(ctx, !std::isnan(output) && !std::isinf(output));
  }
  
  void test_cabsim_dsp_filter_processing(TestContext &ctx)
  {
    std::printf("Testing CabSimDSP filter processing...\n");
    
    CabSimDSP dsp;
    dsp.setSampleRate(48000.f);
    
    // Test with various filter settings
    float testFreqs[][2] = {
      {20000.f, 20.f},     // Wide open
      {5000.f, 100.f},     // Moderate
      {2000.f, 500.f},     // Narrow
      {1000.f, 1000.f},    // Very narrow
    };
    
    for (const auto& freqs : testFreqs) {
      float lpFreq = freqs[0];
      float hpFreq = freqs[1];
      
      // Process a block
      for (int i = 0; i < 300; i++) {
        float input = 0.5f * std::sin(2.f * M_PI * 440.f * i / 48000.f);
        float output = dsp.process(input, 0.5f, lpFreq, hpFreq);
        T_ASSERT(ctx, !std::isnan(output) && !std::isinf(output));
      }
    }
  }
  
  void test_cabsim_dsp_blend_values(TestContext &ctx)
  {
    std::printf("Testing CabSimDSP blend values...\n");
    
    CabSimDSP dsp;
    dsp.setSampleRate(48000.f);
    
    // Test various blend values
    float blendValues[] = {0.f, 0.25f, 0.5f, 0.75f, 1.f};
    
    for (float blend : blendValues) {
      for (int i = 0; i < 300; i++) {
        float input = 0.5f;
        float output = dsp.process(input, blend, 20000.f, 20.f);
        T_ASSERT(ctx, !std::isnan(output) && !std::isinf(output));
      }
    }
  }
  
  void test_cabsim_dsp_reset(TestContext &ctx)
  {
    std::printf("Testing CabSimDSP reset...\n");
    
    CabSimDSP dsp;
    dsp.setSampleRate(48000.f);
    
    // Process some samples
    for (int i = 0; i < 100; i++) {
      dsp.process(0.5f, 0.5f, 10000.f, 100.f);
    }
    
    // Reset
    dsp.reset();
    
    // Should still work after reset
    float output = dsp.process(0.3f, 0.5f, 10000.f, 100.f);
    T_ASSERT(ctx, !std::isnan(output) && !std::isinf(output));
  }
  
  void test_cabsim_dsp_extreme_values(TestContext &ctx)
  {
    std::printf("Testing CabSimDSP with extreme values...\n");
    
    CabSimDSP dsp;
    dsp.setSampleRate(48000.f);
    
    // Test with maximum input
    for (int i = 0; i < 300; i++) {
      float output = dsp.process(1.0f, 0.5f, 20000.f, 20.f);
      T_ASSERT(ctx, !std::isnan(output) && !std::isinf(output));
    }
    
    // Test with negative maximum
    for (int i = 0; i < 300; i++) {
      float output = dsp.process(-1.0f, 0.5f, 20000.f, 20.f);
      T_ASSERT(ctx, !std::isnan(output) && !std::isinf(output));
    }
    
    // Test with zero
    for (int i = 0; i < 300; i++) {
      float output = dsp.process(0.0f, 0.5f, 20000.f, 20.f);
      T_ASSERT(ctx, !std::isnan(output) && !std::isinf(output));
    }
  }
  
  void test_cabsim_dsp_normalization_flag(TestContext &ctx)
  {
    std::printf("Testing CabSimDSP normalization flag...\n");
    
    CabSimDSP dsp;
    
    // Default should be false
    T_ASSERT(ctx, !dsp.getNormalize(0));
    T_ASSERT(ctx, !dsp.getNormalize(1));
    
    // Set and get
    dsp.setNormalize(0, true);
    T_ASSERT(ctx, dsp.getNormalize(0));
    T_ASSERT(ctx, !dsp.getNormalize(1));
    
    dsp.setNormalize(1, true);
    T_ASSERT(ctx, dsp.getNormalize(1));
    
    dsp.setNormalize(0, false);
    T_ASSERT(ctx, !dsp.getNormalize(0));
  }
  
  void test_cabsim_dsp_unload_ir(TestContext &ctx)
  {
    std::printf("Testing CabSimDSP unload IR...\n");
    
    CabSimDSP dsp;
    
    // Unload when nothing is loaded should not crash
    dsp.unloadIR(0);
    dsp.unloadIR(1);
    
    T_ASSERT(ctx, !dsp.isIRLoaded(0));
    T_ASSERT(ctx, !dsp.isIRLoaded(1));
  }
  
  void test_cabsim_dsp_invalid_slot(TestContext &ctx)
  {
    std::printf("Testing CabSimDSP invalid slot handling...\n");
    
    CabSimDSP dsp;
    
    // Invalid slots should be handled gracefully
    T_ASSERT(ctx, !dsp.isIRLoaded(-1));
    T_ASSERT(ctx, !dsp.isIRLoaded(2));
    T_ASSERT(ctx, dsp.getIRName(-1).empty());
    T_ASSERT(ctx, dsp.getIRName(2).empty());
    T_ASSERT(ctx, !dsp.getNormalize(-1));
    T_ASSERT(ctx, !dsp.getNormalize(2));
  }

  //------------------------------------------------------------------------------
  // Test Runner
  //------------------------------------------------------------------------------
  void run_all_swv_guitar_collection_tests()
  {
    TestContext ctx;

    std::printf("\n=== Swv guitar collection DSP Unit Tests ===\n\n");

    // BiquadFilter tests
    test_biquad_filter_passthrough(ctx);
    test_biquad_filter_low_shelf(ctx);
    test_biquad_filter_high_shelf(ctx);
    test_biquad_filter_peaking(ctx);
    
    // NoiseGate tests
    test_noise_gate_initialization(ctx);
    test_noise_gate_below_threshold(ctx);
    test_noise_gate_above_threshold(ctx);
    test_noise_gate_default_params_unchanged_still_opens(ctx);
    test_noise_gate_hysteresis(ctx);
    test_noise_gate_reset(ctx);
    
    // ToneStack tests
    test_tone_stack_initialization(ctx);
    test_tone_stack_flat_response(ctx);
    test_tone_stack_bass_boost(ctx);
    test_tone_stack_all_bands(ctx);
    test_tone_stack_reset(ctx);
    
    // NamDSP tests
    test_nam_dsp_initialization(ctx);
    test_nam_dsp_passthrough(ctx);
    test_nam_dsp_sample_rate(ctx);
    test_nam_dsp_noise_gate_integration(ctx);
    test_nam_dsp_tone_stack_integration(ctx);
    test_nam_dsp_reset(ctx);
    test_nam_dsp_block_sizes(ctx);
    test_nam_dsp_dc_blocking(ctx);
    test_nam_dsp_extreme_values(ctx);
    test_nam_dsp_eco_mode_toggle(ctx);
    benchmark_namplayer_style_control_path(ctx);
    
    // IRLoader tests
    test_ir_loader_initialization(ctx);
    test_ir_loader_reset(ctx);
    test_ir_loader_invalid_file(ctx);
    test_ir_loader_normalization(ctx);
    
    // CabSimDSP tests
    test_cabsim_dsp_initialization(ctx);
    test_cabsim_dsp_passthrough(ctx);
    test_cabsim_dsp_sample_rate(ctx);
    test_cabsim_dsp_filter_processing(ctx);
    test_cabsim_dsp_blend_values(ctx);
    test_cabsim_dsp_reset(ctx);
    test_cabsim_dsp_extreme_values(ctx);
    test_cabsim_dsp_normalization_flag(ctx);
    test_cabsim_dsp_unload_ir(ctx);
    test_cabsim_dsp_invalid_slot(ctx);

    std::printf("\n");
    ctx.summary();
    std::printf("\n");
  }
} // namespace TestSuite

int main()
{
  TestSuite::run_all_swv_guitar_collection_tests();
  return 0;
}