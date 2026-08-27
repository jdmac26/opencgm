#ifndef OPENCGM_GZIP_UTILS_H
#define OPENCGM_GZIP_UTILS_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace opencgm
{
bool isGzipStream(const uint8_t *data, size_t size);
bool gzipDecompress(const uint8_t *data, size_t size, std::vector<uint8_t> &out);

/**
 * @brief Compress data using gzip format (for SVGZ output)
 * @param data Input data to compress
 * @param size Size of input data
 * @param out Output vector for compressed data
 * @param level Compression level (1-9, default 6; higher = better compression, slower)
 * @return true if compression succeeded
 */
bool gzipCompress(const uint8_t *data, size_t size, std::vector<uint8_t> &out, int level = 6);

/**
 * @brief Convenience overload for string input
 */
bool gzipCompress(const std::string &input, std::vector<uint8_t> &out, int level = 6);
}

#endif // OPENCGM_GZIP_UTILS_H
