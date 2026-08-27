#include <gtest/gtest.h>

#include "opencgm/svg/tile_raster.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace
{
    using opencgm::Color;
    using opencgm::ColorModel;
    using opencgm::ColorSelectionMode;
    using opencgm::svg::TileBitmapFormat;
    using opencgm::svg::TileRasterDecoder;

    uint32_t read32(
        const std::vector<uint8_t> &bytes,
        std::size_t offset)
    {
        return
            static_cast<uint32_t>(bytes[offset]) |
            (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
            (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
            (static_cast<uint32_t>(bytes[offset + 3]) << 24);
    }

    TEST(TileRasterDecoderTest, BuildsBottomUpBitonalBmp)
    {
        const auto bmp = TileRasterDecoder::buildBitonalBmp(
            {0x80, 0x40},
            2,
            2,
            Color(10, 20, 30),
            Color(200, 210, 220));

        ASSERT_TRUE(bmp.has_value());
        ASSERT_EQ(bmp->size(), 70u);
        EXPECT_EQ((*bmp)[0], 'B');
        EXPECT_EQ((*bmp)[1], 'M');
        EXPECT_EQ(read32(*bmp, 10), 62u);
        EXPECT_EQ(read32(*bmp, 18), 2u);
        EXPECT_EQ(read32(*bmp, 22), 2u);
        EXPECT_EQ((*bmp)[54], 30);
        EXPECT_EQ((*bmp)[55], 20);
        EXPECT_EQ((*bmp)[56], 10);
        EXPECT_EQ((*bmp)[58], 220);
        EXPECT_EQ((*bmp)[59], 210);
        EXPECT_EQ((*bmp)[60], 200);
        EXPECT_EQ((*bmp)[62], 0x40);
        EXPECT_EQ((*bmp)[66], 0x80);
    }

    TEST(TileRasterDecoderTest, RejectsTruncatedBitonalPayload)
    {
        EXPECT_FALSE(
            TileRasterDecoder::buildBitonalBmp(
                {0x80},
                8,
                2,
                Color::White(),
                Color::Black())
                .has_value());
        EXPECT_FALSE(
            TileRasterDecoder::buildBitonalBmp(
                {},
                0,
                1,
                Color::White(),
                Color::Black())
                .has_value());
    }

    TEST(TileRasterDecoderTest, DecodesIndexedBitmap)
    {
        TileBitmapFormat format;
        format.width = 2;
        format.height = 1;
        format.selection_mode = ColorSelectionMode::INDEXED;
        format.local_color_precision = 4;
        const std::map<int, Color> palette = {
            {1, Color(255, 0, 0)},
            {2, Color(0, 255, 0)}};

        const auto bmp =
            TileRasterDecoder::decodeBitmapToBmp(
                {0x12},
                format,
                palette);

        ASSERT_TRUE(bmp.has_value());
        ASSERT_EQ(bmp->size(), 62u);
        EXPECT_EQ(read32(*bmp, 10), 54u);
        EXPECT_EQ((*bmp)[54], 0);
        EXPECT_EQ((*bmp)[55], 0);
        EXPECT_EQ((*bmp)[56], 255);
        EXPECT_EQ((*bmp)[57], 0);
        EXPECT_EQ((*bmp)[58], 255);
        EXPECT_EQ((*bmp)[59], 0);
    }

    TEST(TileRasterDecoderTest, DecodesDirectRgbBitmap)
    {
        TileBitmapFormat format;
        format.width = 1;
        format.height = 1;
        format.selection_mode = ColorSelectionMode::DIRECT;
        format.color_model = ColorModel::RGB;
        format.direct_color_precision = 8;

        const auto bmp =
            TileRasterDecoder::decodeBitmapToBmp(
                {10, 20, 30},
                format,
                {});

        ASSERT_TRUE(bmp.has_value());
        EXPECT_EQ((*bmp)[54], 30);
        EXPECT_EQ((*bmp)[55], 20);
        EXPECT_EQ((*bmp)[56], 10);
    }

    TEST(TileRasterDecoderTest, DecodesDirectCmykBitmap)
    {
        TileBitmapFormat format;
        format.width = 1;
        format.height = 1;
        format.selection_mode = ColorSelectionMode::DIRECT;
        format.color_model = ColorModel::CMYK;
        format.direct_color_precision = 8;

        const auto bmp =
            TileRasterDecoder::decodeBitmapToBmp(
                {0, 255, 255, 0},
                format,
                {});

        ASSERT_TRUE(bmp.has_value());
        EXPECT_EQ((*bmp)[54], 0);
        EXPECT_EQ((*bmp)[55], 0);
        EXPECT_EQ((*bmp)[56], 255);
    }

    TEST(TileRasterDecoderTest, DecodesCieNeutralWhite)
    {
        for (const ColorModel model :
             {ColorModel::CIELAB, ColorModel::CIELUV})
        {
            TileBitmapFormat format;
            format.width = 1;
            format.height = 1;
            format.selection_mode = ColorSelectionMode::DIRECT;
            format.color_model = model;
            format.direct_color_precision = 8;

            const auto bmp =
                TileRasterDecoder::decodeBitmapToBmp(
                    {255, 0, 0},
                    format,
                    {});

            ASSERT_TRUE(bmp.has_value());
            EXPECT_GE((*bmp)[54], 250);
            EXPECT_GE((*bmp)[55], 250);
            EXPECT_GE((*bmp)[56], 250);
        }
    }

    TEST(TileRasterDecoderTest, HonorsPaddedSourceRows)
    {
        TileBitmapFormat format;
        format.width = 1;
        format.height = 2;
        format.selection_mode = ColorSelectionMode::INDEXED;
        format.color_index_precision = 8;
        const std::map<int, Color> palette = {
            {1, Color(255, 0, 0)},
            {2, Color(0, 255, 0)}};

        const auto bmp =
            TileRasterDecoder::decodeBitmapToBmp(
                {1, 0xEE, 2, 0xEE},
                format,
                palette);

        ASSERT_TRUE(bmp.has_value());
        ASSERT_EQ(bmp->size(), 62u);
        EXPECT_EQ((*bmp)[54], 0);
        EXPECT_EQ((*bmp)[55], 255);
        EXPECT_EQ((*bmp)[56], 0);
        EXPECT_EQ((*bmp)[58], 0);
        EXPECT_EQ((*bmp)[59], 0);
        EXPECT_EQ((*bmp)[60], 255);
    }

    TEST(TileRasterDecoderTest, UsesOpaqueBlackForMissingIndex)
    {
        TileBitmapFormat format;
        format.width = 1;
        format.height = 1;
        format.selection_mode = ColorSelectionMode::INDEXED;
        format.color_index_precision = 8;

        const auto bmp =
            TileRasterDecoder::decodeBitmapToBmp(
                {42},
                format,
                {});

        ASSERT_TRUE(bmp.has_value());
        EXPECT_EQ((*bmp)[54], 0);
        EXPECT_EQ((*bmp)[55], 0);
        EXPECT_EQ((*bmp)[56], 0);
    }

    TEST(TileRasterDecoderTest, RejectsInvalidOrTruncatedBitmap)
    {
        TileBitmapFormat format;
        format.width = 2;
        format.height = 1;
        format.selection_mode = ColorSelectionMode::DIRECT;
        format.direct_color_precision = 8;

        EXPECT_FALSE(
            TileRasterDecoder::decodeBitmapToBmp(
                {1, 2, 3},
                format,
                {})
                .has_value());

        format.width = 1;
        format.direct_color_precision = 33;
        EXPECT_FALSE(
            TileRasterDecoder::decodeBitmapToBmp(
                {1, 2, 3, 4},
                format,
                {})
                .has_value());
    }
}
