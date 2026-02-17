#include "lstm.h"
#include <stdexcept>
#include <cmath>

namespace nam {
namespace lstm {

// ============================================================================
// LSTMCell
// ============================================================================

LSTMCell::LSTMCell(int input_size, int hidden_size, std::vector<float>::iterator& weights)
    : mInputSize(input_size)
    , mHiddenSize(hidden_size) {

    // Weight matrix: (4*hidden_size) x (input_size + hidden_size)
    // Row-major order to match PyTorch export
    int rows = 4 * hidden_size;
    int cols = input_size + hidden_size;
    mW.resize(rows, cols);

    // Load weights in row-major order
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            mW(i, j) = *(weights++);
        }
    }

    // Bias: (4*hidden_size)
    mB.resize(rows);
    for (int i = 0; i < rows; i++) {
        mB(i) = *(weights++);
    }

    // Initialize state vectors
    mXH.resize(cols);
    mXH.setZero();
    mIFGO.resize(rows);
    mIFGO.setZero();
    mCellState.resize(hidden_size);
    mCellState.setZero();
    mHiddenState.resize(hidden_size);
    mHiddenState.setZero();

    // Load initial hidden state
    int h_offset = input_size;
    for (int i = 0; i < hidden_size; i++) {
        mXH(h_offset + i) = *(weights++);
    }

    // Load initial cell state
    for (int i = 0; i < hidden_size; i++) {
        mCellState(i) = *(weights++);
    }
}

void LSTMCell::process(const Vector& x) {
    // Copy input to xh
    for (int i = 0; i < mInputSize; i++) {
        mXH(i) = x(i);
    }
    // Hidden state is already in xh at position [input_size:]

    // Matrix multiply: ifgo = W * xh + b
    // mW is (4*hidden_size) x (input_size + hidden_size)
    // mXH is (input_size + hidden_size)
    // Result is (4*hidden_size)
    for (int i = 0; i < 4 * mHiddenSize; i++) {
        float sum = mB(i);
        for (int j = 0; j < mInputSize + mHiddenSize; j++) {
            sum += mW(i, j) * mXH(j);
        }
        mIFGO(i) = sum;
    }

    // Gate offsets
    const int i_offset = 0;
    const int f_offset = mHiddenSize;
    const int g_offset = 2 * mHiddenSize;
    const int o_offset = 3 * mHiddenSize;
    const int h_offset = mInputSize;

    // Apply gates
    // c = sigmoid(f) * c + sigmoid(i) * tanh(g)
    // h = sigmoid(o) * tanh(c)
    for (int i = 0; i < mHiddenSize; i++) {
        float i_gate = activations::fast_sigmoid(mIFGO(i_offset + i));
        float f_gate = activations::fast_sigmoid(mIFGO(f_offset + i));
        float g_gate = activations::fast_tanh(mIFGO(g_offset + i));
        float o_gate = activations::fast_sigmoid(mIFGO(o_offset + i));

        mCellState(i) = f_gate * mCellState(i) + i_gate * g_gate;
        mHiddenState(i) = o_gate * activations::fast_tanh(mCellState(i));

        // Update xh for next iteration
        mXH(h_offset + i) = mHiddenState(i);
    }
}

// ============================================================================
// LSTM
// ============================================================================

LSTM::LSTM(int num_layers, int input_size, int hidden_size,
           std::vector<float>& weights, double expected_sample_rate)
    : DSP(expected_sample_rate)
    , mHeadBias(0.0f) {

    mInput.resize(1);

    auto it = weights.begin();

    // Create LSTM layers
    for (int i = 0; i < num_layers; i++) {
        int layer_input_size = (i == 0) ? input_size : hidden_size;
        mLayers.push_back(LSTMCell(layer_input_size, hidden_size, it));
    }

    // Load head weights
    mHeadWeight.resize(hidden_size);
    for (int i = 0; i < hidden_size; i++) {
        mHeadWeight(i) = *(it++);
    }
    mHeadBias = *(it++);

    if (it != weights.end()) {
        throw std::runtime_error("Weight mismatch: not all weights were used for LSTM");
    }
}

int LSTM::prewarmSamples() {
    // Half-second prewarm (matches original NAM)
    int result = static_cast<int>(0.5 * mExpectedSampleRate);
    return result <= 0 ? 1 : result;
}

void LSTM::process(NAM_SAMPLE* input, NAM_SAMPLE* output, int num_frames) {
    for (int i = 0; i < num_frames; i++) {
        output[i] = static_cast<NAM_SAMPLE>(processSample(static_cast<float>(input[i])));
    }
}

float LSTM::processSample(float x) {
    if (mLayers.empty()) {
        return x;
    }

    // Set input
    mInput(0) = x;

    // Process through layers
    mLayers[0].process(mInput);
    for (size_t i = 1; i < mLayers.size(); i++) {
        mLayers[i].process(mLayers[i - 1].getHiddenState());
    }

    // Compute output: head_weight.dot(hidden_state) + head_bias
    const Vector& h = mLayers.back().getHiddenState();
    float result = mHeadBias;
    for (int i = 0; i < mHeadWeight.size(); i++) {
        result += mHeadWeight(i) * h(i);
    }

    return result;
}

// Factory
std::unique_ptr<DSP> create(int num_layers, int input_size, int hidden_size,
                            std::vector<float>& weights,
                            double expected_sample_rate) {
    return std::unique_ptr<DSP>(
        new LSTM(num_layers, input_size, hidden_size, weights, expected_sample_rate)
    );
}

} // namespace lstm
} // namespace nam
