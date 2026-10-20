#ifndef OPENCGM_SVG_PNG_ENCODER_H
#define OPENCGM_SVG_PNG_ENCODER_H

#include <cstdint>
#include <vector>

namespace opencgm {
namespace svg {

// Encodes 8-bit RGBA pixels (row-major, width * height * 4 bytes) as a PNG in
// the smallest form that represents them exactly:
//   - up to 256 distinct colours: an indexed PNG at 1, 2, 4 or 8 bits per
//     pixel, with a tRNS chunk only when some colour is not fully opaque;
//   - otherwise truecolour, RGB when every pixel is opaque and RGBA if not,
//     with a per-row adaptive filter.
// CGM rasters are dominated by bitonal CCITT and small-palette tiles, which
// a 32-bit RGBA encoding inflates several times over.
// Returns an empty vector for empty or inconsistent input.
std::vector<uint8_t> encodePng(const std::vector<uint8_t>& rgba, uint32_t width, uint32_t height);

} // namespace svg
} // namespace opencgm

#endif // OPENCGM_SVG_PNG_ENCODER_H
