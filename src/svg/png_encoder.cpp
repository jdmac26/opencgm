#include "opencgm/svg/png_encoder.h"

#include "miniz/miniz.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <string>
#include <unordered_map>

namespace opencgm {
namespace svg {

namespace {

void appendU32(std::vector<uint8_t>& out, uint32_t value)
{
    out.push_back(static_cast<uint8_t>(value >> 24));
    out.push_back(static_cast<uint8_t>(value >> 16));
    out.push_back(static_cast<uint8_t>(value >> 8));
    out.push_back(static_cast<uint8_t>(value));
}

void appendChunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data)
{
    appendU32(out, static_cast<uint32_t>(data.size()));
    const size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    const auto crc = static_cast<uint32_t>(
        mz_crc32(MZ_CRC32_INIT, out.data() + start, out.size() - start));
    appendU32(out, crc);
}

uint8_t paeth(int a, int b, int c)
{
    const int p = a + b - c;
    const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    if (pa <= pb && pa <= pc)
        return static_cast<uint8_t>(a);
    return static_cast<uint8_t>(pb <= pc ? b : c);
}

// Truecolour rows: try each PNG filter and keep the one whose output has the
// smallest sum of absolute (signed) values, the usual libpng heuristic.
void appendFilteredRow(std::vector<uint8_t>& raw, const uint8_t* row, const uint8_t* prev,
                       size_t length, size_t bpp, std::array<std::vector<uint8_t>, 5>& scratch)
{
    for (int filter = 0; filter < 5; ++filter)
    {
        auto& out = scratch[filter];
        out.resize(length);
        for (size_t i = 0; i < length; ++i)
        {
            const int a = i >= bpp ? row[i - bpp] : 0;
            const int b = prev ? prev[i] : 0;
            const int c = (prev && i >= bpp) ? prev[i - bpp] : 0;
            int predicted = 0;
            switch (filter)
            {
            case 1: predicted = a; break;
            case 2: predicted = b; break;
            case 3: predicted = (a + b) / 2; break;
            case 4: predicted = paeth(a, b, c); break;
            default: break;
            }
            out[i] = static_cast<uint8_t>(row[i] - predicted);
        }
    }
    int best = 0;
    long bestScore = -1;
    for (int filter = 0; filter < 5; ++filter)
    {
        long score = 0;
        for (uint8_t v : scratch[filter])
            score += v < 128 ? v : 256 - v;
        if (bestScore < 0 || score < bestScore)
        {
            bestScore = score;
            best = filter;
        }
    }
    raw.push_back(static_cast<uint8_t>(best));
    raw.insert(raw.end(), scratch[best].begin(), scratch[best].end());
}

} // namespace

std::vector<uint8_t> encodePng(const std::vector<uint8_t>& rgba, uint32_t width, uint32_t height)
{
    const size_t pixels = static_cast<size_t>(width) * height;
    if (width == 0 || height == 0 || rgba.size() != pixels * 4)
        return {};

    auto pixelAt = [&](size_t i) {
        return (static_cast<uint32_t>(rgba[i * 4]) << 24) | (static_cast<uint32_t>(rgba[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(rgba[i * 4 + 2]) << 8) | rgba[i * 4 + 3];
    };

    // Distinct colours, up to the 256 a palette can hold.
    std::unordered_map<uint32_t, uint8_t> index;
    std::vector<uint32_t> palette;
    bool opaque = true;
    for (size_t i = 0; i < pixels; ++i)
    {
        const uint32_t px = pixelAt(i);
        opaque = opaque && (px & 0xFF) == 0xFF;
        if (palette.size() <= 256 && index.find(px) == index.end())
        {
            if (palette.size() < 256)
                index.emplace(px, static_cast<uint8_t>(palette.size()));
            palette.push_back(px);
        }
    }

    const bool indexed = palette.size() <= 256;
    std::vector<uint8_t> raw;
    uint8_t bitDepth = 8;
    uint8_t colourType = 0;

    if (indexed)
    {
        // Translucent entries first, so tRNS only needs to cover them.
        std::stable_partition(palette.begin(), palette.end(), [](uint32_t c) { return (c & 0xFF) != 0xFF; });
        for (size_t i = 0; i < palette.size(); ++i)
            index[palette[i]] = static_cast<uint8_t>(i);

        bitDepth = palette.size() <= 2 ? 1 : palette.size() <= 4 ? 2 : palette.size() <= 16 ? 4 : 8;
        colourType = 3;
        const size_t perByte = 8 / bitDepth;
        const size_t rowBytes = (width + perByte - 1) / perByte;
        raw.reserve((rowBytes + 1) * height);
        for (uint32_t y = 0; y < height; ++y)
        {
            raw.push_back(0);  // filter: none suits indexed data
            const size_t rowStart = raw.size();
            raw.resize(rowStart + rowBytes, 0);
            for (uint32_t x = 0; x < width; ++x)
            {
                const uint8_t value = index[pixelAt(static_cast<size_t>(y) * width + x)];
                const size_t shift = 8 - bitDepth * (x % perByte + 1);
                raw[rowStart + x / perByte] |= static_cast<uint8_t>(value << shift);
            }
        }
    }
    else
    {
        const size_t channels = opaque ? 3 : 4;
        colourType = opaque ? 2 : 6;
        const size_t rowBytes = static_cast<size_t>(width) * channels;
        std::vector<uint8_t> current(rowBytes), previous(rowBytes);
        std::array<std::vector<uint8_t>, 5> scratch;
        raw.reserve((rowBytes + 1) * height);
        for (uint32_t y = 0; y < height; ++y)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                const size_t src = (static_cast<size_t>(y) * width + x) * 4;
                std::copy_n(&rgba[src], channels, &current[x * channels]);
            }
            appendFilteredRow(raw, current.data(), y ? previous.data() : nullptr, rowBytes, channels, scratch);
            std::swap(current, previous);
        }
    }

    mz_ulong compressedSize = mz_compressBound(static_cast<mz_ulong>(raw.size()));
    std::vector<uint8_t> compressed(compressedSize);
    if (mz_compress2(compressed.data(), &compressedSize, raw.data(), static_cast<mz_ulong>(raw.size()),
                     MZ_BEST_COMPRESSION) != MZ_OK)
        return {};
    compressed.resize(compressedSize);

    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> header;
    appendU32(header, width);
    appendU32(header, height);
    header.insert(header.end(), {bitDepth, colourType, 0, 0, 0});
    appendChunk(png, "IHDR", header);

    if (indexed)
    {
        std::vector<uint8_t> plte, trns;
        for (uint32_t c : palette)
        {
            plte.insert(plte.end(), {static_cast<uint8_t>(c >> 24), static_cast<uint8_t>(c >> 16),
                                     static_cast<uint8_t>(c >> 8)});
            if ((c & 0xFF) != 0xFF)
                trns.push_back(static_cast<uint8_t>(c & 0xFF));
        }
        appendChunk(png, "PLTE", plte);
        if (!trns.empty())
            appendChunk(png, "tRNS", trns);
    }
    appendChunk(png, "IDAT", compressed);
    appendChunk(png, "IEND", {});
    return png;
}

} // namespace svg
} // namespace opencgm
