#include "conv1x1.h"
#include <cassert>
#include <cstring>

namespace nam {

Conv1x1::Conv1x1()
    : _numGroups(1)
    , _doBias(false) {
}

Conv1x1::Conv1x1(int in_channels, int out_channels, bool bias, int groups)
    : _numGroups(groups)
    , _doBias(bias) {

    if (in_channels % groups != 0) {
        throw std::runtime_error("in_channels must be divisible by groups");
    }
    if (out_channels % groups != 0) {
        throw std::runtime_error("out_channels must be divisible by groups");
    }

    _weight.resize(out_channels, in_channels);

    if (_doBias) {
        _bias.resize(out_channels);
    }
}

void Conv1x1::setWeights(std::vector<float>::iterator& weights) {
    const long in_channels = getInChannels();
    const long out_channels = getOutChannels();
    const long in_per_group = in_channels / _numGroups;
    const long out_per_group = out_channels / _numGroups;

    // Weight layout for grouped convolution matches Conv1D:
    // For each group, weights are (out_per_group x in_per_group)
    for (int g = 0; g < _numGroups; g++) {
        for (long oc = 0; oc < out_per_group; oc++) {
            for (long ic = 0; ic < in_per_group; ic++) {
                _weight(g * out_per_group + oc, g * in_per_group + ic) = *(weights++);
            }
        }
    }

    // Set bias
    if (_doBias) {
        for (long i = 0; i < _bias.size(); i++) {
            _bias(i) = *(weights++);
        }
    }
}

void Conv1x1::setMaxBufferSize(int maxBufferSize) {
    _output.resize(getOutChannels(), maxBufferSize);
    _output.setZero();
}

void Conv1x1::process(const Matrix& input, int num_frames) {
    assert(num_frames <= _output.cols());

    const long in_channels = getInChannels();
    const long out_channels = getOutChannels();
    const long in_per_group = in_channels / _numGroups;
    const long out_per_group = out_channels / _numGroups;

    if (_numGroups == 1) {
        if (in_channels == 1) {
            const float* w0 = _weight.col(0);
            for (int f = 0; f < num_frames; f++) {
                const float* in_col = input.col(f);
                float* out_col = _output.col(f);

                if (_doBias) {
                    for (long oc = 0; oc < out_channels; oc++) {
                        out_col[oc] = _bias(oc);
                    }
                } else {
                    std::memset(out_col, 0, static_cast<size_t>(out_channels) * sizeof(float));
                }

                const float x0 = in_col[0];
                for (long oc = 0; oc < out_channels; oc++) {
                    out_col[oc] += w0[oc] * x0;
                }
            }
            return;
        }

        if (in_channels == 2) {
            const float* w0 = _weight.col(0);
            const float* w1 = _weight.col(1);
            for (int f = 0; f < num_frames; f++) {
                const float* in_col = input.col(f);
                float* out_col = _output.col(f);

                if (_doBias) {
                    for (long oc = 0; oc < out_channels; oc++) {
                        out_col[oc] = _bias(oc);
                    }
                } else {
                    std::memset(out_col, 0, static_cast<size_t>(out_channels) * sizeof(float));
                }

                const float x0 = in_col[0];
                const float x1 = in_col[1];
                for (long oc = 0; oc < out_channels; oc++) {
                    out_col[oc] += w0[oc] * x0 + w1[oc] * x1;
                }
            }
            return;
        }

        if (in_channels == 4) {
            const float* w0 = _weight.col(0);
            const float* w1 = _weight.col(1);
            const float* w2 = _weight.col(2);
            const float* w3 = _weight.col(3);
            for (int f = 0; f < num_frames; f++) {
                const float* in_col = input.col(f);
                float* out_col = _output.col(f);

                if (_doBias) {
                    for (long oc = 0; oc < out_channels; oc++) {
                        out_col[oc] = _bias(oc);
                    }
                } else {
                    std::memset(out_col, 0, static_cast<size_t>(out_channels) * sizeof(float));
                }

                const float x0 = in_col[0];
                const float x1 = in_col[1];
                const float x2 = in_col[2];
                const float x3 = in_col[3];
                for (long oc = 0; oc < out_channels; oc++) {
                    out_col[oc] += w0[oc] * x0 + w1[oc] * x1 + w2[oc] * x2 + w3[oc] * x3;
                }
            }
            return;
        }

        for (int f = 0; f < num_frames; f++) {
            const float* in_col = input.col(f);
            float* out_col = _output.col(f);

            // Initialize output column (bias or zero)
            if (_doBias) {
                for (long oc = 0; oc < out_channels; oc++) {
                    out_col[oc] = _bias(oc);
                }
            } else {
                std::memset(out_col, 0, static_cast<size_t>(out_channels) * sizeof(float));
            }

            // Column-major friendly accumulation
            for (long ic = 0; ic < in_channels; ic++) {
                const float x = in_col[ic];
                const float* w_col = _weight.col(static_cast<int>(ic));
                for (long oc = 0; oc < out_channels; oc++) {
                    out_col[oc] += w_col[oc] * x;
                }
            }
        }
    } else {
        for (int f = 0; f < num_frames; f++) {
            const float* in_col = input.col(f);
            float* out_col = _output.col(f);

            // Initialize output column (bias or zero)
            if (_doBias) {
                for (long oc = 0; oc < out_channels; oc++) {
                    out_col[oc] = _bias(oc);
                }
            } else {
                std::memset(out_col, 0, static_cast<size_t>(out_channels) * sizeof(float));
            }

            for (int g = 0; g < _numGroups; g++) {
                const long in_base = g * in_per_group;
                const long out_base = g * out_per_group;

                for (long ic = 0; ic < in_per_group; ic++) {
                    const float x = in_col[in_base + ic];
                    const float* w_col = _weight.col(static_cast<int>(in_base + ic));
                    for (long oc = 0; oc < out_per_group; oc++) {
                        out_col[out_base + oc] += w_col[out_base + oc] * x;
                    }
                }
            }
        }
    }
}

} // namespace nam
