#ifndef OPENCGM_SVG_CONVERTER_H
#define OPENCGM_SVG_CONVERTER_H

#include "cgm_file.h"
#include "cgm_point.h"
#include "command.h"
#include "opencgm/enums.h"
#include "svg/aps_policy.h"
#include "svg/attribute_manager.h"
#include "svg/color_resolver.h"
#include "svg/conversion_plan.h"
#include "svg/document_serializer.h"
#include "svg/internal_types.h"
#include "svg/tile_geometry.h"
#include "svg/xml_utils.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <iomanip>
#include <map>
#include <vector>
#include <optional>
#include <iostream>
#include <utility>
#include <cmath>
#include <memory>
#include <regex>
#include <set>
#include <cstdint>

// Forward declarations to avoid including stb headers in the public interface
struct stbtt_fontinfo;

namespace opencgm {

// LoadedFont is now defined in svg/internal_types.h

struct RasterMetrics {
    enum class Kind {
        CellArray,
        BitonalTile
    };

    Kind kind = Kind::CellArray;
    int picture_index = -1;
    size_t command_index = 0;
    int pixel_width = 0;
    int pixel_height = 0;
    size_t transparent_pixels = 0;
    size_t opaque_pixels = 0;
    bool tcc_active = false;
    bool had_tcc_match = false;
    CGMPoint origin;
    CGMPoint path_vector;
    CGMPoint line_vector;
    bool has_transform = false;
    double transform[6] = {0, 0, 0, 0, 0, 0};
    std::string mime_type;
    std::string compression;
};

struct GeometryMetrics {
    bool has_geometry = false;
    double geometry_min_x = 0.0;
    double geometry_min_y = 0.0;
    double geometry_max_x = 0.0;
    double geometry_max_y = 0.0;
    double geometry_width = 0.0;
    double geometry_height = 0.0;

    double picture_min_x = 0.0;
    double picture_min_y = 0.0;
    double picture_max_x = 0.0;
    double picture_max_y = 0.0;
    double picture_width = 0.0;
    double picture_height = 0.0;

    double coverage_x = 1.0;
    double coverage_y = 1.0;
    bool auto_fit_applied = false;
    double auto_fit_margin_x = 0.0;
    double auto_fit_margin_y = 0.0;

    bool view_context_present = false;
    bool view_context_multiple = false;
    bool view_context_adopted = false;
    double view_context_min_x = 0.0;
    double view_context_min_y = 0.0;
    double view_context_max_x = 0.0;
    double view_context_max_y = 0.0;

    double canvas_width = 0.0;
    double canvas_height = 0.0;
    double scale_factor = 1.0;
    bool flip_y_applied = true;

    double viewbox_x = 0.0;
    double viewbox_y = 0.0;
    double viewbox_width = 0.0;
    double viewbox_height = 0.0;

    bool compatibility_mode = false;
};

// Forward declarations
class Polyline;
class DisjointPolyline;
class Polygon;
class PolygonSet;
class Polymarker;
class Circle;
class Rectangle;
class Ellipse;
class EllipticalArc;
class EllipticalArcClose;
class Text;
class RestrictedText;
class CircularArcCentre;
class CircularArc3Point;
class CircularArc3PointClose;
class CircularArcCentreClose;
class CircularArcCentreReversed;
class AppendText;
class PolyBezier;
class GeneralizedDrawingPrimitive;
class BeginPicture;
class BeginPictureBody;
class EndPicture;
class NonUniformBSpline;
class NonUniformRationalBSpline;
class HyperbolicArc;
class ParabolicArc;
class CellArray;
class BitonalTile;
class Tile;
class TileElement;
class PolySymbol;
class BeginTileArray;
class EndTileArray;
class SymbolLibraryList;
class MaximumColourIndex;
class ColourValueExtent;
class ConnectingEdge;
class FontList;
class ClipRectangle;
class ClipIndicator;
class ProtectionRegionIndicator;
class BeginProtectionRegion;
class EndProtectionRegion;
class BeginApplicationStructure;
class BeginApplicationStructureBody;
class EndApplicationStructure;
class ApplicationStructureAttribute;
class PatternTable;
class PatternSize;
class PatternIndex;
class HatchStyleDefinition;
class FillReferencePoint;
class LineWidthSpecificationMode;
class EdgeWidthSpecificationMode;
class TransparentCellColour;
class FillRepresentation;
class FillBundleIndex;
class LineColour;
class FillColour;
class LineWidth;
class LineType;
class LineTypeContinuation;
class LineTypeInitialOffset;
class LineAndEdgeTypeDefinition;
class InteriorStyle;
class HatchIndex;
class EdgeColour;
class EdgeWidth;
class EdgeType;
class EdgeTypeContinuation;
class EdgeTypeInitialOffset;
class EdgeVisibility;
class LineCap;
class LineJoin;
class MitreLimit;
class EdgeCap;
class EdgeJoin;
class Escape;
class TextColour;
class TextFontIndex;
class TextPrecision;
class CharacterExpansionFactor;
class CharacterSpacing;
class CharacterHeight;
class CharacterOrientation;
class TextAlignment;
class MarkerType;
class MarkerSize;
class MarkerColour;
class ColourTable;
class ColourSelectionMode;

struct PatternTableData {
    int nx = 0;
    int ny = 0;
    std::vector<Color> cells;
};

struct PatternSizeData {
    double widthX = 1.0;
    double widthY = 0.0;
    double heightX = 0.0;
    double heightY = 1.0;
};

struct HatchDefinition {
    int styleIndicator = 0;
    CGMPoint direction;
    CGMPoint spacing;
};

struct FillBundleEntry {
    int interiorStyle = 1;
    CGMColor color;
    int hatchIndex = 1;
    int patternIndex = 1;
};

// Text segment for APPEND TEXT sequences - stores text + attributes captured at segment creation
struct PendingTextSegment {
    std::string text;
    Color color;
    double height = 12.0;
    std::string font_family;
    double letter_spacing = 0.0;
    double expansion = 1.0;
};

// ============================================================================
// Coordinate Transformation
// ============================================================================

class CoordinateTransform {
public:
    CoordinateTransform() :
        vdc_x1_(0), vdc_y1_(0), vdc_x2_(32767), vdc_y2_(32767),
        svg_width_(800), svg_height_(600),
        flip_y_(true),
        vdc_x_left_(false),
        vdc_y_down_(false) {}

    void setVdcExtent(double x1, double y1, double x2, double y2) {
        vdc_x1_ = x1;
        vdc_y1_ = y1;
        vdc_x2_ = x2;
        vdc_y2_ = y2;
        vdc_x_left_ = (x1 > x2);  // X decreases left-to-right
        vdc_y_down_ = (y1 > y2);
    }

    void setSvgSize(double width, double height) {
        svg_width_ = width;
        svg_height_ = height;
    }

    void setFlipY(bool flip) {
        flip_y_ = flip;
    }

    bool isYAxisInverted() const {
        return flip_y_ && !vdc_y_down_;
    }

    bool isXAxisInverted() const {
        return vdc_x_left_;
    }

    bool isVdcYDown() const {
        return vdc_y_down_;
    }

    // Transform VDC point to SVG coordinates
    void transform(double vdc_x, double vdc_y, double& svg_x, double& svg_y) const {
        double vdc_width = vdc_x2_ - vdc_x1_;
        double vdc_height = vdc_y2_ - vdc_y1_;

        if (vdc_width == 0) vdc_width = 1;
        if (vdc_height == 0) vdc_height = 1;

        // Detect VDC coordinate system orientation
        // If vdc_y1 > vdc_y2, the VDC already has +Y down (top-left origin)
        bool vdc_y_down = (vdc_y1_ > vdc_y2_);

        // Normalize to 0-1
        double norm_x = (vdc_x - vdc_x1_) / vdc_width;
        double norm_y;

        if (vdc_y_down) {
            // VDC has +Y down (y1 > y2, e.g. y1=1000 is bottom, y2=0 is top)
            // Both VDC and SVG have same orientation: Y increases downward
            // Direct mapping: VDC Y=y2 (top) → SVG Y=0, VDC Y=y1 (bottom) → SVG Y=height
            double abs_height = std::abs(vdc_height);
            norm_y = (vdc_y - vdc_y2_) / abs_height;
        } else {
            // VDC has +Y up: normalize relative to bottom
            norm_y = (vdc_y - vdc_y1_) / vdc_height;
        }

        // Scale to SVG size
        svg_x = norm_x * svg_width_;
        svg_y = norm_y * svg_height_;

        // Flip Y if needed (CGM typically has Y increasing upward, SVG has Y increasing downward)
        // But don't flip if VDC already has Y down
        if (flip_y_ && !vdc_y_down) {
            svg_y = svg_height_ - svg_y;
        }
    }

    CGMPoint transformPoint(const CGMPoint& pt) const {
        double svg_x, svg_y;
        transform(pt.x(), pt.y(), svg_x, svg_y);
        return CGMPoint(svg_x, svg_y);
    }

    double transformLength(double vdc_length) const {
        double vdc_width = vdc_x2_ - vdc_x1_;
        if (vdc_width == 0) return vdc_length;
        // Use absolute value to handle X-axis inversion
        return (vdc_length / std::abs(vdc_width)) * svg_width_;
    }

    void getVdcExtent(double& x1, double& y1, double& x2, double& y2) const {
        x1 = vdc_x1_;
        y1 = vdc_y1_;
        x2 = vdc_x2_;
        y2 = vdc_y2_;
    }

    bool getFlipY() const {
        return flip_y_;
    }

    double scaleX() const {
        double width = vdc_x2_ - vdc_x1_;
        if (width == 0.0)
        {
            return 1.0;
        }
        // Use absolute value to handle X-axis inversion
        return svg_width_ / std::abs(width);
    }

    double scaleY() const {
        double height = vdc_y2_ - vdc_y1_;
        if (height == 0.0)
        {
            return 1.0;
        }
        // Use absolute value to handle Y-axis inversion
        return svg_height_ / std::abs(height);
    }

private:
    double vdc_x1_, vdc_y1_, vdc_x2_, vdc_y2_;
    double svg_width_, svg_height_;
    bool flip_y_;
    bool vdc_x_left_;
    bool vdc_y_down_;
};

// ============================================================================
// SVG Style Manager
// ============================================================================

class SVGStyle {
public:
    SVGStyle() :
        line_color_(Color::Black()),
        // ISO 8632-1 §A.1: default FILL COLOUR is colour-index-1 (foreground),
        // which resolves to black for the standard palette and for monochrome
        // CGMs (where index 1 is the only foreground colour). Any CGM that
        // wants a non-foreground fill must emit FILL COLOUR explicitly.
        fill_color_(Color::Black()),
        line_width_(1.0),
        line_type_(1),
        fill_style_(0),
        hatch_index_(1),
        edge_visibility_(false),  // ISO 8632-1: EDGE VISIBILITY defaults to OFF
        edge_color_(Color::Black()),
        edge_width_(1.0),
        edge_type_(1),
        text_color_(Color::Black()),
        text_height_(12.0),
        original_text_height_(12.0),
        character_height_explicit_(false),
        line_color_explicit_(false),
        fill_color_explicit_(false),
        edge_color_explicit_(false),
        text_color_explicit_(false),
        marker_color_explicit_(false),
        text_h_align_(0),
        text_v_align_(0),
        marker_type_(1),
        marker_size_(1.0),
        marker_color_(Color::Black()),
        line_cap_(LineCapIndicator::UNSPECIFIED),
        line_join_(JoinIndicator::UNSPECIFIED),
        edge_cap_(LineCapIndicator::UNSPECIFIED),
        edge_join_(JoinIndicator::UNSPECIFIED),
        font_index_(1),
        character_spacing_(0.0),
        character_expansion_(1.0),
        font_family_("Arial, Helvetica, sans-serif"),
        pattern_index_(1),
        line_dash_continuation_(true),
        line_dash_offset_(0.0),
        edge_dash_continuation_(true),
        edge_dash_offset_(0.0),
        text_precision_(0),
        miter_limit_(10.0),
        opacity_(1.0) {}

    void setLineColor(const Color& color) { line_color_ = color; }
    void setFillColor(const Color& color) { fill_color_ = color; }
    void setLineWidth(double width) { line_width_ = width; }
    void setLineType(int type) {
        line_type_ = type;
        // Reset any cached custom-pattern from a prior LINE TYPE DEFINITION
        // selection — the next one will set it explicitly via setLineTypeCustomDash.
        line_type_custom_dash_.clear();
    }
    void setLineTypeCustomDash(std::string dash) {
        line_type_custom_dash_ = std::move(dash);
    }
    void setFillStyle(int style) { fill_style_ = style; }
    void setHatchIndex(int index) { hatch_index_ = index; }
    void setEdgeVisibility(bool visible) { edge_visibility_ = visible; }
    void setEdgeColor(const Color& color) { edge_color_ = color; }
    void setEdgeWidth(double width) { edge_width_ = width; }
    void setEdgeType(int type) {
        edge_type_ = type;
        edge_type_custom_dash_.clear();
    }
    void setEdgeTypeCustomDash(std::string dash) {
        edge_type_custom_dash_ = std::move(dash);
    }
    void setTextColor(const Color& color) { text_color_ = color; }
    void setTextHeight(double height) { text_height_ = height; }
    void setOriginalTextHeight(double height) { original_text_height_ = height; }
    void markCharacterHeightExplicit() { character_height_explicit_ = true; }
    bool characterHeightExplicit() const { return character_height_explicit_; }

    // Explicit-tracking flags for ISO 8632-1 §A.1 default colours
    // (default = colour-index-1 in INDEXED mode). When the CGM hasn't
    // emitted an explicit LINE/FILL/EDGE/TEXT/MARKER COLOUR command,
    // the engine refreshes the corresponding *_color_ from
    // color_table_[1] whenever that index is updated by COLR-TABLE.
    void markLineColorExplicit() { line_color_explicit_ = true; }
    bool lineColorExplicit() const { return line_color_explicit_; }
    void markFillColorExplicit() { fill_color_explicit_ = true; }
    bool fillColorExplicit() const { return fill_color_explicit_; }
    void markEdgeColorExplicit() { edge_color_explicit_ = true; }
    bool edgeColorExplicit() const { return edge_color_explicit_; }
    void markTextColorExplicit() { text_color_explicit_ = true; }
    bool textColorExplicit() const { return text_color_explicit_; }
    void markMarkerColorExplicit() { marker_color_explicit_ = true; }
    bool markerColorExplicit() const { return marker_color_explicit_; }
    void setTextAlignment(int h_align, int v_align) {
        text_h_align_ = h_align;
        text_v_align_ = v_align;
    }
    void setMarkerType(int type) { marker_type_ = type; }
    void setMarkerSize(double size) { marker_size_ = size; }
    void setMarkerColor(const Color& color) { marker_color_ = color; }
    void setLineCap(LineCapIndicator indicator) { line_cap_ = indicator; }
    void setLineJoin(JoinIndicator indicator) { line_join_ = indicator; }
    void setEdgeCap(LineCapIndicator indicator) { edge_cap_ = indicator; }
    void setEdgeJoin(JoinIndicator indicator) { edge_join_ = indicator; }
    void setMiterLimit(double limit) { miter_limit_ = limit; }
    void setFontIndex(int index) { font_index_ = std::max(index, 1); }
    void setFontFamily(const std::string& family) { font_family_ = family; }
    void setCharacterSpacing(double spacing) { character_spacing_ = spacing; }
    void setCharacterExpansion(double expansion) { character_expansion_ = expansion; }
    void setPatternIndex(int index) { pattern_index_ = index; }
    void setLineDashContinuation(bool cont) { line_dash_continuation_ = cont; }
    void setLineDashOffset(double offset) { line_dash_offset_ = offset; }
    void setEdgeDashContinuation(bool cont) { edge_dash_continuation_ = cont; }
    void setEdgeDashOffset(double offset) { edge_dash_offset_ = offset; }
    void setTextPrecision(int precision) { text_precision_ = precision; }
    void setOpacity(double opacity) { opacity_ = std::max(0.0, std::min(1.0, opacity)); }

    double lineWidth() const { return line_width_; }
    double edgeWidth() const { return edge_width_; }
    int lineType() const { return line_type_; }
    const Color& lineColor() const { return line_color_; }
    const Color& edgeColor() const { return edge_color_; }
    int edgeType() const { return edge_type_; }

    const Color& textColor() const { return text_color_; }
    double textHeight() const { return text_height_; }
    double originalTextHeight() const { return original_text_height_; }
    int textHAlign() const { return text_h_align_; }
    int textVAlign() const { return text_v_align_; }
    int fillStyle() const { return fill_style_; }
    int hatchIndex() const { return hatch_index_; }
    bool edgeVisibility() const { return edge_visibility_; }
    int markerType() const { return marker_type_; }
    double markerSize() const { return marker_size_; }
    const Color& markerColor() const { return marker_color_; }
    const Color& fillColor() const { return fill_color_; }
    int fontIndex() const { return font_index_; }
    const std::string& fontFamily() const { return font_family_; }
    double characterSpacing() const { return character_spacing_; }
    double characterExpansion() const { return character_expansion_; }
    int patternIndex() const { return pattern_index_; }

    std::string getStrokeStyle() const {
        std::ostringstream ss;
        ss << "stroke=\"" << colorToHex(line_color_) << "\" ";
        ss << "stroke-width=\"" << formatNumber(line_width_) << "\" ";

        bool dashHasDot = false;
        if (line_type_ != 1) {
            // Only emit the attribute when we have an actual dash pattern.
            // Emitting stroke-dasharray="" is invalid SVG (empty value) and
            // resvg silently renders the stroke as solid, masking the
            // unsupported line type as a "solid line" bug instead of letting
            // it fall through to whatever default the renderer applies.
            std::string dash = getStrokeDashPattern();
            if (!dash.empty()) {
                ss << "stroke-dasharray=\"" << dash << "\" ";
                dashHasDot = dashContainsZeroMark(dash);
            }
        }
        // Always output line cap and join for WebCGM compatibility. When the
        // dash pattern contains a CGM "dot" mark (0-length entry), force round
        // linecap so the dot renders as a visible circle; butt would render
        // 0-length to nothing and lose the dot.
        const char* cap = dashHasDot ? "round" : toSvgLineCap(line_cap_);
        ss << "stroke-linecap=\"" << cap << "\" ";
        ss << "stroke-linejoin=\"" << toSvgLineJoin(line_join_) << "\" ";

        if (line_join_ == JoinIndicator::MITER || line_join_ == JoinIndicator::UNSPECIFIED) {
            ss << "stroke-miterlimit=\"" << formatNumber(miter_limit_) << "\" ";
        }

        if (opacity_ < 1.0) {
            ss << "stroke-opacity=\"" << formatNumber(opacity_) << "\" ";
        }

        return ss.str();
    }

    std::string getFillStyle() const {
        std::ostringstream ss;

        if (fill_style_ == 0 || fill_style_ == 4) {
            // Hollow (0) or Empty (4) - no fill
            ss << "fill=\"none\" ";
        } else if (fill_style_ == 1) {
            // Solid fill - use the fill color as-is
            ss << "fill=\"" << colorToHex(fill_color_) << "\" ";
            if (opacity_ < 1.0) {
                ss << "fill-opacity=\"" << formatNumber(opacity_) << "\" ";
            }
        } else if (fill_style_ == 3) {
            // Hatch fill - render as solid fill
            ss << "fill=\"" << colorToHex(fill_color_) << "\" ";
            if (opacity_ < 1.0) {
                ss << "fill-opacity=\"" << formatNumber(opacity_) << "\" ";
            }
        } else {
            // Other patterns (2=pattern, etc.) - use the fill color
            ss << "fill=\"" << colorToHex(fill_color_) << "\" ";
            if (opacity_ < 1.0) {
                ss << "fill-opacity=\"" << formatNumber(opacity_) << "\" ";
            }
        }

        return ss.str();
    }

    std::string getStyle() const {
        return getFillStyle() + getStrokeStyle();
    }

    std::string getEdgeStyle() const {
        std::ostringstream ss;

        // INTERIOR STYLE = HOLLOW (0) per CGM:1999 / WebCGM 2.1 is defined by
        // its outline — the perimeter is always drawn, independent of
        // EDGE VISIBILITY. EMPTY (4) continues to honor EDGE VISIBILITY
        // (nothing-is-drawn semantics).
        bool hollowForcesEdge = (fill_style_ == 0);

        if (!edge_visibility_ && !hollowForcesEdge) {
            ss << "stroke=\"none\" ";
        } else {
            ss << "stroke=\"" << colorToHex(edge_color_) << "\" ";
            // Clamp zero-width edges to a single unit so the outline is
            // actually visible; a literal zero width is a common CGM-author
            // shortcut for "smallest drawable line".
            double effectiveEdgeWidth = (edge_width_ > 0.0) ? edge_width_ : 1.0;
            ss << "stroke-width=\"" << formatNumber(effectiveEdgeWidth) << "\" ";

            bool dashHasDot = false;
            if (edge_type_ != 1) {
                std::string dash = getEdgeDashPattern();
                if (!dash.empty()) {
                    ss << "stroke-dasharray=\"" << dash << "\" ";
                    dashHasDot = dashContainsZeroMark(dash);
                }
            }
            // Round linecap for dot marks (0-length dashes); see getStrokeStyle.
            const char* cap = dashHasDot ? "round" : toSvgLineCap(edge_cap_);
            ss << "stroke-linecap=\"" << cap << "\" ";
            ss << "stroke-linejoin=\"" << toSvgLineJoin(edge_join_) << "\" ";

            if (edge_join_ == JoinIndicator::MITER || edge_join_ == JoinIndicator::UNSPECIFIED) {
                ss << "stroke-miterlimit=\"" << formatNumber(miter_limit_) << "\" ";
            }
        }

        return ss.str();
    }

    std::string getStyleWithEdges() const {
        return getFillStyle() + getEdgeStyle();
    }

private:
    // Scale a hardcoded dash pattern (values in stroke-widths) by an absolute
    // stroke width so the emitted dasharray values are in user-space units.
    // With width 0 or 1 (defaults) the pattern looks the same as the literal
    // constants below; with width 100 (common for CGMs whose VDC is 1000s of
    // units) each dash becomes ~100 * constant and stays visible through
    // rasterization.
    std::string scaleDash(const char* pattern, double width) const {
        if (!pattern || *pattern == '\0') return "";
        double scale = (width > 0.0) ? width : 1.0;
        std::ostringstream ss;
        const char* p = pattern;
        bool first = true;
        while (*p) {
            char* end = nullptr;
            double v = std::strtod(p, &end);
            if (end == p) break;
            if (!first) ss << ",";
            ss << formatNumber(v * scale);
            first = false;
            p = end;
            while (*p == ',' || *p == ' ') ++p;
        }
        return ss.str();
    }

    std::string getEdgeDashPattern() const {
        // ISO 8632-1: LINE AND EDGE TYPE DEFINITION installs user-defined
        // dash patterns shared by both LINE TYPE and EDGE TYPE. Honour an
        // installed custom pattern before falling back to the built-in
        // type table — without this, vendor-extended edge types (e.g.
        // LINSTD03's negative indices for "decreasing", "long gaps", etc.)
        // silently render solid.
        if (!edge_type_custom_dash_.empty()) {
            return scaleDash(edge_type_custom_dash_.c_str(), edge_width_);
        }
        // Edge types use same patterns as line types (CGM/ISO 8632-1)
        const char* base = "";
        switch (edge_type_) {
            case 2:  base = "5,5";           break; // dash
            case 3:  base = "2,2";           break; // dot
            case 4:  base = "5,5,2,5";       break; // dash-dot
            case 5:  base = "5,5,2,5,2,5";   break; // dash-dot-dot
            // Extended registered edge types
            case 9:  base = "2,3";           break; // stitch
            case 10: base = "10,3,3,3";      break; // chain
            case 11: base = "15,5,3,5";      break; // center
            case 12: base = "6,4";           break; // hidden
            case 13: base = "15,5,3,5,3,5";  break; // phantom
            case 14: base = "10,2,2,2,10";   break; // break short
            case 15: base = "15,2,2,2,15";   break; // break long
            default: return "";
        }
        return scaleDash(base, edge_width_);
    }
    static const char* toSvgLineCap(LineCapIndicator indicator) {
        switch (indicator) {
            case LineCapIndicator::BUTT: return "butt";
            case LineCapIndicator::ROUND: return "round";
            case LineCapIndicator::PROJECTING_SQUARE: return "square";
            case LineCapIndicator::UNSPECIFIED: return "butt"; // CGM default maps to SVG butt
            default: return "butt";
        }
    }
    // CGM dot marks encode as 0-length dashes. With butt linecap a 0-length
    // dash renders as nothing, swallowing the dot entirely. Detect dot marks
    // (even-indexed entries == 0) so the caller can force round linecap, which
    // makes a 0-length dash render as a circle of stroke-width diameter.
    static bool dashContainsZeroMark(const std::string& dash) {
        if (dash.empty()) return false;
        size_t pos = 0;
        size_t idx = 0;
        while (pos < dash.size()) {
            size_t comma = dash.find(',', pos);
            size_t len = (comma == std::string::npos) ? dash.size() - pos : comma - pos;
            if ((idx & 1u) == 0u) {
                double v = std::atof(dash.c_str() + pos);
                if (v == 0.0 && len > 0) return true;
            }
            if (comma == std::string::npos) break;
            pos = comma + 1;
            ++idx;
        }
        return false;
    }
    static const char* toSvgLineJoin(JoinIndicator indicator) {
        switch (indicator) {
            case JoinIndicator::MITER: return "miter";
            case JoinIndicator::ROUND: return "round";
            case JoinIndicator::BEVEL: return "bevel";
            case JoinIndicator::UNSPECIFIED: return "miter"; // CGM default maps to SVG miter
            default: return "miter";
        }
    }
    std::string getStrokeDashPattern() const {
        // If the metafile has installed a custom dash pattern via LINE AND
        // EDGE TYPE DEFINITION (e.g. LINSTD01 vendor-extended types -1..-8),
        // use it directly. Otherwise fall back to the standard CGM mapping.
        if (!line_type_custom_dash_.empty()) {
            return scaleDash(line_type_custom_dash_.c_str(), line_width_);
        }
        // CGM Standard Line Types (ISO/IEC 8632-1):
        // 1 = Solid (no dash pattern)
        // 2 = Dash
        // 3 = Dot
        // 4 = Dash-dot
        // 5 = Dash-dot-dot
        // Registered Line Types (ISO 9973):
        // 6-8 = Arrow types (require SVG markers - handled separately)
        // 9 = Stitch line (short dashes)
        // 10 = Chain line (long dash, short dash)
        // 11 = Center line (long dash, short dash pattern)
        // 12 = Hidden line (medium dashes)
        // 13 = Phantom line (long dash, two short dashes)
        // 14 = Break line short
        // 15 = Break line long
        const char* base = "";
        switch (line_type_) {
            case 2:  base = "5,5";           break; // dash
            case 3:  base = "2,2";           break; // dot
            case 4:  base = "5,5,2,5";       break; // dash-dot
            case 5:  base = "5,5,2,5,2,5";   break; // dash-dot-dot
            // Extended registered line types
            case 9:  base = "2,3";           break; // stitch
            case 10: base = "10,3,3,3";      break; // chain
            case 11: base = "15,5,3,5";      break; // center
            case 12: base = "6,4";           break; // hidden
            case 13: base = "15,5,3,5,3,5";  break; // phantom
            case 14: base = "10,2,2,2,10";   break; // break short
            case 15: base = "15,2,2,2,15";   break; // break long
            default: return "";              // solid or unknown
        }
        return scaleDash(base, line_width_);
    }

    std::string colorToHex(const Color& color) const {
        char hex[8];
        snprintf(hex, sizeof(hex), "#%02X%02X%02X",
                 static_cast<int>(color.r),
                 static_cast<int>(color.g),
                 static_cast<int>(color.b));
        return std::string(hex);
    }

    std::string formatNumber(double value) const {
        std::ostringstream ss;
        ss.setf(std::ios::fixed, std::ios::floatfield);
        ss << std::setprecision(6) << value;
        std::string out = ss.str();
        auto dot = out.find('.');
        if (dot != std::string::npos) {
            while (!out.empty() && out.back() == '0') {
                out.pop_back();
            }
            if (!out.empty() && out.back() == '.') {
                out.pop_back();
            }
        }
        if (out.empty()) {
            out = "0";
        }
        return out;
    }

    Color line_color_;
    Color fill_color_;
    double line_width_;
    int line_type_;
    std::string line_type_custom_dash_;  // populated by SVGConverter::processLineType
                                         // when the active LINE TYPE has a
                                         // LINE AND EDGE TYPE DEFINITION
    int fill_style_;
    int hatch_index_;
    bool edge_visibility_;
    Color edge_color_;
    double edge_width_;
    int edge_type_;
    std::string edge_type_custom_dash_;  // populated by SVGConverter::processEdgeType from LINE AND EDGE TYPE DEFINITION
    Color text_color_;
    double text_height_;
    double original_text_height_;
    bool   character_height_explicit_;
    bool   line_color_explicit_;
    bool   fill_color_explicit_;
    bool   edge_color_explicit_;
    bool   text_color_explicit_;
    bool   marker_color_explicit_;
    int text_h_align_;  // 0=normal, 1=left, 2=center, 3=right
    int text_v_align_;  // 0=normal, 1=top, 2=cap, 3=half, 4=base, 5=bottom
    int marker_type_;   // 1=dot, 2=plus, 3=asterisk, 4=circle, 5=cross
    double marker_size_;
    Color marker_color_;
    LineCapIndicator line_cap_;
    JoinIndicator line_join_;
    LineCapIndicator edge_cap_;
    JoinIndicator edge_join_;
    int font_index_;
    double character_spacing_;
    double character_expansion_;
    std::string font_family_;
    int pattern_index_;
    bool line_dash_continuation_;
    double line_dash_offset_;
    bool edge_dash_continuation_;
    double edge_dash_offset_;
    int text_precision_;
    double miter_limit_;
    double opacity_;
};

class SVGConverter {
public:
    enum class SegmentPolicy {
        StrictWebCgm,
        AllowSegments
    };

    using OutputProfile = opencgm::OutputProfile;
    using ConversionPlan = opencgm::ResolvedConversionPlan;

    using ViewerShimMode = svg::ViewerShimMode;

    /// Raster encoding format for embedded images
    enum class RasterEncoding {
        Auto,   ///< Auto-detect optimal format based on image characteristics
        PNG,    ///< Force PNG encoding (lossless, best for monochrome/line art)
        JPEG    ///< Force JPEG encoding (lossy, best for photographic content)
    };

    explicit SVGConverter(CGMFile* cgm_file);

    // Thin text configuration shim (keeps future CLI/C API wiring simple)
    struct TextRenderOptions {
        bool text_as_path = false;                 // outline all text when a font file is available
        double text_as_path_threshold = 0.0;       // outline text at or below this SVG-unit size
        std::vector<std::string> font_fallback_stack; // e.g., {"Noto Sans", "Arial", "sans-serif"}
        std::unordered_map<std::string, std::string> font_overrides; // canonical -> CSS stack
        std::vector<std::pair<std::string, std::string>> font_substitutions; // {regex, useCanonical}
        std::unordered_map<std::string, double> font_baseline_adjustments; // primary font -> em offset
        double default_baseline_adjustment = 0.0;  // applied when no per-font entry
        std::unordered_map<std::string, std::string> font_file_paths; // canonical font -> font file path
        std::string fallback_font_path;            // used if no explicit mapping present
    };

    void setTextRenderOptions(const TextRenderOptions& opts) {
        text_options_ = opts;
        font_substitutions_cache_dirty_ = true;
    }
    void setFontFallbackStack(const std::vector<std::string>& stack) { text_options_.font_fallback_stack = stack; }
    void addFontOverride(const std::string& name, const std::string& cssStack) { text_options_.font_overrides[name] = cssStack; }
    void addFontSubstitution(const std::string& pattern, const std::string& use) {
        text_options_.font_substitutions.emplace_back(pattern, use);
        font_substitutions_cache_dirty_ = true;
    }
    void addFontBaselineAdjustment(const std::string& name, double emOffset) { text_options_.font_baseline_adjustments[name] = emOffset; }
    void setDefaultBaselineAdjustment(double emOffset) { text_options_.default_baseline_adjustment = emOffset; }
    void addFontPathOverride(const std::string& name, const std::string& path) { text_options_.font_file_paths[name] = path; }
    void setFallbackFontPath(const std::string& path) { text_options_.fallback_font_path = path; }

    void setSvgSize(double width, double height) {
        svg_width_ = width;
        svg_height_ = height;
        transform_.setSvgSize(width, height);
    }

    void setHotspotProfile(HotspotProfile profile) {
        pending_conversion_plan_.hotspot =
            HotspotProfileConfig::fromProfile(profile);
    }

    void setHotspotConfig(const HotspotProfileConfig& config) {
        pending_conversion_plan_.hotspot = config;
    }

    void addCustomAttribute(const CustomAttributeRule& rule) {
        pending_conversion_plan_.hotspot.custom_attributes.push_back(rule);
    }

    struct PaletteOverride {
        enum class Mode {
            None,
            Monochrome,
            Custom
        };

        Mode mode = Mode::None;
        std::map<int, Color> customPalette;
        bool applyToFills = false;
    };

    void setSegmentPolicy(SegmentPolicy policy);
    bool encounteredSegments() const { return encountered_segment_; }

    void setPaletteOverride(const PaletteOverride& overrideConfig);
    void clearPaletteOverride();
    void setColorLoggingEnabled(bool enabled);
    void setWidthLoggingEnabled(bool enabled);
    void setRasterLoggingEnabled(bool enabled);
    void setTransparentCellColourEnabled(bool enabled);
    void setAttributeOutputFormat(svg::AttributeManager::OutputFormat format);
    void setCompatibilityMode(bool enabled);

    /// The user-facing profile label embedded in <desc> as `profile=...`.
    /// Decouples the externally-visible profile name from the engine's
    /// internal `OutputProfile` enum so e.g. AutoDetect/Generic (which both
    /// map to OutputProfile::S1000D under the compat branch) can still
    /// surface as `profile=auto` instead of mis-claiming `s1000d-issue-6`.
    /// When empty (default), writeSvgHeader() falls back to the existing
    /// OutputProfile-derived label.
    void setRequestedProfileLabel(const std::string& label) {
        requested_profile_label_ = label;
    }

    /// Embed the WebCGM-style <metadata id="webcgm-aps">{...}</metadata> JSON
    /// blob in the SVG footer. Off by default -- the blob has no known
    /// consumer and just bloats output. Enable when emitting WebCGM 2.1 IETP
    /// payloads that downstream tooling consumes as a single APS dump.
    void setEmbedApsMetadataJson(bool enabled) { embed_aps_metadata_json_ = enabled; }
    bool embedApsMetadataJson() const { return embed_aps_metadata_json_; }
    void setIncludeDocumentMetadata(bool enabled) { include_document_metadata_ = enabled; }
    bool includeDocumentMetadata() const { return include_document_metadata_; }
    void setOutputBackgroundColor(const Color& color) { output_background_color_ = color; }
    void clearOutputBackgroundColor() { output_background_color_.reset(); }
    void setGeometryLoggingEnabled(bool enabled);
    void setPngQuantizationEnabled(bool enabled);
    const std::vector<RasterMetrics>& rasterMetrics() const { return raster_metrics_; }
    const GeometryMetrics& geometryMetrics() const { return geometry_metrics_; }
    void setViewboxPaddingFraction(double fraction);

    /**
     * Maximum deviation (in SVG output units) allowed when approximating
     * NUBS/NURBS curves with cubic Beziers. Smaller = more accurate, larger
     * output. Clamped to [0.001, 10]; default 0.5.
     */
    void setNurbsToleranceSvgUnits(double svgUnits);
    void setAdoptViewOnLoad(bool enabled) {
        if (pending_conversion_plan_.adopt_view_on_load != enabled) {
            pending_conversion_plan_.adopt_view_on_load = enabled;
            document_metrics_dirty_ = true;
        }
    }
    void setEmitSourceHash(bool enabled) {
        if (emit_source_hash_ == enabled) {
            return;
        }

        emit_source_hash_ = enabled;
        source_hash_computed_ = false;

        if (!emit_source_hash_) {
            source_file_hash_.clear();
        }
    }
    void setOutputProfile(OutputProfile profile);
    OutputProfile getOutputProfile() const {
        return pending_conversion_plan_.output_profile;
    }

    void setConversionPlan(const ConversionPlan& plan);
    const ConversionPlan& getConversionPlan() const {
        return pending_conversion_plan_;
    }

    /**
     * Set custom output target configuration
     * Only used when OutputProfile is set to Custom
     */
    void setOutputTargetConfig(const OutputTargetConfig& config) {
        pending_conversion_plan_.output_target = config;
    }

    /**
     * Get current output target configuration
     * Returns the effective configuration based on OutputProfile
     */
    OutputTargetConfig getOutputTargetConfig() const;

    void setViewerShimMode(ViewerShimMode mode) { viewer_shim_mode_ = mode; }
    void setViewerShimUrl(const std::string& url) { viewer_shim_url_ = url; }

    /**
     * Enable/disable SVGZ (gzip compressed) output
     * When enabled, the convert() method returns gzip-compressed SVG data
     * @param enabled true to enable compression (SVGZ), false for plain SVG
     */
    void setCompressOutput(bool enabled) { compress_output_ = enabled; }
    bool getCompressOutput() const { return compress_output_; }

    /**
     * Set raster encoding mode for embedded images
     * Auto mode selects PNG for monochrome/line art and JPEG for photographic content
     * @param mode RasterEncoding::Auto, PNG, or JPEG
     */
    void setRasterEncoding(RasterEncoding mode) { raster_encoding_ = mode; }
    RasterEncoding getRasterEncoding() const { return raster_encoding_; }

    /**
     * Set JPEG quality for raster encoding (1-100)
     * Only used when raster encoding is JPEG or Auto selects JPEG
     * Default: 85 (good balance of quality and size)
     * @param quality 1-100, higher = better quality, larger file
     */
    void setJpegQuality(int quality) { jpeg_quality_ = (quality < 1) ? 1 : ((quality > 100) ? 100 : quality); }
    int getJpegQuality() const { return jpeg_quality_; }

    /**
     * Enable/disable 4:4:4 chroma subsampling for JPEG
     * When enabled, preserves color edge sharpness (important for wiring diagrams)
     * When disabled, uses 4:2:0 subsampling (smaller files, slight color blur)
     * Default: true (4:4:4 for technical illustrations)
     */
    void setJpeg444Subsampling(bool enabled) { jpeg_444_subsampling_ = enabled; }
    bool getJpeg444Subsampling() const { return jpeg_444_subsampling_; }

    /**
     * Set the target rendering DPI for the output SVG.
     *
     * The SVG viewport stays in VDC / metric units, but the target DPI is emitted
     * as `data-opencgm-target-dpi` on the root <svg> so downstream rasterisers and
     * print pipelines know the intended physical resolution. Also used to write the
     * pHYs chunk when embedding PNG raster tiles. Default 96 (screen DPI).
     */
    void setTargetDpi(int dpi) { target_dpi_ = (dpi > 0) ? dpi : 96; }
    int getTargetDpi() const { return target_dpi_; }

    /**
     * Set XCF merger for companion file metadata integration
     * The merger must remain valid during conversion.
     * @param merger Pointer to XcfMerger (can be nullptr to disable)
     */
    void setXcfMerger(class XcfMerger* merger) { xcf_merger_ = merger; }

    /**
     * Get the current XCF merger
     */
    class XcfMerger* getXcfMerger() const { return xcf_merger_; }

    /**
     * @brief Convert the CGM file to SVG format
     * @param pictureIndex Index of picture to convert (-1 for all pictures, default)
     * @return SVG content as string
     */
    std::string convert(int pictureIndex = -1);

private:
    void recomputeDocumentMetrics();
    using APSNode = svg::ApsNode;

    using APSMetadataEntry = svg::ApsMetadataEntry;

    void writeSvgHeader();
    void writeSvgFooter();
    void handleSegmentEncountered(const Command* cmd, const std::string& label);
    void processCommand(Command* cmd);
    void emitViewerShimIfNeeded();

    // Graphical primitive processors
    void processPolyline(Polyline* cmd);
    void processDisjointPolyline(DisjointPolyline* cmd);
    void processPolygon(Polygon* cmd);
    void processPolygonSet(PolygonSet* cmd);
    void processPolymarker(Polymarker* cmd);
    void processCircle(Circle* cmd);
    void processEllipse(Ellipse* cmd);
    void processEllipticalArc(EllipticalArc* cmd);
    void processEllipticalArcClose(EllipticalArcClose* cmd);
    void processCircularArcCentre(CircularArcCentre* cmd);
    void processCircularArcCentreClose(CircularArcCentreClose* cmd);
    void processCircularArcCentreReversed(CircularArcCentreReversed* cmd);
    void processCircularArc3Point(CircularArc3Point* cmd);
    void processCircularArc3PointClose(CircularArc3PointClose* cmd);
    void processHyperbolicArc(HyperbolicArc* cmd);
    void processParabolicArc(ParabolicArc* cmd);
    void processRectangle(Rectangle* cmd);
    void processText(Text* cmd);
    void processRestrictedText(RestrictedText* cmd);
    void processAppendText(AppendText* cmd);
    void processConnectingEdge(ConnectingEdge* cmd);
    void processPolyBezier(PolyBezier* cmd);
    void processNonUniformBSpline(NonUniformBSpline* cmd);
    void processNonUniformRationalBSpline(NonUniformRationalBSpline* cmd);
    void processGeneralizedDrawingPrimitive(GeneralizedDrawingPrimitive* cmd);
    void processCellArray(CellArray* cmd);
    void processBitonalTile(BitonalTile* cmd);
    void processTile(Tile* cmd);
    void processPolySymbol(PolySymbol* cmd);
    void processSymbolLibraryList(SymbolLibraryList* cmd);
    void processMaximumColourIndex(MaximumColourIndex* cmd);
    void processColourValueExtent(ColourValueExtent* cmd);
    void processFontList(FontList* cmd);
    void processBeginTileArray(BeginTileArray* cmd);
    void processEndTileArray(EndTileArray* cmd);
    void processBeginPicture(BeginPicture* cmd);
    void processBeginPictureBody(BeginPictureBody* cmd);
    void processEndPicture(EndPicture* cmd);
    void processBeginFigure(Command* cmd);
    void processEndFigure(Command* cmd);
    void processBeginApplicationStructure(BeginApplicationStructure* cmd);
    void processBeginApplicationStructureBody(BeginApplicationStructureBody* cmd);
    void processEndApplicationStructure(EndApplicationStructure* cmd);
    void processApplicationStructureAttribute(ApplicationStructureAttribute* cmd);

    // Picture descriptor processors
    void processFillRepresentation(FillRepresentation* cmd);
    void processHatchStyleDefinition(HatchStyleDefinition* cmd);

    // Control processors
    void processClipRectangle(ClipRectangle* cmd);
    void processClipIndicator(ClipIndicator* cmd);
    void processProtectionRegionIndicator(ProtectionRegionIndicator* cmd);
    void processBeginProtectionRegion(BeginProtectionRegion* cmd);
    void processEndProtectionRegion(EndProtectionRegion* cmd);
    void processTransparentCellColour(TransparentCellColour* cmd);

    using TileGeometry = svg::TileGeometry;

    TileGeometry computeTileGeometry(const TileElement& tile, bool applyMalformedSizeHeuristics);
    bool emitTileImage(const std::vector<uint8_t>& encodedBytes,
                       const std::string& mimeType,
                       const TileGeometry& geometry,
                       const char* imageRendering,
                       const char* debugLabel);
    void advanceTileIndex();

    // Attribute processors
    void processLineColor(LineColour* cmd);
    void processFillColor(FillColour* cmd);
    void processLineWidth(LineWidth* cmd);
    void processLineType(LineType* cmd);
    void processLineTypeContinuation(LineTypeContinuation* cmd);
    void processLineTypeInitialOffset(LineTypeInitialOffset* cmd);
    void processLineAndEdgeTypeDefinition(LineAndEdgeTypeDefinition* cmd);
    void processInteriorStyle(InteriorStyle* cmd);
    void processHatchIndex(HatchIndex* cmd);
    void processPatternIndex(PatternIndex* cmd);
    void processPatternTable(PatternTable* cmd);
    void processPatternSize(PatternSize* cmd);
    void processFillReferencePoint(FillReferencePoint* cmd);
    void processEdgeColor(EdgeColour* cmd);
    void processEdgeWidth(EdgeWidth* cmd);
    void processEdgeType(EdgeType* cmd);
    void processEdgeTypeContinuation(EdgeTypeContinuation* cmd);
    void processEdgeTypeInitialOffset(EdgeTypeInitialOffset* cmd);
    void processEdgeVisibility(EdgeVisibility* cmd);
    void processLineCap(LineCap* cmd);
    void processLineJoin(LineJoin* cmd);
    void processMitreLimit(MitreLimit* cmd);
    void processEdgeCap(EdgeCap* cmd);
    void processEdgeJoin(EdgeJoin* cmd);
    void processEscape(Escape* cmd);
    void processFillBundleIndex(FillBundleIndex* cmd);
    void processTextColor(TextColour* cmd);
    void processTextFontIndex(TextFontIndex* cmd);
    void processTextPrecision(TextPrecision* cmd);
    void processCharacterExpansion(CharacterExpansionFactor* cmd);
    void processCharacterSpacing(CharacterSpacing* cmd);
    void processCharacterHeight(CharacterHeight* cmd);
    void processCharacterOrientation(CharacterOrientation* cmd);
    void processTextPath(Command* cmd);
    void processTextAlignment(TextAlignment* cmd);
    void processMarkerType(MarkerType* cmd);
    void processMarkerSize(MarkerSize* cmd);
    void processMarkerColor(MarkerColour* cmd);
    void processColourTable(ColourTable* cmd);
    void processColourSelectionMode(ColourSelectionMode* cmd);
    void refreshDefaultIndexedColors();

    // Colour helpers
    using ColorRole = svg::ColorRole;

    Color resolveColor(const CGMColor& cgmColor,
                       ColorRole role,
                       const char* debugLabel = nullptr);
    void logColorResolution(int index,
                            const Color& color,
                            ColorRole role,
                            const std::string& source,
                            bool overrideApplied,
                            bool fromTable,
                            bool isIndexed);
    void initializeDefaultColorTable();

    // Pattern helpers
    std::string ensurePatternFill(int patternIndex);
    std::string ensureHatchPattern(int hatchIndex, const Color& color);
    std::string ensureParallelHatchPattern(int hatchIndex, const HatchDefinition& definition, const Color& color);
    double hatchStrokeWidth() const;
    std::string getFillAttributeForCurrentStyle();
    std::string buildFillAndEdgeAttributes(bool overrideEdgeVisibility = false, bool edgeVisibleOverride = false);
    void applyFillBundle(int index);
    void applyFillBundleEntry(const FillBundleEntry& entry, const char* reason);
    void resetTileArrayState();
    void resetPatternState();
    void resetPictureState();
    Color applyColourValueExtent(const Color& value) const;
    static const char* interiorStyleName(int style);

    // Figure helpers
    void renderFigure();
    bool capturingProtectionRegionClip() const;

    // Text helpers
    void flushPendingText();
    double currentTextRotationDegrees() const;

    // Curve helpers
    void emitSampledPolyline(const std::vector<CGMPoint>& points);
    void emitNurbsPath(int order,
                       const std::vector<CGMPoint>& controlPoints,
                       const std::vector<double>& weights,
                       const std::vector<double>& sourceKnots,
                       double startParam,
                       double endParam,
                       const char* primitiveName);

    // Raster helpers
    std::string encodeCellArrayToPng(const std::vector<std::vector<Color>>& colors, int width, int height);
    std::string encodeCellArrayToJpeg(const std::vector<std::vector<Color>>& colors, int width, int height, int quality);
    std::string encodeCellArrayToBmp(const std::vector<std::vector<Color>>& colors, int width, int height);
    // Symbol helpers
    std::string ensureSymbolDefinition(const std::string& name);
    std::optional<std::string> loadSymbolDefinitionContent(const std::string& name, std::string& sourcePath);

    // APS helpers
    void outputAPSOpenTag();
    void outputAPSCloseTag();
    void recordAPSMetadata(const APSNode& node, const std::map<std::string, std::string>& attributes);
    // Font helpers
    std::string resolveFontFamilyFromIndex(int index) const;
    std::string resolveFontFamilyNameFromRaw(const std::string& raw) const;
    std::vector<std::string> buildFontStack(const std::string& raw) const;
    static std::string formatFontStack(const std::vector<std::string>& stack);
    std::string primaryFontFromStack(const std::string& stack) const;
    double baselineAdjustmentForFont(const std::string& fontFamily) const;
    double measureTextWidth(const std::string& text, const std::string& font_family, double font_size);
    void initializeDefaultFontOptions();

    struct TextEmitterParams {
        CGMPoint position_svg;
        double font_size_svg = 0.0;
        double line_height = 0.0;
        std::string color_hex;
        std::string text_anchor_attr;
        std::string dominant_baseline_attr;
        double rotation_deg = 0.0;
        bool has_rotation = false;
        double letter_spacing = 0.0;
        double expansion = 1.0;
        std::string font_family;
        std::string text_content;
        bool wrap_aps_substring = false;
        bool apply_clip = false;
        bool preserve_whitespace = true;
    };

    void emitTextRun(const TextEmitterParams& params);
    void emitTextRunSingleLine(const TextEmitterParams& params);
    bool shouldEmitTextAsPath(const TextEmitterParams& params) const;
    void emitStandardText(const TextEmitterParams& params);
    void emitTextAsPath(const TextEmitterParams& params);

    void updateClipPathDefinition();
    std::string clipPathAttribute() const;
    std::string debugCommandAttribute() const;
    void closeOpenDefs();
    CGMFile* cgm_file_;
    double svg_width_;
    double svg_height_;
    CoordinateTransform transform_;
    SVGStyle current_style_;
    std::ostringstream svg_output_;
    std::string source_file_path_;
    std::string source_file_hash_;
    bool source_hash_computed_ = false;
    bool emit_source_hash_ = false;
    bool document_metrics_dirty_ = true;
    bool include_document_metadata_ = true;
    std::optional<Color> output_background_color_;
    std::string current_picture_name_;
    bool png_quantization_enabled_ = false;
    int target_dpi_ = 96;
    std::map<std::string, std::string> clip_path_cache_;
    Color background_color_;
    bool background_color_explicit_;  // CGM emitted BACKGROUND COLOUR (Class 2 elem 7); default white otherwise
    PaletteOverride palette_override_;
    bool color_logging_enabled_;
    std::map<std::string, Color> last_logged_colors_;
    bool width_logging_enabled_;
    double viewbox_padding_fraction_;
    double nurbs_tolerance_svg_units_ = 0.5;
    ConversionPlan conversion_plan_;
    ConversionPlan pending_conversion_plan_;
    std::string requested_profile_label_;
    class XcfMerger* xcf_merger_ = nullptr;

    SpecificationMode line_width_spec_mode_;
    SpecificationMode edge_width_spec_mode_;
    double abstract_line_width_unit_;
    double abstract_edge_width_unit_;
    double nominal_line_width_svg_;
    double nominal_edge_width_svg_;
    SpecificationMode scaling_mode_;
    double metric_scale_factor_;
    double picture_scale_x_;
    double picture_scale_y_;
    double picture_vdc_width_;
    double picture_vdc_height_;
    bool view_context_active_;
    double view_context_x1_;
    double view_context_y1_;
    double view_context_x2_;
    double view_context_y2_;
    int view_context_count_;
    bool view_context_multiple_;
    bool apply_view_context_on_load_;
    bool view_context_nvdc_valid_;
    double view_context_nvdc_x1_;
    double view_context_nvdc_y1_;
    double view_context_nvdc_x2_;
    double view_context_nvdc_y2_;
    double picture_vdc_width_raw_;
    double picture_vdc_height_raw_;
    double picture_longest_side_raw_;
    double picture_vdc_min_x_;
    double picture_vdc_min_y_;
    double picture_vdc_max_x_;
    double picture_vdc_max_y_;
    // Original VDC extent preserving axis order (for coordinate transformation)
    double picture_vdc_orig_x1_;
    double picture_vdc_orig_y1_;
    double picture_vdc_orig_x2_;
    double picture_vdc_orig_y2_;
    bool picture_vdc_x_left_;  // X decreases left-to-right (x1 > x2)
    bool picture_vdc_y_down_;  // Y increases downward (y1 > y2)
    double svg_canvas_width_;
    double svg_canvas_height_;
    double svg_viewbox_x_;
    double svg_viewbox_y_;
    double svg_viewbox_width_;
    double svg_viewbox_height_;

    // Color table for indexed color resolution
    std::map<int, Color> color_table_;
    size_t expected_colour_table_size_;
    Color colour_value_extent_min_;
    Color colour_value_extent_max_;

    // ISO 8632-1 §6.3.2 "two pots" color attribute model: each colour
    // attribute (LINE/FILL/EDGE/TEXT) carries an independent value for
    // each COLOUR SELECTION MODE (DIRECT vs INDEXED). When a colour
    // command is received we update the slot that matches the *current*
    // mode; when COLOUR SELECTION MODE flips mid-picture we restore the
    // other mode's slot into the live PaintState. Without this, samples
    // like static10/COLRMD02 — which set red in DIRECT, switch to
    // INDEXED, set green via index, switch back to DIRECT — render the
    // last-set value (green) instead of reverting to the DIRECT-slot red.
    struct ColorSlots {
        Color direct;
        Color indexed;
        ColorSlots() : direct(0, 0, 0), indexed(0, 0, 0) {}
        ColorSlots(const Color &d, const Color &i) : direct(d), indexed(i) {}
    };
    ColorSlots line_color_slots_;
    ColorSlots fill_color_slots_;
    ColorSlots edge_color_slots_;
    ColorSlots text_color_slots_;
    ColorSelectionMode active_color_mode_;

    // Figure state
    bool in_figure_;
    bool figure_connects_subpaths_;
    std::vector<std::vector<CGMPoint>> figure_polylines_;
    std::vector<std::string> figure_path_fragments_;
    // Unified ordered list of figure sub-primitives (polylines, arcs, beziers)
    // captured in the order they appear in the CGM. Each entry carries the SVG
    // path-data fragment plus its start/end points. At END FIGURE we walk the
    // list and decide for each subpath whether its start matches the previous
    // subpath's end:
    //   - matches: rewrite leading "M " to "L " so the figure forms ONE
    //     continuous closed boundary (FIGURE01 "no connecting edge" polylines).
    //   - mismatch: keep the leading "M" so this subpath is a NEW disjoint
    //     region within the same figure (FIGURE03 two filled shapes inside one
    //     BEGIN FIGURE block).
    // The trailing Z closes whichever subpath was last open. fill-rule="evenodd"
    // does the right thing for both cases.
    struct FigureSubpath {
        std::string svg;       // "M sx sy ... " path data
        CGMPoint start;
        CGMPoint end;
        bool connect_to_prev = false;  // CONNECTING EDGE seen just before
        bool closed_primitive = false; // CGM primitive is intrinsically closed
                                       // (RECTANGLE, CIRCLE, ELLIPSE, full
                                       // ARC). Open primitives (POLYLINE,
                                       // partial arcs, BEZIER) implicitly
                                       // bridge to the next subpath per
                                       // ISO 8632-1 §7.2.4.
    };
    std::vector<FigureSubpath> figure_ordered_subpaths_;
    bool pending_connect_to_prev_ = false;

    // Tile array state
    bool in_tile_array_;
    CGMPoint tile_array_position_;
    int tile_path_direction_;
    int tile_line_direction_;
    double tile_cell_width_;
    double tile_cell_height_;
    int tile_tiles_in_path_;
    int tile_tiles_in_line_;
    int tile_cells_per_tile_path_;
    int tile_cells_per_tile_line_;
    int tile_image_offset_path_;
    int tile_image_offset_line_;
    int tile_image_cells_path_;
    int tile_image_cells_line_;
    int tile_current_index_;
    std::vector<std::string> symbol_libraries_;
    std::vector<std::string> symbol_library_paths_;
    int symbol_placeholder_counter_;
    int gdp_placeholder_counter_;
    bool defs_open_;
    std::unordered_map<std::string, std::string> symbol_definition_ids_;
    std::unordered_set<std::string> emitted_symbol_defs_;
    std::unordered_map<std::string, std::string> symbol_definition_sources_;
    std::unordered_map<std::string, std::string> symbol_definition_fragments_;
    std::string cgm_base_dir_;

    // Pattern fill state
    int pattern_counter_;
    std::unordered_map<std::string, std::string> hatch_pattern_ids_;
    std::unordered_map<int, std::string> pattern_fill_ids_;
    std::unordered_map<int, PatternTableData> pattern_tables_;
    // PATTERN SIZE handling is hybrid for compatibility with both common
    // metafile authoring conventions:
    //   - Per-table: many CGMs emit PATTERN TABLE N then PATTERN SIZE,
    //     intending that size to bind to that specific table (INTSTL08).
    //   - Global: ISO 8632-1 §6.5.21 says PATTERN SIZE applies to all
    //     subsequent fills (PATTBL01: 64 tables, 1 PATTERN SIZE element).
    // We capture both: bind to last_defined_pattern_index_ AND update the
    // global. At draw time, prefer per-index, fall through to global, then
    // to a viewbox-derived default.
    std::unordered_map<int, PatternSizeData> pattern_sizes_;
    PatternSizeData active_pattern_size_;
    bool has_active_pattern_size_;
    std::unordered_map<int, HatchDefinition> hatch_definitions_;
    // CGM LINE AND EDGE TYPE DEFINITION (Class 2 elem 17) installs a custom
    // dash pattern for a (possibly negative) line-type index. The dash list
    // is alternating mark/gap lengths in VDC units. Looked up by getStrokeDashPattern
    // when a non-standard line type is active so vendor-extended dashes
    // (LINSTD01 uses indices -1 to -8) render correctly instead of falling
    // through to "no pattern -> solid line".
    std::unordered_map<int, std::vector<double>> line_type_definitions_;
    CGMPoint fill_reference_point_;
    bool has_fill_reference_point_;
    int last_defined_pattern_index_;
    std::unordered_map<int, FillBundleEntry> fill_bundles_;
    int active_fill_bundle_index_;

    // Clipping state
    bool clip_rectangle_defined_;
    CGMPoint clip_rect_first_;
    CGMPoint clip_rect_second_;
    bool clip_enabled_;
    std::string clip_path_attribute_;
    int clip_path_counter_;

    // Protection region state.
    // CGM region clipping flow per ISO 8632-1: BEGIN PROTECTION REGION (rid)
    // ... define geometry ... END PROTECTION REGION stores the geometry as
    // region rid's SHAPE without drawing it. Later, PROTECTION REGION
    // INDICATOR (rid, ind=2) enables clipping by that region for subsequent
    // primitives. Indicator=1 disables it.
    bool in_protection_region_;
    int protection_region_indicator_;
    int active_protection_region_index_;
    std::vector<std::string> protection_region_paths_;        // scratch during BPR/EPR
    std::unordered_map<int, std::vector<std::string>> protection_region_definitions_;  // rid -> shape paths
    std::unordered_map<int, std::string> protection_region_clip_ids_;  // rid -> emitted clipPath id (cached)

    // Debug / bookkeeping
    size_t current_command_index_;
    bool debug_fill_logging_;
    bool transparent_cell_active_;
    Color transparent_cell_color_;
    ViewerShimMode viewer_shim_mode_;
    std::string viewer_shim_url_;
    bool compress_output_;  ///< Output as SVGZ (gzip-compressed SVG)
    RasterEncoding raster_encoding_;  ///< Raster encoding mode (Auto, PNG, or JPEG)
    int jpeg_quality_;                ///< JPEG quality (1-100, default 85)
    bool jpeg_444_subsampling_;       ///< Use 4:4:4 chroma subsampling (default true)
    bool has_layer_aps_;
    bool has_linkuri_aps_;
    bool has_viewcontext_aps_;

    std::vector<APSNode> aps_stack_;
    std::map<std::string, std::string> current_aps_attributes_;
    std::vector<LinkuriEntry> current_aps_linkuris_;  ///< Multiple linkuri entries for multi-link support
    svg::UniqueIdAllocator aps_id_allocator_{"aps"};
    std::vector<APSMetadataEntry> aps_metadata_entries_;
    bool embed_aps_metadata_json_ = false;

    // APS attribute emission
    svg::AttributeManager attribute_manager_;

    // Pending text state for APPEND TEXT sequences (multi-segment with per-segment attributes)
    bool pending_text_active_;
    CGMPoint pending_text_position_;
    std::vector<PendingTextSegment> pending_text_segments_;  // Segments with individual attributes
    int pending_text_h_align_;
    int pending_text_v_align_;
    bool has_last_text_position_;
    CGMPoint last_text_position_;
    double pending_text_rotation_deg_;
    bool pending_text_has_rotation_;
    bool pending_is_restricted_text_;         // True if started with RESTRICTED TEXT
    double pending_restricted_delta_width_;   // RESTRICTED TEXT extent width
    double pending_restricted_delta_height_;  // RESTRICTED TEXT extent height

    // Text orientation state
    CGMPoint character_orientation_base_;
    CGMPoint character_orientation_up_;

    // Font lists from FONT LIST command
    std::vector<std::string> font_list_;

    // Marker profile cache
    std::string getMarkerPathData(int markerType, double size);

    // Segment handling
    SegmentPolicy segment_policy_;
    bool encountered_segment_;
    int current_picture_index_;

    // Transformed bounding box (in SVG coordinates)
    double svg_bounds_x1_;
    double svg_bounds_y1_;
    double svg_bounds_x2_;
    double svg_bounds_y2_;

    // Original VDC extent (for ABSTRACT mode calculations)
    double original_vdc_x1_;
    double original_vdc_y1_;
    double original_vdc_x2_;
    double original_vdc_y2_;

    // Text rendering options (thin config shim)
    TextRenderOptions text_options_;
    bool text_as_path_warning_emitted_;

    // Latches for the fill fallbacks in getFillAttributeForCurrentStyle(). Each degradation
    // warns once per conversion: the same unsupported style typically covers every filled
    // primitive in a metafile, and a line per shape would bury the fact rather than report it.
    std::set<int> unsupported_fill_styles_warned_;
    bool pattern_fallback_warning_emitted_ = false;
    bool hatch_fallback_warning_emitted_ = false;
    mutable bool font_substitutions_cache_dirty_ = true;
    mutable std::vector<std::pair<std::regex, std::string>> compiled_font_substitutions_;

    std::unordered_map<std::string, std::shared_ptr<LoadedFont>> font_cache_;           // keyed by absolute font path
    std::unordered_map<std::string, std::shared_ptr<LoadedFont>> font_lookup_;          // canonical name -> cached font

    std::shared_ptr<LoadedFont> acquireFontForFamily(const std::string& fontFamily);
    std::shared_ptr<LoadedFont> loadFontFromPath(const std::string& fontPath);
    void rebuildCompiledFontSubstitutions() const;
    static std::vector<uint32_t> utf8ToCodepoints(const std::string& text);

    bool transparent_cell_colour_enabled_;
    bool compatibility_mode_;
    bool geometry_logging_enabled_;
    GeometryMetrics geometry_metrics_;
    bool raster_logging_enabled_;
    std::vector<RasterMetrics> raster_metrics_;
};

} // namespace opencgm

#endif // OPENCGM_SVG_CONVERTER_H




