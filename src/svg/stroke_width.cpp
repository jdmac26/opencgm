#include "opencgm/svg/stroke_width.h"
#include "opencgm/svg_converter.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace opencgm {
namespace svg {

double computeStrokeWidth(
    double raw_width,
    const StrokeWidthContext& ctx,
    bool enable_logging,
    const char* log_label)
{
    double svg_width = 0.0;

    if (ctx.spec_mode == SpecificationMode::ABS) {
        double base_extent = ctx.picture_longest_side_raw > 0.0
                                 ? ctx.picture_longest_side_raw
                                 : std::max(ctx.picture_vdc_width, ctx.picture_vdc_height);

        double width_ratio = (base_extent > 0.0)
                                 ? std::fabs(raw_width) / base_extent
                                 : 0.0;

        double width_vdc = ctx.transform
                               ? std::fabs(ctx.transform->transformLength(raw_width))
                               : std::fabs(raw_width);

        double width_abstract = width_vdc;
        if (base_extent > 0.0) {
            double unit = ctx.nominal_width_svg / base_extent;
            width_abstract = std::fabs(raw_width * unit);
        }

        // Boundary tightened to strictly-greater so that raw widths exactly
        // 5% of the picture extent (e.g., EDGE WIDTH = 50 on a 1000-VDC
        // canvas, common in NIST static10 PLGSET/POLYGN samples) take the
        // direct-VDC path. ISO 8632-1 §6.5.4 says ABS mode widths are in
        // VDC; the abstract heuristic is for CGMs that overload the value
        // as per-thousandth of extent — only those use widths strictly
        // bigger than 5% of extent.
        if (width_ratio > 0.05) {
            svg_width = width_abstract;
        } else {
            svg_width = width_vdc;
        }
    } else {
        // SCALED mode
        svg_width = std::fabs(raw_width * ctx.nominal_width_svg);
    }

    // Calculate viewbox dimensions for hairline and viewport minimum calculations
    double viewbox_width = ctx.viewbox_x2 - ctx.viewbox_x1;
    double viewbox_height = ctx.viewbox_y2 - ctx.viewbox_y1;
    double viewbox_longest = std::max(viewbox_width, viewbox_height);

    // ISO/IEC 8632-1:1999 (Annex D.2.2.1) - map zero/near-zero widths to a hairline
    double hairline = ctx.nominal_width_svg / 256.0;

    double abstract_hairline = ctx.transform
                                   ? ctx.transform->transformLength(ctx.abstract_width_unit) / 256.0
                                   : ctx.abstract_width_unit / 256.0;

    if (std::isfinite(abstract_hairline) && abstract_hairline > hairline) {
        hairline = abstract_hairline;
    }

    double viewbox_hairline = viewbox_longest / 120000.0;
    if (std::isfinite(viewbox_hairline) && viewbox_hairline > hairline) {
        hairline = viewbox_hairline;
    }

    if (!std::isfinite(hairline) || hairline <= 0.0) {
        hairline = 0.01;
    }

    if (!std::isfinite(svg_width) || svg_width <= 0.0) {
        svg_width = hairline;
    } else if (svg_width < hairline) {
        svg_width = hairline;
    }

    // Viewport-relative minimum: ensure strokes are visible at typical display sizes
    // For a 20000 viewBox displayed at 800px, 1 pixel ≈ 25 viewBox units
    // Using viewbox/2000 gives ~0.5px minimum at typical display (10 for 20000 viewBox)
    double viewport_min_width = viewbox_longest / 2000.0;
    if (viewport_min_width > 0.0 && svg_width < viewport_min_width) {
        svg_width = viewport_min_width;
    }

    if (enable_logging && log_label) {
        std::cerr << "[svg] " << log_label << " raw=" << raw_width
                  << " mode=" << (ctx.spec_mode == SpecificationMode::ABS ? "ABS" : "SCALED")
                  << " svg=" << svg_width << "\n";
    }

    return svg_width;
}

} // namespace svg
} // namespace opencgm
