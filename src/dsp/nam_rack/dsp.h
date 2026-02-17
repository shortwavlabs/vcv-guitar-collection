#pragma once

/**
 * Base DSP class for NAM models
 *
 * Features:
 * - C++11 compatible
 * - No Eigen dependency
 * - Common interface for all model architectures
 * - Sample rate handling
 * - Loudness/level metadata
 */

#include <memory>
#include <string>
#include <vector>

// Sample type for NAM core processing.
// Must be consistent across ALL translation units to avoid ABI/type mismatch.
// The Rack plugin uses float buffers end-to-end, so fix this to float.
#define NAM_SAMPLE float

#define NAM_UNKNOWN_EXPECTED_SAMPLE_RATE -1.0

namespace nam {

/**
 * Base class for all NAM DSP models
 */
class DSP {
public:
    DSP(double expected_sample_rate = -1.0);
    virtual ~DSP() = default;

    /**
     * Prewarm the model - settle initial conditions
     * Called once during initialization, not during real-time processing
     */
    virtual void prewarm();

    /**
     * Process audio samples
     *
     * @param input Input samples
     * @param output Output samples (can be same as input)
     * @param num_frames Number of sample frames to process
     */
    virtual void process(NAM_SAMPLE* input, NAM_SAMPLE* output, int num_frames) = 0;

    /**
     * Reset the DSP state
     *
     * @param sampleRate External sample rate
     * @param maxBufferSize Maximum buffer size expected
     */
    virtual void reset(double sampleRate, int maxBufferSize);

    /**
     * Reset and prewarm in one call
     */
    void resetAndPrewarm(double sampleRate, int maxBufferSize) {
        reset(sampleRate, maxBufferSize);
        prewarm();
    }

    // Getters
    double getExpectedSampleRate() const { return mExpectedSampleRate; }
    bool hasLoudness() const { return mHasLoudness; }
    double getLoudness() const;
    bool hasInputLevel() const { return mInputLevel.haveLevel; }
    double getInputLevel() const { return mInputLevel.level; }
    bool hasOutputLevel() const { return mOutputLevel.haveLevel; }
    double getOutputLevel() const { return mOutputLevel.level; }

    // Setters
    void setInputLevel(double level) {
        mInputLevel.haveLevel = true;
        mInputLevel.level = level;
    }
    void setLoudness(double loudness);
    void setOutputLevel(double level) {
        mOutputLevel.haveLevel = true;
        mOutputLevel.level = level;
    }

protected:
    /**
     * How many samples needed for prewarm?
     */
    virtual int prewarmSamples() { return 0; }

    /**
     * Called by reset() to update max buffer size
     */
    virtual void setMaxBufferSize(int maxBufferSize);
    int getMaxBufferSize() const { return mMaxBufferSize; }

    // Model parameters
    double mExpectedSampleRate;
    bool mHasLoudness = false;
    double mLoudness = 0.0;

    // Runtime state
    bool mHaveExternalSampleRate = false;
    double mExternalSampleRate = -1.0;
    int mMaxBufferSize = 0;

private:
    struct Level {
        bool haveLevel = false;
        float level = 0.0f;
    };
    Level mInputLevel;
    Level mOutputLevel;
};

/**
 * Buffer-based DSP - keeps input buffer for temporal effects
 *
 * Used by Linear and ConvNet architectures that need history
 */
class Buffer : public DSP {
public:
    Buffer(int receptive_field, double expected_sample_rate = -1.0);

protected:
    // Input/output buffers
    static const int INPUT_BUFFER_CHANNELS = 1;  // Mono
    int mReceptiveField;
    long mInputBufferOffset;
    std::vector<float> mInputBuffer;
    std::vector<float> mOutputBuffer;

    void advanceInputBuffer(int num_frames);
    void setReceptiveField(int new_receptive_field, int input_buffer_size);
    void setReceptiveField(int new_receptive_field);
    void resetInputBuffer();
    virtual void updateBuffers(NAM_SAMPLE* input, int num_frames);
    virtual void rewindBuffers();
};

} // namespace nam
