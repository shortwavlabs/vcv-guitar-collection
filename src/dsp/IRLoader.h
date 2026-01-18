#pragma once

// Define M_PI for Windows before including cmath
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <dsp/resampler.hpp>

// Custom WAV file loader
#include "WavFile.h"

/**
 * IRLoader - Impulse Response file loader with resampling and normalization
 * 
 * Loads WAV files for cabinet impulse responses:
 * - Supports mono and stereo files (stereo is summed to mono)
 * - Automatically resamples to target sample rate
 * - Optional peak normalization
 * - Trims IRs longer than MAX_IR_LENGTH (1 second at target rate)
 */
class IRLoader {
public:
    static constexpr size_t MAX_IR_SECONDS = 1;  // Maximum IR length in seconds
    
    IRLoader() = default;
    ~IRLoader() = default;
    
    /**
     * Load a WAV file from disk
     * @param filePath Path to the WAV file
     * @return true if successful, false on error
     */
    bool load(const std::string& filePath) {
        reset();
        
        WavFile wav;
        if (!wav.load(filePath)) {
            return false;
        }
        
        originalSampleRate = wav.getSampleRate();
        originalChannels = wav.getChannels();
        
        const std::vector<float>& rawSamples = wav.getSamples();
        size_t totalFrames = wav.getFrameCount();
        
        if (totalFrames == 0) {
            return false;
        }
        
        // Convert to mono (average channels)
        samples.resize(totalFrames);
        if (originalChannels == 1) {
            // Already mono
            for (size_t i = 0; i < totalFrames; i++) {
                samples[i] = rawSamples[i];
            }
        } else {
            // Sum and average channels
            for (size_t i = 0; i < totalFrames; i++) {
                float sum = 0.f;
                for (unsigned int ch = 0; ch < originalChannels; ch++) {
                    sum += rawSamples[i * originalChannels + ch];
                }
                samples[i] = sum / static_cast<float>(originalChannels);
            }
        }
        
        // Trim to max length (1 second at original rate)
        size_t maxSamples = originalSampleRate * MAX_IR_SECONDS;
        if (samples.size() > maxSamples) {
            samples.resize(maxSamples);
        }
        
        originalLength = samples.size();
        // Initialize currentSampleRate to originalSampleRate in case resampleTo is not called
        currentSampleRate = static_cast<float>(originalSampleRate);
        path = filePath;
        
        // Extract filename without extension for display
        size_t lastSlash = filePath.find_last_of("/\\");
        size_t lastDot = filePath.find_last_of('.');
        if (lastSlash == std::string::npos) lastSlash = 0;
        else lastSlash++;
        if (lastDot == std::string::npos || lastDot < lastSlash) lastDot = filePath.length();
        name = filePath.substr(lastSlash, lastDot - lastSlash);
        
        loaded = true;
        return true;
    }
    
    /**
     * Resample the IR to a target sample rate
     * @param targetRate Target sample rate in Hz
     */
    void resampleTo(float targetRate) {
        if (!loaded || samples.empty()) return;
        if (std::abs(static_cast<float>(originalSampleRate) - targetRate) < 1.f) {
            return;  // No resampling needed
        }
        
        float ratio = targetRate / static_cast<float>(originalSampleRate);
        
        // Calculate output size with some padding
        size_t outputSize = static_cast<size_t>(samples.size() * ratio) + 100;
        std::vector<float> resampled(outputSize);
        
        // Use VCV Rack's SampleRateConverter (Speex-based)
        rack::dsp::SampleRateConverter<1> src;
        src.setRates(static_cast<float>(originalSampleRate), targetRate);
        src.setQuality(8);  // High quality for IR resampling
        
        int inLen = static_cast<int>(samples.size());
        int outLen = static_cast<int>(outputSize);
        
        // Create Frame arrays explicitly for type safety
        // Frame<1> is guaranteed to be layout-compatible with a single float,
        // but we use explicit conversion to be standards-compliant
        std::vector<rack::dsp::Frame<1>> inFrames(samples.size());
        std::vector<rack::dsp::Frame<1>> outFrames(outputSize);
        
        // Copy input samples to Frame array
        for (size_t i = 0; i < samples.size(); i++) {
            inFrames[i].samples[0] = samples[i];
        }
        
        src.process(inFrames.data(), &inLen, outFrames.data(), &outLen);
        
        // Copy output back to resampled buffer
        for (int i = 0; i < outLen; i++) {
            resampled[i] = outFrames[i].samples[0];
        }
        
        // Trim to actual output length
        resampled.resize(outLen);
        
        // Trim to max 1 second at target rate
        size_t maxSamples = static_cast<size_t>(targetRate) * MAX_IR_SECONDS;
        if (resampled.size() > maxSamples) {
            resampled.resize(maxSamples);
        }
        
        samples = std::move(resampled);
        currentSampleRate = targetRate;
    }
    
    /**
     * Normalize the IR so peak absolute value is 1.0
     * Call this after resampling if normalization is desired
     */
    void normalize() {
        if (samples.empty()) return;
        
        // Find peak
        float peak = 0.f;
        for (const float& s : samples) {
            float absVal = std::abs(s);
            if (absVal > peak) {
                peak = absVal;
            }
        }
        
        peakLevel = peak;
        
        // Scale to normalize
        if (peak > 1e-6f) {
            float scale = 1.f / peak;
            for (float& s : samples) {
                s *= scale;
            }
        }
        
        normalized = true;
    }
    
    /**
     * Reset the loader to empty state
     */
    void reset() {
        samples.clear();
        path.clear();
        name.clear();
        originalSampleRate = 0;
        currentSampleRate = 0.f;
        originalChannels = 0;
        originalLength = 0;
        peakLevel = 0.f;
        loaded = false;
        normalized = false;
    }
    
    // Getters
    const std::vector<float>& getSamples() const { return samples; }
    size_t getLength() const { return samples.size(); }
    unsigned int getOriginalSampleRate() const { return originalSampleRate; }
    float getCurrentSampleRate() const { return currentSampleRate; }
    unsigned int getOriginalChannels() const { return originalChannels; }
    size_t getOriginalLength() const { return originalLength; }
    float getPeakLevel() const { return peakLevel; }
    const std::string& getPath() const { return path; }
    const std::string& getName() const { return name; }
    bool isLoaded() const { return loaded; }
    bool isNormalized() const { return normalized; }
    
private:
    std::vector<float> samples;
    std::string path;
    std::string name;
    
    unsigned int originalSampleRate = 0;
    float currentSampleRate = 0.f;
    unsigned int originalChannels = 0;
    size_t originalLength = 0;
    float peakLevel = 0.f;
    
    bool loaded = false;
    bool normalized = false;
};
