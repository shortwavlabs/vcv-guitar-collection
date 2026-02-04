#pragma once

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <cmath>
#include <algorithm>

/**
 * HardClipper - DS-1 style hard clipping stage
 */
class HardClipper {
public:
    void setSampleRate(double sr) {
        sampleRate = sr;
    }

    void setDrive(float value) {
        drive = clampf(value, 0.f, 1.f);
    }

    float process(float input) {
        float gain = 1.f + drive * 100000.f / R13;

        float hpFc = 1.f / (2.f * static_cast<float>(M_PI) * R13 * C8);
        float hpCoeff = std::exp(-2.f * static_cast<float>(M_PI) * hpFc / static_cast<float>(sampleRate));
        float hpOut = hpCoeff * (hpState + input - hpPrevInput);
        hpState = hpOut;
        hpPrevInput = input;

        float boosted = hpOut * gain;
        float clipped = clampf(boosted, -VF_DIODE, VF_DIODE);

        float lpFc = 1.f / (2.f * static_cast<float>(M_PI) * R14 * C10);
        float lpCoeff = std::exp(-2.f * static_cast<float>(M_PI) * lpFc / static_cast<float>(sampleRate));
        lpState = (1.f - lpCoeff) * clipped + lpCoeff * lpState;
        return lpState;
    }

    void reset() {
        hpState = 0.f;
        hpPrevInput = 0.f;
        lpState = 0.f;
    }

private:
    double sampleRate = 48000.0;
    float drive = 0.5f;

    static constexpr float R13 = 4700.0f;
    static constexpr float C8 = 0.47e-6f;
    static constexpr float C10 = 0.01e-6f;
    static constexpr float R14 = 2200.0f;
    static constexpr float VF_DIODE = 0.7f;

    float hpState = 0.f;
    float hpPrevInput = 0.f;
    float lpState = 0.f;

    static float clampf(float v, float lo, float hi) {
        return std::min(hi, std::max(lo, v));
    }
};
