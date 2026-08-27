#include "opencgm/svg/color_resolver.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace opencgm::svg
{
    namespace
    {
        uint8_t scaleChannel(
            uint8_t channel,
            uint8_t minimum,
            uint8_t maximum)
        {
            if (maximum <= minimum)
            {
                return channel;
            }

            double normalized =
                (static_cast<double>(channel) -
                 static_cast<double>(minimum)) /
                static_cast<double>(maximum - minimum);
            normalized = std::clamp(normalized, 0.0, 1.0);
            return static_cast<uint8_t>(
                std::round(normalized * 255.0));
        }
    }

    ColorResolution ColorResolver::resolve(
        const CGMColor &cgm_color,
        ColorRole role,
        const std::map<int, Color> &color_table,
        const ColorOverride &override_config,
        const Color &extent_minimum,
        const Color &extent_maximum)
    {
        ColorResolution resolution;
        resolution.indexed = cgm_color.isIndexed();

        if (resolution.indexed)
        {
            resolution.index = cgm_color.colorIndex();
            const auto table_entry =
                color_table.find(resolution.index);
            if (table_entry != color_table.end())
            {
                resolution.color = table_entry->second;
                resolution.from_table = true;
            }
            else
            {
                resolution.color = applyValueExtent(
                    fallbackIndexedColor(resolution.index),
                    extent_minimum,
                    extent_maximum);
            }
        }
        else
        {
            resolution.color = applyValueExtent(
                cgm_color.color(),
                extent_minimum,
                extent_maximum);
        }

        const bool may_override =
            !isFillRole(role) || override_config.apply_to_fills;
        if (!may_override)
        {
            return resolution;
        }

        if (override_config.mode ==
            PaletteOverrideMode::Monochrome)
        {
            resolution.color = isFillRole(role)
                                   ? Color::White()
                                   : Color::Black();
            resolution.override_applied = true;
        }
        else if (
            resolution.indexed &&
            override_config.mode ==
                PaletteOverrideMode::Custom &&
            override_config.custom_palette != nullptr)
        {
            const auto custom_entry =
                override_config.custom_palette->find(
                    resolution.index);
            if (custom_entry !=
                override_config.custom_palette->end())
            {
                resolution.color = custom_entry->second;
                resolution.override_applied = true;
            }
        }

        return resolution;
    }

    Color ColorResolver::applyValueExtent(
        const Color &value,
        const Color &extent_minimum,
        const Color &extent_maximum)
    {
        Color scaled = value;
        scaled.r = scaleChannel(
            value.r,
            extent_minimum.r,
            extent_maximum.r);
        scaled.g = scaleChannel(
            value.g,
            extent_minimum.g,
            extent_maximum.g);
        scaled.b = scaleChannel(
            value.b,
            extent_minimum.b,
            extent_maximum.b);
        return scaled;
    }

    Color ColorResolver::fallbackIndexedColor(int index)
    {
        static const Color fallback_palette[8] = {
            Color(255, 255, 255),
            Color(0, 0, 0),
            Color(255, 0, 0),
            Color(0, 255, 0),
            Color(0, 0, 255),
            Color(255, 255, 0),
            Color(255, 0, 255),
            Color(0, 255, 255)};

        return index >= 0 && index < 8
                   ? fallback_palette[index]
                   : Color::Black();
    }

    bool ColorResolver::isFillRole(ColorRole role)
    {
        return role == ColorRole::Fill ||
               role == ColorRole::Pattern ||
               role == ColorRole::Raster;
    }

    const char *ColorResolver::roleName(ColorRole role)
    {
        switch (role)
        {
        case ColorRole::Stroke:
            return "stroke";
        case ColorRole::Fill:
            return "fill";
        case ColorRole::Edge:
            return "edge";
        case ColorRole::Text:
            return "text";
        case ColorRole::Marker:
            return "marker";
        case ColorRole::Pattern:
            return "pattern";
        case ColorRole::Raster:
            return "raster";
        default:
            return "unknown";
        }
    }
}
