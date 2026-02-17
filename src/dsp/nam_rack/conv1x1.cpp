#include "conv1x1.h"
#include <cassert>

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
        // Standard matrix multiplication: output = weight * input
        // weight is (out_channels x in_channels)
        // input is (in_channels x num_frames)
        // output is (out_channels x num_frames)

        for (long oc = 0; oc < out_channels; oc++) {
            for (int f = 0; f < num_frames; f++) {
                float sum = 0.0f;
                for (long ic = 0; ic < in_channels; ic++) {
                    sum += _weight(oc, ic) * input(ic, f);
                }
                _output(oc, f) = sum;
            }
        }
    } else {
        // Grouped convolution: process each group separately
        for (int g = 0; g < _numGroups; g++) {
            for (long oc = 0; oc < out_per_group; oc++) {
                for (int f = 0; f < num_frames; f++) {
                    float sum = 0.0f;
                    for (long ic = 0; ic < in_per_group; ic++) {
                        sum += _weight(g * out_per_group + oc, g * in_per_group + ic) *
                               input(g * in_per_group + ic, f);
                    }
                    _output(g * out_per_group + oc, f) = sum;
                }
            }
        }
    }

    // Add bias if present
    if (_doBias) {
        for (int f = 0; f < num_frames; f++) {
            for (long c = 0; c < _bias.size(); c++) {
                _output(c, f) += _bias(c);
            }
        }
    }
}

} // namespace nam
