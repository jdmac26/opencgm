#include <gtest/gtest.h>

#include "opencgm/nurbs_approximator.h"

#include <limits>
#include <vector>

namespace
{
    using opencgm::CGMPoint;
    using opencgm::NurbsApproximationOptions;
    using opencgm::NurbsApproximator;

    TEST(NurbsApproximatorTest, BuildsClampedUniformKnotVector)
    {
        const auto knots =
            NurbsApproximator::buildClampedUniformKnots(
                3,
                5,
                2.0,
                8.0);

        EXPECT_EQ(
            knots,
            (std::vector<double>{
                2.0, 2.0, 2.0,
                4.0, 6.0,
                8.0, 8.0, 8.0}));
    }

    TEST(NurbsApproximatorTest, RejectsInvalidImplicitKnotInputs)
    {
        EXPECT_TRUE(
            NurbsApproximator::buildClampedUniformKnots(
                1, 2, 0.0, 1.0)
                .empty());
        EXPECT_TRUE(
            NurbsApproximator::buildClampedUniformKnots(
                3, 2, 0.0, 1.0)
                .empty());
        EXPECT_TRUE(
            NurbsApproximator::buildClampedUniformKnots(
                2, 2, 1.0, 1.0)
                .empty());
        EXPECT_TRUE(
            NurbsApproximator::buildClampedUniformKnots(
                2,
                2,
                0.0,
                std::numeric_limits<double>::infinity())
                .empty());
    }

    TEST(NurbsApproximatorTest, PreservesValidExplicitKnotVector)
    {
        const std::vector<double> source = {
            0.0, 0.0, 0.5, 1.0, 1.0};

        EXPECT_EQ(
            NurbsApproximator::resolveKnotVector(
                source, 2, 3, 0.0, 1.0),
            source);
    }

    TEST(NurbsApproximatorTest, ReplacesIncompleteKnotVector)
    {
        EXPECT_EQ(
            NurbsApproximator::resolveKnotVector(
                {0.0, 1.0},
                2,
                3,
                0.0,
                1.0),
            (std::vector<double>{
                0.0, 0.0, 0.5, 1.0, 1.0}));
    }

    TEST(NurbsApproximatorTest, RejectsInvalidExplicitKnotVector)
    {
        EXPECT_TRUE(
            NurbsApproximator::resolveKnotVector(
                {
                    0.0,
                    0.0,
                    std::numeric_limits<double>::quiet_NaN(),
                    1.0,
                    1.0},
                2,
                3,
                0.0,
                1.0)
                .empty());
        EXPECT_TRUE(
            NurbsApproximator::resolveKnotVector(
                {0.0, 0.5, 0.25, 1.0, 1.0},
                2,
                3,
                0.0,
                1.0)
                .empty());
    }

    TEST(NurbsApproximatorTest, ApproximatesValidLinearSpline)
    {
        NurbsApproximator approximator;
        const std::vector<CGMPoint> controlPoints = {
            CGMPoint(0.0, 0.0),
            CGMPoint(10.0, 5.0)};
        const std::vector<double> knots = {
            0.0, 0.0, 1.0, 1.0};

        const auto segments = approximator.approximateNurbs(
            2,
            controlPoints,
            {},
            knots,
            0.0,
            1.0);

        ASSERT_FALSE(segments.empty());
        EXPECT_NEAR(segments.front().p0.x(), 0.0, 1e-9);
        EXPECT_NEAR(segments.front().p0.y(), 0.0, 1e-9);
        EXPECT_NEAR(segments.back().p3.x(), 10.0, 1e-9);
        EXPECT_NEAR(segments.back().p3.y(), 5.0, 1e-9);
    }

    TEST(NurbsApproximatorTest, RejectsStructurallyInvalidSplines)
    {
        NurbsApproximator approximator;
        const std::vector<CGMPoint> controlPoints = {
            CGMPoint(0.0, 0.0),
            CGMPoint(10.0, 5.0)};

        EXPECT_TRUE(approximator.approximateNurbs(
            3,
            controlPoints,
            {},
            {0.0, 0.0, 0.0, 1.0, 1.0},
            0.0,
            1.0)
                        .empty());
        EXPECT_TRUE(approximator.approximateNurbs(
            2,
            controlPoints,
            {},
            {0.0, 0.0, 1.0},
            0.0,
            1.0)
                        .empty());
        EXPECT_TRUE(approximator.approximateNurbs(
            2,
            controlPoints,
            {},
            {0.0, 1.0, 0.5, 1.0},
            0.0,
            1.0)
                        .empty());
    }

    TEST(NurbsApproximatorTest, RejectsNonFiniteAndInvalidOptions)
    {
        NurbsApproximator approximator;
        const std::vector<CGMPoint> controlPoints = {
            CGMPoint(0.0, 0.0),
            CGMPoint(10.0, 5.0)};
        const std::vector<double> knots = {
            0.0, 0.0, 1.0, 1.0};

        NurbsApproximationOptions options;
        options.maxSubdivisions = 0;
        EXPECT_TRUE(approximator.approximateNurbs(
            2,
            controlPoints,
            {},
            knots,
            0.0,
            1.0,
            options)
                        .empty());

        options.maxSubdivisions = 100;
        EXPECT_TRUE(approximator.approximateNurbs(
            2,
            controlPoints,
            {
                1.0,
                std::numeric_limits<double>::quiet_NaN()},
            knots,
            0.0,
            1.0,
            options)
                        .empty());
    }
}
