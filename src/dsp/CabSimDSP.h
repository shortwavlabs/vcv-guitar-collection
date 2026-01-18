#pragma once

// Define M_PI for Windows before including cmath
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include "IRLoader.h"
#include <dsp/fir.hpp>
#include <dsp/filter.hpp>
#include <memory>
#include <vector>
#include <mutex>
#include <cmath>
#include <algorithm>

/**
 * CabSimDSP - Cabinet Simulator DSP engine
 * 
 * Features:
 * - Dual IR convolution with blending (or wet/dry when single IR)
 * - Block-based processing using VCV Rack's RealTimeConvolver
 * - 2-pole (12 dB/oct) high-pass and low-pass filters
 * - Thread-safe IR loading support
 */
class CabSimDSP {
public:
    static constexpr int MAX_IR_LENGTH = 48000;  // 1 second at 48kHz
    static constexpr int BLOCK_SIZE = 256;       // Convolution block size
    
    CabSimDSP() {
        // Pre-allocate block buffers
        inputBuffer.resize(BLOCK_SIZE, 0.f);
        outputBuffer.resize(BLOCK_SIZE, 0.f);
        tempBufferA.resize(BLOCK_SIZE, 0.f);
        tempBufferB.resize(BLOCK_SIZE, 0.f);
        
        // Initialize filters with default values
        forceUpdateFilters();
    }
    
    ~CabSimDSP() = default;
    
    /**
     * Set the engine sample rate
     * @param rate Sample rate in Hz
     */
    void setSampleRate(float rate) {
        if (std::abs(sampleRate - rate) < 1.f) return;
        sampleRate = rate;
        forceUpdateFilters();
    }
    
    /**
     * Load an IR into the specified slot
     * @param slot 0 for IR A, 1 for IR B
     * @param path File path to the IR WAV file
     * @return true if successful
     * 
     * Note: This method is NOT thread-safe. Use with external locking
     * or call from a background thread with proper synchronization.
     */
    bool loadIR(int slot, const std::string& path) {
        if (slot < 0 || slot > 1) return false;
        
        IRLoader loader;
        if (!loader.load(path)) {
            return false;
        }
        
        // Resample to current engine sample rate
        loader.resampleTo(sampleRate);
        
        // Apply normalization if enabled for this slot
        if (normalizeEnabled[slot]) {
            loader.normalize();
        }
        
        // Store IR metadata
        irData[slot] = std::move(loader);
        
        // Create new convolver with the IR
        auto newConvolver = std::make_unique<rack::dsp::RealTimeConvolver>(BLOCK_SIZE);
        const auto& samples = irData[slot].getSamples();
        if (!samples.empty()) {
            newConvolver->setKernel(samples.data(), samples.size());
        }
        
        convolvers[slot] = std::move(newConvolver);
        return true;
    }
    
    /**
     * Set the IR kernel directly (for thread-safe loading from module)
     * @param slot 0 for IR A, 1 for IR B
     * @param samples IR samples (already resampled and optionally normalized)
     * @param irPath Original file path for display
     * @param irName Display name
     */
    void setIRKernel(int slot, const std::vector<float>& samples, 
                     const std::string& irPath, const std::string& irName) {
        if (slot < 0 || slot > 1) return;
        if (samples.empty()) return;
        
        auto newConvolver = std::make_unique<rack::dsp::RealTimeConvolver>(BLOCK_SIZE);
        newConvolver->setKernel(samples.data(), samples.size());
        
        convolvers[slot] = std::move(newConvolver);
        irPaths[slot] = irPath;
        irNames[slot] = irName;
    }
    
    /**
     * Unload an IR from a slot
     * @param slot 0 for IR A, 1 for IR B
     */
    void unloadIR(int slot) {
        if (slot < 0 || slot > 1) return;
        convolvers[slot].reset();
        irData[slot].reset();
        irPaths[slot].clear();
        irNames[slot].clear();
    }
    
    /**
     * Check if an IR is loaded in a slot
     */
    bool isIRLoaded(int slot) const {
        if (slot < 0 || slot > 1) return false;
        return convolvers[slot] != nullptr;
    }
    
    /**
     * Get the display name of a loaded IR
     */
    std::string getIRName(int slot) const {
        if (slot < 0 || slot > 1) return "";
        if (!irNames[slot].empty()) return irNames[slot];
        return irData[slot].getName();
    }
    
    /**
     * Get the file path of a loaded IR
     */
    std::string getIRPath(int slot) const {
        if (slot < 0 || slot > 1) return "";
        if (!irPaths[slot].empty()) return irPaths[slot];
        return irData[slot].getPath();
    }
    
    /**
     * Set normalization enable for a slot
     */
    void setNormalize(int slot, bool enabled) {
        if (slot < 0 || slot > 1) return;
        normalizeEnabled[slot] = enabled;
    }
    
    /**
     * Get normalization enable for a slot
     */
    bool getNormalize(int slot) const {
        if (slot < 0 || slot > 1) return false;
        return normalizeEnabled[slot];
    }
    
    /**
     * Process a single sample through the cabinet simulator
     * 
     * @param input Input sample
     * @param blend Blend amount (0-1). With both IRs: crossfade A/B. 
     *              With single IR: wet/dry mix.
     * @param lowpassFreq Low-pass filter cutoff frequency (Hz)
     * @param highpassFreq High-pass filter cutoff frequency (Hz)
     * @return Processed output sample
     */
    float process(float input, float blend, float lowpassFreq, float highpassFreq) {
        // Update filter coefficients if changed
        updateFilters(lowpassFreq, highpassFreq);
        
        // Accumulate into input buffer
        inputBuffer[bufferPos] = input;
        
        // Get output from previous block
        float output = outputBuffer[bufferPos];
        
        bufferPos++;
        
        // Process when block is full
        if (bufferPos >= BLOCK_SIZE) {
            processBlock(blend);
            bufferPos = 0;
        }
        
        // Apply filters to output
        output = hpf.process(output);
        output = lpf.process(output);
        
        return output;
    }
    
    /**
     * Reset all DSP state
     */
    void reset() {
        std::fill(inputBuffer.begin(), inputBuffer.end(), 0.f);
        std::fill(outputBuffer.begin(), outputBuffer.end(), 0.f);
        std::fill(tempBufferA.begin(), tempBufferA.end(), 0.f);
        std::fill(tempBufferB.begin(), tempBufferB.end(), 0.f);
        bufferPos = 0;
        lpf.reset();
        hpf.reset();
        // Re-initialize filter coefficients after reset
        forceUpdateFilters();
    }
    
private:
    // Sample rate
    float sampleRate = 48000.f;
    
    // Dual convolvers
    std::unique_ptr<rack::dsp::RealTimeConvolver> convolvers[2];
    
    // IR data and metadata
    IRLoader irData[2];
    std::string irPaths[2];
    std::string irNames[2];
    bool normalizeEnabled[2] = {false, false};
    
    // Block buffers
    std::vector<float> inputBuffer;
    std::vector<float> outputBuffer;
    std::vector<float> tempBufferA;
    std::vector<float> tempBufferB;
    int bufferPos = 0;
    
    // Output filters (2-pole, 12 dB/oct)
    rack::dsp::TBiquadFilter<float> lpf;
    rack::dsp::TBiquadFilter<float> hpf;
    float lastLpFreq = 20000.f;
    float lastHpFreq = 20.f;
    
    /**
     * Process a full block through the convolvers
     */
    void processBlock(float blend) {
        bool hasA = convolvers[0] != nullptr;
        bool hasB = convolvers[1] != nullptr;
        
        // Process through convolvers
        if (hasA) {
            convolvers[0]->processBlock(inputBuffer.data(), tempBufferA.data());
        }
        if (hasB) {
            convolvers[1]->processBlock(inputBuffer.data(), tempBufferB.data());
        }
        
        // Blend results
        for (int i = 0; i < BLOCK_SIZE; i++) {
            if (hasA && hasB) {
                // Both IRs: crossfade between A and B
                outputBuffer[i] = tempBufferA[i] * (1.f - blend) + tempBufferB[i] * blend;
            } else if (hasA) {
                // Only IR A: blend is wet/dry mix
                float wet = tempBufferA[i];
                outputBuffer[i] = inputBuffer[i] * (1.f - blend) + wet * blend;
            } else if (hasB) {
                // Only IR B: blend is wet/dry mix
                float wet = tempBufferB[i];
                outputBuffer[i] = inputBuffer[i] * (1.f - blend) + wet * blend;
            } else {
                // No IRs: passthrough
                outputBuffer[i] = inputBuffer[i];
            }
        }
    }
    
    /**
     * Update filter coefficients
     */
    void updateFilters(float lpFreq, float hpFreq) {
        // Only update if frequencies changed significantly
        if (std::abs(lpFreq - lastLpFreq) > 1.f) {
            float lpNorm = std::clamp(lpFreq / sampleRate, 0.001f, 0.499f);
            lpf.setParameters(rack::dsp::TBiquadFilter<float>::LOWPASS, lpNorm, 0.707f, 1.f);
            lastLpFreq = lpFreq;
        }
        
        if (std::abs(hpFreq - lastHpFreq) > 1.f) {
            float hpNorm = std::clamp(hpFreq / sampleRate, 0.001f, 0.499f);
            hpf.setParameters(rack::dsp::TBiquadFilter<float>::HIGHPASS, hpNorm, 0.707f, 1.f);
            lastHpFreq = hpFreq;
        }
    }
    
    /**
     * Force update filters (for sample rate change)
     */
    void forceUpdateFilters() {
        float lpNorm = std::clamp(lastLpFreq / sampleRate, 0.001f, 0.499f);
        lpf.setParameters(rack::dsp::TBiquadFilter<float>::LOWPASS, lpNorm, 0.707f, 1.f);
        
        float hpNorm = std::clamp(lastHpFreq / sampleRate, 0.001f, 0.499f);
        hpf.setParameters(rack::dsp::TBiquadFilter<float>::HIGHPASS, hpNorm, 0.707f, 1.f);
    }
};
