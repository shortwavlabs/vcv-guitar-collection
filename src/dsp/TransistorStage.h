#pragma once

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <cmath>
#include <algorithm>

/**
 * EmitterFollower - Simple unity buffer with DC blocking
 */
class EmitterFollower {
public:
    void setSampleRate(double sr) {
        sampleRate = sr;
    }

    float process(float input) {
        float hpCoeff = std::exp(-2.f * static_cast<float>(M_PI) * 20.f / static_cast<float>(sampleRate));
        float output = hpCoeff * (dcState + input - dcPrevInput);
        dcState = output;
        dcPrevInput = input;
        return output;
    }

    void reset() {
        dcState = 0.f;
        dcPrevInput = 0.f;
    }

private:
    double sampleRate = 48000.0;
    float dcState = 0.f;
    float dcPrevInput = 0.f;
};

/**
 * TransistorBooster - Asymmetric gain stage
 */
class TransistorBooster {
public:
    void setSampleRate(double sr) {
        sampleRate = sr;
    }

    float process(float input) {
        float hpCoeff = std::exp(-2.f * static_cast<float>(M_PI) * 33.f / static_cast<float>(sampleRate));
        float hpOut = hpCoeff * (inputHpState + input - inputHpPrevInput);
        inputHpState = hpOut;
        inputHpPrevInput = input;

        float boosted = hpOut * 8.0f;
        float clipped = asymmetricClip(boosted);

        float dcCoeff = std::exp(-2.f * static_cast<float>(M_PI) * 10.f / static_cast<float>(sampleRate));
        float dcOut = dcCoeff * (outputDcState + clipped - outputDcPrevInput);
        outputDcState = dcOut;
        outputDcPrevInput = clipped;
        return dcOut;
    }

    void reset() {
        inputHpState = 0.f;
        inputHpPrevInput = 0.f;
        outputDcState = 0.f;
        outputDcPrevInput = 0.f;
    }

private:
    double sampleRate = 48000.0;
    float inputHpState = 0.f;
    float inputHpPrevInput = 0.f;
    float outputDcState = 0.f;
    float outputDcPrevInput = 0.f;

    float asymmetricClip(float x) const {
        if (x >= 0.f) {
            return std::tanh(x);
        }
        return -0.7f * std::tanh(-x * 1.4f);
    }
};
