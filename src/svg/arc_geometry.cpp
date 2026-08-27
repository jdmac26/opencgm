#include "opencgm/svg/arc_geometry.h"
#include "opencgm/svg/svg_utils.h"
#include "opencgm/svg_converter.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace opencgm {
namespace svg {

CGMPoint scaleToRadius(const CGMPoint& delta, double radius)
{
    double len = std::hypot(delta.x(), delta.y());
    if (len <= 1e-9) {
        return delta;
    }
    double scale = radius / len;
    return CGMPoint(delta.x() * scale, delta.y() * scale);
}

std::optional<CGMPoint> computeCenterFrom3Points(
    const CGMPoint& p1,
    const CGMPoint& p2,
    const CGMPoint& p3)
{
    // Midpoints of segments p1-p2 and p2-p3
    double mx1 = (p1.x() + p2.x()) / 2.0;
    double my1 = (p1.y() + p2.y()) / 2.0;
    double mx2 = (p2.x() + p3.x()) / 2.0;
    double my2 = (p2.y() + p3.y()) / 2.0;

    // Direction vectors of segments
    double dx1 = p2.x() - p1.x();
    double dy1 = p2.y() - p1.y();
    double dx2 = p3.x() - p2.x();
    double dy2 = p3.y() - p2.y();

    // Perpendicular vectors (rotate 90 degrees)
    double px1 = -dy1;
    double py1 = dx1;
    double px2 = -dy2;
    double py2 = dx2;

    // Find intersection of perpendicular bisectors
    double det = px1 * py2 - py1 * px2;

    if (std::abs(det) < 1e-10) {
        // Points are collinear
        return std::nullopt;
    }

    double t = ((mx2 - mx1) * py2 - (my2 - my1) * px2) / det;

    double center_x = mx1 + t * px1;
    double center_y = my1 + t * py1;

    return CGMPoint(center_x, center_y);
}

std::pair<int, int> computeSvgArcFlags(
    double sweep_angle,
    bool ccw_vdc,
    bool x_inverted,
    bool y_inverted)
{
    // SVG arc sweep flag:
    // - 0 = clockwise
    // - 1 = counter-clockwise
    // However, if a single axis is inverted, the visual direction flips
    bool single_axis_inverted = (x_inverted != y_inverted);
    int sweep_flag = single_axis_inverted ? (ccw_vdc ? 1 : 0) : (ccw_vdc ? 0 : 1);

    // Large arc flag: 1 if the arc spans more than 180 degrees
    int large_arc_flag = (std::fabs(sweep_angle) > M_PI + kAngleTolerance) ? 1 : 0;

    return {large_arc_flag, sweep_flag};
}

CircularArcResult computeCenterBasedArc(
    const CGMPoint& center_vdc,
    const CGMPoint& startDelta,
    const CGMPoint& endDelta,
    double radius,
    bool reversed,
    bool x_inverted,
    bool y_inverted,
    const CoordinateTransform& transform)
{
    CircularArcResult result;

    // Scale deltas to the actual radius
    CGMPoint startScaled = scaleToRadius(startDelta, radius);
    CGMPoint endScaled = scaleToRadius(endDelta, radius);

    // Compute VDC points
    CGMPoint start_vdc(center_vdc.x() + startScaled.x(), center_vdc.y() + startScaled.y());
    CGMPoint end_vdc(center_vdc.x() + endScaled.x(), center_vdc.y() + endScaled.y());

    // Transform to SVG coordinates
    result.center_svg = transform.transformPoint(center_vdc);
    result.start_svg = transform.transformPoint(start_vdc);
    result.end_svg = transform.transformPoint(end_vdc);

    // Calculate radii in SVG space
    double scale_x = std::fabs(transform.scaleX());
    double scale_y = std::fabs(transform.scaleY());
    result.rx = radius * scale_x;
    result.ry = radius * scale_y;

    // Check for full circle
    if (vectorsNearlyEqual(startDelta, endDelta)) {
        result.is_full_circle = true;
        return result;
    }

    // Calculate angles and sweep
    double start_angle = std::atan2(startDelta.y(), startDelta.x());
    double end_angle = std::atan2(endDelta.y(), endDelta.x());

    double sweep;
    bool ccw_vdc;

    if (reversed) {
        // Clockwise arc: sweep from end to start
        sweep = angularDistanceCCW(end_angle, start_angle);
        ccw_vdc = false;
    } else {
        // Counter-clockwise arc: sweep from start to end
        sweep = angularDistanceCCW(start_angle, end_angle);
        ccw_vdc = true;
    }

    if (std::fabs(sweep) < kAngleTolerance) {
        sweep = 2.0 * M_PI;
    }

    // Calculate SVG arc flags
    auto [large_arc, sweep_flag] = computeSvgArcFlags(sweep, ccw_vdc, x_inverted, y_inverted);
    result.large_arc_flag = large_arc;
    result.sweep_flag = sweep_flag;

    return result;
}

std::optional<CircularArcResult> compute3PointArc(
    const CGMPoint& p1,
    const CGMPoint& p2,
    const CGMPoint& p3,
    bool x_inverted,
    bool y_inverted,
    const CoordinateTransform& transform)
{
    // Compute center from 3 points
    auto center_opt = computeCenterFrom3Points(p1, p2, p3);
    if (!center_opt) {
        return std::nullopt;  // Points are collinear
    }

    CGMPoint center_vdc = *center_opt;

    CircularArcResult result;
    result.center_svg = transform.transformPoint(center_vdc);
    result.start_svg = transform.transformPoint(p1);
    result.end_svg = transform.transformPoint(p3);

    // Calculate radius and vectors from center
    CGMPoint start_vec(p1.x() - center_vdc.x(), p1.y() - center_vdc.y());
    CGMPoint mid_vec(p2.x() - center_vdc.x(), p2.y() - center_vdc.y());
    CGMPoint end_vec(p3.x() - center_vdc.x(), p3.y() - center_vdc.y());

    double radius_vdc = vectorLength(start_vec);
    double scale_x = std::fabs(transform.scaleX());
    double scale_y = std::fabs(transform.scaleY());
    result.rx = radius_vdc * scale_x;
    result.ry = radius_vdc * scale_y;

    // Check for full circle
    if (vectorsNearlyEqual(start_vec, end_vec)) {
        result.is_full_circle = true;
        return result;
    }

    // Determine arc direction from cross product
    double cross = crossProduct(start_vec, end_vec);
    bool ccw_vdc = cross > 0.0;

    // Calculate angles
    double start_angle = wrapAnglePositive(std::atan2(start_vec.y(), start_vec.x()));
    double mid_angle = wrapAnglePositive(std::atan2(mid_vec.y(), mid_vec.x()));
    double end_angle = wrapAnglePositive(std::atan2(end_vec.y(), end_vec.x()));

    // Calculate sweep, adjusting for intermediate point.
    //
    // The cross-product sign tells us the SHORT-WAY direction from start
    // to end (< 180°). If the intermediate point lies on that short path,
    // the arc goes that way. Otherwise the arc goes the OTHER way around,
    // which means both the span (>180°) AND the rotation direction flip.
    // Without flipping ccw_vdc the SVG sweep_flag computation gets the
    // direction wrong and the arc visually curves the opposite side of
    // the chord (FIGURE03's pill caps were rendering inverted).
    double sweep;
    if (ccw_vdc) {
        double span = angularDistanceCCW(start_angle, end_angle);
        if (!angleIsBetweenCCW(start_angle, mid_angle, end_angle)) {
            span = 2.0 * M_PI - span;
            ccw_vdc = false;
        }
        sweep = span;
    } else {
        double span = angularDistanceCCW(end_angle, start_angle);
        if (!angleIsBetweenCCW(end_angle, mid_angle, start_angle)) {
            span = 2.0 * M_PI - span;
            ccw_vdc = true;
        }
        sweep = ccw_vdc ? span : -span;
    }

    if (std::fabs(sweep) < kAngleTolerance) {
        sweep = (ccw_vdc ? 1 : -1) * 2.0 * M_PI;
    }

    // Calculate SVG arc flags
    auto [large_arc, sweep_flag] = computeSvgArcFlags(std::fabs(sweep), ccw_vdc, x_inverted, y_inverted);
    result.large_arc_flag = large_arc;
    result.sweep_flag = sweep_flag;

    return result;
}

} // namespace svg
} // namespace opencgm
