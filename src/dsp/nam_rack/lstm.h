#pragma once

/**
 * LSTM architecture for NAM
 *
 * Features:
 * - C++11 compatible
 * - Multi-layer LSTM with cell states
 * - Sample-by-sample processing
 */

#include <vector>
#include <memory>
#include "dsp.h"
#include "matrix.h"
#include "activations.h"

namespace nam {
namespace lstm {

/**
 * Single LSTM cell
 *
 * Implements: i, f, g, o gates with cell and hidden states
 * i = input gate
 * f = forget gate
 * g = cell gate (candidate values)
 * o = output gate
 */
class LSTMCell {
public:
    LSTMCell(int input_size, int hidden_size, std::vector<float>::iterator& weights);

    // Process one input sample, updating internal state
    void process(const Vector& x);

    // Get the hidden state output
    const Vector& getHiddenState() const { return mHiddenState; }
    int getHiddenSize() const { return mHiddenSize; }

private:
    // Parameters
    Matrix mW;      // Weight matrix: (4*hidden_size) x (input_size + hidden_size)
    Vector mB;      // Bias: (4*hidden_size)

    // State
    Vector mXH;         // Concatenated input and hidden state
    Vector mIFGO;       // Gate outputs: i, f, g, o concatenated
    Vector mCellState;  // Cell state
    Vector mHiddenState; // Hidden state (output)

    int mInputSize;
    int mHiddenSize;
};

/**
 * Multi-layer LSTM model
 */
class LSTM : public DSP {
public:
    LSTM(int num_layers, int input_size, int hidden_size,
         std::vector<float>& weights, double expected_sample_rate = -1.0);

    void process(NAM_SAMPLE* input, NAM_SAMPLE* output, int num_frames) override;

protected:
    void setMaxBufferSize(int maxBufferSize) override {}
    int prewarmSamples() override;

private:
    std::vector<LSTMCell> mLayers;
    Vector mHeadWeight;
    float mHeadBias;
    Vector mInput;  // Single sample input vector

    float processSample(float x);
};

// Factory function
std::unique_ptr<DSP> create(int num_layers, int input_size, int hidden_size,
                            std::vector<float>& weights,
                            double expected_sample_rate = -1.0);

} // namespace lstm
} // namespace nam
