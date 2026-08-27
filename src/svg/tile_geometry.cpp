#include "opencgm/svg/tile_geometry.h"

#include <algorithm>
#include <cmath>

namespace opencgm::svg
{
    namespace
    {
        double positiveFiniteOr(
            double preferred,
            double secondary,
            double fallback)
        {
            if (std::isfinite(preferred) && preferred > 0.0)
            {
                return preferred;
            }
            if (std::isfinite(secondary) && secondary > 0.0)
            {
                return secondary;
            }
            if (std::isfinite(fallback) && fallback > 0.0)
            {
                return fallback;
            }
            return 1.0;
        }

        double unitsPerCell(double value)
        {
            const double absoluteValue = std::abs(value);
            return std::isfinite(absoluteValue) &&
                           absoluteValue > 0.0
                       ? 1.0 / absoluteValue
                       : 0.0;
        }

        double fallbackUnits(double referenceSpan, int cells)
        {
            if (!std::isfinite(referenceSpan) ||
                referenceSpan <= 0.0 ||
                cells <= 0)
            {
                return 0.0;
            }
            return referenceSpan / static_cast<double>(cells);
        }

        int resolvePixelCount(
            int imageCells,
            int preferred,
            int perTile,
            int fallbackCells)
        {
            if (imageCells > 0)
            {
                return imageCells;
            }
            if (preferred > 0)
            {
                return preferred;
            }
            if (perTile > 0)
            {
                return perTile;
            }
            return std::max(fallbackCells, 1);
        }

        CGMPoint add(
            const CGMPoint &left,
            const CGMPoint &right)
        {
            return CGMPoint(
                left.x() + right.x(),
                left.y() + right.y());
        }

        CGMPoint scale(const CGMPoint &point, double factor)
        {
            return CGMPoint(
                point.x() * factor,
                point.y() * factor);
        }
    }

    TileGeometry TileGeometryResolver::resolve(
        const TileGeometryInput &input)
    {
        TileGeometry geometry;
        geometry.preferredWidth = input.preferred_width;
        geometry.preferredHeight = input.preferred_height;
        geometry.tileWidthCellCount = std::max(
            1,
            input.image_cells_path > 0
                ? input.image_cells_path
                : (geometry.preferredWidth > 0
                       ? geometry.preferredWidth
                       : input.cells_per_tile_path));
        geometry.tileHeightCellCount = std::max(
            1,
            input.image_cells_line > 0
                ? input.image_cells_line
                : (geometry.preferredHeight > 0
                       ? geometry.preferredHeight
                       : input.cells_per_tile_line));
        geometry.activeWidthCells = std::max(
            1,
            input.image_cells_path > 0
                ? input.image_cells_path
                : geometry.tileWidthCellCount);
        geometry.activeHeightCells = std::max(
            1,
            input.image_cells_line > 0
                ? input.image_cells_line
                : geometry.tileHeightCellCount);

        geometry.referenceWidth = positiveFiniteOr(
            input.picture_width,
            input.picture_width_raw,
            input.viewbox_width);
        geometry.referenceHeight = positiveFiniteOr(
            input.picture_height,
            input.picture_height_raw,
            input.viewbox_height);

        geometry.unitsPerCellPath =
            unitsPerCell(input.cell_size_path);
        geometry.unitsPerCellLine =
            unitsPerCell(input.cell_size_line);
        if (geometry.unitsPerCellPath <= 0.0)
        {
            geometry.invalidWidthCellSize = true;
            geometry.unitsPerCellPath = fallbackUnits(
                geometry.referenceWidth,
                geometry.activeWidthCells);
            geometry.usedWidthFallback = true;
        }
        if (geometry.unitsPerCellLine <= 0.0)
        {
            geometry.invalidHeightCellSize = true;
            geometry.unitsPerCellLine = fallbackUnits(
                geometry.referenceHeight,
                geometry.activeHeightCells);
        }

        geometry.totalWidthUnits =
            geometry.unitsPerCellPath *
            static_cast<double>(geometry.activeWidthCells);
        geometry.totalHeightUnits =
            geometry.unitsPerCellLine *
            static_cast<double>(geometry.activeHeightCells);

        const bool widthTooSmall =
            input.apply_malformed_size_heuristics &&
            geometry.totalWidthUnits > 0.0 &&
            geometry.totalWidthUnits <
                geometry.referenceWidth * 0.01;
        const bool heightTooSmall =
            input.apply_malformed_size_heuristics &&
            geometry.totalHeightUnits > 0.0 &&
            geometry.totalHeightUnits <
                geometry.referenceHeight * 0.01;

        if (geometry.unitsPerCellPath <= 0.0 ||
            geometry.totalWidthUnits <= 0.0 ||
            widthTooSmall)
        {
            geometry.usedWidthHeuristic = true;
            const double estimatedWidth =
                geometry.referenceWidth *
                (input.apply_malformed_size_heuristics
                     ? 0.55
                     : 1.0);
            geometry.totalWidthUnits =
                fallbackUnits(
                    estimatedWidth,
                    geometry.activeWidthCells) *
                static_cast<double>(geometry.activeWidthCells);
            geometry.unitsPerCellPath =
                geometry.totalWidthUnits /
                static_cast<double>(geometry.activeWidthCells);
            geometry.usedWidthFallback = true;
        }
        if (geometry.unitsPerCellLine <= 0.0 ||
            geometry.totalHeightUnits <= 0.0 ||
            heightTooSmall)
        {
            geometry.usedHeightHeuristic = true;
            const double estimatedHeight =
                geometry.referenceHeight *
                (input.apply_malformed_size_heuristics
                     ? 0.40
                     : 1.0);
            geometry.totalHeightUnits =
                fallbackUnits(
                    estimatedHeight,
                    geometry.activeHeightCells) *
                static_cast<double>(geometry.activeHeightCells);
            geometry.unitsPerCellLine =
                geometry.totalHeightUnits /
                static_cast<double>(geometry.activeHeightCells);
        }

        const int tileIndex =
            input.in_tile_array ? std::max(input.tile_index, 0) : 0;
        const int tilesPerRow = std::max(1, input.tiles_in_path);
        const int tileColumn =
            input.in_tile_array ? tileIndex % tilesPerRow : 0;
        const int tileRow =
            input.in_tile_array ? tileIndex / tilesPerRow : 0;
        const double offsetCellsPath =
            static_cast<double>(input.image_offset_path) +
            tileColumn *
                static_cast<double>(geometry.tileWidthCellCount);
        const double offsetCellsLine =
            static_cast<double>(input.image_offset_line) +
            tileRow *
                static_cast<double>(geometry.tileHeightCellCount);

        geometry.pixelWidth = resolvePixelCount(
            input.image_cells_path,
            geometry.preferredWidth,
            input.cells_per_tile_path,
            geometry.activeWidthCells);
        geometry.pixelHeight = resolvePixelCount(
            input.image_cells_line,
            geometry.preferredHeight,
            input.cells_per_tile_line,
            geometry.activeHeightCells);

        const CGMPoint pathUnit = directionVector(
            input.path_direction,
            input.flip_y);
        const CGMPoint lineUnit = directionVector(
            input.line_direction,
            input.flip_y);
        geometry.origin = input.in_tile_array
                              ? input.array_position
                              : CGMPoint(0.0, 0.0);
        if (input.in_tile_array)
        {
            geometry.origin = add(
                geometry.origin,
                scale(
                    pathUnit,
                    geometry.unitsPerCellPath *
                        offsetCellsPath));
            geometry.origin = add(
                geometry.origin,
                scale(
                    lineUnit,
                    geometry.unitsPerCellLine *
                        offsetCellsLine));
        }
        geometry.pathVector =
            scale(pathUnit, geometry.totalWidthUnits);
        geometry.lineVector =
            scale(lineUnit, geometry.totalHeightUnits);
        return geometry;
    }

    CGMPoint TileGeometryResolver::directionVector(
        int direction,
        bool flipY)
    {
        int normalized = direction % 4;
        if (normalized < 0)
        {
            normalized += 4;
        }

        switch (normalized)
        {
            case 0:
                return CGMPoint(1.0, 0.0);
            case 1:
                return flipY
                           ? CGMPoint(0.0, -1.0)
                           : CGMPoint(0.0, 1.0);
            case 2:
                return CGMPoint(-1.0, 0.0);
            case 3:
                return flipY
                           ? CGMPoint(0.0, 1.0)
                           : CGMPoint(0.0, -1.0);
            default:
                return CGMPoint(1.0, 0.0);
        }
    }
}
