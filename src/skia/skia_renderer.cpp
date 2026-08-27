#include "opencgm/skia_renderer.h"

#ifdef CGM_SKIA_RENDERER_ENABLED

#include "opencgm/cgm_file.h"
#include "opencgm/commands/picture_descriptor_commands.h"
#include "opencgm/commands/graphical_primitive_commands.h"
#include "opencgm/commands/attribute_commands.h"
#include "opencgm/commands/delimiter_commands.h"
#include "opencgm/enums.h"
#include "opencgm/security_limits.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkFont.h"
#include "include/core/SkTypeface.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkColor.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkSpan.h"
#include "include/effects/SkDashPathEffect.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <tuple>
#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Converts CGM conjugate diameter vectors to ellipse parameters (rx, ry, rotation).
// CGM defines ellipses via: P(t) = center + u*cos(t) + v*sin(t)
// where u and v are the conjugate diameter vectors (NOT necessarily orthogonal).
// Returns the true semi-axis lengths and rotation angle in radians.
static std::tuple<double, double, double> conjugateDiametersToEllipse(
    double ux, double uy,  // First conjugate diameter vector
    double vx, double vy)  // Second conjugate diameter vector
{
    // Form matrix A = M * M^T where M = [u | v] (columns are u and v)
    // The eigenvalues of A give the squared semi-axis lengths.
    double a = ux * ux + vx * vx;  // A[0][0]
    double b = ux * uy + vx * vy;  // A[0][1] = A[1][0]
    double c = uy * uy + vy * vy;  // A[1][1]

    // Eigenvalues of 2x2 symmetric matrix via quadratic formula
    double trace = a + c;
    double det = a * c - b * b;
    double disc = trace * trace - 4.0 * det;
    if (disc < 0.0) disc = 0.0;
    double sqrtDisc = std::sqrt(disc);

    double lambda1 = (trace + sqrtDisc) / 2.0;  // Larger eigenvalue
    double lambda2 = (trace - sqrtDisc) / 2.0;  // Smaller eigenvalue

    double rx = std::sqrt(std::max(lambda1, 1e-10));
    double ry = std::sqrt(std::max(lambda2, 1e-10));

    // Rotation angle from eigenvector direction (in radians)
    double rotation;
    if (std::fabs(b) > 1e-10) {
        rotation = 0.5 * std::atan2(2.0 * b, a - c);
    } else {
        rotation = (a >= c) ? 0.0 : M_PI / 2.0;
    }

    return {rx, ry, rotation};
}

namespace opencgm {

// ============================================================================
// RenderTransform Implementation
// ============================================================================

RenderTransform::RenderTransform()
    : vdc_x1_(0), vdc_y1_(0), vdc_x2_(32767), vdc_y2_(32767)
    , output_width_(800), output_height_(600)
    , flip_y_(true)
    , vdc_y_down_(false)
{
}

void RenderTransform::setVdcExtent(double x1, double y1, double x2, double y2) {
    vdc_x1_ = x1;
    vdc_y1_ = y1;
    vdc_x2_ = x2;
    vdc_y2_ = y2;
    vdc_y_down_ = (y1 > y2);
}

void RenderTransform::setOutputSize(double width, double height) {
    output_width_ = width;
    output_height_ = height;
}

void RenderTransform::setFlipY(bool flip) {
    flip_y_ = flip;
}

void RenderTransform::transform(double vdc_x, double vdc_y, double& out_x, double& out_y) const {
    double vdc_width = std::abs(vdc_x2_ - vdc_x1_);
    double vdc_height = std::abs(vdc_y2_ - vdc_y1_);

    if (vdc_width < 1e-9) vdc_width = 1;
    if (vdc_height < 1e-9) vdc_height = 1;

    double scale_x = output_width_ / vdc_width;
    double scale_y = output_height_ / vdc_height;

    double min_vdc_x = std::min(vdc_x1_, vdc_x2_);
    double min_vdc_y = std::min(vdc_y1_, vdc_y2_);

    out_x = (vdc_x - min_vdc_x) * scale_x;
    out_y = (vdc_y - min_vdc_y) * scale_y;

    if (flip_y_ && !vdc_y_down_) {
        out_y = output_height_ - out_y;
    }
}

CGMPoint RenderTransform::transformPoint(const CGMPoint& pt) const {
    double ox, oy;
    transform(pt.x(), pt.y(), ox, oy);
    return CGMPoint(ox, oy);
}

double RenderTransform::transformLength(double vdc_length) const {
    double vdc_width = std::abs(vdc_x2_ - vdc_x1_);
    if (vdc_width < 1e-9) vdc_width = 1;
    return vdc_length * (output_width_ / vdc_width);
}

void RenderTransform::getVdcExtent(double& x1, double& y1, double& x2, double& y2) const {
    x1 = vdc_x1_;
    y1 = vdc_y1_;
    x2 = vdc_x2_;
    y2 = vdc_y2_;
}

double RenderTransform::scaleX() const {
    double vdc_width = std::abs(vdc_x2_ - vdc_x1_);
    if (vdc_width < 1e-9) vdc_width = 1;
    return output_width_ / vdc_width;
}

double RenderTransform::scaleY() const {
    double vdc_height = std::abs(vdc_y2_ - vdc_y1_);
    if (vdc_height < 1e-9) vdc_height = 1;
    return output_height_ / vdc_height;
}

// ============================================================================
// RenderStyle Implementation
// ============================================================================

RenderStyle::RenderStyle()
    : line_color_(0, 0, 0)
    , line_width_(1.0)
    , line_type_(1)
    , line_cap_(LineCapIndicator::BUTT)
    , line_join_(JoinIndicator::MITER)
    , fill_color_(255, 255, 255)
    , fill_style_(1)
    , hatch_index_(1)
    , pattern_index_(1)
    , edge_color_(0, 0, 0)
    , edge_width_(1.0)
    , edge_type_(1)
    , edge_visibility_(false)
    , edge_cap_(LineCapIndicator::BUTT)
    , edge_join_(JoinIndicator::MITER)
    , text_color_(0, 0, 0)
    , text_height_(12.0)
    , text_h_align_(0)
    , text_v_align_(0)
    , font_index_(1)
    , font_family_("Arial")
    , character_spacing_(0.0)
    , character_expansion_(1.0)
    , marker_type_(3)
    , marker_size_(8.0)
    , marker_color_(0, 0, 0)
{
}

void RenderStyle::setTextAlignment(int h_align, int v_align) {
    text_h_align_ = h_align;
    text_v_align_ = v_align;
}

std::vector<float> RenderStyle::getLineDashPattern(double scale) const {
    std::vector<float> pattern;
    float s = static_cast<float>(scale * line_width_);
    if (s < 1.0f) s = 1.0f;

    switch (line_type_) {
        case 2: // Dash
            pattern = {4.0f * s, 2.0f * s};
            break;
        case 3: // Dot
            pattern = {1.0f * s, 2.0f * s};
            break;
        case 4: // Dash-dot
            pattern = {4.0f * s, 2.0f * s, 1.0f * s, 2.0f * s};
            break;
        case 5: // Dash-dot-dot
            pattern = {4.0f * s, 2.0f * s, 1.0f * s, 2.0f * s, 1.0f * s, 2.0f * s};
            break;
        default: // Solid
            break;
    }
    return pattern;
}

std::vector<float> RenderStyle::getEdgeDashPattern(double scale) const {
    std::vector<float> pattern;
    float s = static_cast<float>(scale * edge_width_);
    if (s < 1.0f) s = 1.0f;

    switch (edge_type_) {
        case 2:
            pattern = {4.0f * s, 2.0f * s};
            break;
        case 3:
            pattern = {1.0f * s, 2.0f * s};
            break;
        case 4:
            pattern = {4.0f * s, 2.0f * s, 1.0f * s, 2.0f * s};
            break;
        case 5:
            pattern = {4.0f * s, 2.0f * s, 1.0f * s, 2.0f * s, 1.0f * s, 2.0f * s};
            break;
        default:
            break;
    }
    return pattern;
}

// ============================================================================
// SkiaRenderer Implementation
// ============================================================================

SkiaRenderer::SkiaRenderer(CGMFile* cgm_file)
    : cgm_file_(cgm_file)
    , surface_(nullptr)
    , canvas_(nullptr)
    , bg_r_(255), bg_g_(255), bg_b_(255), bg_a_(255)
    , antialias_(true)
    , dpi_(96)
    , in_picture_(false)
    , current_picture_index_(0)
{
}

SkiaRenderer::~SkiaRenderer() = default;

void SkiaRenderer::setBackgroundColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    bg_r_ = r;
    bg_g_ = g;
    bg_b_ = b;
    bg_a_ = a;
}

void SkiaRenderer::setAntiAlias(bool enabled) {
    antialias_ = enabled;
}

void SkiaRenderer::setDpi(int dpi) {
    dpi_ = dpi;
}

SkiaRenderResult SkiaRenderer::render(const SkiaRenderOptions& options) {
    SkiaRenderResult result;

    if (!cgm_file_) {
        result.error_message = "No CGM file loaded";
        return result;
    }

    // Calculate output dimensions
    int width = 800, height = 600;
    calculateOutputDimensions(options, width, height);

    // Apply scale factor
    width = static_cast<int>(width * options.scale_factor);
    height = static_cast<int>(height * options.scale_factor);

    if (width <= 0 || height <= 0) {
        result.error_message = "Invalid output dimensions";
        return result;
    }

    // Initialize surface
    if (!initializeSurface(width, height)) {
        result.error_message = "Failed to create rendering surface";
        return result;
    }

    // Clear with background color
    canvas_->clear(SkColorSetARGB(options.background_a, options.background_r,
                                   options.background_g, options.background_b));

    // Setup transform
    transform_.setOutputSize(width, height);

    // Initialize default color table and style
    initializeDefaultColorTable();
    current_style_ = RenderStyle();

    // Process all CGM commands
    processCommands();

    // Get pixel data
    SkImageInfo info = surface_->imageInfo();
    size_t rowBytes = info.minRowBytes();
    size_t totalBytes = rowBytes * height;

    result.pixel_data.resize(totalBytes);
    if (!surface_->readPixels(info, result.pixel_data.data(), rowBytes, 0, 0)) {
        result.error_message = "Failed to read pixels from surface";
        return result;
    }

    result.success = true;
    result.width = width;
    result.height = height;
    result.stride = static_cast<int>(rowBytes);

    return result;
}

bool SkiaRenderer::renderToBuffer(
    int target_width,
    int target_height,
    double scale_factor,
    uint8_t** out_buffer,
    size_t* out_buffer_size,
    int* out_width,
    int* out_height)
{
    SkiaRenderOptions options;
    options.target_width = target_width;
    options.target_height = target_height;
    options.scale_factor = scale_factor;
    options.background_r = bg_r_;
    options.background_g = bg_g_;
    options.background_b = bg_b_;
    options.background_a = bg_a_;
    options.antialias = antialias_;
    options.dpi = dpi_;

    SkiaRenderResult result = render(options);

    if (!result.success) {
        last_error_ = result.error_message;
        return false;
    }

    // Allocate output buffer
    *out_buffer_size = result.pixel_data.size();
    *out_buffer = new uint8_t[*out_buffer_size];
    std::memcpy(*out_buffer, result.pixel_data.data(), *out_buffer_size);

    *out_width = result.width;
    *out_height = result.height;

    return true;
}

void SkiaRenderer::freeBuffer(uint8_t* buffer) {
    delete[] buffer;
}

bool SkiaRenderer::initializeSurface(int width, int height) {
    SkImageInfo info = SkImageInfo::Make(
        width, height,
        kBGRA_8888_SkColorType,
        kPremul_SkAlphaType
    );

    surface_ = SkSurfaces::Raster(info);
    if (!surface_) {
        return false;
    }

    canvas_ = surface_->getCanvas();
    return canvas_ != nullptr;
}

void SkiaRenderer::calculateOutputDimensions(const SkiaRenderOptions& options, int& width, int& height) {
    // Get VDC extent from the CGM file
    double vdc_x1 = 0, vdc_y1 = 0, vdc_x2 = 32767, vdc_y2 = 32767;

    // Try to get actual VDC extent from CGM
    if (cgm_file_) {
        for (const auto& cmd : cgm_file_->commands()) {
            if (cmd->elementClass() == ClassCode::PictureDescriptorElements && cmd->elementId() == 6) {
                // VDC Extent command
                auto* vdcCmd = dynamic_cast<VDCExtent*>(cmd.get());
                if (vdcCmd) {
                    vdc_x1 = vdcCmd->firstCorner().x();
                    vdc_y1 = vdcCmd->firstCorner().y();
                    vdc_x2 = vdcCmd->secondCorner().x();
                    vdc_y2 = vdcCmd->secondCorner().y();
                    break;
                }
            }
        }
    }

    transform_.setVdcExtent(vdc_x1, vdc_y1, vdc_x2, vdc_y2);

    double vdc_width = std::abs(vdc_x2 - vdc_x1);
    double vdc_height = std::abs(vdc_y2 - vdc_y1);
    double aspect_ratio = (vdc_height > 0) ? (vdc_width / vdc_height) : 1.0;

    if (options.target_width > 0 && options.target_height > 0) {
        width = options.target_width;
        height = options.target_height;
    } else if (options.target_width > 0) {
        width = options.target_width;
        height = static_cast<int>(width / aspect_ratio);
    } else if (options.target_height > 0) {
        height = options.target_height;
        width = static_cast<int>(height * aspect_ratio);
    } else {
        // Default size based on VDC extent
        constexpr int defaultDim = render_limits::DEFAULT_OUTPUT_DIMENSION;
        if (vdc_width > vdc_height) {
            width = defaultDim;
            height = static_cast<int>(defaultDim / aspect_ratio);
        } else {
            height = defaultDim;
            width = static_cast<int>(defaultDim * aspect_ratio);
        }
    }

    // Clamp to reasonable bounds (see security_limits.h for rationale)
    constexpr int minDim = render_limits::MIN_OUTPUT_DIMENSION;
    constexpr int maxDim = render_limits::MAX_OUTPUT_DIMENSION;
    if (width < minDim) width = minDim;
    if (height < minDim) height = minDim;
    if (width > maxDim) width = maxDim;
    if (height > maxDim) height = maxDim;
}

Color SkiaRenderer::resolveColor(const CGMColor& cgmColor) const {
    if (cgmColor.isIndexed()) {
        auto it = color_table_.find(cgmColor.colorIndex());
        if (it != color_table_.end()) {
            return it->second;
        }
        return Color(0, 0, 0);
    }
    const Color& c = cgmColor.color();
    return Color(c.r, c.g, c.b);
}

void SkiaRenderer::initializeDefaultColorTable() {
    color_table_[0] = Color(255, 255, 255);
    color_table_[1] = Color(0, 0, 0);
    color_table_[2] = Color(255, 0, 0);
    color_table_[3] = Color(0, 255, 0);
    color_table_[4] = Color(0, 0, 255);
    color_table_[5] = Color(255, 255, 0);
    color_table_[6] = Color(0, 255, 255);
    color_table_[7] = Color(255, 0, 255);
}

// ============================================================================
// Command Processing Implementation
// ============================================================================

void SkiaRenderer::processCommands() {
    if (!cgm_file_ || !canvas_) return;

    for (const auto& cmd : cgm_file_->commands()) {
        processCommand(cmd.get());
    }
}

void SkiaRenderer::processCommand(Command* cmd) {
    if (!cmd) return;

    switch (cmd->elementClass()) {
        case ClassCode::DelimiterElement:
            // Handle picture start/end
            if (cmd->elementId() == 3) { // BEGIN PICTURE
                processBeginPicture(dynamic_cast<BeginPicture*>(cmd));
            } else if (cmd->elementId() == 4) { // BEGIN PICTURE BODY
                in_picture_ = true;
            } else if (cmd->elementId() == 5) { // END PICTURE
                processEndPicture(dynamic_cast<EndPicture*>(cmd));
            }
            break;

        case ClassCode::PictureDescriptorElements:
            if (cmd->elementId() == 6) { // VDC EXTENT
                processVdcExtent(cmd);
            }
            break;

        case ClassCode::AttributeElements:
            processAttributeCommand(cmd);
            break;

        case ClassCode::GraphicalPrimitiveElements:
            // Route to specific primitive renderer based on element ID
            switch (cmd->elementId()) {
                case 1:  // POLYLINE
                    renderPolyline(dynamic_cast<Polyline*>(cmd));
                    break;
                case 2:  // DISJOINT POLYLINE
                    renderDisjointPolyline(dynamic_cast<DisjointPolyline*>(cmd));
                    break;
                case 3:  // POLYMARKER
                    renderPolymarker(dynamic_cast<Polymarker*>(cmd));
                    break;
                case 4:  // TEXT
                    renderText(dynamic_cast<Text*>(cmd));
                    break;
                case 5:  // RESTRICTED TEXT
                    renderRestrictedText(dynamic_cast<RestrictedText*>(cmd));
                    break;
                case 6:  // APPEND TEXT
                    renderAppendText(dynamic_cast<AppendText*>(cmd));
                    break;
                case 7:  // POLYGON
                    renderPolygon(dynamic_cast<Polygon*>(cmd));
                    break;
                case 8:  // POLYGON SET
                    renderPolygonSet(dynamic_cast<PolygonSet*>(cmd));
                    break;
                case 9:  // CELL ARRAY
                    renderCellArray(dynamic_cast<CellArray*>(cmd));
                    break;
                case 11: // RECTANGLE
                    renderRectangle(dynamic_cast<Rectangle*>(cmd));
                    break;
                case 12: // CIRCLE
                    renderCircle(dynamic_cast<Circle*>(cmd));
                    break;
                case 13: // CIRCULAR ARC 3 POINT
                    renderCircularArc3Point(dynamic_cast<CircularArc3Point*>(cmd));
                    break;
                case 14: // CIRCULAR ARC 3 POINT CLOSE
                    renderCircularArc3PointClose(dynamic_cast<CircularArc3PointClose*>(cmd));
                    break;
                case 15: // CIRCULAR ARC CENTRE
                    renderCircularArcCentre(dynamic_cast<CircularArcCentre*>(cmd));
                    break;
                case 16: // CIRCULAR ARC CENTRE CLOSE
                    renderCircularArcCentreClose(dynamic_cast<CircularArcCentreClose*>(cmd));
                    break;
                case 17: // ELLIPSE
                    renderEllipse(dynamic_cast<Ellipse*>(cmd));
                    break;
                case 18: // ELLIPTICAL ARC
                    renderEllipticalArc(dynamic_cast<EllipticalArc*>(cmd));
                    break;
                case 19: // ELLIPTICAL ARC CLOSE
                    renderEllipticalArcClose(dynamic_cast<EllipticalArcClose*>(cmd));
                    break;
                case 20: // CIRCULAR ARC CENTRE REVERSED
                    renderCircularArcCentreReversed(dynamic_cast<CircularArcCentreReversed*>(cmd));
                    break;
                case 24: // NON-UNIFORM B-SPLINE
                    renderNonUniformBSpline(dynamic_cast<NonUniformBSpline*>(cmd));
                    break;
                case 25: // NON-UNIFORM RATIONAL B-SPLINE
                    renderNonUniformRationalBSpline(dynamic_cast<NonUniformRationalBSpline*>(cmd));
                    break;
                case 26: // POLYBEZIER
                    renderPolyBezier(dynamic_cast<PolyBezier*>(cmd));
                    break;
                case 28: // BITONAL TILE
                    renderBitonalTile(dynamic_cast<BitonalTile*>(cmd));
                    break;
                default:
                    // Unsupported primitive - skip silently
                    break;
            }
            break;

        default:
            // Other element classes not handled in rendering
            break;
    }
}

void SkiaRenderer::processAttributeCommand(Command* cmd) {
    if (!cmd) return;

    switch (cmd->elementId()) {
        case 2: { // LINE TYPE
            auto* lt = dynamic_cast<LineType*>(cmd);
            if (lt) current_style_.setLineType(lt->type());
            break;
        }
        case 3: { // LINE WIDTH
            auto* lw = dynamic_cast<LineWidth*>(cmd);
            if (lw) current_style_.setLineWidth(lw->width());
            break;
        }
        case 4: { // LINE COLOUR
            auto* lc = dynamic_cast<LineColour*>(cmd);
            if (lc) current_style_.setLineColor(resolveColor(lc->color()));
            break;
        }
        case 5: { // MARKER TYPE
            auto* mt = dynamic_cast<MarkerType*>(cmd);
            if (mt) current_style_.setMarkerType(mt->type());
            break;
        }
        case 6: { // MARKER SIZE
            auto* ms = dynamic_cast<MarkerSize*>(cmd);
            if (ms) current_style_.setMarkerSize(ms->size());
            break;
        }
        case 7: { // MARKER COLOUR
            auto* mc = dynamic_cast<MarkerColour*>(cmd);
            if (mc) current_style_.setMarkerColor(resolveColor(mc->color()));
            break;
        }
        case 10: { // TEXT FONT INDEX
            auto* tf = dynamic_cast<TextFontIndex*>(cmd);
            if (tf) current_style_.setFontIndex(tf->index());
            break;
        }
        case 12: { // CHARACTER EXPANSION FACTOR
            auto* ce = dynamic_cast<CharacterExpansionFactor*>(cmd);
            if (ce) current_style_.setCharacterExpansion(ce->factor());
            break;
        }
        case 13: { // CHARACTER SPACING
            auto* cs = dynamic_cast<CharacterSpacing*>(cmd);
            if (cs) current_style_.setCharacterSpacing(cs->spacing());
            break;
        }
        case 14: { // TEXT COLOUR
            auto* tc = dynamic_cast<TextColour*>(cmd);
            if (tc) current_style_.setTextColor(resolveColor(tc->color()));
            break;
        }
        case 15: { // CHARACTER HEIGHT
            auto* ch = dynamic_cast<CharacterHeight*>(cmd);
            if (ch) current_style_.setTextHeight(ch->height());
            break;
        }
        case 18: { // TEXT ALIGNMENT
            auto* ta = dynamic_cast<TextAlignment*>(cmd);
            if (ta) current_style_.setTextAlignment(ta->horizontalAlignment(), ta->verticalAlignment());
            break;
        }
        case 21: { // INTERIOR STYLE
            auto* is = dynamic_cast<InteriorStyle*>(cmd);
            if (is) current_style_.setFillStyle(is->style());
            break;
        }
        case 22: { // FILL COLOUR
            auto* fc = dynamic_cast<FillColour*>(cmd);
            if (fc) current_style_.setFillColor(resolveColor(fc->color()));
            break;
        }
        case 23: { // HATCH INDEX
            auto* hi = dynamic_cast<HatchIndex*>(cmd);
            if (hi) current_style_.setHatchIndex(hi->index());
            break;
        }
        case 24: { // PATTERN INDEX
            auto* pi = dynamic_cast<PatternIndex*>(cmd);
            if (pi) current_style_.setPatternIndex(pi->index());
            break;
        }
        case 27: { // EDGE TYPE
            auto* et = dynamic_cast<EdgeType*>(cmd);
            if (et) current_style_.setEdgeType(et->type());
            break;
        }
        case 28: { // EDGE WIDTH
            auto* ew = dynamic_cast<EdgeWidth*>(cmd);
            if (ew) current_style_.setEdgeWidth(ew->width());
            break;
        }
        case 29: { // EDGE COLOUR
            auto* ec = dynamic_cast<EdgeColour*>(cmd);
            if (ec) current_style_.setEdgeColor(resolveColor(ec->color()));
            break;
        }
        case 30: { // EDGE VISIBILITY
            auto* ev = dynamic_cast<EdgeVisibility*>(cmd);
            if (ev) current_style_.setEdgeVisibility(ev->isVisible());
            break;
        }
        case 33: { // COLOUR TABLE
            auto* ct = dynamic_cast<ColourTable*>(cmd);
            if (ct) {
                int idx = ct->startIndex();
                for (const auto& c : ct->colors()) {
                    color_table_[idx++] = c;
                }
            }
            break;
        }
        case 37: { // LINE CAP
            auto* lc = dynamic_cast<LineCap*>(cmd);
            if (lc) current_style_.setLineCap(lc->lineIndicator());
            break;
        }
        case 38: { // LINE JOIN
            auto* lj = dynamic_cast<LineJoin*>(cmd);
            if (lj) current_style_.setLineJoin(lj->type());
            break;
        }
        case 44: { // EDGE CAP
            auto* ec = dynamic_cast<EdgeCap*>(cmd);
            if (ec) current_style_.setEdgeCap(ec->lineIndicator());
            break;
        }
        case 45: { // EDGE JOIN
            auto* ej = dynamic_cast<EdgeJoin*>(cmd);
            if (ej) current_style_.setEdgeJoin(ej->type());
            break;
        }
        default:
            // Unhandled attribute - skip
            break;
    }
}

void SkiaRenderer::processBeginPicture(BeginPicture* cmd) {
    (void)cmd;
    in_picture_ = false;
    current_picture_index_++;
    // Reset style to defaults for new picture
    current_style_ = RenderStyle();
}

void SkiaRenderer::processEndPicture(EndPicture* cmd) {
    (void)cmd;
    in_picture_ = false;
}

void SkiaRenderer::processVdcExtent(Command* cmd) {
    auto* vdc = dynamic_cast<VDCExtent*>(cmd);
    if (vdc) {
        transform_.setVdcExtent(
            vdc->firstCorner().x(), vdc->firstCorner().y(),
            vdc->secondCorner().x(), vdc->secondCorner().y());
    }
}

// ============================================================================
// Paint Configuration
// ============================================================================

void SkiaRenderer::configureStrokePaint(SkPaint& paint) const {
    const Color& c = current_style_.lineColor();
    paint.setColor(SkColorSetARGB(c.a, c.r, c.g, c.b));
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setAntiAlias(antialias_);

    // Scale line width
    double scaledWidth = transform_.transformLength(current_style_.lineWidth());
    if (scaledWidth < 1.0) scaledWidth = 1.0;
    paint.setStrokeWidth(static_cast<SkScalar>(scaledWidth));

    // Set line cap
    switch (current_style_.lineCap()) {
        case LineCapIndicator::ROUND:
            paint.setStrokeCap(SkPaint::kRound_Cap);
            break;
        case LineCapIndicator::PROJECTING_SQUARE:
            paint.setStrokeCap(SkPaint::kSquare_Cap);
            break;
        default:
            paint.setStrokeCap(SkPaint::kButt_Cap);
            break;
    }

    // Set line join
    switch (current_style_.lineJoin()) {
        case JoinIndicator::ROUND:
            paint.setStrokeJoin(SkPaint::kRound_Join);
            break;
        case JoinIndicator::BEVEL:
            paint.setStrokeJoin(SkPaint::kBevel_Join);
            break;
        default:
            paint.setStrokeJoin(SkPaint::kMiter_Join);
            break;
    }

    // Apply dash pattern if not solid
    auto dashPattern = current_style_.getLineDashPattern(transform_.scaleX());
    if (!dashPattern.empty()) {
        paint.setPathEffect(SkDashPathEffect::Make(
            SkSpan<const SkScalar>(dashPattern.data(), dashPattern.size()), 0.0f));
    }
}

void SkiaRenderer::configureFillPaint(SkPaint& paint) const {
    int style = current_style_.fillStyle();

    // Fill style: 0=hollow, 1=solid, 2=pattern, 3=hatch, 4=empty
    if (style == 4) { // Empty - no fill
        paint.setColor(SK_ColorTRANSPARENT);
        return;
    }

    if (style == 0) { // Hollow - same as stroke color
        const Color& c = current_style_.lineColor();
        paint.setColor(SkColorSetARGB(c.a, c.r, c.g, c.b));
    } else { // Solid, pattern, hatch
        const Color& c = current_style_.fillColor();
        paint.setColor(SkColorSetARGB(c.a, c.r, c.g, c.b));
    }

    paint.setStyle(SkPaint::kFill_Style);
    paint.setAntiAlias(antialias_);

    // TODO: Implement hatch and pattern shaders in Phase 7
}

void SkiaRenderer::configureEdgePaint(SkPaint& paint) const {
    const Color& c = current_style_.edgeColor();
    paint.setColor(SkColorSetARGB(c.a, c.r, c.g, c.b));
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setAntiAlias(antialias_);

    double scaledWidth = transform_.transformLength(current_style_.edgeWidth());
    if (scaledWidth < 1.0) scaledWidth = 1.0;
    paint.setStrokeWidth(static_cast<SkScalar>(scaledWidth));

    // Set edge cap
    switch (current_style_.lineCap()) { // Edge cap mirrors line cap style
        case LineCapIndicator::ROUND:
            paint.setStrokeCap(SkPaint::kRound_Cap);
            break;
        case LineCapIndicator::PROJECTING_SQUARE:
            paint.setStrokeCap(SkPaint::kSquare_Cap);
            break;
        default:
            paint.setStrokeCap(SkPaint::kButt_Cap);
            break;
    }

    // Apply edge dash pattern
    auto dashPattern = current_style_.getEdgeDashPattern(transform_.scaleX());
    if (!dashPattern.empty()) {
        paint.setPathEffect(SkDashPathEffect::Make(
            SkSpan<const SkScalar>(dashPattern.data(), dashPattern.size()), 0.0f));
    }
}

void SkiaRenderer::configureTextPaint(SkPaint& paint) const {
    const Color& c = current_style_.textColor();
    paint.setColor(SkColorSetARGB(c.a, c.r, c.g, c.b));
    paint.setStyle(SkPaint::kFill_Style);
    paint.setAntiAlias(antialias_);
}

// ============================================================================
// Path Building Helpers
// ============================================================================

void SkiaRenderer::buildPolylinePath(SkPath& path, const std::vector<CGMPoint>& points) const {
    if (points.empty()) return;

    CGMPoint p = transform_.transformPoint(points[0]);
    path.moveTo(static_cast<SkScalar>(p.x()), static_cast<SkScalar>(p.y()));

    for (size_t i = 1; i < points.size(); ++i) {
        p = transform_.transformPoint(points[i]);
        path.lineTo(static_cast<SkScalar>(p.x()), static_cast<SkScalar>(p.y()));
    }
}

void SkiaRenderer::buildPolygonPath(SkPath& path, const std::vector<CGMPoint>& points) const {
    buildPolylinePath(path, points);
    path.close();
}

void SkiaRenderer::buildArcPath(SkPath& path, const CGMPoint& center, double rx, double ry,
                                 double startAngle, double sweepAngle) const {
    CGMPoint c = transform_.transformPoint(center);
    double scaledRx = transform_.transformLength(rx);
    double scaledRy = transform_.transformLength(ry);

    SkRect oval = SkRect::MakeXYWH(
        static_cast<SkScalar>(c.x() - scaledRx),
        static_cast<SkScalar>(c.y() - scaledRy),
        static_cast<SkScalar>(scaledRx * 2),
        static_cast<SkScalar>(scaledRy * 2));

    // Skia uses degrees, startAngle and sweepAngle are in radians
    SkScalar startDeg = static_cast<SkScalar>(startAngle * 180.0 / M_PI);
    SkScalar sweepDeg = static_cast<SkScalar>(sweepAngle * 180.0 / M_PI);

    // Handle Y-axis flip in arc direction
    if (transform_.isYAxisInverted()) {
        startDeg = -startDeg;
        sweepDeg = -sweepDeg;
    }

    path.arcTo(oval, startDeg, sweepDeg, false);
}

// ============================================================================
// Graphical Primitive Renderers
// ============================================================================

void SkiaRenderer::renderPolyline(Polyline* cmd) {
    if (!cmd || !canvas_ || cmd->points().size() < 2) return;

    SkPath path;
    buildPolylinePath(path, cmd->points());

    SkPaint paint;
    configureStrokePaint(paint);
    canvas_->drawPath(path, paint);
}

void SkiaRenderer::renderDisjointPolyline(DisjointPolyline* cmd) {
    if (!cmd || !canvas_ || cmd->points().size() < 2) return;

    SkPaint paint;
    configureStrokePaint(paint);

    // Draw pairs of points as separate line segments
    const auto& pts = cmd->points();
    for (size_t i = 0; i + 1 < pts.size(); i += 2) {
        CGMPoint p1 = transform_.transformPoint(pts[i]);
        CGMPoint p2 = transform_.transformPoint(pts[i + 1]);
        canvas_->drawLine(
            static_cast<SkScalar>(p1.x()), static_cast<SkScalar>(p1.y()),
            static_cast<SkScalar>(p2.x()), static_cast<SkScalar>(p2.y()),
            paint);
    }
}

void SkiaRenderer::renderPolygon(Polygon* cmd) {
    if (!cmd || !canvas_ || cmd->points().size() < 3) return;

    SkPath path;
    buildPolygonPath(path, cmd->points());

    // Fill if style is not empty or hollow
    int style = current_style_.fillStyle();
    if (style != 4) { // Not empty
        SkPaint fillPaint;
        configureFillPaint(fillPaint);
        canvas_->drawPath(path, fillPaint);
    }

    // Draw edge if visible
    if (current_style_.edgeVisibility()) {
        SkPaint edgePaint;
        configureEdgePaint(edgePaint);
        canvas_->drawPath(path, edgePaint);
    }
}

void SkiaRenderer::renderPolygonSet(PolygonSet* cmd) {
    if (!cmd || !canvas_) return;

    const auto& edges = cmd->edges();
    if (edges.empty()) return;

    SkPath path;
    bool needsMove = true;

    for (const auto& edge : edges) {
        CGMPoint p = transform_.transformPoint(edge.point);

        if (needsMove) {
            path.moveTo(static_cast<SkScalar>(p.x()), static_cast<SkScalar>(p.y()));
            needsMove = false;
        } else {
            path.lineTo(static_cast<SkScalar>(p.x()), static_cast<SkScalar>(p.y()));
        }

        // Edge out flag: 2=close invisible, 3=close visible
        if (edge.edgeOutFlag >= 2) {
            path.close();
            needsMove = true;
        }
    }

    // Fill
    int style = current_style_.fillStyle();
    if (style != 4) {
        SkPaint fillPaint;
        configureFillPaint(fillPaint);
        canvas_->drawPath(path, fillPaint);
    }

    // Draw edges if visible
    if (current_style_.edgeVisibility()) {
        SkPaint edgePaint;
        configureEdgePaint(edgePaint);
        canvas_->drawPath(path, edgePaint);
    }
}

void SkiaRenderer::renderPolymarker(Polymarker* cmd) {
    if (!cmd || !canvas_) return;

    SkPaint paint;
    const Color& c = current_style_.markerColor();
    paint.setColor(SkColorSetARGB(c.a, c.r, c.g, c.b));
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setAntiAlias(antialias_);

    double size = transform_.transformLength(current_style_.markerSize());
    if (size < 2.0) size = 2.0;
    double half = size / 2.0;

    for (const auto& pt : cmd->points()) {
        CGMPoint p = transform_.transformPoint(pt);
        SkScalar x = static_cast<SkScalar>(p.x());
        SkScalar y = static_cast<SkScalar>(p.y());
        SkScalar h = static_cast<SkScalar>(half);

        switch (current_style_.markerType()) {
            case 1: // Dot
                canvas_->drawPoint(x, y, paint);
                break;
            case 2: // Plus
                canvas_->drawLine(x - h, y, x + h, y, paint);
                canvas_->drawLine(x, y - h, x, y + h, paint);
                break;
            case 3: // Asterisk
                canvas_->drawLine(x - h, y, x + h, y, paint);
                canvas_->drawLine(x, y - h, x, y + h, paint);
                canvas_->drawLine(x - h * 0.7f, y - h * 0.7f, x + h * 0.7f, y + h * 0.7f, paint);
                canvas_->drawLine(x - h * 0.7f, y + h * 0.7f, x + h * 0.7f, y - h * 0.7f, paint);
                break;
            case 4: // Circle
                canvas_->drawCircle(x, y, h, paint);
                break;
            case 5: // Cross (X)
                canvas_->drawLine(x - h, y - h, x + h, y + h, paint);
                canvas_->drawLine(x - h, y + h, x + h, y - h, paint);
                break;
            default:
                canvas_->drawPoint(x, y, paint);
                break;
        }
    }
}

void SkiaRenderer::renderCircle(Circle* cmd) {
    if (!cmd || !canvas_) return;

    CGMPoint c = transform_.transformPoint(cmd->center());
    double r = transform_.transformLength(cmd->radius());

    // Fill
    int style = current_style_.fillStyle();
    if (style != 4) {
        SkPaint fillPaint;
        configureFillPaint(fillPaint);
        canvas_->drawCircle(
            static_cast<SkScalar>(c.x()),
            static_cast<SkScalar>(c.y()),
            static_cast<SkScalar>(r),
            fillPaint);
    }

    // Edge
    if (current_style_.edgeVisibility()) {
        SkPaint edgePaint;
        configureEdgePaint(edgePaint);
        canvas_->drawCircle(
            static_cast<SkScalar>(c.x()),
            static_cast<SkScalar>(c.y()),
            static_cast<SkScalar>(r),
            edgePaint);
    }
}

void SkiaRenderer::renderRectangle(Rectangle* cmd) {
    if (!cmd || !canvas_) return;

    CGMPoint p1 = transform_.transformPoint(cmd->firstCorner());
    CGMPoint p2 = transform_.transformPoint(cmd->secondCorner());

    SkScalar left = static_cast<SkScalar>(std::min(p1.x(), p2.x()));
    SkScalar top = static_cast<SkScalar>(std::min(p1.y(), p2.y()));
    SkScalar right = static_cast<SkScalar>(std::max(p1.x(), p2.x()));
    SkScalar bottom = static_cast<SkScalar>(std::max(p1.y(), p2.y()));

    SkRect rect = SkRect::MakeLTRB(left, top, right, bottom);

    // Fill
    int style = current_style_.fillStyle();
    if (style != 4) {
        SkPaint fillPaint;
        configureFillPaint(fillPaint);
        canvas_->drawRect(rect, fillPaint);
    }

    // Edge
    if (current_style_.edgeVisibility()) {
        SkPaint edgePaint;
        configureEdgePaint(edgePaint);
        canvas_->drawRect(rect, edgePaint);
    }
}

void SkiaRenderer::renderEllipse(Ellipse* cmd) {
    if (!cmd || !canvas_) return;

    // Compute ellipse from conjugate diameters
    CGMPoint center = transform_.transformPoint(cmd->center());
    CGMPoint cdp1 = cmd->firstConjugateDiameter();
    CGMPoint cdp2 = cmd->secondConjugateDiameter();

    // Convert conjugate diameters to true ellipse parameters (rx, ry, rotation)
    auto [rx, ry, rotation] = conjugateDiametersToEllipse(
        cdp1.x(), cdp1.y(), cdp2.x(), cdp2.y());

    rx = transform_.transformLength(rx);
    ry = transform_.transformLength(ry);

    canvas_->save();
    canvas_->translate(static_cast<SkScalar>(center.x()), static_cast<SkScalar>(center.y()));
    canvas_->rotate(static_cast<SkScalar>(rotation * 180.0 / M_PI));

    SkRect oval = SkRect::MakeXYWH(
        static_cast<SkScalar>(-rx), static_cast<SkScalar>(-ry),
        static_cast<SkScalar>(rx * 2), static_cast<SkScalar>(ry * 2));

    int style = current_style_.fillStyle();
    if (style != 4) {
        SkPaint fillPaint;
        configureFillPaint(fillPaint);
        canvas_->drawOval(oval, fillPaint);
    }

    if (current_style_.edgeVisibility()) {
        SkPaint edgePaint;
        configureEdgePaint(edgePaint);
        canvas_->drawOval(oval, edgePaint);
    }

    canvas_->restore();
}

void SkiaRenderer::renderText(Text* cmd) {
    if (!cmd || !canvas_) return;

    CGMPoint pos = transform_.transformPoint(cmd->position());

    SkPaint paint;
    configureTextPaint(paint);

    // Calculate font size from text height
    double textHeight = transform_.transformLength(current_style_.textHeight());
    if (textHeight < 6.0) textHeight = 6.0;

    SkFont font;
    font.setSize(static_cast<SkScalar>(textHeight));
    font.setEdging(antialias_ ? SkFont::Edging::kAntiAlias : SkFont::Edging::kAlias);

    canvas_->drawString(cmd->text().c_str(),
        static_cast<SkScalar>(pos.x()),
        static_cast<SkScalar>(pos.y()),
        font, paint);
}

void SkiaRenderer::renderRestrictedText(RestrictedText* cmd) {
    if (!cmd || !canvas_) return;

    // For now, render as regular text
    // TODO: Implement text box constraints
    CGMPoint pos = transform_.transformPoint(cmd->position());

    SkPaint paint;
    configureTextPaint(paint);

    double textHeight = transform_.transformLength(current_style_.textHeight());
    if (textHeight < 6.0) textHeight = 6.0;

    SkFont font;
    font.setSize(static_cast<SkScalar>(textHeight));

    canvas_->drawString(cmd->text().c_str(),
        static_cast<SkScalar>(pos.x()),
        static_cast<SkScalar>(pos.y()),
        font, paint);
}

void SkiaRenderer::renderAppendText(AppendText* cmd) {
    // Append text continues from previous text position
    // For now, skip - proper implementation requires text state tracking
    (void)cmd;
}

void SkiaRenderer::renderCircularArcCentre(CircularArcCentre* cmd) {
    if (!cmd || !canvas_) return;

    CGMPoint center = transform_.transformPoint(cmd->center());
    double r = transform_.transformLength(cmd->radius());

    // Calculate start and end angles from deltas
    double startAngle = std::atan2(cmd->startDelta().y(), cmd->startDelta().x());
    double endAngle = std::atan2(cmd->endDelta().y(), cmd->endDelta().x());

    double sweep = endAngle - startAngle;
    if (sweep < 0) sweep += 2 * M_PI;

    // Handle Y flip
    if (transform_.isYAxisInverted()) {
        startAngle = -startAngle;
        sweep = -sweep;
    }

    SkPath path;
    SkRect oval = SkRect::MakeXYWH(
        static_cast<SkScalar>(center.x() - r),
        static_cast<SkScalar>(center.y() - r),
        static_cast<SkScalar>(r * 2),
        static_cast<SkScalar>(r * 2));

    path.arcTo(oval,
        static_cast<SkScalar>(startAngle * 180.0 / M_PI),
        static_cast<SkScalar>(sweep * 180.0 / M_PI),
        true);

    SkPaint paint;
    configureStrokePaint(paint);
    canvas_->drawPath(path, paint);
}

void SkiaRenderer::renderCircularArcCentreClose(CircularArcCentreClose* cmd) {
    if (!cmd || !canvas_) return;

    CGMPoint center = transform_.transformPoint(cmd->center());
    double r = transform_.transformLength(cmd->radius());

    double startAngle = std::atan2(cmd->startDelta().y(), cmd->startDelta().x());
    double endAngle = std::atan2(cmd->endDelta().y(), cmd->endDelta().x());
    double sweep = endAngle - startAngle;
    if (sweep < 0) sweep += 2 * M_PI;

    if (transform_.isYAxisInverted()) {
        startAngle = -startAngle;
        sweep = -sweep;
    }

    SkPath path;
    SkRect oval = SkRect::MakeXYWH(
        static_cast<SkScalar>(center.x() - r),
        static_cast<SkScalar>(center.y() - r),
        static_cast<SkScalar>(r * 2),
        static_cast<SkScalar>(r * 2));

    if (cmd->closure() == 0) { // Pie
        path.moveTo(static_cast<SkScalar>(center.x()), static_cast<SkScalar>(center.y()));
        path.arcTo(oval,
            static_cast<SkScalar>(startAngle * 180.0 / M_PI),
            static_cast<SkScalar>(sweep * 180.0 / M_PI),
            false);
        path.close();
    } else { // Chord
        path.arcTo(oval,
            static_cast<SkScalar>(startAngle * 180.0 / M_PI),
            static_cast<SkScalar>(sweep * 180.0 / M_PI),
            true);
        path.close();
    }

    int style = current_style_.fillStyle();
    if (style != 4) {
        SkPaint fillPaint;
        configureFillPaint(fillPaint);
        canvas_->drawPath(path, fillPaint);
    }

    if (current_style_.edgeVisibility()) {
        SkPaint edgePaint;
        configureEdgePaint(edgePaint);
        canvas_->drawPath(path, edgePaint);
    }
}

void SkiaRenderer::renderCircularArcCentreReversed(CircularArcCentreReversed* cmd) {
    if (!cmd || !canvas_) return;

    // Same as CircularArcCentre but with reversed direction
    CGMPoint center = transform_.transformPoint(cmd->center());
    double r = transform_.transformLength(cmd->radius());

    double startAngle = std::atan2(cmd->startDelta().y(), cmd->startDelta().x());
    double endAngle = std::atan2(cmd->endDelta().y(), cmd->endDelta().x());

    // Reversed: swap direction
    double sweep = startAngle - endAngle;
    if (sweep > 0) sweep -= 2 * M_PI;

    if (transform_.isYAxisInverted()) {
        endAngle = -endAngle;
        sweep = -sweep;
    }

    SkPath path;
    SkRect oval = SkRect::MakeXYWH(
        static_cast<SkScalar>(center.x() - r),
        static_cast<SkScalar>(center.y() - r),
        static_cast<SkScalar>(r * 2),
        static_cast<SkScalar>(r * 2));

    path.arcTo(oval,
        static_cast<SkScalar>(endAngle * 180.0 / M_PI),
        static_cast<SkScalar>(sweep * 180.0 / M_PI),
        true);

    SkPaint paint;
    configureStrokePaint(paint);
    canvas_->drawPath(path, paint);
}

void SkiaRenderer::renderCircularArc3Point(CircularArc3Point* cmd) {
    if (!cmd || !canvas_) return;

    // Calculate circle from 3 points
    CGMPoint p1 = transform_.transformPoint(cmd->start());
    CGMPoint p2 = transform_.transformPoint(cmd->intermediate());
    CGMPoint p3 = transform_.transformPoint(cmd->end());

    // Circle center from 3 points using perpendicular bisectors
    double ax = p1.x(), ay = p1.y();
    double bx = p2.x(), by = p2.y();
    double cx = p3.x(), cy = p3.y();

    double d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
    if (std::abs(d) < 1e-9) {
        // Points are collinear, draw as line
        SkPath path;
        path.moveTo(static_cast<SkScalar>(ax), static_cast<SkScalar>(ay));
        path.lineTo(static_cast<SkScalar>(bx), static_cast<SkScalar>(by));
        path.lineTo(static_cast<SkScalar>(cx), static_cast<SkScalar>(cy));
        SkPaint paint;
        configureStrokePaint(paint);
        canvas_->drawPath(path, paint);
        return;
    }

    double ux = ((ax * ax + ay * ay) * (by - cy) + (bx * bx + by * by) * (cy - ay) + (cx * cx + cy * cy) * (ay - by)) / d;
    double uy = ((ax * ax + ay * ay) * (cx - bx) + (bx * bx + by * by) * (ax - cx) + (cx * cx + cy * cy) * (bx - ax)) / d;
    double r = std::sqrt((ax - ux) * (ax - ux) + (ay - uy) * (ay - uy));

    double startAngle = std::atan2(ay - uy, ax - ux);
    double endAngle = std::atan2(cy - uy, cx - ux);
    double midAngle = std::atan2(by - uy, bx - ux);

    // Determine sweep direction based on intermediate point
    double sweep = endAngle - startAngle;
    if (sweep < 0) sweep += 2 * M_PI;

    // Check if midpoint is in the arc
    double midCheck = midAngle - startAngle;
    if (midCheck < 0) midCheck += 2 * M_PI;
    if (midCheck > sweep) {
        sweep = sweep - 2 * M_PI;
    }

    SkPath path;
    SkRect oval = SkRect::MakeXYWH(
        static_cast<SkScalar>(ux - r),
        static_cast<SkScalar>(uy - r),
        static_cast<SkScalar>(r * 2),
        static_cast<SkScalar>(r * 2));

    path.arcTo(oval,
        static_cast<SkScalar>(startAngle * 180.0 / M_PI),
        static_cast<SkScalar>(sweep * 180.0 / M_PI),
        true);

    SkPaint paint;
    configureStrokePaint(paint);
    canvas_->drawPath(path, paint);
}

void SkiaRenderer::renderCircularArc3PointClose(CircularArc3PointClose* cmd) {
    if (!cmd || !canvas_) return;

    // Similar to 3-point arc but with closure
    CGMPoint p1 = transform_.transformPoint(cmd->start());
    CGMPoint p2 = transform_.transformPoint(cmd->intermediate());
    CGMPoint p3 = transform_.transformPoint(cmd->end());

    double ax = p1.x(), ay = p1.y();
    double bx = p2.x(), by = p2.y();
    double cx = p3.x(), cy = p3.y();

    double d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
    if (std::abs(d) < 1e-9) return;

    double ux = ((ax * ax + ay * ay) * (by - cy) + (bx * bx + by * by) * (cy - ay) + (cx * cx + cy * cy) * (ay - by)) / d;
    double uy = ((ax * ax + ay * ay) * (cx - bx) + (bx * bx + by * by) * (ax - cx) + (cx * cx + cy * cy) * (bx - ax)) / d;
    double r = std::sqrt((ax - ux) * (ax - ux) + (ay - uy) * (ay - uy));

    double startAngle = std::atan2(ay - uy, ax - ux);
    double endAngle = std::atan2(cy - uy, cx - ux);
    double midAngle = std::atan2(by - uy, bx - ux);

    double sweep = endAngle - startAngle;
    if (sweep < 0) sweep += 2 * M_PI;

    double midCheck = midAngle - startAngle;
    if (midCheck < 0) midCheck += 2 * M_PI;
    if (midCheck > sweep) {
        sweep = sweep - 2 * M_PI;
    }

    SkPath path;
    SkRect oval = SkRect::MakeXYWH(
        static_cast<SkScalar>(ux - r),
        static_cast<SkScalar>(uy - r),
        static_cast<SkScalar>(r * 2),
        static_cast<SkScalar>(r * 2));

    if (cmd->closure() == 0) { // Pie
        path.moveTo(static_cast<SkScalar>(ux), static_cast<SkScalar>(uy));
    }

    path.arcTo(oval,
        static_cast<SkScalar>(startAngle * 180.0 / M_PI),
        static_cast<SkScalar>(sweep * 180.0 / M_PI),
        cmd->closure() != 0);
    path.close();

    int style = current_style_.fillStyle();
    if (style != 4) {
        SkPaint fillPaint;
        configureFillPaint(fillPaint);
        canvas_->drawPath(path, fillPaint);
    }

    if (current_style_.edgeVisibility()) {
        SkPaint edgePaint;
        configureEdgePaint(edgePaint);
        canvas_->drawPath(path, edgePaint);
    }
}

void SkiaRenderer::renderEllipticalArc(EllipticalArc* cmd) {
    if (!cmd || !canvas_) return;

    CGMPoint center = transform_.transformPoint(cmd->center());
    CGMPoint cdp1 = cmd->firstConjugate();
    CGMPoint cdp2 = cmd->secondConjugate();

    // Convert conjugate diameters to true ellipse parameters (rx, ry, rotation)
    auto [rx, ry, rotation] = conjugateDiametersToEllipse(
        cdp1.x(), cdp1.y(), cdp2.x(), cdp2.y());

    rx = transform_.transformLength(rx);
    ry = transform_.transformLength(ry);

    double startAngle = std::atan2(cmd->startDelta().y(), cmd->startDelta().x()) - rotation;
    double endAngle = std::atan2(cmd->endDelta().y(), cmd->endDelta().x()) - rotation;
    double sweep = endAngle - startAngle;
    if (sweep < 0) sweep += 2 * M_PI;

    canvas_->save();
    canvas_->translate(static_cast<SkScalar>(center.x()), static_cast<SkScalar>(center.y()));
    canvas_->rotate(static_cast<SkScalar>(rotation * 180.0 / M_PI));

    SkPath path;
    SkRect oval = SkRect::MakeXYWH(
        static_cast<SkScalar>(-rx), static_cast<SkScalar>(-ry),
        static_cast<SkScalar>(rx * 2), static_cast<SkScalar>(ry * 2));

    path.arcTo(oval,
        static_cast<SkScalar>(startAngle * 180.0 / M_PI),
        static_cast<SkScalar>(sweep * 180.0 / M_PI),
        true);

    SkPaint paint;
    configureStrokePaint(paint);
    canvas_->drawPath(path, paint);

    canvas_->restore();
}

void SkiaRenderer::renderEllipticalArcClose(EllipticalArcClose* cmd) {
    if (!cmd || !canvas_) return;

    CGMPoint center = transform_.transformPoint(cmd->center());
    CGMPoint cdp1 = cmd->firstConjugate();
    CGMPoint cdp2 = cmd->secondConjugate();

    // Convert conjugate diameters to true ellipse parameters (rx, ry, rotation)
    auto [rx, ry, rotation] = conjugateDiametersToEllipse(
        cdp1.x(), cdp1.y(), cdp2.x(), cdp2.y());

    rx = transform_.transformLength(rx);
    ry = transform_.transformLength(ry);

    double startAngle = std::atan2(cmd->startDelta().y(), cmd->startDelta().x()) - rotation;
    double endAngle = std::atan2(cmd->endDelta().y(), cmd->endDelta().x()) - rotation;
    double sweep = endAngle - startAngle;
    if (sweep < 0) sweep += 2 * M_PI;

    canvas_->save();
    canvas_->translate(static_cast<SkScalar>(center.x()), static_cast<SkScalar>(center.y()));
    canvas_->rotate(static_cast<SkScalar>(rotation * 180.0 / M_PI));

    SkPath path;
    SkRect oval = SkRect::MakeXYWH(
        static_cast<SkScalar>(-rx), static_cast<SkScalar>(-ry),
        static_cast<SkScalar>(rx * 2), static_cast<SkScalar>(ry * 2));

    if (cmd->closure() == 0) { // Pie
        path.moveTo(0, 0);
    }

    path.arcTo(oval,
        static_cast<SkScalar>(startAngle * 180.0 / M_PI),
        static_cast<SkScalar>(sweep * 180.0 / M_PI),
        cmd->closure() != 0);
    path.close();

    int style = current_style_.fillStyle();
    if (style != 4) {
        SkPaint fillPaint;
        configureFillPaint(fillPaint);
        canvas_->drawPath(path, fillPaint);
    }

    if (current_style_.edgeVisibility()) {
        SkPaint edgePaint;
        configureEdgePaint(edgePaint);
        canvas_->drawPath(path, edgePaint);
    }

    canvas_->restore();
}

void SkiaRenderer::renderPolyBezier(PolyBezier* cmd) {
    if (!cmd || !canvas_) return;

    const auto& pts = cmd->controlPoints();
    if (pts.size() < 4) return;

    SkPath path;
    CGMPoint p0 = transform_.transformPoint(pts[0]);
    path.moveTo(static_cast<SkScalar>(p0.x()), static_cast<SkScalar>(p0.y()));

    // Cubic Bezier segments: groups of 4 points (or 3 for continuation)
    int continuity = cmd->continuityIndicator();

    if (continuity == 1) { // Discontinuous - each segment is 4 points
        for (size_t i = 0; i + 3 < pts.size(); i += 4) {
            CGMPoint p1 = transform_.transformPoint(pts[i + 1]);
            CGMPoint p2 = transform_.transformPoint(pts[i + 2]);
            CGMPoint p3 = transform_.transformPoint(pts[i + 3]);
            path.cubicTo(
                static_cast<SkScalar>(p1.x()), static_cast<SkScalar>(p1.y()),
                static_cast<SkScalar>(p2.x()), static_cast<SkScalar>(p2.y()),
                static_cast<SkScalar>(p3.x()), static_cast<SkScalar>(p3.y()));

            if (i + 4 < pts.size()) {
                CGMPoint nextStart = transform_.transformPoint(pts[i + 4]);
                path.moveTo(static_cast<SkScalar>(nextStart.x()), static_cast<SkScalar>(nextStart.y()));
            }
        }
    } else { // Continuous - first segment is 4 points, rest are 3
        if (pts.size() >= 4) {
            CGMPoint p1 = transform_.transformPoint(pts[1]);
            CGMPoint p2 = transform_.transformPoint(pts[2]);
            CGMPoint p3 = transform_.transformPoint(pts[3]);
            path.cubicTo(
                static_cast<SkScalar>(p1.x()), static_cast<SkScalar>(p1.y()),
                static_cast<SkScalar>(p2.x()), static_cast<SkScalar>(p2.y()),
                static_cast<SkScalar>(p3.x()), static_cast<SkScalar>(p3.y()));

            for (size_t i = 4; i + 2 < pts.size(); i += 3) {
                p1 = transform_.transformPoint(pts[i]);
                p2 = transform_.transformPoint(pts[i + 1]);
                p3 = transform_.transformPoint(pts[i + 2]);
                path.cubicTo(
                    static_cast<SkScalar>(p1.x()), static_cast<SkScalar>(p1.y()),
                    static_cast<SkScalar>(p2.x()), static_cast<SkScalar>(p2.y()),
                    static_cast<SkScalar>(p3.x()), static_cast<SkScalar>(p3.y()));
            }
        }
    }

    SkPaint paint;
    configureStrokePaint(paint);
    canvas_->drawPath(path, paint);
}

// ============================================================================
// NURBS Implementation
// ============================================================================

double SkiaRenderer::basisFunction(int i, int k, double t, const std::vector<double>& knots) const {
    if (k == 1) {
        if (i < 0 || i + 1 >= static_cast<int>(knots.size())) return 0.0;
        return (t >= knots[i] && t < knots[i + 1]) ? 1.0 : 0.0;
    }

    if (i < 0 || i + k >= static_cast<int>(knots.size())) return 0.0;

    double left = 0.0, right = 0.0;
    double denom1 = knots[i + k - 1] - knots[i];
    double denom2 = knots[i + k] - knots[i + 1];

    if (std::abs(denom1) > 1e-9) {
        left = ((t - knots[i]) / denom1) * basisFunction(i, k - 1, t, knots);
    }
    if (std::abs(denom2) > 1e-9) {
        right = ((knots[i + k] - t) / denom2) * basisFunction(i + 1, k - 1, t, knots);
    }

    return left + right;
}

CGMPoint SkiaRenderer::evaluateNURBS(double t, int order, const std::vector<CGMPoint>& controlPoints,
                                      const std::vector<double>& weights, const std::vector<double>& knots) const {
    double x = 0, y = 0, w = 0;
    int n = static_cast<int>(controlPoints.size());

    for (int i = 0; i < n; ++i) {
        double basis = basisFunction(i, order, t, knots);
        double weight = (i < static_cast<int>(weights.size())) ? weights[i] : 1.0;
        double bw = basis * weight;
        x += bw * controlPoints[i].x();
        y += bw * controlPoints[i].y();
        w += bw;
    }

    if (std::abs(w) > 1e-9) {
        x /= w;
        y /= w;
    }

    return CGMPoint(x, y);
}

void SkiaRenderer::renderNonUniformBSpline(NonUniformBSpline* cmd) {
    if (!cmd || !canvas_) return;

    const auto& pts = cmd->controlPoints();
    const auto& knots = cmd->knots();
    if (pts.size() < 2 || knots.empty()) return;

    int order = cmd->splineOrder();
    double tStart = cmd->startParameter();
    double tEnd = cmd->endParameter();

    // Sample the spline
    SkPath path;
    int samples = std::max(50, static_cast<int>(pts.size() * 10));
    double dt = (tEnd - tStart) / samples;

    std::vector<double> weights(pts.size(), 1.0); // Uniform weights for B-spline

    for (int i = 0; i <= samples; ++i) {
        double t = tStart + i * dt;
        CGMPoint p = evaluateNURBS(t, order, pts, weights, knots);
        CGMPoint tp = transform_.transformPoint(p);

        if (i == 0) {
            path.moveTo(static_cast<SkScalar>(tp.x()), static_cast<SkScalar>(tp.y()));
        } else {
            path.lineTo(static_cast<SkScalar>(tp.x()), static_cast<SkScalar>(tp.y()));
        }
    }

    SkPaint paint;
    configureStrokePaint(paint);
    canvas_->drawPath(path, paint);
}

void SkiaRenderer::renderNonUniformRationalBSpline(NonUniformRationalBSpline* cmd) {
    if (!cmd || !canvas_) return;

    const auto& pts = cmd->controlPoints();
    const auto& weights = cmd->weights();
    const auto& knots = cmd->knots();
    if (pts.size() < 2 || knots.empty()) return;

    int order = cmd->splineOrder();
    double tStart = cmd->startParameter();
    double tEnd = cmd->endParameter();

    SkPath path;
    int samples = std::max(50, static_cast<int>(pts.size() * 10));
    double dt = (tEnd - tStart) / samples;

    for (int i = 0; i <= samples; ++i) {
        double t = tStart + i * dt;
        CGMPoint p = evaluateNURBS(t, order, pts, weights, knots);
        CGMPoint tp = transform_.transformPoint(p);

        if (i == 0) {
            path.moveTo(static_cast<SkScalar>(tp.x()), static_cast<SkScalar>(tp.y()));
        } else {
            path.lineTo(static_cast<SkScalar>(tp.x()), static_cast<SkScalar>(tp.y()));
        }
    }

    SkPaint paint;
    configureStrokePaint(paint);
    canvas_->drawPath(path, paint);
}

void SkiaRenderer::renderCellArray(CellArray* cmd) {
    if (!cmd || !canvas_) return;

    // CellArray is a raster image embedded in the CGM
    int nx = cmd->nx();
    int ny = cmd->ny();
    if (nx <= 0 || ny <= 0) return;

    const auto& colorArray = cmd->colorArray();
    if (colorArray.empty() || colorArray[0].empty()) return;

    // Transform corners
    CGMPoint p = transform_.transformPoint(cmd->cornerP());
    CGMPoint q = transform_.transformPoint(cmd->cornerQ());

    double width = std::abs(q.x() - p.x());
    double height = std::abs(q.y() - p.y());

    // Create pixel buffer
    std::vector<uint32_t> pixels(nx * ny);
    for (int row = 0; row < ny && row < static_cast<int>(colorArray.size()); ++row) {
        for (int col = 0; col < nx && col < static_cast<int>(colorArray[row].size()); ++col) {
            Color c = resolveColor(colorArray[row][col]);
            pixels[row * nx + col] = SkColorSetARGB(c.a, c.r, c.g, c.b);
        }
    }

    SkImageInfo info = SkImageInfo::Make(nx, ny, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
    SkBitmap bitmap;
    bitmap.installPixels(info, pixels.data(), nx * 4);

    SkRect destRect = SkRect::MakeXYWH(
        static_cast<SkScalar>(std::min(p.x(), q.x())),
        static_cast<SkScalar>(std::min(p.y(), q.y())),
        static_cast<SkScalar>(width),
        static_cast<SkScalar>(height));

    canvas_->drawImageRect(bitmap.asImage(), destRect, SkSamplingOptions());
}

void SkiaRenderer::renderBitonalTile(BitonalTile* cmd) {
    if (!cmd || !canvas_) return;

    // BitonalTile is a 1-bit image with foreground/background colors
    int width = cmd->bitmapWidth();
    int height = cmd->bitmapHeight();
    if (width <= 0 || height <= 0) return;

    const auto& imageData = cmd->imageData();
    if (imageData.empty()) return;

    Color bg = resolveColor(cmd->backgroundColor());
    Color fg = resolveColor(cmd->foregroundColor());

    // Create pixel buffer
    std::vector<uint32_t> pixels(width * height);
    int bitIndex = 0;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int byteIndex = bitIndex / 8;
            int bitOffset = 7 - (bitIndex % 8);

            if (byteIndex < static_cast<int>(imageData.size())) {
                bool bit = (imageData[byteIndex] >> bitOffset) & 1;
                const Color& c = bit ? fg : bg;
                pixels[y * width + x] = SkColorSetARGB(c.a, c.r, c.g, c.b);
            }
            bitIndex++;
        }
    }

    SkImageInfo info = SkImageInfo::Make(width, height, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
    SkBitmap bitmap;
    bitmap.installPixels(info, pixels.data(), width * 4);

    // TODO: Get tile position from CGM tile state
    canvas_->drawImage(bitmap.asImage(), 0, 0);
}

} // namespace opencgm

#else // CGM_SKIA_RENDERER_ENABLED not defined

// Stub implementations when Skia is not available
namespace opencgm {

RenderTransform::RenderTransform()
    : vdc_x1_(0), vdc_y1_(0), vdc_x2_(32767), vdc_y2_(32767)
    , output_width_(800), output_height_(600)
    , flip_y_(true), vdc_y_down_(false) {}

void RenderTransform::setVdcExtent(double, double, double, double) {}
void RenderTransform::setOutputSize(double, double) {}
void RenderTransform::setFlipY(bool) {}
void RenderTransform::transform(double, double, double&, double&) const {}
CGMPoint RenderTransform::transformPoint(const CGMPoint& pt) const { return pt; }
double RenderTransform::transformLength(double l) const { return l; }
void RenderTransform::getVdcExtent(double&, double&, double&, double&) const {}
double RenderTransform::scaleX() const { return 1.0; }
double RenderTransform::scaleY() const { return 1.0; }

RenderStyle::RenderStyle()
    : line_color_(0, 0, 0), line_width_(1.0), line_type_(1)
    , line_cap_(LineCapIndicator::BUTT), line_join_(JoinIndicator::MITER)
    , fill_color_(255, 255, 255), fill_style_(1), hatch_index_(1), pattern_index_(1)
    , edge_color_(0, 0, 0), edge_width_(1.0), edge_type_(1), edge_visibility_(false)
    , edge_cap_(LineCapIndicator::BUTT), edge_join_(JoinIndicator::MITER)
    , text_color_(0, 0, 0), text_height_(12.0), text_h_align_(0), text_v_align_(0)
    , font_index_(1), font_family_("Arial"), character_spacing_(0.0), character_expansion_(1.0)
    , marker_type_(3), marker_size_(8.0), marker_color_(0, 0, 0) {}

void RenderStyle::setTextAlignment(int, int) {}
std::vector<float> RenderStyle::getLineDashPattern(double) const { return {}; }
std::vector<float> RenderStyle::getEdgeDashPattern(double) const { return {}; }

SkiaRenderer::SkiaRenderer(CGMFile*) : cgm_file_(nullptr), canvas_(nullptr)
    , bg_r_(255), bg_g_(255), bg_b_(255), bg_a_(255)
    , antialias_(true), dpi_(96), in_picture_(false), current_picture_index_(0) {}

SkiaRenderer::~SkiaRenderer() = default;

void SkiaRenderer::setBackgroundColor(uint8_t, uint8_t, uint8_t, uint8_t) {}
void SkiaRenderer::setAntiAlias(bool) {}
void SkiaRenderer::setDpi(int) {}

SkiaRenderResult SkiaRenderer::render(const SkiaRenderOptions&) {
    SkiaRenderResult result;
    result.error_message = "Skia renderer not available - rebuild with ENABLE_SKIA_RENDERER=ON";
    return result;
}

bool SkiaRenderer::renderToBuffer(int, int, double, uint8_t**, size_t*, int*, int*) {
    return false;
}

void SkiaRenderer::freeBuffer(uint8_t* buffer) {
    delete[] buffer;
}

bool SkiaRenderer::initializeSurface(int, int) { return false; }
void SkiaRenderer::calculateOutputDimensions(const SkiaRenderOptions&, int&, int&) {}
Color SkiaRenderer::resolveColor(const CGMColor&) const { return Color(0, 0, 0); }
void SkiaRenderer::initializeDefaultColorTable() {}
void SkiaRenderer::processCommands() {}
void SkiaRenderer::processCommand(Command*) {}
void SkiaRenderer::renderPolyline(Polyline*) {}
void SkiaRenderer::renderDisjointPolyline(DisjointPolyline*) {}
void SkiaRenderer::renderPolygon(Polygon*) {}
void SkiaRenderer::renderPolygonSet(PolygonSet*) {}
void SkiaRenderer::renderPolymarker(Polymarker*) {}
void SkiaRenderer::renderCircle(Circle*) {}
void SkiaRenderer::renderEllipse(Ellipse*) {}
void SkiaRenderer::renderEllipticalArc(EllipticalArc*) {}
void SkiaRenderer::renderEllipticalArcClose(EllipticalArcClose*) {}
void SkiaRenderer::renderCircularArcCentre(CircularArcCentre*) {}
void SkiaRenderer::renderCircularArcCentreClose(CircularArcCentreClose*) {}
void SkiaRenderer::renderCircularArcCentreReversed(CircularArcCentreReversed*) {}
void SkiaRenderer::renderCircularArc3Point(CircularArc3Point*) {}
void SkiaRenderer::renderCircularArc3PointClose(CircularArc3PointClose*) {}
void SkiaRenderer::renderRectangle(Rectangle*) {}
void SkiaRenderer::renderText(Text*) {}
void SkiaRenderer::renderRestrictedText(RestrictedText*) {}
void SkiaRenderer::renderAppendText(AppendText*) {}
void SkiaRenderer::renderPolyBezier(PolyBezier*) {}
void SkiaRenderer::renderNonUniformBSpline(NonUniformBSpline*) {}
void SkiaRenderer::renderNonUniformRationalBSpline(NonUniformRationalBSpline*) {}
void SkiaRenderer::renderCellArray(CellArray*) {}
void SkiaRenderer::renderBitonalTile(BitonalTile*) {}
void SkiaRenderer::processAttributeCommand(Command*) {}
void SkiaRenderer::processBeginPicture(BeginPicture*) {}
void SkiaRenderer::processEndPicture(EndPicture*) {}
void SkiaRenderer::processVdcExtent(Command*) {}
void SkiaRenderer::configureStrokePaint(SkPaint&) const {}
void SkiaRenderer::configureFillPaint(SkPaint&) const {}
void SkiaRenderer::configureEdgePaint(SkPaint&) const {}
void SkiaRenderer::configureTextPaint(SkPaint&) const {}
void SkiaRenderer::buildPolylinePath(SkPath&, const std::vector<CGMPoint>&) const {}
void SkiaRenderer::buildPolygonPath(SkPath&, const std::vector<CGMPoint>&) const {}
void SkiaRenderer::buildArcPath(SkPath&, const CGMPoint&, double, double, double, double) const {}
double SkiaRenderer::basisFunction(int, int, double, const std::vector<double>&) const { return 0; }
CGMPoint SkiaRenderer::evaluateNURBS(double, int, const std::vector<CGMPoint>&,
                                      const std::vector<double>&, const std::vector<double>&) const {
    return CGMPoint(0, 0);
}

} // namespace opencgm

#endif // CGM_SKIA_RENDERER_ENABLED
