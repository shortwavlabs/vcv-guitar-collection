#pragma once

/**
 * WaveNet architecture for NAM
 *
 * Features:
 * - C++11 compatible
 * - Dilated convolutions with gating
 * - Skip connections and residual connections
 * - Multiple layer arrays
 */

#include <vector>
#include <string>
#include <memory>
#include "dsp.h"
#include "conv1d.h"
#include "conv1x1.h"
#include "activations.h"

namespace nam {
namespace wavenet {

/**
 * Single WaveNet layer
 *
 * Processes: dilated conv + input mixin -> activation -> 1x1 conv
 * Outputs: residual (for next layer) and skip (for head)
 */
class Layer {
public:
    Layer(int condition_size, int channels, int bottleneck, int kernel_size,
          int dilation, const std::string& activation, bool gated,
          int groups_input, int groups_1x1);

    void setMaxBufferSize(int maxBufferSize);
    void setWeights(std::vector<float>::iterator& weights);
    void process(const Matrix& input, const Matrix& condition, int num_frames);

    // Accessors
    long getChannels() const { return mConv.getInChannels(); }
    int getDilation() const { return mConv.getDilation(); }
    long getKernelSize() const { return mConv.getKernelSize(); }
    Matrix& getOutputNextLayer() { return mOutputNextLayer; }
    Matrix& getOutputHead() { return mOutputHead; }

private:
    Conv1D mConv;              // Dilated convolution
    Conv1x1 mInputMixin;       // Condition input mixing
    Conv1x1 m1x1;              // Output projection
    Matrix mZ;                 // Internal activation state
    Matrix mTopRows;           // Preallocated gated top-row scratch
    Matrix mOutputNextLayer;   // Residual output (input + 1x1 output)
    Matrix mOutputHead;        // Skip output (activated conv output)

    activations::Activation* mActivation;
    activations::Activation* mSigmoidActivation;
    bool mGated;
    int mBottleneck;
};

/**
 * Layer array configuration
 */
struct LayerArrayConfig {
    int inputSize;
    int conditionSize;
    int headSize;
    int channels;
    int bottleneck;
    int kernelSize;
    std::vector<int> dilations;
    std::string activation;
    bool gated;
    bool headBias;
    int groupsInput;
    int groups1x1;
};

/**
 * Array of layers with same configuration
 */
class LayerArray {
public:
    LayerArray(const LayerArrayConfig& config);

    void setMaxBufferSize(int maxBufferSize);
    void setWeights(std::vector<float>::iterator& weights);

    // Process without head input (first layer array)
    void process(const Matrix& layer_inputs, const Matrix& condition, int num_frames);
    // Process with head input from previous layer array
    void process(const Matrix& layer_inputs, const Matrix& condition,
                 const Matrix& head_inputs, int num_frames);

    Matrix& getLayerOutputs() { return mLayerOutputs; }
    Matrix& getHeadOutputs() { return mHeadRechannel.getOutput(); }
    long getReceptiveField() const;

private:
    Conv1x1 mRechannel;        // Input rechanneling
    std::vector<Layer> mLayers;
    Matrix mLayerOutputs;      // Output to next layer array
    Matrix mHeadInputs;        // Accumulated skip connections
    Conv1x1 mHeadRechannel;    // Head rechanneling
    int mBottleneck;

    void processInner(const Matrix& layer_inputs, const Matrix& condition, int num_frames);
};

/**
 * Full WaveNet model
 */
class WaveNet : public DSP {
public:
    WaveNet(const std::vector<LayerArrayConfig>& layer_array_configs,
            float head_scale, bool with_head, std::vector<float>& weights,
            double expected_sample_rate = -1.0);

    void process(NAM_SAMPLE* input, NAM_SAMPLE* output, int num_frames) override;

protected:
    void setMaxBufferSize(int maxBufferSize) override;
    int prewarmSamples() override { return mPrewarmSamples; }

private:
    std::vector<LayerArray> mLayerArrays;
    Matrix mCondition;  // Conditioning input
    float mHeadScale;
    int mPrewarmSamples;

    void setWeights(std::vector<float>& weights);
    int getConditionDim() const { return 1; }  // Mono audio
    void setConditionArray(NAM_SAMPLE* input, int num_frames);
};

// Factory function
std::unique_ptr<DSP> create(const std::vector<LayerArrayConfig>& configs,
                            float head_scale, bool with_head,
                            std::vector<float>& weights,
                            double expected_sample_rate = -1.0);

} // namespace wavenet
} // namespace nam
