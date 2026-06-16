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
    static constexpr float DEFAULT_SAMPLE_RATE = 48000.f;

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
        setSampleRate(DEFAULT_SAMPLE_RATE);
        reset();
    }

    void setSampleRate(float rate) {
        if (!std::isfinite(rate) || rate < 8000.f) {
            rate = DEFAULT_SAMPLE_RATE;
        }

        sampleRate = rate;
        sampleTime = 1.f / sampleRate;
        levelSmoothCoeff = smoothingCoeff(8.f);
        controlSmoothCoeff = smoothingCoeff(18.f);
        delaySmoothCoeff = smoothingCoeff(22.f);

        inputFilters.setSampleRate(sampleRate);
        preBbdFilters.setSampleRate(sampleRate);
        postBbdFilters.setSampleRate(sampleRate);
        outputFilters.setSampleRate(sampleRate);
        compander.setSampleRate(sampleRate);

        inputAmp.setSampleRate(sampleRate);
        bbdDriverAmp.setSampleRate(sampleRate);
        recoveryAmp.setSampleRate(sampleRate);
        feedbackAmp.setSampleRate(sampleRate);
        outputAmp.setSampleRate(sampleRate);

        inputAmp.configure(1.75f, 0.032f, 1.0f, 0.95f, 0.09f);
        bbdDriverAmp.configure(1.38f, -0.018f, 0.92f, 0.72f, 0.07f);
        recoveryAmp.configure(1.32f, 0.012f, 1.02f, 0.82f, 0.06f);
        feedbackAmp.configure(1.2f, -0.006f, 1.15f, 0.75f, 0.08f);
        outputAmp.configure(1.08f, 0.f, 1.0f, 1.1f, 0.04f);

        for (int i = 0; i < 4; ++i) {
            bbdChips[i].configure(i);
        }
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
        filterUpdateCountdown = 0;

        inputFilters.reset();
        preBbdFilters.reset();
        postBbdFilters.reset();
        outputFilters.reset();
        compander.reset();

        inputAmp.reset();
        bbdDriverAmp.reset();
        recoveryAmp.reset();
        feedbackAmp.reset();
        outputAmp.reset();

        for (int i = 0; i < 4; ++i) {
            bbdChips[i].reset();
        }
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
        const float basePeriodUs = clockPeriodUsForDelay(smoothedDelay);
        const float lfo = advanceLfo(p.vibrato);
        const float maxMod = p.vibrato ? 0.18f : 0.085f;
        float periodUs = basePeriodUs * (1.f + lfo * maxMod * smoothedDepth);
        periodUs = clampf(periodUs, kMinClockPeriodUs * 0.65f, kMaxClockPeriodUs * 1.35f);

        const float clockHz = 1000000.f / periodUs;
        const float delaySeconds = bbdDelaySecondsForClockPeriodUs(periodUs);
        const float delayNorm = clampf((periodUs - kMinClockPeriodUs) / (kMaxClockPeriodUs - kMinClockPeriodUs), 0.f, 1.f);

        if (--filterUpdateCountdown <= 0) {
            preBbdFilters.setClock(clockHz);
            postBbdFilters.setClock(clockHz);
            filterUpdateCountdown = 16;
        }

        const float inputGain = 0.055f + 3.25f * std::pow(clampf(smoothedLevel, 0.f, 1.f), 2.08f);
        const float feedbackReturn = feedbackAmp.process(feedbackState * feedbackGain(smoothedFeedback));
        float preamp = inputFilters.process(input * inputGain + feedbackReturn);
        preamp = inputAmp.process(preamp);
        updateOverload(preamp);

        float compressed = compander.compress(preamp);
        float bbdDrive = preBbdFilters.process(compressed);
        bbdDrive = bbdDriverAmp.process(bbdDrive);

        clockBbd(bbdDrive, clockHz, artifact, delayNorm);

        float bbdOut = heldBbdSample;
        const float hiss = randomSigned() * (0.00006f + 0.0026f * delayNorm * delayNorm) * artifact;
        const float clockBleed = std::sin(2.f * static_cast<float>(M_PI) * clockPhase) *
            (0.00012f + 0.0014f * delayNorm) * artifact;
        bbdOut += hiss + clockBleed;

        float post = postBbdFilters.process(bbdOut);
        post = compander.expand(post);
        post = recoveryAmp.process(post);
        post = outputFilters.process(post);

        const float feedbackDamping = 1.f - 0.13f * artifact * delayNorm;
        feedbackState = softLimit(post * feedbackDamping, 1.8f);

        const float wet = post;
        const float mixed = input * (1.f - smoothedBlend) + wet * smoothedBlend;
        float output = outputAmp.process(mixed);
        if (!p.engaged) {
            output = input;
        }

        Result result;
        result.direct = input;
        result.wet = sanitize(wet);
        result.output = sanitize(output);
        result.overload = clampf(overloadEnv * 1.6f, 0.f, 1.f);
        result.clockHz = clockHz;
        result.delaySeconds = delaySeconds;
        return result;
    }

    float getOverload() const {
        return clampf(overloadEnv * 1.6f, 0.f, 1.f);
    }

    float getSampleRate() const {
        return sampleRate;
    }

    static float clockPeriodUsForDelay(float delayNorm) {
        delayNorm = clampf(delayNorm, 0.f, 1.f);
        return kMinClockPeriodUs * std::pow(kMaxClockPeriodUs / kMinClockPeriodUs, delayNorm);
    }

    static float bbdDelaySecondsForClockPeriodUs(float periodUs) {
        return static_cast<float>(kBbdDelayTicks) * periodUs * 0.000001f;
    }

    static float delaySecondsForDelay(float delayNorm) {
        return bbdDelaySecondsForClockPeriodUs(clockPeriodUsForDelay(delayNorm));
    }

private:
    static constexpr float kMinClockPeriodUs = 8.f;
    static constexpr float kMaxClockPeriodUs = 100.f;
    static const int kBbdDelayTicks = 4096;
    static const int kDelayBufferSize = 8192;
    static const int kDelayBufferMask = kDelayBufferSize - 1;

    class Biquad {
    public:
        void setSampleRate(float sr) {
            sampleRate = std::max(8000.f, sr);
        }

        void setLowpass(float hz, float q) {
            const float fc = clampf(hz, 8.f, sampleRate * 0.45f);
            const float qq = std::max(0.35f, q);
            const float w0 = 2.f * static_cast<float>(M_PI) * fc / sampleRate;
            const float cs = std::cos(w0);
            const float sn = std::sin(w0);
            const float alpha = sn / (2.f * qq);
            setNormalized((1.f - cs) * 0.5f, 1.f - cs, (1.f - cs) * 0.5f,
                          1.f + alpha, -2.f * cs, 1.f - alpha);
        }

        void setHighpass(float hz, float q) {
            const float fc = clampf(hz, 8.f, sampleRate * 0.45f);
            const float qq = std::max(0.35f, q);
            const float w0 = 2.f * static_cast<float>(M_PI) * fc / sampleRate;
            const float cs = std::cos(w0);
            const float sn = std::sin(w0);
            const float alpha = sn / (2.f * qq);
            setNormalized((1.f + cs) * 0.5f, -(1.f + cs), (1.f + cs) * 0.5f,
                          1.f + alpha, -2.f * cs, 1.f - alpha);
        }

        void setPeaking(float hz, float q, float gainDb) {
            const float fc = clampf(hz, 8.f, sampleRate * 0.45f);
            const float qq = std::max(0.35f, q);
            const float a = std::pow(10.f, gainDb / 40.f);
            const float w0 = 2.f * static_cast<float>(M_PI) * fc / sampleRate;
            const float cs = std::cos(w0);
            const float sn = std::sin(w0);
            const float alpha = sn / (2.f * qq);
            setNormalized(1.f + alpha * a, -2.f * cs, 1.f - alpha * a,
                          1.f + alpha / a, -2.f * cs, 1.f - alpha / a);
        }

        float process(float x) {
            const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1;
            x1 = x;
            y2 = y1;
            y1 = sanitize(y);
            return y1;
        }

        void reset() {
            x1 = x2 = y1 = y2 = 0.f;
        }

    private:
        float sampleRate = 48000.f;
        float b0 = 1.f;
        float b1 = 0.f;
        float b2 = 0.f;
        float a1 = 0.f;
        float a2 = 0.f;
        float x1 = 0.f;
        float x2 = 0.f;
        float y1 = 0.f;
        float y2 = 0.f;

        void setNormalized(float nb0, float nb1, float nb2, float a0, float na1, float na2) {
            if (std::fabs(a0) < 1e-12f || !std::isfinite(a0)) {
                b0 = 1.f;
                b1 = b2 = a1 = a2 = 0.f;
                return;
            }
            b0 = nb0 / a0;
            b1 = nb1 / a0;
            b2 = nb2 / a0;
            a1 = na1 / a0;
            a2 = na2 / a0;
        }
    };

    class OnePoleLowpass {
    public:
        void setSampleRate(float sr) {
            sampleRate = std::max(8000.f, sr);
            setCutoff(cutoff);
        }

        void setCutoff(float hz) {
            cutoff = clampf(hz, 4.f, sampleRate * 0.45f);
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

    class OpAmpStage {
    public:
        void setSampleRate(float sr) {
            sampleRate = std::max(8000.f, sr);
        }

        void configure(float newDrive, float newBias, float newLimit, float slewPerMs, float newAsymmetry) {
            drive = newDrive;
            bias = newBias;
            limit = newLimit;
            slewPerSecond = slewPerMs * 1000.f;
            asymmetry = newAsymmetry;
        }

        float process(float x) {
            const float shifted = x * drive + bias;
            float shaped = std::tanh(shifted);
            shaped -= asymmetry * std::tanh(shifted * shifted);
            shaped = (shaped / std::max(0.001f, drive)) * limit;

            const float maxStep = slewPerSecond / sampleRate;
            const float delta = clampf(shaped - last, -maxStep, maxStep);
            last = sanitize(last + delta);
            return last;
        }

        void reset() {
            last = 0.f;
        }

    private:
        float sampleRate = 48000.f;
        float drive = 1.f;
        float bias = 0.f;
        float limit = 1.f;
        float slewPerSecond = 1000.f;
        float asymmetry = 0.05f;
        float last = 0.f;
    };

    class InputFilterBank {
    public:
        void setSampleRate(float sr) {
            hp.setSampleRate(sr);
            lp.setSampleRate(sr);
            presence.setSampleRate(sr);
            hp.setHighpass(rcCutoff(100000.f, 0.1e-6f), 0.68f);
            lp.setLowpass(14500.f, 0.65f);
            presence.setPeaking(rcCutoff(100000.f, 1.2e-9f), 0.72f, 1.8f);
        }

        float process(float x) {
            x = hp.process(x);
            x = presence.process(x);
            return lp.process(x);
        }

        void reset() {
            hp.reset();
            lp.reset();
            presence.reset();
        }

    private:
        Biquad hp;
        Biquad lp;
        Biquad presence;
    };

    class PreBbdFilterBank {
    public:
        void setSampleRate(float sr) {
            sampleRate = std::max(8000.f, sr);
            hp.setSampleRate(sampleRate);
            lp1.setSampleRate(sampleRate);
            lp2.setSampleRate(sampleRate);
            biasServo.setSampleRate(sampleRate);
            hp.setHighpass(42.f, 0.62f);
            biasServo.setCutoff(6.f);
            setClock(45000.f);
        }

        void setClock(float clockHz) {
            const float u2aC11 = rcCutoff(24000.f, 2.7e-9f);
            const float u2aC12 = rcCutoff(16000.f, 2.7e-9f);
            const float clockLimit = clampf(clockHz * 0.42f, 1400.f, 13500.f);
            lp1.setLowpass(std::min(u2aC11, clockLimit), 0.62f);
            lp2.setLowpass(std::min(u2aC12, clockLimit * 1.18f), 0.58f);
        }

        float process(float x) {
            x -= 0.08f * biasServo.process(x);
            x = hp.process(x);
            x = lp1.process(x);
            return lp2.process(x);
        }

        void reset() {
            hp.reset();
            lp1.reset();
            lp2.reset();
            biasServo.reset();
        }

    private:
        float sampleRate = 48000.f;
        Biquad hp;
        Biquad lp1;
        Biquad lp2;
        OnePoleLowpass biasServo;
    };

    class PostBbdFilterBank {
    public:
        void setSampleRate(float sr) {
            sampleRate = std::max(8000.f, sr);
            hp.setSampleRate(sampleRate);
            lp1.setSampleRate(sampleRate);
            lp2.setSampleRate(sampleRate);
            lp3.setSampleRate(sampleRate);
            mildNasal.setSampleRate(sampleRate);
            hp.setHighpass(72.f, 0.62f);
            mildNasal.setPeaking(rcCutoff(33200.f, 2.7e-9f), 0.8f, -1.5f);
            setClock(45000.f);
        }

        void setClock(float clockHz) {
            const float fixedA = rcCutoff(15000.f, 2.7e-9f);
            const float fixedB = rcCutoff(16000.f, 2.7e-9f);
            const float fixedC = rcCutoff(33200.f, 2.7e-9f);
            const float clockLimit = clampf(clockHz * 0.34f, 1350.f, 5200.f);
            lp1.setLowpass(std::min(fixedA, clockLimit), 0.58f);
            lp2.setLowpass(std::min(fixedB, clockLimit * 0.86f), 0.55f);
            lp3.setLowpass(std::min(fixedC + 0.25f * clockLimit, clockLimit * 0.78f), 0.55f);
        }

        float process(float x) {
            x = hp.process(x);
            x = lp1.process(x);
            x = lp2.process(x);
            x = mildNasal.process(x);
            return lp3.process(x);
        }

        void reset() {
            hp.reset();
            lp1.reset();
            lp2.reset();
            lp3.reset();
            mildNasal.reset();
        }

    private:
        float sampleRate = 48000.f;
        Biquad hp;
        Biquad lp1;
        Biquad lp2;
        Biquad lp3;
        Biquad mildNasal;
    };

    class OutputFilterBank {
    public:
        void setSampleRate(float sr) {
            hp.setSampleRate(sr);
            lp.setSampleRate(sr);
            sweetener.setSampleRate(sr);
            hp.setHighpass(18.f, 0.7f);
            lp.setLowpass(9800.f, 0.62f);
            sweetener.setPeaking(1200.f, 0.75f, -0.8f);
        }

        float process(float x) {
            x = hp.process(x);
            x = sweetener.process(x);
            return lp.process(x);
        }

        void reset() {
            hp.reset();
            lp.reset();
            sweetener.reset();
        }

    private:
        Biquad hp;
        Biquad lp;
        Biquad sweetener;
    };

    class Ne570Compander {
    public:
        void setSampleRate(float sr) {
            sampleRate = std::max(8000.f, sr);
            compAttack = coeffForMs(1.6f);
            compRelease = coeffForMs(72.f);
            expAttack = coeffForMs(4.2f);
            expRelease = coeffForMs(115.f);
            gainSmoothing = coeffForMs(7.f);
        }

        void reset() {
            compEnv = 0.f;
            expEnv = 0.f;
            compGain = 1.f;
            expGain = 1.f;
        }

        float compress(float x) {
            compEnv = envelope(compEnv, rectifier(x), compAttack, compRelease);
            const float sidechain = std::tanh(compEnv * 2.1f);
            const float targetGain = clampf(0.62f / std::pow(sidechain + 0.028f, 0.42f), 0.31f, 2.75f);
            compGain += gainSmoothing * (targetGain - compGain);
            return sanitize(x * compGain * 0.72f);
        }

        float expand(float x) {
            expEnv = envelope(expEnv, rectifier(x), expAttack, expRelease);
            const float sidechain = std::tanh(expEnv * 2.4f);
            const float targetGain = clampf(std::pow(sidechain + 0.022f, 0.48f) * 1.85f, 0.18f, 2.05f);
            expGain += gainSmoothing * (targetGain - expGain);
            return sanitize(x * expGain);
        }

    private:
        float sampleRate = 48000.f;
        float compAttack = 0.01f;
        float compRelease = 0.0002f;
        float expAttack = 0.004f;
        float expRelease = 0.00015f;
        float gainSmoothing = 0.002f;
        float compEnv = 0.f;
        float expEnv = 0.f;
        float compGain = 1.f;
        float expGain = 1.f;

        float coeffForMs(float ms) const {
            return 1.f - std::exp(-1.f / (sampleRate * ms * 0.001f));
        }

        static float rectifier(float x) {
            return std::fabs(x) + 0.055f * x * x;
        }

        static float envelope(float env, float x, float attack, float release) {
            const float coeff = x > env ? attack : release;
            return env + coeff * (x - env);
        }
    };

    class BbdChip {
    public:
        void configure(int chipIndex) {
            index = chipIndex;
            gain = 0.9975f - 0.0011f * static_cast<float>(chipIndex);
            bias = (chipIndex % 2 == 0 ? 1.f : -1.f) * (0.0015f + 0.0007f * chipIndex);
            clockPhaseOffset = 0.25f * static_cast<float>(chipIndex);
        }

        float processClockEdge(float x, float clockHz, float artifact, float delayNorm, float clockPhase) {
            const float tickRate = std::max(1000.f, clockHz);
            const float cutoff = clampf(clockHz * (0.32f - 0.025f * index), 1100.f, 12500.f);
            const float coeff = 1.f - std::exp(-2.f * static_cast<float>(M_PI) * cutoff / tickRate);

            float v = x + bias * artifact;
            v = std::tanh(v * (1.06f + 0.08f * index)) / (1.06f + 0.08f * index);
            memory += coeff * (v - memory);

            const float feedthrough = std::sin(2.f * static_cast<float>(M_PI) * (clockPhase + clockPhaseOffset)) *
                (0.00006f + 0.00022f * delayNorm) * artifact;
            return sanitize(memory * gain + feedthrough);
        }

        void reset() {
            memory = 0.f;
        }

    private:
        int index = 0;
        float gain = 0.997f;
        float bias = 0.f;
        float clockPhaseOffset = 0.f;
        float memory = 0.f;
    };

    std::vector<float> delayLine;
    int writeIndex = 0;

    float sampleRate = DEFAULT_SAMPLE_RATE;
    float sampleTime = 1.f / DEFAULT_SAMPLE_RATE;
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
    int filterUpdateCountdown = 0;

    InputFilterBank inputFilters;
    PreBbdFilterBank preBbdFilters;
    PostBbdFilterBank postBbdFilters;
    OutputFilterBank outputFilters;
    Ne570Compander compander;
    OpAmpStage inputAmp;
    OpAmpStage bbdDriverAmp;
    OpAmpStage recoveryAmp;
    OpAmpStage feedbackAmp;
    OpAmpStage outputAmp;
    BbdChip bbdChips[4];

    float smoothingCoeff(float ms) const {
        return 1.f - std::exp(-1.f / (sampleRate * ms * 0.001f));
    }

    float advanceLfo(bool vibrato) {
        const float lfoRate = vibrato ? 1.65f : 0.35f;
        lfoPhase += lfoRate * sampleTime;
        if (lfoPhase >= 1.f) {
            lfoPhase -= std::floor(lfoPhase);
        }
        const float phase = 2.f * static_cast<float>(M_PI) * lfoPhase;
        return std::sin(phase) + 0.08f * std::sin(2.f * phase + 0.6f);
    }

    void clockBbd(float bbdDrive, float clockHz, float artifact, float delayNorm) {
        clockPhase += clockHz * sampleTime;
        int edges = 0;
        while (clockPhase >= 1.f && edges < 32) {
            const float rawDelayed = readDelayTicks(kBbdDelayTicks);
            writeDelayTick(bbdDrive);

            float staged = rawDelayed;
            for (int i = 0; i < 4; ++i) {
                staged = bbdChips[i].processClockEdge(staged, clockHz, artifact, delayNorm, clockPhase);
            }
            heldBbdSample = staged;

            clockPhase -= 1.f;
            ++edges;
        }
        if (clockPhase >= 1.f) {
            clockPhase -= std::floor(clockPhase);
        }
    }

    float readDelayTicks(int ticks) const {
        const int readIndex = (writeIndex - ticks) & kDelayBufferMask;
        return delayLine[readIndex];
    }

    void writeDelayTick(float x) {
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

    static float softLimit(float x, float limit) {
        return limit * std::tanh(x / std::max(0.001f, limit));
    }

    static float rcCutoff(float resistanceOhms, float capacitanceFarads) {
        return 1.f / (2.f * static_cast<float>(M_PI) * resistanceOhms * capacitanceFarads);
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
