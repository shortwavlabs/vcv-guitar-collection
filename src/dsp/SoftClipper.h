#pragma once

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <cmath>
#include <algorithm>

enum class OverdriveModel {
    TS808 = 0,
    TS9 = 1,
    DS1 = 2
};

/**
 * SoftClipper - Tubescreamer-style soft clipping stage
 */
class SoftClipper {
public:
    SoftClipper() {
        updateModelOutput();
    }

    void setSampleRate(double sr) {
        sampleRate = sr;
    }

    void setDrive(float value) {
        drive = clampf(value, 0.f, 1.f);
    }

    void setAttackPosition(int pos) {
        if (pos < 0) pos = 0;
        if (pos > 5) pos = 5;
        attackPosition = pos;
    }

    void setModel(OverdriveModel m) {
        model = m;
        updateModelOutput();
    }

    float process(float input) {
        float attackCap = ATTACK_CAPS[attackPosition];
        float hpFc = 1.f / (2.f * static_cast<float>(M_PI) * R4 * attackCap);
        float hpCoeff = std::exp(-2.f * static_cast<float>(M_PI) * hpFc / static_cast<float>(sampleRate));
        float hpOut = hpCoeff * (hpState + input - hpPrevInput);
        hpState = hpOut;
        hpPrevInput = input;

        float gain = 1.f + (R6 + drive * 500000.f) / R4;
        float boosted = hpOut * gain;

        float clipped = softClipDiode(boosted);

        float lpFc = 7000.f;
        float lpCoeff = std::exp(-2.f * static_cast<float>(M_PI) * lpFc / static_cast<float>(sampleRate));
        lpState = (1.f - lpCoeff) * clipped + lpCoeff * lpState;

        return lpState * outputGain;
    }

    void reset() {
        hpState = 0.f;
        hpPrevInput = 0.f;
        lpState = 0.f;
    }

    float getOutputGain() const {
        return outputGain;
    }

private:
    double sampleRate = 48000.0;
    float drive = 0.5f;
    int attackPosition = 3;
    OverdriveModel model = OverdriveModel::TS808;

    // Tubescreamer component values
    static constexpr float R4 = 4700.0f;
    static constexpr float R6 = 51000.0f;
    static constexpr float VF_DIODE = 1.0f;

    static constexpr float ATTACK_CAPS[6] = {
        470e-9f, 220e-9f, 100e-9f, 47e-9f, 22e-9f, 10e-9f
    };

    // Filter states
    float hpState = 0.f;
    float hpPrevInput = 0.f;
    float lpState = 0.f;

    // Output buffer resistors
    float outputRB = 100.0f;
    float outputRC = 10000.0f;
    float outputGain = 0.99f;

    static float clampf(float v, float lo, float hi) {
        return std::min(hi, std::max(lo, v));
    }

    void updateModelOutput() {
        if (model == OverdriveModel::TS9) {
            outputRB = 470.0f;
            outputRC = 100000.0f;
        } else {
            outputRB = 100.0f;
            outputRC = 10000.0f;
        }
        outputGain = outputRC / (outputRC + outputRB);
    }

    float softClipDiode(float x) const {
        return VF_DIODE * std::tanh(x / VF_DIODE);
    }
};
