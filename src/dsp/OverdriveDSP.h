#pragma once

#include "Oversampler.h"
#include "SingleKnobNoiseGate.h"
#include "SoftClipper.h"
#include "ToneStack.h"
#include "TransistorStage.h"

#include <algorithm>

/**
 * OverdriveDSP - Unified DSP engine for the Overdrive module
 */
class OverdriveDSP {
public:
    OverdriveDSP() {
        setSampleRate(48000.0);
    }

    void setSampleRate(double sr) {
        sampleRate = sr;
        double stageSampleRate = sampleRate * static_cast<double>(Oversampler::kFactor);
        oversampler.setSampleRate(sr);
        noiseGate.setSampleRate(stageSampleRate);
        inputBuffer.setSampleRate(stageSampleRate);
        outputBuffer.setSampleRate(stageSampleRate);
        softClipper.setSampleRate(stageSampleRate);
        tsTone.setSampleRate(stageSampleRate);
    }

    void setModel(OverdriveModel model) {
        currentModel = model;
        softClipper.setModel(model);
    }

    OverdriveModel getModel() const {
        return currentModel;
    }

    void setDrive(float value) {
        drive = clampf(value, 0.f, 1.f);
        softClipper.setDrive(drive);
    }

    void setTone(float value) {
        tone = clampf(value, 0.f, 1.f);
        tsTone.setTone(tone);
    }

    void setLevel(float value) {
        outputLevel = clampf(value, 0.f, 1.f);
    }

    void setAttack(int position) {
        softClipper.setAttackPosition(position);
    }

    void setGate(float threshold) {
        noiseGate.setThreshold(threshold);
    }

    float process(float input) {
        float upsampled[Oversampler::kFactor];
        float processed[Oversampler::kFactor];

        oversampler.upsample(input, upsampled);
        switch (currentModel) {
            case OverdriveModel::TS808:
            case OverdriveModel::TS9:
            case OverdriveModel::SD1:
                for (int i = 0; i < Oversampler::kFactor; ++i) {
                    float sample = noiseGate.process(upsampled[i]);
                    sample = inputBuffer.process(sample);
                    sample = softClipper.process(sample);
                    sample = tsTone.process(sample);
                    sample = outputBuffer.process(sample);
                    processed[i] = sample;
                }
                break;
        }

        float output = oversampler.downsample(processed);
        return output * outputLevel;
    }

    void reset() {
        oversampler.reset();
        noiseGate.reset();
        inputBuffer.reset();
        outputBuffer.reset();
        softClipper.reset();
        tsTone.reset();
    }

    bool isGateOpen() const {
        return noiseGate.isOpen();
    }

private:
    double sampleRate = 48000.0;
    OverdriveModel currentModel = OverdriveModel::TS808;

    Oversampler oversampler;
    SingleKnobNoiseGate noiseGate;

    EmitterFollower inputBuffer;
    SoftClipper softClipper;
    TubeScreamerTone tsTone;
    EmitterFollower outputBuffer;

    float drive = 0.5f;
    float tone = 0.5f;
    float outputLevel = 1.0f;

    static float clampf(float v, float lo, float hi) {
        return std::min(hi, std::max(lo, v));
    }
};
