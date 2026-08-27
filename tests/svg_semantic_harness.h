#ifndef OPENCGM_TESTS_SVG_SEMANTIC_HARNESS_H
#define OPENCGM_TESTS_SVG_SEMANTIC_HARNESS_H

#include <cctype>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace opencgm::tests
{
    struct SvgSemanticSummary
    {
        std::map<std::string, size_t> element_counts;
        std::map<std::string, size_t> attribute_counts;
        std::vector<std::string> errors;

        bool wellFormed() const
        {
            return errors.empty();
        }
    };

    inline SvgSemanticSummary inspectSvgSemantics(const std::string &svg)
    {
        SvgSemanticSummary summary;
        std::vector<std::string> elementStack;
        const std::regex tagPattern(
            R"(<\s*(/?)\s*([A-Za-z_][A-Za-z0-9_.:-]*)([^>]*)>)");
        const auto isAttributeStart = [](unsigned char ch)
        {
            return std::isalpha(ch) != 0 || ch == '_' || ch == ':';
        };
        const auto isAttributeCharacter =
            [&](unsigned char ch)
        {
            return isAttributeStart(ch) ||
                   std::isdigit(ch) != 0 ||
                   ch == '.' ||
                   ch == '-';
        };

        for (auto tag = std::sregex_iterator(svg.begin(), svg.end(), tagPattern);
             tag != std::sregex_iterator();
             ++tag)
        {
            const bool closing = (*tag)[1].matched && (*tag)[1].str() == "/";
            const std::string name = (*tag)[2].str();
            const std::string suffix = (*tag)[3].str();
            const bool selfClosing =
                !closing && suffix.find_last_not_of(" \t\r\n") != std::string::npos &&
                suffix[suffix.find_last_not_of(" \t\r\n")] == '/';

            if (closing)
            {
                if (elementStack.empty())
                {
                    summary.errors.push_back("Unexpected closing element: " + name);
                }
                else if (elementStack.back() != name)
                {
                    summary.errors.push_back(
                        "Mismatched closing element: expected " +
                        elementStack.back() + ", got " + name);
                }
                else
                {
                    elementStack.pop_back();
                }
                continue;
            }

            ++summary.element_counts[name];
            std::set<std::string> namesOnElement;
            size_t cursor = 0;
            while (cursor < suffix.size())
            {
                while (cursor < suffix.size() &&
                       std::isspace(
                           static_cast<unsigned char>(suffix[cursor])) != 0)
                {
                    ++cursor;
                }
                if (cursor >= suffix.size() || suffix[cursor] == '/')
                {
                    break;
                }
                if (!isAttributeStart(
                        static_cast<unsigned char>(suffix[cursor])))
                {
                    ++cursor;
                    continue;
                }

                const size_t nameStart = cursor++;
                while (cursor < suffix.size() &&
                       isAttributeCharacter(
                           static_cast<unsigned char>(suffix[cursor])))
                {
                    ++cursor;
                }
                const std::string attributeName =
                    suffix.substr(nameStart, cursor - nameStart);
                while (cursor < suffix.size() &&
                       std::isspace(
                           static_cast<unsigned char>(suffix[cursor])) != 0)
                {
                    ++cursor;
                }
                if (cursor >= suffix.size() || suffix[cursor] != '=')
                {
                    continue;
                }
                ++cursor;

                ++summary.attribute_counts[attributeName];
                if (!namesOnElement.insert(attributeName).second)
                {
                    summary.errors.push_back(
                        "Duplicate attribute '" + attributeName +
                        "' on <" + name + ">");
                }

                while (cursor < suffix.size() &&
                       std::isspace(
                           static_cast<unsigned char>(suffix[cursor])) != 0)
                {
                    ++cursor;
                }
                if (cursor < suffix.size() &&
                    (suffix[cursor] == '"' || suffix[cursor] == '\''))
                {
                    const char quote = suffix[cursor++];
                    while (cursor < suffix.size() &&
                           suffix[cursor] != quote)
                    {
                        ++cursor;
                    }
                    if (cursor < suffix.size())
                    {
                        ++cursor;
                    }
                }
                else
                {
                    while (cursor < suffix.size() &&
                           std::isspace(
                               static_cast<unsigned char>(
                                   suffix[cursor])) == 0)
                    {
                        ++cursor;
                    }
                }
            }

            if (!selfClosing)
            {
                elementStack.push_back(name);
            }
        }

        for (auto it = elementStack.rbegin(); it != elementStack.rend(); ++it)
        {
            summary.errors.push_back("Unclosed element: " + *it);
        }
        if (summary.element_counts["svg"] != 1)
        {
            summary.errors.push_back("Expected exactly one root svg element");
        }

        return summary;
    }
}

#endif
