#pragma once

#include <dsp/resampler.hpp>

/**
 * Oversampler - 4x oversampling helper using Rack's FIR up/down samplers
 */
class Oversampler {
public:
    static constexpr int kFactor = 4;
    static constexpr int kQuality = 8;  // 32 taps total

    Oversampler() = default;
    ~Oversampler() = default;

    void setSampleRate(double sr) {
        sampleRate = sr;
    }

    void setOversamplingFactor(int factor) {
        (void)factor;  // Fixed at 4x
    }

    void upsample(float input, float output[kFactor]) {
        up.process(input, output);
    }

    float downsample(float input[kFactor]) {
        return down.process(input);
    }

    void reset() {
        up.reset();
        down.reset();
    }

private:
    double sampleRate = 48000.0;
    rack::dsp::Upsampler<kFactor, kQuality> up;
    rack::dsp::Decimator<kFactor, kQuality, float> down;
};
