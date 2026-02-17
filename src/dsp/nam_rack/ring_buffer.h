#pragma once

/**
 * Ring buffer for dilated convolution temporal memory
 *
 * This ring buffer manages multi-channel audio data with support for:
 * - Write operations at current position
 * - Read operations with configurable lookback (needed for dilated convolutions)
 * - Automatic rewind when buffer approaches end
 *
 * C++11 compatible, no Eigen dependency.
 */

#include <vector>
#include <cassert>
#include <cstring>
#include "matrix.h"

namespace nam {

/**
 * Ring buffer for managing multi-channel audio data with write/read pointers
 *
 * Storage layout: channels x time (rows = channels, cols = time)
 * This matches NAM's usage pattern where we process frames column by column
 */
class RingBuffer {
public:
    RingBuffer() : m_writePos(0), m_maxLookback(0), m_maxBufferSize(0) {}

    /**
     * Initialize/resize storage
     *
     * @param channels Number of channels (rows in the storage matrix)
     * @param max_buffer_size Maximum amount that will be written or read at once
     * @param max_lookback Maximum history needed when rewinding (default 0)
     */
    void reset(int channels, int max_buffer_size, long max_lookback = 0);

    /**
     * Write new data at write pointer
     *
     * @param input Input data array (channels interleaved or planar)
     * @param num_frames Number of frames to write
     * @param planar If true, input is in planar format (channel-major)
     */
    void write(const float* input, int num_frames, bool planar = false);

    /**
     * Write from a matrix (channels x num_frames)
     *
     * @param input Input matrix
     * @param num_frames Number of frames to write
     */
    void write(const Matrix& input, int num_frames);

    /**
     * Read data with optional lookback
     *
     * @param output Output buffer (channels x num_frames)
     * @param num_frames Number of frames to read
     * @param lookback Number of frames to look back from write pointer (default 0)
     */
    void read(float* output, int num_frames, long lookback = 0) const;

    /**
     * Read into a matrix with optional lookback
     *
     * @param output Output matrix
     * @param num_frames Number of frames to read
     * @param lookback Number of frames to look back (default 0)
     */
    void read(Matrix& output, int num_frames, long lookback = 0) const;

    /**
     * Advance write pointer
     *
     * @param num_frames Number of frames to advance
     */
    void advance(int num_frames);

    /**
     * Get max buffer size (the value passed to reset())
     */
    int getMaxBufferSize() const { return m_maxBufferSize; }

    /**
     * Get number of channels (rows)
     */
    int getChannels() const { return m_storage.rows(); }

    /**
     * Get total storage capacity in frames (columns)
     */
    int getCapacity() const { return m_storage.cols(); }

    /**
     * Set the max lookback (maximum history needed when rewinding)
     */
    void setMaxLookback(long max_lookback) { m_maxLookback = max_lookback; }
    long getMaxLookback() const { return m_maxLookback; }

    /**
     * Get current write position
     */
    long getWritePos() const { return m_writePos; }

    /**
     * Direct access to internal storage (for advanced use)
     * Returns pointer to column at given position
     */
    float* getCol(int col) { return m_storage.col(col); }
    const float* getCol(int col) const { return m_storage.col(col); }

private:
    /**
     * Wrap buffer when approaching end (called automatically if needed)
     */
    void rewind();

    /**
     * Check if rewind is needed before num_frames are written or read
     *
     * @param num_frames Number of frames that will be written
     * @return true if rewind is needed
     */
    bool needsRewind(int num_frames) const;

    Matrix m_storage;        // channels x storage_size
    long m_writePos;         // Current write position
    long m_maxLookback;      // Maximum lookback needed when rewinding
    int m_maxBufferSize;     // Maximum buffer size passed to reset()
};

} // namespace nam
