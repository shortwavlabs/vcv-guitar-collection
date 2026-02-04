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
        float lpf = onePoleLowpass(input, mainLpfState, 723.f);
        float hpf = onePoleHighpass(input, toneHpfState, toneHpfPrevInput, 3200.f);
        return (1.f - tone) * lpf + tone * hpf;
    }

    void reset() {
        mainLpfState = 0.f;
        toneHpfState = 0.f;
        toneHpfPrevInput = 0.f;
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
};

/**
 * DS1Tone - Big Muff-style tone blend
 */
class DS1Tone {
public:
    void setSampleRate(double sr) {
        sampleRate = sr;
    }

    void setTone(float value) {
        tone = clampf(value, 0.f, 1.f);
    }

    float process(float input) {
        float lpf = onePoleLowpass(input, lpfState, 234.f);
        float hpf = onePoleHighpass(input, hpfState, hpfPrevInput, 1063.f);
        return (1.f - tone) * lpf + tone * hpf;
    }

    void reset() {
        lpfState = 0.f;
        hpfState = 0.f;
        hpfPrevInput = 0.f;
    }

private:
    double sampleRate = 48000.0;
    float tone = 0.5f;

    float lpfState = 0.f;
    float hpfState = 0.f;
    float hpfPrevInput = 0.f;

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
};
