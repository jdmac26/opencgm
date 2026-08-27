#include "opencgm/svg/svg_utils.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace opencgm {
namespace svg {

// ============================================================================
// Base64 Encoding
// ============================================================================

std::string base64Encode(const std::string& input)
{
    static const char* base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string output;
    int val = 0;
    int valb = -6;

    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            output.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    if (valb > -6) {
        output.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }

    while (output.size() % 4) {
        output.push_back('=');
    }

    return output;
}

// ============================================================================
// PNG Utilities
// ============================================================================

void appendUint32BE(std::vector<uint8_t>& buffer, uint32_t value)
{
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
}

uint32_t crc32(const uint8_t* data, size_t length)
{
    static uint32_t table[256];
    static std::once_flag initFlag;
    std::call_once(initFlag, []() {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t crc = i;
            for (int j = 0; j < 8; ++j) {
                if (crc & 1) {
                    crc = 0xEDB88320u ^ (crc >> 1);
                } else {
                    crc >>= 1;
                }
            }
            table[i] = crc;
        }
    });

    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; ++i) {
        uint8_t byte = data[i];
        crc = table[(crc ^ byte) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

bool hasPngSignature(const std::vector<uint8_t>& data)
{
    static const uint8_t signature[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (data.size() < sizeof(signature)) {
        return false;
    }
    for (size_t i = 0; i < sizeof(signature); ++i) {
        if (data[i] != signature[i]) {
            return false;
        }
    }
    return true;
}

std::vector<uint8_t> ensurePngHeader(
    const std::vector<uint8_t>& data,
    uint32_t width,
    uint32_t height,
    uint8_t bitDepth,
    uint8_t colorType)
{
    if (hasPngSignature(data)) {
        return data;
    }

    std::vector<uint8_t> result;
    static const uint8_t signature[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    result.insert(result.end(), std::begin(signature), std::end(signature));

    if (data.size() >= 8 &&
        data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x0D &&
        data[4] == 'I' && data[5] == 'H' && data[6] == 'D' && data[7] == 'R') {
        result.insert(result.end(), data.begin(), data.end());
        return result;
    }

    // IHDR chunk
    std::array<uint8_t, 4 + 13> ihdrPayload{};
    ihdrPayload[0] = 'I';
    ihdrPayload[1] = 'H';
    ihdrPayload[2] = 'D';
    ihdrPayload[3] = 'R';
    ihdrPayload[4] = static_cast<uint8_t>((width >> 24) & 0xFF);
    ihdrPayload[5] = static_cast<uint8_t>((width >> 16) & 0xFF);
    ihdrPayload[6] = static_cast<uint8_t>((width >> 8) & 0xFF);
    ihdrPayload[7] = static_cast<uint8_t>(width & 0xFF);
    ihdrPayload[8] = static_cast<uint8_t>((height >> 24) & 0xFF);
    ihdrPayload[9] = static_cast<uint8_t>((height >> 16) & 0xFF);
    ihdrPayload[10] = static_cast<uint8_t>((height >> 8) & 0xFF);
    ihdrPayload[11] = static_cast<uint8_t>(height & 0xFF);
    ihdrPayload[12] = bitDepth;
    ihdrPayload[13] = colorType;
    ihdrPayload[14] = 0; // compression
    ihdrPayload[15] = 0; // filter
    ihdrPayload[16] = 0; // interlace

    appendUint32BE(result, 13);
    result.insert(result.end(), ihdrPayload.begin(), ihdrPayload.begin() + 4 + 13);
    uint32_t crcVal = crc32(ihdrPayload.data(), ihdrPayload.size());
    appendUint32BE(result, crcVal);

    result.insert(result.end(), data.begin(), data.end());
    return result;
}

std::vector<uint8_t> buildPngFromIdat(
    const std::vector<uint8_t>& chunkStream,
    uint32_t width,
    uint32_t height,
    uint8_t bitDepth,
    uint8_t colorType)
{
    if (chunkStream.empty()) {
        return {};
    }

    std::vector<uint8_t> result;
    static const uint8_t signature[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    result.insert(result.end(), std::begin(signature), std::end(signature));

    std::array<uint8_t, 4 + 13> ihdrPayload{};
    ihdrPayload[0] = 'I';
    ihdrPayload[1] = 'H';
    ihdrPayload[2] = 'D';
    ihdrPayload[3] = 'R';
    ihdrPayload[4] = static_cast<uint8_t>((width >> 24) & 0xFF);
    ihdrPayload[5] = static_cast<uint8_t>((width >> 16) & 0xFF);
    ihdrPayload[6] = static_cast<uint8_t>((width >> 8) & 0xFF);
    ihdrPayload[7] = static_cast<uint8_t>(width & 0xFF);
    ihdrPayload[8] = static_cast<uint8_t>((height >> 24) & 0xFF);
    ihdrPayload[9] = static_cast<uint8_t>((height >> 16) & 0xFF);
    ihdrPayload[10] = static_cast<uint8_t>((height >> 8) & 0xFF);
    ihdrPayload[11] = static_cast<uint8_t>(height & 0xFF);
    ihdrPayload[12] = bitDepth;
    ihdrPayload[13] = colorType;
    ihdrPayload[14] = 0;
    ihdrPayload[15] = 0;
    ihdrPayload[16] = 0;

    appendUint32BE(result, 13);
    result.insert(result.end(), ihdrPayload.begin(), ihdrPayload.begin() + 4 + 13);
    uint32_t crcVal = crc32(ihdrPayload.data(), ihdrPayload.size());
    appendUint32BE(result, crcVal);

    result.insert(result.end(), chunkStream.begin(), chunkStream.end());

    if (chunkStream.size() < 12 ||
        !(chunkStream.end()[-12] == 0x00 && chunkStream.end()[-11] == 0x00 &&
          chunkStream.end()[-10] == 0x00 && chunkStream.end()[-9] == 0x00 &&
          chunkStream.end()[-8] == 0x49 && chunkStream.end()[-7] == 0x45 &&
          chunkStream.end()[-6] == 0x4E && chunkStream.end()[-5] == 0x44 &&
          chunkStream.end()[-4] == 0xAE && chunkStream.end()[-3] == 0x42 &&
          chunkStream.end()[-2] == 0x60 && chunkStream.end()[-1] == 0x82)) {
        appendUint32BE(result, 0);
        result.push_back('I');
        result.push_back('E');
        result.push_back('N');
        result.push_back('D');
        appendUint32BE(result, crc32(reinterpret_cast<const uint8_t*>("IEND"), 4));
    }

    return result;
}

// ============================================================================
// Point/Vector Operations
// ============================================================================

CGMPoint addPoints(const CGMPoint& lhs, const CGMPoint& rhs)
{
    return CGMPoint(lhs.x() + rhs.x(), lhs.y() + rhs.y());
}

CGMPoint subtractPoints(const CGMPoint& lhs, const CGMPoint& rhs)
{
    return CGMPoint(lhs.x() - rhs.x(), lhs.y() - rhs.y());
}

CGMPoint scalePoint(const CGMPoint& point, double scalar)
{
    return CGMPoint(point.x() * scalar, point.y() * scalar);
}

double crossProduct(const CGMPoint& a, const CGMPoint& b)
{
    return a.x() * b.y() - a.y() * b.x();
}

double lengthSquared(const CGMPoint& pt)
{
    return pt.x() * pt.x() + pt.y() * pt.y();
}

double vectorLength(const CGMPoint& pt)
{
    return std::sqrt(lengthSquared(pt));
}

bool vectorsNearlyEqual(const CGMPoint& a, const CGMPoint& b)
{
    double dx = a.x() - b.x();
    double dy = a.y() - b.y();
    double scale = std::max({1.0,
                             std::fabs(a.x()), std::fabs(a.y()),
                             std::fabs(b.x()), std::fabs(b.y())});
    double tol = scale * 1e-6;
    return dx * dx + dy * dy <= tol * tol;
}

// ============================================================================
// Angle Operations
// ============================================================================

double wrapAnglePositive(double angle)
{
    const double twoPi = 2.0 * M_PI;
    double wrapped = std::fmod(angle, twoPi);
    if (wrapped < 0) {
        wrapped += twoPi;
    }
    return wrapped;
}

double angularDistanceCCW(double startAngle, double endAngle)
{
    const double twoPi = 2.0 * M_PI;
    double start = wrapAnglePositive(startAngle);
    double end = wrapAnglePositive(endAngle);
    double dist = end - start;
    if (dist < 0) {
        dist += twoPi;
    }
    return dist;
}

bool angleIsBetweenCCW(double startAngle, double midAngle, double endAngle, double tol)
{
    double span = angularDistanceCCW(startAngle, endAngle);
    double target = angularDistanceCCW(startAngle, midAngle);
    return target <= span + tol;
}

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
    double& detM)
{
    const double ux = u.x();
    const double uy = u.y();
    const double vx = v.x();
    const double vy = v.y();
    detM = ux * vy - uy * vx;
    const double eps = 1e-12;
    if (std::fabs(detM) < eps) {
        return false;
    }

    auto toParam = [&](const CGMPoint& d, double& t) {
        const double dx = d.x();
        const double dy = d.y();
        const double c = (vy * dx - vx * dy) / detM;  // cos(t)
        const double s = (-uy * dx + ux * dy) / detM; // sin(t)
        t = std::atan2(s, c);
    };

    toParam(startDelta, tStart);
    toParam(endDelta, tEnd);
    return true;
}

std::tuple<double, double, double> conjugateDiametersToEllipse(
    const CGMPoint& u,
    const CGMPoint& v)
{
    // Form matrix A = M * M^T where M = [u | v]
    double a = u.x() * u.x() + v.x() * v.x();
    double b = u.x() * u.y() + v.x() * v.y();
    double c = u.y() * u.y() + v.y() * v.y();

    // Eigenvalues via quadratic formula
    double trace = a + c;
    double det = a * c - b * b;
    double disc = trace * trace - 4.0 * det;
    if (disc < 0.0)
        disc = 0.0;
    double sqrtDisc = std::sqrt(disc);

    double lambda1 = (trace + sqrtDisc) / 2.0;
    double lambda2 = (trace - sqrtDisc) / 2.0;

    double rx = std::sqrt(std::max(lambda1, 1e-10));
    double ry = std::sqrt(std::max(lambda2, 1e-10));

    double rotation;
    if (std::fabs(b) > 1e-10) {
        rotation = 0.5 * std::atan2(2.0 * b, a - c) * 180.0 / M_PI;
    } else {
        rotation = (a >= c) ? 0.0 : 90.0;
    }

    return {rx, ry, rotation};
}

// ============================================================================
// Color Utilities
// ============================================================================

bool colorsEqualRgb(const Color& lhs, const Color& rhs)
{
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b;
}

bool colorsNearlyEqualRgb(const Color& lhs, const Color& rhs, int tolerance)
{
    if (tolerance <= 0) {
        return colorsEqualRgb(lhs, rhs);
    }
    auto within = [tolerance](int a, int b) {
        return std::abs(a - b) <= tolerance;
    };
    return within(lhs.r, rhs.r) && within(lhs.g, rhs.g) && within(lhs.b, rhs.b);
}

std::string colorToHexString(const Color& color)
{
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X",
                  static_cast<int>(color.r),
                  static_cast<int>(color.g),
                  static_cast<int>(color.b));
    return std::string(buffer);
}

// ============================================================================
// PNG Format Detection
// ============================================================================

uint8_t clampTruecolorBitDepth(int precision)
{
    if (precision > 8) {
        return 16;
    }
    return 8;
}

bool derivePngFormat(
    ColorSelectionMode selectionMode,
    ColorModel model,
    int cellColorPrecision,
    int defaultColourPrecision,
    int defaultIndexPrecision,
    uint8_t& bitDepth,
    uint8_t& colorType)
{
    int precision = cellColorPrecision;
    if (precision <= 0) {
        precision = (selectionMode == ColorSelectionMode::DIRECT)
                        ? defaultColourPrecision
                        : defaultIndexPrecision;
    }
    if (precision <= 0) {
        precision = 8;
    }

    if (selectionMode == ColorSelectionMode::DIRECT) {
        switch (model) {
        case ColorModel::RGB:
            bitDepth = clampTruecolorBitDepth(precision);
            colorType = 2; // truecolour
            return true;
        default:
            return false;
        }
    }

    return false;
}

// ============================================================================
// Text Utilities
// ============================================================================

std::vector<std::string> splitTextIntoLines(const std::string& text)
{
    std::vector<std::string> lines;
    size_t start = 0;
    const size_t n = text.size();

    while (start <= n) {
        size_t pos = text.find_first_of("\r\n", start);
        if (pos == std::string::npos) {
            lines.emplace_back(text.substr(start));
            break;
        }

        lines.emplace_back(text.substr(start, pos - start));

        size_t next = pos + 1;
        if (next < n && text[pos] == '\r' && text[next] == '\n') {
            ++next;
        }
        start = next;

        if (start == n) {
            lines.emplace_back(std::string());
            break;
        }
    }

    if (lines.empty()) {
        lines.emplace_back(std::string());
    }
    return lines;
}

} // namespace svg
} // namespace opencgm
