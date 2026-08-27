#ifndef OPENCGM_UTILS_SYMBOL_UTILS_H
#define OPENCGM_UTILS_SYMBOL_UTILS_H

#include "string_utils.h"
#include <string>
#include <optional>
#include <filesystem>
#include <unordered_set>
#include <vector>

namespace opencgm {
namespace utils {

/**
 * @brief Descriptor for symbol library references in CGM files
 *
 * Parses symbol library strings that may contain:
 * - Simple label: "symbolName"
 * - Label with URI: "label|uri" or "label=uri"
 * - URI with fragment: "uri#fragment"
 */
struct SymbolLibraryDescriptor {
    std::string raw;       // Original raw string
    std::string label;     // Display label or identifier
    std::string uri;       // URI or file path (without fragment)
    std::string fragment;  // Fragment identifier (after #)
};

/**
 * @brief Parse a symbol library descriptor string
 *
 * Supports formats:
 * - "symbolName"
 * - "label|uri" or "label=uri"
 * - "uri#fragment"
 * - "label|uri#fragment"
 *
 * @param raw Raw descriptor string from CGM file
 * @return Parsed descriptor with label, uri, and fragment separated
 */
inline SymbolLibraryDescriptor parseSymbolLibraryDescriptor(const std::string& raw) {
    SymbolLibraryDescriptor descriptor;
    std::string trimmed = trimString(raw);
    trimmed = stripQuotes(trimmed);
    descriptor.raw = trimmed;

    if (trimmed.empty()) {
        descriptor.label.clear();
        descriptor.uri.clear();
        return descriptor;
    }

    // Look for delimiter (| or =) between label and URI
    size_t delimiterPos = std::string::npos;
    for (char delimiter : {'|', '='}) {
        size_t pos = trimmed.find(delimiter);
        if (pos != std::string::npos) {
            delimiterPos = pos;
            break;
        }
    }

    // Split into label and URI if delimiter found
    if (delimiterPos != std::string::npos) {
        descriptor.label = trimString(trimmed.substr(0, delimiterPos));
        descriptor.uri = trimString(trimmed.substr(delimiterPos + 1));
    } else {
        descriptor.label = trimmed;
        descriptor.uri = trimmed;
    }

    // Default empty URI to label
    if (descriptor.uri.empty()) {
        descriptor.uri = descriptor.label;
    }

    // Extract fragment identifier (after #)
    size_t hashPos = descriptor.uri.find('#');
    if (hashPos != std::string::npos) {
        descriptor.fragment = trimString(descriptor.uri.substr(hashPos + 1));
        descriptor.uri = trimString(descriptor.uri.substr(0, hashPos));
    }

    // If no explicit fragment, use label as fragment (if different from URI)
    if (descriptor.fragment.empty() && !descriptor.label.empty() &&
        descriptor.label != descriptor.uri) {
        descriptor.fragment = descriptor.label;
    }

    return descriptor;
}

/**
 * @brief Check if a URI appears to be a remote resource
 *
 * @param uri URI string to check
 * @return true if URI has a protocol scheme (http://, https://, ftp://, etc.)
 */
inline bool isRemoteUri(const std::string& uri) {
    if (uri.empty()) {
        return false;
    }

    std::string lower = toLower(uri);

    // Check for common remote protocols
    if (lower.rfind("http://", 0) == 0 || lower.rfind("https://", 0) == 0 ||
        lower.rfind("ftp://", 0) == 0 || lower.rfind("mailto:", 0) == 0 ||
        lower.rfind("data:", 0) == 0) {
        return true;
    }

    // Check for any protocol scheme
    return lower.find("://") != std::string::npos;
}

/**
 * @brief Resolve a symbol library descriptor to a local file path
 *
 * Attempts to locate the symbol library file by checking multiple candidate paths:
 * 1. Absolute path (if URI is absolute)
 * 2. Relative to baseDir
 * 3. Relative to baseDir/symbols/
 * 4. Relative to baseDir/symbol-libraries/
 * 5. Current directory
 * 6. Same paths with .svg extension added (if no extension present)
 *
 * @param descriptor Parsed symbol library descriptor
 * @param baseDir Base directory for relative path resolution (typically CGM file directory)
 * @return Resolved absolute path if file exists, std::nullopt otherwise
 */
inline std::optional<std::filesystem::path> resolveSymbolLibraryPath(
    const SymbolLibraryDescriptor& descriptor,
    const std::string& baseDir) {

    // Skip remote URIs
    if (descriptor.uri.empty() || isRemoteUri(descriptor.uri)) {
        return std::nullopt;
    }

    std::filesystem::path uriPath(descriptor.uri);
    std::vector<std::filesystem::path> candidates;
    std::unordered_set<std::string> seen;

    // Helper to add candidate with deduplication
    auto addCandidate = [&](const std::filesystem::path& candidate) {
        if (candidate.empty()) {
            return;
        }
        // Normalize path for consistent comparison
        std::filesystem::path normalized = candidate.lexically_normal();
        std::string key = normalized.generic_string();
        if (seen.insert(key).second) {
            candidates.push_back(normalized);
        }
    };

    // 1. Try absolute path if provided
    if (uriPath.is_absolute()) {
        addCandidate(uriPath);
    }

    // 2-4. Try relative to base directory and common subdirectories
    if (!baseDir.empty()) {
        std::filesystem::path base(baseDir);
        addCandidate(base / uriPath);
        addCandidate(base / "symbols" / uriPath);
        addCandidate(base / "symbol-libraries" / uriPath);
    }

    // 5. Try current directory
    addCandidate(uriPath);

    // 6. If no extension, try adding .svg
    if (uriPath.extension().empty()) {
        std::filesystem::path withSvg = uriPath;
        withSvg += ".svg";

        if (uriPath.is_absolute()) {
            addCandidate(withSvg);
        }

        addCandidate(withSvg);

        if (!baseDir.empty()) {
            std::filesystem::path base(baseDir);
            addCandidate(base / withSvg);
            addCandidate(base / "symbols" / withSvg);
            addCandidate(base / "symbol-libraries" / withSvg);
        }
    }

    // Check each candidate path
    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec) &&
            std::filesystem::is_regular_file(candidate, ec)) {
            return candidate;
        }
    }

    return std::nullopt;
}

} // namespace utils
} // namespace opencgm

#endif // OPENCGM_UTILS_SYMBOL_UTILS_H
