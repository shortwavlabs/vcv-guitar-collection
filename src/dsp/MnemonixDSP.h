#pragma once

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class MnemonixDSP {
public:
    enum ArtifactProfile {
        ARTIFACT_CLEAN = 0,
        ARTIFACT_AUTHENTIC = 1,
        ARTIFACT_WORN = 2
    };

    struct Params {
        float level = 0.55f;
        float blend = 0.5f;
        float feedback = 0.25f;
        float delay = 0.45f;
        float depth = 0.2f;
        bool vibrato = false;
        bool engaged = true;
        int artifactProfile = ARTIFACT_AUTHENTIC;
    };

    struct Result {
        float output = 0.f;
        float direct = 0.f;
        float wet = 0.f;
        float overload = 0.f;
        float clockHz = 0.f;
        float delaySeconds = 0.f;
    };

    MnemonixDSP() {
        delayLine.assign(kDelayBufferSize, 0.f);
        setSampleRate(48000.f);
        reset();
    }

    void setSampleRate(float rate) {
        if (!std::isfinite(rate) || rate < 1000.f) {
            rate = 48000.f;
        }
        sampleRate = rate;
        sampleTime = 1.f / sampleRate;

        levelSmoothCoeff = smoothingCoeff(8.f);
        controlSmoothCoeff = smoothingCoeff(18.f);
        delaySmoothCoeff = smoothingCoeff(24.f);

        inputHp.setSampleRate(sampleRate);
        inputLp.setSampleRate(sampleRate);
        preBbdHp.setSampleRate(sampleRate);
        preBbdLp.setSampleRate(sampleRate);
        postBbdHp.setSampleRate(sampleRate);
        postBbdLp1.setSampleRate(sampleRate);
        postBbdLp2.setSampleRate(sampleRate);
        outputLp.setSampleRate(sampleRate);
        compander.setSampleRate(sampleRate);

        inputHp.setCutoff(24.f);
        inputLp.setCutoff(14000.f);
        preBbdHp.setCutoff(55.f);
        postBbdHp.setCutoff(75.f);
        outputLp.setCutoff(10500.f);
    }

    void reset() {
        std::fill(delayLine.begin(), delayLine.end(), 0.f);
        writeIndex = 0;
        feedbackState = 0.f;
        heldBbdSample = 0.f;
        clockPhase = 0.f;
        lfoPhase = 0.13f;
        rng = 0x4567abcdU;
        overloadEnv = 0.f;
        smoothedLevel = 0.55f;
        smoothedBlend = 0.5f;
        smoothedFeedback = 0.25f;
        smoothedDelay = 0.45f;
        smoothedDepth = 0.2f;

        inputHp.reset();
        inputLp.reset();
        preBbdHp.reset();
        preBbdLp.reset();
        postBbdHp.reset();
        postBbdLp1.reset();
        postBbdLp2.reset();
        outputLp.reset();
        compander.reset();
    }

    Result process(float input, const Params& rawParams) {
        Params p = sanitizeParams(rawParams);
        input = sanitize(input);

        smoothedLevel += levelSmoothCoeff * (p.level - smoothedLevel);
        smoothedBlend += controlSmoothCoeff * (p.blend - smoothedBlend);
        smoothedFeedback += controlSmoothCoeff * (p.feedback - smoothedFeedback);
        smoothedDelay += delaySmoothCoeff * (p.delay - smoothedDelay);
        smoothedDepth += controlSmoothCoeff * (p.depth - smoothedDepth);

        const float artifact = artifactAmount(p.artifactProfile);
        const float delayControl = clampf(smoothedDelay, 0.f, 1.f);
        const float basePeriodUs = clockPeriodUsForDelay(delayControl);

        const float lfoRate = p.vibrato ? 1.65f : 0.35f;
        lfoPhase += lfoRate * sampleTime;
        if (lfoPhase >= 1.f) {
            lfoPhase -= std::floor(lfoPhase);
        }

        const float lfoSin = std::sin(2.f * static_cast<float>(M_PI) * lfoPhase);
        const float lfoAsym = lfoSin + 0.08f * std::sin(4.f * static_cast<float>(M_PI) * lfoPhase + 0.6f);
        const float maxMod = p.vibrato ? 0.18f : 0.085f;
        float periodUs = basePeriodUs * (1.f + lfoAsym * maxMod * smoothedDepth);
        periodUs = clampf(periodUs, kMinClockPeriodUs * 0.65f, kMaxClockPeriodUs * 1.35f);

        const float clockHz = 1000000.f / periodUs;
        const float delaySeconds = bbdDelaySecondsForClockPeriodUs(periodUs);
        const float delaySamples = clampf(delaySeconds * sampleRate, 1.f, static_cast<float>(kDelayBufferSize - 4));

        const float inputGain = 0.06f + 3.15f * std::pow(clampf(smoothedLevel, 0.f, 1.f), 2.05f);
        float preamp = input * inputGain + feedbackState * feedbackGain(smoothedFeedback);
        preamp = inputHp.process(preamp);
        preamp = inputLp.process(preamp);
        preamp = opAmpSaturate(preamp, 1.65f, 0.035f);

        updateOverload(preamp);

        float compressed = compander.compress(preamp);

        const float trackedCutoff = clampf(clockHz * 0.36f, 1400.f, 11800.f);
        preBbdLp.setCutoff(clampf(trackedCutoff * 1.12f, 1600.f, 13200.f));
        postBbdLp1.setCutoff(trackedCutoff);
        postBbdLp2.setCutoff(clampf(trackedCutoff * 0.72f, 900.f, 9800.f));

        float bbdDrive = preBbdHp.process(compressed);
        bbdDrive = preBbdLp.process(bbdDrive);
        bbdDrive = opAmpSaturate(bbdDrive, 1.25f, -0.015f);

        float delayed = readDelay(delaySamples);
        writeDelay(bbdDrive);

        clockPhase += clockHz * sampleTime;
        if (clockPhase >= 1.f) {
            clockPhase -= std::floor(clockPhase);
            heldBbdSample = delayed;
        }

        const float delayNorm = clampf((periodUs - kMinClockPeriodUs) / (kMaxClockPeriodUs - kMinClockPeriodUs), 0.f, 1.f);
        float bbdOut = heldBbdSample;

        const float hiss = randomSigned() * (0.00008f + 0.0024f * delayNorm * delayNorm) * artifact;
        const float clockBleed = std::sin(2.f * static_cast<float>(M_PI) * clockPhase) *
            (0.00015f + 0.00125f * delayNorm) * artifact;
        bbdOut += hiss + clockBleed;

        float post = postBbdHp.process(bbdOut);
        post = postBbdLp1.process(post);
        post = postBbdLp2.process(post);
        post = compander.expand(post);
        post = opAmpSaturate(post, 1.22f, 0.012f);
        post = outputLp.process(post);

        const float feedbackDamping = 1.f - 0.12f * artifact * delayNorm;
        feedbackState = softLimit(post * feedbackDamping, 1.8f);

        const float wet = post;
        const float mixed = input * (1.f - smoothedBlend) + wet * smoothedBlend;
        float output = opAmpSaturate(mixed, 1.05f, 0.f);
        if (!p.engaged) {
            output = input;
        }

        Result result;
        result.direct = input;
        result.wet = wet;
        result.output = sanitize(output);
        result.overload = clampf(overloadEnv * 1.6f, 0.f, 1.f);
        result.clockHz = clockHz;
        result.delaySeconds = delaySeconds;
        return result;
    }

    float getOverload() const {
        return clampf(overloadEnv * 1.6f, 0.f, 1.f);
    }

    static float clockPeriodUsForDelay(float delayNorm) {
        delayNorm = clampf(delayNorm, 0.f, 1.f);
        return kMinClockPeriodUs * std::pow(kMaxClockPeriodUs / kMinClockPeriodUs, delayNorm);
    }

    static float bbdDelaySecondsForClockPeriodUs(float periodUs) {
        return 4096.f * periodUs * 0.000001f;
    }

    static float delaySecondsForDelay(float delayNorm) {
        return bbdDelaySecondsForClockPeriodUs(clockPeriodUsForDelay(delayNorm));
    }

private:
    static constexpr float kMinClockPeriodUs = 8.f;
    static constexpr float kMaxClockPeriodUs = 100.f;
    static const int kDelayBufferSize = 262144;
    static const int kDelayBufferMask = kDelayBufferSize - 1;

    class OnePoleLowpass {
    public:
        void setSampleRate(float sr) {
            sampleRate = sr;
            setCutoff(cutoff);
        }

        void setCutoff(float hz) {
            cutoff = clampf(hz, 5.f, sampleRate * 0.45f);
            coeff = 1.f - std::exp(-2.f * static_cast<float>(M_PI) * cutoff / sampleRate);
        }

        float process(float x) {
            state += coeff * (x - state);
            return state;
        }

        void reset() {
            state = 0.f;
        }

    private:
        float sampleRate = 48000.f;
        float cutoff = 1000.f;
        float coeff = 0.1f;
        float state = 0.f;
    };

    class OnePoleHighpass {
    public:
        void setSampleRate(float sr) {
            lowpass.setSampleRate(sr);
        }

        void setCutoff(float hz) {
            lowpass.setCutoff(hz);
        }

        float process(float x) {
            return x - lowpass.process(x);
        }

        void reset() {
            lowpass.reset();
        }

    private:
        OnePoleLowpass lowpass;
    };

    class Ne570Approx {
    public:
        void setSampleRate(float sr) {
            sampleRate = sr;
            attackCoeff = coeffForMs(2.5f);
            releaseCoeff = coeffForMs(85.f);
        }

        void reset() {
            compEnv = 0.f;
            expEnv = 0.f;
        }

        float compress(float x) {
            compEnv = envelope(compEnv, std::fabs(x));
            const float gain = clampf(0.72f / std::sqrt(compEnv + 0.018f), 0.34f, 2.6f);
            return x * gain * 0.72f;
        }

        float expand(float x) {
            expEnv = envelope(expEnv, std::fabs(x));
            const float gain = clampf(std::sqrt(expEnv + 0.018f) * 1.42f, 0.22f, 1.85f);
            return x * gain;
        }

    private:
        float sampleRate = 48000.f;
        float attackCoeff = 0.01f;
        float releaseCoeff = 0.0002f;
        float compEnv = 0.f;
        float expEnv = 0.f;

        float coeffForMs(float ms) const {
            return 1.f - std::exp(-1.f / (sampleRate * ms * 0.001f));
        }

        float envelope(float env, float x) const {
            const float coeff = x > env ? attackCoeff : releaseCoeff;
            return env + coeff * (x - env);
        }
    };

    std::vector<float> delayLine;
    int writeIndex = 0;

    float sampleRate = 48000.f;
    float sampleTime = 1.f / 48000.f;
    float levelSmoothCoeff = 0.01f;
    float controlSmoothCoeff = 0.004f;
    float delaySmoothCoeff = 0.002f;

    float smoothedLevel = 0.55f;
    float smoothedBlend = 0.5f;
    float smoothedFeedback = 0.25f;
    float smoothedDelay = 0.45f;
    float smoothedDepth = 0.2f;

    float feedbackState = 0.f;
    float heldBbdSample = 0.f;
    float clockPhase = 0.f;
    float lfoPhase = 0.f;
    float overloadEnv = 0.f;
    uint32_t rng = 0x4567abcdU;

    OnePoleHighpass inputHp;
    OnePoleLowpass inputLp;
    OnePoleHighpass preBbdHp;
    OnePoleLowpass preBbdLp;
    OnePoleHighpass postBbdHp;
    OnePoleLowpass postBbdLp1;
    OnePoleLowpass postBbdLp2;
    OnePoleLowpass outputLp;
    Ne570Approx compander;

    float smoothingCoeff(float ms) const {
        return 1.f - std::exp(-1.f / (sampleRate * ms * 0.001f));
    }

    static Params sanitizeParams(const Params& raw) {
        Params p = raw;
        p.level = clampf(sanitize(p.level), 0.f, 1.f);
        p.blend = clampf(sanitize(p.blend), 0.f, 1.f);
        p.feedback = clampf(sanitize(p.feedback), 0.f, 1.f);
        p.delay = clampf(sanitize(p.delay), 0.f, 1.f);
        p.depth = clampf(sanitize(p.depth), 0.f, 1.f);
        if (p.artifactProfile < ARTIFACT_CLEAN || p.artifactProfile > ARTIFACT_WORN) {
            p.artifactProfile = ARTIFACT_AUTHENTIC;
        }
        return p;
    }

    static float artifactAmount(int profile) {
        switch (profile) {
            case ARTIFACT_CLEAN: return 0.25f;
            case ARTIFACT_WORN: return 1.55f;
            case ARTIFACT_AUTHENTIC:
            default: return 1.f;
        }
    }

    static float feedbackGain(float feedback) {
        feedback = clampf(feedback, 0.f, 1.f);
        float gain = 1.12f * std::pow(feedback, 1.55f);
        if (feedback > 0.78f) {
            gain += (feedback - 0.78f) * 0.34f;
        }
        return gain;
    }

    float readDelay(float delaySamples) const {
        float readPos = static_cast<float>(writeIndex) - delaySamples;
        while (readPos < 0.f) {
            readPos += static_cast<float>(kDelayBufferSize);
        }

        const int i1 = static_cast<int>(readPos) & kDelayBufferMask;
        const int i0 = (i1 - 1) & kDelayBufferMask;
        const int i2 = (i1 + 1) & kDelayBufferMask;
        const int i3 = (i1 + 2) & kDelayBufferMask;
        const float frac = readPos - std::floor(readPos);

        const float y0 = delayLine[i0];
        const float y1 = delayLine[i1];
        const float y2 = delayLine[i2];
        const float y3 = delayLine[i3];
        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    void writeDelay(float x) {
        delayLine[writeIndex] = sanitize(x);
        writeIndex = (writeIndex + 1) & kDelayBufferMask;
    }

    void updateOverload(float x) {
        const float target = std::max(0.f, std::fabs(x) - 0.58f);
        const float coeff = target > overloadEnv ? 0.04f : 0.0015f;
        overloadEnv += coeff * (target - overloadEnv);
    }

    float randomSigned() {
        rng = rng * 1664525U + 1013904223U;
        const float unit = static_cast<float>((rng >> 8) & 0x00ffffffU) * (1.f / 8388607.5f);
        return unit - 1.f;
    }

    static float opAmpSaturate(float x, float drive, float bias) {
        const float shifted = x * drive + bias;
        const float asymmetric = std::tanh(shifted) - 0.08f * std::tanh(shifted * shifted);
        return sanitize(asymmetric / std::max(0.001f, drive));
    }

    static float softLimit(float x, float limit) {
        return limit * std::tanh(x / std::max(0.001f, limit));
    }

    static float sanitize(float x) {
        if (!std::isfinite(x)) {
            return 0.f;
        }
        return clampf(x, -8.f, 8.f);
    }

    static float clampf(float x, float lo, float hi) {
        return std::max(lo, std::min(hi, x));
    }
};

