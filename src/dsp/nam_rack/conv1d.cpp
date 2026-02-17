#include "conv1d.h"
#include <cstring>

namespace nam {

Conv1D::Conv1D()
    : _dilation(1)
    , _numGroups(1)
    , _maxBufferSize(0) {
}

Conv1D::Conv1D(int in_channels, int out_channels, int kernel_size,
               bool bias, int dilation, int groups)
    : _dilation(dilation)
    , _numGroups(groups)
    , _maxBufferSize(0) {
    setSize(in_channels, out_channels, kernel_size, bias, dilation, groups);
}

void Conv1D::setSize(int in_channels, int out_channels, int kernel_size,
                     bool do_bias, int dilation, int groups) {
    // Validate that channels divide evenly by groups
    if (in_channels % groups != 0) {
        throw std::runtime_error("in_channels (" + std::to_string(in_channels) +
                                 ") must be divisible by groups (" +
                                 std::to_string(groups) + ")");
    }
    if (out_channels % groups != 0) {
        throw std::runtime_error("out_channels (" + std::to_string(out_channels) +
                                 ") must be divisible by groups (" +
                                 std::to_string(groups) + ")");
    }

    _numGroups = groups;
    _dilation = dilation;

    // Allocate weight matrices: one per kernel position
    // Each matrix is (out_channels x in_channels) for y = W * x
    _weight.resize(kernel_size);
    for (size_t i = 0; i < _weight.size(); i++) {
        _weight[i].resize(out_channels, in_channels);
    }

    // Allocate bias if needed
    if (do_bias) {
        _bias.resize(out_channels);
    } else {
        _bias.resize(0);
    }
}

void Conv1D::setWeights(std::vector<float>::iterator& weights) {
    if (_weight.size() > 0) {
        const long out_channels = _weight[0].rows();
        const long in_channels = _weight[0].cols();
        const long out_per_group = out_channels / _numGroups;
        const long in_per_group = in_channels / _numGroups;

        // For grouped convolutions, weights are organized per group
        // Weight layout: for each kernel position k, weights are [group0, group1, ..., groupN-1]
        // Each group's weight matrix is (out_channels/numGroups, in_channels/numGroups)
        for (int g = 0; g < _numGroups; g++) {
            for (long i = 0; i < out_per_group; i++) {
                for (long j = 0; j < in_per_group; j++) {
                    for (size_t k = 0; k < _weight.size(); k++) {
                        _weight[k](g * out_per_group + i, g * in_per_group + j) = *(weights++);
                    }
                }
            }
        }
    }

    // Set bias
    for (long i = 0; i < _bias.size(); i++) {
        _bias(i) = *(weights++);
    }
}

void Conv1D::setSizeAndWeights(int in_channels, int out_channels, int kernel_size,
                                int dilation, bool do_bias, int groups,
                                std::vector<float>::iterator& weights) {
    setSize(in_channels, out_channels, kernel_size, do_bias, dilation, groups);
    setWeights(weights);
}

void Conv1D::setMaxBufferSize(int maxBufferSize) {
    _maxBufferSize = maxBufferSize;

    const long kernel_size = getKernelSize();
    const long receptive_field = kernel_size > 0 ? (kernel_size - 1) * _dilation : 0;
    const long in_channels = getInChannels();
    const long out_channels = getOutChannels();

    // Initialize input ring buffer with lookback for dilation
    _inputBuffer.setMaxLookback(receptive_field);
    _inputBuffer.reset(in_channels, maxBufferSize, receptive_field);

    // Pre-allocate output matrix
    _output.resize(out_channels, maxBufferSize);
    _output.setZero();
}

void Conv1D::process(const Matrix& input, int num_frames) {
    // Write input to ring buffer
    _inputBuffer.write(input, num_frames);

    // Initialize output before accumulation (bias or zero)
    const long out_channels = getOutChannels();
    if (_bias.size() > 0) {
        for (int f = 0; f < num_frames; f++) {
            float* out_col = _output.col(f);
            for (long c = 0; c < out_channels; c++) {
                out_col[c] = _bias(c);
            }
        }
    } else {
        for (int f = 0; f < num_frames; f++) {
            std::memset(_output.col(f), 0, static_cast<size_t>(out_channels) * sizeof(float));
        }
    }

    const long write_pos = _inputBuffer.getWritePos();
    const long in_channels = getInChannels();

    if (_numGroups == 1) {
        // Standard convolution (no grouping)
        for (size_t k = 0; k < _weight.size(); k++) {
            const long offset = _dilation * (static_cast<long>(k) + 1 - static_cast<long>(_weight.size()));
            const long lookback = -offset;
            const long read_pos = write_pos - lookback;

            if (in_channels == 1) {
                const float* w0 = _weight[k].col(0);
                for (int f = 0; f < num_frames; f++) {
                    const float* in_col = _inputBuffer.getCol(static_cast<int>(read_pos + f));
                    float* out_col = _output.col(f);
                    const float x0 = in_col[0];
                    for (long oc = 0; oc < out_channels; oc++) {
                        out_col[oc] += w0[oc] * x0;
                    }
                }
                continue;
            }

            if (in_channels == 2) {
                const float* w0 = _weight[k].col(0);
                const float* w1 = _weight[k].col(1);
                for (int f = 0; f < num_frames; f++) {
                    const float* in_col = _inputBuffer.getCol(static_cast<int>(read_pos + f));
                    float* out_col = _output.col(f);
                    const float x0 = in_col[0];
                    const float x1 = in_col[1];
                    for (long oc = 0; oc < out_channels; oc++) {
                        out_col[oc] += w0[oc] * x0 + w1[oc] * x1;
                    }
                }
                continue;
            }

            if (in_channels == 4) {
                const float* w0 = _weight[k].col(0);
                const float* w1 = _weight[k].col(1);
                const float* w2 = _weight[k].col(2);
                const float* w3 = _weight[k].col(3);
                for (int f = 0; f < num_frames; f++) {
                    const float* in_col = _inputBuffer.getCol(static_cast<int>(read_pos + f));
                    float* out_col = _output.col(f);
                    const float x0 = in_col[0];
                    const float x1 = in_col[1];
                    const float x2 = in_col[2];
                    const float x3 = in_col[3];
                    for (long oc = 0; oc < out_channels; oc++) {
                        out_col[oc] += w0[oc] * x0 + w1[oc] * x1 + w2[oc] * x2 + w3[oc] * x3;
                    }
                }
                continue;
            }

            for (int f = 0; f < num_frames; f++) {
                const float* in_col = _inputBuffer.getCol(static_cast<int>(read_pos + f));
                float* out_col = _output.col(f);

                // Cache-friendly order for column-major weights:
                // Iterate input channels, then accumulate over contiguous output rows.
                for (long ic = 0; ic < in_channels; ic++) {
                    const float x = in_col[ic];
                    const float* w_col = _weight[k].col(static_cast<int>(ic));
                    for (long oc = 0; oc < out_channels; oc++) {
                        out_col[oc] += w_col[oc] * x;
                    }
                }
            }
        }
    } else {
        // Grouped convolution
        const long in_per_group = in_channels / _numGroups;
        const long out_per_group = out_channels / _numGroups;

        for (size_t k = 0; k < _weight.size(); k++) {
            const long offset = _dilation * (static_cast<long>(k) + 1 - static_cast<long>(_weight.size()));
            const long lookback = -offset;
            const long read_pos = write_pos - lookback;

            for (int f = 0; f < num_frames; f++) {
                const float* in_col = _inputBuffer.getCol(static_cast<int>(read_pos + f));
                float* out_col = _output.col(f);

                for (int g = 0; g < _numGroups; g++) {
                    const long in_base = g * in_per_group;
                    const long out_base = g * out_per_group;

                    for (long ic = 0; ic < in_per_group; ic++) {
                        const float x = in_col[in_base + ic];
                        const float* w_col = _weight[k].col(static_cast<int>(in_base + ic));
                        for (long oc = 0; oc < out_per_group; oc++) {
                            out_col[out_base + oc] += w_col[out_base + oc] * x;
                        }
                    }
                }
            }
        }
    }

    // Advance ring buffer write pointer
    _inputBuffer.advance(num_frames);
}
long Conv1D::getInChannels() const {
    return _weight.size() > 0 ? _weight[0].cols() : 0;
}

long Conv1D::getOutChannels() const {
    return _weight.size() > 0 ? _weight[0].rows() : 0;
}

long Conv1D::getNumWeights() const {
    long num_weights = _bias.size();
    if (_weight.size() > 0) {
        const long out_channels = _weight[0].rows();
        const long in_channels = _weight[0].cols();
        // For grouped convolutions, the number of weights is reduced by numGroups
        num_weights += (out_channels * in_channels * _weight.size()) / _numGroups;
    }
    return num_weights;
}

} // namespace nam
