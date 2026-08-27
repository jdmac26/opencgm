#include "opencgm/svg/xml_utils.h"

#include <cctype>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <utility>

namespace opencgm::svg
{
    namespace
    {
        struct Utf8CodePoint
        {
            uint32_t value = 0;
            size_t length = 1;
            bool valid = false;
        };

        Utf8CodePoint decodeUtf8(
            const std::string &value,
            size_t offset)
        {
            const auto first =
                static_cast<unsigned char>(value[offset]);
            if (first < 0x80)
            {
                return {first, 1, true};
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
                return {};
            }

            if (offset + length > value.size())
            {
                return {};
            }

            for (size_t index = 1; index < length; ++index)
            {
                const auto continuation =
                    static_cast<unsigned char>(value[offset + index]);
                if ((continuation & 0xC0) != 0x80)
                {
                    return {};
                }
                codePoint =
                    (codePoint << 6) | (continuation & 0x3F);
            }

            if (codePoint < minimum ||
                codePoint > 0x10FFFF ||
                (codePoint >= 0xD800 && codePoint <= 0xDFFF))
            {
                return {};
            }

            return {codePoint, length, true};
        }

        bool isXml10CodePoint(uint32_t codePoint)
        {
            return codePoint == 0x09 ||
                   codePoint == 0x0A ||
                   codePoint == 0x0D ||
                   (codePoint >= 0x20 && codePoint <= 0xD7FF) ||
                   (codePoint >= 0xE000 && codePoint <= 0xFFFD) ||
                   (codePoint >= 0x10000 && codePoint <= 0x10FFFF);
        }

        std::string escapeXml(
            const std::string &value,
            bool attribute)
        {
            std::string escaped;
            escaped.reserve(value.size() + 10);

            size_t offset = 0;
            while (offset < value.size())
            {
                const Utf8CodePoint decoded = decodeUtf8(value, offset);
                if (!decoded.valid)
                {
                    ++offset;
                    continue;
                }
                if (!isXml10CodePoint(decoded.value))
                {
                    offset += decoded.length;
                    continue;
                }

                if (decoded.value >= 0x80)
                {
                    escaped.append(value, offset, decoded.length);
                    offset += decoded.length;
                    continue;
                }

                switch (static_cast<char>(decoded.value))
                {
                case '&':
                    escaped += "&amp;";
                    break;
                case '<':
                    escaped += "&lt;";
                    break;
                case '>':
                    escaped += "&gt;";
                    break;
                case '"':
                    escaped += attribute ? "&quot;" : "\"";
                    break;
                case '\'':
                    escaped += attribute ? "&apos;" : "'";
                    break;
                default:
                    escaped.push_back(
                        static_cast<char>(decoded.value));
                    break;
                }
                offset += decoded.length;
            }

            return escaped;
        }

        void appendEncodedIdentifierCodePoint(
            std::string &output,
            uint32_t codePoint)
        {
            std::ostringstream encoded;
            encoded << "_u"
                    << std::uppercase
                    << std::hex
                    << codePoint;
            output += encoded.str();
        }
    }

    std::string escapeXmlAttribute(const std::string &value)
    {
        return escapeXml(value, true);
    }

    std::string escapeXmlText(const std::string &value)
    {
        return escapeXml(value, false);
    }

    std::string sanitizeIdentifier(const std::string &value)
    {
        std::string result;
        size_t offset = 0;
        bool firstCharacter = true;

        while (offset < value.size())
        {
            const Utf8CodePoint decoded = decodeUtf8(value, offset);
            if (!decoded.valid)
            {
                appendEncodedIdentifierCodePoint(result, 0xFFFD);
                ++offset;
                continue;
            }
            offset += decoded.length;

            if (firstCharacter)
            {
                if (decoded.value < 128 &&
                    (std::isalpha(
                         static_cast<unsigned char>(decoded.value)) ||
                     decoded.value == '_'))
                {
                    result.push_back(
                        static_cast<char>(decoded.value));
                }
                else if (
                    decoded.value < 128 &&
                    std::isdigit(
                        static_cast<unsigned char>(decoded.value)))
                {
                    result.push_back('_');
                    result.push_back(
                        static_cast<char>(decoded.value));
                }
                else
                {
                    appendEncodedIdentifierCodePoint(
                        result,
                        decoded.value);
                }
                firstCharacter = false;
                continue;
            }

            if (decoded.value < 128 &&
                (std::isalnum(
                     static_cast<unsigned char>(decoded.value)) ||
                 decoded.value == '-' ||
                 decoded.value == '_' ||
                 decoded.value == ':' ||
                 decoded.value == '.'))
            {
                result.push_back(
                    static_cast<char>(decoded.value));
            }
            else
            {
                appendEncodedIdentifierCodePoint(
                    result,
                    decoded.value);
            }
        }

        return result.empty() ? "_" : result;
    }

    UniqueIdAllocator::UniqueIdAllocator(std::string fallback)
        : fallback_(sanitizeIdentifier(fallback))
    {
        if (fallback_ == "_")
        {
            fallback_ = "id";
        }
    }

    std::string UniqueIdAllocator::allocate(
        const std::string &rawIdentifier)
    {
        std::string base = sanitizeIdentifier(rawIdentifier);
        if (base == "_")
        {
            base = fallback_;
        }

        auto &counter = counters_[base];
        if (counter == 0 && allocated_.insert(base).second)
        {
            counter = 1;
            return base;
        }

        size_t suffix = counter;
        std::string unique;
        do
        {
            ++suffix;
            unique = base + "_" + std::to_string(suffix);
        } while (!allocated_.insert(unique).second);

        counter = suffix;
        return unique;
    }

    void UniqueIdAllocator::reset()
    {
        counters_.clear();
        allocated_.clear();
    }
}
