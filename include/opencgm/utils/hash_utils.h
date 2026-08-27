#ifndef OPENCGM_UTILS_HASH_UTILS_H
#define OPENCGM_UTILS_HASH_UTILS_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <optional>
#include <vector>

namespace opencgm {
namespace utils {

// Compute MD5 digest for an arbitrary byte buffer.
std::string computeMd5Hex(const uint8_t* data, std::size_t length);

// Convenience overload for typed containers.
template <typename Container>
std::string computeMd5Hex(const Container& buffer) {
    if (buffer.empty()) {
        return {};
    }
    return computeMd5Hex(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size());
}

// Compute MD5 hash for a file on disk. Returns empty string when the file
// cannot be read.
std::string computeFileMd5Hex(const std::string& path);

} // namespace utils
} // namespace opencgm

#endif // OPENCGM_UTILS_HASH_UTILS_H
