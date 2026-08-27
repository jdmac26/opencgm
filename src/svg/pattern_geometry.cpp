#include "opencgm/svg/pattern_geometry.h"

#include <algorithm>
#include <cmath>

namespace opencgm::svg
{
    namespace
    {
        constexpr double epsilon = 1e-9;

        double vectorLength(const PatternVector &vector)
        {
            return std::hypot(vector.x, vector.y);
        }

        bool validLength(double length)
        {
            return std::isfinite(length) &&
                   length > epsilon;
        }

        bool validStep(const PatternVector &step)
        {
            return std::isfinite(step.x) &&
                   std::isfinite(step.y) &&
                   std::abs(step.x) + std::abs(step.y) >
                       epsilon;
        }
    }

    PatternGeometry PatternGeometryResolver::resolve(
        const PatternGeometryInput &input)
    {
        PatternGeometry geometry;
        geometry.columns = input.columns;
        geometry.rows = input.rows;

        if (geometry.columns <= 0 || geometry.rows <= 0)
        {
            if (!input.compatibility_mode)
            {
                geometry.issue =
                    PatternGeometryIssue::InvalidGrid;
                return geometry;
            }

            geometry.columns =
                geometry.columns <= 0 ? 1 : geometry.columns;
            geometry.rows =
                geometry.rows <= 0 ? 1 : geometry.rows;
        }

        geometry.expected_cells =
            static_cast<std::size_t>(geometry.columns) *
            static_cast<std::size_t>(geometry.rows);
        if (!input.compatibility_mode &&
            input.cell_count != geometry.expected_cells)
        {
            geometry.issue =
                PatternGeometryIssue::CellCountMismatch;
            return geometry;
        }

        geometry.width_vector = input.width_vector;
        geometry.height_vector = input.height_vector;
        geometry.width_length =
            vectorLength(geometry.width_vector);
        geometry.height_length =
            vectorLength(geometry.height_vector);

        if (!validLength(geometry.width_length) ||
            !validLength(geometry.height_length))
        {
            if (!input.compatibility_mode)
            {
                geometry.issue =
                    PatternGeometryIssue::DegenerateSize;
                return geometry;
            }

            geometry.width_length =
                vectorLength(input.fallback_x_vector);
            geometry.height_length =
                vectorLength(input.fallback_y_vector);
            if (!validLength(geometry.width_length))
            {
                geometry.width_length = 1.0;
            }
            if (!validLength(geometry.height_length))
            {
                geometry.height_length = 1.0;
            }

            geometry.width_vector = {
                geometry.width_length,
                0.0};
            geometry.height_vector = {
                0.0,
                geometry.height_length};
            geometry.used_size_fallback = true;
        }

        if (input.has_reference_point)
        {
            geometry.phase = input.reference_point;
            const double determinant =
                geometry.width_vector.x *
                    geometry.height_vector.y -
                geometry.width_vector.y *
                    geometry.height_vector.x;
            if (std::isfinite(determinant) &&
                std::abs(determinant) > epsilon)
            {
                const double u =
                    (geometry.phase.x *
                         geometry.height_vector.y -
                     geometry.phase.y *
                         geometry.height_vector.x) /
                    determinant;
                const double v =
                    (-geometry.phase.x *
                         geometry.width_vector.y +
                     geometry.phase.y *
                         geometry.width_vector.x) /
                    determinant;
                const double u_floor = std::floor(u);
                const double v_floor = std::floor(v);
                if (std::isfinite(u_floor) &&
                    std::isfinite(v_floor))
                {
                    geometry.phase.x -=
                        u_floor * geometry.width_vector.x +
                        v_floor * geometry.height_vector.x;
                    geometry.phase.y -=
                        u_floor * geometry.width_vector.y +
                        v_floor * geometry.height_vector.y;
                }
            }
        }

        geometry.width_step = {
            geometry.width_vector.x /
                static_cast<double>(geometry.columns),
            geometry.width_vector.y /
                static_cast<double>(geometry.columns)};
        geometry.height_step = {
            geometry.height_vector.x /
                static_cast<double>(geometry.rows),
            geometry.height_vector.y /
                static_cast<double>(geometry.rows)};

        if (!validStep(geometry.width_step) ||
            !validStep(geometry.height_step))
        {
            if (!input.compatibility_mode)
            {
                geometry.issue =
                    PatternGeometryIssue::ZeroTileStep;
                return geometry;
            }

            geometry.width_step = {
                geometry.width_length /
                    static_cast<double>(geometry.columns),
                0.0};
            geometry.height_step = {
                0.0,
                geometry.height_length /
                    static_cast<double>(geometry.rows)};
            geometry.used_step_fallback = true;
        }

        return geometry;
    }

    HatchGeometry PatternGeometryResolver::resolveParallelHatch(
        const ParallelHatchInput &input)
    {
        HatchGeometry geometry;
        geometry.stroke_width = input.stroke_width;
        geometry.square_line_caps = true;
        if (input.has_reference_point)
        {
            geometry.phase = input.reference_point;
        }

        const double direction_length =
            vectorLength(input.direction_vector);
        const double spacing_length =
            vectorLength(input.spacing_vector);
        if (!validLength(direction_length) ||
            !validLength(spacing_length))
        {
            geometry.issue =
                HatchGeometryIssue::DegenerateDefinition;
            return geometry;
        }

        const PatternVector direction_unit{
            input.direction_vector.x / direction_length,
            input.direction_vector.y / direction_length};
        const PatternVector perpendicular{
            -direction_unit.y,
            direction_unit.x};
        const double spacing_distance = std::abs(
            input.spacing_vector.x * perpendicular.x +
            input.spacing_vector.y * perpendicular.y);
        if (!validLength(spacing_distance))
        {
            geometry.issue =
                HatchGeometryIssue::ParallelSpacing;
            return geometry;
        }

        double line_repeat = std::max(
            {spacing_distance * 4.0,
             input.stroke_width * 32.0,
             8.0});
        if (!validLength(line_repeat))
        {
            line_repeat = 8.0;
        }

        geometry.first_basis = input.spacing_vector;
        geometry.second_basis = {
            direction_unit.x * line_repeat,
            direction_unit.y * line_repeat};
        if (!std::isfinite(geometry.second_basis.x) ||
            !std::isfinite(geometry.second_basis.y))
        {
            geometry.issue =
                HatchGeometryIssue::InvalidDirection;
            return geometry;
        }

        constexpr double overshoot = 1.5;
        geometry.lines = {
            {0.0, -overshoot, 0.0, 1.0 + overshoot},
            {1.0, -overshoot, 1.0, 1.0 + overshoot}};
        return geometry;
    }

    HatchGeometry PatternGeometryResolver::resolveStandardHatch(
        const StandardHatchInput &input)
    {
        HatchGeometry geometry;
        geometry.first_basis = input.x_basis;
        geometry.second_basis = input.y_basis;
        if (input.has_reference_point)
        {
            geometry.phase = input.reference_point;
        }

        const double determinant =
            geometry.first_basis.x *
                geometry.second_basis.y -
            geometry.first_basis.y *
                geometry.second_basis.x;
        const bool basis_valid =
            std::isfinite(geometry.first_basis.x) &&
            std::isfinite(geometry.first_basis.y) &&
            std::isfinite(geometry.second_basis.x) &&
            std::isfinite(geometry.second_basis.y) &&
            std::abs(geometry.first_basis.x) +
                    std::abs(geometry.first_basis.y) >
                epsilon &&
            std::abs(geometry.second_basis.x) +
                    std::abs(geometry.second_basis.y) >
                epsilon &&
            std::abs(determinant) > epsilon;
        if (!basis_valid)
        {
            if (!input.compatibility_mode)
            {
                geometry.issue =
                    HatchGeometryIssue::DegenerateBasis;
                return geometry;
            }

            geometry.first_basis = {1.0, 0.0};
            geometry.second_basis = {0.0, 1.0};
            geometry.used_basis_fallback = true;
        }

        const double viewbox_width =
            input.viewbox_width > 0.0
                ? input.viewbox_width
                : 1.0;
        const double viewbox_height =
            input.viewbox_height > 0.0
                ? input.viewbox_height
                : 1.0;
        const double viewbox_diagonal =
            std::sqrt(
                viewbox_width * viewbox_width +
                viewbox_height * viewbox_height);
        // Target roughly 100 hatch lines across the view-box diagonal.
        // This matches the NIST/CGMOpen reference density used by INTSTL04,
        // FIGURE04, and ESCAPE01.
        double pattern_size = viewbox_diagonal / 100.0;
        const double maximum_pattern_size =
            std::min(viewbox_width, viewbox_height) /
            20.0;
        if (pattern_size < 2.0)
        {
            pattern_size = 2.0;
        }
        if (maximum_pattern_size > 2.0 &&
            pattern_size > maximum_pattern_size)
        {
            pattern_size = maximum_pattern_size;
        }

        geometry.pattern_width = pattern_size;
        geometry.pattern_height = pattern_size;
        geometry.stroke_width =
            std::max(pattern_size / 8.0, 1.0);

        const double half = pattern_size / 2.0;
        const double extend = pattern_size / 2.0;
        const HatchLine horizontal{
            0.0,
            half,
            pattern_size,
            half};
        const HatchLine vertical{
            half,
            0.0,
            half,
            pattern_size};
        const HatchLine positive_diagonal{
            -extend,
            -extend,
            pattern_size + extend,
            pattern_size + extend};
        const HatchLine negative_diagonal{
            -extend,
            pattern_size + extend,
            pattern_size + extend,
            -extend};

        // The CGM-to-SVG basis normally flips Y. Emitting these local
        // diagonals in the opposite apparent direction makes indices 3 and 4
        // render with their ISO/IEC 8632-1 visual slope after transformation.
        switch (input.hatch_index)
        {
        case 1:
            geometry.lines = {horizontal};
            break;
        case 2:
            geometry.lines = {vertical};
            break;
        case 3:
            geometry.lines = {positive_diagonal};
            break;
        case 4:
            geometry.lines = {negative_diagonal};
            break;
        case 5:
            geometry.lines = {horizontal, vertical};
            break;
        case 6:
        default:
            geometry.lines = {
                positive_diagonal,
                negative_diagonal};
            break;
        }

        return geometry;
    }
}
