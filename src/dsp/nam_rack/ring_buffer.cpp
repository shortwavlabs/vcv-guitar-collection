#include "ring_buffer.h"

namespace nam {

void RingBuffer::reset(int channels, int max_buffer_size, long max_lookback) {
    // Store parameters
    m_maxBufferSize = max_buffer_size;
    m_maxLookback = max_lookback;

    // Calculate storage size: 2 * max_lookback + max_buffer_size
    // This ensures we have enough room for:
    // - max_lookback at the start (for history after rewind)
    // - max_buffer_size in the middle (for writes/reads)
    // - no aliasing when rewinding
    const long storage_size = 2 * m_maxLookback + max_buffer_size;
    m_storage.resize(channels, storage_size);
    m_storage.setZero();

    // Initialize write position to max_lookback to leave room for history
    m_writePos = m_maxLookback;
}

void RingBuffer::write(const float* input, int num_frames, bool planar) {
    // Assert that num_frames doesn't exceed max buffer size
    assert(num_frames <= m_maxBufferSize && "Write: num_frames must not exceed max_buffer_size");

    // Check if we need to rewind
    if (needsRewind(num_frames)) {
        rewind();
    }

    const int channels = m_storage.rows();

    if (planar) {
        // Input is planar: [ch0_samples..., ch1_samples..., ...]
        for (int c = 0; c < channels; c++) {
            for (int f = 0; f < num_frames; f++) {
                m_storage(c, m_writePos + f) = input[c * num_frames + f];
            }
        }
    } else {
        // Input is interleaved: [ch0_f0, ch1_f0, ch2_f0, ch0_f1, ...]
        for (int f = 0; f < num_frames; f++) {
            for (int c = 0; c < channels; c++) {
                m_storage(c, m_writePos + f) = input[f * channels + c];
            }
        }
    }
}

void RingBuffer::write(const Matrix& input, int num_frames) {
    // Assert that num_frames doesn't exceed max buffer size
    assert(num_frames <= m_maxBufferSize && "Write: num_frames must not exceed max_buffer_size");
    assert(input.rows() == m_storage.rows() && "Write: input channels must match storage channels");

    // Check if we need to rewind
    if (needsRewind(num_frames)) {
        rewind();
    }

    // Copy from input matrix (channels x num_frames)
    // Columns are contiguous in memory for both matrices (column-major), so memcpy per column.
    const size_t colBytes = static_cast<size_t>(m_storage.rows()) * sizeof(float);
    for (int f = 0; f < num_frames; f++) {
        std::memcpy(m_storage.col(static_cast<int>(m_writePos + f)), input.col(f), colBytes);
    }
}

void RingBuffer::read(float* output, int num_frames, long lookback) const {
    // Assert that lookback doesn't exceed max_lookback
    assert(lookback <= m_maxLookback && "Read: lookback must not exceed max_lookback");

    // Assert that num_frames doesn't exceed max buffer size
    assert(num_frames <= m_maxBufferSize && "Read: num_frames must not exceed max_buffer_size");

    const long read_pos = m_writePos - lookback;
    assert(read_pos >= 0 && "Read: read position must be non-negative");

    const int channels = m_storage.rows();

    // Output in interleaved format
    for (int f = 0; f < num_frames; f++) {
        for (int c = 0; c < channels; c++) {
            output[f * channels + c] = m_storage(c, read_pos + f);
        }
    }
}

void RingBuffer::read(Matrix& output, int num_frames, long lookback) const {
    // Assert that lookback doesn't exceed max_lookback
    assert(lookback <= m_maxLookback && "Read: lookback must not exceed max_lookback");

    // Assert that num_frames doesn't exceed max buffer size
    assert(num_frames <= m_maxBufferSize && "Read: num_frames must not exceed max_buffer_size");

    const long read_pos = m_writePos - lookback;
    assert(read_pos >= 0 && "Read: read position must be non-negative");

    // Ensure output matrix is properly sized
    assert(output.rows() == m_storage.rows() && "Read: output channels must match storage");
    assert(output.cols() >= num_frames && "Read: output must have enough columns");

    // Copy to output matrix (channels x num_frames)
    // Columns are contiguous in memory for both matrices (column-major), so memcpy per column.
    const size_t colBytes = static_cast<size_t>(m_storage.rows()) * sizeof(float);
    for (int f = 0; f < num_frames; f++) {
        std::memcpy(output.col(f), m_storage.col(static_cast<int>(read_pos + f)), colBytes);
    }
}

void RingBuffer::advance(int num_frames) {
    m_writePos += num_frames;
}

bool RingBuffer::needsRewind(int num_frames) const {
    return m_writePos + num_frames > (long)m_storage.cols();
}

void RingBuffer::rewind() {
    if (m_maxLookback == 0) {
        // No history to preserve, just reset
        m_writePos = 0;
        return;
    }

    // Assert that write pointer is far enough along to avoid aliasing when copying
    // We copy from position (m_writePos - m_maxLookback) to position 0
    // For no aliasing, we need: m_writePos - m_maxLookback >= m_maxLookback
    // Which simplifies to: m_writePos >= 2 * m_maxLookback
    assert(m_writePos >= 2 * m_maxLookback
           && "Write pointer must be at least 2 * max_lookback to avoid aliasing during rewind");

    // Copy the max lookback amount of history back to the beginning
    // This is the history that will be needed for lookback reads
    const long copy_start = m_writePos - m_maxLookback;
    assert(copy_start >= 0 && copy_start < (long)m_storage.cols()
           && "Copy start position must be within storage bounds");

    // Copy m_maxLookback samples from before the write position to the start
    // We do this column by column to respect the column-major layout
    for (long col = 0; col < m_maxLookback; col++) {
        for (int row = 0; row < m_storage.rows(); row++) {
            m_storage(row, col) = m_storage(row, copy_start + col);
        }
    }

    // Reset write position to just after the copied history
    m_writePos = m_maxLookback;
}

} // namespace nam
