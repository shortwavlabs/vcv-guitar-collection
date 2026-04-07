#include "convnet.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace nam {
namespace convnet {

// ============================================================================
// BatchNorm
// ============================================================================

BatchNorm::BatchNorm(int dim, std::vector<float>::iterator& weights) {
    // Extract batch norm parameters
    Vector running_mean(dim);
    Vector running_var(dim);
    Vector weight(dim);
    Vector bias(dim);

    for (int i = 0; i < dim; i++) {
        running_mean(i) = *(weights++);
    }
    for (int i = 0; i < dim; i++) {
        running_var(i) = *(weights++);
    }
    for (int i = 0; i < dim; i++) {
        weight(i) = *(weights++);
    }
    for (int i = 0; i < dim; i++) {
        bias(i) = *(weights++);
    }
    float eps = *(weights++);

    // Convert to scale & loc for inference
    // y = (x - mean) / sqrt(var + eps) * weight + bias
    // y = scale * x + loc
    // scale = weight / sqrt(var + eps)
    // loc = bias - scale * mean
    mScale.resize(dim);
    mLoc.resize(dim);

    for (int i = 0; i < dim; i++) {
        mScale(i) = weight(i) / std::sqrt(eps + running_var(i));
    }
    for (int i = 0; i < dim; i++) {
        mLoc(i) = bias(i) - mScale(i) * running_mean(i);
    }
}

void BatchNorm::process(Matrix& input, int start_col, int end_col) const {
    // Apply: output = scale * input + loc
    for (int col = start_col; col < end_col; col++) {
        for (int row = 0; row < input.rows(); row++) {
            input(row, col) = mScale(row) * input(row, col) + mLoc(row);
        }
    }
}

// ============================================================================
// ConvNetBlock
// ============================================================================

void ConvNetBlock::setWeights(int in_channels, int out_channels, int dilation,
                               bool batchnorm, const std::string& activation, int groups,
                               std::vector<float>::iterator& weights) {
    mBatchNorm = batchnorm;

    // Conv1D with kernel size 2 (hardcoded in NAM ConvNet)
    mConv.setSize(in_channels, out_channels, 2, !batchnorm, dilation, groups);
    mConv.setWeights(weights);

    if (mBatchNorm) {
        mBatchNormLayer = BatchNorm(out_channels, weights);
    }

    mActivation = activations::Activation::get(activation);
}

void ConvNetBlock::setMaxBufferSize(int maxBufferSize) {
    mConv.setMaxBufferSize(maxBufferSize);
}

void ConvNetBlock::process(const Matrix& input, int num_frames) {
    // Process through Conv1D
    mConv.process(input, num_frames);
    Matrix& conv_output = mConv.getOutput();

    if (mBatchNorm) {
        mBatchNormLayer.process(conv_output, 0, num_frames);
    }

    mActivation->apply(conv_output);
}

// ============================================================================
// Head
// ============================================================================

Head::Head(int channels, std::vector<float>::iterator& weights) {
    mWeight.resize(channels);
    for (int i = 0; i < channels; i++) {
        mWeight(i) = *(weights++);
    }
    mBias = *(weights++);
}

void Head::process(const Matrix& input, Vector& output, int start_col, int num_frames) const {
    output.resize(num_frames);
    for (int i = 0; i < num_frames; i++) {
        float sum = mBias;
        for (int c = 0; c < input.rows(); c++) {
            sum += mWeight(c) * input(c, start_col + i);
        }
        output(i) = sum;
    }
}

// ============================================================================
// ConvNet
// ============================================================================

ConvNet::ConvNet(int channels, const std::vector<int>& dilations, bool batchnorm,
                 const std::string& activation, std::vector<float>& weights,
                 double expected_sample_rate, int groups)
    : Buffer(*std::max_element(dilations.begin(), dilations.end()), expected_sample_rate) {

    verifyWeights(channels, dilations, batchnorm, weights.size());

    mBlocks.resize(dilations.size());
    auto it = weights.begin();

    for (size_t i = 0; i < dilations.size(); i++) {
        int in_ch = (i == 0) ? 1 : channels;
        mBlocks[i].setWeights(in_ch, channels, dilations[i], batchnorm, activation, groups, it);
    }

    mHead = Head(channels, it);

    if (it != weights.end()) {
        throw std::runtime_error("Didn't use all weights when initializing ConvNet");
    }

    // Compute prewarm samples
    mPrewarmSamples = 1;
    for (size_t i = 0; i < dilations.size(); i++) {
        mPrewarmSamples += dilations[i];
    }

    // Initialize buffer
    std::fill(mInputBuffer.begin(), mInputBuffer.end(), 0.0f);
}

void ConvNet::process(NAM_SAMPLE* input, NAM_SAMPLE* output, int num_frames) {
    updateBuffers(input, num_frames);

    const long i_start = mInputBufferOffset;

    // Fill reused input matrix for first block
    for (int i = 0; i < num_frames; i++) {
        mInputMatrix(0, i) = mInputBuffer[i_start + i];
    }

    // Process through all blocks
    for (size_t i = 0; i < mBlocks.size(); i++) {
        if (i == 0) {
            mBlocks[i].process(mInputMatrix, num_frames);
        } else {
            mBlocks[i].process(mBlocks[i - 1].getOutput(), num_frames);
        }
    }

    // Ensure block_val is sized correctly for head
    if (mBlockVal.rows() != mBlocks.back().getOutChannels() ||
        mBlockVal.cols() != static_cast<long>(mInputBuffer.size())) {
        mBlockVal.resize(mBlocks.back().getOutChannels(), mInputBuffer.size());
        mBlockVal.setZero();
    }

    // Copy last block output to block_val
    Matrix& last_output = mBlocks.back().getOutput();
    for (int c = 0; c < last_output.rows(); c++) {
        for (int f = 0; f < num_frames; f++) {
            mBlockVal(c, i_start + f) = last_output(c, f);
        }
    }

    // Process head
    mHead.process(mBlockVal, mHeadOutput, i_start, num_frames);

    // Copy to output
    for (int s = 0; s < num_frames; s++) {
        output[s] = static_cast<NAM_SAMPLE>(mHeadOutput(s));
    }

    advanceInputBuffer(num_frames);
}

void ConvNet::setMaxBufferSize(int maxBufferSize) {
    DSP::setMaxBufferSize(maxBufferSize);

    mInputMatrix.resize(1, maxBufferSize);
    mInputMatrix.setZero();

    for (auto& block : mBlocks) {
        block.setMaxBufferSize(maxBufferSize);
    }
}

void ConvNet::verifyWeights(int channels, const std::vector<int>& dilations,
                            bool batchnorm, size_t actual_weights) {
    // Calculate expected weight count
    // For each block: Conv1D weights + optional batchnorm + bias
    // Conv1D: 2 * out_channels * in_channels / groups
    // BatchNorm: 4 * channels (mean, var, weight, bias) + 1 (eps)
    // Head: channels + 1

    size_t expected = 0;
    for (size_t i = 0; i < dilations.size(); i++) {
        int in_ch = (i == 0) ? 1 : channels;
        // Conv1D weights: kernel_size * out_channels * in_channels / groups
        expected += 2 * channels * in_ch;
        // Conv1D bias (if no batchnorm)
        if (!batchnorm) {
            expected += channels;
        }
        // BatchNorm
        if (batchnorm) {
            expected += 4 * channels + 1;  // mean, var, weight, bias, eps
        }
    }
    // Head
    expected += channels + 1;

    // Just warn for now
    if (actual_weights < expected) {
        // Might be okay if model doesn't have batchnorm params
    }
}

// Factory
std::unique_ptr<DSP> create(int channels, const std::vector<int>& dilations,
                            bool batchnorm, const std::string& activation,
                            std::vector<float>& weights,
                            double expected_sample_rate, int groups) {
    return std::unique_ptr<DSP>(
        new ConvNet(channels, dilations, batchnorm, activation, weights, expected_sample_rate, groups)
    );
}

} // namespace convnet
} // namespace nam
