#include "opencgm/utils/hash_utils.h"

#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cstring>

namespace opencgm {
namespace utils {
namespace {

constexpr uint32_t kSineTable[64] = {
    0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu,
    0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
    0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu,
    0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
    0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau,
    0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
    0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu,
    0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
    0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu,
    0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
    0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u,
    0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
    0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u,
    0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
    0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
    0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u};

constexpr uint32_t kShiftAmounts[64] = {
    7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
    5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
    4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
    6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21};

inline uint32_t leftRotate(uint32_t value, uint32_t amount) {
    return (value << amount) | (value >> (32 - amount));
}

std::array<uint8_t, 16> computeMd5Buffer(const uint8_t* data, std::size_t length) {
    uint64_t bitLen = static_cast<uint64_t>(length) * 8;

    // Prepare message with padding and length (little endian).
    std::size_t paddedLength = length + 1;
    while ((paddedLength % 64) != 56) {
        ++paddedLength;
    }
    std::vector<uint8_t> buffer(paddedLength + 8, 0);
    std::copy(data, data + length, buffer.begin());
    buffer[length] = 0x80;

    for (int i = 0; i < 8; ++i) {
        buffer[paddedLength + i] = static_cast<uint8_t>((bitLen >> (8 * i)) & 0xFF);
    }

    uint32_t a0 = 0x67452301u;
    uint32_t b0 = 0xEFCDAB89u;
    uint32_t c0 = 0x98BADCFEu;
    uint32_t d0 = 0x10325476u;

    const std::size_t chunkCount = buffer.size() / 64;
    for (std::size_t chunkIdx = 0; chunkIdx < chunkCount; ++chunkIdx) {
        const uint8_t* chunk = buffer.data() + chunkIdx * 64;
        uint32_t M[16];
        for (int i = 0; i < 16; ++i) {
            uint32_t word = 0;
            std::memcpy(&word, chunk + i * 4, sizeof(uint32_t));
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            M[i] = word;
#else
            M[i] = (word & 0x000000FFu) << 24 |
                   (word & 0x0000FF00u) << 8  |
                   (word & 0x00FF0000u) >> 8  |
                   (word & 0xFF000000u) >> 24;
#endif
        }

        uint32_t A = a0;
        uint32_t B = b0;
        uint32_t C = c0;
        uint32_t D = d0;

        for (uint32_t i = 0; i < 64; ++i) {
            uint32_t F = 0;
            uint32_t g = 0;

            if (i < 16) {
                F = (B & C) | (~B & D);
                g = i;
            } else if (i < 32) {
                F = (D & B) | (~D & C);
                g = (5 * i + 1) & 0x0F;
            } else if (i < 48) {
                F = B ^ C ^ D;
                g = (3 * i + 5) & 0x0F;
            } else {
                F = C ^ (B | ~D);
                g = (7 * i) & 0x0F;
            }

            uint32_t temp = D;
            D = C;
            C = B;
            uint32_t rotateInput = A + F + kSineTable[i] + M[g];
            B = B + leftRotate(rotateInput, kShiftAmounts[i]);
            A = temp;
        }

        a0 += A;
        b0 += B;
        c0 += C;
        d0 += D;
    }

    std::array<uint8_t, 16> digest{};
    uint32_t words[4] = {a0, b0, c0, d0};
    for (int i = 0; i < 4; ++i) {
        digest[i * 4 + 0] = static_cast<uint8_t>(words[i] & 0xFF);
        digest[i * 4 + 1] = static_cast<uint8_t>((words[i] >> 8) & 0xFF);
        digest[i * 4 + 2] = static_cast<uint8_t>((words[i] >> 16) & 0xFF);
        digest[i * 4 + 3] = static_cast<uint8_t>((words[i] >> 24) & 0xFF);
    }
    return digest;
}

std::string toHex(const std::array<uint8_t, 16>& digest) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : digest) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

} // namespace

std::string computeMd5Hex(const uint8_t* data, std::size_t length) {
    if (!data || length == 0) {
        return {};
    }
    auto digest = computeMd5Buffer(data, length);
    return toHex(digest);
}

std::string computeFileMd5Hex(const std::string& path) {
    if (path.empty()) {
        return {};
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return {};
    }

    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(stream)),
                                std::istreambuf_iterator<char>());
    if (buffer.empty()) {
        return {};
    }
    return computeMd5Hex(buffer.data(), buffer.size());
}

} // namespace utils
} // namespace opencgm
