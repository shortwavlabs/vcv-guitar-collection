#pragma once

/**
 * Linear model for NAM (simple FIR filter / impulse response)
 *
 * Features:
 * - C++11 compatible
 * - No Eigen dependency
 * - Simple dot product implementation
 */

#include <vector>
#include "dsp.h"

namespace nam {

class Linear : public Buffer {
public:
    /**
     * Construct a Linear model
     *
     * @param receptive_field Length of impulse response
     * @param bias Whether to use bias
     * @param weights Impulse response weights
     * @param expected_sample_rate Expected sample rate
     */
    Linear(int receptive_field, bool bias, const std::vector<float>& weights,
           double expected_sample_rate = -1.0);

    void process(NAM_SAMPLE* input, NAM_SAMPLE* output, int num_frames) override;

private:
    std::vector<float> mWeights;  // Impulse response
    float mBias;
};

namespace linear {

/**
 * Factory function to create Linear model from config
 *
 * @param receptive_field Length of impulse response
 * @param bias Whether model has bias
 * @param weights Model weights
 * @param expected_sample_rate Expected sample rate
 * @return Unique pointer to Linear DSP
 */
std::unique_ptr<DSP> create(int receptive_field, bool bias,
                            std::vector<float>& weights,
                            double expected_sample_rate = -1.0);

} // namespace linear

} // namespace nam
