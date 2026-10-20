// The PNG encoder must pick the smallest exact form and round-trip every
// pixel. The decoder here is deliberately independent of the encoder: it
// parses chunks, inflates with miniz, undoes all five filters and expands
// palettes, so a mismatch shows up as a pixel difference.

#include <gtest/gtest.h>

#include "opencgm/svg/png_encoder.h"
#include "miniz/miniz.h"

#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

using opencgm::svg::encodePng;

namespace
{
    struct Decoded
    {
        uint32_t width = 0, height = 0;
        uint8_t bitDepth = 0, colourType = 0;
        bool hasTrns = false;
        std::vector<uint8_t> rgba;
    };

    uint32_t be32(const uint8_t *p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }

    Decoded decode(const std::vector<uint8_t> &png)
    {
        Decoded d;
        std::vector<uint8_t> idat, plte, trns;
        size_t pos = 8;
        while (pos + 12 <= png.size())
        {
            const uint32_t length = be32(&png[pos]);
            const std::string type(png.begin() + pos + 4, png.begin() + pos + 8);
            const uint8_t *data = &png[pos + 8];
            const uint32_t crc = be32(&png[pos + 8 + length]);
            EXPECT_EQ(crc, mz_crc32(MZ_CRC32_INIT, &png[pos + 4], length + 4)) << type;
            if (type == "IHDR")
            {
                d.width = be32(data);
                d.height = be32(data + 4);
                d.bitDepth = data[8];
                d.colourType = data[9];
            }
            else if (type == "PLTE") plte.assign(data, data + length);
            else if (type == "tRNS") trns.assign(data, data + length);
            else if (type == "IDAT") idat.insert(idat.end(), data, data + length);
            pos += 12 + length;
        }
        d.hasTrns = !trns.empty();

        const size_t channels = d.colourType == 2 ? 3 : d.colourType == 6 ? 4 : 1;
        const size_t rowBytes = (d.width * channels * d.bitDepth + 7) / 8;
        mz_ulong rawSize = static_cast<mz_ulong>((rowBytes + 1) * d.height);
        std::vector<uint8_t> raw(rawSize);
        EXPECT_EQ(mz_uncompress(raw.data(), &rawSize, idat.data(), static_cast<mz_ulong>(idat.size())), MZ_OK);

        const size_t bpp = std::max<size_t>(1, channels * d.bitDepth / 8);
        std::vector<uint8_t> prev(rowBytes, 0), row(rowBytes);
        for (uint32_t y = 0; y < d.height; ++y)
        {
            const uint8_t filter = raw[y * (rowBytes + 1)];
            const uint8_t *src = &raw[y * (rowBytes + 1) + 1];
            for (size_t i = 0; i < rowBytes; ++i)
            {
                const int a = i >= bpp ? row[i - bpp] : 0, b = prev[i], c = i >= bpp ? prev[i - bpp] : 0;
                int p = 0;
                if (filter == 1) p = a;
                else if (filter == 2) p = b;
                else if (filter == 3) p = (a + b) / 2;
                else if (filter == 4)
                {
                    const int e = a + b - c, pa = std::abs(e - a), pb = std::abs(e - b), pc = std::abs(e - c);
                    p = (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
                }
                row[i] = static_cast<uint8_t>(src[i] + p);
            }
            for (uint32_t x = 0; x < d.width; ++x)
            {
                if (d.colourType == 3)
                {
                    const size_t perByte = 8 / d.bitDepth;
                    const uint8_t mask = static_cast<uint8_t>((1 << d.bitDepth) - 1);
                    const uint8_t idx = (row[x / perByte] >> (8 - d.bitDepth * (x % perByte + 1))) & mask;
                    d.rgba.insert(d.rgba.end(), {plte[idx * 3], plte[idx * 3 + 1], plte[idx * 3 + 2],
                                                 static_cast<uint8_t>(idx < trns.size() ? trns[idx] : 255)});
                }
                else
                {
                    d.rgba.insert(d.rgba.end(), &row[x * channels], &row[x * channels] + 3);
                    d.rgba.push_back(channels == 4 ? row[x * channels + 3] : 255);
                }
            }
            prev = row;
        }
        return d;
    }

    std::vector<uint8_t> image(uint32_t w, uint32_t h, uint32_t (*colour)(uint32_t, uint32_t))
    {
        std::vector<uint8_t> rgba;
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x)
            {
                const uint32_t c = colour(x, y);
                rgba.insert(rgba.end(), {uint8_t(c >> 24), uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c)});
            }
        return rgba;
    }
}

TEST(PngEncoder, BitonalImageIsOneBitIndexed)
{
    // 13 wide: the last byte of each row is partly padding.
    const auto rgba = image(13, 7, [](uint32_t x, uint32_t y) { return ((x + y) % 3) ? 0x000000FFu : 0xFFFFFFFFu; });
    const auto d = decode(encodePng(rgba, 13, 7));
    EXPECT_EQ(d.colourType, 3);
    EXPECT_EQ(d.bitDepth, 1);
    EXPECT_FALSE(d.hasTrns);
    EXPECT_EQ(d.rgba, rgba);
}

TEST(PngEncoder, BitDepthGrowsWithThePalette)
{
    const std::map<uint32_t, uint8_t> expected{{3, 2}, {16, 4}, {17, 8}, {256, 8}};
    for (const auto &[colours, depth] : expected)
    {
        std::vector<uint8_t> rgba;
        for (uint32_t i = 0; i < 300; ++i)
        {
            const uint32_t c = i % colours;
            rgba.insert(rgba.end(), {uint8_t(c), uint8_t(c * 7), uint8_t(c * 13), 255});
        }
        const auto d = decode(encodePng(rgba, 30, 10));
        EXPECT_EQ(d.colourType, 3) << colours << " colours";
        EXPECT_EQ(d.bitDepth, depth) << colours << " colours";
        EXPECT_EQ(d.rgba, rgba) << colours << " colours";
    }
}

TEST(PngEncoder, TranslucentPaletteEntriesGetTrns)
{
    const auto rgba = image(9, 9, [](uint32_t x, uint32_t) { return x < 3 ? 0x00000000u : x < 6 ? 0xFF000080u : 0x00FF00FFu; });
    const auto d = decode(encodePng(rgba, 9, 9));
    EXPECT_EQ(d.colourType, 3);
    EXPECT_TRUE(d.hasTrns);
    EXPECT_EQ(d.rgba, rgba);
}

TEST(PngEncoder, ManyColoursUseFilteredTruecolour)
{
    // A gradient has far more than 256 colours and rewards row filtering.
    const auto opaque = image(64, 48, [](uint32_t x, uint32_t y) { return (x * 4) << 24 | (y * 5) << 16 | ((x + y) * 2) << 8 | 0xFFu; });
    const auto d = decode(encodePng(opaque, 64, 48));
    EXPECT_EQ(d.colourType, 2);
    EXPECT_EQ(d.rgba, opaque);

    const auto translucent = image(64, 48, [](uint32_t x, uint32_t y) { return (x * 4) << 24 | (y * 5) << 16 | 0x8000u | (x + y); });
    const auto t = decode(encodePng(translucent, 64, 48));
    EXPECT_EQ(t.colourType, 6);
    EXPECT_EQ(t.rgba, translucent);
}

TEST(PngEncoder, RejectsInconsistentInput)
{
    EXPECT_TRUE(encodePng({}, 0, 0).empty());
    EXPECT_TRUE(encodePng(std::vector<uint8_t>(15), 2, 2).empty());
}
