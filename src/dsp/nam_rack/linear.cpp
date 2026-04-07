#include "linear.h"

namespace nam {

Linear::Linear(int receptive_field, bool bias, const std::vector<float>& weights,
               double expected_sample_rate)
    : Buffer(receptive_field, expected_sample_rate) {

    // Extract impulse response weights
    mWeights.resize(receptive_field);
    std::copy(weights.begin(), weights.begin() + receptive_field, mWeights.begin());

    // Extract bias if present
    if (bias && weights.size() > static_cast<size_t>(receptive_field)) {
        mBias = weights[receptive_field];
    } else {
        mBias = 0.0f;
    }
}

void Linear::process(NAM_SAMPLE* input, NAM_SAMPLE* output, int num_frames) {
    // Update input buffer
    updateBuffers(input, num_frames);

    // Process each output sample
    for (int i = 0; i < num_frames; i++) {
        // Compute dot product: sum of weight[j] * input[i - j]
        float sum = mBias;
        const long input_pos = mInputBufferOffset + i;

        for (int j = 0; j < mReceptiveField; j++) {
            // Input index goes backwards (convolution)
            const long idx = input_pos - j;
            if (idx >= 0) {
                sum += mWeights[j] * mInputBuffer[idx];
            }
        }

        output[i] = static_cast<NAM_SAMPLE>(sum);
    }

    // Advance buffer
    advanceInputBuffer(num_frames);
}

namespace linear {

std::unique_ptr<DSP> create(int receptive_field, bool bias,
                            std::vector<float>& weights,
                            double expected_sample_rate) {
    return std::unique_ptr<DSP>(
        new Linear(receptive_field, bias, weights, expected_sample_rate)
    );
}

} // namespace linear

} // namespace nam
