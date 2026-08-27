#include <gtest/gtest.h>

#include "opencgm/svg/pattern_geometry.h"

#include <climits>
#include <cmath>

namespace
{
    using opencgm::svg::PatternGeometryInput;
    using opencgm::svg::PatternGeometryIssue;
    using opencgm::svg::PatternGeometryResolver;
    using opencgm::svg::HatchGeometryIssue;
    using opencgm::svg::ParallelHatchInput;
    using opencgm::svg::StandardHatchInput;

    TEST(PatternGeometryResolverTest, ResolvesSkewedPatternSteps)
    {
        PatternGeometryInput input;
        input.columns = 2;
        input.rows = 4;
        input.cell_count = 8;
        input.width_vector = {8.0, 4.0};
        input.height_vector = {-2.0, 12.0};

        const auto geometry =
            PatternGeometryResolver::resolve(input);

        ASSERT_TRUE(geometry.valid());
        EXPECT_EQ(geometry.columns, 2);
        EXPECT_EQ(geometry.rows, 4);
        EXPECT_EQ(geometry.expected_cells, 8u);
        EXPECT_DOUBLE_EQ(geometry.width_step.x, 4.0);
        EXPECT_DOUBLE_EQ(geometry.width_step.y, 2.0);
        EXPECT_DOUBLE_EQ(geometry.height_step.x, -0.5);
        EXPECT_DOUBLE_EQ(geometry.height_step.y, 3.0);
        EXPECT_FALSE(geometry.used_size_fallback);
        EXPECT_FALSE(geometry.used_step_fallback);
    }

    TEST(PatternGeometryResolverTest, NormalizesReferencePointToTilePhase)
    {
        PatternGeometryInput input;
        input.columns = 2;
        input.rows = 3;
        input.cell_count = 6;
        input.width_vector = {8.0, 0.0};
        input.height_vector = {0.0, -6.0};
        input.reference_point = {18.0, -13.0};
        input.has_reference_point = true;

        const auto geometry =
            PatternGeometryResolver::resolve(input);

        ASSERT_TRUE(geometry.valid());
        EXPECT_DOUBLE_EQ(geometry.phase.x, 2.0);
        EXPECT_DOUBLE_EQ(geometry.phase.y, -1.0);
    }

    TEST(PatternGeometryResolverTest, RejectsInvalidGridInStrictMode)
    {
        PatternGeometryInput input;
        input.columns = 0;
        input.rows = -2;
        input.cell_count = 1;
        input.width_vector = {1.0, 0.0};
        input.height_vector = {0.0, 1.0};

        const auto geometry =
            PatternGeometryResolver::resolve(input);

        EXPECT_EQ(
            geometry.issue,
            PatternGeometryIssue::InvalidGrid);
    }

    TEST(PatternGeometryResolverTest, RepairsInvalidGridInCompatibilityMode)
    {
        PatternGeometryInput input;
        input.columns = 0;
        input.rows = -2;
        input.cell_count = 0;
        input.width_vector = {1.0, 0.0};
        input.height_vector = {0.0, 1.0};
        input.compatibility_mode = true;

        const auto geometry =
            PatternGeometryResolver::resolve(input);

        ASSERT_TRUE(geometry.valid());
        EXPECT_EQ(geometry.columns, 1);
        EXPECT_EQ(geometry.rows, 1);
        EXPECT_EQ(geometry.expected_cells, 1u);
    }

    TEST(PatternGeometryResolverTest, EnforcesCellCountOnlyInStrictMode)
    {
        PatternGeometryInput input;
        input.columns = 3;
        input.rows = 2;
        input.cell_count = 5;
        input.width_vector = {3.0, 0.0};
        input.height_vector = {0.0, 2.0};

        const auto strict =
            PatternGeometryResolver::resolve(input);
        input.compatibility_mode = true;
        const auto compatible =
            PatternGeometryResolver::resolve(input);

        EXPECT_EQ(
            strict.issue,
            PatternGeometryIssue::CellCountMismatch);
        EXPECT_EQ(strict.expected_cells, 6u);
        EXPECT_TRUE(compatible.valid());
    }

    TEST(PatternGeometryResolverTest, UsesAxisAlignedSizeFallback)
    {
        PatternGeometryInput input;
        input.columns = 2;
        input.rows = 2;
        input.cell_count = 4;
        input.width_vector = {0.0, 0.0};
        input.height_vector = {0.0, 0.0};
        input.fallback_x_vector = {3.0, 4.0};
        input.fallback_y_vector = {0.0, 0.0};

        const auto strict =
            PatternGeometryResolver::resolve(input);
        input.compatibility_mode = true;
        const auto compatible =
            PatternGeometryResolver::resolve(input);

        EXPECT_EQ(
            strict.issue,
            PatternGeometryIssue::DegenerateSize);
        ASSERT_TRUE(compatible.valid());
        EXPECT_TRUE(compatible.used_size_fallback);
        EXPECT_DOUBLE_EQ(compatible.width_vector.x, 5.0);
        EXPECT_DOUBLE_EQ(compatible.width_vector.y, 0.0);
        EXPECT_DOUBLE_EQ(compatible.height_vector.x, 0.0);
        EXPECT_DOUBLE_EQ(compatible.height_vector.y, 1.0);
    }

    TEST(PatternGeometryResolverTest, ReportsSubEpsilonTileStep)
    {
        PatternGeometryInput input;
        input.columns = INT_MAX;
        input.rows = 1;
        input.cell_count =
            static_cast<std::size_t>(INT_MAX);
        input.width_vector = {1.0, 0.0};
        input.height_vector = {0.0, 1.0};

        const auto strict =
            PatternGeometryResolver::resolve(input);
        input.compatibility_mode = true;
        const auto compatible =
            PatternGeometryResolver::resolve(input);

        EXPECT_EQ(
            strict.issue,
            PatternGeometryIssue::ZeroTileStep);
        EXPECT_TRUE(compatible.valid());
        EXPECT_TRUE(compatible.used_step_fallback);
    }

    TEST(PatternGeometryResolverTest, ResolvesParallelHatchDefinition)
    {
        ParallelHatchInput input;
        input.direction_vector = {2.0, 0.0};
        input.spacing_vector = {0.0, 3.0};
        input.stroke_width = 0.5;
        input.reference_point = {7.0, 8.0};
        input.has_reference_point = true;

        const auto geometry =
            PatternGeometryResolver::resolveParallelHatch(
                input);

        ASSERT_TRUE(geometry.valid());
        EXPECT_DOUBLE_EQ(geometry.first_basis.x, 0.0);
        EXPECT_DOUBLE_EQ(geometry.first_basis.y, 3.0);
        EXPECT_DOUBLE_EQ(geometry.second_basis.x, 16.0);
        EXPECT_DOUBLE_EQ(geometry.second_basis.y, 0.0);
        EXPECT_DOUBLE_EQ(geometry.phase.x, 7.0);
        EXPECT_DOUBLE_EQ(geometry.phase.y, 8.0);
        EXPECT_DOUBLE_EQ(geometry.stroke_width, 0.5);
        EXPECT_TRUE(geometry.square_line_caps);
        ASSERT_EQ(geometry.lines.size(), 2u);
        EXPECT_DOUBLE_EQ(geometry.lines[0].y1, -1.5);
        EXPECT_DOUBLE_EQ(geometry.lines[0].y2, 2.5);
        EXPECT_DOUBLE_EQ(geometry.lines[1].x1, 1.0);
    }

    TEST(PatternGeometryResolverTest, RejectsInvalidParallelHatchVectors)
    {
        ParallelHatchInput input;
        input.direction_vector = {0.0, 0.0};
        input.spacing_vector = {0.0, 3.0};

        const auto degenerate =
            PatternGeometryResolver::resolveParallelHatch(
                input);

        input.direction_vector = {1.0, 0.0};
        input.spacing_vector = {4.0, 0.0};
        const auto parallel =
            PatternGeometryResolver::resolveParallelHatch(
                input);

        EXPECT_EQ(
            degenerate.issue,
            HatchGeometryIssue::DegenerateDefinition);
        EXPECT_EQ(
            parallel.issue,
            HatchGeometryIssue::ParallelSpacing);
    }

    TEST(PatternGeometryResolverTest, ResolvesStandardHatchDensityAndBasis)
    {
        StandardHatchInput input;
        input.hatch_index = 1;
        input.x_basis = {2.0, 0.0};
        input.y_basis = {0.0, -3.0};
        input.viewbox_width = 1000.0;
        input.viewbox_height = 500.0;
        input.reference_point = {9.0, 10.0};
        input.has_reference_point = true;

        const auto geometry =
            PatternGeometryResolver::resolveStandardHatch(
                input);

        ASSERT_TRUE(geometry.valid());
        EXPECT_DOUBLE_EQ(geometry.first_basis.x, 2.0);
        EXPECT_DOUBLE_EQ(geometry.second_basis.y, -3.0);
        EXPECT_NEAR(
            geometry.pattern_width,
            std::hypot(1000.0, 500.0) / 100.0,
            1e-12);
        EXPECT_NEAR(
            geometry.stroke_width,
            geometry.pattern_width / 8.0,
            1e-12);
        EXPECT_DOUBLE_EQ(geometry.phase.x, 9.0);
        EXPECT_DOUBLE_EQ(geometry.phase.y, 10.0);
        ASSERT_EQ(geometry.lines.size(), 1u);
        EXPECT_DOUBLE_EQ(
            geometry.lines[0].y1,
            geometry.pattern_height / 2.0);
    }

    TEST(PatternGeometryResolverTest, ClampsSmallHatchAndBuildsCrosshatch)
    {
        StandardHatchInput input;
        input.hatch_index = 5;
        input.viewbox_width = 10.0;
        input.viewbox_height = 10.0;

        const auto geometry =
            PatternGeometryResolver::resolveStandardHatch(
                input);

        ASSERT_TRUE(geometry.valid());
        EXPECT_DOUBLE_EQ(geometry.pattern_width, 2.0);
        EXPECT_DOUBLE_EQ(geometry.pattern_height, 2.0);
        EXPECT_DOUBLE_EQ(geometry.stroke_width, 1.0);
        ASSERT_EQ(geometry.lines.size(), 2u);
        EXPECT_DOUBLE_EQ(geometry.lines[0].y1, 1.0);
        EXPECT_DOUBLE_EQ(geometry.lines[1].x1, 1.0);
    }

    TEST(PatternGeometryResolverTest, PreservesDiagonalIndexOrientation)
    {
        StandardHatchInput input;
        input.hatch_index = 3;
        input.viewbox_width = 100.0;
        input.viewbox_height = 100.0;
        const auto positive =
            PatternGeometryResolver::resolveStandardHatch(
                input);

        input.hatch_index = 4;
        const auto negative =
            PatternGeometryResolver::resolveStandardHatch(
                input);

        ASSERT_EQ(positive.lines.size(), 1u);
        ASSERT_EQ(negative.lines.size(), 1u);
        EXPECT_LT(
            positive.lines[0].y1,
            positive.lines[0].y2);
        EXPECT_GT(
            negative.lines[0].y1,
            negative.lines[0].y2);
    }

    TEST(PatternGeometryResolverTest, FallsBackFromDegenerateHatchBasis)
    {
        StandardHatchInput input;
        input.x_basis = {1.0, 0.0};
        input.y_basis = {2.0, 0.0};

        const auto strict =
            PatternGeometryResolver::resolveStandardHatch(
                input);
        input.compatibility_mode = true;
        const auto compatible =
            PatternGeometryResolver::resolveStandardHatch(
                input);

        EXPECT_EQ(
            strict.issue,
            HatchGeometryIssue::DegenerateBasis);
        ASSERT_TRUE(compatible.valid());
        EXPECT_TRUE(compatible.used_basis_fallback);
        EXPECT_DOUBLE_EQ(compatible.first_basis.x, 1.0);
        EXPECT_DOUBLE_EQ(compatible.first_basis.y, 0.0);
        EXPECT_DOUBLE_EQ(compatible.second_basis.x, 0.0);
        EXPECT_DOUBLE_EQ(compatible.second_basis.y, 1.0);
    }
}
