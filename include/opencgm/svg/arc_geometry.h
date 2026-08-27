#ifndef OPENCGM_SVG_ARC_GEOMETRY_H
#define OPENCGM_SVG_ARC_GEOMETRY_H

#include "opencgm/cgm_point.h"
#include <optional>

namespace opencgm {

class CoordinateTransform;

namespace svg {

// Result of computing arc SVG parameters
struct CircularArcResult {
    CGMPoint start_svg;
    CGMPoint end_svg;
    CGMPoint center_svg;
    double rx = 0.0;
    double ry = 0.0;
    int large_arc_flag = 0;
    int sweep_flag = 0;
    bool is_full_circle = false;  // True if start == end (full ellipse)
};

// Scale a delta vector to the specified radius length
CGMPoint scaleToRadius(const CGMPoint& delta, double radius);

// Compute center from 3 points using perpendicular bisector intersection.
// Returns nullopt if points are collinear (degenerate case).
std::optional<CGMPoint> computeCenterFrom3Points(
    const CGMPoint& p1,
    const CGMPoint& p2,
    const CGMPoint& p3);

// Calculate SVG arc flags from sweep angle and axis inversion state
// Parameters:
//   sweep_angle:  The arc sweep in radians (positive = CCW in VDC)
//   ccw_vdc:      True if the arc goes counter-clockwise in VDC space
//   x_inverted:   True if VDC X axis is inverted (x1 > x2)
//   y_inverted:   True if VDC Y axis is inverted (y1 > y2)
// Returns: {large_arc_flag, sweep_flag}
std::pair<int, int> computeSvgArcFlags(
    double sweep_angle,
    bool ccw_vdc,
    bool x_inverted,
    bool y_inverted);

// Compute circular arc parameters for center-based arcs (CircularArcCentre, CircularArcCentreReversed, CircularArcCentreClose)
// Parameters:
//   center_vdc:   Center point in VDC coordinates
//   startDelta:   Start point as offset from center (will be scaled to radius)
//   endDelta:     End point as offset from center (will be scaled to radius)
//   radius:       Arc radius
//   reversed:     True for clockwise arc (CircularArcCentreReversed)
//   x_inverted:   True if VDC X axis is inverted
//   y_inverted:   True if VDC Y axis is inverted
//   transform:    Coordinate transform for VDC -> SVG
CircularArcResult computeCenterBasedArc(
    const CGMPoint& center_vdc,
    const CGMPoint& startDelta,
    const CGMPoint& endDelta,
    double radius,
    bool reversed,
    bool x_inverted,
    bool y_inverted,
    const CoordinateTransform& transform);

// Compute circular arc parameters for 3-point arcs (CircularArc3Point, CircularArc3PointClose)
// Parameters:
//   p1, p2, p3:   Three points defining the arc (start, intermediate, end) in VDC
//   x_inverted:   True if VDC X axis is inverted
//   y_inverted:   True if VDC Y axis is inverted
//   transform:    Coordinate transform for VDC -> SVG
// Returns nullopt if points are collinear
std::optional<CircularArcResult> compute3PointArc(
    const CGMPoint& p1,
    const CGMPoint& p2,
    const CGMPoint& p3,
    bool x_inverted,
    bool y_inverted,
    const CoordinateTransform& transform);

} // namespace svg
} // namespace opencgm

#endif // OPENCGM_SVG_ARC_GEOMETRY_H
