#ifndef OPENCGM_SVG_COLOR_RESOLVER_H
#define OPENCGM_SVG_COLOR_RESOLVER_H

#include "opencgm/cgm_color.h"

#include <map>

namespace opencgm::svg
{
    enum class ColorRole
    {
        Stroke,
        Fill,
        Edge,
        Text,
        Marker,
        Pattern,
        Raster
    };

    enum class PaletteOverrideMode
    {
        None,
        Monochrome,
        Custom
    };

    struct ColorOverride
    {
        PaletteOverrideMode mode = PaletteOverrideMode::None;
        const std::map<int, Color> *custom_palette = nullptr;
        bool apply_to_fills = false;
    };

    struct ColorResolution
    {
        Color color;
        int index = -1;
        bool indexed = false;
        bool from_table = false;
        bool override_applied = false;
    };

    /**
     * Pure indexed/direct colour resolution for SVG output.
     *
     * Colour-table mutation, converter state, and diagnostics remain with
     * SVGConverter. Table entries are expected to have already been scaled
     * through the active colour value extent.
     */
    class ColorResolver
    {
    public:
        static ColorResolution resolve(
            const CGMColor &cgm_color,
            ColorRole role,
            const std::map<int, Color> &color_table,
            const ColorOverride &override_config,
            const Color &extent_minimum,
            const Color &extent_maximum);

        static Color applyValueExtent(
            const Color &value,
            const Color &extent_minimum,
            const Color &extent_maximum);

        static Color fallbackIndexedColor(int index);
        static bool isFillRole(ColorRole role);
        static const char *roleName(ColorRole role);
    };
}

#endif
