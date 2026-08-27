#include <gtest/gtest.h>

#include "opencgm/svg/cell_array.h"

#include <limits>

namespace
{
    using opencgm::Color;
    using opencgm::svg::CellArrayPixelsInput;
    using opencgm::svg::CellArrayPlacementInput;
    using opencgm::svg::CellArrayPreparer;

    TEST(CellArrayPreparerTest, RejectsInvalidDimensions)
    {
        CellArrayPixelsInput pixels;
        pixels.width = 0;
        pixels.height = 2;
        EXPECT_FALSE(
            CellArrayPreparer::preparePixels(pixels).valid);

        CellArrayPlacementInput placement;
        placement.pixel_width = 2;
        placement.pixel_height = 0;
        const auto resolved =
            CellArrayPreparer::resolvePlacement(placement);
        EXPECT_FALSE(resolved.use_matrix);
        EXPECT_FALSE(resolved.fallback_valid);
    }

    TEST(CellArrayPreparerTest, NormalizesRaggedRowsToOpaqueWhite)
    {
        CellArrayPixelsInput input;
        input.width = 3;
        input.height = 2;
        input.resolved_rows = {
            {Color(255, 0, 0, 4)},
            {Color(0, 255, 0), Color(0, 0, 255)}};

        const auto prepared =
            CellArrayPreparer::preparePixels(input);

        ASSERT_TRUE(prepared.valid);
        ASSERT_EQ(prepared.colors.size(), 2u);
        ASSERT_EQ(prepared.colors[0].size(), 3u);
        EXPECT_EQ(prepared.colors[0][0], Color(255, 0, 0));
        EXPECT_EQ(prepared.colors[0][1], Color::White());
        EXPECT_EQ(prepared.colors[1][2], Color::White());
        EXPECT_EQ(prepared.total_pixels, 6u);
        EXPECT_EQ(prepared.transparent_pixels, 0u);
        EXPECT_EQ(prepared.opaque_pixels, 6u);
    }

    TEST(CellArrayPreparerTest, AppliesRgbTransparencyOnlyToProvidedCells)
    {
        CellArrayPixelsInput input;
        input.width = 2;
        input.height = 2;
        input.resolved_rows = {
            {Color(10, 20, 30, 99)}};
        input.apply_transparency = true;
        input.transparent_color = Color(10, 20, 30, 1);

        const auto prepared =
            CellArrayPreparer::preparePixels(input);

        ASSERT_TRUE(prepared.valid);
        EXPECT_EQ(
            prepared.colors[0][0],
            Color(10, 20, 30, 0));
        EXPECT_EQ(prepared.colors[0][1], Color::White());
        EXPECT_EQ(prepared.colors[1][0], Color::White());
        EXPECT_EQ(prepared.transparent_pixels, 1u);
        EXPECT_EQ(prepared.opaque_pixels, 3u);
    }

    TEST(CellArrayPreparerTest, ResolvesSkewedImageMatrix)
    {
        CellArrayPlacementInput input;
        input.pixel_width = 2;
        input.pixel_height = 3;
        input.origin = {10.0, 20.0};
        input.row_end = {14.0, 22.0};
        input.column_end = {7.0, 29.0};
        input.diagonal = {11.0, 31.0};

        const auto placement =
            CellArrayPreparer::resolvePlacement(input);

        ASSERT_TRUE(placement.use_matrix);
        EXPECT_DOUBLE_EQ(placement.matrix.a, 2.0);
        EXPECT_DOUBLE_EQ(placement.matrix.b, 1.0);
        EXPECT_DOUBLE_EQ(placement.matrix.c, -1.0);
        EXPECT_DOUBLE_EQ(placement.matrix.d, 3.0);
        EXPECT_DOUBLE_EQ(placement.matrix.e, 10.0);
        EXPECT_DOUBLE_EQ(placement.matrix.f, 20.0);
        EXPECT_FALSE(placement.fallback_valid);
    }

    TEST(CellArrayPreparerTest, FallsBackToCornerBounds)
    {
        CellArrayPlacementInput input;
        input.pixel_width = 2;
        input.pixel_height = 2;
        input.origin = {0.0, 0.0};
        input.row_end = {0.0, 0.0};
        input.column_end = {0.0, 10.0};
        input.diagonal = {10.0, 10.0};

        const auto placement =
            CellArrayPreparer::resolvePlacement(input);

        EXPECT_FALSE(placement.use_matrix);
        ASSERT_TRUE(placement.fallback_valid);
        EXPECT_DOUBLE_EQ(placement.fallback_x, 0.0);
        EXPECT_DOUBLE_EQ(placement.fallback_y, 0.0);
        EXPECT_DOUBLE_EQ(placement.fallback_width, 10.0);
        EXPECT_DOUBLE_EQ(placement.fallback_height, 10.0);
    }

    TEST(CellArrayPreparerTest, RejectsNonFiniteAndDegenerateFallbacks)
    {
        CellArrayPlacementInput nonfinite;
        nonfinite.pixel_width = 1;
        nonfinite.pixel_height = 1;
        nonfinite.origin = {
            std::numeric_limits<double>::infinity(),
            0.0};
        nonfinite.row_end = {1.0, 0.0};
        nonfinite.column_end = {0.0, 1.0};
        nonfinite.diagonal = {1.0, 1.0};

        const auto invalid =
            CellArrayPreparer::resolvePlacement(nonfinite);
        EXPECT_FALSE(invalid.use_matrix);
        EXPECT_FALSE(invalid.fallback_valid);

        CellArrayPlacementInput flat;
        flat.pixel_width = 1;
        flat.pixel_height = 1;
        flat.origin = {2.0, 2.0};
        flat.row_end = {2.0, 2.0};
        flat.column_end = {2.0, 2.0};
        flat.diagonal = {2.0, 2.0};

        const auto degenerate =
            CellArrayPreparer::resolvePlacement(flat);
        EXPECT_FALSE(degenerate.use_matrix);
        EXPECT_FALSE(degenerate.fallback_valid);
    }
}
