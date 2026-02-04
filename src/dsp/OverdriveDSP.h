#pragma once

#include "Oversampler.h"
#include "SingleKnobNoiseGate.h"
#include "SoftClipper.h"
#include "HardClipper.h"
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
        oversampler.setSampleRate(sr);
        noiseGate.setSampleRate(sr);
        inputBuffer.setSampleRate(sr);
        outputBuffer.setSampleRate(sr);
        softClipper.setSampleRate(sr);
        hardClipper.setSampleRate(sr);
        transistorBooster.setSampleRate(sr);
        tsTone.setSampleRate(sr);
        dsTone.setSampleRate(sr);
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
        hardClipper.setDrive(drive);
    }

    void setTone(float value) {
        tone = clampf(value, 0.f, 1.f);
        tsTone.setTone(tone);
        dsTone.setTone(tone);
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
        for (int i = 0; i < Oversampler::kFactor; ++i) {
            float sample = noiseGate.process(upsampled[i]);
            switch (currentModel) {
                case OverdriveModel::TS808:
                case OverdriveModel::TS9:
                    sample = inputBuffer.process(sample);
                    sample = softClipper.process(sample);
                    sample = tsTone.process(sample);
                    sample = outputBuffer.process(sample);
                    break;
                case OverdriveModel::DS1:
                    sample = inputBuffer.process(sample);
                    sample = transistorBooster.process(sample);
                    sample = hardClipper.process(sample);
                    sample = dsTone.process(sample);
                    sample = outputBuffer.process(sample);
                    break;
            }
            processed[i] = sample;
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
        hardClipper.reset();
        transistorBooster.reset();
        tsTone.reset();
        dsTone.reset();
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
    HardClipper hardClipper;
    TransistorBooster transistorBooster;
    TubeScreamerTone tsTone;
    DS1Tone dsTone;
    EmitterFollower outputBuffer;

    float drive = 0.5f;
    float tone = 0.5f;
    float outputLevel = 1.0f;

    static float clampf(float v, float lo, float hi) {
        return std::min(hi, std::max(lo, v));
    }
};
