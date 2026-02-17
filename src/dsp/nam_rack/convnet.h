#pragma once

/**
 * ConvNet architecture for NAM
 *
 * Features:
 * - C++11 compatible
 * - Stack of dilated Conv1D blocks with optional batch normalization
 * - Head layer for final output
 */

#include <vector>
#include <string>
#include <memory>
#include "dsp.h"
#include "conv1d.h"
#include "activations.h"

namespace nam {
namespace convnet {

/**
 * Batch normalization layer (inference mode - just affine transform)
 */
class BatchNorm {
public:
    BatchNorm() {}
    BatchNorm(int dim, std::vector<float>::iterator& weights);

    void process(Matrix& input, int start_col, int end_col) const;

private:
    Vector mScale;  // scale = weight / sqrt(eps + running_var)
    Vector mLoc;    // loc = bias - scale * running_mean
};

/**
 * ConvNet block: Conv1D -> (optional) BatchNorm -> Activation
 */
class ConvNetBlock {
public:
    ConvNetBlock() : mBatchNorm(false), mActivation(nullptr) {}

    void setWeights(int in_channels, int out_channels, int dilation,
                    bool batchnorm, const std::string& activation, int groups,
                    std::vector<float>::iterator& weights);

    void setMaxBufferSize(int maxBufferSize);

    void process(const Matrix& input, int num_frames);

    Matrix& getOutput() { return mConv.getOutput(); }
    long getOutChannels() const { return mConv.getOutChannels(); }

private:
    Conv1D mConv;
    BatchNorm mBatchNormLayer;
    bool mBatchNorm;
    activations::Activation* mActivation;
};

/**
 * Head layer: weighted sum + bias
 */
class Head {
public:
    Head() : mBias(0.0f) {}
    Head(int channels, std::vector<float>::iterator& weights);

    void process(const Matrix& input, Vector& output, int start_col, int num_frames) const;

private:
    Vector mWeight;
    float mBias;
};

/**
 * Full ConvNet model
 */
class ConvNet : public Buffer {
public:
    ConvNet(int channels, const std::vector<int>& dilations, bool batchnorm,
            const std::string& activation, std::vector<float>& weights,
            double expected_sample_rate = -1.0, int groups = 1);

    void process(NAM_SAMPLE* input, NAM_SAMPLE* output, int num_frames) override;

protected:
    void setMaxBufferSize(int maxBufferSize) override;
    int prewarmSamples() override { return mPrewarmSamples; }

private:
    std::vector<ConvNetBlock> mBlocks;
    Matrix mInputMatrix;  // Reused single-channel input view for first block
    Matrix mBlockVal;  // For head input
    Vector mHeadOutput;
    Head mHead;
    int mPrewarmSamples;

    void verifyWeights(int channels, const std::vector<int>& dilations,
                       bool batchnorm, size_t actual_weights);
};

// Factory function
std::unique_ptr<DSP> create(int channels, const std::vector<int>& dilations,
                            bool batchnorm, const std::string& activation,
                            std::vector<float>& weights,
                            double expected_sample_rate = -1.0, int groups = 1);

} // namespace convnet
} // namespace nam
