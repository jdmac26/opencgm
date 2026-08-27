#include "opencgm/utils/gzip_utils.h"
#include "opencgm/security_limits.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

extern "C"
{
#include "miniz/miniz.h"
}

namespace opencgm
{
    namespace
    {
        struct GzipView
        {
            const uint8_t *deflate = nullptr;
            size_t deflateSize = 0;
            uint32_t crc32 = 0;
            uint32_t isize = 0;
        };

        uint32_t crc32(const uint8_t *data, size_t length)
        {
            static uint32_t table[256];
            static bool initialized = false;
            if (!initialized)
            {
                for (uint32_t i = 0; i < 256; ++i)
                {
                    uint32_t crc = i;
                    for (int j = 0; j < 8; ++j)
                    {
                        if (crc & 1u)
                        {
                            crc = 0xEDB88320u ^ (crc >> 1);
                        }
                        else
                        {
                            crc >>= 1;
                        }
                    }
                    table[i] = crc;
                }
                initialized = true;
            }

            uint32_t crc = 0xFFFFFFFFu;
            for (size_t i = 0; i < length; ++i)
            {
                crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
            }
            return crc ^ 0xFFFFFFFFu;
        }

        bool parseGzip(const uint8_t *data, size_t size, GzipView &view)
        {
            if (!data || size < 18)
            {
                return false;
            }

            if (!(data[0] == 0x1F && data[1] == 0x8B))
            {
                return false;
            }

            if (data[2] != 8)
            {
                return false; // only deflate supported
            }

            uint8_t flags = data[3];
            size_t offset = 10; // basic header length

            auto ensureAvailable = [&](size_t count) -> bool {
                if (offset + count > size)
                {
                    return false;
                }
                return true;
            };

            if (flags & 0x04)
            {
                if (!ensureAvailable(2))
                {
                    return false;
                }
                uint16_t extraLen = static_cast<uint16_t>(data[offset]) |
                                    (static_cast<uint16_t>(data[offset + 1]) << 8);
                offset += 2;
                if (!ensureAvailable(extraLen))
                {
                    return false;
                }
                offset += extraLen;
            }

            auto skipZString = [&](void) -> bool {
                while (offset < size && data[offset] != 0)
                {
                    ++offset;
                }
                if (offset >= size)
                {
                    return false;
                }
                ++offset; // skip NUL terminator
                return true;
            };

            if (flags & 0x08)
            {
                if (!skipZString())
                {
                    return false;
                }
            }

            if (flags & 0x10)
            {
                if (!skipZString())
                {
                    return false;
                }
            }

            if (flags & 0x02)
            {
                if (!ensureAvailable(2))
                {
                    return false;
                }
                offset += 2;
            }

            if (offset >= size || size - offset < 8)
            {
                return false;
            }

            size_t trailerPos = size - 8;
            if (trailerPos < offset)
            {
                return false;
            }

            view.deflate = data + offset;
            view.deflateSize = trailerPos - offset;
            view.crc32 = static_cast<uint32_t>(data[trailerPos]) |
                         (static_cast<uint32_t>(data[trailerPos + 1]) << 8) |
                         (static_cast<uint32_t>(data[trailerPos + 2]) << 16) |
                         (static_cast<uint32_t>(data[trailerPos + 3]) << 24);
            view.isize = static_cast<uint32_t>(data[trailerPos + 4]) |
                         (static_cast<uint32_t>(data[trailerPos + 5]) << 8) |
                         (static_cast<uint32_t>(data[trailerPos + 6]) << 16) |
                         (static_cast<uint32_t>(data[trailerPos + 7]) << 24);

            return view.deflate && view.deflateSize > 0;
        }

    } // namespace

    bool isGzipStream(const uint8_t *data, size_t size)
    {
        if (!data || size < 2)
            return false;
        return data[0] == 0x1F && data[1] == 0x8B;
    }

    bool gzipDecompress(const uint8_t *data, size_t size, std::vector<uint8_t> &out)
    {
        out.clear();
        if (!data || size == 0)
        {
            return false;
        }

        GzipView view;
        if (!parseGzip(data, size, view))
        {
            return false;
        }

        // Fast reject based on advertised ISIZE before any decompression work.
        if (view.isize > static_cast<uint32_t>(security::MAX_DECOMPRESSED_SIZE))
        {
            std::cerr << "[gzip] Advertised decompressed size " << view.isize
                      << " exceeds security limit " << security::MAX_DECOMPRESSED_SIZE << "\n";
            return false;
        }

        mz_stream stream = {};
        stream.next_in = nullptr;
        stream.avail_in = 0;
        stream.next_out = nullptr;
        stream.avail_out = 0;

        if (mz_inflateInit2(&stream, -MZ_DEFAULT_WINDOW_BITS) != MZ_OK)
        {
            std::cerr << "[gzip] mz_inflateInit2 failed\n";
            return false;
        }

        constexpr size_t CHUNK_SIZE = 16 * 1024;
        std::vector<uint8_t> chunk(CHUNK_SIZE);
        size_t inputOffset = 0;
        int status = MZ_OK;

        while (status != MZ_STREAM_END)
        {
            if (stream.avail_in == 0 && inputOffset < view.deflateSize)
            {
                size_t remaining = view.deflateSize - inputOffset;
                size_t toFeed = std::min(remaining, static_cast<size_t>(std::numeric_limits<mz_uint>::max()));
                stream.next_in = const_cast<mz_uint8 *>(reinterpret_cast<const mz_uint8 *>(view.deflate + inputOffset));
                stream.avail_in = static_cast<mz_uint>(toFeed);
                inputOffset += toFeed;
            }

            stream.next_out = reinterpret_cast<mz_uint8 *>(chunk.data());
            stream.avail_out = static_cast<mz_uint>(chunk.size());

            status = mz_inflate(&stream, MZ_NO_FLUSH);
            if (status != MZ_OK && status != MZ_STREAM_END && status != MZ_BUF_ERROR)
            {
                mz_inflateEnd(&stream);
                std::cerr << "[gzip] mz_inflate failed with code " << status << "\n";
                out.clear();
                return false;
            }

            size_t produced = chunk.size() - stream.avail_out;
            if (produced > 0)
            {
                if (out.size() > security::MAX_DECOMPRESSED_SIZE - produced)
                {
                    mz_inflateEnd(&stream);
                    std::cerr << "[gzip] Decompressed data exceeds security limit "
                              << security::MAX_DECOMPRESSED_SIZE << "\n";
                    out.clear();
                    return false;
                }
                out.insert(out.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(produced));
            }

            if (status == MZ_BUF_ERROR)
            {
                // No forward progress possible; if we've consumed all input this is malformed data.
                if (stream.avail_in == 0 && inputOffset >= view.deflateSize && produced == 0)
                {
                    mz_inflateEnd(&stream);
                    std::cerr << "[gzip] Unexpected end of compressed stream\n";
                    out.clear();
                    return false;
                }
                status = MZ_OK;
            }
        }

        mz_inflateEnd(&stream);

        uint32_t crc = crc32(out.data(), out.size());
        if (crc != view.crc32)
        {
            return false;
        }
        if ((view.isize & 0xFFFFFFFFu) != (static_cast<uint32_t>(out.size()) & 0xFFFFFFFFu))
        {
            return false;
        }

        return true;
    }

    bool gzipCompress(const uint8_t *data, size_t size, std::vector<uint8_t> &out, int level)
    {
        out.clear();
        if (!data || size == 0)
        {
            return false;
        }

        // Clamp compression level
        if (level < 1) level = 1;
        if (level > 9) level = 9;

        // Calculate CRC32 of input data
        uint32_t inputCrc = crc32(data, size);

        // Estimate compressed size (usually smaller than input, but allocate worst case)
        size_t maxCompressedSize = mz_compressBound(size);
        std::vector<uint8_t> deflateBuffer(maxCompressedSize);

        // Compress using raw deflate
        mz_ulong compressedSize = static_cast<mz_ulong>(maxCompressedSize);
        int result = mz_compress2(deflateBuffer.data(), &compressedSize, data, static_cast<mz_ulong>(size), level);
        if (result != MZ_OK)
        {
            std::cerr << "[gzip] mz_compress2 failed with code " << result << "\n";
            return false;
        }

        // Strip zlib header (first 2 bytes) and Adler-32 checksum (last 4 bytes)
        // to get raw deflate data for gzip format
        if (compressedSize < 6)
        {
            return false;
        }
        size_t deflateSize = compressedSize - 6; // Remove zlib header (2) and Adler-32 (4)
        const uint8_t *deflateData = deflateBuffer.data() + 2; // Skip zlib header

        // Build gzip output
        // Gzip format:
        // - 10-byte header
        // - compressed data (raw deflate)
        // - 8-byte trailer (CRC32 + ISIZE)
        size_t totalSize = 10 + deflateSize + 8;
        out.reserve(totalSize);

        // Write gzip header
        out.push_back(0x1F);                             // Magic number (gzip signature)
        out.push_back(0x8B);                             // Magic number
        out.push_back(0x08);                             // Compression method (deflate)
        out.push_back(0x00);                             // Flags (none)
        out.push_back(0x00);                             // Modification time (4 bytes, all zeros)
        out.push_back(0x00);
        out.push_back(0x00);
        out.push_back(0x00);
        out.push_back(level >= 9 ? 0x02 : (level <= 1 ? 0x04 : 0x00)); // Extra flags
        out.push_back(0x00);                             // OS (unknown)

        // Append deflate data
        out.insert(out.end(), deflateData, deflateData + deflateSize);

        // Write gzip trailer (CRC32 + ISIZE in little-endian)
        out.push_back(static_cast<uint8_t>(inputCrc & 0xFF));
        out.push_back(static_cast<uint8_t>((inputCrc >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>((inputCrc >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((inputCrc >> 24) & 0xFF));

        uint32_t isize = static_cast<uint32_t>(size & 0xFFFFFFFF);
        out.push_back(static_cast<uint8_t>(isize & 0xFF));
        out.push_back(static_cast<uint8_t>((isize >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>((isize >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((isize >> 24) & 0xFF));

        return true;
    }

    bool gzipCompress(const std::string &input, std::vector<uint8_t> &out, int level)
    {
        return gzipCompress(reinterpret_cast<const uint8_t *>(input.data()), input.size(), out, level);
    }
} // namespace opencgm
