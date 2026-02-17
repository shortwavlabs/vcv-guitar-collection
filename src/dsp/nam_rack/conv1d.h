#pragma once

/**
 * 1D Dilated Convolution for NAM
 *
 * Features:
 * - C++11 compatible
 * - No Eigen dependency - uses nam_rack::Matrix
 * - Supports grouped convolution (for WaveNet)
 * - Dilated convolution with ring buffer for temporal memory
 * - Pre-allocated buffers for real-time safety
 */

#include <vector>
#include <stdexcept>
#include "matrix.h"
#include "ring_buffer.h"

namespace nam {

class Conv1D {
public:
    Conv1D();
    Conv1D(int in_channels, int out_channels, int kernel_size,
           bool bias, int dilation, int groups = 1);

    /**
     * Set layer dimensions
     */
    void setSize(int in_channels, int out_channels, int kernel_size,
                 bool do_bias, int dilation, int groups = 1);

    /**
     * Set weights from a flat iterator
     * Iterator is advanced by the number of weights consumed
     */
    void setWeights(std::vector<float>::iterator& weights);

    /**
     * Convenience: set size and weights in one call
     */
    void setSizeAndWeights(int in_channels, int out_channels, int kernel_size,
                           int dilation, bool do_bias, int groups,
                           std::vector<float>::iterator& weights);

    /**
     * Initialize buffers for a given max buffer size
     * Must be called before Process()
     */
    void setMaxBufferSize(int maxBufferSize);

    /**
     * Process input through the convolution
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
    long getInChannels() const;
    long getOutChannels() const;
    long getKernelSize() const { return static_cast<long>(_weight.size()); }
    long getNumWeights() const;
    int getDilation() const { return _dilation; }
    int getNumGroups() const { return _numGroups; }
    bool hasBias() const { return _bias.size() > 0; }

private:
    // Weight matrices: one per kernel position
    // Each matrix is (out_channels x in_channels)
    std::vector<Matrix> _weight;

    // Bias vector (out_channels)
    Vector _bias;

    // Configuration
    int _dilation;
    int _numGroups;

    // Runtime buffers
    RingBuffer _inputBuffer;  // Ring buffer for dilated convolution
    Matrix _output;           // Pre-allocated output buffer
    int _maxBufferSize;

    // Helper for grouped convolution
    void processGrouped(const Matrix& input, int num_frames);
};

} // namespace nam
