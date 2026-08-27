#include <gtest/gtest.h>

#include "opencgm/svg/color_resolver.h"

#include <map>
#include <string>

namespace
{
    using opencgm::CGMColor;
    using opencgm::Color;
    using opencgm::svg::ColorOverride;
    using opencgm::svg::ColorResolver;
    using opencgm::svg::ColorRole;
    using opencgm::svg::PaletteOverrideMode;

    const Color default_minimum = Color::Black();
    const Color default_maximum = Color::White();

    TEST(ColorResolverTest, ResolvesIndexedTableEntryWithoutRescaling)
    {
        const std::map<int, Color> table = {
            {12, Color(40, 80, 120, 64)}};

        const auto resolution = ColorResolver::resolve(
            CGMColor(12),
            ColorRole::Stroke,
            table,
            ColorOverride{},
            Color(20, 20, 20),
            Color(120, 120, 120));

        EXPECT_EQ(
            resolution.color,
            Color(40, 80, 120, 64));
        EXPECT_EQ(resolution.index, 12);
        EXPECT_TRUE(resolution.indexed);
        EXPECT_TRUE(resolution.from_table);
        EXPECT_FALSE(resolution.override_applied);
    }

    TEST(ColorResolverTest, UsesStandardIndexedFallbackPalette)
    {
        const std::map<int, Color> table;

        const auto yellow = ColorResolver::resolve(
            CGMColor(5),
            ColorRole::Stroke,
            table,
            ColorOverride{},
            default_minimum,
            default_maximum);
        const auto unknown = ColorResolver::resolve(
            CGMColor(99),
            ColorRole::Stroke,
            table,
            ColorOverride{},
            default_minimum,
            default_maximum);

        EXPECT_EQ(yellow.color, Color(255, 255, 0));
        EXPECT_FALSE(yellow.from_table);
        EXPECT_EQ(unknown.color, Color::Black());
        EXPECT_FALSE(unknown.from_table);
    }

    TEST(ColorResolverTest, ScalesDirectColorThroughValueExtent)
    {
        const auto resolution = ColorResolver::resolve(
            CGMColor(Color(10, 60, 120, 77)),
            ColorRole::Text,
            {},
            ColorOverride{},
            Color(10, 10, 20),
            Color(110, 110, 120));

        EXPECT_EQ(resolution.color, Color(0, 128, 255, 77));
        EXPECT_FALSE(resolution.indexed);
        EXPECT_EQ(resolution.index, -1);
        EXPECT_FALSE(resolution.from_table);

        EXPECT_EQ(
            ColorResolver::applyValueExtent(
                Color(30, 40, 50, 60),
                Color(100, 100, 100),
                Color(50, 50, 50)),
            Color(30, 40, 50, 60));
    }

    TEST(ColorResolverTest, MonochromeOverrideRespectsFillPolicy)
    {
        ColorOverride override_config;
        override_config.mode =
            PaletteOverrideMode::Monochrome;

        const auto stroke = ColorResolver::resolve(
            CGMColor(Color::Red()),
            ColorRole::Stroke,
            {},
            override_config,
            default_minimum,
            default_maximum);
        const auto fill_unchanged = ColorResolver::resolve(
            CGMColor(Color::Red()),
            ColorRole::Fill,
            {},
            override_config,
            default_minimum,
            default_maximum);

        override_config.apply_to_fills = true;
        const auto fill_overridden = ColorResolver::resolve(
            CGMColor(Color::Red()),
            ColorRole::Fill,
            {},
            override_config,
            default_minimum,
            default_maximum);

        EXPECT_EQ(stroke.color, Color::Black());
        EXPECT_TRUE(stroke.override_applied);
        EXPECT_EQ(fill_unchanged.color, Color::Red());
        EXPECT_FALSE(fill_unchanged.override_applied);
        EXPECT_EQ(fill_overridden.color, Color::White());
        EXPECT_TRUE(fill_overridden.override_applied);
    }

    TEST(ColorResolverTest, CustomOverrideAppliesOnlyToIndexedColors)
    {
        ColorOverride override_config;
        override_config.mode = PaletteOverrideMode::Custom;
        const std::map<int, Color> custom_palette = {
            {3, Color(9, 8, 7, 6)}};
        override_config.custom_palette = &custom_palette;

        const auto indexed = ColorResolver::resolve(
            CGMColor(3),
            ColorRole::Edge,
            {},
            override_config,
            default_minimum,
            default_maximum);
        const auto direct = ColorResolver::resolve(
            CGMColor(Color(1, 2, 3)),
            ColorRole::Edge,
            {},
            override_config,
            default_minimum,
            default_maximum);
        const auto fill = ColorResolver::resolve(
            CGMColor(3),
            ColorRole::Raster,
            {},
            override_config,
            default_minimum,
            default_maximum);

        EXPECT_EQ(indexed.color, Color(9, 8, 7, 6));
        EXPECT_TRUE(indexed.override_applied);
        EXPECT_EQ(direct.color, Color(1, 2, 3));
        EXPECT_FALSE(direct.override_applied);
        EXPECT_EQ(fill.color, Color(0, 255, 0));
        EXPECT_FALSE(fill.override_applied);
    }

    TEST(ColorResolverTest, ExposesStableRoleNames)
    {
        EXPECT_EQ(
            std::string(ColorResolver::roleName(ColorRole::Stroke)),
            "stroke");
        EXPECT_EQ(
            std::string(ColorResolver::roleName(ColorRole::Pattern)),
            "pattern");
        EXPECT_TRUE(
            ColorResolver::isFillRole(ColorRole::Raster));
        EXPECT_FALSE(
            ColorResolver::isFillRole(ColorRole::Marker));
    }
}
