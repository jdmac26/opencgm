#ifndef OPENCGM_COLOR_H
#define OPENCGM_COLOR_H

#include <cstdint>
#include <string>

namespace opencgm {

/**
 * @brief Simple RGB color structure
 */
struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;

    Color() = default;
    Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}

    static Color fromArgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        return Color(r, g, b, a);
    }

    /**
     * @brief Create Color from packed ARGB32 integer
     * @param argb Packed color in ARGB32 format:
     *             bits [31:24] = Alpha (0xFF = opaque)
     *             bits [23:16] = Red
     *             bits [15:8]  = Green
     *             bits [7:0]   = Blue
     * @return Color with extracted components
     */
    static Color fromArgb(int argb) {
        return Color(
            static_cast<uint8_t>((argb >> 16) & 0xFF),  // Red:   bits [23:16]
            static_cast<uint8_t>((argb >> 8) & 0xFF),   // Green: bits [15:8]
            static_cast<uint8_t>(argb & 0xFF),          // Blue:  bits [7:0]
            static_cast<uint8_t>((argb >> 24) & 0xFF)   // Alpha: bits [31:24]
        );
    }

    /**
     * @brief Convert to packed ARGB32 integer
     * @return Packed color in ARGB32 format (same layout as fromArgb)
     */
    int toArgb() const {
        return (static_cast<int>(a) << 24) |  // Alpha: bits [31:24]
               (static_cast<int>(r) << 16) |  // Red:   bits [23:16]
               (static_cast<int>(g) << 8) |   // Green: bits [15:8]
               static_cast<int>(b);           // Blue:  bits [7:0]
    }

    bool isEmpty() const {
        return r == 0 && g == 0 && b == 0 && a == 0;
    }

    bool operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }

    bool operator!=(const Color& other) const {
        return !(*this == other);
    }

    // Common colors
    static Color Black() { return Color(0, 0, 0); }
    static Color White() { return Color(255, 255, 255); }
    static Color Red() { return Color(255, 0, 0); }
    static Color Green() { return Color(0, 255, 0); }
    static Color Blue() { return Color(0, 0, 255); }
    static Color Cyan() { return Color(0, 255, 255); }
    static Color Empty() { return Color(0, 0, 0, 0); }
};

/**
 * @brief Represents a color parameter type in CGM
 */
class CGMColor {
public:
    CGMColor() : colorIndex_(-1) {}
    CGMColor(const Color& color) : color_(color), colorIndex_(-1) {}
    CGMColor(int colorIndex) : colorIndex_(colorIndex) {}

    const Color& color() const { return color_; }
    void set_color(const Color& color) { color_ = color; }

    int colorIndex() const { return colorIndex_; }
    void set_colorIndex(int index) { colorIndex_ = index; }

    bool isIndexed() const { return colorIndex_ >= 0; }

    bool operator==(const CGMColor& other) const;
    bool operator!=(const CGMColor& other) const;

    std::string toString() const;

private:
    Color color_;
    int colorIndex_;
};

} // namespace opencgm

#endif // OPENCGM_COLOR_H