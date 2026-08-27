#include <gtest/gtest.h>

#include "opencgm/svg/aps_geometry.h"

#include <limits>
#include <string>
#include <vector>

namespace
{
    TEST(ApsGeometryTest, OrdersAndStrictlyParsesRectangles)
    {
        const auto ordered =
            opencgm::svg::ApsGeometry::orderedRect(
                10.0,
                20.0,
                1.0,
                2.0);
        ASSERT_TRUE(ordered);
        EXPECT_DOUBLE_EQ(ordered->min_x, 1.0);
        EXPECT_DOUBLE_EQ(ordered->min_y, 2.0);
        EXPECT_DOUBLE_EQ(ordered->max_x, 10.0);
        EXPECT_DOUBLE_EQ(ordered->max_y, 20.0);

        EXPECT_FALSE(
            opencgm::svg::ApsGeometry::orderedRect(
                std::numeric_limits<double>::infinity(),
                0.0,
                1.0,
                1.0));
        EXPECT_TRUE(
            opencgm::svg::ApsGeometry::parseRectTokens(
                {"10", "20", "1", "2"}));
        EXPECT_FALSE(
            opencgm::svg::ApsGeometry::parseRectTokens(
                {"10x", "20", "1", "2"}));
        EXPECT_FALSE(
            opencgm::svg::ApsGeometry::parseRectTokens(
                {"10", "20", "1"}));
    }

    TEST(ApsGeometryTest, ClampsAndFormatsRectangles)
    {
        const auto clamped = opencgm::svg::ApsGeometry::clampRect(
            {-10.0, 2.5, 20.0, 12.0},
            {0.0, 0.0, 10.0, 10.0});
        ASSERT_TRUE(clamped);
        EXPECT_EQ(
            opencgm::svg::ApsGeometry::formatRect(*clamped),
            "0 2.5 10 10");

        EXPECT_FALSE(opencgm::svg::ApsGeometry::clampRect(
            {-10.0, -10.0, -1.0, -1.0},
            {0.0, 0.0, 10.0, 10.0}));
    }

    TEST(ApsGeometryTest, ConvertsBothVdcYAxisDirectionsToNvdc)
    {
        const opencgm::svg::ApsRect rect{
            10.0, 20.0, 30.0, 40.0};

        const auto yUp = opencgm::svg::ApsGeometry::toNvdc(
            rect, true, 0.1, 0.0, 0.0, 100.0, false);
        ASSERT_TRUE(yUp);
        EXPECT_EQ(
            opencgm::svg::ApsGeometry::formatRect(*yUp),
            "1 2 3 4");

        const auto yDown = opencgm::svg::ApsGeometry::toNvdc(
            rect, true, 0.1, 0.0, 0.0, 100.0, true);
        ASSERT_TRUE(yDown);
        EXPECT_EQ(
            opencgm::svg::ApsGeometry::formatRect(*yDown),
            "1 6 3 8");

        EXPECT_FALSE(opencgm::svg::ApsGeometry::toNvdc(
            rect, false, 0.1, 0.0, 0.0, 100.0, false));
    }

    TEST(ApsGeometryTest, StrictlyParsesRegionPointLists)
    {
        const auto points =
            opencgm::svg::ApsGeometry::parseRegionPoints(
                "0,0 10,0 10,10");
        ASSERT_TRUE(points);
        EXPECT_EQ(points->size(), 3U);

        EXPECT_FALSE(
            opencgm::svg::ApsGeometry::parseRegionPoints(
                "0,0 10"));
        EXPECT_FALSE(
            opencgm::svg::ApsGeometry::parseRegionPoints(
                "0,0 10,0 trailing"));
        EXPECT_FALSE(
            opencgm::svg::ApsGeometry::parseRegionPoints(
                "0,0 nan,10"));
    }

    TEST(ApsGeometryTest, ExpandsTwoCornerRegionsToRectangles)
    {
        const auto geometry =
            opencgm::svg::ApsGeometry::normalizeRegion({
                opencgm::CGMPoint(10.0, 20.0),
                opencgm::CGMPoint(1.0, 2.0)});

        EXPECT_TRUE(geometry.expanded_rectangle);
        EXPECT_TRUE(geometry.validPolygon());
        EXPECT_EQ(geometry.metadata_points.size(), 4U);
        EXPECT_EQ(
            geometry.metadata_value,
            "1,2 10,2 10,20 1,20");
        EXPECT_EQ(
            geometry.polygon_value,
            geometry.metadata_value);
    }

    TEST(ApsGeometryTest, RemovesRedundantPolygonClosingPoint)
    {
        const auto geometry =
            opencgm::svg::ApsGeometry::normalizeRegion({
                opencgm::CGMPoint(0.0, 0.0),
                opencgm::CGMPoint(10.0, 0.0),
                opencgm::CGMPoint(10.0, 10.0),
                opencgm::CGMPoint(0.0, 0.0)});

        EXPECT_EQ(geometry.metadata_points.size(), 4U);
        EXPECT_EQ(geometry.polygon_points.size(), 3U);
        EXPECT_EQ(
            geometry.polygon_value,
            "0,0 10,0 10,10");
    }
}
