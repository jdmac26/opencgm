#include "opencgm/svg/aps_text_decoder.h"

#include "opencgm/utils/string_utils.h"

#include <cctype>
#include <cstdint>

namespace opencgm::svg
{
    namespace
    {
        void appendCodePointUtf8(
            std::string &output,
            uint32_t codePoint)
        {
            if (codePoint <= 0x7F)
            {
                output.push_back(static_cast<char>(codePoint));
            }
            else if (codePoint <= 0x7FF)
            {
                output.push_back(
                    static_cast<char>(
                        0xC0 | ((codePoint >> 6) & 0x1F)));
                output.push_back(
                    static_cast<char>(
                        0x80 | (codePoint & 0x3F)));
            }
            else if (codePoint <= 0xFFFF)
            {
                output.push_back(
                    static_cast<char>(
                        0xE0 | ((codePoint >> 12) & 0x0F)));
                output.push_back(
                    static_cast<char>(
                        0x80 | ((codePoint >> 6) & 0x3F)));
                output.push_back(
                    static_cast<char>(
                        0x80 | (codePoint & 0x3F)));
            }
            else
            {
                output.push_back(
                    static_cast<char>(
                        0xF0 | ((codePoint >> 18) & 0x07)));
                output.push_back(
                    static_cast<char>(
                        0x80 | ((codePoint >> 12) & 0x3F)));
                output.push_back(
                    static_cast<char>(
                        0x80 | ((codePoint >> 6) & 0x3F)));
                output.push_back(
                    static_cast<char>(
                        0x80 | (codePoint & 0x3F)));
            }
        }

        bool isValidUtf8(const std::string &value)
        {
            size_t offset = 0;
            while (offset < value.size())
            {
                const auto first =
                    static_cast<unsigned char>(value[offset]);
                if (first < 0x80)
                {
                    ++offset;
                    continue;
                }

                size_t length = 0;
                uint32_t codePoint = 0;
                uint32_t minimum = 0;
                if (first >= 0xC2 && first <= 0xDF)
                {
                    length = 2;
                    codePoint = first & 0x1F;
                    minimum = 0x80;
                }
                else if (first >= 0xE0 && first <= 0xEF)
                {
                    length = 3;
                    codePoint = first & 0x0F;
                    minimum = 0x800;
                }
                else if (first >= 0xF0 && first <= 0xF4)
                {
                    length = 4;
                    codePoint = first & 0x07;
                    minimum = 0x10000;
                }
                else
                {
                    return false;
                }

                if (offset + length > value.size())
                {
                    return false;
                }
                for (size_t index = 1; index < length; ++index)
                {
                    const auto continuation =
                        static_cast<unsigned char>(
                            value[offset + index]);
                    if ((continuation & 0xC0) != 0x80)
                    {
                        return false;
                    }
                    codePoint =
                        (codePoint << 6) | (continuation & 0x3F);
                }

                if (codePoint < minimum ||
                    codePoint > 0x10FFFF ||
                    (codePoint >= 0xD800 &&
                     codePoint <= 0xDFFF))
                {
                    return false;
                }
                offset += length;
            }
            return true;
        }

        bool looksLikeUtf16Be(
            const std::vector<unsigned char> &raw)
        {
            if (raw.size() < 4 || (raw.size() % 2) != 0)
            {
                return false;
            }
            if (raw[0] == 0xFE && raw[1] == 0xFF)
            {
                return true;
            }

            size_t zeroHighBytes = 0;
            size_t asciiPairs = 0;
            for (size_t index = 0;
                 index + 1 < raw.size();
                 index += 2)
            {
                if (raw[index] == 0)
                {
                    ++zeroHighBytes;
                    if (raw[index + 1] >= 0x20 &&
                        raw[index + 1] <= 0x7E)
                    {
                        ++asciiPairs;
                    }
                }
            }
            return asciiPairs > 0 &&
                   zeroHighBytes >= raw.size() / 4;
        }

        bool looksLikeUtf16Le(
            const std::vector<unsigned char> &raw)
        {
            if (raw.size() < 4 || (raw.size() % 2) != 0)
            {
                return false;
            }
            if (raw[0] == 0xFF && raw[1] == 0xFE)
            {
                return true;
            }

            size_t zeroHighBytes = 0;
            size_t asciiPairs = 0;
            for (size_t index = 0;
                 index + 1 < raw.size();
                 index += 2)
            {
                if (raw[index + 1] == 0)
                {
                    ++zeroHighBytes;
                    if (raw[index] >= 0x20 &&
                        raw[index] <= 0x7E)
                    {
                        ++asciiPairs;
                    }
                }
            }
            return asciiPairs > 0 &&
                   zeroHighBytes >= raw.size() / 4;
        }

        std::string decodeUtf16(
            const std::vector<unsigned char> &raw,
            bool bigEndian,
            size_t startIndex)
        {
            std::string result;
            for (size_t index = startIndex;
                 index + 1 < raw.size();
                 index += 2)
            {
                const uint16_t unit =
                    bigEndian
                        ? static_cast<uint16_t>(
                              (raw[index] << 8) |
                              raw[index + 1])
                        : static_cast<uint16_t>(
                              (raw[index + 1] << 8) |
                              raw[index]);

                if (unit == 0)
                {
                    continue;
                }
                if (unit >= 0xD800 && unit <= 0xDBFF)
                {
                    if (index + 3 >= raw.size())
                    {
                        break;
                    }
                    const uint16_t next =
                        bigEndian
                            ? static_cast<uint16_t>(
                                  (raw[index + 2] << 8) |
                                  raw[index + 3])
                            : static_cast<uint16_t>(
                                  (raw[index + 3] << 8) |
                                  raw[index + 2]);
                    if (next >= 0xDC00 && next <= 0xDFFF)
                    {
                        const uint32_t codePoint =
                            0x10000u +
                            (((unit - 0xD800u) << 10u) |
                             (next - 0xDC00u));
                        appendCodePointUtf8(result, codePoint);
                        index += 2;
                    }
                    continue;
                }
                if (unit >= 0xDC00 && unit <= 0xDFFF)
                {
                    continue;
                }
                appendCodePointUtf8(result, unit);
            }
            return result;
        }

        std::string decodeLatin1(
            const std::vector<unsigned char> &raw)
        {
            std::string result;
            result.reserve(raw.size());
            for (const unsigned char value : raw)
            {
                appendCodePointUtf8(result, value);
            }
            return result;
        }

        std::string decodeTokenBytes(
            const std::vector<unsigned char> &raw,
            bool assumedUtf8)
        {
            if (raw.empty())
            {
                return {};
            }

            if (assumedUtf8)
            {
                const std::string candidate(
                    raw.begin(),
                    raw.end());
                if (isValidUtf8(candidate))
                {
                    return candidate;
                }
            }

            if (raw.size() >= 2)
            {
                if (raw[0] == 0xFE && raw[1] == 0xFF)
                {
                    return decodeUtf16(raw, true, 2);
                }
                if (raw[0] == 0xFF && raw[1] == 0xFE)
                {
                    return decodeUtf16(raw, false, 2);
                }
            }
            if (looksLikeUtf16Be(raw))
            {
                return decodeUtf16(raw, true, 0);
            }
            if (looksLikeUtf16Le(raw))
            {
                return decodeUtf16(raw, false, 0);
            }
            return decodeLatin1(raw);
        }
    }

    bool ApsTextDecoder::isUsableToken(
        const std::string &value)
    {
        if (value.empty() ||
            value == "?" ||
            value.find("\xEF\xBF\xBD") != std::string::npos)
        {
            return false;
        }

        bool hasGraphicalCharacter = false;
        for (const unsigned char character : value)
        {
            if (character < 0x20 || character == 0x7F)
            {
                return false;
            }
            if (character >= 0xA0 ||
                std::isalnum(character) ||
                character == '#' ||
                character == '_' ||
                character == '-' ||
                character == '.' ||
                character == ':' ||
                character == '@' ||
                character == '/')
            {
                hasGraphicalCharacter = true;
            }
        }
        return hasGraphicalCharacter;
    }

    std::vector<std::string> ApsTextDecoder::decodeTokens(
        const std::string &raw)
    {
        std::string normalizedPayload;
        const std::string *payload = &raw;
        const std::vector<unsigned char> rawBytes(
            raw.begin(),
            raw.end());
        if (rawBytes.size() >= 2 &&
            rawBytes[0] == 0xFE &&
            rawBytes[1] == 0xFF)
        {
            normalizedPayload = "\x1B%G";
            normalizedPayload += decodeUtf16(rawBytes, true, 2);
            payload = &normalizedPayload;
        }
        else if (
            rawBytes.size() >= 2 &&
            rawBytes[0] == 0xFF &&
            rawBytes[1] == 0xFE)
        {
            normalizedPayload = "\x1B%G";
            normalizedPayload += decodeUtf16(rawBytes, false, 2);
            payload = &normalizedPayload;
        }
        else if (looksLikeUtf16Be(rawBytes))
        {
            normalizedPayload = "\x1B%G";
            normalizedPayload += decodeUtf16(rawBytes, true, 0);
            payload = &normalizedPayload;
        }
        else if (looksLikeUtf16Le(rawBytes))
        {
            normalizedPayload = "\x1B%G";
            normalizedPayload += decodeUtf16(rawBytes, false, 0);
            payload = &normalizedPayload;
        }

        std::vector<std::string> tokens;
        std::vector<unsigned char> current;
        bool utf8Mode = false;
        bool tokenUtf8Mode = false;

        const auto flush = [&]()
        {
            if (current.empty())
            {
                return;
            }
            const std::string decoded =
                decodeTokenBytes(current, tokenUtf8Mode);
            const std::string trimmed =
                utils::trimString(decoded);
            if (isUsableToken(trimmed))
            {
                tokens.push_back(trimmed);
            }
            current.clear();
            tokenUtf8Mode = utf8Mode;
        };

        size_t offset = 0;
        while (offset < payload->size())
        {
            const auto character =
                static_cast<unsigned char>((*payload)[offset]);

            if (character == 0x1B)
            {
                flush();
                std::string escapeSequence;
                ++offset;
                while (offset < payload->size())
                {
                    const auto next =
                        static_cast<unsigned char>((*payload)[offset]);
                    escapeSequence.push_back(
                        static_cast<char>(next));
                    ++offset;
                    if (next >= 0x30 && next <= 0x7E)
                    {
                        break;
                    }
                }

                if (escapeSequence == "%G" ||
                    escapeSequence == "%/G")
                {
                    utf8Mode = true;
                }
                else if (
                    escapeSequence == "%@" ||
                    escapeSequence == "%/@" ||
                    (escapeSequence.size() == 2 &&
                     (escapeSequence[0] == '(' ||
                      escapeSequence[0] == ')')))
                {
                    utf8Mode = false;
                }
                tokenUtf8Mode = utf8Mode;
                continue;
            }

            if (character == 0x0D || character == 0x0A)
            {
                flush();
                ++offset;
                continue;
            }
            if (!utf8Mode &&
                (character < 0x20 ||
                 character == 0x7F ||
                 (character >= 0x80 && character <= 0x9F) ||
                 character == 0xFF))
            {
                flush();
                ++offset;
                continue;
            }
            if (character == 0x20 || character == 0xA0)
            {
                if (!current.empty() && current.back() != ' ')
                {
                    current.push_back(' ');
                }
                ++offset;
                continue;
            }
            if (current.empty())
            {
                tokenUtf8Mode = utf8Mode;
            }
            current.push_back(character);
            ++offset;
        }

        flush();
        return tokens;
    }
}
