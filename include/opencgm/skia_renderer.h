#ifndef OPENCGM_SKIA_RENDERER_H
#define OPENCGM_SKIA_RENDERER_H

#include "cgm_file.h"
#include "cgm_point.h"
#include "command.h"
#include "enums.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <map>

#ifdef CGM_SKIA_RENDERER_ENABLED
#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkFont.h"
#include "include/core/SkRefCnt.h"

// Use sk_sp smart pointer for Skia objects
template<typename T> using SkiaPtr = sk_sp<T>;
#else
// Forward declarations for Skia types when Skia is not enabled
class SkCanvas;
class SkSurface;
class SkPaint;
class SkPath;
class SkFont;

// Stub smart pointer when Skia is disabled
template<typename T> using SkiaPtr = std::unique_ptr<T>;
#endif

namespace opencgm {

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
class NonUniformBSpline;
class NonUniformRationalBSpline;
class CellArray;
class BitonalTile;
class BeginPicture;
class EndPicture;

/**
 * @brief Render options for native CGM rasterization
 */
struct SkiaRenderOptions {
    int target_width = 0;           ///< Target width (0 = auto from aspect ratio)
    int target_height = 0;          ///< Target height (0 = auto from aspect ratio)
    double scale_factor = 1.0;      ///< Scale factor for output
    uint8_t background_r = 255;     ///< Background red component
    uint8_t background_g = 255;     ///< Background green component
    uint8_t background_b = 255;     ///< Background blue component
    uint8_t background_a = 255;     ///< Background alpha component
    bool antialias = true;          ///< Enable antialiasing
    bool fit_to_content = false;    ///< Fit viewbox to geometry bounds
    int dpi = 96;                   ///< DPI for text sizing

    static SkiaRenderOptions defaults() {
        return SkiaRenderOptions();
    }
};

/**
 * @brief Result from native CGM rendering
 */
struct SkiaRenderResult {
    bool success = false;
    std::string error_message;
    std::vector<uint8_t> pixel_data;    ///< BGRA8888 pixel data
    int width = 0;
    int height = 0;
    int stride = 0;                      ///< Bytes per row
};

/**
 * @brief Coordinate transformation for VDC to output coordinates
 *
 * Shared logic extracted from SVGConverter's CoordinateTransform
 */
class RenderTransform {
public:
    RenderTransform();

    void setVdcExtent(double x1, double y1, double x2, double y2);
    void setOutputSize(double width, double height);
    void setFlipY(bool flip);

    void transform(double vdc_x, double vdc_y, double& out_x, double& out_y) const;
    CGMPoint transformPoint(const CGMPoint& pt) const;
    double transformLength(double vdc_length) const;

    void getVdcExtent(double& x1, double& y1, double& x2, double& y2) const;
    bool getFlipY() const { return flip_y_; }
    bool isYAxisInverted() const { return flip_y_ && !vdc_y_down_; }

    double scaleX() const;
    double scaleY() const;
    double outputWidth() const { return output_width_; }
    double outputHeight() const { return output_height_; }

private:
    double vdc_x1_, vdc_y1_, vdc_x2_, vdc_y2_;
    double output_width_, output_height_;
    bool flip_y_;
    bool vdc_y_down_;
};

/**
 * @brief Rendering style state
 *
 * Mirrors SVGStyle but for raster rendering
 */
class RenderStyle {
public:
    RenderStyle();

    // Line attributes
    void setLineColor(const Color& color) { line_color_ = color; }
    void setLineWidth(double width) { line_width_ = width; }
    void setLineType(int type) { line_type_ = type; }
    void setLineCap(LineCapIndicator cap) { line_cap_ = cap; }
    void setLineJoin(JoinIndicator join) { line_join_ = join; }

    // Fill attributes
    void setFillColor(const Color& color) { fill_color_ = color; }
    void setFillStyle(int style) { fill_style_ = style; }
    void setHatchIndex(int index) { hatch_index_ = index; }
    void setPatternIndex(int index) { pattern_index_ = index; }

    // Edge attributes
    void setEdgeColor(const Color& color) { edge_color_ = color; }
    void setEdgeWidth(double width) { edge_width_ = width; }
    void setEdgeType(int type) { edge_type_ = type; }
    void setEdgeVisibility(bool visible) { edge_visibility_ = visible; }
    void setEdgeCap(LineCapIndicator cap) { edge_cap_ = cap; }
    void setEdgeJoin(JoinIndicator join) { edge_join_ = join; }

    // Text attributes
    void setTextColor(const Color& color) { text_color_ = color; }
    void setTextHeight(double height) { text_height_ = height; }
    void setTextAlignment(int h_align, int v_align);
    void setFontIndex(int index) { font_index_ = index; }
    void setFontFamily(const std::string& family) { font_family_ = family; }
    void setCharacterSpacing(double spacing) { character_spacing_ = spacing; }
    void setCharacterExpansion(double expansion) { character_expansion_ = expansion; }

    // Marker attributes
    void setMarkerType(int type) { marker_type_ = type; }
    void setMarkerSize(double size) { marker_size_ = size; }
    void setMarkerColor(const Color& color) { marker_color_ = color; }

    // Accessors
    const Color& lineColor() const { return line_color_; }
    double lineWidth() const { return line_width_; }
    int lineType() const { return line_type_; }
    LineCapIndicator lineCap() const { return line_cap_; }
    JoinIndicator lineJoin() const { return line_join_; }

    const Color& fillColor() const { return fill_color_; }
    int fillStyle() const { return fill_style_; }
    int hatchIndex() const { return hatch_index_; }
    int patternIndex() const { return pattern_index_; }

    const Color& edgeColor() const { return edge_color_; }
    double edgeWidth() const { return edge_width_; }
    int edgeType() const { return edge_type_; }
    bool edgeVisibility() const { return edge_visibility_; }

    const Color& textColor() const { return text_color_; }
    double textHeight() const { return text_height_; }
    int textHAlign() const { return text_h_align_; }
    int textVAlign() const { return text_v_align_; }
    int fontIndex() const { return font_index_; }
    const std::string& fontFamily() const { return font_family_; }
    double characterSpacing() const { return character_spacing_; }
    double characterExpansion() const { return character_expansion_; }

    int markerType() const { return marker_type_; }
    double markerSize() const { return marker_size_; }
    const Color& markerColor() const { return marker_color_; }

    // Dash pattern for line type
    std::vector<float> getLineDashPattern(double scale = 1.0) const;
    std::vector<float> getEdgeDashPattern(double scale = 1.0) const;

private:
    // Line state
    Color line_color_;
    double line_width_;
    int line_type_;
    LineCapIndicator line_cap_;
    JoinIndicator line_join_;

    // Fill state
    Color fill_color_;
    int fill_style_;
    int hatch_index_;
    int pattern_index_;

    // Edge state
    Color edge_color_;
    double edge_width_;
    int edge_type_;
    bool edge_visibility_;
    LineCapIndicator edge_cap_;
    JoinIndicator edge_join_;

    // Text state
    Color text_color_;
    double text_height_;
    int text_h_align_;
    int text_v_align_;
    int font_index_;
    std::string font_family_;
    double character_spacing_;
    double character_expansion_;

    // Marker state
    int marker_type_;
    double marker_size_;
    Color marker_color_;
};

/**
 * @brief Native CGM renderer using Skia
 *
 * Renders CGM files directly to a raster bitmap without intermediate
 * format conversion. Used for accurate CGM preview and comparison
 * against SVG conversion output.
 */
class SkiaRenderer {
public:
    explicit SkiaRenderer(CGMFile* cgm_file);
    ~SkiaRenderer();

    // Non-copyable
    SkiaRenderer(const SkiaRenderer&) = delete;
    SkiaRenderer& operator=(const SkiaRenderer&) = delete;

    /**
     * @brief Render the CGM to a pixel buffer
     *
     * @param options Rendering options
     * @return Render result containing pixel data or error
     */
    SkiaRenderResult render(const SkiaRenderOptions& options = SkiaRenderOptions::defaults());

    /**
     * @brief Render to a raw buffer (C API compatible)
     *
     * @param target_width Target width (0 = auto)
     * @param target_height Target height (0 = auto)
     * @param scale_factor Scale multiplier
     * @param out_buffer Output buffer (caller must free with freeBuffer)
     * @param out_buffer_size Size of output buffer in bytes
     * @param out_width Actual rendered width
     * @param out_height Actual rendered height
     * @return true on success
     */
    bool renderToBuffer(
        int target_width,
        int target_height,
        double scale_factor,
        uint8_t** out_buffer,
        size_t* out_buffer_size,
        int* out_width,
        int* out_height
    );

    /**
     * @brief Free a buffer allocated by renderToBuffer
     */
    static void freeBuffer(uint8_t* buffer);

    // Configuration
    void setBackgroundColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void setAntiAlias(bool enabled);
    void setDpi(int dpi);

private:
    // Initialization
    bool initializeSurface(int width, int height);
    void calculateOutputDimensions(const SkiaRenderOptions& options, int& width, int& height);

    // Command processing
    void processCommands();
    void processCommand(Command* cmd);

    // Graphical primitive renderers
    void renderPolyline(Polyline* cmd);
    void renderDisjointPolyline(DisjointPolyline* cmd);
    void renderPolygon(Polygon* cmd);
    void renderPolygonSet(PolygonSet* cmd);
    void renderPolymarker(Polymarker* cmd);
    void renderCircle(Circle* cmd);
    void renderEllipse(Ellipse* cmd);
    void renderEllipticalArc(EllipticalArc* cmd);
    void renderEllipticalArcClose(EllipticalArcClose* cmd);
    void renderCircularArcCentre(CircularArcCentre* cmd);
    void renderCircularArcCentreClose(CircularArcCentreClose* cmd);
    void renderCircularArcCentreReversed(CircularArcCentreReversed* cmd);
    void renderCircularArc3Point(CircularArc3Point* cmd);
    void renderCircularArc3PointClose(CircularArc3PointClose* cmd);
    void renderRectangle(Rectangle* cmd);
    void renderText(Text* cmd);
    void renderRestrictedText(RestrictedText* cmd);
    void renderAppendText(AppendText* cmd);
    void renderPolyBezier(PolyBezier* cmd);
    void renderNonUniformBSpline(NonUniformBSpline* cmd);
    void renderNonUniformRationalBSpline(NonUniformRationalBSpline* cmd);
    void renderCellArray(CellArray* cmd);
    void renderBitonalTile(BitonalTile* cmd);

    // Attribute processors (update style state)
    void processAttributeCommand(Command* cmd);

    // Picture descriptors
    void processBeginPicture(BeginPicture* cmd);
    void processEndPicture(EndPicture* cmd);
    void processVdcExtent(Command* cmd);

    // Color resolution
    Color resolveColor(const CGMColor& cgmColor) const;
    void initializeDefaultColorTable();

    // Paint helpers
    void configureStrokePaint(SkPaint& paint) const;
    void configureFillPaint(SkPaint& paint) const;
    void configureEdgePaint(SkPaint& paint) const;
    void configureTextPaint(SkPaint& paint) const;

    // Path building helpers
    void buildPolylinePath(SkPath& path, const std::vector<CGMPoint>& points) const;
    void buildPolygonPath(SkPath& path, const std::vector<CGMPoint>& points) const;
    void buildArcPath(SkPath& path, const CGMPoint& center, double rx, double ry,
                      double startAngle, double sweepAngle) const;

    // NURBS helpers
    double basisFunction(int i, int k, double t, const std::vector<double>& knots) const;
    CGMPoint evaluateNURBS(double t, int order, const std::vector<CGMPoint>& controlPoints,
                           const std::vector<double>& weights, const std::vector<double>& knots) const;

    // Member variables
    CGMFile* cgm_file_;
    SkiaPtr<SkSurface> surface_;
    SkCanvas* canvas_;  // Owned by surface_

    RenderTransform transform_;
    RenderStyle current_style_;

    // Color table for indexed color resolution
    std::map<int, Color> color_table_;

    // Configuration
    uint8_t bg_r_, bg_g_, bg_b_, bg_a_;
    bool antialias_;
    int dpi_;

    // State tracking
    bool in_picture_;
    int current_picture_index_;
    std::string last_error_;
};

} // namespace opencgm

#endif // OPENCGM_SKIA_RENDERER_H
