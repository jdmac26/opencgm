#include "opencgm/svg/aps_attribute_interpreter.h"

#include "opencgm/svg/aps_text_decoder.h"
#include "opencgm/utils/string_utils.h"

#include <cctype>
#include <cstdio>
#include <deque>
#include <unordered_map>
#include <unordered_set>

namespace opencgm::svg
{
    namespace
    {
        const std::unordered_set<std::string> &reservedKeys()
        {
            static const std::unordered_set<std::string> keys = {
                "apsid", "apsname", "apsclass", "aps", "apsattr",
                "behavior", "content", "desc", "grobject", "grnode",
                "hotspot", "layer", "layerdesc", "layername", "linkuri",
                "name", "para", "region", "regionlist", "screentip",
                "subpara", "target", "type", "viewcontext", "visibility",
                "title", "?"};
            return keys;
        }

        bool looksLikeInlineUri(const std::string &value)
        {
            if (value.find("://") != std::string::npos)
            {
                return true;
            }
            if (!value.empty())
            {
                const char first = value.front();
                if (first == '#' || first == '/' || first == '&')
                {
                    return true;
                }
            }
            return value.find('\\') != std::string::npos ||
                   value.find(".html") != std::string::npos ||
                   value.find(".htm") != std::string::npos ||
                   value.find(".cgm") != std::string::npos ||
                   value.find(".svg") != std::string::npos;
        }

        bool isUriUnreserved(unsigned char character)
        {
            return std::isalnum(character) ||
                   character == '-' ||
                   character == '.' ||
                   character == '_' ||
                   character == '~';
        }

        bool isUriReserved(unsigned char character)
        {
            switch (character)
            {
            case ':':
            case '/':
            case '?':
            case '#':
            case '[':
            case ']':
            case '@':
            case '!':
            case '$':
            case '&':
            case '\'':
            case '(':
            case ')':
            case '*':
            case '+':
            case ',':
            case ';':
            case '=':
            case '%':
                return true;
            default:
                return false;
            }
        }

        std::string sanitizeLinkTarget(const std::string &value)
        {
            std::string sanitized;
            sanitized.reserve(value.size());
            for (unsigned char character : value)
            {
                if (character < 0x20 || character == 0x7F)
                {
                    continue;
                }
                if (character == 0xA0)
                {
                    character = ' ';
                }
                if (std::isspace(character))
                {
                    continue;
                }
                if (character < 0x80)
                {
                    if (std::isalnum(character) ||
                        character == '_' ||
                        character == '-' ||
                        character == '.' ||
                        character == ':' ||
                        character == '#')
                    {
                        sanitized.push_back(
                            static_cast<char>(character));
                    }
                    continue;
                }
                sanitized.push_back(static_cast<char>(character));
            }
            return utils::trimString(sanitized);
        }

        const std::unordered_set<std::string> &allowedLinkSchemes()
        {
            static const std::unordered_set<std::string> schemes = {
                "http", "https", "mailto", "tel", "dm"};
            return schemes;
        }

        std::vector<std::string> splitLinkuriSegments(
            const std::vector<std::string> &tokens)
        {
            std::vector<std::string> segments;
            for (const std::string &token : tokens)
            {
                std::string trimmed = utils::trimString(token);
                if (trimmed.empty())
                {
                    continue;
                }

                std::string lowered = utils::toLower(trimmed);
                if (lowered == "linkuri")
                {
                    continue;
                }
                if (lowered.rfind("linkuri=", 0) == 0)
                {
                    trimmed =
                        utils::trimString(trimmed.substr(8));
                }

                trimmed = utils::stripQuotes(trimmed);
                trimmed =
                    ApsAttributeInterpreter::sanitizeScalar(
                        trimmed,
                        false);
                if (trimmed.empty())
                {
                    continue;
                }

                size_t start = 0;
                while (start <= trimmed.size())
                {
                    const size_t separator =
                        trimmed.find('!', start);
                    std::string segment = utils::trimString(
                        trimmed.substr(start, separator - start));
                    segment = utils::stripQuotes(segment);
                    segment =
                        ApsAttributeInterpreter::sanitizeScalar(segment);
                    if (!segment.empty())
                    {
                        segments.push_back(std::move(segment));
                    }
                    if (separator == std::string::npos)
                    {
                        break;
                    }
                    start = separator + 1;
                }
            }
            return segments;
        }

        bool isValidTargetCandidate(const std::string &value)
        {
            if (value.empty())
            {
                return false;
            }
            for (const unsigned char character : value)
            {
                if (std::isspace(character))
                {
                    return false;
                }
            }
            return true;
        }

        const std::unordered_set<std::string> &knownLinkBehaviors()
        {
            static const std::unordered_set<std::string> behaviors = {
                "_blank", "_self", "_parent", "_top", "_replace",
                "replace", "replaceall", "new", "newwindow", "embed",
                "donothing", "information", "highlight", "select",
                "zoom", "pan", "magnify", "activate", "deactivate",
                "toggle", "play", "pause", "stop", "linkmode"};
            return behaviors;
        }

        std::string normalizeBehavior(
            const std::string &lower,
            const std::string &original)
        {
            static const std::unordered_map<std::string, std::string>
                normalized = {
                    {"new", "NewWindow"},
                    {"newwindow", "NewWindow"},
                    {"replace", "Replace"},
                    {"replaceall", "Replace"},
                    {"embed", "Embed"},
                    {"donothing", "DoNothing"},
                    {"information", "Information"},
                    {"highlight", "Highlight"},
                    {"activate", "Activate"},
                    {"deactivate", "Deactivate"},
                    {"select", "Select"},
                    {"zoom", "Zoom"},
                    {"pan", "Pan"},
                    {"magnify", "Magnify"},
                    {"toggle", "Toggle"},
                    {"play", "Play"},
                    {"pause", "Pause"},
                    {"stop", "Stop"}};
            const auto match = normalized.find(lower);
            return match == normalized.end()
                       ? original
                       : match->second;
        }
    }

    bool ApsAttributeInterpreter::isReservedKey(
        const std::string &value)
    {
        return reservedKeys().count(utils::toLower(
                   utils::trimString(value))) != 0;
    }

    bool ApsAttributeInterpreter::parseVisibility(
        const std::string &value,
        bool defaultVisible)
    {
        const std::string lower =
            utils::toLower(utils::trimString(value));
        if (lower == "off" ||
            lower == "0" ||
            lower == "false" ||
            lower == "hidden" ||
            lower == "no")
        {
            return false;
        }
        if (lower == "on" ||
            lower == "1" ||
            lower == "true" ||
            lower == "visible" ||
            lower == "yes")
        {
            return true;
        }
        return defaultVisible;
    }

    std::vector<std::string>
    ApsAttributeInterpreter::collectValueTokens(
        const std::vector<std::string> &tokens,
        const std::string &key)
    {
        const std::string keyLower =
            utils::toLower(utils::trimString(key));
        std::vector<std::string> values;

        for (size_t index = 0; index < tokens.size(); ++index)
        {
            const std::string token =
                utils::trimString(tokens[index]);
            if (token.empty())
            {
                continue;
            }
            const std::string tokenLower =
                utils::toLower(token);
            if (tokenLower == keyLower)
            {
                for (size_t valueIndex = index + 1;
                     valueIndex < tokens.size();
                     ++valueIndex)
                {
                    const std::string candidate =
                        utils::trimString(tokens[valueIndex]);
                    if (candidate.empty())
                    {
                        continue;
                    }
                    const std::string candidateLower =
                        utils::toLower(candidate);
                    if (candidateLower == keyLower &&
                        !values.empty())
                    {
                        break;
                    }
                    if (isReservedKey(candidateLower) &&
                        candidateLower != keyLower)
                    {
                        break;
                    }
                    if (ApsTextDecoder::isUsableToken(candidate))
                    {
                        values.push_back(candidate);
                    }
                }
                if (!values.empty())
                {
                    break;
                }
                continue;
            }

            const size_t separator = tokenLower.find('=');
            if (separator != std::string::npos &&
                tokenLower.substr(0, separator) == keyLower)
            {
                std::string value = utils::trimString(
                    token.substr(separator + 1),
                    false);
                value = utils::stripQuotes(value);
                if (!value.empty())
                {
                    values.push_back(std::move(value));
                    break;
                }
            }
        }
        return values;
    }

    std::string ApsAttributeInterpreter::joinValues(
        const std::vector<std::string> &parts)
    {
        std::string combined;
        for (const std::string &part : parts)
        {
            if (part.empty())
            {
                continue;
            }
            if (!combined.empty())
            {
                combined.push_back(' ');
            }
            combined += part;
        }
        return utils::trimString(combined);
    }

    std::string ApsAttributeInterpreter::fallbackValue(
        const std::vector<std::string> &tokens,
        const std::string &key)
    {
        const std::string keyLower =
            utils::toLower(utils::trimString(key));
        for (auto token = tokens.rbegin();
             token != tokens.rend();
             ++token)
        {
            const std::string candidate =
                utils::trimString(*token);
            if (candidate.empty())
            {
                continue;
            }
            const std::string candidateLower =
                utils::toLower(candidate);
            if (candidateLower == keyLower ||
                isReservedKey(candidateLower) ||
                !ApsTextDecoder::isUsableToken(candidate))
            {
                continue;
            }
            return candidate;
        }
        return {};
    }

    int ApsAttributeInterpreter::scoreTokens(
        const std::vector<std::string> &tokens)
    {
        if (tokens.empty())
        {
            return -100;
        }

        int score = 0;
        for (const std::string &token : tokens)
        {
            if (token.empty())
            {
                score -= 5;
                continue;
            }

            bool hasAsciiAlphanumeric = false;
            for (const unsigned char character : token)
            {
                if (character >= 0x20 && character < 0x7F)
                {
                    score += 2;
                    hasAsciiAlphanumeric =
                        hasAsciiAlphanumeric ||
                        std::isalnum(character);
                }
                else if (
                    character < 0x20 &&
                    character != 0x09 &&
                    character != 0x0A &&
                    character != 0x0D)
                {
                    score -= 6;
                }
                else
                {
                    ++score;
                }
            }
            if (hasAsciiAlphanumeric)
            {
                score += 6;
            }
            if (token.find("\xEF\xBF\xBD") != std::string::npos)
            {
                score -= 12;
            }
        }
        return score;
    }

    std::string ApsAttributeInterpreter::sanitizeScalar(
        const std::string &input,
        bool collapseWhitespace)
    {
        std::string result;
        result.reserve(input.size());
        bool lastWasSpace = false;

        for (unsigned char character : input)
        {
            if (character == 0)
            {
                continue;
            }
            if (character == 0xA0)
            {
                character = ' ';
            }
            if (character < 0x20 || character == 0x7F)
            {
                if (collapseWhitespace && !lastWasSpace)
                {
                    result.push_back(' ');
                    lastWasSpace = true;
                }
                continue;
            }
            if (std::isspace(character))
            {
                if (!collapseWhitespace)
                {
                    result.push_back(' ');
                }
                else if (!lastWasSpace)
                {
                    result.push_back(' ');
                    lastWasSpace = true;
                }
                continue;
            }
            result.push_back(static_cast<char>(character));
            if (collapseWhitespace)
            {
                lastWasSpace = false;
            }
        }
        return utils::trimString(result);
    }

    std::string ApsAttributeInterpreter::encodeUri(
        const std::string &value)
    {
        std::string encoded;
        encoded.reserve(value.size());
        for (const unsigned char character : value)
        {
            if (character == ' ')
            {
                encoded += "%20";
            }
            else if (
                character < 0x20 ||
                character == 0x7F ||
                (character < 0x80 &&
                 !isUriUnreserved(character) &&
                 !isUriReserved(character)))
            {
                char buffer[4];
                std::snprintf(
                    buffer,
                    sizeof(buffer),
                    "%02X",
                    character);
                encoded.push_back('%');
                encoded += buffer;
            }
            else
            {
                encoded.push_back(
                    static_cast<char>(character));
            }
        }
        return encoded;
    }

    std::string ApsAttributeInterpreter::sanitizeLinkHref(
        const std::string &value)
    {
        const std::string sanitized =
            sanitizeScalar(value, false);
        if (sanitized.empty() ||
            (sanitized.size() >= 2 &&
             sanitized[0] == '/' &&
             sanitized[1] == '/'))
        {
            return {};
        }
        if (sanitized.front() == '#')
        {
            return sanitized;
        }

        const size_t fragment = sanitized.find('#');
        size_t colon = sanitized.find(':');
        if (colon != std::string::npos &&
            fragment != std::string::npos &&
            fragment < colon)
        {
            colon = std::string::npos;
        }
        if (colon != std::string::npos)
        {
            const std::string scheme =
                utils::toLower(sanitized.substr(0, colon));
            if (allowedLinkSchemes().count(scheme) == 0)
            {
                return {};
            }
        }
        return sanitized;
    }

    LinkuriFields ApsAttributeInterpreter::parseLinkuri(
        const std::vector<std::string> &tokens)
    {
        LinkuriFields fields;
        const std::vector<std::string> segments =
            splitLinkuriSegments(tokens);
        if (segments.empty())
        {
            return fields;
        }

        std::deque<std::string> queue(
            segments.begin(),
            segments.end());
        const auto &behaviors = knownLinkBehaviors();

        if (!queue.empty())
        {
            const std::string &first = queue.front();
            const std::string lower = utils::toLower(first);
            if (lower.find('=') == std::string::npos &&
                behaviors.count(lower) == 0 &&
                !(isValidTargetCandidate(first) &&
                  !looksLikeInlineUri(first)))
            {
                fields.uri = encodeUri(first);
                queue.pop_front();
            }
        }

        const std::unordered_map<std::string, std::string *>
            destinations = {
                {"behavior", &fields.behavior},
                {"behaviour", &fields.behavior},
                {"target", &fields.target},
                {"window", &fields.target},
                {"content", &fields.content},
                {"highlight", &fields.highlight}};

        while (!queue.empty())
        {
            std::string segment = std::move(queue.front());
            queue.pop_front();
            if (segment.empty())
            {
                continue;
            }

            const std::string lower = utils::toLower(segment);
            const size_t separator = lower.find('=');
            if (separator != std::string::npos)
            {
                const std::string key = utils::trimString(
                    lower.substr(0, separator));
                std::string rawValue = utils::stripQuotes(
                    utils::trimString(segment.substr(separator + 1)));
                rawValue = sanitizeScalar(rawValue);

                const auto destination = destinations.find(key);
                if (destination != destinations.end() &&
                    destination->second != nullptr)
                {
                    std::string interpreted;
                    if (key == "target" || key == "window")
                    {
                        interpreted = sanitizeLinkTarget(rawValue);
                    }
                    else if (
                        key == "behavior" ||
                        key == "behaviour")
                    {
                        const std::string behaviorLower =
                            utils::toLower(rawValue);
                        if (behaviors.count(behaviorLower) != 0)
                        {
                            interpreted = normalizeBehavior(
                                behaviorLower,
                                rawValue);
                        }
                    }
                    else
                    {
                        interpreted = sanitizeScalar(rawValue);
                    }
                    if (!interpreted.empty())
                    {
                        *destination->second = std::move(interpreted);
                    }
                    continue;
                }
            }

            if (fields.behavior.empty())
            {
                const std::string behaviorLower =
                    utils::toLower(segment);
                if (behaviors.count(behaviorLower) != 0)
                {
                    fields.behavior = normalizeBehavior(
                        behaviorLower,
                        segment);
                    continue;
                }
            }
            if (fields.target.empty() &&
                isValidTargetCandidate(segment) &&
                !looksLikeInlineUri(segment))
            {
                fields.target = sanitizeLinkTarget(segment);
                continue;
            }
            if (fields.content.empty())
            {
                fields.content = sanitizeScalar(segment);
                continue;
            }
            if (fields.highlight.empty())
            {
                fields.highlight = sanitizeScalar(segment);
            }
        }

        if (!fields.uri.empty())
        {
            fields.uri = sanitizeScalar(fields.uri, false);
        }
        return fields;
    }
}
