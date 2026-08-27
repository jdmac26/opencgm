#include "opencgm/utils/sdr_parser.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <optional>
#include <cstdint>

namespace {

void appendUtf8(std::string &out, uint32_t cp)
{
    if (cp <= 0x7F)
    {
        out.push_back(static_cast<char>(cp));
    }
    else if (cp <= 0x7FF)
    {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    else if (cp <= 0xFFFF)
    {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    else
    {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

std::string decodeUtf16Le(const unsigned char *data, size_t length)
{
    std::string result;
    for (size_t i = 0; i + 1 < length; i += 2)
    {
        uint16_t unit = static_cast<uint16_t>(data[i] | (data[i + 1] << 8));
        if (unit == 0)
        {
            continue;
        }
        if (unit >= 0xD800 && unit <= 0xDBFF)
        {
            if (i + 3 >= length)
            {
                break;
            }
            uint16_t nextUnit = static_cast<uint16_t>(data[i + 2] | (data[i + 3] << 8));
            if (nextUnit >= 0xDC00 && nextUnit <= 0xDFFF)
            {
                uint32_t cp = 0x10000u + (((unit - 0xD800u) << 10u) | (nextUnit - 0xDC00u));
                appendUtf8(result, cp);
                i += 2;
                continue;
            }
            continue;
        }
        if (unit >= 0xDC00 && unit <= 0xDFFF)
        {
            continue;
        }
        appendUtf8(result, static_cast<uint32_t>(unit));
    }
    return result;
}

std::string decodeUtf16Be(const unsigned char *data, size_t length)
{
    std::string result;
    for (size_t i = 0; i + 1 < length; i += 2)
    {
        uint16_t unit = static_cast<uint16_t>((data[i] << 8) | data[i + 1]);
        if (unit == 0)
        {
            continue;
        }
        if (unit >= 0xD800 && unit <= 0xDBFF)
        {
            if (i + 3 >= length)
            {
                break;
            }
            uint16_t nextUnit = static_cast<uint16_t>((data[i + 2] << 8) | data[i + 3]);
            if (nextUnit >= 0xDC00 && nextUnit <= 0xDFFF)
            {
                uint32_t cp = 0x10000u + (((unit - 0xD800u) << 10u) | (nextUnit - 0xDC00u));
                appendUtf8(result, cp);
                i += 2;
                continue;
            }
            continue;
        }
        if (unit >= 0xDC00 && unit <= 0xDFFF)
        {
            continue;
        }
        appendUtf8(result, static_cast<uint32_t>(unit));
    }
    return result;
}

} // namespace

namespace opencgm {

// IEEE 754 32-bit float: 1 sign bit, 8 exponent bits, 23 mantissa bits
float SDRParser::readIEEE754Float(const std::string& data, size_t offset) {
    if (offset + 4 > data.size()) {
        throw std::runtime_error("SDRParser: insufficient data for IEEE 754 float");
    }

    // Read 4 bytes in big-endian order (CGM binary encoding standard)
    uint8_t be[4];
    be[0] = static_cast<uint8_t>(data[offset + 0]);
    be[1] = static_cast<uint8_t>(data[offset + 1]);
    be[2] = static_cast<uint8_t>(data[offset + 2]);
    be[3] = static_cast<uint8_t>(data[offset + 3]);

    // Convert to host order
    uint8_t host[4];
    const uint16_t one = 1;
    const bool little = (*reinterpret_cast<const uint8_t*>(&one) == 1);
    if (little) {
        host[0] = be[3]; host[1] = be[2]; host[2] = be[1]; host[3] = be[0];
    } else {
        host[0] = be[0]; host[1] = be[1]; host[2] = be[2]; host[3] = be[3];
    }

    float result = 0.0f;
    std::memcpy(&result, host, sizeof(host));
    return result;
}

uint16_t SDRParser::readUInt16(const std::string& data, size_t offset) {
    if (offset + 2 > data.size()) {
        throw std::runtime_error("SDRParser: insufficient data for uint16");
    }

    // Big-endian
    uint16_t value = 0;
    value |= (static_cast<uint8_t>(data[offset + 0]) << 8);
    value |= (static_cast<uint8_t>(data[offset + 1]) << 0);
    return value;
}

uint8_t SDRParser::readUInt8(const std::string& data, size_t offset) {
    if (offset >= data.size()) {
        throw std::runtime_error("SDRParser: insufficient data for uint8");
    }
    return static_cast<uint8_t>(data[offset]);
}

size_t SDRParser::readStringLength(const std::string& data, size_t& offset) {
    if (offset >= data.size()) {
        throw std::runtime_error("SDRParser: insufficient data for string length");
    }

    uint8_t firstByte = readUInt8(data, offset);
    offset++;

    // Per ISO 8632-3 NOTE 6:
    // - If first octet is 0-254: that's the complete string length
    // - If first octet is 255: next 16 bits contain length + continuation flag
    if (firstByte < 255) {
        return firstByte;
    } else {
        // Long form: 16-bit length with continuation flag
        if (offset + 2 > data.size()) {
            throw std::runtime_error("SDRParser: insufficient data for long string length");
        }
        uint16_t lengthField = readUInt16(data, offset);
        offset += 2;

        // First bit is continuation flag, next 15 bits are length (0-32767)
        // For now, we don't handle multi-part strings (continuation)
        size_t length = lengthField & 0x7FFF;  // Mask out continuation bit
        return length;
    }
}

bool SDRParser::isBinarySDR(const std::string& data) {
    if (data.empty()) {
        return false;
    }

    // Check for binary markers:
    // 1. High-bit bytes that suggest binary encoding (not printable text)
    // 2. Common SF type codes in the range 0x00-0x1F
    for (unsigned char c : data) {
        // Check for control characters (except tab, newline, carriage return)
        if (c < 0x20 && c != 0x09 && c != 0x0A && c != 0x0D) {
            return true;
        }
        // Check for high-bit bytes (> 127) that suggest binary data
        if (c > 0x7F) {
            return true;
        }
    }

    return false;
}

SDRParser::TileSdrInfo SDRParser::parseTileSdr(const std::string& data) {
    TileSdrInfo info;
    if (data.empty()) return info;

    // Try interpreting SDR as a sequence of Structured Fields (SF):
    // [u16 length][length bytes of payload] ... (ISO/IEC 8632-3)
    const size_t n = data.size();
    size_t pos = 0;
    bool sawField = false;
    while (pos + 2 <= n) {
        uint16_t len = readUInt16(data, pos);
        if (len == 0 || pos + 2 + len > n) break;
        size_t s = pos + 2;
        size_t e = s + len;
        sawField = true;

        // Within this field, try to parse simple TLVs (1b tag + 1b len [+ value])
        // Recognized tags: 1/ 'W' width, 2/'H' height, 3/'C' local precision, 4/'R' row padding
        size_t p = s;
        while (p + 2 <= e) {
            uint8_t tag = static_cast<uint8_t>(data[p++]);
            uint8_t len1 = static_cast<uint8_t>(data[p++]);
            if (len1 == 0 || p + len1 > e) break;
            auto asU16 = [&](size_t off){ return static_cast<int>(readUInt16(data, off)); };
            auto asU8 = [&](size_t off){ return static_cast<int>(static_cast<uint8_t>(data[off])); };
            auto asU32 = [&](size_t off){ if (off+4>e) return 0; uint32_t v= (static_cast<uint8_t>(data[off])<<24) | (static_cast<uint8_t>(data[off+1])<<16) | (static_cast<uint8_t>(data[off+2])<<8) | static_cast<uint8_t>(data[off+3]); return static_cast<int>(v); };
            bool consumed = false;
            switch (tag) {
                case 1: case 'W':
                    if (len1 == 2) { info.width = asU16(p); info.ok = info.ok || (info.width>0); consumed = true; }
                    else if (len1 == 4) { int v = asU32(p); if (v>0 && v<65536) { info.width = v; info.ok = true; } consumed = true; }
                    break;
                case 2: case 'H':
                    if (len1 == 2) { info.height = asU16(p); info.ok = info.ok || (info.height>0); consumed = true; }
                    else if (len1 == 4) { int v = asU32(p); if (v>0 && v<65536) { info.height = v; info.ok = true; } consumed = true; }
                    break;
                case 3: case 'C':
                    if (len1 == 1) { int v = asU8(p); if (v>=1 && v<=32) info.localColorPrecision = v; consumed = true; }
                    else if (len1 == 2) { int v = asU16(p); if (v>=1 && v<=32) info.localColorPrecision = v; consumed = true; }
                    break;
                case 4: case 'R':
                    if (len1 == 1) { int v = asU8(p); if (v>=0 && v<=3) info.rowPadding = v; consumed = true; }
                    else if (len1 == 2) { int v = asU16(p); if (v>=0 && v<=3) info.rowPadding = v; consumed = true; }
                    break;
                default:
                    break;
            }
            if (!consumed) {
                // Skip unknown tag
            }
            p += len1;
        }

        // If TLV parse did not fully resolve, fall back to scanning 16-bit pairs
        for (size_t off = s; off + 3 < e; off += 2) {
            uint16_t a = readUInt16(data, off);
            uint16_t b = readUInt16(data, off + 2);
            if (a > 0 && b > 0 && a < 65535 && b < 65535) {
                if (a == 1 && b == 1) continue;
                if (!info.ok) {
                    info.width = static_cast<int>(a);
                    info.height = static_cast<int>(b);
                    info.ok = true;
                }
                // Peek for small precision or row padding nearby
                if (off + 4 < e) {
                    uint8_t c = static_cast<uint8_t>(data[off + 4]);
                    if (info.localColorPrecision == 0 && c >= 1 && c <= 32) info.localColorPrecision = static_cast<int>(c);
                    if (info.rowPadding < 0) {
                        uint8_t d = (off + 5 < e) ? static_cast<uint8_t>(data[off + 5]) : 255;
                        if (d <= 3) info.rowPadding = static_cast<int>(d);
                    }
                }
                break;
            }
        }
        pos = e;
        if (info.ok && info.localColorPrecision > 0 && info.rowPadding >= 0) break;
    }

    if (!sawField) {
        // Heuristic fallback: scan early bytes if not in SF format
        for (size_t i = 0; i + 3 < n && i < 32; i += 2) {
            uint16_t a = readUInt16(data, i);
            uint16_t b = readUInt16(data, i + 2);
            if (a > 0 && b > 0 && a < 65535 && b < 65535) {
                if (a == 1 && b == 1) continue;
                info.width = static_cast<int>(a);
                info.height = static_cast<int>(b);
                info.ok = true;
                break;
            }
        }
    }

    // Final small-value sweep for precision/padding if still unset
    for (size_t i = 0; i < n; ++i) {
        uint8_t v = static_cast<uint8_t>(data[i]);
        if (info.localColorPrecision == 0 && v >= 1 && v <= 32) {
            info.localColorPrecision = static_cast<int>(v);
        }
        if (info.rowPadding < 0 && v <= 3) {
            info.rowPadding = static_cast<int>(v);
        }
        if (info.ok && info.localColorPrecision > 0 && info.rowPadding >= 0) break;
    }

    return info;
}

std::vector<std::pair<double, double>> SDRParser::parseRegionCoordinates(const std::string& binaryData) {
    std::vector<std::pair<double, double>> coordinates;

    if (binaryData.empty()) {
        return coordinates;
    }

    try {
        size_t offset = 0;

        // SDR format per ISO 8632-3 NOTE 17:
        // Based on actual WebCGM region data analysis:
        // Hex: 00 0b 00 01 00 03 00 10 00 0a 43 10 5d 2d 42 d9 ba 53...
        //      ^^^^^       ^^^^^       ^^^^^ ^^^^^^^^^^^ ^^^^^^^^^^^
        //      SF len      SF type     count   Float 1     Float 2
        //
        // Structure:
        // - 2 bytes: SF length (0x000b = 11 bytes)
        // - 2 bytes: SF type code (0x0001)
        // - 2 bytes: Another type code (0x0003)
        // - 2 bytes: Format indicator (0x0010)
        // - 2 bytes: Coordinate pair count (0x000a = 10 pairs)
        // - N*8 bytes: IEEE 754 floats (x1,y1, x2,y2, ...)

        // Skip outer SDR length if present (check if first byte is 0x00)
        if (offset < binaryData.size() && readUInt8(binaryData, offset) == 0x00) {
            // Likely has SF structure with length prefix
            offset += 2;  // Skip 2-byte SF length

            // Skip type codes (typically 4-6 bytes of metadata)
            if (offset + 6 <= binaryData.size()) {
                offset += 6;  // Skip SF type (2 bytes), type code (2 bytes), format (2 bytes)
            }

            // Read coordinate pair count (2 bytes)
            if (offset + 2 <= binaryData.size()) {
                uint16_t pairCount = readUInt16(binaryData, offset);
                offset += 2;

                // Sanity check: reasonable pair count (1-1000)
                if (pairCount > 0 && pairCount < 1000) {
                    // Read coordinate pairs
                    for (uint16_t i = 0; i < pairCount && offset + 8 <= binaryData.size(); i++) {
                        float x = readIEEE754Float(binaryData, offset);
                        offset += 4;
                        float y = readIEEE754Float(binaryData, offset);
                        offset += 4;

                        // VDC coordinates should be finite and reasonable
                        if (std::isfinite(x) && std::isfinite(y)) {
                            coordinates.push_back({static_cast<double>(x), static_cast<double>(y)});
                        }
                    }

                    if (!coordinates.empty()) {
                        return coordinates;
                    }
                }
            }
        }

        // Fallback: Scan for sequences of IEEE 754 floats
        // This handles non-standard or simple formats
        offset = 0;
        size_t consecutiveValidPairs = 0;
        std::vector<std::pair<double, double>> candidateCoords;

        while (offset + 8 <= binaryData.size()) {
            try {
                float x = readIEEE754Float(binaryData, offset);
                float y = readIEEE754Float(binaryData, offset + 4);

                // Check if these look like valid VDC coordinates
                if (std::isfinite(x) && std::isfinite(y) &&
                    x >= -1e6 && x <= 1e6 && y >= -1e6 && y <= 1e6) {
                    candidateCoords.push_back({static_cast<double>(x), static_cast<double>(y)});
                    consecutiveValidPairs++;
                    offset += 8;

                    // If we have at least 3 consecutive valid pairs, likely correct
                    if (consecutiveValidPairs >= 3) {
                        coordinates = candidateCoords;
                    }
                } else {
                    // Reset on invalid pair
                    if (consecutiveValidPairs >= 3) {
                        break;  // Keep what we have
                    }
                    candidateCoords.clear();
                    consecutiveValidPairs = 0;
                    offset++;
                }
            } catch (...) {
                if (consecutiveValidPairs >= 3) {
                    break;
                }
                candidateCoords.clear();
                consecutiveValidPairs = 0;
                offset++;
            }
        }

    } catch (const std::exception&) {
        // If parsing fails, return empty vector
        coordinates.clear();
    }

    return coordinates;
}

std::optional<SDRParser::ViewContextRect> SDRParser::parseViewContextRect(const std::string& binaryData) {
    if (binaryData.size() < 12) {
        return std::nullopt;
    }

    try {
        // Viewcontext SDRs observed in WebCGM encode as:
        // [u16 format=0x0010][u16 count][count * float32]
        uint16_t format = readUInt16(binaryData, 0);
        uint16_t count = readUInt16(binaryData, 2);
        if (format != 0x0010 || count < 4) {
            return std::nullopt;
        }

        size_t expectedSize = 4 + static_cast<size_t>(count) * 4;
        if (binaryData.size() < expectedSize) {
            count = static_cast<uint16_t>((binaryData.size() > 4 ? (binaryData.size() - 4) / 4 : 0));
        }

        size_t offset = 4;
        std::vector<double> values;
        values.reserve(count);

        for (uint16_t i = 0; i < count && offset + 4 <= binaryData.size(); ++i) {
            float v = readIEEE754Float(binaryData, offset);
            offset += 4;
            if (std::isfinite(v)) {
                values.push_back(static_cast<double>(v));
            } else {
                values.push_back(0.0);
            }
        }

        if (values.size() < 4) {
            return std::nullopt;
        }

        ViewContextRect rect;
        rect.minX = values[0];
        rect.minY = values[1];
        rect.maxX = values[2];
        rect.maxY = values[3];
        return rect;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> SDRParser::decodeStructuredText(const std::string& data) {
    if (data.empty()) {
        return std::nullopt;
    }

    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(data.data());
    size_t length = data.size();

    if (length >= 5 && bytes[0] == 0x00 && bytes[2] == 0x00 && bytes[1] < 0x20 && bytes[3] < 0x20) {
        uint8_t declaredLen = bytes[4];
        size_t available = (length > 5) ? (length - 5) : 0;
        size_t useLen = std::min<size_t>(declaredLen, available);
        return decodeUtf16Be(bytes + 5, useLen);
    }

    const unsigned char* payload = bytes;
    size_t payloadLength = length;

    if (payloadLength < 2) {
        return std::nullopt;
    }

    if (payloadLength >= 2 && payload[0] == 0xFF && payload[1] == 0xFE) {
        return decodeUtf16Le(payload + 2, payloadLength - 2);
    }
    if (payloadLength >= 2 && payload[0] == 0xFE && payload[1] == 0xFF) {
        return decodeUtf16Be(payload + 2, payloadLength - 2);
    }

    auto looksLikeBomlessUtf16 = [](const unsigned char* buffer, size_t length, bool bigEndian) -> bool {
        if (length < 4 || (length % 2) != 0) {
            return false;
        }

        size_t printablePairs = 0;
        size_t controlPairs = 0;

        for (size_t i = 0; i + 1 < length; i += 2) {
            unsigned char high = bigEndian ? buffer[i] : buffer[i + 1];
            unsigned char low = bigEndian ? buffer[i + 1] : buffer[i];

            if (high != 0x00) {
                return false;
            }

            if (low == 0x00) {
                continue;
            }

            if (low < 0x20 && low != 0x09 && low != 0x0A && low != 0x0D) {
                controlPairs++;
            } else {
                printablePairs++;
            }
        }

        if (printablePairs == 0) {
            return false;
        }

        return controlPairs * 4 <= printablePairs;
    };

    if (looksLikeBomlessUtf16(payload, payloadLength - (payloadLength % 2), false)) {
        size_t evenLength = payloadLength - (payloadLength % 2);
        std::string text = decodeUtf16Le(payload, evenLength);
        if (payloadLength % 2 == 1) {
            appendUtf8(text, payload[payloadLength - 1]);
        }
        return text;
    }

    if (looksLikeBomlessUtf16(payload, payloadLength - (payloadLength % 2), true)) {
        size_t evenLength = payloadLength - (payloadLength % 2);
        std::string text = decodeUtf16Be(payload, evenLength);
        if (payloadLength % 2 == 1) {
            appendUtf8(text, payload[payloadLength - 1]);
        }
        return text;
    }

    return std::nullopt;
}

std::vector<std::string> SDRParser::decodeLinkuriStructuredData(const std::string &data)
{
    std::vector<std::string> components;
    if (data.empty())
    {
        return components;
    }

    const size_t totalSize = data.size();
    size_t offset = 0;

    auto alignEven = [&](size_t value) -> size_t {
        return (value % 2 == 0) ? value : value + 1;
    };

    while (offset < totalSize)
    {
        size_t fieldCursor = offset;
        size_t fieldLength = 0;

        try
        {
            fieldLength = readStringLength(data, fieldCursor);
        }
        catch (const std::exception &)
        {
            break;
        }

        if (fieldLength == 0)
        {
            offset = alignEven(fieldCursor);
            continue;
        }

        if (fieldCursor + fieldLength > totalSize)
        {
            break;
        }

        size_t fieldEnd = fieldCursor + fieldLength;
        if (fieldEnd - fieldCursor < 4)
        {
            offset = alignEven(fieldEnd);
            continue;
        }

        uint16_t fieldType = readUInt16(data, fieldCursor);
        fieldCursor += 2;
        uint16_t componentCount = readUInt16(data, fieldCursor);
        fieldCursor += 2;

        if (fieldType == 0x000E)
        {
            for (uint16_t i = 0; i < componentCount && fieldCursor <= fieldEnd; ++i)
            {
                size_t stringLength = 0;
                try
                {
                    stringLength = readStringLength(data, fieldCursor);
                }
                catch (const std::exception &)
                {
                    fieldCursor = fieldEnd;
                    break;
                }

                if (fieldCursor + stringLength > fieldEnd)
                {
                    stringLength = fieldEnd - fieldCursor;
                }

                std::string value(data.data() + fieldCursor, stringLength);
                fieldCursor += stringLength;
                components.push_back(std::move(value));
            }

            // Only interested in the first String Fixed field
            break;
        }

        offset = alignEven(fieldEnd);
    }

    return components;
}

} // namespace opencgm
