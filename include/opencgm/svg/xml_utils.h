#ifndef OPENCGM_SVG_XML_UTILS_H
#define OPENCGM_SVG_XML_UTILS_H

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace opencgm::svg
{
    /**
     * Escape an XML 1.0 attribute value.
     *
     * Invalid XML control characters, malformed UTF-8, surrogate code points,
     * and values above U+10FFFF are omitted.
     */
    std::string escapeXmlAttribute(const std::string &value);

    /**
     * Escape XML 1.0 character data.
     *
     * This has the same UTF-8 validation as escapeXmlAttribute, but only
     * escapes characters that are significant in element text.
     */
    std::string escapeXmlText(const std::string &value);

    /**
     * Convert arbitrary UTF-8 input to the identifier form historically used
     * by the converter. Non-ASCII code points become `_u<HEX>`.
     */
    std::string sanitizeIdentifier(const std::string &value);

    /**
     * Allocates stable, document-local unique identifiers from arbitrary text.
     */
    class UniqueIdAllocator
    {
    public:
        explicit UniqueIdAllocator(std::string fallback = "id");

        std::string allocate(const std::string &rawIdentifier);
        void reset();

    private:
        std::string fallback_;
        std::unordered_map<std::string, size_t> counters_;
        std::unordered_set<std::string> allocated_;
    };
}

#endif
