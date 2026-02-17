#include "dsp.h"
#include <stdexcept>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace nam {

// ============================================================================
// DSP base class
// ============================================================================

DSP::DSP(double expected_sample_rate)
    : mExpectedSampleRate(expected_sample_rate) {
}

void DSP::prewarm() {
    // If max buffer size not set, use default
    if (mMaxBufferSize == 0) {
        setMaxBufferSize(4096);
    }

    const int samples = prewarmSamples();
    if (samples == 0) {
        return;
    }

    // Process silence to warm up the model
    const size_t bufferSize = static_cast<size_t>(std::max(mMaxBufferSize, 1));
    std::vector<NAM_SAMPLE> inputBuffer(bufferSize, NAM_SAMPLE(0));
    std::vector<NAM_SAMPLE> outputBuffer(bufferSize, NAM_SAMPLE(0));

    int processed = 0;
    while (processed < samples) {
        process(inputBuffer.data(), outputBuffer.data(), static_cast<int>(bufferSize));
        processed += static_cast<int>(bufferSize);
    }
}

void DSP::reset(double sampleRate, int maxBufferSize) {
    mHaveExternalSampleRate = true;
    mExternalSampleRate = sampleRate;
    setMaxBufferSize(maxBufferSize);
    prewarm();
}

void DSP::setMaxBufferSize(int maxBufferSize) {
    mMaxBufferSize = maxBufferSize;
}

void DSP::setLoudness(double loudness) {
    mHasLoudness = true;
    mLoudness = loudness;
}

double DSP::getLoudness() const {
    if (!mHasLoudness) {
        throw std::runtime_error("Model does not know its loudness");
    }
    return mLoudness;
}

// ============================================================================
// Buffer class
// ============================================================================

Buffer::Buffer(int receptive_field, double expected_sample_rate)
    : DSP(expected_sample_rate)
    , mReceptiveField(receptive_field)
    , mInputBufferOffset(0) {
    setReceptiveField(receptive_field);
}

void Buffer::setReceptiveField(int new_receptive_field) {
    // Default buffer size: 32x receptive field for safety (matches original NAM)
    setReceptiveField(new_receptive_field,
                      std::max(new_receptive_field * 32, 4096));
}

void Buffer::setReceptiveField(int new_receptive_field, int input_buffer_size) {
    mReceptiveField = new_receptive_field;
    mInputBuffer.resize(INPUT_BUFFER_CHANNELS * input_buffer_size, 0.0f);
    mOutputBuffer.resize(input_buffer_size, 0.0f);
    mInputBufferOffset = mReceptiveField;
}

void Buffer::resetInputBuffer() {
    std::fill(mInputBuffer.begin(), mInputBuffer.end(), 0.0f);
    std::fill(mOutputBuffer.begin(), mOutputBuffer.end(), 0.0f);
    mInputBufferOffset = mReceptiveField;
}

void Buffer::advanceInputBuffer(int num_frames) {
    mInputBufferOffset += num_frames;
    // Rewind if needed
    const long input_buffer_size = static_cast<long>(mInputBuffer.size() / INPUT_BUFFER_CHANNELS);
    if (mInputBufferOffset + num_frames > input_buffer_size) {
        rewindBuffers();
    }
}

void Buffer::updateBuffers(NAM_SAMPLE* input, int num_frames) {
    const long minimum_input_buffer_size = static_cast<long>(mReceptiveField) +
        static_cast<long>(32) * static_cast<long>(num_frames);

    const long current_input_buffer_size =
        static_cast<long>(mInputBuffer.size() / INPUT_BUFFER_CHANNELS);

    if (current_input_buffer_size < minimum_input_buffer_size) {
        long new_buffer_size = 2;
        while (new_buffer_size < minimum_input_buffer_size) {
            new_buffer_size *= 2;
        }
        mInputBuffer.resize(static_cast<size_t>(INPUT_BUFFER_CHANNELS * new_buffer_size), 0.0f);
    }

    const long input_buffer_size = static_cast<long>(mInputBuffer.size() / INPUT_BUFFER_CHANNELS);
    if (mInputBufferOffset + num_frames > input_buffer_size) {
        rewindBuffers();
    }

    // Copy input to buffer at current offset
    for (int i = 0; i < num_frames; i++) {
        mInputBuffer[mInputBufferOffset + i] = static_cast<float>(input[i]);
    }

    mOutputBuffer.resize(static_cast<size_t>(num_frames));
    std::fill(mOutputBuffer.begin(), mOutputBuffer.end(), 0.0f);
}

void Buffer::rewindBuffers() {
    // Copy the last receptive_field samples to the beginning
    // This preserves the history needed for convolution
    if (mInputBufferOffset >= mReceptiveField) {
        const long copy_start = mInputBufferOffset - mReceptiveField;
        std::memmove(mInputBuffer.data(),
                     mInputBuffer.data() + copy_start,
                     mReceptiveField * sizeof(float));
    }
    mInputBufferOffset = mReceptiveField;
}

} // namespace nam
