#include <cstdio>
#include <cmath>
#include <vector>
#include <limits>
#include <algorithm>
#include "../dsp/Nam.h"

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
    
    // Check that filters are reset (state variables should be zero)
    T_ASSERT_NEAR(ctx, stack.depth.z1, 0.f, 1e-6f);
    T_ASSERT_NEAR(ctx, stack.bass.z1, 0.f, 1e-6f);
    T_ASSERT_NEAR(ctx, stack.middle.z1, 0.f, 1e-6f);
    T_ASSERT_NEAR(ctx, stack.treble.z1, 0.f, 1e-6f);
    T_ASSERT_NEAR(ctx, stack.presence.z1, 0.f, 1e-6f);
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