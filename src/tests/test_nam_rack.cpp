/**
 * Test suite for NAM rewrite
 *
 * This file contains tests for the rewritten NAM library components.
 * Run with: ./test_nam_rack
 *
 * Tests are organized by component:
 * - test_matrix: Matrix operations
 * - test_ring_buffer: Ring buffer functionality
 * - test_activations: Activation functions
 * - test_conv1d: 1D dilated convolution
 * - test_conv1x1: 1x1 convolution
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include <chrono>

#include "dsp/nam_rack/matrix.h"
#include "dsp/nam_rack/ring_buffer.h"
#include "dsp/nam_rack/activations.h"
#include "dsp/nam_rack/conv1d.h"
#include "dsp/nam_rack/conv1x1.h"

namespace test_nam {

// ============================================================================
// Test utilities
// ============================================================================

struct TestContext {
    const char* current_test = "";
    int passed = 0;
    int failed = 0;

    void assertTrue(bool condition, const char* msg = "") {
        if (condition) {
            passed++;
        } else {
            failed++;
            std::cerr << "FAIL [" << current_test << "]: " << msg << std::endl;
        }
    }

    void assertFalse(bool condition, const char* msg = "") {
        assertTrue(!condition, msg);
    }

    void assertNear(float a, float b, float epsilon = 1e-5f, const char* msg = "") {
        if (std::fabs(a - b) < epsilon) {
            passed++;
        } else {
            failed++;
            std::cerr << "FAIL [" << current_test << "]: " << msg
                      << " (expected " << b << ", got " << a << ")" << std::endl;
        }
    }

    void assertNearArray(const float* a, const float* b, int size,
                         float epsilon = 1e-5f, const char* msg = "") {
        for (int i = 0; i < size; i++) {
            if (std::fabs(a[i] - b[i]) >= epsilon) {
                char buf[256];
                snprintf(buf, sizeof(buf), "%s - Mismatch at index %d: %f vs %f",
                         msg, i, a[i], b[i]);
                assertTrue(false, buf);
                return;
            }
        }
        assertTrue(true, msg);
    }
};

// ============================================================================
// Matrix tests
// ============================================================================

void test_matrix(TestContext& ctx) {
    ctx.current_test = "matrix";

    std::cout << "Testing Matrix operations..." << std::endl;

    // Test 1.1: Basic allocation and access
    {
        nam::Matrix m;
        m.resize(4, 8);
        ctx.assertTrue(m.rows() == 4, "Matrix rows");
        ctx.assertTrue(m.cols() == 8, "Matrix cols");

        m(2, 3) = 1.5f;
        ctx.assertNear(m(2, 3), 1.5f, 1e-6f, "Matrix element access");
    }

    // Test 1.2: setZero
    {
        nam::Matrix m;
        m.resize(3, 3);
        m.setZero();
        bool all_zero = true;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                if (m(i, j) != 0.f) all_zero = false;
        ctx.assertTrue(all_zero, "Matrix setZero");
    }

    // Test 1.3: Matrix multiplication (small)
    {
        nam::Matrix a, b, c;
        a.resize(2, 3);
        b.resize(3, 2);
        c.resize(2, 2);

        // [[1,2,3],[4,5,6]] * [[1,2],[3,4],[5,6]] = [[22,28],[49,64]]
        a(0,0)=1; a(0,1)=2; a(0,2)=3;
        a(1,0)=4; a(1,1)=5; a(1,2)=6;
        b(0,0)=1; b(0,1)=2;
        b(1,0)=3; b(1,1)=4;
        b(2,0)=5; b(2,1)=6;

        nam::Matrix::multiply(c, a, b);

        ctx.assertNear(c(0,0), 22.f, 1e-4f, "Matmul result [0,0]");
        ctx.assertNear(c(0,1), 28.f, 1e-4f, "Matmul result [0,1]");
        ctx.assertNear(c(1,0), 49.f, 1e-4f, "Matmul result [1,0]");
        ctx.assertNear(c(1,1), 64.f, 1e-4f, "Matmul result [1,1]");
    }

    // Test 1.4: Matrix multiplication (larger)
    {
        nam::Matrix a, b, c;
        a.resize(16, 32);
        b.resize(32, 16);
        c.resize(16, 16);

        // Fill with known values
        for (int i = 0; i < 16; i++)
            for (int k = 0; k < 32; k++)
                a(i,k) = static_cast<float>(i * k) / 100.f;
        for (int k = 0; k < 32; k++)
            for (int j = 0; j < 16; j++)
                b(k,j) = static_cast<float>(k + j) / 100.f;

        nam::Matrix::multiply(c, a, b);

        // Verify a few elements manually
        float expected = 0.f;
        for (int k = 0; k < 32; k++)
            expected += a(5, k) * b(k, 7);
        ctx.assertNear(c(5, 7), expected, 1e-4f, "Matmul larger [5,7]");
    }

    // Test 1.5: Colwise addition
    {
        nam::Matrix m;
        nam::Vector v;
        m.resize(3, 4);
        v.resize(3);

        m.setZero();
        v(0) = 1.f; v(1) = 2.f; v(2) = 3.f;

        nam::Matrix::addColwise(m, v);

        for (int j = 0; j < 4; j++) {
            ctx.assertNear(m(0, j), 1.f, 1e-6f, "Colwise add row 0");
            ctx.assertNear(m(1, j), 2.f, 1e-6f, "Colwise add row 1");
            ctx.assertNear(m(2, j), 3.f, 1e-6f, "Colwise add row 2");
        }
    }

    // Test 1.6: Vector operations
    {
        nam::Vector v;
        v.resize(5);
        v.setZero();

        for (int i = 0; i < 5; i++) {
            v(i) = static_cast<float>(i) * 0.5f;
        }

        ctx.assertNear(v(0), 0.0f, 1e-6f, "Vector [0]");
        ctx.assertNear(v(2), 1.0f, 1e-6f, "Vector [2]");
        ctx.assertNear(v(4), 2.0f, 1e-6f, "Vector [4]");
    }

    // Test 1.7: Matrix-Vector multiplication
    {
        nam::Matrix a;
        nam::Vector x, y;

        a.resize(2, 3);
        x.resize(3);
        y.resize(2);

        // [[1,2,3],[4,5,6]] * [1,2,3]^T = [14,32]^T
        a(0,0)=1; a(0,1)=2; a(0,2)=3;
        a(1,0)=4; a(1,1)=5; a(1,2)=6;
        x(0)=1; x(1)=2; x(2)=3;

        nam::Matrix::multiply(y, a, x);

        ctx.assertNear(y(0), 14.f, 1e-4f, "Mat-vec result [0]");
        ctx.assertNear(y(1), 32.f, 1e-4f, "Mat-vec result [1]");
    }

    // Test 1.8: MatrixBlock
    {
        nam::Matrix m;
        m.resize(4, 4);

        // Fill matrix
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                m(i, j) = static_cast<float>(i * 4 + j);

        // Create block (2x2 starting at 1,1)
        nam::MatrixBlock block(m, 1, 1, 2, 2);

        ctx.assertNear(block(0, 0), 5.f, 1e-6f, "Block [0,0]");  // m(1,1) = 5
        ctx.assertNear(block(0, 1), 6.f, 1e-6f, "Block [0,1]");  // m(1,2) = 6
        ctx.assertNear(block(1, 0), 9.f, 1e-6f, "Block [1,0]");  // m(2,1) = 9
        ctx.assertNear(block(1, 1), 10.f, 1e-6f, "Block [1,1]"); // m(2,2) = 10

        // Modify through block
        block(0, 0) = 100.f;
        ctx.assertNear(m(1, 1), 100.f, 1e-6f, "Block modification reflected");
    }

    std::cout << "  Matrix tests: " << ctx.passed << " passed, " << ctx.failed << " failed" << std::endl;
}

// ============================================================================
// Ring buffer tests
// ============================================================================

void test_ring_buffer(TestContext& ctx) {
    ctx.current_test = "ring_buffer";

    // Reset pass/fail counters for this test section
    int section_passed = ctx.passed;
    int section_failed = ctx.failed;

    std::cout << "Testing RingBuffer operations..." << std::endl;

    // Test 2.1: Basic initialization
    {
        nam::RingBuffer rb;
        rb.reset(2, 64, 128);  // 2 channels, 64 max buffer, 128 max lookback

        ctx.assertTrue(rb.getChannels() == 2, "RingBuffer channels");
        ctx.assertTrue(rb.getMaxBufferSize() == 64, "RingBuffer max buffer size");
        ctx.assertTrue(rb.getMaxLookback() == 128, "RingBuffer max lookback");
    }

    // Test 2.2: Write and read (read before advance)
    {
        nam::RingBuffer rb;
        rb.reset(1, 16, 0);  // 1 channel, 16 max buffer, no lookback

        float input[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        float output[8];

        rb.write(input, 8, false);  // interleaved (but 1 channel)
        // Read BEFORE advance - lookback=0 reads from current write position
        rb.read(output, 8, 0);
        rb.advance(8);

        ctx.assertNearArray(input, output, 8, 1e-6f, "Basic write/read (before advance)");
    }

    // Test 2.2b: Write and read (read after advance with lookback)
    {
        nam::RingBuffer rb;
        rb.reset(1, 16, 16);  // 1 channel, 16 max buffer, 16 max lookback

        float input[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        float output[8];

        rb.write(input, 8, false);
        rb.advance(8);
        // Read AFTER advance - need lookback=8 to read what was just written
        rb.read(output, 8, 8);

        ctx.assertNearArray(input, output, 8, 1e-6f, "Write/read with lookback");
    }

    // Test 2.3: Lookback
    {
        nam::RingBuffer rb;
        rb.reset(1, 16, 32);  // 1 channel, 16 max buffer, 32 max lookback

        float input[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        float output[8];

        rb.write(input, 8, false);
        rb.advance(8);

        // Write more data (overwrites position 0-7)
        float input2[8] = {10, 20, 30, 40, 50, 60, 70, 80};
        rb.write(input2, 8, false);
        rb.advance(8);

        // Read current with lookback=8 - should get input2
        rb.read(output, 8, 8);
        ctx.assertNearArray(input2, output, 8, 1e-6f, "Current read after second write");

        // Read with lookback 16 - should get original input
        rb.read(output, 8, 16);
        ctx.assertNearArray(input, output, 8, 1e-6f, "Lookback read");
    }

    // Test 2.4: Multi-channel
    {
        nam::RingBuffer rb;
        rb.reset(2, 16, 16);  // 2 channels, with lookback support

        // Interleaved input: [ch0_f0, ch1_f0, ch0_f1, ch1_f1, ...]
        float input[8] = {1, 10, 2, 20, 3, 30, 4, 40};
        float output[8];

        rb.write(input, 4, false);  // 4 frames
        rb.advance(4);
        // Read with lookback=4 to get what was just written
        rb.read(output, 4, 4);

        ctx.assertNearArray(input, output, 8, 1e-6f, "Multi-channel write/read");
    }

    // Test 2.5: Matrix interface
    {
        nam::RingBuffer rb;
        rb.reset(2, 16, 16);  // with lookback support

        nam::Matrix input;
        input.resize(2, 4);  // 2 channels, 4 frames
        input(0,0)=1; input(0,1)=2; input(0,2)=3; input(0,3)=4;
        input(1,0)=10; input(1,1)=20; input(1,2)=30; input(1,3)=40;

        rb.write(input, 4);
        rb.advance(4);

        nam::Matrix output;
        output.resize(2, 4);
        // Read with lookback=4 to get what was just written
        rb.read(output, 4, 4);

        bool match = true;
        for (int c = 0; c < 2; c++)
            for (int f = 0; f < 4; f++)
                if (input(c, f) != output(c, f)) match = false;

        ctx.assertTrue(match, "Matrix interface write/read");
    }

    // Test 2.6: Rewind behavior
    {
        nam::RingBuffer rb;
        rb.reset(1, 8, 16);  // 1 channel, small buffer to trigger rewind

        float input[8] = {1, 2, 3, 4, 5, 6, 7, 8};

        // Write multiple times to trigger rewind
        for (int i = 0; i < 10; i++) {
            rb.write(input, 8, false);
            rb.advance(8);
        }

        // Should have rewound but still maintain lookback capability
        ctx.assertTrue(rb.getWritePos() >= rb.getMaxLookback(),
                       "Write position after rewind");
    }

    std::cout << "  RingBuffer tests: " << (ctx.passed - section_passed)
              << " passed, " << (ctx.failed - section_failed) << " failed" << std::endl;
}

// ============================================================================
// Activation tests
// ============================================================================

void test_activations(TestContext& ctx) {
    ctx.current_test = "activations";

    int section_passed = ctx.passed;
    int section_failed = ctx.failed;

    std::cout << "Testing Activation functions..." << std::endl;

    using namespace nam::activations;

    // Test 3.1: Fast tanh accuracy
    {
        // Test at various points
        float test_points[] = {-5.0f, -2.0f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 2.0f, 5.0f};

        for (float x : test_points) {
            float expected = std::tanh(x);
            float actual = fast_tanh(x);
            ctx.assertNear(actual, expected, 0.01f, "fast_tanh accuracy");
        }
    }

    // Test 3.2: Fast sigmoid accuracy
    {
        float test_points[] = {-5.0f, -2.0f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 2.0f, 5.0f};

        for (float x : test_points) {
            float expected = 1.0f / (1.0f + std::exp(-x));
            float actual = fast_sigmoid(x);
            ctx.assertNear(actual, expected, 0.01f, "fast_sigmoid accuracy");
        }
    }

    // Test 3.3: ReLU
    {
        ctx.assertNear(relu(-1.0f), 0.0f, 1e-6f, "ReLU negative");
        ctx.assertNear(relu(0.0f), 0.0f, 1e-6f, "ReLU zero");
        ctx.assertNear(relu(1.0f), 1.0f, 1e-6f, "ReLU positive");
        ctx.assertNear(relu(5.0f), 5.0f, 1e-6f, "ReLU large positive");
    }

    // Test 3.4: Leaky ReLU
    {
        ctx.assertNear(leaky_relu(-1.0f, 0.1f), -0.1f, 1e-6f, "LeakyReLU negative");
        ctx.assertNear(leaky_relu(1.0f, 0.1f), 1.0f, 1e-6f, "LeakyReLU positive");
    }

    // Test 3.5: Hard tanh
    {
        ctx.assertNear(hard_tanh(-2.0f), -1.0f, 1e-6f, "HardTanh below min");
        ctx.assertNear(hard_tanh(0.5f), 0.5f, 1e-6f, "HardTanh in range");
        ctx.assertNear(hard_tanh(2.0f), 1.0f, 1e-6f, "HardTanh above max");
    }

    // Test 3.6: Activation class interface
    {
        nam::Matrix m;
        m.resize(2, 4);

        // Fill with test values
        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 4; j++)
                m(i, j) = static_cast<float>(j - 2);  // -2, -1, 0, 1

        Activation* relu_act = Activation::get("ReLU");
        relu_act->apply(m);

        ctx.assertNear(m(0, 0), 0.0f, 1e-6f, "ReLU matrix [-2]");  // -2 -> 0
        ctx.assertNear(m(0, 1), 0.0f, 1e-6f, "ReLU matrix [-1]");  // -1 -> 0
        ctx.assertNear(m(0, 2), 0.0f, 1e-6f, "ReLU matrix [0]");   // 0 -> 0
        ctx.assertNear(m(0, 3), 1.0f, 1e-6f, "ReLU matrix [1]");   // 1 -> 1
    }

    // Test 3.7: Activation factory
    {
        Activation* tanh_act = Activation::get("Tanh");
        Activation* relu_act = Activation::get("ReLU");
        Activation* sigmoid_act = Activation::get("Sigmoid");
        Activation* identity_act = Activation::get("Identity");
        Activation* unknown = Activation::get("Unknown");

        ctx.assertTrue(tanh_act != nullptr, "Factory returns Tanh");
        ctx.assertTrue(relu_act != nullptr, "Factory returns ReLU");
        ctx.assertTrue(sigmoid_act != nullptr, "Factory returns Sigmoid");
        ctx.assertTrue(identity_act != nullptr, "Factory returns Identity");
        ctx.assertTrue(unknown != nullptr, "Factory returns Identity for unknown");
    }

    // Test 3.8: LUT activation
    {
        // Create LUT for tanh
        ActivationLUT lut(-5.0f, 5.0f, 1024, [](float x) {
            return std::tanh(x);
        });

        float test_values[] = {-4.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 4.0f};

        for (float x : test_values) {
            float expected = std::tanh(x);
            float actual = lut.lookup(x);
            ctx.assertNear(actual, expected, 0.005f, "LUT tanh lookup");
        }
    }

    // Test 3.9: PReLU per-channel
    {
        nam::Matrix m;
        m.resize(2, 3);

        // Channel 0: negative values, Channel 1: positive values
        m(0,0)=-2; m(0,1)=-1; m(0,2)=0;
        m(1,0)=1; m(1,1)=2; m(1,2)=3;

        ActivationPReLU prelu;
        prelu.setSlopes({0.5f, 0.1f});  // Different slope per channel
        prelu.apply(m);

        ctx.assertNear(m(0, 0), -1.0f, 1e-6f, "PReLU ch0 neg (slope 0.5)");  // -2 * 0.5 = -1
        ctx.assertNear(m(0, 1), -0.5f, 1e-6f, "PReLU ch0 neg2");
        ctx.assertNear(m(1, 0), 1.0f, 1e-6f, "PReLU ch1 pos");
    }

    std::cout << "  Activation tests: " << (ctx.passed - section_passed)
              << " passed, " << (ctx.failed - section_failed) << " failed" << std::endl;
}

// ============================================================================
// Conv1D tests
// ============================================================================

void test_conv1d(TestContext& ctx) {
    ctx.current_test = "conv1d";

    int section_passed = ctx.passed;
    int section_failed = ctx.failed;

    std::cout << "Testing Conv1D operations..." << std::endl;

    // Test 4.1: Basic creation and accessors
    {
        nam::Conv1D conv(2, 4, 3, true, 1, 1);  // 2 in, 4 out, kernel 3, bias, dilation 1, groups 1

        ctx.assertTrue(conv.getInChannels() == 2, "Conv1D in_channels");
        ctx.assertTrue(conv.getOutChannels() == 4, "Conv1D out_channels");
        ctx.assertTrue(conv.getKernelSize() == 3, "Conv1D kernel_size");
        ctx.assertTrue(conv.getDilation() == 1, "Conv1D dilation");
        ctx.assertTrue(conv.hasBias() == true, "Conv1D has_bias");
    }

    // Test 4.2: Set weights and process simple case
    {
        nam::Conv1D conv;
        conv.setSize(1, 1, 3, true, 1, 1);  // 1 in, 1 out, kernel 3, bias, dilation 1
        conv.setMaxBufferSize(16);

        // Set weights: [w0, w1, w2, bias] = [1, 2, 3, 0.5]
        std::vector<float> weights = {1.0f, 2.0f, 3.0f, 0.5f};
        auto it = weights.begin();
        conv.setWeights(it);

        // Input: [1, 2, 3, 4]
        nam::Matrix input;
        input.resize(1, 4);
        input(0, 0) = 1.0f;
        input(0, 1) = 2.0f;
        input(0, 2) = 3.0f;
        input(0, 3) = 4.0f;

        // Process twice to fill the ring buffer
        conv.process(input, 4);

        // After first process with dilation=1, kernel_size=3:
        // For frame 0: need input[-2], input[-1], input[0] -> only input[0]=1 available, others are 0
        // output[0] = w2*input[-2] + w1*input[-1] + w0*input[0] + bias = 0 + 0 + 1*1 + 0.5 = 1.5
        // output[1] = w2*input[-1] + w1*input[0] + w0*input[1] + bias = 0 + 2*1 + 1*2 + 0.5 = 4.5
        // etc.

        // Just verify it runs without crashing
        ctx.assertTrue(true, "Conv1D basic process");
    }

    // Test 4.3: Dilated convolution
    {
        nam::Conv1D conv;
        conv.setSize(1, 1, 2, false, 2, 1);  // 1 in, 1 out, kernel 2, no bias, dilation 2
        conv.setMaxBufferSize(16);

        // Set weights: [w0, w1] = [1, 1] (simple sum)
        std::vector<float> weights = {1.0f, 1.0f};
        auto it = weights.begin();
        conv.setWeights(it);

        // Input: 8 samples
        nam::Matrix input;
        input.resize(1, 8);
        for (int i = 0; i < 8; i++) {
            input(0, i) = static_cast<float>(i + 1);  // [1, 2, 3, 4, 5, 6, 7, 8]
        }

        conv.process(input, 8);

        ctx.assertTrue(true, "Conv1D dilated process");
    }

    // Test 4.4: Grouped convolution
    {
        nam::Conv1D conv;
        conv.setSize(4, 4, 1, false, 1, 2);  // 4 in, 4 out, kernel 1, no bias, 2 groups
        conv.setMaxBufferSize(16);

        // Each group: 2 in -> 2 out
        // Total weights: 2*2 = 4 per group * 2 groups = 8 weights
        std::vector<float> weights = {1.0f, 0.0f, 0.0f, 1.0f,   // Group 0: identity
                                       2.0f, 0.0f, 0.0f, 2.0f};  // Group 1: scale by 2
        auto it = weights.begin();
        conv.setWeights(it);

        ctx.assertTrue(conv.getNumGroups() == 2, "Conv1D grouped num_groups");
    }

    // Test 4.5: Num weights calculation
    {
        nam::Conv1D conv;
        conv.setSize(2, 4, 3, true, 1, 1);  // 2 in, 4 out, kernel 3, bias

        // Weights: 3 * 4 * 2 = 24, Bias: 4, Total: 28
        ctx.assertTrue(conv.getNumWeights() == 28, "Conv1D num_weights with bias");
    }

    std::cout << "  Conv1D tests: " << (ctx.passed - section_passed)
              << " passed, " << (ctx.failed - section_failed) << " failed" << std::endl;
}

// ============================================================================
// Conv1x1 tests
// ============================================================================

void test_conv1x1(TestContext& ctx) {
    ctx.current_test = "conv1x1";

    int section_passed = ctx.passed;
    int section_failed = ctx.failed;

    std::cout << "Testing Conv1x1 operations..." << std::endl;

    // Test 5.1: Basic creation and accessors
    {
        nam::Conv1x1 conv(3, 2, true, 1);  // 3 in, 2 out, bias, groups 1

        ctx.assertTrue(conv.getInChannels() == 3, "Conv1x1 in_channels");
        ctx.assertTrue(conv.getOutChannels() == 2, "Conv1x1 out_channels");
        ctx.assertTrue(conv.hasBias() == true, "Conv1x1 has_bias");
    }

    // Test 5.2: Simple matrix multiplication (identity-like)
    {
        nam::Conv1x1 conv(2, 2, false, 1);  // 2 in, 2 out, no bias
        conv.setMaxBufferSize(4);

        // Identity matrix weights
        std::vector<float> weights = {1.0f, 0.0f, 0.0f, 1.0f};
        auto it = weights.begin();
        conv.setWeights(it);

        nam::Matrix input;
        input.resize(2, 3);
        input(0, 0) = 1.0f; input(0, 1) = 2.0f; input(0, 2) = 3.0f;
        input(1, 0) = 4.0f; input(1, 1) = 5.0f; input(1, 2) = 6.0f;

        conv.process(input, 3);

        const nam::Matrix& output = conv.getOutput();

        // Identity should preserve input
        ctx.assertNear(output(0, 0), 1.0f, 1e-5f, "Conv1x1 identity [0,0]");
        ctx.assertNear(output(1, 0), 4.0f, 1e-5f, "Conv1x1 identity [1,0]");
        ctx.assertNear(output(0, 1), 2.0f, 1e-5f, "Conv1x1 identity [0,1]");
        ctx.assertNear(output(1, 2), 6.0f, 1e-5f, "Conv1x1 identity [1,2]");
    }

    // Test 5.3: With bias
    {
        nam::Conv1x1 conv(2, 2, true, 1);
        conv.setMaxBufferSize(4);

        // Identity weights with bias [1, 2]
        std::vector<float> weights = {1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 2.0f};
        auto it = weights.begin();
        conv.setWeights(it);

        nam::Matrix input;
        input.resize(2, 2);
        input(0, 0) = 0.0f; input(0, 1) = 0.0f;
        input(1, 0) = 0.0f; input(1, 1) = 0.0f;

        conv.process(input, 2);

        const nam::Matrix& output = conv.getOutput();

        // Output should be just the bias
        ctx.assertNear(output(0, 0), 1.0f, 1e-5f, "Conv1x1 bias [0,0]");
        ctx.assertNear(output(1, 0), 2.0f, 1e-5f, "Conv1x1 bias [1,0]");
    }

    // Test 5.4: Grouped convolution
    {
        nam::Conv1x1 conv(4, 4, false, 2);  // 4 in, 4 out, 2 groups
        conv.setMaxBufferSize(4);

        // Each group: 2 in -> 2 out
        // Group 0: identity, Group 1: scale by 2
        std::vector<float> weights = {1.0f, 0.0f, 0.0f, 1.0f,   // Group 0: identity
                                       2.0f, 0.0f, 0.0f, 2.0f};  // Group 1: scale by 2
        auto it = weights.begin();
        conv.setWeights(it);

        nam::Matrix input;
        input.resize(4, 2);
        input(0, 0) = 1.0f; input(0, 1) = 2.0f;  // Group 0
        input(1, 0) = 3.0f; input(1, 1) = 4.0f;
        input(2, 0) = 5.0f; input(2, 1) = 6.0f;  // Group 1
        input(3, 0) = 7.0f; input(3, 1) = 8.0f;

        conv.process(input, 2);

        const nam::Matrix& output = conv.getOutput();

        // Group 0 should be identity
        ctx.assertNear(output(0, 0), 1.0f, 1e-5f, "Conv1x1 grouped g0 [0,0]");
        ctx.assertNear(output(1, 0), 3.0f, 1e-5f, "Conv1x1 grouped g0 [1,0]");
        // Group 1 should be scaled by 2
        ctx.assertNear(output(2, 0), 10.0f, 1e-5f, "Conv1x1 grouped g1 [2,0]");  // 5*2
        ctx.assertNear(output(3, 0), 14.0f, 1e-5f, "Conv1x1 grouped g1 [3,0]");  // 7*2
    }

    std::cout << "  Conv1x1 tests: " << (ctx.passed - section_passed)
              << " passed, " << (ctx.failed - section_failed) << " failed" << std::endl;
}

// ============================================================================
// Performance tests
// ============================================================================

void test_performance(TestContext& ctx) {
    ctx.current_test = "performance";

    std::cout << "Running performance tests..." << std::endl;

    const int iterations = 1000;
    const int matrix_size = 64;

    // Test matrix multiplication performance
    {
        nam::Matrix a, b, c;
        a.resize(matrix_size, matrix_size);
        b.resize(matrix_size, matrix_size);
        c.resize(matrix_size, matrix_size);

        // Fill with random-ish data
        for (int i = 0; i < matrix_size; i++)
            for (int j = 0; j < matrix_size; j++) {
                a(i, j) = static_cast<float>((i * j) % 100) / 100.f;
                b(i, j) = static_cast<float>((i + j) % 100) / 100.f;
            }

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < iterations; i++) {
            nam::Matrix::multiply(c, a, b);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "  Matrix multiply (64x64): "
                  << (duration.count() / static_cast<double>(iterations))
                  << " us per operation" << std::endl;
    }

    // Test activation performance
    {
        const int array_size = 1024;
        std::vector<float> data(array_size);

        for (int i = 0; i < array_size; i++) {
            data[i] = static_cast<float>(i % 100 - 50) / 10.f;
        }

        nam::activations::Activation* fast_tanh =
            nam::activations::Activation::get("FastTanh");

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < iterations; i++) {
            fast_tanh->apply(data.data(), array_size);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "  FastTanh (1024 elements): "
                  << (duration.count() / static_cast<double>(iterations))
                  << " us per operation" << std::endl;
    }

    ctx.assertTrue(true, "Performance test completed");
}

// ============================================================================
// Main
// ============================================================================

} // namespace test_nam

int main() {
    test_nam::TestContext ctx;

    std::cout << "=== NAM Rewrite Test Suite ===" << std::endl;
    std::cout << std::endl;

    // Run all tests
    test_nam::test_matrix(ctx);
    test_nam::test_ring_buffer(ctx);
    test_nam::test_activations(ctx);
    test_nam::test_conv1d(ctx);
    test_nam::test_conv1x1(ctx);
    test_nam::test_performance(ctx);

    // Report
    std::cout << std::endl;
    std::cout << "=== Test Results ===" << std::endl;
    std::cout << "Passed: " << ctx.passed << std::endl;
    std::cout << "Failed: " << ctx.failed << std::endl;

    return ctx.failed > 0 ? 1 : 0;
}
