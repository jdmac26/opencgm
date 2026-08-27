#ifndef OPENCGM_SECURITY_LIMITS_H
#define OPENCGM_SECURITY_LIMITS_H

/**
 * @file security_limits.h
 * @brief Security-related size limits to prevent DoS attacks from malicious CGM files
 *
 * These limits protect against:
 * - Memory exhaustion attacks (allocating excessive memory)
 * - Integer overflow attacks (crafted dimension values)
 * - Zip bomb attacks (compressed data that expands massively)
 *
 * All limits are defined here for easy auditing and adjustment.
 */

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace opencgm {
namespace security {

/**
 * Maximum dimension for CellArray (nx, ny values)
 * 16384 x 16384 = 256M pixels max, reasonable for any CGM use case
 */
constexpr int MAX_CELL_ARRAY_DIMENSION = 16384;

/**
 * Maximum total cells in a CellArray (prevents nx*ny overflow)
 * 256M cells * 4 bytes (RGBA) = 1GB max memory
 */
constexpr size_t MAX_CELL_ARRAY_TOTAL_CELLS = 256 * 1024 * 1024;

/**
 * Maximum number of control points in B-Spline curves
 * 100,000 points * 16 bytes = 1.6MB, reasonable for complex curves
 */
constexpr size_t MAX_SPLINE_CONTROL_POINTS = 100000;

/**
 * Maximum number of knots in B-Spline curves
 * Typically knots = control_points + order, so 100,000 is generous
 */
constexpr size_t MAX_SPLINE_KNOTS = 100000;

/**
 * Maximum number of points in a polygon/polyline
 */
constexpr size_t MAX_POLYGON_POINTS = 1000000;

/**
 * Maximum decompressed size for gzip data (100 MB)
 * Prevents zip bomb attacks
 */
constexpr size_t MAX_DECOMPRESSED_SIZE = 100 * 1024 * 1024;

/**
 * Maximum string length in CGM files (10 MB)
 */
constexpr size_t MAX_STRING_LENGTH = 10 * 1024 * 1024;

/**
 * Maximum command argument buffer size (50 MB)
 */
constexpr size_t MAX_COMMAND_ARGUMENTS_SIZE = 50 * 1024 * 1024;

/**
 * Maximum tokens per statement in clear text CGM (prevents unbounded accumulation)
 */
constexpr size_t MAX_TOKENS_PER_STATEMENT = 1000000;

} // namespace security

/**
 * Render output dimension limits for native (Skia) rendering.
 * These prevent excessive memory allocation for rasterized output.
 */
namespace render_limits {

/**
 * Default output dimension when no target size is specified.
 * Used for the larger dimension; aspect ratio determines the other.
 */
constexpr int DEFAULT_OUTPUT_DIMENSION = 1024;

/**
 * Minimum output dimension to ensure usable output.
 * Prevents degenerate cases that could cause rendering issues.
 */
constexpr int MIN_OUTPUT_DIMENSION = 64;

/**
 * Maximum output dimension to prevent excessive memory usage.
 * 8192 x 8192 x 4 bytes = 256 MB maximum buffer size.
 */
constexpr int MAX_OUTPUT_DIMENSION = 8192;

} // namespace render_limits

namespace security {

/**
 * @brief Validates array/vector allocation size before allocation
 * @param count Number of elements to allocate
 * @param maxCount Maximum allowed elements
 * @param elementName Name for error message
 * @throws std::runtime_error if count exceeds limit
 */
inline void validateAllocationSize(size_t count, size_t maxCount, const char* elementName) {
    if (count > maxCount) {
        throw std::runtime_error(
            std::string("CGM security limit exceeded: ") + elementName +
            " count " + std::to_string(count) +
            " exceeds maximum " + std::to_string(maxCount)
        );
    }
}

/**
 * @brief Validates CellArray dimensions before allocation
 * @param nx Width dimension
 * @param ny Height dimension
 * @throws std::runtime_error if dimensions are invalid or exceed limits
 */
inline void validateCellArrayDimensions(int nx, int ny) {
    if (nx <= 0 || ny <= 0) {
        throw std::runtime_error("Invalid CellArray dimensions: must be positive");
    }
    if (nx > MAX_CELL_ARRAY_DIMENSION || ny > MAX_CELL_ARRAY_DIMENSION) {
        throw std::runtime_error(
            "CellArray dimension exceeds security limit: " +
            std::to_string(nx) + "x" + std::to_string(ny) +
            " (max " + std::to_string(MAX_CELL_ARRAY_DIMENSION) + ")"
        );
    }
    // Check for overflow in total cell calculation
    size_t totalCells = static_cast<size_t>(nx) * static_cast<size_t>(ny);
    if (totalCells > MAX_CELL_ARRAY_TOTAL_CELLS) {
        throw std::runtime_error(
            "CellArray total cells exceeds security limit: " +
            std::to_string(totalCells) +
            " (max " + std::to_string(MAX_CELL_ARRAY_TOTAL_CELLS) + ")"
        );
    }
}

/**
 * @brief Validates decompressed size before allocation
 * @param size Decompressed size in bytes
 * @throws std::runtime_error if size exceeds limit
 */
inline void validateDecompressedSize(size_t size) {
    if (size > MAX_DECOMPRESSED_SIZE) {
        throw std::runtime_error(
            "Decompressed size exceeds security limit: " +
            std::to_string(size) + " bytes (max " +
            std::to_string(MAX_DECOMPRESSED_SIZE) + ")"
        );
    }
}

} // namespace security
} // namespace opencgm

#endif // OPENCGM_SECURITY_LIMITS_H
