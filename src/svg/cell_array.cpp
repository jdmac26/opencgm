#include "opencgm/svg/cell_array.h"

#include "opencgm/svg/svg_utils.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace opencgm::svg
{
    PreparedCellArrayPixels CellArrayPreparer::preparePixels(
        CellArrayPixelsInput input)
    {
        PreparedCellArrayPixels prepared;
        if (input.width <= 0 || input.height <= 0)
        {
            return prepared;
        }

        prepared.total_pixels =
            static_cast<std::size_t>(input.width) *
            static_cast<std::size_t>(input.height);
        const int available_rows = std::min(
            input.height,
            static_cast<int>(input.resolved_rows.size()));
        input.resolved_rows.resize(
            static_cast<std::size_t>(input.height));
        for (int y = 0; y < input.height; ++y)
        {
            auto &row =
                input.resolved_rows[static_cast<std::size_t>(y)];
            const int available_columns =
                y < available_rows
                    ? std::min(
                          input.width,
                          static_cast<int>(row.size()))
                    : 0;
            for (int x = 0; x < available_columns; ++x)
            {
                Color &color = row[static_cast<std::size_t>(x)];
                if (input.apply_transparency &&
                    colorsEqualRgb(
                        color,
                        input.transparent_color))
                {
                    color.a = 0;
                    ++prepared.transparent_pixels;
                }
                else
                {
                    color.a = 255;
                }
            }
            row.resize(
                static_cast<std::size_t>(input.width),
                Color::White());
        }

        prepared.colors = std::move(input.resolved_rows);
        prepared.opaque_pixels =
            prepared.total_pixels -
            prepared.transparent_pixels;
        prepared.valid = true;
        return prepared;
    }

    CellArrayPlacement CellArrayPreparer::resolvePlacement(
        const CellArrayPlacementInput &input)
    {
        CellArrayPlacement placement;
        if (input.pixel_width <= 0 ||
            input.pixel_height <= 0)
        {
            return placement;
        }

        placement.matrix.a =
            (input.row_end.x - input.origin.x) /
            static_cast<double>(input.pixel_width);
        placement.matrix.b =
            (input.row_end.y - input.origin.y) /
            static_cast<double>(input.pixel_width);
        placement.matrix.c =
            (input.column_end.x - input.origin.x) /
            static_cast<double>(input.pixel_height);
        placement.matrix.d =
            (input.column_end.y - input.origin.y) /
            static_cast<double>(input.pixel_height);
        placement.matrix.e = input.origin.x;
        placement.matrix.f = input.origin.y;

        constexpr double epsilon = 1e-9;
        const bool row_nonzero =
            std::abs(placement.matrix.a) >= epsilon ||
            std::abs(placement.matrix.b) >= epsilon;
        const bool column_nonzero =
            std::abs(placement.matrix.c) >= epsilon ||
            std::abs(placement.matrix.d) >= epsilon;
        placement.use_matrix =
            row_nonzero &&
            column_nonzero &&
            std::isfinite(placement.matrix.a) &&
            std::isfinite(placement.matrix.b) &&
            std::isfinite(placement.matrix.c) &&
            std::isfinite(placement.matrix.d) &&
            std::isfinite(placement.matrix.e) &&
            std::isfinite(placement.matrix.f);
        if (placement.use_matrix)
        {
            return placement;
        }

        const bool corners_finite =
            std::isfinite(input.origin.x) &&
            std::isfinite(input.origin.y) &&
            std::isfinite(input.row_end.x) &&
            std::isfinite(input.row_end.y) &&
            std::isfinite(input.column_end.x) &&
            std::isfinite(input.column_end.y) &&
            std::isfinite(input.diagonal.x) &&
            std::isfinite(input.diagonal.y);
        if (!corners_finite)
        {
            return placement;
        }

        const double minimum_x = std::min(
            std::min(input.origin.x, input.row_end.x),
            std::min(input.column_end.x, input.diagonal.x));
        const double maximum_x = std::max(
            std::max(input.origin.x, input.row_end.x),
            std::max(input.column_end.x, input.diagonal.x));
        const double minimum_y = std::min(
            std::min(input.origin.y, input.row_end.y),
            std::min(input.column_end.y, input.diagonal.y));
        const double maximum_y = std::max(
            std::max(input.origin.y, input.row_end.y),
            std::max(input.column_end.y, input.diagonal.y));

        placement.fallback_x = minimum_x;
        placement.fallback_y = minimum_y;
        placement.fallback_width = maximum_x - minimum_x;
        placement.fallback_height = maximum_y - minimum_y;
        placement.fallback_valid =
            placement.fallback_width > 0.0 &&
            placement.fallback_height > 0.0;
        return placement;
    }
}
