#pragma once

/**
 * Fast activation functions for NAM (Neural Amp Modeler)
 *
 * Features:
 * - C++11 compatible
 * - Fast polynomial approximations (no std::tanh/std::exp in hot path)
 * - SIMD support via Rack SDK's float_4 when available
 * - Polymorphic activation class for dynamic selection
 */

#include <cmath>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <cassert>

#include "matrix.h"

// Check if building as VCV Rack plugin (enables SIMD)
#if defined(VCVRACK)
    #include <simd/functions.hpp>
    #define NAM_USE_SIMD 1
#else
    #define NAM_USE_SIMD 0
#endif

namespace nam {
namespace activations {

// ============================================================================
// Fast scalar activation functions
// ============================================================================

/**
 * Fast tanh approximation using rational polynomial
 * Maximum error: ~0.001 in range [-5, 5]
 * Based on the NAM reference implementation
 */
inline float fast_tanh(float x) {
    const float ax = std::fabs(x);
    const float x2 = x * x;

    return (x * (2.45550750702956f + 2.45550750702956f * ax
                + (0.893229853513558f + 0.821226666969744f * ax) * x2))
         / (2.44506634652299f + (2.44506634652299f + x2)
                * std::fabs(x + 0.814642734961073f * x * ax));
}

/**
 * Fast sigmoid approximation based on fast_tanh
 * sigmoid(x) = 0.5 * (tanh(x * 0.5) + 1)
 */
inline float fast_sigmoid(float x) {
    return 0.5f * (fast_tanh(x * 0.5f) + 1.0f);
}

/**
 * Standard ReLU
 */
inline float relu(float x) {
    return x > 0.0f ? x : 0.0f;
}

/**
 * Leaky ReLU
 */
inline float leaky_relu(float x, float negative_slope = 0.01f) {
    return x > 0.0f ? x : negative_slope * x;
}

/**
 * Standard sigmoid (for reference/comparison)
 */
inline float sigmoid(float x) {
    return 1.0f / (1.0f + std::exp(-x));
}

/**
 * Swish (SiLU): x * sigmoid(x)
 */
inline float swish(float x) {
    return x * sigmoid(x);
}

/**
 * Fast swish using fast_sigmoid
 */
inline float fast_swish(float x) {
    return x * fast_sigmoid(x);
}

/**
 * Hard tanh: clip to [-1, 1]
 */
inline float hard_tanh(float x) {
    if (x < -1.0f) return -1.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

/**
 * Leaky hard tanh with configurable slopes
 */
inline float leaky_hardtanh(float x, float min_val, float max_val,
                            float min_slope, float max_slope) {
    if (x < min_val) {
        return (x - min_val) * min_slope + min_val;
    } else if (x > max_val) {
        return (x - max_val) * max_slope + max_val;
    } else {
        return x;
    }
}

/**
 * Hard swish (mobile-optimized)
 */
inline float hard_swish(float x) {
    if (x <= -3.0f) {
        return 0.0f;
    } else if (x >= 3.0f) {
        return x;
    } else {
        return x * (x + 3.0f) / 6.0f;
    }
}

// ============================================================================
// SIMD activation functions (when Rack SIMD is available)
// ============================================================================

#if NAM_USE_SIMD

/**
 * SIMD fast tanh - process 4 floats at once
 */
inline rack::simd::float_4 fast_tanh_simd(rack::simd::float_4 x) {
    // For now, fall back to scalar for each lane
    // TODO: Implement true SIMD version using SIMD approximations
    alignas(16) float vals[4];
    x.store(vals);
    for (int i = 0; i < 4; i++) {
        vals[i] = fast_tanh(vals[i]);
    }
    return rack::simd::float_4::load(vals);
}

/**
 * SIMD fast sigmoid - process 4 floats at once
 */
inline rack::simd::float_4 fast_sigmoid_simd(rack::simd::float_4 x) {
    alignas(16) float vals[4];
    x.store(vals);
    for (int i = 0; i < 4; i++) {
        vals[i] = fast_sigmoid(vals[i]);
    }
    return rack::simd::float_4::load(vals);
}

#endif // NAM_USE_SIMD

// ============================================================================
// Polymorphic activation class
// ============================================================================

/**
 * Base class for polymorphic activation functions
 *
 * Allows runtime selection of activation type while maintaining
 * a consistent interface for apply() operations.
 */
class Activation {
public:
    Activation() = default;
    virtual ~Activation() = default;

    /**
     * Apply activation to raw float array
     */
    virtual void apply(float* data, long size) {
        // Default: identity (do nothing)
        (void)data;
        (void)size;
    }

    /**
     * Apply activation to Matrix
     */
    virtual void apply(Matrix& matrix) {
        apply(matrix.data(), matrix.rows() * matrix.cols());
    }

    /**
     * Get activation by name (factory method)
     */
    static Activation* get(const std::string& name);

protected:
    static std::unordered_map<std::string, Activation*>& getRegistry() {
        static std::unordered_map<std::string, Activation*> registry;
        return registry;
    }
};

// ============================================================================
// Concrete activation implementations
// ============================================================================

/**
 * Identity activation (no-op)
 */
class ActivationIdentity : public Activation {
public:
    ActivationIdentity() = default;
    void apply(float* data, long size) override {
        (void)data;
        (void)size;
        // Do nothing
    }
};

/**
 * Tanh activation (using std::tanh)
 */
class ActivationTanh : public Activation {
public:
    void apply(float* data, long size) override {
        for (long i = 0; i < size; i++) {
            data[i] = std::tanh(data[i]);
        }
    }
};

/**
 * Fast tanh activation (polynomial approximation)
 */
class ActivationFastTanh : public Activation {
public:
    void apply(float* data, long size) override {
#if NAM_USE_SIMD
        // SIMD path: process 4 elements at a time
        long i = 0;
        for (; i + 3 < size; i += 4) {
            rack::simd::float_4 v = rack::simd::float_4::load(data + i);
            v = rack::simd::tanh(v);  // Use Rack's SIMD tanh
            v.store(data + i);
        }
        // Handle remaining elements
        for (; i < size; i++) {
            data[i] = fast_tanh(data[i]);
        }
#else
        // Scalar path
        for (long i = 0; i < size; i++) {
            data[i] = fast_tanh(data[i]);
        }
#endif
    }
};

/**
 * Hard tanh activation
 */
class ActivationHardTanh : public Activation {
public:
    void apply(float* data, long size) override {
        for (long i = 0; i < size; i++) {
            data[i] = hard_tanh(data[i]);
        }
    }
};

/**
 * Leaky hard tanh with configurable parameters
 */
class ActivationLeakyHardTanh : public Activation {
public:
    ActivationLeakyHardTanh(float min_val = -1.0f, float max_val = 1.0f,
                            float min_slope = 0.01f, float max_slope = 0.01f)
        : m_minVal(min_val), m_maxVal(max_val)
        , m_minSlope(min_slope), m_maxSlope(max_slope) {}

    void apply(float* data, long size) override {
        for (long i = 0; i < size; i++) {
            data[i] = leaky_hardtanh(data[i], m_minVal, m_maxVal, m_minSlope, m_maxSlope);
        }
    }

private:
    float m_minVal, m_maxVal, m_minSlope, m_maxSlope;
};

/**
 * ReLU activation
 */
class ActivationReLU : public Activation {
public:
    void apply(float* data, long size) override {
        for (long i = 0; i < size; i++) {
            data[i] = relu(data[i]);
        }
    }
};

/**
 * Leaky ReLU activation
 */
class ActivationLeakyReLU : public Activation {
public:
    ActivationLeakyReLU(float negative_slope = 0.01f)
        : m_negativeSlope(negative_slope) {}

    void apply(float* data, long size) override {
        for (long i = 0; i < size; i++) {
            data[i] = leaky_relu(data[i], m_negativeSlope);
        }
    }

private:
    float m_negativeSlope;
};

/**
 * PReLU (Parametric ReLU) with per-channel slopes
 */
class ActivationPReLU : public Activation {
public:
    ActivationPReLU() = default;

    ActivationPReLU(float slope) {
        m_slopes.push_back(slope);
    }

    ActivationPReLU(const std::vector<float>& slopes) : m_slopes(slopes) {}

    void apply(Matrix& matrix) override {
        // Matrix is organized as (channels, time_steps)
        const int n_channels = static_cast<int>(m_slopes.size());
        const int actual_channels = matrix.rows();

        // Note: Check should be done during model loading
        assert(actual_channels == n_channels || n_channels == 1);

        if (n_channels == 1) {
            // Single slope applies to all channels
            apply(matrix.data(), matrix.rows() * matrix.cols());
        } else {
            // Per-channel slopes
            for (int c = 0; c < std::min(n_channels, actual_channels); c++) {
                for (int t = 0; t < matrix.cols(); t++) {
                    matrix(c, t) = leaky_relu(matrix(c, t), m_slopes[c]);
                }
            }
        }
    }

    void apply(float* data, long size) override {
        if (m_slopes.size() == 1) {
            // Single slope for all
            for (long i = 0; i < size; i++) {
                data[i] = leaky_relu(data[i], m_slopes[0]);
            }
        }
        // For per-channel, use apply(Matrix&)
    }

    void setSlopes(const std::vector<float>& slopes) {
        m_slopes = slopes;
    }

private:
    std::vector<float> m_slopes;
};

/**
 * Sigmoid activation (using std::exp)
 */
class ActivationSigmoid : public Activation {
public:
    void apply(float* data, long size) override {
        for (long i = 0; i < size; i++) {
            data[i] = sigmoid(data[i]);
        }
    }
};

/**
 * Fast sigmoid activation (using fast_tanh)
 */
class ActivationFastSigmoid : public Activation {
public:
    void apply(float* data, long size) override {
#if NAM_USE_SIMD
        // SIMD path
        long i = 0;
        for (; i + 3 < size; i += 4) {
            rack::simd::float_4 v = rack::simd::float_4::load(data + i);
            // fast_sigmoid(x) = 0.5 * (fast_tanh(x * 0.5) + 1)
            v = v * 0.5f;
            v = rack::simd::tanh(v);
            v = (v + 1.0f) * 0.5f;
            v.store(data + i);
        }
        for (; i < size; i++) {
            data[i] = fast_sigmoid(data[i]);
        }
#else
        for (long i = 0; i < size; i++) {
            data[i] = fast_sigmoid(data[i]);
        }
#endif
    }
};

/**
 * Swish activation (SiLU)
 */
class ActivationSwish : public Activation {
public:
    void apply(float* data, long size) override {
        for (long i = 0; i < size; i++) {
            data[i] = swish(data[i]);
        }
    }
};

/**
 * Fast swish activation
 */
class ActivationFastSwish : public Activation {
public:
    void apply(float* data, long size) override {
        for (long i = 0; i < size; i++) {
            data[i] = fast_swish(data[i]);
        }
    }
};

/**
 * Hard swish activation
 */
class ActivationHardSwish : public Activation {
public:
    void apply(float* data, long size) override {
        for (long i = 0; i < size; i++) {
            data[i] = hard_swish(data[i]);
        }
    }
};

/**
 * Lookup table (LUT) activation for maximum performance
 *
 * Pre-computes activation values and uses linear interpolation
 */
class ActivationLUT : public Activation {
public:
    ActivationLUT(float min_x, float max_x, size_t size,
                  std::function<float(float)> func)
        : m_minX(min_x), m_maxX(max_x), m_size(size) {

        m_step = (max_x - min_x) / static_cast<float>(size - 1);
        m_invStep = 1.0f / m_step;

        m_table.reserve(size);
        for (size_t i = 0; i < size; i++) {
            m_table.push_back(func(min_x + static_cast<float>(i) * m_step));
        }
    }

    // Fast lookup with linear interpolation
    inline float lookup(float x) const {
        // Clamp input to range
        if (x <= m_minX) return m_table.front();
        if (x >= m_maxX) return m_table.back();

        // Calculate float index
        float f_idx = (x - m_minX) * m_invStep;
        size_t i = static_cast<size_t>(f_idx);

        // Handle edge case at max
        if (i >= m_size - 1) return m_table.back();

        // Linear interpolation
        float frac = f_idx - static_cast<float>(i);
        return m_table[i] + (m_table[i + 1] - m_table[i]) * frac;
    }

    void apply(float* data, long size) override {
        for (long i = 0; i < size; i++) {
            data[i] = lookup(data[i]);
        }
    }

    // Create LUT for tanh (common case)
    static ActivationLUT* createTanhLUT(float min_x = -10.0f, float max_x = 10.0f,
                                        size_t size = 1024) {
        return new ActivationLUT(min_x, max_x, size, [](float x) {
            return std::tanh(x);
        });
    }

    // Create LUT for sigmoid (common case)
    static ActivationLUT* createSigmoidLUT(float min_x = -10.0f, float max_x = 10.0f,
                                           size_t size = 1024) {
        return new ActivationLUT(min_x, max_x, size, [](float x) {
            return 1.0f / (1.0f + std::exp(-x));
        });
    }

private:
    float m_minX, m_maxX, m_step, m_invStep;
    size_t m_size;
    std::vector<float> m_table;
};

// ============================================================================
// Activation registry and factory
// ============================================================================

// Global flag to use fast tanh instead of std::tanh
static bool g_useFastTanh = true;

inline void enableFastTanh() { g_useFastTanh = true; }
inline void disableFastTanh() { g_useFastTanh = false; }
inline bool isUsingFastTanh() { return g_useFastTanh; }

inline Activation* Activation::get(const std::string& name) {
    // Create activations on demand
    static ActivationIdentity s_identity;
    static ActivationTanh s_tanh;
    static ActivationFastTanh s_fastTanh;
    static ActivationHardTanh s_hardTanh;
    static ActivationLeakyHardTanh s_leakyHardTanh;
    static ActivationReLU s_relu;
    static ActivationLeakyReLU s_leakyRelu;
    static ActivationPReLU s_prelu(0.01f);
    static ActivationSigmoid s_sigmoid;
    static ActivationFastSigmoid s_fastSigmoid;
    static ActivationSwish s_swish;
    static ActivationHardSwish s_hardSwish;

    if (name == "Identity" || name == "identity") return &s_identity;
    if (name == "Tanh" || name == "tanh") {
        return g_useFastTanh ? static_cast<Activation*>(&s_fastTanh)
                             : static_cast<Activation*>(&s_tanh);
    }
    if (name == "FastTanh" || name == "fast_tanh" || name == "Fasttanh") return &s_fastTanh;
    if (name == "HardTanh" || name == "hard_tanh" || name == "Hardtanh") return &s_hardTanh;
    if (name == "ReLU" || name == "relu") return &s_relu;
    if (name == "LeakyReLU" || name == "leaky_relu") return &s_leakyRelu;
    if (name == "Sigmoid" || name == "sigmoid") return &s_sigmoid;
    if (name == "FastSigmoid" || name == "fast_sigmoid") return &s_fastSigmoid;
    if (name == "Swish" || name == "swish" || name == "SiLU") return &s_swish;
    if (name == "HardSwish" || name == "hard_swish" || name == "hswish" || name == "Hardswish") return &s_hardSwish;
    if (name == "LeakyHardTanh" || name == "LeakyHardtanh" || name == "leaky_hardtanh") return &s_leakyHardTanh;
    if (name == "PReLU" || name == "prelu") return &s_prelu;

    // Default to identity if unknown
    return &s_identity;
}

} // namespace activations
} // namespace nam
