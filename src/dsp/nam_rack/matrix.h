#pragma once

/**
 * Lightweight matrix library for NAM (Neural Amp Modeler)
 *
 * Design goals:
 * - C++11 compatible
 * - Column-major storage (compatible with NAM's access patterns)
 * - No dynamic allocation during audio processing
 * - Minimal API covering only what NAM needs
 */

#include <vector>
#include <cstring>
#include <cassert>
#include <algorithm>

namespace nam {

/**
 * Simple vector class for column vectors
 */
class Vector {
public:
    Vector() : m_size(0) {}

    Vector(int size) : m_size(size), m_data(size, 0.f) {}

    void resize(int size) {
        m_size = size;
        m_data.resize(size, 0.f);
    }

    void setZero() {
        std::fill(m_data.begin(), m_data.end(), 0.f);
    }

    int size() const { return m_size; }

    float& operator()(int i) {
        assert(i >= 0 && i < m_size);
        return m_data[i];
    }

    const float& operator()(int i) const {
        assert(i >= 0 && i < m_size);
        return m_data[i];
    }

    float* data() { return m_data.data(); }
    const float* data() const { return m_data.data(); }

private:
    int m_size;
    std::vector<float> m_data;
};

/**
 * Matrix class with column-major storage
 *
 * Memory layout: [col0_row0, col0_row1, ..., col0_rowM, col1_row0, ...]
 * This matches Eigen's default and is cache-friendly for column-wise access
 */
class Matrix {
public:
    Matrix() : m_rows(0), m_cols(0) {}

    Matrix(int rows, int cols) : m_rows(rows), m_cols(cols), m_data(rows * cols, 0.f) {}

    void resize(int rows, int cols) {
        m_rows = rows;
        m_cols = cols;
        m_data.resize(rows * cols, 0.f);
    }

    void setZero() {
        std::fill(m_data.begin(), m_data.end(), 0.f);
    }

    int rows() const { return m_rows; }
    int cols() const { return m_cols; }

    // Element access (row, col)
    float& operator()(int row, int col) {
        assert(row >= 0 && row < m_rows);
        assert(col >= 0 && col < m_cols);
        return m_data[col * m_rows + row];
    }

    const float& operator()(int row, int col) const {
        assert(row >= 0 && row < m_rows);
        assert(col >= 0 && col < m_cols);
        return m_data[col * m_rows + row];
    }

    float* data() { return m_data.data(); }
    const float* data() const { return m_data.data(); }

    // Get pointer to start of a column
    float* col(int c) {
        assert(c >= 0 && c < m_cols);
        return m_data.data() + c * m_rows;
    }

    const float* col(int c) const {
        assert(c >= 0 && c < m_cols);
        return m_data.data() + c * m_rows;
    }

    /**
     * Matrix multiplication: out = a * b
     *
     * Standard matrix multiplication with optional SIMD optimization.
     * a is M x K, b is K x N, out is M x N
     */
    static void multiply(Matrix& out, const Matrix& a, const Matrix& b) {
        assert(a.cols() == b.rows());
        assert(out.rows() == a.rows());
        assert(out.cols() == b.cols());

        const int M = a.rows();
        const int K = a.cols();
        const int N = b.cols();

        // Column-major friendly traversal:
        // - out.col(j) is contiguous
        // - a.col(k) is contiguous
        // - innermost i-loop is stride-1 on both arrays
        for (int j = 0; j < N; j++) {
            float* out_col_j = out.col(j);
            std::memset(out_col_j, 0, static_cast<size_t>(M) * sizeof(float));

            for (int k = 0; k < K; k++) {
                const float b_kj = b(k, j);
                const float* a_col_k = a.col(k);

                for (int i = 0; i < M; i++) {
                    out_col_j[i] += a_col_k[i] * b_kj;
                }
            }
        }
    }

    /**
     * Matrix-vector multiplication: out = a * v
     *
     * a is M x K, v is K x 1, out is M x 1
     */
    static void multiply(Vector& out, const Matrix& a, const Vector& v) {
        assert(a.cols() == v.size());
        assert(out.size() == a.rows());

        const int M = a.rows();
        const int K = a.cols();

        for (int i = 0; i < M; i++) {
            float sum = 0.f;
            for (int k = 0; k < K; k++) {
                sum += a(i, k) * v(k);
            }
            out(i) = sum;
        }
    }

    /**
     * Add vector to each column of matrix (colwise addition)
     *
     * For each column j: matrix(:, j) += vec
     */
    static void addColwise(Matrix& matrix, const Vector& vec) {
        assert(matrix.rows() == vec.size());

        const int rows = matrix.rows();
        const int cols = matrix.cols();

        for (int j = 0; j < cols; j++) {
            for (int i = 0; i < rows; i++) {
                matrix(i, j) += vec(i);
            }
        }
    }

    /**
     * Element-wise multiplication with another matrix
     */
    static void multiplyElementwise(Matrix& out, const Matrix& a, const Matrix& b) {
        assert(a.rows() == b.rows());
        assert(a.cols() == b.cols());
        assert(out.rows() == a.rows());
        assert(out.cols() == a.cols());

        const int size = a.rows() * a.cols();
        for (int i = 0; i < size; i++) {
            out.data()[i] = a.data()[i] * b.data()[i];
        }
    }

private:
    int m_rows;
    int m_cols;
    std::vector<float> m_data;
};

/**
 * Matrix block/view class - provides a view into a submatrix without copying
 *
 * Important: Does not own data! The referenced matrix must outlive the block.
 */
class MatrixBlock {
public:
    MatrixBlock(Matrix& matrix, int startRow, int startCol, int rows, int cols)
        : m_matrix(matrix)
        , m_startRow(startRow)
        , m_startCol(startCol)
        , m_rows(rows)
        , m_cols(cols) {
        assert(startRow >= 0 && startRow + rows <= matrix.rows());
        assert(startCol >= 0 && startCol + cols <= matrix.cols());
    }

    int rows() const { return m_rows; }
    int cols() const { return m_cols; }

    float& operator()(int row, int col) {
        assert(row >= 0 && row < m_rows);
        assert(col >= 0 && col < m_cols);
        return m_matrix(m_startRow + row, m_startCol + col);
    }

    const float& operator()(int row, int col) const {
        assert(row >= 0 && row < m_rows);
        assert(col >= 0 && col < m_cols);
        return m_matrix(m_startRow + row, m_startCol + col);
    }

    // Copy from another matrix/block
    void copyFrom(const MatrixBlock& other) {
        assert(rows() == other.rows());
        assert(cols() == other.cols());
        for (int j = 0; j < m_cols; j++) {
            for (int i = 0; i < m_rows; i++) {
                (*this)(i, j) = other(i, j);
            }
        }
    }

    void copyFrom(const Matrix& other) {
        assert(rows() == other.rows());
        assert(cols() == other.cols());
        for (int j = 0; j < m_cols; j++) {
            for (int i = 0; i < m_rows; i++) {
                (*this)(i, j) = other(i, j);
            }
        }
    }

    void setZero() {
        for (int j = 0; j < m_cols; j++) {
            for (int i = 0; i < m_rows; i++) {
                (*this)(i, j) = 0.f;
            }
        }
    }

private:
    Matrix& m_matrix;
    int m_startRow;
    int m_startCol;
    int m_rows;
    int m_cols;
};

/**
 * Const version of MatrixBlock for read-only access
 */
class ConstMatrixBlock {
public:
    ConstMatrixBlock(const Matrix& matrix, int startRow, int startCol, int rows, int cols)
        : m_matrix(matrix)
        , m_startRow(startRow)
        , m_startCol(startCol)
        , m_rows(rows)
        , m_cols(cols) {
        assert(startRow >= 0 && startRow + rows <= matrix.rows());
        assert(startCol >= 0 && startCol + cols <= matrix.cols());
    }

    int rows() const { return m_rows; }
    int cols() const { return m_cols; }

    const float& operator()(int row, int col) const {
        assert(row >= 0 && row < m_rows);
        assert(col >= 0 && col < m_cols);
        return m_matrix(m_startRow + row, m_startCol + col);
    }

private:
    const Matrix& m_matrix;
    int m_startRow;
    int m_startCol;
    int m_rows;
    int m_cols;
};

/**
 * Pre-allocated memory pool for temporary matrices
 *
 * Allows allocation during model init, but reuses memory during processing
 */
class MatrixPool {
public:
    MatrixPool() : m_offset(0) {}

    void reserve(size_t numFloats) {
        m_buffer.resize(numFloats, 0.f);
    }

    // Note: returned matrices are invalidated when reset() is called
    // or when allocate() overwrites their memory
    Matrix allocate(int rows, int cols) {
        int size = rows * cols;
        assert(m_offset + size <= (int)m_buffer.size());
        Matrix m;
        // This is a hack - we need to set internal data directly
        // For now, we'll just track the offset for manual management
        m_offset += size;
        return m;
    }

    void reset() { m_offset = 0; }

    float* data() { return m_buffer.data(); }
    size_t capacity() const { return m_buffer.size(); }
    size_t used() const { return m_offset; }

private:
    std::vector<float> m_buffer;
    int m_offset;
};

} // namespace nam
