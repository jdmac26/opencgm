#include "opencgm/binary_reader.h"
#include "opencgm/command.h"
#include "opencgm/security_limits.h"
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cctype>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <limits>

namespace opencgm {

namespace {

void appendUtf8(std::string& out, uint32_t codepoint) {
    if (codepoint <= 0x7F) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        out.push_back('?');
    }
}

} // namespace

namespace {

constexpr double REF_X = 95.047; // ISO/IEC 8632-1 Annex G (D65 white)
constexpr double REF_Y = 100.0;
constexpr double REF_Z = 108.883;
constexpr double LAB_MAX_AB = 128.0;   // Annex G Table G.1 typical Lab span
constexpr double LUV_MAX_U = 134.0;    // Annex G Table G.2 Luv span
constexpr double LUV_MAX_V = 140.0;

double normalizeUnsigned(int value, int precision) {
    // SECURITY: Validate precision to avoid undefined behavior in bit-shift
    if (precision <= 0 || precision > 64) {
        return 0.0;
    }
    const double maxValue = static_cast<double>((1ULL << precision) - 1ULL);
    if (maxValue <= 0.0) {
        return 0.0;
    }
    return std::clamp(static_cast<double>(value) / maxValue, 0.0, 1.0);
}

double normalizeSigned(int value, int precision) {
    // SECURITY: Validate precision to avoid undefined behavior in bit-shift
    if (precision <= 1 || precision > 64) {
        return 0.0;
    }
    const double maxMagnitude = static_cast<double>((1ULL << (precision - 1)) - 1ULL);
    if (maxMagnitude <= 0.0) {
        return 0.0;
    }
    double normalized = static_cast<double>(value) / maxMagnitude;
    return std::clamp(normalized, -1.0, 1.0);
}

double clamp01(double value) {
    if (std::isnan(value)) {
        return 0.0;
    }
    return std::clamp(value, 0.0, 1.0);
}

uint8_t toByte(double value) {
    return static_cast<uint8_t>(std::lround(clamp01(value) * 255.0));
}

Color xyzToColor(double X, double Y, double Z) {
    double x = X / 100.0;
    double y = Y / 100.0;
    double z = Z / 100.0;

    // Convert to linear sRGB (IEC 61966-2-1)
    double r = 3.2406 * x - 1.5372 * y - 0.4986 * z;
    double g = -0.9689 * x + 1.8758 * y + 0.0415 * z;
    double b = 0.0557 * x - 0.2040 * y + 1.0570 * z;

    auto gammaCorrect = [](double channel) {
        channel = std::max(0.0, channel);
        if (channel <= 0.0031308) {
            return 12.92 * channel;
        }
        return 1.055 * std::pow(channel, 1.0 / 2.4) - 0.055;
    };

    r = gammaCorrect(r);
    g = gammaCorrect(g);
    b = gammaCorrect(b);

    return Color(toByte(r), toByte(g), toByte(b));
}

Color labToColor(double L, double a, double b) {
    const double delta = 6.0 / 29.0;

    auto fInv = [delta](double t) {
        if (t > delta) {
            return t * t * t;
        }
        return 3.0 * delta * delta * (t - 4.0 / 29.0);
    };

    double fy = (L + 16.0) / 116.0;
    double fx = fy + (a / 500.0);
    double fz = fy - (b / 200.0);

    double xr = fInv(fx);
    double yr = fInv(fy);
    double zr = fInv(fz);

    double X = REF_X * xr;
    double Y = REF_Y * yr;
    double Z = REF_Z * zr;

    return xyzToColor(X, Y, Z);
}

Color luvToColor(double L, double uStar, double vStar) {
    if (L <= 0.0) {
        return Color::Black();
    }

    double refDen = REF_X + 15.0 * REF_Y + 3.0 * REF_Z;
    double refU = (4.0 * REF_X) / refDen;
    double refV = (9.0 * REF_Y) / refDen;

    double uPrime = (uStar / (13.0 * L)) + refU;
    double vPrime = (vStar / (13.0 * L)) + refV;

    double Y = (L > 8.0) ? std::pow((L + 16.0) / 116.0, 3.0) : (L / 903.3);
    double denominator = (uPrime - 4.0) * vPrime - uPrime * vPrime;
    if (std::abs(denominator) < 1e-9 || std::abs(vPrime) < 1e-9) {
        double YScaled = Y * REF_Y;
        return xyzToColor(0.0, YScaled, 0.0);
    }

    double X = - (9.0 * Y * uPrime) / denominator;
    double Z = (9.0 * Y - 15.0 * vPrime * Y - vPrime * X) / (3.0 * vPrime);

    double XScaled = X * REF_Y;
    double YScaled = Y * REF_Y;
    double ZScaled = Z * REF_Y;

    return xyzToColor(XScaled, YScaled, ZScaled);
}

Color cmykToColor(double c, double m, double y, double k) {
    double r = 1.0 - std::min(1.0, c + k);
    double g = 1.0 - std::min(1.0, m + k);
    double b = 1.0 - std::min(1.0, y + k);
    return Color(toByte(r), toByte(g), toByte(b));
}

static const uint16_t ISO8859_2_TABLE[] = {
    0x00A0, 0x0104, 0x02D8, 0x0141, 0x00A4, 0x013D, 0x015A, 0x00A7,
    0x00A8, 0x0160, 0x015E, 0x0164, 0x0179, 0x00AD, 0x017D, 0x017B,
    0x00B0, 0x0105, 0x02DB, 0x0142, 0x00B4, 0x013E, 0x015B, 0x02C7,
    0x00B8, 0x0161, 0x015F, 0x0165, 0x017A, 0x02DD, 0x017E, 0x017C,
    0x0154, 0x00C1, 0x00C2, 0x0102, 0x00C4, 0x0139, 0x0106, 0x00C7,
    0x010C, 0x00C9, 0x0118, 0x00CB, 0x011A, 0x00CD, 0x00CE, 0x010E,
    0x0110, 0x0143, 0x0147, 0x00D3, 0x00D4, 0x0150, 0x00D6, 0x00D7,
    0x0158, 0x016E, 0x00DA, 0x0170, 0x00DC, 0x00DD, 0x0162, 0x00DF,
    0x0155, 0x00E1, 0x00E2, 0x0103, 0x00E4, 0x013A, 0x0107, 0x00E7,
    0x010D, 0x00E9, 0x0119, 0x00EB, 0x011B, 0x00ED, 0x00EE, 0x010F,
    0x0111, 0x0144, 0x0148, 0x00F3, 0x00F4, 0x0151, 0x00F6, 0x00F7,
    0x0159, 0x016F, 0x00FA, 0x0171, 0x00FC, 0x00FD, 0x0163, 0x02D9
};

static const uint16_t ISO8859_3_TABLE[] = {
    0x00A0, 0x0126, 0x02D8, 0x00A3, 0x00A4, 0xFFFD, 0x0124, 0x00A7,
    0x00A8, 0x0130, 0x015E, 0x011E, 0x0134, 0x00AD, 0xFFFD, 0x017B,
    0x00B0, 0x0127, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x0125, 0x00B7,
    0x00B8, 0x0131, 0x015F, 0x011F, 0x0135, 0x00BD, 0xFFFD, 0x017C,
    0x00C0, 0x00C1, 0x00C2, 0xFFFD, 0x00C4, 0x010A, 0x0108, 0x00C7,
    0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
    0xFFFD, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x0120, 0x00D6, 0x00D7,
    0x011C, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x016C, 0x015C, 0x00DF,
    0x00E0, 0x00E1, 0x00E2, 0xFFFD, 0x00E4, 0x010B, 0x0109, 0x00E7,
    0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
    0xFFFD, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x0121, 0x00F6, 0x00F7,
    0x011D, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x016D, 0x015D, 0x02D9
};

static const uint16_t ISO8859_4_TABLE[] = {
    0x00A0, 0x0104, 0x0138, 0x0156, 0x00A4, 0x0128, 0x013B, 0x00A7,
    0x00A8, 0x0160, 0x0112, 0x0122, 0x0166, 0x00AD, 0x017D, 0x00AF,
    0x00B0, 0x0105, 0x02DB, 0x0157, 0x00B4, 0x0129, 0x013C, 0x02C7,
    0x00B8, 0x0161, 0x0113, 0x0123, 0x0167, 0x014A, 0x017E, 0x014B,
    0x0100, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x012E,
    0x010C, 0x00C9, 0x0118, 0x00CB, 0x0116, 0x00CD, 0x00CE, 0x012A,
    0x0110, 0x0145, 0x014C, 0x0136, 0x00D4, 0x00D5, 0x00D6, 0x00D7,
    0x00D8, 0x0172, 0x00DA, 0x00DB, 0x00DC, 0x0168, 0x016A, 0x00DF,
    0x0101, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x012F,
    0x010D, 0x00E9, 0x0119, 0x00EB, 0x0117, 0x00ED, 0x00EE, 0x012B,
    0x0111, 0x0146, 0x014D, 0x0137, 0x00F4, 0x00F5, 0x00F6, 0x00F7,
    0x00F8, 0x0173, 0x00FA, 0x00FB, 0x00FC, 0x0169, 0x016B, 0x02D9
};

static const uint16_t ISO8859_5_TABLE[] = {
    0x00A0, 0x0401, 0x0402, 0x0403, 0x0404, 0x0405, 0x0406, 0x0407,
    0x0408, 0x0409, 0x040A, 0x040B, 0x040C, 0x00AD, 0x040E, 0x040F,
    0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417,
    0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F,
    0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
    0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F,
    0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437,
    0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
    0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447,
    0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F,
    0x2116, 0x0451, 0x0452, 0x0453, 0x0454, 0x0455, 0x0456, 0x0457,
    0x0458, 0x0459, 0x045A, 0x045B, 0x045C, 0x00A7, 0x045E, 0x045F
};

static const uint16_t ISO8859_6_TABLE[] = {
    0x00A0, 0xFFFD, 0xFFFD, 0xFFFD, 0x00A4, 0xFFFD, 0xFFFD, 0xFFFD,
    0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0x060C, 0x00AD, 0xFFFD, 0xFFFD,
    0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
    0xFFFD, 0xFFFD, 0xFFFD, 0x061B, 0xFFFD, 0xFFFD, 0xFFFD, 0x061F,
    0xFFFD, 0x0621, 0x0622, 0x0623, 0x0624, 0x0625, 0x0626, 0x0627,
    0x0628, 0x0629, 0x062A, 0x062B, 0x062C, 0x062D, 0x062E, 0x062F,
    0x0630, 0x0631, 0x0632, 0x0633, 0x0634, 0x0635, 0x0636, 0x0637,
    0x0638, 0x0639, 0x063A, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
    0x0640, 0x0641, 0x0642, 0x0643, 0x0644, 0x0645, 0x0646, 0x0647,
    0x0648, 0x0649, 0x064A, 0x064B, 0x064C, 0x064D, 0x064E, 0x064F,
    0x0650, 0x0651, 0x0652, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
    0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD
};

static const uint16_t ISO8859_7_TABLE[] = {
    0x00A0, 0x2018, 0x2019, 0x00A3, 0x20AC, 0x20AF, 0x00A6, 0x00A7,
    0x00A8, 0x00A9, 0x037A, 0x00AB, 0x00AC, 0x00AD, 0xFFFD, 0x2015,
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x0384, 0x0385, 0x0386, 0x00B7,
    0x0388, 0x0389, 0x038A, 0x00BB, 0x038C, 0x00BD, 0x038E, 0x038F,
    0x0390, 0x0391, 0x0392, 0x0393, 0x0394, 0x0395, 0x0396, 0x0397,
    0x0398, 0x0399, 0x039A, 0x039B, 0x039C, 0x039D, 0x039E, 0x039F,
    0x03A0, 0x03A1, 0xFFFD, 0x03A3, 0x03A4, 0x03A5, 0x03A6, 0x03A7,
    0x03A8, 0x03A9, 0x03AA, 0x03AB, 0x03AC, 0x03AD, 0x03AE, 0x03AF,
    0x03B0, 0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5, 0x03B6, 0x03B7,
    0x03B8, 0x03B9, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BE, 0x03BF,
    0x03C0, 0x03C1, 0x03C2, 0x03C3, 0x03C4, 0x03C5, 0x03C6, 0x03C7,
    0x03C8, 0x03C9, 0x03CA, 0x03CB, 0x03CC, 0x03CD, 0x03CE, 0xFFFD
};

static const uint16_t ISO8859_8_TABLE[] = {
    0x00A0, 0xFFFD, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7,
    0x00A8, 0x00A9, 0x00D7, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7,
    0x00B8, 0x00B9, 0x00F7, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0xFFFD,
    0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
    0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
    0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
    0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
    0x2017, 0x05D0, 0x05D1, 0x05D2, 0x05D3, 0x05D4, 0x05D5, 0x05D6,
    0x05D7, 0x05D8, 0x05D9, 0x05DA, 0x05DB, 0x05DC, 0x05DD, 0x05DE,
    0x05DF, 0x05E0, 0x05E1, 0x05E2, 0x05E3, 0x05E4, 0x05E5, 0x05E6,
    0x05E7, 0x05E8, 0x05E9, 0x05EA, 0xFFFD, 0xFFFD, 0x200E, 0x200F,
    0xFFFD
};

static const uint16_t ISO8859_9_TABLE[] = {
    0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x20AC, 0x00A5, 0x0160, 0x00A7,
    0x0161, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x017D, 0x00B5, 0x00B6, 0x00B7,
    0x017E, 0x00B9, 0x00BA, 0x00BB, 0x0152, 0x0153, 0x0178, 0x00BF,
    0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7,
    0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
    0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7,
    0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF,
    0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7,
    0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
    0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7,
    0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF
};

static const uint16_t ISO8859_10_TABLE[] = {
    0x00A0, 0x0104, 0x0105, 0x0141, 0x20AC, 0x201E, 0x0160, 0x00A7,
    0x0161, 0x00A9, 0x0218, 0x00AB, 0x0179, 0x00AD, 0x017A, 0x017B,
    0x00B0, 0x00B1, 0x010C, 0x0142, 0x017D, 0x201D, 0x00B6, 0x00B7,
    0x017E, 0x010D, 0x0219, 0x00BB, 0x0152, 0x0153, 0x0178, 0x017C,
    0x00C0, 0x00C1, 0x00C2, 0x0102, 0x00C4, 0x0106, 0x00C6, 0x00C7,
    0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
    0x0110, 0x0143, 0x00D2, 0x00D3, 0x00D4, 0x0150, 0x00D6, 0x015A,
    0x0170, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x0118, 0x021A, 0x00DF,
    0x00E0, 0x00E1, 0x00E2, 0x0103, 0x00E4, 0x0107, 0x00E6, 0x00E7,
    0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
    0x0111, 0x0144, 0x00F2, 0x00F3, 0x00F4, 0x0151, 0x00F6, 0x015B,
    0x0171, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x0119, 0x021B, 0x00FF
};

static const uint16_t ISO8859_13_TABLE[] = {
    0x00A0, 0x201D, 0x00A2, 0x00A3, 0x00A4, 0x201E, 0x00A6, 0x00A7,
    0x00D8, 0x00A9, 0x0156, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00C6,
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x201C, 0x00B5, 0x00B6, 0x00B7,
    0x00F8, 0x00B9, 0x0157, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00E6,
    0x0104, 0x012E, 0x0100, 0x0106, 0x00C4, 0x00C5, 0x0118, 0x0112,
    0x010C, 0x00C9, 0x0179, 0x0116, 0x0122, 0x0136, 0x012A, 0x013B,
    0x0160, 0x0143, 0x0145, 0x00D3, 0x014C, 0x00D5, 0x00D6, 0x00D7,
    0x0172, 0x0141, 0x015A, 0x016A, 0x00DC, 0x017B, 0x017D, 0x00DF,
    0x0105, 0x012F, 0x0101, 0x0107, 0x00E4, 0x00E5, 0x0119, 0x0113,
    0x010D, 0x00E9, 0x017A, 0x0117, 0x0123, 0x0137, 0x012B, 0x013C,
    0x0161, 0x0144, 0x0146, 0x00F3, 0x014D, 0x00F5, 0x00F6, 0x00F7,
    0x0173, 0x0142, 0x015B, 0x016B, 0x00FC, 0x017C, 0x017E, 0x2019
};

static const uint16_t ISO8859_14_TABLE[] = {
    0x00A0, 0x1E02, 0x1E03, 0x00A3, 0x010A, 0x010B, 0x1E0A, 0x00A7,
    0x1E80, 0x00A9, 0x1E82, 0x1E0B, 0x1EF2, 0x00AD, 0x00AE, 0x0178,
    0x1E1E, 0x1E1F, 0x0120, 0x0121, 0x1E40, 0x1E41, 0x00B6, 0x1E56,
    0x1E81, 0x1E57, 0x1E83, 0x1E60, 0x1EF3, 0x1E84, 0x1E85, 0x1E61,
    0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7,
    0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
    0x0174, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x1E6A,
    0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x0176, 0x00DF,
    0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7,
    0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
    0x0175, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x1E6B,
    0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x0177, 0x00FF
};

static const uint16_t ISO8859_15_TABLE[] = {
    0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x20AC, 0x00A5, 0x0160, 0x00A7,
    0x0161, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x017D, 0x00B5, 0x00B6, 0x00B7,
    0x017E, 0x00B9, 0x00BA, 0x00BB, 0x0152, 0x0153, 0x0178, 0x00BF,
    0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7,
    0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
    0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7,
    0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF,
    0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7,
    0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
    0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7,
    0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF
};

static const uint16_t ISO8859_16_TABLE[] = {
    0x00A0, 0x0104, 0x0105, 0x0141, 0x20AC, 0x201E, 0x0160, 0x00A7,
    0x0161, 0x00A9, 0x0218, 0x00AB, 0x0179, 0x00AD, 0x017A, 0x017B,
    0x00B0, 0x00B1, 0x010C, 0x0142, 0x017D, 0x201D, 0x00B6, 0x00B7,
    0x017E, 0x010D, 0x0219, 0x00BB, 0x0152, 0x0153, 0x0178, 0x017C,
    0x00C0, 0x00C1, 0x00C2, 0x0102, 0x00C4, 0x0106, 0x00C6, 0x00C7,
    0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
    0x0110, 0x0143, 0x00D2, 0x00D3, 0x00D4, 0x0150, 0x00D6, 0x015A,
    0x0170, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x0118, 0x021A, 0x00DF,
    0x00E0, 0x00E1, 0x00E2, 0x0103, 0x00E4, 0x0107, 0x00E6, 0x00E7,
    0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
    0x0111, 0x0144, 0x00F2, 0x00F3, 0x00F4, 0x0151, 0x00F6, 0x015B,
    0x0171, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x0119, 0x021B, 0x00FF
};

uint32_t mapFromTable(unsigned char byte, const uint16_t* table, CGMFile* file, const std::string& name) {
    if (byte < 0xA0) {
        return static_cast<uint32_t>(byte);
    }
    uint16_t value = table[byte - 0xA0];
    if (value == 0xFFFD) {
        if (file) {
            file->reportUnsupportedCharset(name.empty() ? "undefined" : name + " undefined");
        }
        return 0xFFFD;
    }
    return static_cast<uint32_t>(value);
}

} // namespace

DefaultBinaryReader::DefaultBinaryReader(std::istream& stream, CGMFile* cgm, ICommandFactory* factory)
    : reader_(stream)
    , cgm_(cgm)
    , commandFactory_(factory)
    , currentArg_(0)
    , positionInCurrentArgument_(0)
    , currentCommand_(nullptr)
    , finalFlag_(true) {
}

void DefaultBinaryReader::readCommands() {
    // Note: Leading padding byte handling removed - CGM files typically
    // start with BEGIN METAFILE header (0x00 0x2x) where the first 0x00
    // is part of the valid header, not padding. If padding handling is
    // needed for specific file sources, it should be done at the file
    // loading level before passing to the reader.

    while (true) {
        auto cmd = readCommand();
        if (!cmd) {
            break;
        }

        // Clear arguments after reading
        arguments_.clear();
        cgm_->commands().push_back(std::move(cmd));
    }
}

std::unique_ptr<Command> DefaultBinaryReader::readCommand() {
    // Read command header (16 bits)
    int k;
    try {
        uint8_t byte1 = static_cast<uint8_t>(reader_.get());
        uint8_t byte2 = static_cast<uint8_t>(reader_.get());

        if (reader_.eof()) {
            return nullptr;
        }

        k = (byte1 << 8) | byte2;
    } catch (const std::exception&) {
        return nullptr;
    }

    // Parse command header
    int elementClass = k >> 12;
    int elementId = (k >> 5) & 127;
    int argumentCount = k & 31;

    // Save current state
    Command* oldCommand = currentCommand_;
    auto oldArguments = arguments_;
    int oldCurrentArg = currentArg_;
    int oldPositionInCurrentArgument = positionInCurrentArgument_;

    auto restoreState = [&]() {
        currentCommand_ = oldCommand;
        arguments_ = oldArguments;
        currentArg_ = oldCurrentArg;
        positionInCurrentArgument_ = oldPositionInCurrentArgument;
    };

    // Create new command
    auto cmd = commandFactory_->createCommand(elementId, elementClass, cgm_);
    currentCommand_ = cmd.get();
    arguments_.clear();
    currentArg_ = 0;

    // Read arguments
    try {
        readArguments(argumentCount);
    } catch (const std::exception& ex) {
        const std::string errorMessage = ex.what();
        if (errorMessage.find("Unexpected EOF while reading ") != std::string::npos) {
            messages_.emplace_back(
                Severity::Fatal,
                static_cast<ClassCode>(elementClass),
                elementId,
                errorMessage,
                currentCommand_ ? currentCommand_->toString() : "unknown"
            );
            restoreState();
            return nullptr;
        }
        restoreState();
        throw;
    }

    // Read command data
    try {
        currentCommand_->readFromBinary(*this);
    } catch (const std::exception& ex) {
        messages_.emplace_back(
            Severity::Fatal,
            static_cast<ClassCode>(elementClass),
            elementId,
            ex.what(),
            currentCommand_->toString()
        );
        readArgumentEnd();
    }

    ensureAllArgumentsWereRead();

    // Restore state
    restoreState();

    return cmd;
}

void DefaultBinaryReader::readArguments(int argumentsCount) {
    finalFlag_ = true;
    if (argumentsCount != 31) {
        readShortFormCommandArguments(argumentsCount);
    } else {
        readLongFormCommandArguments();
    }
}

void DefaultBinaryReader::readShortFormCommandArguments(int argumentsCount) {
    // SECURITY: Validate argument count before allocation (matches long-form validation)
    if (argumentsCount < 0) {
        throw std::runtime_error("Negative argument count in short-form command");
    }
    security::validateAllocationSize(
        static_cast<size_t>(argumentsCount),
        security::MAX_COMMAND_ARGUMENTS_SIZE,
        "short-form command arguments"
    );

    arguments_.resize(argumentsCount);
    for (int i = 0; i < argumentsCount; i++) {
        int value = reader_.get();
        if (value == EOF) {
            throw std::runtime_error("Unexpected EOF while reading short-form command arguments");
        }
        arguments_[i] = static_cast<uint8_t>(value);
    }

    // Align on word boundary
    if (argumentsCount % 2 == 1) {
        int pad = reader_.get();
        if (pad == EOF) {
            throw std::runtime_error("Unexpected EOF while reading short-form alignment padding");
        }
    }
}

void DefaultBinaryReader::readLongFormCommandArguments() {
    while (true) {
        int argumentsCount = readInt16Direct();
        if (argumentsCount < 0) {
            throw std::runtime_error("Unexpected EOF while reading long-form argument length");
        }

        bool hasMore = (argumentsCount & 0x8000) != 0;
        argumentsCount &= 0x7FFF;

        size_t oldSize = arguments_.size();
        size_t newSize = oldSize + static_cast<size_t>(argumentsCount);

        // SECURITY: Validate total argument buffer size before allocation
        security::validateAllocationSize(
            newSize,
            security::MAX_COMMAND_ARGUMENTS_SIZE,
            "command arguments"
        );

        arguments_.resize(newSize);

        for (int i = 0; i < argumentsCount; i++) {
            int value = reader_.get();
            if (value == EOF) {
                throw std::runtime_error("Unexpected EOF while reading long-form command arguments");
            }
            arguments_[oldSize + i] = static_cast<uint8_t>(value);
        }

        if (argumentsCount % 2 == 1) {
            int pad = reader_.get();
            if (pad == EOF) {
                throw std::runtime_error("Unexpected EOF while reading long-form alignment padding");
            }
        }

        finalFlag_ = !hasMore;
        if (!hasMore) {
            break;
        }
    }
}

int DefaultBinaryReader::readInt16Direct() {
    int c1 = reader_.get();
    if (c1 == EOF) {
        return -1;
    }
    int c2 = reader_.get();
    if (c2 == EOF) {
        return -1;
    }
    return ((static_cast<uint8_t>(c1) << 8) | static_cast<uint8_t>(c2));
}

// ============================================================================
// Basic reading methods
// ============================================================================

uint8_t DefaultBinaryReader::readByte() {
    skipBits();
    if (currentArg_ >= static_cast<int>(arguments_.size())) {
        throw std::runtime_error(getErrorMessage("readByte"));
    }
    return arguments_[currentArg_++];
}

char DefaultBinaryReader::readChar() {
    skipBits();
    if (currentArg_ >= static_cast<int>(arguments_.size())) {
        throw std::runtime_error(getErrorMessage("readChar"));
    }
    return static_cast<char>(arguments_[currentArg_++]);
}

int DefaultBinaryReader::readSignedInt8() {
    return static_cast<int8_t>(readByte());
}

int DefaultBinaryReader::readSignedInt16() {
    skipBits();
    if (currentArg_ + 1 >= static_cast<int>(arguments_.size())) {
        throw std::runtime_error(getErrorMessage("readSignedInt16"));
    }
    // Combine bytes as unsigned, then cast to signed for proper sign extension
    int16_t result = static_cast<int16_t>((arguments_[currentArg_] << 8) | arguments_[currentArg_ + 1]);
    currentArg_ += 2;
    return result;
}

int DefaultBinaryReader::readSignedInt24() {
    skipBits();
    if (currentArg_ + 2 >= static_cast<int>(arguments_.size())) {
        throw std::runtime_error(getErrorMessage("readSignedInt24"));
    }
    int result = (arguments_[currentArg_] << 16) |
                 (arguments_[currentArg_ + 1] << 8) |
                 arguments_[currentArg_ + 2];
    currentArg_ += 3;
    // Sign extend if negative
    if (result & 0x800000) {
        result |= 0xFF000000;
    }
    return result;
}

int DefaultBinaryReader::readSignedInt32() {
    skipBits();
    if (currentArg_ + 3 >= static_cast<int>(arguments_.size())) {
        throw std::runtime_error(getErrorMessage("readSignedInt32"));
    }
    int result = (arguments_[currentArg_] << 24) |
                 (arguments_[currentArg_ + 1] << 16) |
                 (arguments_[currentArg_ + 2] << 8) |
                 arguments_[currentArg_ + 3];
    currentArg_ += 4;
    return result;
}

int DefaultBinaryReader::readInt(int precision) {
    skipBits();
    switch (precision) {
        case 8:  return readSignedInt8();
        case 16: return readSignedInt16();
        case 24: return readSignedInt24();
        case 32: return readSignedInt32();
        default:
            unsupported("Unsupported integer precision: " + std::to_string(precision));
            return readSignedInt16();
    }
}

int DefaultBinaryReader::readUInt8() {
    return static_cast<uint8_t>(readByte());
}

int DefaultBinaryReader::readUInt16() {
    skipBits();

    if (currentArg_ + 1 < static_cast<int>(arguments_.size())) {
        int result = (arguments_[currentArg_] << 8) | arguments_[currentArg_ + 1];
        currentArg_ += 2;
        return result;
    }

    // Handle edge case: only 8 bits left
    if (currentArg_ < static_cast<int>(arguments_.size())) {
        return arguments_[currentArg_++];
    }

    throw std::runtime_error(getErrorMessage("readUInt16"));
}

int DefaultBinaryReader::readUInt24() {
    return readSignedInt24();
}

int DefaultBinaryReader::readUInt32() {
    return readSignedInt32();
}

int DefaultBinaryReader::readUIntBit(int numBits) {
    if (currentArg_ >= static_cast<int>(arguments_.size())) {
        throw std::runtime_error(getErrorMessage("readUIntBit"));
    }

    int bitsPosition = 8 - numBits - positionInCurrentArgument_;
    int mask = ((1 << numBits) - 1) << bitsPosition;
    int result = (arguments_[currentArg_] & mask) >> bitsPosition;

    positionInCurrentArgument_ += numBits;
    if (positionInCurrentArgument_ % 8 == 0) {
        positionInCurrentArgument_ = 0;
        currentArg_++;
    }

    return result;
}

int DefaultBinaryReader::readUInt1() {
    return readUIntBit(1);
}

int DefaultBinaryReader::readUInt2() {
    return readUIntBit(2);
}

int DefaultBinaryReader::readUInt4() {
    return readUIntBit(4);
}

int DefaultBinaryReader::readUInt(int precision) {
    switch (precision) {
        case 1:  return readUInt1();
        case 2:  return readUInt2();
        case 4:  return readUInt4();
        case 8:  return readUInt8();
        case 16: return readUInt16();
        case 24: return readUInt24();
        case 32: return readUInt32();
        default:
            unsupported("Unsupported uint precision: " + std::to_string(precision));
            return readUInt8();
    }
}

// ============================================================================
// IBinaryReader interface implementation
// ============================================================================

int DefaultBinaryReader::readEnum() {
    return readSignedInt16();
}

int DefaultBinaryReader::readIndex() {
    return readInt(cgm_->indexPrecision());
}

int DefaultBinaryReader::readName() {
    return readInt(cgm_->namePrecision());
}

int DefaultBinaryReader::readInt() {
    return readInt(cgm_->integerPrecision());
}

bool DefaultBinaryReader::readBool() {
    return readEnum() != 0;
}

std::string DefaultBinaryReader::readString() {
    int length = getStringCount();
    return readString(length);
}

std::string DefaultBinaryReader::readRawString() {
    int length = getStringCount();
    return readRawString(length);
}

std::string DefaultBinaryReader::readString(int length) {
    return decodeText(readRawString(length));
}

std::string DefaultBinaryReader::readRawString(int length) {
    std::string result;
    result.reserve(length);

    try {
        for (int i = 0; i < length; i++) {
            result += static_cast<char>(readByte());
        }
    } catch (...) {
        // Return partial string if we run out of data
    }

    return result;
}

std::string DefaultBinaryReader::readFixedString() {
    int length = getStringCount();
    std::string raw;
    raw.reserve(length);

    for (int i = 0; i < length; i++) {
        raw += readChar();
    }

    return decodeText(raw);
}

std::string DefaultBinaryReader::decodeText(const std::string& raw) const {
    if (!cgm_ || raw.empty()) {
        return raw;
    }

    const unsigned char* data = reinterpret_cast<const unsigned char*>(raw.data());
    size_t length = raw.size();

    std::string result;
    result.reserve(length);

    auto* file = const_cast<CGMFile*>(cgm_);
    int coding = cgm_->characterCoding();
    const auto& charsetList = cgm_->characterSetList();

    enum class Charset {
        ASCII,
        LATIN1,
        LATIN2,
        LATIN3,
        LATIN4,
        LATIN5,
        LATIN6,
        LATIN7,
        LATIN8,
        LATIN9,
        LATIN10,
        GREEK,
        GREEK_SUPP,
        CYRILLIC,
        ARABIC,
        HEBREW,
        UNKNOWN
    };

    struct GSet {
        Charset charset = Charset::ASCII;
        bool is96 = false;
        bool multibyte = false;
        std::string designation;
    };

    auto normalizeDesignation = [](const std::string& designation) -> std::string {
        std::string normalized;
        normalized.reserve(designation.size());
        for (char ch : designation) {
            if (!std::isspace(static_cast<unsigned char>(ch))) {
                normalized.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            }
        }
        return normalized;
    };

    auto resolveDesignation = [&](const std::string& designation, bool is96, bool multibyte) -> GSet {
        GSet gset;
        gset.is96 = is96;
        gset.multibyte = multibyte;
        gset.designation = normalizeDesignation(designation);

        const std::string& upper = gset.designation;
        if (upper.empty() || upper == "B" || upper == "ASCII" || upper == "ISO646" ||
            upper == "ISOIR6" || upper == "ISO-IR6" || upper == "ISO_IR6" ||
            upper == "ISO646IRV" || upper == "ISO646-US" || upper == "ISO646US" ||
            upper == "USASCII" || upper == "ANSI_X3.4-1968" || upper == "ANSI_X3.4") {
            gset.charset = Charset::ASCII;
        } else if (upper == "A" || upper == "L" || upper == "LATIN1" || upper == "LATIN-1" ||
                   upper == "ISO8859-1" || upper == "ISO8859_1" || upper == "ISO-8859-1" ||
                   upper == "ISO88591" || upper == "ISOIR100" || upper == "ISO-IR100" ||
                   upper == "ISO_IR100" || upper == "CP819") {
            gset.charset = Charset::LATIN1;
        } else if (upper == "ISOIR101" || upper == "ISO-IR101" || upper == "ISO_IR101" ||
                   upper == "LATIN2" || upper == "LATIN-2" || upper == "ISO8859-2" ||
                   upper == "ISO88592" || upper == "ISO8859_2") {
            gset.charset = Charset::LATIN2;
        } else if (upper == "ISOIR109" || upper == "ISO-IR109" || upper == "ISO_IR109" ||
                   upper == "LATIN3" || upper == "LATIN-3" || upper == "ISO8859-3" ||
                   upper == "ISO88593" || upper == "ISO8859_3") {
            gset.charset = Charset::LATIN3;
        } else if (upper == "ISOIR110" || upper == "ISO-IR110" || upper == "ISO_IR110" ||
                   upper == "LATIN4" || upper == "LATIN-4" || upper == "ISO8859-4" ||
                   upper == "ISO88594" || upper == "ISO8859_4") {
            gset.charset = Charset::LATIN4;
        } else if (upper == "ISOIR148" || upper == "ISO-IR148" || upper == "ISO_IR148" ||
                   upper == "LATIN5" || upper == "LATIN-5" || upper == "ISO8859-9" ||
                   upper == "ISO88599" || upper == "ISO8859_9") {
            gset.charset = Charset::LATIN5;
        } else if (upper == "ISOIR157" || upper == "ISO-IR157" || upper == "ISO_IR157" ||
                   upper == "LATIN6" || upper == "LATIN-6" || upper == "ISO8859-10" ||
                   upper == "ISO885910" || upper == "ISO8859_10") {
            gset.charset = Charset::LATIN6;
        } else if (upper == "ISOIR126" || upper == "ISO-IR126" || upper == "ISO_IR126" ||
                   upper == "ELOT927" || upper == "GREEK" || upper == "ISO8859-7" ||
                   upper == "ISO88597" || upper == "ISO8859_7") {
            gset.charset = Charset::GREEK;
        } else if (upper == "ISOIR144" || upper == "ISO-IR144" || upper == "ISO_IR144" ||
                   upper == "CYRILLIC" || upper == "ISO8859-5" || upper == "ISO88595" ||
                   upper == "ISO8859_5") {
            gset.charset = Charset::CYRILLIC;
        } else if (upper == "ISOIR127" || upper == "ISO-IR127" || upper == "ISO_IR127" ||
                   upper == "ARABIC" || upper == "ISO8859-6" || upper == "ISO88596" ||
                   upper == "ISO8859_6") {
            gset.charset = Charset::ARABIC;
        } else if (upper == "ISOIR138" || upper == "ISO-IR138" || upper == "ISO_IR138" ||
                   upper == "HEBREW" || upper == "ISO8859-8" || upper == "ISO88598" ||
                   upper == "ISO8859_8") {
            gset.charset = Charset::HEBREW;
        } else if (upper == "ISOIR154" || upper == "ISO-IR154" || upper == "ISO_IR154" ||
                   upper == "LATIN7" || upper == "LATIN-7" || upper == "ISO8859-13" ||
                   upper == "ISO885913" || upper == "ISO8859_13") {
            gset.charset = Charset::LATIN7;
        } else if (upper == "ISOIR155" || upper == "ISO-IR155" || upper == "ISO_IR155" ||
                   upper == "LATIN8" || upper == "LATIN-8" || upper == "ISO8859-14" ||
                   upper == "ISO885914" || upper == "ISO8859_14") {
            gset.charset = Charset::LATIN8;
        } else if (upper == "ISOIR203" || upper == "ISO-IR203" || upper == "ISO_IR203" ||
                   upper == "LATIN9" || upper == "LATIN-9" || upper == "ISO8859-15" ||
                   upper == "ISO885915" || upper == "ISO8859_15") {
            gset.charset = Charset::LATIN9;
        } else if (upper == "ISOIR226" || upper == "ISO-IR226" || upper == "ISO_IR226" ||
                   upper == "LATIN10" || upper == "LATIN-10" || upper == "ISO8859-16" ||
                   upper == "ISO885916" || upper == "ISO8859_16") {
            gset.charset = Charset::LATIN10;
        } else if (upper == "ISOIR143" || upper == "ISO-IR143" || upper == "ISO_IR143" ||
                   upper == "GREEK-SUPP" || upper == "GREEK_SUPP") {
            gset.charset = Charset::GREEK_SUPP;
        } else {
            gset.charset = Charset::UNKNOWN;
            if (file) {
                file->reportUnsupportedCharset(upper);
            }
        }

        if (multibyte && file) {
            file->reportUnsupportedCharset(upper.empty() ? "MULTIBYTE" : upper);
        }

        return gset;
    };

    auto gsetFromIndex = [&](int index) -> GSet {
        if (index > 0 && index <= static_cast<int>(charsetList.size())) {
            const auto& entry = charsetList[index - 1];
            bool is96 = (entry.first == 1);
            return resolveDesignation(entry.second, is96, false);
        }
        return resolveDesignation("B", false, false);
    };

    GSet gsets[4];
    gsets[0] = gsetFromIndex(cgm_->characterSetIndex());
    gsets[1] = gsetFromIndex(cgm_->alternateCharacterSetIndex());
    gsets[2] = resolveDesignation("B", false, false);
    gsets[3] = resolveDesignation("B", false, false);

    if (gsets[0].charset == Charset::ASCII && gsets[1].charset == Charset::ASCII && coding == 1) {
        gsets[1] = resolveDesignation("ISO8859-1", true, false);
    }

    auto decodeFromGSet = [&](const GSet& gset,
                              const unsigned char* buffer,
                              size_t remaining,
                              bool isGL,
                              size_t& consumed) -> uint32_t {
        consumed = 0;

        if (remaining == 0) {
            return 0xFFFD;
        }

        if (gset.multibyte) {
            consumed = std::min<size_t>(remaining, 2);
            if (file) {
                const std::string name = gset.designation.empty() ? "MULTIBYTE" : gset.designation;
                file->reportUnsupportedCharset(name);
            }
            return 0xFFFD;
        }

        unsigned char byte = buffer[0];
        unsigned char value = byte & 0x7F;
        consumed = 1;

        auto reportControl = [&]() -> uint32_t {
            if (file) {
                file->reportIllegalControl(value);
            }
            return 0xFFFD;
        };

        auto mapIso8859 = [&](const uint16_t* table, const std::string& designation) -> uint32_t {
            if (isGL) {
                if (value < 0x20) {
                    return reportControl();
                }
                return static_cast<uint32_t>(value);
            }
            return mapFromTable(byte, table, file, designation);
        };

        switch (gset.charset) {
        case Charset::ASCII: {
            if (value < 0x20 && value != '\t' && value != '\n') {
                return reportControl();
            }
            if (value == 0x7F) {
                return reportControl();
            }
            return static_cast<uint32_t>(value);
        }
        case Charset::LATIN1: {
            if (isGL) {
                if (value < 0x20) {
                    return reportControl();
                }
                return static_cast<uint32_t>(value);
            }
            return static_cast<uint32_t>(byte);
        }
        case Charset::LATIN2:
            return mapIso8859(ISO8859_2_TABLE, gset.designation);
        case Charset::LATIN3:
            return mapIso8859(ISO8859_3_TABLE, gset.designation);
        case Charset::LATIN4:
            return mapIso8859(ISO8859_4_TABLE, gset.designation);
        case Charset::LATIN5:
            return mapIso8859(ISO8859_9_TABLE, gset.designation);
        case Charset::LATIN6:
            return mapIso8859(ISO8859_10_TABLE, gset.designation);
        case Charset::LATIN7:
            return mapIso8859(ISO8859_13_TABLE, gset.designation);
        case Charset::LATIN8:
            return mapIso8859(ISO8859_14_TABLE, gset.designation);
        case Charset::LATIN9:
            return mapIso8859(ISO8859_15_TABLE, gset.designation);
        case Charset::LATIN10:
            return mapIso8859(ISO8859_16_TABLE, gset.designation);
        case Charset::GREEK:
            return mapIso8859(ISO8859_7_TABLE, gset.designation);
        case Charset::GREEK_SUPP:
            return mapIso8859(ISO8859_7_TABLE, gset.designation);
        case Charset::CYRILLIC:
            return mapIso8859(ISO8859_5_TABLE, gset.designation);
        case Charset::ARABIC:
            return mapIso8859(ISO8859_6_TABLE, gset.designation);
        case Charset::HEBREW:
            return mapIso8859(ISO8859_8_TABLE, gset.designation);
        default:  // Charset::UNKNOWN - fall back to ASCII passthrough
            if (file) {
                const std::string name = gset.designation.empty() ? "UNKNOWN" : gset.designation;
                file->reportUnsupportedCharset(name);
            }
            // Fall back to ASCII-like behavior for printable characters
            if (value >= 0x20 && value < 0x7F) {
                return static_cast<uint32_t>(value);
            }
            if (value == '\t' || value == '\n') {
                return static_cast<uint32_t>(value);
            }
            return 0xFFFD;  // Only replace actual invalid characters
        }
    };

    auto readDesignation = [&](size_t& index) -> std::string {
        std::string designation;
        while (index < length) {
            unsigned char ch = data[index++];
            designation.push_back(static_cast<char>(ch));
            if (ch >= 0x40 && ch <= 0x7E) {
                break;
            }
        }
        return designation;
    };

    auto emitInvalidControl = [&](unsigned char control) {
        if (file) {
            file->reportIllegalControl(control);
        }
        appendUtf8(result, 0xFFFD);
    };

    int glIndex = 0;
    int grIndex = 1;

    if (coding == 5) { // ISO/IEC 10646 (UCS-2) big-endian
        size_t i = 0;
        while (i + 1 < length) {
            uint16_t codePoint = static_cast<uint16_t>((data[i] << 8) | data[i + 1]);
            appendUtf8(result, codePoint);
            i += 2;
        }
        if (i < length) {
            appendUtf8(result, data[i]);
        }
    } else {
        size_t i = 0;
        while (i < length) {
            unsigned char byte = data[i];

            if (byte == 0x1B) { // ESC
                if (i + 1 >= length) {
                    break;
                }

                size_t escIndex = i + 1;
                unsigned char selector = data[escIndex++];
                bool multibyte = false;

                if (selector == '$') {
                    multibyte = true;
                    if (escIndex >= length) {
                        break;
                    }
                    selector = data[escIndex++];
                }

                auto applyDesignation = [&](int target, bool is96, bool designationConsumed, std::string designation) {
                    if (!designationConsumed) {
                        designation = readDesignation(escIndex);
                    }
                    gsets[target] = resolveDesignation(designation, is96, multibyte);
                };

                bool handled = false;

                if (!multibyte) {
                    switch (selector) {
                    case 'n':
                        glIndex = 2;
                        handled = true;
                        break;
                    case 'o':
                        glIndex = 3;
                        handled = true;
                        break;
                    case '~':
                        grIndex = 1;
                        handled = true;
                        break;
                    case '}':
                        grIndex = 2;
                        handled = true;
                        break;
                    case '|':
                        grIndex = 3;
                        handled = true;
                        break;
                    case 'N': {
                        if (escIndex < length) {
                            size_t consumed = 0;
                            uint32_t cp = decodeFromGSet(gsets[2], data + escIndex, length - escIndex, true, consumed);
                            appendUtf8(result, cp);
                            escIndex += consumed;
                        }
                        handled = true;
                        break;
                    }
                    case 'O': {
                        if (escIndex < length) {
                            size_t consumed = 0;
                            uint32_t cp = decodeFromGSet(gsets[3], data + escIndex, length - escIndex, true, consumed);
                            appendUtf8(result, cp);
                            escIndex += consumed;
                        }
                        handled = true;
                        break;
                    }
                    default:
                        break;
                    }
                }

                if (handled) {
                    i = escIndex;
                    continue;
                }

                int target = -1;
                bool is96 = false;
                bool designationConsumed = false;
                std::string designation;

                switch (selector) {
                case '(':
                    target = 0;
                    is96 = false;
                    break;
                case ')':
                    target = 1;
                    is96 = false;
                    break;
                case '*':
                    target = 2;
                    is96 = false;
                    break;
                case '+':
                    target = 3;
                    is96 = false;
                    break;
                case '-':
                    target = 1;
                    is96 = true;
                    break;
                case '.':
                    target = 2;
                    is96 = true;
                    break;
                case '/':
                    target = 3;
                    is96 = true;
                    break;
                default:
                    if (selector >= 0x40 && selector <= 0x7E) {
                        target = 0;
                        designation.push_back(static_cast<char>(selector));
                        designationConsumed = true;
                    }
                    break;
                }

                if (target == -1) {
                    if (file) {
                        file->reportUnsupportedCharset(std::string("ESC ") + static_cast<char>(selector));
                    }
                    i = escIndex;
                    continue;
                }

                applyDesignation(target, is96, designationConsumed, designation);
                i = escIndex;
                continue;
            }

            if (byte == 0x0E) { // SO
                glIndex = 1;
                ++i;
                continue;
            }
            if (byte == 0x0F) { // SI
                glIndex = 0;
                ++i;
                continue;
            }

            if (byte < 0x20) {
                emitInvalidControl(byte);
                ++i;
                continue;
            }

            if (byte >= 0x80 && byte < 0xA0) {
                if (byte == 0x8E) { // SS2
                    ++i;
                    if (i >= length) {
                        break;
                    }
                    size_t consumed = 0;
                    uint32_t cp = decodeFromGSet(gsets[2], data + i, length - i, true, consumed);
                    appendUtf8(result, cp);
                    i += consumed;
                    continue;
                }
                if (byte == 0x8F) { // SS3
                    ++i;
                    if (i >= length) {
                        break;
                    }
                    size_t consumed = 0;
                    uint32_t cp = decodeFromGSet(gsets[3], data + i, length - i, true, consumed);
                    appendUtf8(result, cp);
                    i += consumed;
                    continue;
                }

                emitInvalidControl(byte);
                ++i;
                continue;
            }

            bool isGL = byte < 0x80;
            const GSet& activeSet = isGL ? gsets[glIndex] : gsets[grIndex];
            size_t consumed = 0;
            uint32_t codepoint = decodeFromGSet(activeSet, data + i, length - i, isGL, consumed);
            if (consumed == 0) {
                ++i;
                continue;
            }
            appendUtf8(result, codepoint);
            i += consumed;
        }
    }

    return result;
}

int DefaultBinaryReader::getStringCount() {
    int length = readUInt8();

    if (length == 255) {
        size_t totalLength = 0;
        bool hasMore = true;

        while (hasMore) {
            int partition = readUInt16();
            hasMore = (partition & 0x8000) != 0;
            totalLength += static_cast<size_t>(partition & 0x7FFF);

            // SECURITY: Validate accumulated string length to prevent DoS
            if (totalLength > security::MAX_STRING_LENGTH) {
                throw std::runtime_error(
                    "CGM string length exceeds security limit: " +
                    std::to_string(totalLength) + " bytes"
                );
            }
        }

        length = static_cast<int>(totalLength);
    }

    return length;
}

// ============================================================================
// Real number reading
// ============================================================================

double DefaultBinaryReader::readReal() {
    Precision precision = cgm_->realPrecision();

    switch (precision) {
        case Precision::Fixed_32:    return readFixedPoint32();
        case Precision::Fixed_64:    return readFixedPoint64();
        case Precision::Floating_32: return readFloatingPoint32();
        case Precision::Floating_64: return readFloatingPoint64();
        default:
            unsupported("Unsupported real precision");
            return readFixedPoint32();
    }
}

double DefaultBinaryReader::readFixedPoint32() {
    int wholePart = readSignedInt16();
    int fractionPart = readUInt16();
    return wholePart + (fractionPart / TWO_EX_16);
}

double DefaultBinaryReader::readFixedPoint64() {
    int wholePart = readSignedInt32();
    unsigned int fractionPart = static_cast<unsigned int>(readUInt32());
    return wholePart + (static_cast<double>(fractionPart) / TWO_EX_32);
}

double DefaultBinaryReader::readFloatingPoint32() {
    skipBits();

    // Read 4 bytes (CGM is big-endian on the wire)
    uint8_t be[4];
    for (int i = 0; i < 4; ++i) {
        be[i] = static_cast<uint8_t>(readChar());
    }

    // Map to host byte order before memcpy into float
    uint8_t host[4];
    // Detect host endianness at runtime
    const uint16_t one = 1;
    const bool little = (*reinterpret_cast<const uint8_t*>(&one) == 1);
    if (little) {
        host[0] = be[3]; host[1] = be[2]; host[2] = be[1]; host[3] = be[0];
    } else {
        host[0] = be[0]; host[1] = be[1]; host[2] = be[2]; host[3] = be[3];
    }

    float value = 0.0f;
    std::memcpy(&value, host, sizeof(host));
    return static_cast<double>(value);
}

double DefaultBinaryReader::readFloatingPoint64() {
    skipBits();

    // Read 8 bytes (CGM is big-endian on the wire)
    uint8_t be[8];
    for (int i = 0; i < 8; ++i) {
        be[i] = static_cast<uint8_t>(readChar());
    }

    // Map to host byte order before memcpy into double
    uint8_t host[8];
    const uint16_t one = 1;
    const bool little = (*reinterpret_cast<const uint8_t*>(&one) == 1);
    if (little) {
        for (int i = 0; i < 8; ++i) host[i] = be[7 - i];
    } else {
        for (int i = 0; i < 8; ++i) host[i] = be[i];
    }

    double value = 0.0;
    std::memcpy(&value, host, sizeof(host));
    return value;
}

// ============================================================================
// VDC and Point reading
// ============================================================================

double DefaultBinaryReader::readVdc() {
    if (cgm_->vdcType() == VDCType::Real) {
        Precision precision = cgm_->vdcRealPrecision();

        switch (precision) {
            case Precision::Fixed_32:    return readFixedPoint32();
            case Precision::Fixed_64:    return readFixedPoint64();
            case Precision::Floating_32: return readFloatingPoint32();
            case Precision::Floating_64: return readFloatingPoint64();
            default:
                unsupported("Unsupported VDC real precision");
                return readFixedPoint32();
        }
    } else {
        int precision = cgm_->vdcIntegerPrecision();

        switch (precision) {
            case 16: return readSignedInt16();
            case 24: return readSignedInt24();
            case 32: return readSignedInt32();
            default:
                unsupported("Unsupported VDC integer precision: " + std::to_string(precision));
                return readSignedInt16();
        }
    }
}

CGMPoint DefaultBinaryReader::readPoint() {
    double x = readVdc();
    double y = readVdc();

    // Validate coordinates - replace NaN/Inf with 0 for robustness
    if (!std::isfinite(x)) {
        x = 0.0;
    }
    if (!std::isfinite(y)) {
        y = 0.0;
    }

    return CGMPoint(x, y);
}

// ============================================================================
// Color reading
// ============================================================================

int DefaultBinaryReader::readColorIndex() {
    return readUInt(cgm_->colourIndexPrecision());
}

int DefaultBinaryReader::readColorIndex(int localColorPrecision) {
    int precision = localColorPrecision;

    if (precision <= 0) {
        precision = cgm_->colourIndexPrecision();
    }

    // Guard against malformed files that never advertised a precision.
    if (precision <= 0) {
        precision = 8;
    }

    return readUInt(precision);
}

Color DefaultBinaryReader::readDirectColor() {
    return readDirectColor(-1);
}

Color DefaultBinaryReader::readDirectColor(int localPrecision) {
    int precision = (localPrecision > 0) ? localPrecision : cgm_->colourPrecision();

    switch (cgm_->colorModel()) {
        case ColorModel::RGB: {
            int r = readUInt(precision);
            int g = readUInt(precision);
            int b = readUInt(precision);
            auto scaled = scaleColorValueRGB(r, g, b, precision);
            return Color::fromArgb(static_cast<uint8_t>(scaled[0]),
                                   static_cast<uint8_t>(scaled[1]),
                                   static_cast<uint8_t>(scaled[2]));
        }
        case ColorModel::CMYK: {
            int c = readUInt(precision);
            int m = readUInt(precision);
            int y = readUInt(precision);
            int k = readUInt(precision);

            double cNorm = normalizeUnsigned(c, precision);
            double mNorm = normalizeUnsigned(m, precision);
            double yNorm = normalizeUnsigned(y, precision);
            double kNorm = normalizeUnsigned(k, precision);
            return cmykToColor(cNorm, mNorm, yNorm, kNorm);
        }
        case ColorModel::CIELAB: {
            int lRaw = readUInt(precision);
            int aRaw = readInt(precision);
            int bRaw = readInt(precision);

            double L = normalizeUnsigned(lRaw, precision) * 100.0;
            double a = normalizeSigned(aRaw, precision) * LAB_MAX_AB;
            double b = normalizeSigned(bRaw, precision) * LAB_MAX_AB;
            return labToColor(L, a, b);
        }
        case ColorModel::CIELUV: {
            int lRaw = readUInt(precision);
            int uRaw = readInt(precision);
            int vRaw = readInt(precision);

            double L = normalizeUnsigned(lRaw, precision) * 100.0;
            double u = normalizeSigned(uRaw, precision) * LUV_MAX_U;
            double v = normalizeSigned(vRaw, precision) * LUV_MAX_V;
            return luvToColor(L, u, v);
        }
        case ColorModel::RGB_RELATED:
        default: {
            // Consume components but report unsupported conversion
            int r = readUInt(precision);
            int g = readUInt(precision);
            int b = readUInt(precision);
            auto scaled = scaleColorValueRGB(r, g, b, precision);
            unsupported("RGB_RELATED colour model treated as RGB per Annex G");
            return Color::fromArgb(static_cast<uint8_t>(scaled[0]),
                                   static_cast<uint8_t>(scaled[1]),
                                   static_cast<uint8_t>(scaled[2]));
        }
    }
}

CGMColor DefaultBinaryReader::readColor() {
    CGMColor result;

    // Check color selection mode to determine if color is indexed or direct RGB
    if (cgm_->colorSelectionMode() == ColorSelectionMode::DIRECT) {
        result.set_color(readDirectColor());
    } else {
        result.set_colorIndex(readColorIndex());
    }

    return result;
}

CGMColor DefaultBinaryReader::readColor(int localColorPrecision) {
    CGMColor result;

    if (cgm_->colorSelectionMode() == ColorSelectionMode::DIRECT) {
        result.set_color(readDirectColor(localColorPrecision));
    } else {
        result.set_colorIndex(readColorIndex(localColorPrecision));
    }

    return result;
}

std::array<int, 3> DefaultBinaryReader::scaleColorValueRGB(int r, int g, int b, int precisionBits) {
    auto computeMaxValue = [](int bits) -> int {
        if (bits <= 0) {
            return 255;
        }
        if (bits >= static_cast<int>(std::numeric_limits<int>::digits)) {
            return std::numeric_limits<int>::max();
        }
        return static_cast<int>((1u << bits) - 1u);
    };

    const int maxValue = computeMaxValue(precisionBits);

    auto convertComponent = [maxValue](int value) -> int {
        int clamped = std::clamp(value, 0, maxValue);
        if (maxValue == 0) {
            return 0;
        }
        if (maxValue == 255) {
            return clamped;
        }
        long long scaled = static_cast<long long>(clamped) * 255 + maxValue / 2;
        return static_cast<int>(scaled / maxValue);
    };

    return {convertComponent(r), convertComponent(g), convertComponent(b)};
}

// ============================================================================
// Helper methods
// ============================================================================

double DefaultBinaryReader::readSizeSpecification(SpecificationMode mode) {
    if (mode == SpecificationMode::ABS) {
        return readVdc();
    } else {
        return readReal();
    }
}

void DefaultBinaryReader::skipBits() {
    if (positionInCurrentArgument_ % 8 != 0) {
        positionInCurrentArgument_ = 0;
        currentArg_++;
    }
}

int DefaultBinaryReader::sizeOfInt() {
    return cgm_->integerPrecision() / 8;
}

int DefaultBinaryReader::sizeOfPoint() {
    return 2 * sizeOfVdc();
}

bool DefaultBinaryReader::hasMoreData() const {
    return currentArg_ < static_cast<int>(arguments_.size());
}

bool DefaultBinaryReader::getFinalFlag() {
    return finalFlag_;
}

void DefaultBinaryReader::alignOnWord() {
    if (!arguments_.empty()) {
        if (positionInCurrentArgument_ != 0) {
            positionInCurrentArgument_ = 0;
            currentArg_ = std::min(currentArg_ + 1, static_cast<int>(arguments_.size()));
        }

        if ((currentArg_ & 1) != 0 && currentArg_ < static_cast<int>(arguments_.size())) {
            ++currentArg_;
        }
    }
}

size_t DefaultBinaryReader::getRemainingByteCount() const {
    if (currentArg_ >= static_cast<int>(arguments_.size())) {
        return 0;
    }
    return arguments_.size() - static_cast<size_t>(currentArg_);
}

std::vector<uint8_t> DefaultBinaryReader::getRemainingBytes() {
    std::vector<uint8_t> result;
    size_t remaining = getRemainingByteCount();
    if (remaining > 0) {
        result.reserve(remaining);
        for (size_t i = 0; i < remaining; i++) {
            result.push_back(arguments_[currentArg_ + i]);
        }
        currentArg_ = static_cast<int>(arguments_.size());
    }
    return result;
}

void DefaultBinaryReader::skipBytes(size_t count) {
    currentArg_ = std::min(currentArg_ + static_cast<int>(count), static_cast<int>(arguments_.size()));
    positionInCurrentArgument_ = 0;
}

int DefaultBinaryReader::sizeOfVdc() {
    if (cgm_->vdcType() == VDCType::Integer) {
        return cgm_->vdcIntegerPrecision() / 8;
    } else {
        Precision precision = cgm_->vdcRealPrecision();
        switch (precision) {
            case Precision::Fixed_32:    return 4;
            case Precision::Fixed_64:    return 8;
            case Precision::Floating_32: return 4;
            case Precision::Floating_64: return 8;
            default: return 1;
        }
    }
}

int DefaultBinaryReader::readArgumentEnd() {
    currentArg_ = static_cast<int>(arguments_.size());
    return currentArg_;
}

void DefaultBinaryReader::ensureAllArgumentsWereRead() {
    bool allRead = (currentArg_ == static_cast<int>(arguments_.size())) ||
                   (currentArg_ == 0 && positionInCurrentArgument_ > 0);

    if (!allRead) {
        // Log warning but don't throw
        unsupported("Not all arguments were read");
    }
}

void DefaultBinaryReader::unsupported(const std::string& message) {
    if (currentCommand_) {
        messages_.emplace_back(
            Severity::Unsupported,
            currentCommand_->elementClass(),
            currentCommand_->elementId(),
            message,
            currentCommand_->toString()
        );
    }
}

std::string DefaultBinaryReader::getErrorMessage(const std::string& callingMethod) {
    std::string cmdName = currentCommand_ ? currentCommand_->toString() : "unknown";
    return "Error in " + callingMethod + " for command " + cmdName +
           " in file '" + cgm_->name() + "'";
}

} // namespace opencgm
