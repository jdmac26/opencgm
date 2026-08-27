#ifndef OPENCGM_SVG_APS_TEXT_DECODER_H
#define OPENCGM_SVG_APS_TEXT_DECODER_H

#include <string>
#include <vector>

namespace opencgm::svg
{
    /**
     * Decodes the character encodings and token separators found in CGM
     * application-structure attributes.
     *
     * The decoder is intentionally stateless. ISO 2022 escape mode applies
     * only within one attribute payload.
     */
    class ApsTextDecoder
    {
    public:
        static std::vector<std::string> decodeTokens(
            const std::string &raw);

        static bool isUsableToken(const std::string &value);
    };
}

#endif
