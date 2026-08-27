#ifndef OPENCGM_SVG_APS_ATTRIBUTE_INTERPRETER_H
#define OPENCGM_SVG_APS_ATTRIBUTE_INTERPRETER_H

#include "opencgm/svg/internal_types.h"

#include <string>
#include <vector>

namespace opencgm::svg
{
    /**
     * Interprets decoded APS tokens without depending on converter state.
     */
    class ApsAttributeInterpreter
    {
    public:
        static bool isReservedKey(const std::string &value);

        static bool parseVisibility(
            const std::string &value,
            bool defaultVisible = true);

        static std::vector<std::string> collectValueTokens(
            const std::vector<std::string> &tokens,
            const std::string &key);

        static std::string joinValues(
            const std::vector<std::string> &parts);

        static std::string fallbackValue(
            const std::vector<std::string> &tokens,
            const std::string &key);

        static int scoreTokens(
            const std::vector<std::string> &tokens);

        static std::string sanitizeScalar(
            const std::string &input,
            bool collapseWhitespace = true);

        static std::string encodeUri(const std::string &value);

        static std::string sanitizeLinkHref(
            const std::string &value);

        static LinkuriFields parseLinkuri(
            const std::vector<std::string> &tokens);
    };
}

#endif
