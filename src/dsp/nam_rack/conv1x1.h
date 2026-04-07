#pragma once

/**
 * 1x1 Convolution for NAM (essentially matrix multiplication)
 *
 * Features:
 * - C++11 compatible
 * - No Eigen dependency - uses nam_rack::Matrix
 * - Supports grouped convolution
 * - Pre-allocated output buffer
 */

#include <vector>
#include <stdexcept>
#include "matrix.h"

namespace nam {

class Conv1x1 {
public:
    Conv1x1();
    Conv1x1(int in_channels, int out_channels, bool bias, int groups = 1);

    /**
     * Set weights from a flat iterator
     */
    void setWeights(std::vector<float>::iterator& weights);

    /**
     * Initialize output buffer
     */
    void setMaxBufferSize(int maxBufferSize);

    /**
     * Process input through the 1x1 convolution
     *
     * @param input Input matrix (channels x num_frames)
     * @param num_frames Number of frames to process
     */
    void process(const Matrix& input, int num_frames);

    /**
     * Get output buffer (out_channels x num_frames valid after process())
     */
    Matrix& getOutput() { return _output; }
    const Matrix& getOutput() const { return _output; }

    // Accessors
    long getInChannels() const { return _weight.cols(); }
    long getOutChannels() const { return _weight.rows(); }
    int getNumGroups() const { return _numGroups; }
    bool hasBias() const { return _doBias; }

private:
    Matrix _weight;      // Weight matrix (out_channels x in_channels)
    Vector _bias;        // Bias vector (out_channels)
    Matrix _output;      // Pre-allocated output buffer
    int _numGroups;
    bool _doBias;
};

} // namespace nam
