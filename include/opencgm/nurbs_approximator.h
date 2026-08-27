#ifndef OPENCGM_NURBS_APPROXIMATOR_H
#define OPENCGM_NURBS_APPROXIMATOR_H

#include "cgm_point.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <functional>
#include <sstream>
#include <iomanip>

namespace opencgm {

/**
 * @brief NURBS approximation mode for SVG output
 */
enum class NurbsApproximationMode {
    None,       // Don't process NURBS (may fail for WebCGM profiles)
    ToBezier,   // Convert to cubic Bezier curves (recommended for ATA)
    ToPolyline  // Sample as polyline (default, simple but less accurate)
};

/**
 * @brief Cubic Bezier segment with four control points
 */
struct CubicBezierSegment {
    CGMPoint p0;  // Start point
    CGMPoint p1;  // First control point
    CGMPoint p2;  // Second control point
    CGMPoint p3;  // End point
};

/**
 * @brief Quadratic Bezier segment with three control points
 */
struct QuadraticBezierSegment {
    CGMPoint p0;  // Start point
    CGMPoint p1;  // Control point
    CGMPoint p2;  // End point
};

/**
 * @brief Approximation options for NURBS conversion
 */
struct NurbsApproximationOptions {
    double tolerance = 0.1;          // Units in VDC - maximum deviation from curve
    int maxSubdivisions = 100;       // Maximum number of Bezier segments per NURBS
    bool adaptiveSampling = true;    // Use adaptive subdivision based on curvature
};

/**
 * @brief NURBS to Bezier approximator
 *
 * Converts NURBS (Non-Uniform Rational B-Spline) curves to cubic Bezier curves
 * for SVG output, since SVG doesn't natively support NURBS.
 *
 * This is especially important for ATA iSpec 2200 files which may contain NURBS
 * that need to be preserved with high fidelity during conversion.
 */
class NurbsApproximator {
public:
    /**
     * @brief Build a clamped uniform knot vector for an implicit CGM spline.
     *
     * @return An empty vector when the order, control-point count, or
     * parameter range is invalid.
     */
    static std::vector<double> buildClampedUniformKnots(
        int order,
        size_t numControlPoints,
        double startParam,
        double endParam);

    /**
     * @brief Validate an explicit knot vector or generate its implicit form.
     *
     * A complete source vector must be finite and nondecreasing. Omitted or
     * incomplete vectors use the clamped-uniform CGM fallback.
     */
    static std::vector<double> resolveKnotVector(
        const std::vector<double>& source,
        int order,
        size_t numControlPoints,
        double startParam,
        double endParam);

    /**
     * @brief Approximate a NURBS curve with cubic Bezier segments
     *
     * @param order Spline order (degree + 1)
     * @param controlPoints NURBS control points
     * @param weights Control point weights (for rational B-splines)
     * @param knots Knot vector
     * @param startParam Start parameter value
     * @param endParam End parameter value
     * @param options Approximation options
     * @return Vector of cubic Bezier segments approximating the NURBS
     */
    std::vector<CubicBezierSegment> approximateNurbs(
        int order,
        const std::vector<CGMPoint>& controlPoints,
        const std::vector<double>& weights,
        const std::vector<double>& knots,
        double startParam,
        double endParam,
        const NurbsApproximationOptions& options = NurbsApproximationOptions());

    /**
     * @brief Convert a parabolic arc to a quadratic Bezier (exact conversion)
     *
     * @param start Start point of the arc
     * @param end End point of the arc
     * @param intermediate An intermediate point on the arc
     * @return Quadratic Bezier segment representing the parabolic arc
     */
    QuadraticBezierSegment approximateParabolicArc(
        const CGMPoint& start,
        const CGMPoint& end,
        const CGMPoint& intermediate);

    /**
     * @brief Generate SVG path data for a sequence of cubic Bezier segments
     *
     * @param segments The Bezier segments
     * @param transform Coordinate transform function
     * @return SVG path data string (e.g., "M x y C x1 y1 x2 y2 x3 y3 ...")
     */
    template<typename TransformFunc>
    std::string generateSvgPath(
        const std::vector<CubicBezierSegment>& segments,
        TransformFunc transform) const;

    /**
     * @brief Generate SVG path data for a quadratic Bezier
     */
    template<typename TransformFunc>
    std::string generateSvgPath(
        const QuadraticBezierSegment& segment,
        TransformFunc transform) const;

private:
    /**
     * @brief Evaluate NURBS curve at parameter t using de Boor's algorithm
     */
    CGMPoint evaluateNurbs(
        double t,
        int order,
        const std::vector<CGMPoint>& controlPoints,
        const std::vector<double>& weights,
        const std::vector<double>& knots) const;

    /**
     * @brief Find the knot span index for parameter t
     */
    int findKnotSpan(
        double t,
        int order,
        int numControlPoints,
        const std::vector<double>& knots) const;

    /**
     * @brief Compute B-spline basis functions at parameter t
     */
    void computeBasisFunctions(
        double t,
        int span,
        int order,
        const std::vector<double>& knots,
        std::vector<double>& basis) const;

    /**
     * @brief Fit a cubic Bezier to a curve segment using endpoint tangents
     *
     * @param curve Evaluator function for the curve
     * @param t0 Start parameter
     * @param t1 End parameter
     * @return Approximating cubic Bezier segment
     */
    CubicBezierSegment fitCubicBezier(
        const std::function<CGMPoint(double)>& curve,
        double t0,
        double t1) const;

    /**
     * @brief Recursively subdivide and fit Beziers until tolerance is met
     */
    void adaptiveSubdivide(
        const std::function<CGMPoint(double)>& curve,
        double t0,
        double t1,
        double tolerance,
        int maxDepth,
        std::vector<CubicBezierSegment>& result) const;

    /**
     * @brief Calculate maximum deviation between curve and Bezier approximation
     */
    double maxDeviation(
        const std::function<CGMPoint(double)>& curve,
        const CubicBezierSegment& bezier,
        double t0,
        double t1,
        int numSamples = 10) const;

    /**
     * @brief Evaluate a cubic Bezier at parameter t (0 to 1)
     */
    CGMPoint evaluateBezier(const CubicBezierSegment& bezier, double t) const;

    /**
     * @brief Calculate distance between two points
     */
    double distance(const CGMPoint& a, const CGMPoint& b) const {
        double dx = a.x() - b.x();
        double dy = a.y() - b.y();
        return std::sqrt(dx * dx + dy * dy);
    }
};

// Template implementations

template<typename TransformFunc>
std::string NurbsApproximator::generateSvgPath(
    const std::vector<CubicBezierSegment>& segments,
    TransformFunc transform) const
{
    if (segments.empty()) return "";

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);

    // Move to start point
    auto start = transform(segments[0].p0);
    oss << "M " << start.x() << " " << start.y();

    // Output cubic Bezier commands
    for (const auto& seg : segments) {
        auto c1 = transform(seg.p1);
        auto c2 = transform(seg.p2);
        auto end = transform(seg.p3);
        oss << " C " << c1.x() << " " << c1.y()
            << " " << c2.x() << " " << c2.y()
            << " " << end.x() << " " << end.y();
    }

    return oss.str();
}

template<typename TransformFunc>
std::string NurbsApproximator::generateSvgPath(
    const QuadraticBezierSegment& segment,
    TransformFunc transform) const
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);

    auto start = transform(segment.p0);
    auto ctrl = transform(segment.p1);
    auto end = transform(segment.p2);

    oss << "M " << start.x() << " " << start.y()
        << " Q " << ctrl.x() << " " << ctrl.y()
        << " " << end.x() << " " << end.y();

    return oss.str();
}

} // namespace opencgm

#endif // OPENCGM_NURBS_APPROXIMATOR_H
