#ifndef OPENCGM_SVG_STROKE_WIDTH_H
#define OPENCGM_SVG_STROKE_WIDTH_H

#include "opencgm/enums.h"

namespace opencgm {

class CoordinateTransform;

namespace svg {

// Context for stroke width calculation - contains all the parameters needed
// to compute the final SVG stroke width from a CGM width value
struct StrokeWidthContext {
    SpecificationMode spec_mode = SpecificationMode::SCALED;
    double nominal_width_svg = 1.0;       // Nominal width in SVG coordinates
    double abstract_width_unit = 1.0;     // Abstract width unit for hairline calculation
    double picture_longest_side_raw = 0.0;
    double picture_vdc_width = 0.0;
    double picture_vdc_height = 0.0;
    double viewbox_x1 = 0.0;
    double viewbox_y1 = 0.0;
    double viewbox_x2 = 0.0;
    double viewbox_y2 = 0.0;
    const CoordinateTransform* transform = nullptr;
};

// Computes the final SVG stroke width from a raw CGM width value.
// This function encapsulates the width calculation logic used for both
// line widths and edge widths, eliminating code duplication.
//
// Parameters:
//   raw_width:       The raw width value from the CGM command
//   ctx:             Context containing all calculation parameters
//   enable_logging:  If true, log the calculation details to stderr
//   log_label:       Label for logging (e.g., "line-width" or "edge-width")
//
// Returns:
//   The calculated SVG stroke width
double computeStrokeWidth(
    double raw_width,
    const StrokeWidthContext& ctx,
    bool enable_logging = false,
    const char* log_label = nullptr);

} // namespace svg
} // namespace opencgm

#endif // OPENCGM_SVG_STROKE_WIDTH_H
