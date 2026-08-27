#ifndef OPENCGM_SVG_TILE_GEOMETRY_H
#define OPENCGM_SVG_TILE_GEOMETRY_H

#include "opencgm/cgm_point.h"

namespace opencgm::svg
{
    struct TileGeometryInput
    {
        int preferred_width = 0;
        int preferred_height = 0;
        int image_cells_path = 0;
        int image_cells_line = 0;
        int cells_per_tile_path = 1;
        int cells_per_tile_line = 1;

        double picture_width = 0.0;
        double picture_height = 0.0;
        double picture_width_raw = 0.0;
        double picture_height_raw = 0.0;
        double viewbox_width = 1.0;
        double viewbox_height = 1.0;
        double cell_size_path = 1.0;
        double cell_size_line = 1.0;

        bool apply_malformed_size_heuristics = false;
        bool in_tile_array = false;
        int tile_index = 0;
        int tiles_in_path = 1;
        int image_offset_path = 0;
        int image_offset_line = 0;
        CGMPoint array_position;
        int path_direction = 0;
        int line_direction = 1;
        bool flip_y = false;
    };

    struct TileGeometry
    {
        int preferredWidth = 0;
        int preferredHeight = 0;
        int tileWidthCellCount = 1;
        int tileHeightCellCount = 1;
        int activeWidthCells = 1;
        int activeHeightCells = 1;
        int pixelWidth = 1;
        int pixelHeight = 1;
        double referenceWidth = 1.0;
        double referenceHeight = 1.0;
        double unitsPerCellPath = 1.0;
        double unitsPerCellLine = 1.0;
        double totalWidthUnits = 1.0;
        double totalHeightUnits = 1.0;
        bool usedWidthFallback = false;
        bool invalidWidthCellSize = false;
        bool invalidHeightCellSize = false;
        bool usedWidthHeuristic = false;
        bool usedHeightHeuristic = false;
        CGMPoint origin;
        CGMPoint pathVector;
        CGMPoint lineVector;
    };

    /**
     * Pure tile-array geometry resolution.
     *
     * Parsing, compatibility positioning, coordinate transformation,
     * diagnostics, and SVG emission remain with SVGConverter. Bitmap
     * decoding belongs to TileRasterDecoder.
     */
    class TileGeometryResolver
    {
    public:
        static TileGeometry resolve(const TileGeometryInput &input);
        static CGMPoint directionVector(int direction, bool flipY);
    };
}

#endif
