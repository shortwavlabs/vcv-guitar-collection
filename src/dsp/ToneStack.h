#pragma once

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <cmath>
#include <algorithm>

/**
 * TubeScreamerTone - Active tone control
 */
class TubeScreamerTone {
public:
    void setSampleRate(double sr) {
        sampleRate = sr;
    }

    void setTone(float value) {
        tone = clampf(value, 0.f, 1.f);
    }

    float process(float input) {
        float body = onePoleLowpass(input, mainLpfState, preToneLowpassFc());
        float treble = onePoleHighpass(input, toneHpfState, toneHpfPrevInput, toneHighpassFc());
        return body + tone * treble;
    }

    void reset() {
        mainLpfState = 0.f;
        toneHpfState = 0.f;
        toneHpfPrevInput = 0.f;
    }

    static float getPreToneLowpassFrequency() {
        return preToneLowpassFc();
    }

    static float getToneHighpassFrequency() {
        return toneHighpassFc();
    }

private:
    double sampleRate = 48000.0;
    float tone = 0.5f;

    float mainLpfState = 0.f;
    float toneHpfState = 0.f;
    float toneHpfPrevInput = 0.f;

    float onePoleLowpass(float x, float& state, float fc) {
        float coeff = std::exp(-2.f * static_cast<float>(M_PI) * fc / static_cast<float>(sampleRate));
        state = (1.f - coeff) * x + coeff * state;
        return state;
    }

    float onePoleHighpass(float x, float& state, float& prevInput, float fc) {
        float coeff = std::exp(-2.f * static_cast<float>(M_PI) * fc / static_cast<float>(sampleRate));
        float y = coeff * (state + x - prevInput);
        state = y;
        prevInput = x;
        return y;
    }

    static float clampf(float v, float lo, float hi) {
        return std::min(hi, std::max(lo, v));
    }

    static float rcFc(float resistance, float capacitance) {
        return 1.f / (2.f * static_cast<float>(M_PI) * resistance * capacitance);
    }

    static float preToneLowpassFc() {
        return rcFc(PRE_TONE_R, PRE_TONE_C);
    }

    static float toneHighpassFc() {
        return rcFc(TONE_GROUND_R, TONE_C);
    }

    static constexpr float PRE_TONE_R = 1000.0f;
    static constexpr float PRE_TONE_C = 0.22e-6f;
    static constexpr float TONE_GROUND_R = 220.0f;
    static constexpr float TONE_C = 0.22e-6f;
};
