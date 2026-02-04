#pragma once

#include "Nam.h"

/**
 * SingleKnobNoiseGate - Simple threshold-only wrapper around NoiseGate
 */
class SingleKnobNoiseGate {
public:
    static constexpr float ATTACK_MS = 1.0f;
    static constexpr float RELEASE_MS = 100.0f;
    static constexpr float HOLD_MS = 50.0f;
    static constexpr float HYSTERESIS_DB = 6.0f;

    SingleKnobNoiseGate() {
        gate.hysteresis = HYSTERESIS_DB;
        gate.setParameters(-60.f, ATTACK_MS, RELEASE_MS, HOLD_MS);
    }

    void setSampleRate(double sr) {
        gate.setSampleRate(sr);
    }

    void setThreshold(float knobValue) {
        if (knobValue < 0.f) knobValue = 0.f;
        if (knobValue > 1.f) knobValue = 1.f;
        float thresholdDb = -20.0f - knobValue * 60.0f;  // -20 to -80 dB
        gate.hysteresis = HYSTERESIS_DB;
        gate.setParameters(thresholdDb, ATTACK_MS, RELEASE_MS, HOLD_MS);
    }

    float process(float input) {
        return gate.process(input);
    }

    bool isOpen() const {
        return gate.isOpen;
    }

    float getThresholdDb() const {
        return gate.threshold;
    }

    void reset() {
        gate.reset();
    }

private:
    NoiseGate gate;
};
