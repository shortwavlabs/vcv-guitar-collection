#pragma once

// Define M_PI for Windows before including cmath
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

// Use the new nam_rack implementation (no Eigen dependency)
#include "nam_rack/dsp.h"
#include "nam_rack/model_loader.h"
#include "nam_rack/activations.h"

// Use VCV Rack's resampler instead of libsamplerate
#include <dsp/resampler.hpp>

#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>

/**
 * BiquadFilter - Second-order IIR filter for tone shaping
 */
struct BiquadFilter {
    // Coefficients
    float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
    // State
    float z1 = 0.f, z2 = 0.f;
    
    void setLowShelf(double sr, double freq, double q, double gainDb) {
        double A = std::pow(10.0, gainDb / 40.0);
        double w0 = 2.0 * M_PI * freq / sr;
        double cosw0 = std::cos(w0);
        double sinw0 = std::sin(w0);
        double alpha = sinw0 / (2.0 * q);
        
        double a0 = (A + 1.0) + (A - 1.0) * cosw0 + 2.0 * std::sqrt(A) * alpha;
        b0 = static_cast<float>(A * ((A + 1.0) - (A - 1.0) * cosw0 + 2.0 * std::sqrt(A) * alpha) / a0);
        b1 = static_cast<float>(2.0 * A * ((A - 1.0) - (A + 1.0) * cosw0) / a0);
        b2 = static_cast<float>(A * ((A + 1.0) - (A - 1.0) * cosw0 - 2.0 * std::sqrt(A) * alpha) / a0);
        a1 = static_cast<float>(-2.0 * ((A - 1.0) + (A + 1.0) * cosw0) / a0);
        a2 = static_cast<float>(((A + 1.0) + (A - 1.0) * cosw0 - 2.0 * std::sqrt(A) * alpha) / a0);
    }
    
    void setHighShelf(double sr, double freq, double q, double gainDb) {
        double A = std::pow(10.0, gainDb / 40.0);
        double w0 = 2.0 * M_PI * freq / sr;
        double cosw0 = std::cos(w0);
        double sinw0 = std::sin(w0);
        double alpha = sinw0 / (2.0 * q);
        
        double a0 = (A + 1.0) - (A - 1.0) * cosw0 + 2.0 * std::sqrt(A) * alpha;
        b0 = static_cast<float>(A * ((A + 1.0) + (A - 1.0) * cosw0 + 2.0 * std::sqrt(A) * alpha) / a0);
        b1 = static_cast<float>(-2.0 * A * ((A - 1.0) + (A + 1.0) * cosw0) / a0);
        b2 = static_cast<float>(A * ((A + 1.0) + (A - 1.0) * cosw0 - 2.0 * std::sqrt(A) * alpha) / a0);
        a1 = static_cast<float>(2.0 * ((A - 1.0) - (A + 1.0) * cosw0) / a0);
        a2 = static_cast<float>(((A + 1.0) - (A - 1.0) * cosw0 - 2.0 * std::sqrt(A) * alpha) / a0);
    }
    
    void setPeaking(double sr, double freq, double q, double gainDb) {
        double A = std::pow(10.0, gainDb / 40.0);
        double w0 = 2.0 * M_PI * freq / sr;
        double cosw0 = std::cos(w0);
        double sinw0 = std::sin(w0);
        double alpha = sinw0 / (2.0 * q);
        
        double a0 = 1.0 + alpha / A;
        b0 = static_cast<float>((1.0 + alpha * A) / a0);
        b1 = static_cast<float>((-2.0 * cosw0) / a0);
        b2 = static_cast<float>((1.0 - alpha * A) / a0);
        a1 = static_cast<float>((-2.0 * cosw0) / a0);
        a2 = static_cast<float>((1.0 - alpha / A) / a0);
    }
    
    float process(float in) {
        float out = b0 * in + z1;
        z1 = b1 * in - a1 * out + z2;
        z2 = b2 * in - a2 * out;
        return out;
    }
    
    void reset() { z1 = z2 = 0.f; }
};

/**
 * NoiseGate - Hysteresis-based noise gate for guitar
 * 
 * Uses RMS envelope detection with hysteresis to prevent
 * chattering on decaying notes. Placed before input gain
 * and NAM processing since distortion amplifies noise.
 */
struct NoiseGate {
    // Parameters
    float threshold = -60.f;    // dB (default, range: -80 to 0)
    float attack = 0.0005f;     // seconds (range: 0.1-50 ms)
    float release = 0.1f;       // seconds (range: 10-500 ms)
    float hold = 0.05f;         // seconds
    float hysteresis = 6.f;     // dB
    
    // State
    float envelope = 0.f;
    float gain = 0.f;
    float holdCounter = 0.f;
    bool isOpen = false;        // LED: 1.0 when open, 0.0 when closed
    
    // Coefficients
    float envAttack = 0.f, envRelease = 0.f;
    float gainAttack = 0.f, gainRelease = 0.f;
    double sampleRate = 48000.0;
    
    void setSampleRate(double sr) {
        sampleRate = sr;
        recalculateCoefficients();
    }
    
    void setParameters(float threshDb, float attackMs, float releaseMs, float holdMs) {
        threshold = threshDb;
        attack = attackMs / 1000.f;
        release = releaseMs / 1000.f;
        hold = holdMs / 1000.f;
        recalculateCoefficients();
    }
    
    void recalculateCoefficients() {
        // Envelope follower coefficients (fast attack, slow release)
        envAttack = 1.f - std::exp(-1.f / (0.001f * static_cast<float>(sampleRate)));
        envRelease = 1.f - std::exp(-1.f / (0.05f * static_cast<float>(sampleRate)));
        
        // Gain smoothing coefficients
        gainAttack = 1.f - std::exp(-1.f / (attack * static_cast<float>(sampleRate)));
        gainRelease = 1.f - std::exp(-1.f / (release * static_cast<float>(sampleRate)));
    }
    
    float process(float sample) {
        // RMS envelope follower
        float rectified = sample * sample;
        float envCoeff = rectified > envelope ? envAttack : envRelease;
        envelope += envCoeff * (rectified - envelope);
        
        // Convert to dB
        float envDb = 10.f * std::log10(envelope + 1e-10f);
        
        // Hysteresis logic
        float openThreshold = threshold;
        float closeThreshold = threshold - hysteresis;
        
        if (isOpen) {
            if (envDb < closeThreshold) {
                holdCounter += 1.f / static_cast<float>(sampleRate);
                if (holdCounter >= hold) {
                    isOpen = false;
                    holdCounter = 0.f;
                }
            } else {
                holdCounter = 0.f;
            }
        } else {
            if (envDb > openThreshold) {
                isOpen = true;
                holdCounter = 0.f;
            }
        }
        
        // Target gain
        float targetGain = isOpen ? 1.f : 0.f;
        
        // Smooth gain changes
        float coeff = targetGain > gain ? gainAttack : gainRelease;
        gain += coeff * (targetGain - gain);
        
        return sample * gain;
    }
    
    void reset() {
        envelope = 0.f;
        gain = 0.f;
        holdCounter = 0.f;
        isOpen = false;
    }
};

/**
 * ToneStack - 5-band EQ for guitar amp tone shaping
 * 
 * Placed after NAM processing to shape output tone:
 * - Depth (Peaking, 80 Hz) - Low-end resonance
 * - Bass (Low Shelf, 100 Hz) - Low-end body
 * - Middle (Peaking, 650 Hz) - Midrange punch
 * - Treble (High Shelf, 3.2 kHz) - High-end clarity
 * - Presence (Peaking, 3.5 kHz) - Upper-mid articulation
 */
struct ToneStack {
    BiquadFilter depth;     // Peaking at 80 Hz
    BiquadFilter bass;      // Low shelf at 100 Hz
    BiquadFilter middle;    // Peaking at 650 Hz
    BiquadFilter treble;    // High shelf at 3.2 kHz
    BiquadFilter presence;  // Peaking at 3.5 kHz
    
    double sampleRate = 48000.0;
    float bassDb = 0.f, midDb = 0.f, trebleDb = 0.f, presenceDb = 0.f, depthDb = 0.f;
    
    void setSampleRate(double sr) {
        sampleRate = sr;
        updateFilters();
    }
    
    void setParameters(float bassGain, float midGain, float trebleGain, float presenceGain, float depthGain) {
        bassDb = bassGain;
        midDb = midGain;
        trebleDb = trebleGain;
        presenceDb = presenceGain;
        depthDb = depthGain;
        updateFilters();
    }
    
    void updateFilters() {
        depth.setPeaking(sampleRate, 80.0, 0.7, depthDb);
        bass.setLowShelf(sampleRate, 100.0, 0.7, bassDb);
        middle.setPeaking(sampleRate, 650.0, 0.7, midDb);
        treble.setHighShelf(sampleRate, 3200.0, 0.7, trebleDb);
        presence.setPeaking(sampleRate, 3500.0, 0.7, presenceDb);
    }
    
    float process(float sample) {
        sample = depth.process(sample);
        sample = bass.process(sample);
        sample = middle.process(sample);
        sample = treble.process(sample);
        sample = presence.process(sample);
        return sample;
    }
    
    void reset() {
        depth.reset();
        bass.reset();
        middle.reset();
        treble.reset();
        presence.reset();
    }
};

/**
 * NamDSP - Wrapper for Neural Amp Modeler DSP with resampling support
 *
 * Handles:
 * - Model loading and management
 * - Sample rate conversion (engine rate <-> model rate) using VCV Rack's Speex resampler
 * - Passthrough when no model loaded
 * - Noise gate (before NAM processing)
 * - 5-band tone stack (after NAM processing)
 */
class NamDSP {
public:
    static constexpr int MAX_BLOCK_SIZE = 2048;
    static constexpr int MAX_RESAMPLE_RATIO = 4;  // Support up to 4x resampling
    static constexpr float IDLE_GATE_EPSILON = 1.0e-6f;
    static constexpr int IDLE_GATE_MIN_BLOCKS = 4;

    NamDSP() {
        // Pre-allocate buffers
        resampleInBuffer.resize(MAX_BLOCK_SIZE * MAX_RESAMPLE_RATIO, 0.f);
        resampleOutBuffer.resize(MAX_BLOCK_SIZE * MAX_RESAMPLE_RATIO, 0.f);
        modelOutputBuffer.resize(MAX_BLOCK_SIZE * MAX_RESAMPLE_RATIO, 0.f);
        gatedInputBuffer.resize(MAX_BLOCK_SIZE, 0.f);

        // Initialize resamplers with quality setting
        srcIn.setQuality(4);  // Better performance (0-10 scale)
        srcOut.setQuality(4);
    }

    ~NamDSP() = default;

    // Model management
    bool loadModel(const std::string& path) {
        nam::ModelConfig loadedConfig;
        try {
            lastLoadDiagnostics.clear();
            auto newModel = nam::loadModel(path, loadedConfig);
            if (!newModel) {
                lastLoadError = "nam::loadModel returned null DSP";
                lastLoadDiagnostics = loadedConfig.loadDiagnostics;
                return false;
            }

            // Get model sample rate
            modelSampleRate = newModel->getExpectedSampleRate();
            if (modelSampleRate <= 0) {
                modelSampleRate = 48000.0;  // Default if not specified
            }

            model = std::move(newModel);
            modelPath = path;
            lastLoadError.clear();
            lastLoadDiagnostics = loadedConfig.loadDiagnostics;

            // Update resampler rates
            updateResamplerRates();

            return true;
        } catch (const std::exception& e) {
            lastLoadError = e.what();
            lastLoadDiagnostics = loadedConfig.loadDiagnostics;
            return false;
        } catch (...) {
            lastLoadError = "Unknown exception while loading NAM model";
            lastLoadDiagnostics = loadedConfig.loadDiagnostics;
            return false;
        }
    }

    void unloadModel() {
        model.reset();
        modelPath.clear();
        lastLoadError.clear();
        lastLoadDiagnostics.clear();
    }

    bool isModelLoaded() const { return model != nullptr; }

    double getModelSampleRate() const { return modelSampleRate; }

    std::string getModelPath() const { return modelPath; }
    std::string getLastLoadError() const { return lastLoadError; }
    std::string getLoadDiagnostics() const { return lastLoadDiagnostics; }

    std::string getModelName() const {
        if (modelPath.empty()) return "";

        // Extract filename without path
        size_t lastSlash = modelPath.find_last_of("/\\");
        std::string filename = (lastSlash != std::string::npos) ?
                              modelPath.substr(lastSlash + 1) : modelPath;

        // Remove extension
        size_t lastDot = filename.find_last_of('.');
        if (lastDot != std::string::npos) {
            filename = filename.substr(0, lastDot);
        }

        return filename;
    }

    // Processing
    void setSampleRate(double rate) {
        if (std::abs(engineSampleRate - rate) > 0.1) {
            engineSampleRate = rate;
            noiseGate.setSampleRate(rate);
            toneStack.setSampleRate(rate);
            updateResamplerRates();
        }
    }

    void process(const float* input, float* output, int numFrames) {
        if (!model) {
            // Passthrough when no model
            std::copy(input, input + numFrames, output);
            return;
        }

        // Apply noise gate to input
        if (numFrames > static_cast<int>(gatedInputBuffer.size())) {
            gatedInputBuffer.resize(numFrames, 0.f);
        }
        for (int i = 0; i < numFrames; i++) {
            gatedInputBuffer[i] = noiseGate.process(input[i]);
        }

        bool nearSilent = true;
        for (int i = 0; i < numFrames; i++) {
            if (std::fabs(gatedInputBuffer[i]) > IDLE_GATE_EPSILON) {
                nearSilent = false;
                break;
            }
        }

        if (!noiseGate.isOpen && nearSilent) {
            idleGateSilentBlockCount++;
        } else {
            idleGateSilentBlockCount = 0;
        }

        if (idleGateSilentBlockCount >= IDLE_GATE_MIN_BLOCKS) {
            for (int i = 0; i < numFrames; i++) {
                float y = toneStack.process(0.0f);
                output[i] = std::isfinite(y) ? y : 0.0f;
            }
            return;
        }

        // Check if resampling is needed
        double ratio = modelSampleRate / engineSampleRate;

        if (std::abs(ratio - 1.0) < 0.001) {
            // No resampling needed - direct processing
            if (numFrames > static_cast<int>(modelOutputBuffer.size())) {
                modelOutputBuffer.resize(numFrames, 0.f);
            }
            model->process(gatedInputBuffer.data(), modelOutputBuffer.data(), numFrames);
            for (int i = 0; i < numFrames; i++) {
                float y = toneStack.process(modelOutputBuffer[i]);
                output[i] = std::isfinite(y) ? y : 0.0f;
            }
        } else {
            // Resampling required using Rack's SampleRateConverter
            int maxResampledFrames = static_cast<int>(numFrames * ratio) + 16;
            if (maxResampledFrames < 1) {
                maxResampledFrames = 1;
            }

            if (maxResampledFrames > static_cast<int>(resampleInBuffer.size())) {
                resampleInBuffer.resize(maxResampledFrames, 0.f);
                resampleOutBuffer.resize(maxResampledFrames, 0.f);
            }

            std::fill(output, output + numFrames, 0.f);

            // Upsample input to model rate
            int inFrames = numFrames;
            int outFrames = maxResampledFrames;
            srcIn.process(gatedInputBuffer.data(), 1, &inFrames, resampleInBuffer.data(), 1, &outFrames);

            int processedFrames = outFrames;

            // Process through NAM (all float now)
            if (processedFrames > 0) {
                model->process(resampleInBuffer.data(), resampleOutBuffer.data(), processedFrames);
            }

            // Downsample output to engine rate
            inFrames = processedFrames;
            outFrames = numFrames;
            if (processedFrames > 0) {
                srcOut.process(resampleOutBuffer.data(), 1, &inFrames, output, 1, &outFrames);
            }

            // Apply tone stack to output
            for (int i = 0; i < numFrames; i++) {
                float y = toneStack.process(output[i]);
                output[i] = std::isfinite(y) ? y : 0.0f;
            }
        }
    }

    void reset() {
        if (model) {
            model->reset(modelSampleRate, MAX_BLOCK_SIZE * MAX_RESAMPLE_RATIO);
        }
        idleGateSilentBlockCount = 0;
        noiseGate.reset();
        toneStack.reset();
        updateResamplerRates();
    }

    // Noise Gate control
    void setNoiseGate(float thresholdDb, float attackMs, float releaseMs, float holdMs) {
        noiseGate.setParameters(thresholdDb, attackMs, releaseMs, holdMs);
    }

    bool isGateOpen() const { return noiseGate.isOpen; }

    // Tone Stack control (values in dB, typically -12 to +12)
    void setToneStack(float bass, float middle, float treble, float presence, float depth) {
        toneStack.setParameters(bass, middle, treble, presence, depth);
    }

    // Monitoring
    bool isSampleRateMismatched() const {
        return std::abs(engineSampleRate - modelSampleRate) > 1.0;
    }

private:
    std::unique_ptr<nam::DSP> model;
    std::string modelPath;
    std::string lastLoadError;
    std::string lastLoadDiagnostics;

    // Resampling state using VCV Rack's SampleRateConverter (Speex-based)
    rack::dsp::SampleRateConverter<1> srcIn;
    rack::dsp::SampleRateConverter<1> srcOut;
    double engineSampleRate = 48000.0;
    double modelSampleRate = 48000.0;

    // Noise Gate (before NAM)
    NoiseGate noiseGate;

    // Tone Stack (5-band EQ, after NAM)
    ToneStack toneStack;

    // Pre-allocated buffers for resampling (all float now, no Eigen)
    std::vector<float> resampleInBuffer;
    std::vector<float> resampleOutBuffer;
    std::vector<float> modelOutputBuffer;
    std::vector<float> gatedInputBuffer;
    int idleGateSilentBlockCount = 0;

    void updateResamplerRates() {
        // srcIn: engine rate -> model rate (upsample if model rate > engine rate)
        srcIn.setRates(static_cast<int>(engineSampleRate), static_cast<int>(modelSampleRate));

        // srcOut: model rate -> engine rate (downsample if model rate > engine rate)
        srcOut.setRates(static_cast<int>(modelSampleRate), static_cast<int>(engineSampleRate));
    }
};
