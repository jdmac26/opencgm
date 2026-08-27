#include "opencgm/nurbs_approximator.h"
#include <functional>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <limits>

namespace opencgm {

std::vector<double> NurbsApproximator::buildClampedUniformKnots(
    int order,
    size_t numControlPoints,
    double startParam,
    double endParam)
{
    if (order < 2 ||
        numControlPoints < static_cast<size_t>(order) ||
        !std::isfinite(startParam) ||
        !std::isfinite(endParam) ||
        endParam <= startParam)
    {
        return {};
    }

    const size_t expectedSize =
        static_cast<size_t>(order) + numControlPoints;
    std::vector<double> knots(expectedSize, 0.0);

    const size_t interiorCount =
        numControlPoints - static_cast<size_t>(order);
    const double span = endParam - startParam;
    const double step =
        interiorCount > 0
            ? span / static_cast<double>(interiorCount + 1)
            : 0.0;

    size_t index = 0;
    for (int i = 0; i < order; ++i)
    {
        knots[index++] = startParam;
    }
    for (size_t i = 0; i < interiorCount; ++i)
    {
        knots[index++] =
            startParam + step * static_cast<double>(i + 1);
    }
    while (index < expectedSize)
    {
        knots[index++] = endParam;
    }

    return knots;
}

std::vector<double> NurbsApproximator::resolveKnotVector(
    const std::vector<double>& source,
    int order,
    size_t numControlPoints,
    double startParam,
    double endParam)
{
    if (order < 2 ||
        numControlPoints < static_cast<size_t>(order) ||
        !std::isfinite(startParam) ||
        !std::isfinite(endParam) ||
        endParam <= startParam)
    {
        return {};
    }

    const size_t expectedSize =
        static_cast<size_t>(order) + numControlPoints;
    if (source.size() == expectedSize)
    {
        const bool hasInvalidKnot = std::any_of(
            source.begin(),
            source.end(),
            [](double knot) { return !std::isfinite(knot); });
        const bool isDecreasing = std::adjacent_find(
            source.begin(),
            source.end(),
            [](double left, double right) {
                return left > right;
            }) != source.end();
        if (hasInvalidKnot || isDecreasing)
        {
            return {};
        }
        return source;
    }

    // CGM permits implicit/uniform knot vectors. Use one generator when the
    // source vector is omitted or incomplete.
    return buildClampedUniformKnots(
        order,
        numControlPoints,
        startParam,
        endParam);
}

std::vector<CubicBezierSegment> NurbsApproximator::approximateNurbs(
    int order,
    const std::vector<CGMPoint>& controlPoints,
    const std::vector<double>& weights,
    const std::vector<double>& knots,
    double startParam,
    double endParam,
    const NurbsApproximationOptions& options)
{
    std::vector<CubicBezierSegment> result;

    const bool hasInvalidControlPoint = std::any_of(
        controlPoints.begin(),
        controlPoints.end(),
        [](const CGMPoint& point) {
            return !std::isfinite(point.x()) || !std::isfinite(point.y());
        });
    const bool hasInvalidWeight = std::any_of(
        weights.begin(),
        weights.end(),
        [](double weight) { return !std::isfinite(weight); });
    const bool hasInvalidKnot = std::any_of(
        knots.begin(),
        knots.end(),
        [](double knot) { return !std::isfinite(knot); });
    const bool hasDecreasingKnots = std::adjacent_find(
        knots.begin(),
        knots.end(),
        [](double left, double right) { return left > right; }) != knots.end();

    if (order < 2 ||
        controlPoints.size() < static_cast<size_t>(order) ||
        knots.size() != controlPoints.size() + static_cast<size_t>(order) ||
        !std::isfinite(startParam) ||
        !std::isfinite(endParam) ||
        endParam <= startParam ||
        !std::isfinite(options.tolerance) ||
        options.tolerance < 0.0 ||
        options.maxSubdivisions <= 0 ||
        hasInvalidControlPoint ||
        hasInvalidWeight ||
        hasInvalidKnot ||
        hasDecreasingKnots) {
        return result;
    }

    // Create an evaluator lambda that captures the NURBS parameters
    auto evaluator = [this, order, &controlPoints, &weights, &knots](double t) {
        return evaluateNurbs(t, order, controlPoints, weights, knots);
    };

    if (options.adaptiveSampling) {
        // Use adaptive subdivision for best quality
        int maxDepth = static_cast<int>(std::log2(options.maxSubdivisions)) + 1;
        adaptiveSubdivide(evaluator, startParam, endParam, options.tolerance, maxDepth, result);
    } else {
        // Simple uniform subdivision
        int numSegments = std::min(options.maxSubdivisions,
                                    static_cast<int>(controlPoints.size()) * 2);
        double dt = (endParam - startParam) / numSegments;

        for (int i = 0; i < numSegments; ++i) {
            double t0 = startParam + i * dt;
            double t1 = startParam + (i + 1) * dt;
            result.push_back(fitCubicBezier(evaluator, t0, t1));
        }
    }

    return result;
}

QuadraticBezierSegment NurbsApproximator::approximateParabolicArc(
    const CGMPoint& start,
    const CGMPoint& end,
    const CGMPoint& intermediate)
{
    // For a parabolic arc passing through three points,
    // we can compute the quadratic Bezier control point
    // The control point is at the intersection of tangent lines at start and end

    // For a parabola y = ax^2, the tangent at x is dy/dx = 2ax
    // The quadratic Bezier control point P1 satisfies:
    // P1 = 2*intermediate - 0.5*(start + end)
    // This gives an exact parabolic arc representation

    QuadraticBezierSegment result;
    result.p0 = start;
    result.p2 = end;

    // Control point for quadratic Bezier that passes through intermediate at t=0.5
    result.p1 = CGMPoint(
        2.0 * intermediate.x() - 0.5 * (start.x() + end.x()),
        2.0 * intermediate.y() - 0.5 * (start.y() + end.y()));

    return result;
}

CGMPoint NurbsApproximator::evaluateNurbs(
    double t,
    int order,
    const std::vector<CGMPoint>& controlPoints,
    const std::vector<double>& weights,
    const std::vector<double>& knots) const
{
    int n = static_cast<int>(controlPoints.size());
    int span = findKnotSpan(t, order, n, knots);

    std::vector<double> basis(order);
    computeBasisFunctions(t, span, order, knots, basis);

    // Compute weighted sum for NURBS
    double sumW = 0.0;
    double sumWX = 0.0;
    double sumWY = 0.0;

    for (int i = 0; i < order; ++i) {
        int idx = span - order + 1 + i;
        if (idx >= 0 && idx < n) {
            double w = (idx < static_cast<int>(weights.size())) ? weights[idx] : 1.0;
            double bw = basis[i] * w;
            sumW += bw;
            sumWX += bw * controlPoints[idx].x();
            sumWY += bw * controlPoints[idx].y();
        }
    }

    CGMPoint result;
    if (std::abs(sumW) > 1e-10) {
        result = CGMPoint(sumWX / sumW, sumWY / sumW);
    } else {
        // Fallback to unweighted if weights sum to zero
        result = CGMPoint(sumWX, sumWY);
    }

    return result;
}

int NurbsApproximator::findKnotSpan(
    double t,
    int order,
    int numControlPoints,
    const std::vector<double>& knots) const
{
    int n = numControlPoints - 1;
    int p = order - 1;

    // Special case: t at end of curve
    if (t >= knots[n + 1]) {
        return n;
    }

    // Binary search for knot span
    int low = p;
    int high = n + 1;
    int mid = (low + high) / 2;

    while (t < knots[mid] || t >= knots[mid + 1]) {
        if (t < knots[mid]) {
            high = mid;
        } else {
            low = mid;
        }
        mid = (low + high) / 2;

        // Prevent infinite loop
        if (high - low <= 1) {
            break;
        }
    }

    return mid;
}

void NurbsApproximator::computeBasisFunctions(
    double t,
    int span,
    int order,
    const std::vector<double>& knots,
    std::vector<double>& basis) const
{
    basis.resize(order);
    std::vector<double> left(order);
    std::vector<double> right(order);

    basis[0] = 1.0;

    for (int j = 1; j < order; ++j) {
        left[j] = t - knots[span + 1 - j];
        right[j] = knots[span + j] - t;

        double saved = 0.0;
        for (int r = 0; r < j; ++r) {
            double denom = right[r + 1] + left[j - r];
            double temp = (std::abs(denom) > 1e-10) ? basis[r] / denom : 0.0;
            basis[r] = saved + right[r + 1] * temp;
            saved = left[j - r] * temp;
        }
        basis[j] = saved;
    }
}

CubicBezierSegment NurbsApproximator::fitCubicBezier(
    const std::function<CGMPoint(double)>& curve,
    double t0,
    double t1) const
{
    CubicBezierSegment result;

    // End points are exact
    result.p0 = curve(t0);
    result.p3 = curve(t1);

    // Estimate tangent directions at endpoints
    double dt = (t1 - t0) / 100.0;
    CGMPoint p0_plus = curve(t0 + dt);
    CGMPoint p3_minus = curve(t1 - dt);

    // Tangent vectors
    double dx0 = p0_plus.x() - result.p0.x();
    double dy0 = p0_plus.y() - result.p0.y();
    double dx1 = result.p3.x() - p3_minus.x();
    double dy1 = result.p3.y() - p3_minus.y();

    // Normalize and scale tangent vectors
    double chord = distance(result.p0, result.p3);
    double scale = chord / 3.0;

    double mag0 = std::sqrt(dx0 * dx0 + dy0 * dy0);
    double mag1 = std::sqrt(dx1 * dx1 + dy1 * dy1);

    if (mag0 > 1e-10) {
        result.p1 = CGMPoint(result.p0.x() + scale * dx0 / mag0, result.p0.y() + scale * dy0 / mag0);
    } else {
        result.p1 = result.p0;
    }

    if (mag1 > 1e-10) {
        result.p2 = CGMPoint(result.p3.x() - scale * dx1 / mag1, result.p3.y() - scale * dy1 / mag1);
    } else {
        result.p2 = result.p3;
    }

    return result;
}

void NurbsApproximator::adaptiveSubdivide(
    const std::function<CGMPoint(double)>& curve,
    double t0,
    double t1,
    double tolerance,
    int maxDepth,
    std::vector<CubicBezierSegment>& result) const
{
    // Fit a cubic Bezier to this segment
    CubicBezierSegment bezier = fitCubicBezier(curve, t0, t1);

    // Check if approximation is good enough
    double error = maxDeviation(curve, bezier, t0, t1);

    if (error <= tolerance || maxDepth <= 0) {
        // Approximation is acceptable or we've reached max depth
        result.push_back(bezier);
    } else {
        // Subdivide and recurse
        double tMid = (t0 + t1) / 2.0;
        adaptiveSubdivide(curve, t0, tMid, tolerance, maxDepth - 1, result);
        adaptiveSubdivide(curve, tMid, t1, tolerance, maxDepth - 1, result);
    }
}

double NurbsApproximator::maxDeviation(
    const std::function<CGMPoint(double)>& curve,
    const CubicBezierSegment& bezier,
    double t0,
    double t1,
    int numSamples) const
{
    double maxDev = 0.0;

    for (int i = 1; i < numSamples - 1; ++i) {
        double ti = static_cast<double>(i) / (numSamples - 1);
        double t = t0 + ti * (t1 - t0);

        CGMPoint curvePoint = curve(t);
        CGMPoint bezierPoint = evaluateBezier(bezier, ti);

        double dev = distance(curvePoint, bezierPoint);
        maxDev = std::max(maxDev, dev);
    }

    return maxDev;
}

CGMPoint NurbsApproximator::evaluateBezier(const CubicBezierSegment& bezier, double t) const
{
    double u = 1.0 - t;
    double u2 = u * u;
    double u3 = u2 * u;
    double t2 = t * t;
    double t3 = t2 * t;

    CGMPoint result(u3 * bezier.p0.x() + 3 * u2 * t * bezier.p1.x() +
               3 * u * t2 * bezier.p2.x() + t3 * bezier.p3.x(), u3 * bezier.p0.y() + 3 * u2 * t * bezier.p1.y() +
               3 * u * t2 * bezier.p2.y() + t3 * bezier.p3.y());

    return result;
}

} // namespace opencgm
