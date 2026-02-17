#include "wavenet.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>

namespace nam {
namespace wavenet {

// ============================================================================
// Layer
// ============================================================================

Layer::Layer(int condition_size, int channels, int bottleneck, int kernel_size,
             int dilation, const std::string& activation, bool gated,
             int groups_input, int groups_1x1)
    : mConv(channels, gated ? 2 * bottleneck : bottleneck, kernel_size, true, dilation, groups_input)
    , mInputMixin(condition_size, gated ? 2 * bottleneck : bottleneck, false)
    , m1x1(bottleneck, channels, true, groups_1x1)
    , mActivation(activations::Activation::get(activation))
    , mGated(gated)
    , mBottleneck(bottleneck) {
}

void Layer::setMaxBufferSize(int maxBufferSize) {
    mConv.setMaxBufferSize(maxBufferSize);
    mInputMixin.setMaxBufferSize(maxBufferSize);

    long z_channels = mConv.getOutChannels();
    mZ.resize(z_channels, maxBufferSize);
    mZ.setZero();
    mTopRows.resize(mBottleneck, maxBufferSize);
    mTopRows.setZero();

    m1x1.setMaxBufferSize(maxBufferSize);

    long channels = getChannels();
    mOutputNextLayer.resize(channels, maxBufferSize);
    mOutputHead.resize(mBottleneck, maxBufferSize);
}

void Layer::setWeights(std::vector<float>::iterator& weights) {
    mConv.setWeights(weights);
    mInputMixin.setWeights(weights);
    m1x1.setWeights(weights);
}

void Layer::process(const Matrix& input, const Matrix& condition, int num_frames) {
    // Step 1: Input convolutions
    mConv.process(input, num_frames);
    mInputMixin.process(condition, num_frames);

    // Add conv outputs: z = conv_out + mixin_out
    const Matrix& conv_out = mConv.getOutput();
    const Matrix& mixin_out = mInputMixin.getOutput();

    const int z_rows = mZ.rows();
    for (int f = 0; f < num_frames; f++) {
        float* z_col = mZ.col(f);
        const float* conv_col = conv_out.col(f);
        const float* mix_col = mixin_out.col(f);
        for (int c = 0; c < z_rows; c++) {
            z_col[c] = conv_col[c] + mix_col[c];
        }
    }

    // Step 2 & 3: Activation and 1x1
    if (!mGated) {
        // Simple activation on active frames only
        for (int f = 0; f < num_frames; f++) {
            mActivation->apply(mZ.col(f), mZ.rows());
        }

        // Store output to head (skip connection)
        const size_t headBytes = static_cast<size_t>(mBottleneck) * sizeof(float);
        for (int f = 0; f < num_frames; f++) {
            std::memcpy(mOutputHead.col(f), mZ.col(f), headBytes);
        }

        m1x1.process(mZ, num_frames);
    } else {
        // Gated activation: tanh(z[0:b]) * sigmoid(z[b:2b])
        activations::Activation* sigmoidActivation = activations::Activation::get("Sigmoid");
        for (int f = 0; f < num_frames; f++) {
            float* col = mZ.col(f);
            mActivation->apply(col, mBottleneck);
            sigmoidActivation->apply(col + mBottleneck, mBottleneck);
            float* top_col = mTopRows.col(f);
            float* head_col = mOutputHead.col(f);
            for (int c = 0; c < mBottleneck; c++) {
                const float v = col[c] * col[mBottleneck + c];
                top_col[c] = v;
                head_col[c] = v;
            }
        }

        // Process through 1x1 with just top rows (already packed)
        m1x1.process(mTopRows, num_frames);
    }

    // Store output to next layer (residual: input + 1x1 output)
    const Matrix& out1x1 = m1x1.getOutput();
    const int channels = static_cast<int>(getChannels());
    for (int f = 0; f < num_frames; f++) {
        const float* in_col = input.col(f);
        const float* out1x1_col = out1x1.col(f);
        float* next_col = mOutputNextLayer.col(f);
        for (int c = 0; c < channels; c++) {
            next_col[c] = in_col[c] + out1x1_col[c];
        }
    }
}

// ============================================================================
// LayerArray
// ============================================================================

LayerArray::LayerArray(const LayerArrayConfig& config)
    : mRechannel(config.inputSize, config.channels, false)
    , mHeadRechannel(config.bottleneck, config.headSize, config.headBias)
    , mBottleneck(config.bottleneck) {

    for (size_t i = 0; i < config.dilations.size(); i++) {
        mLayers.push_back(Layer(
            config.conditionSize, config.channels, config.bottleneck,
            config.kernelSize, config.dilations[i], config.activation,
            config.gated, config.groupsInput, config.groups1x1
        ));
    }
}

void LayerArray::setMaxBufferSize(int maxBufferSize) {
    mRechannel.setMaxBufferSize(maxBufferSize);
    mHeadRechannel.setMaxBufferSize(maxBufferSize);

    for (auto& layer : mLayers) {
        layer.setMaxBufferSize(maxBufferSize);
    }

    long channels = mLayers.size() > 0 ? mLayers[0].getChannels() : 0;
    mLayerOutputs.resize(channels, maxBufferSize);
    mHeadInputs.resize(mBottleneck, maxBufferSize);
}

void LayerArray::setWeights(std::vector<float>::iterator& weights) {
    mRechannel.setWeights(weights);
    for (auto& layer : mLayers) {
        layer.setWeights(weights);
    }
    mHeadRechannel.setWeights(weights);
}

long LayerArray::getReceptiveField() const {
    long result = 0;
    for (const auto& layer : mLayers) {
        result += layer.getDilation() * (layer.getKernelSize() - 1);
    }
    return result;
}

void LayerArray::process(const Matrix& layer_inputs, const Matrix& condition, int num_frames) {
    // Zero head inputs (first layer array)
    mHeadInputs.setZero();
    processInner(layer_inputs, condition, num_frames);
}

void LayerArray::process(const Matrix& layer_inputs, const Matrix& condition,
                         const Matrix& head_inputs, int num_frames) {
    // Copy head inputs from previous layer array
    mHeadInputs.setZero();
    const int head_rows = std::min(mHeadInputs.rows(), head_inputs.rows());
    if (head_rows > 0) {
        const size_t headBytes = static_cast<size_t>(head_rows) * sizeof(float);
        for (int f = 0; f < num_frames; f++) {
            std::memcpy(mHeadInputs.col(f), head_inputs.col(f), headBytes);
        }
    }
    processInner(layer_inputs, condition, num_frames);
}

void LayerArray::processInner(const Matrix& layer_inputs, const Matrix& condition, int num_frames) {
    // Process rechannel
    mRechannel.process(layer_inputs, num_frames);
    Matrix& rechannel_output = mRechannel.getOutput();

    const int head_rows = mHeadInputs.rows();

    // Process layers
    for (size_t i = 0; i < mLayers.size(); i++) {
        if (i == 0) {
            mLayers[i].process(rechannel_output, condition, num_frames);
        } else {
            mLayers[i].process(mLayers[i - 1].getOutputNextLayer(), condition, num_frames);
        }

        // Accumulate head output
        Matrix& head_out = mLayers[i].getOutputHead();
        const int accum_rows = std::min(head_rows, head_out.rows());
        for (int f = 0; f < num_frames; f++) {
            float* head_in_col = mHeadInputs.col(f);
            const float* head_out_col = head_out.col(f);
            for (int c = 0; c < accum_rows; c++) {
                head_in_col[c] += head_out_col[c];
            }
        }
    }

    // Store output from last layer
    if (!mLayers.empty()) {
        Matrix& last_out = mLayers.back().getOutputNextLayer();
        const size_t rowBytes = static_cast<size_t>(mLayerOutputs.rows()) * sizeof(float);
        for (int f = 0; f < num_frames; f++) {
            std::memcpy(mLayerOutputs.col(f), last_out.col(f), rowBytes);
        }
    }

    // Process head rechannel
    mHeadRechannel.process(mHeadInputs, num_frames);
}

// ============================================================================
// WaveNet
// ============================================================================

WaveNet::WaveNet(const std::vector<LayerArrayConfig>& layer_array_configs,
                 float head_scale, bool with_head, std::vector<float>& weights,
                 double expected_sample_rate)
    : DSP(expected_sample_rate)
    , mHeadScale(head_scale) {

    if (with_head) {
        throw std::runtime_error("WaveNet head not implemented!");
    }

    for (const auto& config : layer_array_configs) {
        mLayerArrays.push_back(LayerArray(config));
    }

    setWeights(weights);

    // Compute prewarm samples
    mPrewarmSamples = 1;
    for (const auto& arr : mLayerArrays) {
        mPrewarmSamples += static_cast<int>(arr.getReceptiveField());
    }
}

void WaveNet::setWeights(std::vector<float>& weights) {
    auto it = weights.begin();
    for (auto& arr : mLayerArrays) {
        arr.setWeights(it);
    }
    mHeadScale = *(it++);

    if (it != weights.end()) {
        throw std::runtime_error("Weight mismatch: not all weights were used");
    }
}

void WaveNet::setMaxBufferSize(int maxBufferSize) {
    DSP::setMaxBufferSize(maxBufferSize);

    mCondition.resize(getConditionDim(), maxBufferSize);

    for (auto& arr : mLayerArrays) {
        arr.setMaxBufferSize(maxBufferSize);
    }
}

void WaveNet::setConditionArray(NAM_SAMPLE* input, int num_frames) {
    for (int f = 0; f < num_frames; f++) {
        mCondition(0, f) = static_cast<float>(input[f]);
    }
}

void WaveNet::process(NAM_SAMPLE* input, NAM_SAMPLE* output, int num_frames) {
    // Set up conditioning
    setConditionArray(input, num_frames);

    // Process through layer arrays
    for (size_t i = 0; i < mLayerArrays.size(); i++) {
        if (i == 0) {
            // First array: input is just the condition
            mLayerArrays[i].process(mCondition, mCondition, num_frames);
        } else {
            // Subsequent arrays: use previous layer output and head output
            mLayerArrays[i].process(
                mLayerArrays[i - 1].getLayerOutputs(),
                mCondition,
                mLayerArrays[i - 1].getHeadOutputs(),
                num_frames
            );
        }
    }

    // Get final output from head rechannel
    Matrix& head_output = mLayerArrays.back().getHeadOutputs();

    // Copy to output with head scale
    for (int f = 0; f < num_frames; f++) {
        output[f] = static_cast<NAM_SAMPLE>(mHeadScale * head_output(0, f));
    }
}

// Factory
std::unique_ptr<DSP> create(const std::vector<LayerArrayConfig>& configs,
                            float head_scale, bool with_head,
                            std::vector<float>& weights,
                            double expected_sample_rate) {
    return std::unique_ptr<DSP>(
        new WaveNet(configs, head_scale, with_head, weights, expected_sample_rate)
    );
}

} // namespace wavenet
} // namespace nam
