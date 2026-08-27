#ifndef OPENCGM_SVG_CELL_ARRAY_H
#define OPENCGM_SVG_CELL_ARRAY_H

#include "opencgm/cgm_color.h"

#include <cstddef>
#include <vector>

namespace opencgm::svg
{
    struct CellArrayPixelsInput
    {
        int width = 0;
        int height = 0;
        std::vector<std::vector<Color>> resolved_rows;
        bool apply_transparency = false;
        Color transparent_color = Color::White();
    };

    struct PreparedCellArrayPixels
    {
        std::vector<std::vector<Color>> colors;
        std::size_t total_pixels = 0;
        std::size_t transparent_pixels = 0;
        std::size_t opaque_pixels = 0;
        bool valid = false;
    };

    struct CellArrayPoint
    {
        double x = 0.0;
        double y = 0.0;
    };

    struct CellArrayMatrix
    {
        double a = 0.0;
        double b = 0.0;
        double c = 0.0;
        double d = 0.0;
        double e = 0.0;
        double f = 0.0;
    };

    struct CellArrayPlacementInput
    {
        int pixel_width = 0;
        int pixel_height = 0;
        CellArrayPoint origin;
        CellArrayPoint row_end;
        CellArrayPoint column_end;
        CellArrayPoint diagonal;
    };

    struct CellArrayPlacement
    {
        CellArrayMatrix matrix;
        bool use_matrix = false;
        double fallback_x = 0.0;
        double fallback_y = 0.0;
        double fallback_width = 0.0;
        double fallback_height = 0.0;
        bool fallback_valid = false;
    };

    /**
     * Pure CELL ARRAY pixel normalization and SVG placement resolution.
     *
     * CGM colour lookup, diagnostics, image encoding, metrics collection, and
     * SVG serialization remain with SVGConverter.
     */
    class CellArrayPreparer
    {
    public:
        static PreparedCellArrayPixels preparePixels(
            CellArrayPixelsInput input);

        static CellArrayPlacement resolvePlacement(
            const CellArrayPlacementInput &input);
    };
}

#endif
