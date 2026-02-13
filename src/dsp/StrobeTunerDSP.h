#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

/**
 * StrobeTunerDSP
 *
 * Lightweight monophonic pitch detector optimized for tuner workflows.
 * Uses normalized autocorrelation on a rolling analysis window, with
 * confidence gating and temporal smoothing to reduce octave chatter.
 */
class StrobeTunerDSP {
public:
    struct PitchResult {
        bool valid = false;
        float frequencyHz = 0.f;
        float smoothedFrequencyHz = 0.f;
        float confidence = 0.f;
        float rms = 0.f;
    };

    static constexpr float DEFAULT_MIN_FREQUENCY = 65.f;
    static constexpr float DEFAULT_MAX_FREQUENCY = 1200.f;

    StrobeTunerDSP() {
        setSampleRate(48000.0);
        reset();
    }

    void setSampleRate(double sr) {
        sampleRate = std::max(sr, 1000.0);
        configureAnalysisSizes();
        updateLagRange();
        reset();
    }

    double getSampleRate() const {
        return sampleRate;
    }

    void setFrequencyRange(float minHz, float maxHz) {
        minFrequencyHz = std::clamp(minHz, 20.f, 2000.f);
        maxFrequencyHz = std::clamp(maxHz, minFrequencyHz + 10.f, 5000.f);
        updateLagRange();
    }

    void setConfidenceThreshold(float threshold) {
        confidenceThreshold = std::clamp(threshold, 0.1f, 0.99f);
    }

    float getConfidenceThreshold() const {
        return confidenceThreshold;
    }

    void setMinRms(float value) {
        minRms = std::clamp(value, 1.0e-5f, 1.0f);
    }

    float getMinRms() const {
        return minRms;
    }

    void setSmoothing(float coefficient) {
        smoothingCoefficient = std::clamp(coefficient, 0.f, 0.995f);
    }

    float getSmoothing() const {
        return smoothingCoefficient;
    }

    int getAnalysisWindowSize() const {
        return analysisWindowSize;
    }

    int getHopSize() const {
        return hopSize;
    }

    const PitchResult& getLastResult() const {
        return lastResult;
    }

    void reset() {
        std::fill(ringBuffer.begin(), ringBuffer.end(), 0.f);
        std::fill(windowBuffer.begin(), windowBuffer.end(), 0.f);
        std::fill(correlationScores.begin(), correlationScores.end(), 0.f);

        writeIndex = 0;
        samplesInBuffer = 0;
        samplesUntilAnalysis = hopSize;

        previousInput = 0.f;
        previousOutput = 0.f;

        hasSmoothedFrequency = false;
        smoothedFrequencyHz = 0.f;
        lastResult = PitchResult{};
    }

    /**
     * Process one sample. Returns true when a new analysis result is produced.
     */
    bool processSample(float input, PitchResult& resultOut) {
        if (ringBuffer.empty() || analysisWindowSize <= 0) {
            resultOut = PitchResult{};
            return false;
        }

        const float conditioned = dcBlock(input);
        ringBuffer[writeIndex] = conditioned;
        writeIndex = (writeIndex + 1) % static_cast<int>(ringBuffer.size());
        samplesInBuffer = std::min(samplesInBuffer + 1, static_cast<int>(ringBuffer.size()));

        samplesUntilAnalysis--;
        if (samplesUntilAnalysis > 0) {
            return false;
        }
        samplesUntilAnalysis = hopSize;

        if (samplesInBuffer < analysisWindowSize) {
            return false;
        }

        copyLatestWindow();
        analyzeCurrentWindow();
        resultOut = lastResult;
        return true;
    }

    static float midiToFrequency(int midiNote, float a4Hz = 440.f) {
        return a4Hz * std::pow(2.f, (static_cast<float>(midiNote) - 69.f) / 12.f);
    }

    static float frequencyToMidi(float frequencyHz, float a4Hz = 440.f) {
        if (frequencyHz <= 0.f || a4Hz <= 0.f) {
            return 0.f;
        }
        return 69.f + 12.f * std::log2(frequencyHz / a4Hz);
    }

    static int nearestMidi(float frequencyHz, float a4Hz = 440.f) {
        return static_cast<int>(std::round(frequencyToMidi(frequencyHz, a4Hz)));
    }

    static float centsDifference(float frequencyHz, float referenceHz) {
        if (frequencyHz <= 0.f || referenceHz <= 0.f) {
            return 0.f;
        }
        return 1200.f * std::log2(frequencyHz / referenceHz);
    }

    static float midiToRackPitchVoltage(int midiNote) {
        // VCV convention: 0V == C4 (MIDI 60), 1V per octave.
        return (static_cast<float>(midiNote) - 60.f) / 12.f;
    }

private:
    static constexpr float DC_BLOCK_COEFFICIENT = 0.995f;
    static constexpr int MIN_ANALYSIS_WINDOW = 1024;
    static constexpr int MAX_ANALYSIS_WINDOW = 4096;
    static constexpr int MIN_RING_BUFFER_SIZE = 8192;

    double sampleRate = 48000.0;
    float minFrequencyHz = DEFAULT_MIN_FREQUENCY;
    float maxFrequencyHz = DEFAULT_MAX_FREQUENCY;
    float confidenceThreshold = 0.70f;
    float minRms = 0.0020f;
    float smoothingCoefficient = 0.88f;

    int analysisWindowSize = 1440;
    int hopSize = 240;
    int minLag = 40;
    int maxLag = 736;

    std::vector<float> ringBuffer;
    std::vector<float> windowBuffer;
    std::vector<float> correlationScores;

    int writeIndex = 0;
    int samplesInBuffer = 0;
    int samplesUntilAnalysis = 240;

    float previousInput = 0.f;
    float previousOutput = 0.f;

    bool hasSmoothedFrequency = false;
    float smoothedFrequencyHz = 0.f;
    PitchResult lastResult;

    void configureAnalysisSizes() {
        analysisWindowSize = static_cast<int>(std::round(sampleRate * 0.03));
        analysisWindowSize = std::clamp(analysisWindowSize, MIN_ANALYSIS_WINDOW, MAX_ANALYSIS_WINDOW);
        hopSize = std::max(64, analysisWindowSize / 6);

        const int requiredRingSize = std::max(MIN_RING_BUFFER_SIZE, analysisWindowSize * 2);
        ringBuffer.assign(requiredRingSize, 0.f);
        windowBuffer.assign(analysisWindowSize, 0.f);
        correlationScores.assign(analysisWindowSize, 0.f);
    }

    void updateLagRange() {
        minLag = std::max(1, static_cast<int>(std::floor(sampleRate / maxFrequencyHz)));
        maxLag = std::max(minLag + 2, static_cast<int>(std::ceil(sampleRate / minFrequencyHz)));
        maxLag = std::min(maxLag, std::max(3, analysisWindowSize - 2));
    }

    float dcBlock(float input) {
        const float output = input - previousInput + DC_BLOCK_COEFFICIENT * previousOutput;
        previousInput = input;
        previousOutput = output;
        return output;
    }

    void copyLatestWindow() {
        const int ringSize = static_cast<int>(ringBuffer.size());
        int readIndex = writeIndex - analysisWindowSize;
        if (readIndex < 0) {
            readIndex += ringSize;
        }

        double mean = 0.0;
        for (int i = 0; i < analysisWindowSize; ++i) {
            const float sample = ringBuffer[(readIndex + i) % ringSize];
            windowBuffer[i] = sample;
            mean += sample;
        }
        mean /= static_cast<double>(analysisWindowSize);

        // Remove residual DC from the copied window to improve correlation quality.
        for (int i = 0; i < analysisWindowSize; ++i) {
            windowBuffer[i] -= static_cast<float>(mean);
        }
    }

    void analyzeCurrentWindow() {
        PitchResult result;

        double powerSum = 0.0;
        for (float x : windowBuffer) {
            powerSum += static_cast<double>(x) * static_cast<double>(x);
        }
        result.rms = static_cast<float>(std::sqrt(powerSum / static_cast<double>(analysisWindowSize)));

        if (result.rms < minRms || minLag >= maxLag) {
            result.valid = false;
            result.smoothedFrequencyHz = hasSmoothedFrequency ? smoothedFrequencyHz : 0.f;
            result.confidence = 0.f;
            result.frequencyHz = 0.f;
            lastResult = result;
            return;
        }

        float bestScore = -1.f;
        int bestLag = minLag;

        for (int lag = minLag; lag <= maxLag; ++lag) {
            const int sampleCount = analysisWindowSize - lag;
            double correlation = 0.0;
            double energyA = 0.0;
            double energyB = 0.0;

            for (int i = 0; i < sampleCount; ++i) {
                const float a = windowBuffer[i];
                const float b = windowBuffer[i + lag];
                correlation += static_cast<double>(a) * static_cast<double>(b);
                energyA += static_cast<double>(a) * static_cast<double>(a);
                energyB += static_cast<double>(b) * static_cast<double>(b);
            }

            float score = 0.f;
            if (energyA > 1.0e-12 && energyB > 1.0e-12) {
                score = static_cast<float>(correlation / std::sqrt(energyA * energyB));
            }
            correlationScores[lag] = score;

            if (score > bestScore) {
                bestScore = score;
                bestLag = lag;
            }
        }

        result.confidence = std::clamp(bestScore, 0.f, 1.f);
        if (bestScore < confidenceThreshold) {
            result.valid = false;
            result.smoothedFrequencyHz = hasSmoothedFrequency ? smoothedFrequencyHz : 0.f;
            result.frequencyHz = 0.f;
            lastResult = result;
            return;
        }

        float refinedLag = static_cast<float>(bestLag);
        if (bestLag > minLag && bestLag < maxLag) {
            const float yPrev = correlationScores[bestLag - 1];
            const float yCurr = correlationScores[bestLag];
            const float yNext = correlationScores[bestLag + 1];
            const float denominator = yPrev - 2.f * yCurr + yNext;
            if (std::fabs(denominator) > 1.0e-6f) {
                float delta = 0.5f * (yPrev - yNext) / denominator;
                delta = std::clamp(delta, -1.f, 1.f);
                refinedLag += delta;
            }
        }

        refinedLag = std::clamp(refinedLag, static_cast<float>(minLag), static_cast<float>(maxLag));
        float frequencyHz = static_cast<float>(sampleRate / refinedLag);
        frequencyHz = resolveOctaveContinuity(frequencyHz);

        result.valid = true;
        result.frequencyHz = frequencyHz;
        if (!hasSmoothedFrequency) {
            smoothedFrequencyHz = frequencyHz;
            hasSmoothedFrequency = true;
        } else {
            smoothedFrequencyHz = smoothingCoefficient * smoothedFrequencyHz
                                + (1.f - smoothingCoefficient) * frequencyHz;
        }
        result.smoothedFrequencyHz = smoothedFrequencyHz;

        lastResult = result;
    }

    float resolveOctaveContinuity(float frequencyHz) const {
        if (!hasSmoothedFrequency || smoothedFrequencyHz <= 0.f) {
            return frequencyHz;
        }

        float candidates[3] = {frequencyHz * 0.5f, frequencyHz, frequencyHz * 2.f};
        float bestCandidate = frequencyHz;
        float bestDistance = std::numeric_limits<float>::max();

        for (float candidate : candidates) {
            if (candidate < minFrequencyHz || candidate > maxFrequencyHz) {
                continue;
            }
            const float ratio = candidate / smoothedFrequencyHz;
            const float distance = std::fabs(std::log2(std::max(ratio, 1.0e-6f)));
            if (distance < bestDistance) {
                bestDistance = distance;
                bestCandidate = candidate;
            }
        }

        return bestCandidate;
    }
};
