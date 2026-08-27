#include <gtest/gtest.h>

#include "opencgm/svg/tile_geometry.h"

#include <limits>

namespace
{
    using opencgm::svg::TileGeometryInput;
    using opencgm::svg::TileGeometryResolver;

    TEST(TileGeometryResolverTest, ResolvesStandaloneTileDimensions)
    {
        TileGeometryInput input;
        input.preferred_width = 16;
        input.preferred_height = 8;
        input.picture_width = 100.0;
        input.picture_height = 50.0;
        input.cell_size_path = 2.0;
        input.cell_size_line = 4.0;
        input.path_direction = 0;
        input.line_direction = 1;

        const auto geometry =
            TileGeometryResolver::resolve(input);

        EXPECT_EQ(geometry.tileWidthCellCount, 16);
        EXPECT_EQ(geometry.tileHeightCellCount, 8);
        EXPECT_EQ(geometry.activeWidthCells, 16);
        EXPECT_EQ(geometry.activeHeightCells, 8);
        EXPECT_EQ(geometry.pixelWidth, 16);
        EXPECT_EQ(geometry.pixelHeight, 8);
        EXPECT_DOUBLE_EQ(geometry.unitsPerCellPath, 0.5);
        EXPECT_DOUBLE_EQ(geometry.unitsPerCellLine, 0.25);
        EXPECT_DOUBLE_EQ(geometry.totalWidthUnits, 8.0);
        EXPECT_DOUBLE_EQ(geometry.totalHeightUnits, 2.0);
        EXPECT_DOUBLE_EQ(geometry.origin.x(), 0.0);
        EXPECT_DOUBLE_EQ(geometry.origin.y(), 0.0);
        EXPECT_DOUBLE_EQ(geometry.pathVector.x(), 8.0);
        EXPECT_DOUBLE_EQ(geometry.pathVector.y(), 0.0);
        EXPECT_DOUBLE_EQ(geometry.lineVector.x(), 0.0);
        EXPECT_DOUBLE_EQ(geometry.lineVector.y(), 2.0);
        EXPECT_FALSE(geometry.usedWidthFallback);
    }

    TEST(TileGeometryResolverTest, AppliesTileArrayOffsetsAndYAxisFlip)
    {
        TileGeometryInput input;
        input.image_cells_path = 4;
        input.image_cells_line = 2;
        input.picture_width = 100.0;
        input.picture_height = 50.0;
        input.cell_size_path = 2.0;
        input.cell_size_line = 2.0;
        input.in_tile_array = true;
        input.tile_index = 5;
        input.tiles_in_path = 3;
        input.image_offset_path = 1;
        input.image_offset_line = 2;
        input.array_position = opencgm::CGMPoint(10.0, 20.0);
        input.path_direction = 0;
        input.line_direction = 1;
        input.flip_y = true;

        const auto geometry =
            TileGeometryResolver::resolve(input);

        EXPECT_DOUBLE_EQ(geometry.origin.x(), 14.5);
        EXPECT_DOUBLE_EQ(geometry.origin.y(), 18.0);
        EXPECT_DOUBLE_EQ(geometry.pathVector.x(), 2.0);
        EXPECT_DOUBLE_EQ(geometry.pathVector.y(), 0.0);
        EXPECT_DOUBLE_EQ(geometry.lineVector.x(), 0.0);
        EXPECT_DOUBLE_EQ(geometry.lineVector.y(), -1.0);
    }

    TEST(TileGeometryResolverTest, UsesMalformedSizeHeuristics)
    {
        TileGeometryInput input;
        input.image_cells_path = 10;
        input.image_cells_line = 10;
        input.picture_width = 1000.0;
        input.picture_height = 500.0;
        input.cell_size_path = 1000000.0;
        input.cell_size_line = 1000000.0;
        input.apply_malformed_size_heuristics = true;

        const auto geometry =
            TileGeometryResolver::resolve(input);

        EXPECT_TRUE(geometry.usedWidthFallback);
        EXPECT_TRUE(geometry.usedWidthHeuristic);
        EXPECT_TRUE(geometry.usedHeightHeuristic);
        EXPECT_DOUBLE_EQ(geometry.totalWidthUnits, 550.0);
        EXPECT_DOUBLE_EQ(geometry.totalHeightUnits, 200.0);
        EXPECT_DOUBLE_EQ(geometry.unitsPerCellPath, 55.0);
        EXPECT_DOUBLE_EQ(geometry.unitsPerCellLine, 20.0);
    }

    TEST(TileGeometryResolverTest, FallsBackFromInvalidSizesAndReferences)
    {
        TileGeometryInput input;
        input.preferred_width = 0;
        input.preferred_height = 0;
        input.image_cells_path = 0;
        input.image_cells_line = 0;
        input.cells_per_tile_path = 0;
        input.cells_per_tile_line = -1;
        input.picture_width =
            std::numeric_limits<double>::infinity();
        input.picture_height =
            std::numeric_limits<double>::quiet_NaN();
        input.picture_width_raw = -1.0;
        input.picture_height_raw = 0.0;
        input.viewbox_width = 200.0;
        input.viewbox_height = 80.0;
        input.cell_size_path = 0.0;
        input.cell_size_line =
            std::numeric_limits<double>::quiet_NaN();

        const auto geometry =
            TileGeometryResolver::resolve(input);

        EXPECT_EQ(geometry.tileWidthCellCount, 1);
        EXPECT_EQ(geometry.tileHeightCellCount, 1);
        EXPECT_EQ(geometry.pixelWidth, 1);
        EXPECT_EQ(geometry.pixelHeight, 1);
        EXPECT_DOUBLE_EQ(geometry.referenceWidth, 200.0);
        EXPECT_DOUBLE_EQ(geometry.referenceHeight, 80.0);
        EXPECT_DOUBLE_EQ(geometry.totalWidthUnits, 200.0);
        EXPECT_DOUBLE_EQ(geometry.totalHeightUnits, 80.0);
        EXPECT_TRUE(geometry.invalidWidthCellSize);
        EXPECT_TRUE(geometry.invalidHeightCellSize);
        EXPECT_TRUE(geometry.usedWidthFallback);
        EXPECT_FALSE(geometry.usedWidthHeuristic);
        EXPECT_FALSE(geometry.usedHeightHeuristic);
    }

    TEST(TileGeometryResolverTest, NormalizesAllDirectionValues)
    {
        const auto positiveX =
            TileGeometryResolver::directionVector(4, true);
        const auto negativeX =
            TileGeometryResolver::directionVector(-2, false);
        const auto positiveY =
            TileGeometryResolver::directionVector(-1, true);
        const auto negativeY =
            TileGeometryResolver::directionVector(5, true);

        EXPECT_DOUBLE_EQ(positiveX.x(), 1.0);
        EXPECT_DOUBLE_EQ(positiveX.y(), 0.0);
        EXPECT_DOUBLE_EQ(negativeX.x(), -1.0);
        EXPECT_DOUBLE_EQ(negativeX.y(), 0.0);
        EXPECT_DOUBLE_EQ(positiveY.x(), 0.0);
        EXPECT_DOUBLE_EQ(positiveY.y(), 1.0);
        EXPECT_DOUBLE_EQ(negativeY.x(), 0.0);
        EXPECT_DOUBLE_EQ(negativeY.y(), -1.0);
    }
}
