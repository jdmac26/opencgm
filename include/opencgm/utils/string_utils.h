#ifndef OPENCGM_UTILS_STRING_UTILS_H
#define OPENCGM_UTILS_STRING_UTILS_H

#include <string>
#include <algorithm>
#include <cctype>

namespace opencgm {
namespace utils {

/**
 * @brief Remove leading and trailing whitespace from a string
 *
 * @param value The string to trim
 * @param stripQuotes If true, also removes surrounding single or double quotes
 * @return Trimmed string
 */
inline std::string trimString(const std::string& value, bool stripQuotes = false) {
    size_t start = 0;
    size_t end = value.size();

    // Trim leading whitespace
    while (start < end && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    // Trim trailing whitespace
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    // Handle empty result
    if (start >= end) {
        return std::string();
    }

    std::string trimmed = value.substr(start, end - start);

    // Optionally strip surrounding quotes
    if (stripQuotes && trimmed.size() >= 2) {
        char first = trimmed.front();
        char last = trimmed.back();
        if ((first == '\'' && last == '\'') || (first == '"' && last == '"')) {
            trimmed = trimmed.substr(1, trimmed.size() - 2);
        }
    }

    return trimmed;
}

/**
 * @brief Convert a string to lowercase
 *
 * @param value The string to convert
 * @return Lowercase copy of the string
 */
inline std::string toLower(const std::string& value) {
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    return result;
}

/**
 * @brief Remove surrounding quotes from a string
 *
 * @param value The string to process
 * @return String with quotes removed if present
 */
inline std::string stripQuotes(const std::string& value) {
    if (value.size() >= 2) {
        char first = value.front();
        char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return value.substr(1, value.size() - 2);
        }
    }
    return value;
}

} // namespace utils
} // namespace opencgm

#endif // OPENCGM_UTILS_STRING_UTILS_H
