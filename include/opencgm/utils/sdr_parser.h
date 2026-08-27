#ifndef OPENCGM_UTILS_SDR_PARSER_H
#define OPENCGM_UTILS_SDR_PARSER_H

#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <cstdint>

namespace opencgm {

/**
 * Parser for CGM Structured Data Records (SDR) - ISO/IEC 8632-3:1999
 *
 * SDRs are self-defining binary structures used in WebCGM for APS region
 * coordinates and other complex data. This parser handles:
 * - SF (Structured Field) type codes
 * - IEEE 754 32-bit floating point coordinate data
 * - String length encoding per ISO 8632-3 NOTE 6
 */
class SDRParser {
public:
    /**
     * Parse region coordinates from binary SDR data.
     *
     * Expects SDR containing SF with coordinate pairs encoded as IEEE 754
     * 32-bit floats in big-endian format (CGM binary encoding standard).
     *
     * @param binaryData Raw binary SDR string from ApplicationStructureAttribute
     * @return Vector of (x, y) coordinate pairs in VDC space
     */
    static std::vector<std::pair<double, double>> parseRegionCoordinates(const std::string& binaryData);

    struct ViewContextRect {
        double minX = 0.0;
        double minY = 0.0;
        double maxX = 0.0;
        double maxY = 0.0;
    };

    /**
     * Parse a viewcontext attribute SDR consisting of four IEEE 754 floats
     * encoded in big-endian order (minX, minY, maxX, maxY).
     *
     * @param binaryData Raw binary SDR string from ApplicationStructureAttribute
     * @return Rectangle coordinates if parsing succeeds
     */
    static std::optional<ViewContextRect> parseViewContextRect(const std::string& binaryData);

    /**
     * Decode a structured text record that may contain UTF-16 big or little endian data
     * with optional CGM structured-field headers.
     *
     * @param data Raw attribute data bytes
     * @return UTF-8 string if decoding succeeds, std::nullopt otherwise
     */
    static std::optional<std::string> decodeStructuredText(const std::string& data);

    /**
     * Check if data appears to be binary SDR format.
     *
     * @param data String that may contain binary SDR data
     * @return true if data contains binary markers (SF type codes, IEEE 754 floats)
     */
    static bool isBinarySDR(const std::string& data);

    /**
     * Decode linkuri structured field payload (SF type 0x000E) into component strings.
     * Components follow WebCGM ordering (URI, behavior, target, content, highlight...).
     *
     * @param data Raw attribute data bytes
     * @return Components parsed in order; may be empty if decoding fails
     */
    static std::vector<std::string> decodeLinkuriStructuredData(const std::string& data);

    struct TileSdrInfo {
        int width = 0;
        int height = 0;
        int localColorPrecision = 0; // 0 if unspecified
        int rowPadding = -1;         // -1 if unspecified
        bool ok = false;
    };

    /**
     * Best-effort parse of Tile/BitonalTile SDR to extract width/height and
     * optional local color precision and row padding indicators (ISO/IEC 8632-3).
     * If the SDR is not recognized, returns ok=false and leaves fields at defaults.
     */
    static TileSdrInfo parseTileSdr(const std::string& data);

private:
    /**
     * Read IEEE 754 32-bit float from 4 bytes (big-endian).
     *
     * @param data Binary data
     * @param offset Starting position
     * @return Decoded float value
     */
    static float readIEEE754Float(const std::string& data, size_t offset);

    /**
     * Read 16-bit unsigned integer (big-endian).
     *
     * @param data Binary data
     * @param offset Starting position
     * @return Decoded integer value
     */
    static uint16_t readUInt16(const std::string& data, size_t offset);

    /**
     * Read 8-bit unsigned integer.
     *
     * @param data Binary data
     * @param offset Starting position
     * @return Decoded byte value
     */
    static uint8_t readUInt8(const std::string& data, size_t offset);

    /**
     * Read string length per ISO 8632-3 NOTE 6.
     *
     * If first octet is 0-254: that's the length
     * If first octet is 255: next 16 bits contain length (with continuation flag)
     *
     * @param data Binary data
     * @param offset Starting position (updated to position after length field)
     * @return String length in bytes
     */
    static size_t readStringLength(const std::string& data, size_t& offset);
};

} // namespace opencgm

#endif // OPENCGM_UTILS_SDR_PARSER_H
