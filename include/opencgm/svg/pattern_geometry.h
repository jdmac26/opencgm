#ifndef OPENCGM_SVG_PATTERN_GEOMETRY_H
#define OPENCGM_SVG_PATTERN_GEOMETRY_H

#include <cstddef>
#include <vector>

namespace opencgm::svg
{
    struct PatternVector
    {
        double x = 0.0;
        double y = 0.0;
    };

    enum class PatternGeometryIssue
    {
        None,
        InvalidGrid,
        CellCountMismatch,
        DegenerateSize,
        ZeroTileStep
    };

    struct PatternGeometryInput
    {
        int columns = 0;
        int rows = 0;
        std::size_t cell_count = 0;
        PatternVector width_vector;
        PatternVector height_vector;
        PatternVector fallback_x_vector{1.0, 0.0};
        PatternVector fallback_y_vector{0.0, 1.0};
        PatternVector reference_point;
        bool has_reference_point = false;
        bool compatibility_mode = false;
    };

    struct PatternGeometry
    {
        int columns = 1;
        int rows = 1;
        std::size_t expected_cells = 1;
        PatternVector width_vector;
        PatternVector height_vector;
        PatternVector width_step;
        PatternVector height_step;
        PatternVector phase;
        double width_length = 1.0;
        double height_length = 1.0;
        bool used_size_fallback = false;
        bool used_step_fallback = false;
        PatternGeometryIssue issue =
            PatternGeometryIssue::None;

        bool valid() const
        {
            return issue == PatternGeometryIssue::None;
        }
    };

    enum class HatchGeometryIssue
    {
        None,
        DegenerateDefinition,
        ParallelSpacing,
        InvalidDirection,
        DegenerateBasis
    };

    struct HatchLine
    {
        double x1 = 0.0;
        double y1 = 0.0;
        double x2 = 0.0;
        double y2 = 0.0;
    };

    struct ParallelHatchInput
    {
        PatternVector direction_vector;
        PatternVector spacing_vector;
        double stroke_width = 1.0;
        PatternVector reference_point;
        bool has_reference_point = false;
    };

    struct StandardHatchInput
    {
        int hatch_index = 1;
        PatternVector x_basis{1.0, 0.0};
        PatternVector y_basis{0.0, 1.0};
        double viewbox_width = 1.0;
        double viewbox_height = 1.0;
        PatternVector reference_point;
        bool has_reference_point = false;
        bool compatibility_mode = false;
    };

    struct HatchGeometry
    {
        PatternVector first_basis;
        PatternVector second_basis;
        PatternVector phase;
        double pattern_width = 1.0;
        double pattern_height = 1.0;
        double stroke_width = 1.0;
        std::vector<HatchLine> lines;
        bool used_basis_fallback = false;
        bool square_line_caps = false;
        HatchGeometryIssue issue =
            HatchGeometryIssue::None;

        bool valid() const
        {
            return issue == HatchGeometryIssue::None;
        }
    };

    /**
     * Pure PATTERN TABLE/PATTERN SIZE geometry validation and normalization.
     *
     * Command state, diagnostics, caching, and SVG serialization remain in
     * SVGConverter.
     */
    class PatternGeometryResolver
    {
    public:
        static PatternGeometry resolve(
            const PatternGeometryInput &input);

        static HatchGeometry resolveParallelHatch(
            const ParallelHatchInput &input);

        static HatchGeometry resolveStandardHatch(
            const StandardHatchInput &input);
    };
}

#endif
