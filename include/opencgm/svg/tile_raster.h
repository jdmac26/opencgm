#ifndef OPENCGM_SVG_TILE_RASTER_H
#define OPENCGM_SVG_TILE_RASTER_H

#include "opencgm/cgm_color.h"
#include "opencgm/enums.h"

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace opencgm::svg
{
    struct TileBitmapFormat
    {
        int width = 0;
        int height = 0;
        ColorSelectionMode selection_mode =
            ColorSelectionMode::INDEXED;
        ColorModel color_model = ColorModel::RGB;
        int local_color_precision = 0;
        int direct_color_precision = 0;
        int color_index_precision = 0;
    };

    /**
     * Bounds-checked CGM BITMAP decoding and BMP construction.
     *
     * Platform image transcoding, colour-table ownership, diagnostics,
     * metrics, and SVG serialization remain with SVGConverter.
     */
    class TileRasterDecoder
    {
    public:
        static std::optional<std::vector<uint8_t>>
        buildBitonalBmp(
            const std::vector<uint8_t> &source,
            int width,
            int height,
            const Color &background,
            const Color &foreground);

        static std::optional<std::vector<uint8_t>>
        decodeBitmapToBmp(
            const std::vector<uint8_t> &source,
            const TileBitmapFormat &format,
            const std::map<int, Color> &colorTable);
    };
}

#endif
