#ifndef OPENCGM_SVG_UTILS_H
#define OPENCGM_SVG_UTILS_H

#include "opencgm/cgm_point.h"
#include "opencgm/cgm_color.h"
#include "opencgm/enums.h"
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace opencgm {
namespace svg {

// ============================================================================
// Base64 Encoding
// ============================================================================

std::string base64Encode(const std::string& input);

// ============================================================================
// PNG Utilities
// ============================================================================

void appendUint32BE(std::vector<uint8_t>& buffer, uint32_t value);
uint32_t crc32(const uint8_t* data, size_t length);
bool hasPngSignature(const std::vector<uint8_t>& data);

std::vector<uint8_t> ensurePngHeader(
    const std::vector<uint8_t>& data,
    uint32_t width,
    uint32_t height,
    uint8_t bitDepth = 8,
    uint8_t colorType = 6);

std::vector<uint8_t> buildPngFromIdat(
    const std::vector<uint8_t>& chunkStream,
    uint32_t width,
    uint32_t height,
    uint8_t bitDepth,
    uint8_t colorType);

// ============================================================================
// Point/Vector Operations
// ============================================================================

CGMPoint addPoints(const CGMPoint& lhs, const CGMPoint& rhs);
CGMPoint subtractPoints(const CGMPoint& lhs, const CGMPoint& rhs);
CGMPoint scalePoint(const CGMPoint& point, double scalar);

double crossProduct(const CGMPoint& a, const CGMPoint& b);
double lengthSquared(const CGMPoint& pt);
double vectorLength(const CGMPoint& pt);
bool vectorsNearlyEqual(const CGMPoint& a, const CGMPoint& b);

// ============================================================================
// Angle Operations
// ============================================================================

constexpr double kAngleTolerance = 1e-8;

double wrapAnglePositive(double angle);
double angularDistanceCCW(double startAngle, double endAngle);
bool angleIsBetweenCCW(double startAngle, double midAngle, double endAngle, double tol = 1e-8);

// ============================================================================
// Ellipse Geometry
// ============================================================================

bool ellipseAnglesFromDeltas(
    const CGMPoint& u,
    const CGMPoint& v,
    const CGMPoint& startDelta,
    const CGMPoint& endDelta,
    double& tStart,
    double& tEnd,
    double& detM);

std::tuple<double, double, double> conjugateDiametersToEllipse(
    const CGMPoint& u,
    const CGMPoint& v);

// ============================================================================
// Color Utilities
// ============================================================================

bool colorsEqualRgb(const Color& lhs, const Color& rhs);
bool colorsNearlyEqualRgb(const Color& lhs, const Color& rhs, int tolerance = 0);
std::string colorToHexString(const Color& color);

// ============================================================================
// PNG Format Detection
// ============================================================================

uint8_t clampTruecolorBitDepth(int precision);
bool derivePngFormat(
    ColorSelectionMode selectionMode,
    ColorModel model,
    int cellColorPrecision,
    int defaultColourPrecision,
    int defaultIndexPrecision,
    uint8_t& bitDepth,
    uint8_t& colorType);

// ============================================================================
// Text Utilities
// ============================================================================

std::vector<std::string> splitTextIntoLines(const std::string& text);

} // namespace svg
} // namespace opencgm

#endif // OPENCGM_SVG_UTILS_H
