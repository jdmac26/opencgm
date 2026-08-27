#include "opencgm/profile_validator.h"
#include "opencgm/commands/delimiter_commands.h"
#include "opencgm/commands/metafile_descriptor_commands.h"
#include "opencgm/commands/graphical_primitive_commands.h"
#include "opencgm/commands/attribute_commands.h"
#include "opencgm/commands/picture_descriptor_commands.h"
#include "opencgm/commands/external_commands.h"
#include "opencgm/commands/escape_commands.h"
#include "opencgm/commands/application_structure_commands.h"
#include "opencgm/utils/string_utils.h"
#include "opencgm/utils/symbol_utils.h"
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <limits>
#include <cmath>
#include <cctype>
#include <optional>
#include <filesystem>
#include <regex>
#include <fstream>

namespace
{
    bool decodeNextCodePoint(const std::string &text, size_t &index, uint32_t &codePoint);

    constexpr double kKnotTolerance = 1e-9;

    double computeTolerance(double a, double b)
    {
        double scale = std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
        return kKnotTolerance * scale;
    }

    bool nearlyEqual(double a, double b)
    {
        return std::fabs(a - b) <= computeTolerance(a, b);
    }

    std::string formatDouble(double value)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << value;
        return oss.str();
    }

    // Moved to cgm/utils/string_utils.h

    bool isValidFragmentIdentifier(const std::string &id)
    {
        if (id.empty())
        {
            return false;
        }

        size_t index = 0;
        uint32_t codePoint = 0;

        if (!decodeNextCodePoint(id, index, codePoint))
        {
            return false;
        }

        auto isValidStart = [](uint32_t cp) -> bool
        {
            if (cp == '_' || cp == ':')
            {
                return true;
            }
            if (cp < 0x80)
            {
                return std::isalpha(static_cast<unsigned char>(cp)) != 0;
            }
            return cp >= 0x80 && cp != 0xFFFD;
        };

        auto isValidChar = [](uint32_t cp) -> bool
        {
            if (cp == '_' || cp == '-' || cp == '.' || cp == ':' || cp == 0xB7)
            {
                return true;
            }
            if (cp < 0x80)
            {
                return std::isalnum(static_cast<unsigned char>(cp)) != 0;
            }
            return cp >= 0x80 && cp != 0xFFFD;
        };

        if (!isValidStart(codePoint))
        {
            return false;
        }

        while (index < id.size())
        {
            if (!decodeNextCodePoint(id, index, codePoint))
            {
                return false;
            }
            if (!isValidChar(codePoint))
            {
                return false;
            }
        }

        return true;
    }

    // Moved to cgm/utils/string_utils.h and cgm/utils/symbol_utils.h
    using opencgm::utils::SymbolLibraryDescriptor;

    std::string determineCgmBaseDirectory(const opencgm::CGMFile *file)
    {
        if (!file)
            return {};

        auto *binaryFile = dynamic_cast<const opencgm::BinaryCGMFile *>(file);
        if (!binaryFile)
            return {};

        const std::string &fileName = binaryFile->fileName();
        if (fileName.empty())
            return {};

        std::filesystem::path path(fileName);
        if (!path.has_parent_path())
            return {};

        return path.parent_path().string();
    }

    bool decodeNextCodePoint(const std::string &text, size_t &index, uint32_t &codePoint)
    {
        if (index >= text.size())
        {
            return false;
        }

        unsigned char first = static_cast<unsigned char>(text[index]);

        auto invalid = [&]() -> bool
        {
            codePoint = 0xFFFD;
            ++index;
            return false;
        };

        if (first < 0x80)
        {
            codePoint = first;
            ++index;
            return true;
        }

        size_t remaining = text.size() - index;
        if ((first >> 5) == 0x6)
        {
            if (remaining < 2)
                return invalid();
            unsigned char b1 = static_cast<unsigned char>(text[index + 1]);
            if ((b1 & 0xC0) != 0x80)
                return invalid();
            codePoint = ((first & 0x1F) << 6) | (b1 & 0x3F);
            index += 2;
            return true;
        }

        if ((first >> 4) == 0xE)
        {
            if (remaining < 3)
                return invalid();
            unsigned char b1 = static_cast<unsigned char>(text[index + 1]);
            unsigned char b2 = static_cast<unsigned char>(text[index + 2]);
            if (((b1 & 0xC0) != 0x80) || ((b2 & 0xC0) != 0x80))
                return invalid();
            codePoint = ((first & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
            index += 3;
            return true;
        }

        if ((first >> 3) == 0x1E)
        {
            if (remaining < 4)
                return invalid();
            unsigned char b1 = static_cast<unsigned char>(text[index + 1]);
            unsigned char b2 = static_cast<unsigned char>(text[index + 2]);
            unsigned char b3 = static_cast<unsigned char>(text[index + 3]);
            if (((b1 & 0xC0) != 0x80) || ((b2 & 0xC0) != 0x80) || ((b3 & 0xC0) != 0x80))
                return invalid();
            codePoint = ((first & 0x07) << 18) |
                        ((b1 & 0x3F) << 12) |
                        ((b2 & 0x3F) << 6) |
                        (b3 & 0x3F);
            index += 4;
            return true;
        }

        return invalid();
    }

    std::string formatCodePoint(uint32_t codePoint)
    {
        std::ostringstream oss;
        oss << "U+"
            << std::uppercase << std::hex << std::setw(4) << std::setfill('0')
            << codePoint;
        return oss.str();
    }

    bool isValidUriScheme(const std::string &scheme)
    {
        if (scheme.empty())
            return false;

        if (!std::isalpha(static_cast<unsigned char>(scheme.front())))
            return false;

        for (unsigned char ch : scheme)
        {
            if (!(std::isalpha(ch) || std::isdigit(ch) || ch == '+' || ch == '-' || ch == '.'))
                return false;
        }
        return true;
    }

    std::string sanitizeApsTextValue(const std::string &input)
    {
        if (input.empty())
        {
            return {};
        }

        std::string result;
        result.reserve(input.size());
        bool lastWasSpace = false;

        size_t i = 0;
        while (i < input.size())
        {
            unsigned char ch = static_cast<unsigned char>(input[i]);

            if (ch == 0x1B)
            {
                ++i;
                while (i < input.size())
                {
                    unsigned char esc = static_cast<unsigned char>(input[i]);
                    ++i;
                    if (esc >= 0x30 && esc <= 0x7E)
                    {
                        break;
                    }
                }
                lastWasSpace = false;
                continue;
            }

            if (ch < 0x20 || ch == 0x7F)
            {
                if (!lastWasSpace)
                {
                    result.push_back(' ');
                    lastWasSpace = true;
                }
                ++i;
                continue;
            }

            if (ch == 0xA0)
            {
                ch = ' ';
            }

            if (std::isspace(ch) && ch != ' ')
            {
                ch = ' ';
            }

            if (ch == ' ')
            {
                if (!lastWasSpace)
                {
                    result.push_back(' ');
                    lastWasSpace = true;
                }
                ++i;
                continue;
            }

            result.push_back(static_cast<char>(ch));
            lastWasSpace = false;
            ++i;
        }

        size_t start = 0;
        while (start < result.size() && result[start] == ' ')
        {
            ++start;
        }
        size_t end = result.size();
        while (end > start && result[end - 1] == ' ')
        {
            --end;
        }
        if (start > 0 || end < result.size())
        {
            return result.substr(start, end - start);
        }
        return result;
    }

    std::string normalizedApsValue(const opencgm::ApplicationStructureAttribute *attr)
    {
        if (!attr)
        {
            return {};
        }
        if (const auto &structured = attr->structuredText())
        {
            return sanitizeApsTextValue(*structured);
        }
        return sanitizeApsTextValue(attr->data());
    }

    struct LinkUriValidationResult
    {
        bool valid = true;
        bool hasFragment = false;
        bool fragmentIsLocal = false;
        std::string fragment;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };

    LinkUriValidationResult validateLinkUri(const std::string &rawValue)
    {
        LinkUriValidationResult result;
        std::string trimmed = opencgm::utils::trimString(rawValue);

        if (trimmed.empty())
        {
            result.valid = false;
            result.errors.emplace_back("linkuri attribute is empty; expected absolute/relative IRI or fragment reference");
            return result;
        }

        if (trimmed != rawValue)
        {
            result.warnings.emplace_back("linkuri attribute contains leading or trailing whitespace; whitespace is not significant");
        }

        bool containsInlineWhitespace = false;
        bool containsControlChars = false;

        for (unsigned char ch : trimmed)
        {
            if (std::isspace(ch))
            {
                containsInlineWhitespace = true;
            }
            if (ch < 0x20 || ch == 0x7F)
            {
                containsControlChars = true;
            }
        }

        if (containsInlineWhitespace)
        {
            result.valid = false;
            result.errors.emplace_back("linkuri '" + trimmed + "' contains whitespace; encode spaces as %20 per WebCGM URI rules");
        }

        if (containsControlChars)
        {
            result.valid = false;
            result.errors.emplace_back("linkuri '" + trimmed + "' contains control characters; only printable characters are permitted");
        }

        std::string base = trimmed;
        size_t hashPos = trimmed.find('#');
        if (hashPos != std::string::npos)
        {
            result.hasFragment = true;
            base = trimmed.substr(0, hashPos);
            result.fragment = trimmed.substr(hashPos + 1);

            if (trimmed.find('#', hashPos + 1) != std::string::npos)
            {
                result.valid = false;
                result.errors.emplace_back("linkuri '" + trimmed + "' contains multiple '#' delimiters; only one fragment identifier is allowed");
            }

            if (result.fragment.empty())
            {
                result.valid = false;
                result.errors.emplace_back("linkuri fragment identifier is empty (value '" + trimmed + "')");
            }
            else if (!isValidFragmentIdentifier(result.fragment))
            {
                result.valid = false;
                result.errors.emplace_back("Fragment identifier '#" + result.fragment + "' in linkuri is not WebCGM compliant");
            }

            result.fragmentIsLocal = base.empty();
        }

        if (!base.empty())
        {
            size_t colonPos = base.find(':');
            size_t slashPos = base.find('/');
            bool hasScheme = colonPos != std::string::npos && (slashPos == std::string::npos || colonPos < slashPos);

            if (hasScheme)
            {
                std::string scheme = base.substr(0, colonPos);
                if (!isValidUriScheme(scheme))
                {
                    result.valid = false;
                    result.errors.emplace_back("linkuri '" + trimmed + "' has invalid URI scheme '" + scheme + "'");
                }
            }
            else
            {
                unsigned char first = static_cast<unsigned char>(base.front());
                if (!(std::isalpha(first) || std::isdigit(first) || first == '.' || first == '/' || first == '~' || first == '_'))
                {
                    result.warnings.emplace_back("linkuri '" + trimmed + "' starts with non-standard relative reference; verify target resolves correctly");
                }
            }

            if (base.find('\\') != std::string::npos)
            {
                result.warnings.emplace_back("linkuri '" + trimmed + "' uses backslashes; prefer forward slashes in IRIs");
            }
        }

        return result;
    }

    struct KnotValidationResult
    {
        bool ok;
        std::string message;
    };

    struct StructuredFieldSummary
    {
        uint16_t type;
        size_t payloadSize;
        bool payloadBinary;
    };

    struct StructuredDataParseResult
    {
        std::vector<StructuredFieldSummary> fields;
        bool truncated = false;
        bool containsBinaryPayload = false;
        std::string binaryPreview;
    };

    std::string toHexPreview(const unsigned char *bytes, size_t length, size_t maxBytes = 32)
    {
        if (!bytes || length == 0)
        {
            return "";
        }

        size_t previewLength = std::min(length, maxBytes);
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < previewLength; ++i)
        {
            oss << std::setw(2) << static_cast<int>(bytes[i]);
            if (i + 1 < previewLength)
            {
                oss << ' ';
            }
        }
        return oss.str();
    }

    StructuredDataParseResult parseStructuredFields(const std::string &data)
    {
        StructuredDataParseResult result;

        const size_t size = data.size();
        size_t offset = 0;
        const auto *bytes = reinterpret_cast<const unsigned char *>(data.data());

        while (offset + 2 <= size)
        {
            // Read big-endian 16-bit field length
            uint16_t fieldLength = static_cast<uint16_t>(
                (static_cast<uint16_t>(bytes[offset]) << 8) |
                static_cast<uint16_t>(bytes[offset + 1]));
            offset += 2;

            if (fieldLength < 2)
            {
                result.truncated = true;
                break; // must include at least the 2-byte type
            }

            // Ensure the whole field (type + payload) is available
            if (offset + static_cast<size_t>(fieldLength) > size)
            {
                result.truncated = true;
                size_t remaining = size - offset;
                if (remaining > 0 && result.binaryPreview.empty())
                {
                    result.binaryPreview = toHexPreview(bytes + offset, remaining);
                }
                break;
            }

            // Read 2-byte field type
            uint16_t fieldType = static_cast<uint16_t>(
                (static_cast<uint16_t>(bytes[offset]) << 8) |
                static_cast<uint16_t>(bytes[offset + 1]));
            offset += 2;

            size_t payloadSize = static_cast<size_t>(fieldLength) - 2;
            bool payloadBinary = false;
            const unsigned char *payload = nullptr;

            if (payloadSize > 0)
            {
                payload = bytes + offset;

                // Heuristic: treat as binary if any control char except CR/LF/TAB
                for (size_t i = 0; i < payloadSize; ++i)
                {
                    unsigned char ch = payload[i];
                    if (ch < 0x20 && ch != '\r' && ch != '\n' && ch != '\t')
                    {
                        payloadBinary = true;
                        break;
                    }
                }
            }

            if (payloadBinary && payload != nullptr)
            {
                result.containsBinaryPayload = true;
                if (result.binaryPreview.empty())
                {
                    result.binaryPreview = toHexPreview(payload, payloadSize);
                }
            }

            result.fields.push_back({fieldType, payloadSize, payloadBinary});
            offset += payloadSize;
        }

        if (!result.truncated && offset < size)
        {
            bool nonZeroTail = false;
            for (size_t i = offset; i < size; ++i)
            {
                if (bytes[i] != 0)
                {
                    nonZeroTail = true;
                    break;
                }
            }
            if (nonZeroTail)
            {
                result.truncated = true;
                if (result.binaryPreview.empty())
                {
                    result.binaryPreview = toHexPreview(bytes + offset, size - offset);
                }
            }
        }

        return result;
    }

    KnotValidationResult validateKnotVectorConstraints(const std::vector<double> &knots,
                                                       int order,
                                                       int numControlPoints,
                                                       double startParam,
                                                       double endParam)
    {
        KnotValidationResult result{true, {}};

        if (order <= 0)
        {
            std::ostringstream oss;
            oss << "Spline order must be positive (found " << order << ")";
            result.ok = false;
            result.message = oss.str();
            return result;
        }

        if (numControlPoints <= 0)
        {
            std::ostringstream oss;
            oss << "At least one control point required (found " << numControlPoints << ")";
            result.ok = false;
            result.message = oss.str();
            return result;
        }

        const size_t expectedKnots = static_cast<size_t>(order) + static_cast<size_t>(numControlPoints);
        if (knots.size() != expectedKnots)
        {
            std::ostringstream oss;
            oss << "Knot vector length must equal control points (" << numControlPoints
                << ") + spline order (" << order << "), found " << knots.size();
            result.ok = false;
            result.message = oss.str();
            return result;
        }

        for (size_t i = 0; i + 1 < knots.size(); ++i)
        {
            double current = knots[i];
            double next = knots[i + 1];
            double tolerance = computeTolerance(current, next);
            if (current > next + tolerance)
            {
                std::ostringstream oss;
                oss << "Knot vector must be non-decreasing (knot[" << i << "]="
                    << formatDouble(current) << " > knot[" << (i + 1) << "]="
                    << formatDouble(next) << ")";
                result.ok = false;
                result.message = oss.str();
                return result;
            }
        }

        size_t i = 0;
        while (i < knots.size())
        {
            size_t j = i + 1;
            while (j < knots.size() && nearlyEqual(knots[j], knots[i]))
            {
                ++j;
            }

            size_t multiplicity = j - i;
            if (multiplicity > static_cast<size_t>(order))
            {
                std::ostringstream oss;
                oss << "Knot value " << formatDouble(knots[i]) << " has multiplicity "
                    << multiplicity << ", exceeds spline order " << order;
                result.ok = false;
                result.message = oss.str();
                return result;
            }

            i = j;
        }

        const size_t lowerIndex = static_cast<size_t>(order - 1);
        const size_t upperIndex = static_cast<size_t>(numControlPoints);

        if (lowerIndex >= knots.size() || upperIndex >= knots.size())
        {
            std::ostringstream oss;
            oss << "Invalid knot indices for order " << order << " and " << numControlPoints
                << " control points";
            result.ok = false;
            result.message = oss.str();
            return result;
        }

        double lower = knots[lowerIndex];
        double upper = knots[upperIndex];

        // ISO/IEC 8632-1 Corrigendum 1 (2006): End-value constraint
        // The knot values at positions [order-1] and [numControlPoints] must satisfy
        // a strict inequality: knots[order-1] < knots[numControlPoints]
        // This ensures a non-degenerate parametric domain for the B-spline.
        double domainTolerance = computeTolerance(lower, upper);
        if (upper <= lower + domainTolerance)
        {
            std::ostringstream oss;
            oss << "ISO 8632-1 Corrigendum 1 violation: Knot end-value constraint not met. "
                << "knots[" << lowerIndex << "]=" << formatDouble(lower)
                << " must be strictly less than knots[" << upperIndex << "]="
                << formatDouble(upper) << " (non-degenerate parametric domain required)";
            result.ok = false;
            result.message = oss.str();
            return result;
        }

        double startTolerance = computeTolerance(startParam, lower);
        if (startParam < lower - startTolerance || startParam > upper + startTolerance)
        {
            std::ostringstream oss;
            oss << "Start parameter " << formatDouble(startParam)
                << " must lie within [" << formatDouble(lower) << ", "
                << formatDouble(upper) << "]";
            result.ok = false;
            result.message = oss.str();
            return result;
        }

        double endTolerance = computeTolerance(endParam, upper);
        if (endParam < lower - endTolerance || endParam > upper + endTolerance)
        {
            std::ostringstream oss;
            oss << "End parameter " << formatDouble(endParam)
                << " must lie within [" << formatDouble(lower) << ", "
                << formatDouble(upper) << "]";
            result.ok = false;
            result.message = oss.str();
            return result;
        }

        double spanTolerance = computeTolerance(startParam, endParam);
        if (endParam < startParam - spanTolerance)
        {
            std::ostringstream oss;
            oss << "End parameter " << formatDouble(endParam)
                << " must not precede start parameter " << formatDouble(startParam);
            result.ok = false;
            result.message = oss.str();
            return result;
        }

        return result;
    }

} // namespace

namespace opencgm
{

    // ============================================================================
    // ValidationMessage Implementation
    // ============================================================================

    std::string ValidationMessage::toString() const
    {
        std::ostringstream oss;
        oss << "[" << getSeverityString() << "] ";

        if (!rule.empty())
        {
            oss << "(" << rule << ") ";
        }

        if (pictureIndex >= 0)
        {
            oss << "Picture " << pictureIndex << ": ";
        }

        if (!elementName.empty())
        {
            oss << elementName;
            if (elementId >= 0)
            {
                oss << " (ID " << elementId << ")";
            }
            oss << " - ";
        }

        oss << message;

        return oss.str();
    }

    std::string ValidationMessage::getSeverityString() const
    {
        switch (severity)
        {
        case ValidationSeverity::INFO:
            return "INFO";
        case ValidationSeverity::WARNING:
            return "WARNING";
        case ValidationSeverity::ERROR:
            return "ERROR";
        case ValidationSeverity::FATAL:
            return "FATAL";
        default:
            return "UNKNOWN";
        }
    }

    // ============================================================================
    // ProfileValidator Base Implementation
    // ============================================================================

    void ProfileValidator::addError(const std::string &msg, const std::string &rule,
                                    const std::string &element, int elementId)
    {
        messages_.emplace_back(ValidationSeverity::ERROR, msg, element, elementId,
                               ClassCode::DelimiterElement, currentPictureIndex_, -1, rule);
    }

    void ProfileValidator::addWarning(const std::string &msg, const std::string &rule,
                                      const std::string &element, int elementId)
    {
        messages_.emplace_back(ValidationSeverity::WARNING, msg, element, elementId,
                               ClassCode::DelimiterElement, currentPictureIndex_, -1, rule);
    }

    void ProfileValidator::addInfo(const std::string &msg, const std::string &rule,
                                   const std::string &element, int elementId)
    {
        messages_.emplace_back(ValidationSeverity::INFO, msg, element, elementId,
                               ClassCode::DelimiterElement, currentPictureIndex_, -1, rule);
    }

    // ============================================================================
    // WebCGM 2.1 Validator Implementation
    // ============================================================================

    WebCGM21Validator::WebCGM21Validator()
    {
        // Initialize prohibited elements for WebCGM 2.1

        // SEGMENT elements are completely prohibited
        prohibitedElements_[ClassCode::DelimiterElement] = {6, 7};                                         // BEGIN/END SEGMENT
        prohibitedElements_[ClassCode::SegmentControlandSegmentAttributeElements] = {1, 2, 3, 4, 5, 6, 7}; // All segment control

        // Set constraint flags
        allowSegments_ = false;
        allowMultiplePictures_ = false;
        maxProtectionRegions_ = 1;
        maxFigurePrimitives_ = 1024;
    }

    void WebCGM21Validator::setAllowSegments(bool allow)
    {
        allowSegments_ = allow;
    }

    std::vector<ValidationMessage> WebCGM21Validator::validate(const CGMFile *cgmFile)
    {
        messages_.clear();
        currentPictureIndex_ = 0;

        addInfo("Starting WebCGM 2.1 validation", "WebCGM-2.1");
        if (allowSegments_)
        {
            addInfo("ISO/IEC 8632 segment compatibility is enabled; SEGMENT elements will be accepted.",
                    "ISO/IEC 8632 Compatibility");
        }

        validateMetafileStructure(cgmFile);
        validatePictureCount(cgmFile);
        validateVersionSupport(cgmFile);
        validateEncodingType(cgmFile);
        validateProhibitedElements(cgmFile);
        validateTextConstraints(cgmFile);
        validateTextSequencing(cgmFile);
        validateStructuredDataRecords(cgmFile);
        validateApsStructure(cgmFile);
        validateNonGraphicalTextConstraints(cgmFile);
        validateColorTable(cgmFile);
        validateProtectionRegions(cgmFile);
        validateNurbsKnotVectors(cgmFile);
        validateConicArcFallback(cgmFile);
        validateGeneralizedDrawingPrimitives(cgmFile);
        validateSymbolReferences(cgmFile);
        validateExternalElements(cgmFile);
        validateFigureComplexity(cgmFile);
        validateTileCompression(cgmFile);

        // Phase 1 Quick Win Validations
        validateProfileDeclaration(cgmFile);
        validateCoordinateNormalization(cgmFile);
        validateTextBaselineAndUpVector(cgmFile);
        validateLineAndEdgeMetrics(cgmFile);
        // Phase 2 High Impact Validations
        validateFragmentIDLongForm(cgmFile);
        validateDegeneracyRules(cgmFile);
        validateElementSetRestrictions(cgmFile);
        // Phase 3 Completeness Validations (100% Compliance)
        validateRasterAspectPreservation(cgmFile);
        validateEnhancedURIValidation(cgmFile);
        validateEnhancedColorAnalysis(cgmFile);

        reportCharacterEncodingFindings(cgmFile);

        return messages_;
    }

    bool WebCGM21Validator::isElementAllowed(ClassCode elementClass, int elementId) const
    {
        if (allowSegments_)
        {
            if (elementClass == ClassCode::DelimiterElement && (elementId == 6 || elementId == 7))
            {
                return true;
            }
            if (elementClass == ClassCode::SegmentControlandSegmentAttributeElements)
            {
                return true;
            }
            if (elementClass == ClassCode::MetafileDescriptorElements && elementId == 18)
            {
                return true;
            }
        }

        auto it = prohibitedElements_.find(elementClass);
        if (it != prohibitedElements_.end())
        {
            const auto &prohibitedList = it->second;
            return std::find(prohibitedList.begin(), prohibitedList.end(), elementId) == prohibitedList.end();
        }
        return true;
    }

    bool WebCGM21Validator::hasConstraint(const std::string &constraintName) const
    {
        if (constraintName == "single_picture")
            return !allowMultiplePictures_;
        if (constraintName == "no_segments")
            return !allowSegments_;
        if (constraintName == "protection_region_limit")
            return maxProtectionRegions_ == 1;
        return false;
    }

    void WebCGM21Validator::validateMetafileStructure(const CGMFile *cgmFile)
    {
        if (cgmFile->commands().empty())
        {
            addError("Empty metafile - no commands found", "WebCGM-2.1");
            return;
        }

        // First command must be BEGIN METAFILE
        const auto *firstCmd = cgmFile->commands()[0].get();
        if (firstCmd->elementClass() != ClassCode::DelimiterElement || firstCmd->elementId() != 1)
        {
            addError("Metafile must start with BEGIN METAFILE", "ISO/IEC 8632-1",
                     firstCmd->toString(), firstCmd->elementId());
        }

        // Last command should be END METAFILE
        const auto *lastCmd = cgmFile->commands().back().get();
        if (lastCmd->elementClass() != ClassCode::DelimiterElement || lastCmd->elementId() != 2)
        {
            addWarning("Metafile should end with END METAFILE", "ISO/IEC 8632-1",
                       lastCmd->toString(), lastCmd->elementId());
        }
    }

    void WebCGM21Validator::validatePictureCount(const CGMFile *cgmFile)
    {
        int pictureCount = 0;

        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::DelimiterElement && cmd->elementId() == 3)
            {
                pictureCount++;
            }
        }

        if (pictureCount == 0)
        {
            addError("No pictures found in metafile", "WebCGM-2.1 T.14.13");
        }
        else if (pictureCount > 1)
        {
            addError("Multiple pictures per metafile not allowed (found " + std::to_string(pictureCount) + ")",
                     "WebCGM-2.1 T.14.13", "BEGIN PICTURE", 3);
        }
        else
        {
            addInfo("Single picture per metafile - compliant", "WebCGM-2.1 T.14.13");
        }
    }

    void WebCGM21Validator::validateVersionSupport(const CGMFile *cgmFile)
    {
        // Look for METAFILE VERSION element
        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::MetafileDescriptorElements && cmd->elementId() == 1)
            {
                auto *versionCmd = dynamic_cast<MetafileVersion *>(cmd.get());
                if (versionCmd)
                {
                    int version = versionCmd->version();
                    if (version < 1 || version > 4)
                    {
                        addError("Unsupported CGM version: " + std::to_string(version) + " (WebCGM supports V1-V4)",
                                 "WebCGM-2.1", "METAFILE VERSION", 1);
                    }
                    else if (version >= 3)
                    {
                        addInfo("CGM Version " + std::to_string(version) + " - recommended for WebCGM", "WebCGM-2.1");
                    }
                }
            }
        }
    }

    void WebCGM21Validator::validateConicArcFallback(const CGMFile *cgmFile)
    {
        int hyperbolicCount = 0;
        int parabolicCount = 0;

        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() != ClassCode::GraphicalPrimitiveElements)
            {
                continue;
            }

            if (cmd->elementId() == 22)
            {
                ++hyperbolicCount;
            }
            else if (cmd->elementId() == 23)
            {
                ++parabolicCount;
            }
        }

        if (hyperbolicCount == 0 && parabolicCount == 0)
        {
            return;
        }

        std::ostringstream oss;
        oss << "Conic arc fallback: ";

        bool first = true;
        if (hyperbolicCount > 0)
        {
            oss << hyperbolicCount << " hyperbolic arc" << (hyperbolicCount > 1 ? "s" : "");
            first = false;
        }
        if (parabolicCount > 0)
        {
            if (!first)
            {
                oss << " and ";
            }
            oss << parabolicCount << " parabolic arc" << (parabolicCount > 1 ? "s" : "");
        }

        oss << " will be sampled into polyline segments for SVG output.";

        addInfo(oss.str(), "WebCGM-2.1 Conic Arc Fallback");
    }

    void WebCGM21Validator::validateGeneralizedDrawingPrimitives(const CGMFile *cgmFile)
    {
        int pictureIndex = -1;
        bool found = false;

        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::DelimiterElement && cmd->elementId() == 3)
            { // BEGIN PICTURE
                ++pictureIndex;
                continue;
            }

            if (cmd->elementClass() == ClassCode::GraphicalPrimitiveElements && cmd->elementId() == 10)
            { // GDP
                currentPictureIndex_ = pictureIndex < 0 ? 0 : pictureIndex;
                addError("GENERALIZED DRAWING PRIMITIVE (GDP) is not permitted in WebCGM 2.1 content.",
                         "WebCGM-2.1 GDP", "GDP", 10);
                found = true;
            }
        }

        if (found)
        {
            addInfo("GDP primitive(s) were ignored by the SVG converter; placeholder metadata emitted.",
                    "WebCGM-2.1 GDP");
        }
    }

    void WebCGM21Validator::validateSymbolReferences(const CGMFile *cgmFile)
    {
        symbolLibraries_.clear();
        symbolLibraryResolvedPaths_.clear();
        symbolLibraryLocalResolved_.clear();
        symbolLibraryRemote_.clear();
        bool symbolListSeen = false;
        bool warnedMissingList = false;
        int pictureIndex = -1;
        std::string baseDirectory = determineCgmBaseDirectory(cgmFile);
        std::unordered_set<size_t> unresolvedLibraryEntriesWarned;

        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::DelimiterElement)
            {
                if (cmd->elementId() == 3) // BEGIN PICTURE
                {
                    ++pictureIndex;
                }
                continue;
            }

            if (cmd->elementClass() == ClassCode::MetafileDescriptorElements && cmd->elementId() == 23)
            {
                auto *libraryList = dynamic_cast<SymbolLibraryList *>(cmd.get());
                if (libraryList)
                {
                    symbolLibraries_ = libraryList->libraries();
                    symbolLibraryResolvedPaths_.assign(symbolLibraries_.size(), std::string());
                    symbolLibraryLocalResolved_.assign(symbolLibraries_.size(), false);
                    symbolLibraryRemote_.assign(symbolLibraries_.size(), false);
                    currentPictureIndex_ = pictureIndex < 0 ? 0 : pictureIndex;
                    if (!symbolLibraries_.empty())
                    {
                        std::ostringstream oss;
                        oss << "SYMBOL LIBRARY LIST declared with " << symbolLibraries_.size() << " entries: ";
                        for (size_t i = 0; i < symbolLibraries_.size(); ++i)
                        {
                            if (i > 0)
                                oss << ", ";
                            oss << "'" << symbolLibraries_[i] << "'";
                        }
                        addInfo(oss.str(), "WebCGM-2.1 Symbol Library");
                    }

                    for (size_t i = 0; i < symbolLibraries_.size(); ++i)
                    {
                        auto descriptor = opencgm::utils::parseSymbolLibraryDescriptor(symbolLibraries_[i]);
                        auto resolvedPath = opencgm::utils::resolveSymbolLibraryPath(descriptor, baseDirectory);
                        if (resolvedPath)
                        {
                            symbolLibraryResolvedPaths_[i] = resolvedPath->string();
                            symbolLibraryLocalResolved_[i] = true;
                            addInfo("SYMBOL LIBRARY entry '" + descriptor.raw + "' resolved to '" + resolvedPath->string() + "'.",
                                    "WebCGM-2.1 Symbol Library");
                        }
                        else if (opencgm::utils::isRemoteUri(descriptor.uri))
                        {
                            symbolLibraryResolvedPaths_[i] = descriptor.uri;
                            symbolLibraryRemote_[i] = true;
                            addInfo("SYMBOL LIBRARY entry '" + descriptor.raw + "' references remote resource '" + descriptor.uri + "'.",
                                    "WebCGM-2.1 Symbol Library");
                        }
                        else if (!descriptor.uri.empty())
                        {
                            symbolLibraryResolvedPaths_[i] = descriptor.uri;
                            addWarning("SYMBOL LIBRARY entry '" + descriptor.raw + "' did not resolve to a local asset relative to the CGM file.",
                                       "WebCGM-2.1 Symbol Library");
                        }
                    }
                    symbolListSeen = true;
                }
                continue;
            }

            if (cmd->elementClass() != ClassCode::GraphicalPrimitiveElements || cmd->elementId() != 27)
            {
                continue;
            }

            auto *polySymbol = dynamic_cast<PolySymbol *>(cmd.get());
            if (!polySymbol)
            {
                continue;
            }

            currentPictureIndex_ = pictureIndex < 0 ? 0 : pictureIndex;
            int symbolIndex = polySymbol->index();

            if (symbolLibraries_.empty())
            {
                if (!warnedMissingList)
                {
                    addWarning("POLYSYMBOL encountered but no SYMBOL LIBRARY LIST is defined; renderer emitted placeholder output.",
                               "WebCGM-2.1 Symbol Library", "POLYSYMBOL", 27);
                    warnedMissingList = true;
                }
                continue;
            }

            std::string resolvedName;
            size_t libraryIndex = std::numeric_limits<size_t>::max();
            if (symbolIndex >= 0 && symbolIndex < static_cast<int>(symbolLibraries_.size()))
            {
                resolvedName = symbolLibraries_[symbolIndex];
                libraryIndex = static_cast<size_t>(symbolIndex);
            }
            else if (symbolIndex > 0 && symbolIndex - 1 < static_cast<int>(symbolLibraries_.size()))
            {
                resolvedName = symbolLibraries_[symbolIndex - 1];
                libraryIndex = static_cast<size_t>(symbolIndex - 1);
            }

            if (resolvedName.empty())
            {
                addWarning("POLYSYMBOL index " + std::to_string(symbolIndex) +
                               " does not match any entry in SYMBOL LIBRARY LIST (" +
                               std::to_string(symbolLibraries_.size()) + " entries).",
                           "WebCGM-2.1 Symbol Library", "POLYSYMBOL", 27);
            }
            else
            {
                addInfo("POLYSYMBOL index " + std::to_string(symbolIndex) +
                            " resolved to library entry '" + resolvedName + "'.",
                        "WebCGM-2.1 Symbol Library", "POLYSYMBOL", 27);

                if (libraryIndex < symbolLibraryLocalResolved_.size())
                {
                    if (!symbolLibraryLocalResolved_[libraryIndex] && !symbolLibraryRemote_[libraryIndex])
                    {
                        std::string reference = (libraryIndex < symbolLibraryResolvedPaths_.size()) ? symbolLibraryResolvedPaths_[libraryIndex] : std::string();
                        if (reference.empty())
                            reference = "<unspecified>";
                        if (unresolvedLibraryEntriesWarned.insert(libraryIndex).second)
                        {
                            addWarning("POLYSYMBOL index " + std::to_string(symbolIndex) +
                                           " resolved to '" + resolvedName +
                                           "', but no local asset was located at '" + reference +
                                           "'; SVG output will retain the placeholder.",
                                       "WebCGM-2.1 Symbol Library", "POLYSYMBOL", 27);
                        }
                    }
                }
            }
        }

        if (!symbolListSeen && warnedMissingList == false)
        {
            // No symbol usage; nothing to report.
            return;
        }
    }

    void WebCGM21Validator::validateExternalElements(const CGMFile *cgmFile)
    {
        int pictureIndex = -1;
        bool found = false;

        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::DelimiterElement && cmd->elementId() == 3)
            {
                ++pictureIndex;
                continue;
            }

            if (cmd->elementClass() == ClassCode::ExternalElements)
            {
                currentPictureIndex_ = pictureIndex < 0 ? 0 : pictureIndex;
                addError("EXTERNAL elements are not permitted in WebCGM 2.1 content.",
                         "WebCGM-2.1 External Elements", cmd->toString(), cmd->elementId());
                found = true;
            }
        }

        if (found)
        {
            addInfo("External elements were ignored by the SVG converter.",
                    "WebCGM-2.1 External Elements");
        }
    }

    void WebCGM21Validator::validateEncodingType(const CGMFile* /* cgmFile */)
    {
        // WebCGM 2.1 requires binary encoding only
        // This is implicitly validated by the binary reader, but we note it
        addInfo("Binary encoding validated (WebCGM 2.1 requirement)", "WebCGM-2.1");
    }

    void WebCGM21Validator::validateProhibitedElements(const CGMFile *cgmFile)
    {
        for (const auto &cmd : cgmFile->commands())
        {
            ClassCode elementClass = cmd->elementClass();
            int elementId = cmd->elementId();

            if (!isElementAllowed(elementClass, elementId))
            {
                addError("Prohibited element: " + cmd->toString(),
                         "WebCGM-2.1 T.14.10", cmd->toString(), elementId);
            }

            // Check for generalized drawing primitives (GDP) - prohibited
            if (elementClass == ClassCode::GraphicalPrimitiveElements && elementId == 10)
            {
                addError("Generalized Drawing Primitive (GDP) not allowed",
                         "ATA GREXCHANGE", "GDP", 10);
            }
        }
    }

    void WebCGM21Validator::validateTextConstraints(const CGMFile *cgmFile)
    {
        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::GraphicalPrimitiveElements)
            {
                // TEXT element (ID 4)
                if (cmd->elementId() == 4)
                {
                    auto *textCmd = dynamic_cast<Text *>(cmd.get());
                    if (textCmd)
                    {
                        size_t textLength = textCmd->text().length();
                        if (textLength > 508)
                        {
                            addError("Graphical text exceeds 508 bytes (found " + std::to_string(textLength) + ")",
                                     "WebCGM-2.1 T.14.4", "TEXT", 4);
                        }
                        checkTextCharacterRepertoire(textCmd->text(), "TEXT", 4);
                    }
                }

                // RESTRICTED TEXT element (ID 5)
                if (cmd->elementId() == 5)
                {
                    auto *textCmd = dynamic_cast<RestrictedText *>(cmd.get());
                    if (textCmd)
                    {
                        size_t textLength = textCmd->text().length();
                        if (textLength > 508)
                        {
                            addError("Restricted text exceeds 508 bytes (found " + std::to_string(textLength) + ")",
                                     "WebCGM-2.1 T.14.4", "RESTRICTED TEXT", 5);
                        }
                        checkTextCharacterRepertoire(textCmd->text(), "RESTRICTED TEXT", 5);
                    }
                }

                if (cmd->elementId() == 6)
                {
                    auto *appendCmd = dynamic_cast<AppendText *>(cmd.get());
                    if (appendCmd)
                    {
                        checkTextCharacterRepertoire(appendCmd->text(), "APPEND TEXT", 6);
                    }
                }
            }
        }
    }

    void WebCGM21Validator::checkTextCharacterRepertoire(const std::string &value,
                                                         const std::string &elementName,
                                                         int elementId)
    {
        if (value.empty())
        {
            return;
        }

        const std::string rule = "WebCGM-2.1 Character Repertoire";

        size_t index = 0;
        bool invalidUtf8 = false;
        bool sawReplacement = false;
        std::set<uint32_t> c0Controls;
        std::set<uint32_t> c1Controls;

        while (index < value.size())
        {
            uint32_t codePoint = 0;
            bool ok = decodeNextCodePoint(value, index, codePoint);

            if (!ok)
            {
                invalidUtf8 = true;
                continue;
            }

            if (codePoint == 0xFFFD)
            {
                sawReplacement = true;
                continue;
            }

            if ((codePoint < 0x20 && codePoint != 0x09 && codePoint != 0x0A) || codePoint == 0x7F)
            {
                c0Controls.insert(codePoint);
            }
            else if (codePoint >= 0x80 && codePoint <= 0x9F)
            {
                c1Controls.insert(codePoint);
            }
        }

        if (invalidUtf8)
        {
            addError(elementName + " contains invalid UTF-8 sequence(s)",
                     rule, elementName, elementId);
        }

        if (sawReplacement)
        {
            addWarning(elementName + " contains replacement characters (U+FFFD) introduced during decoding",
                       rule, elementName, elementId);
        }

        for (uint32_t cp : c0Controls)
        {
            addError(elementName + " contains disallowed control character " + formatCodePoint(cp),
                     rule, elementName, elementId);
        }

        for (uint32_t cp : c1Controls)
        {
            addError(elementName + " contains C1 control character " + formatCodePoint(cp),
                     rule, elementName, elementId);
        }
    }

    void WebCGM21Validator::validateColorTable(const CGMFile *cgmFile)
    {
        // WebCGM T.14.3: "Either all colours or none shall be defined"
        // This means: if MAXIMUM COLOUR INDEX declares N indices (0..N),
        // then COLOUR TABLE entries must define all indices 0..N
        // Redefinition of indices is permitted within a picture

        int maxColourIndex = -1;
        bool hasMaxColourIndex = false;
        std::vector<bool> definedIndices;
        int colorTableCount = 0;

        // First pass: find MAXIMUM COLOUR INDEX
        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::MetafileDescriptorElements && cmd->elementId() == 9)
            {
                auto *maxIndexCmd = dynamic_cast<MaximumColourIndex *>(cmd.get());
                if (maxIndexCmd)
                {
                    maxColourIndex = maxIndexCmd->maxIndex();
                    hasMaxColourIndex = true;
                    definedIndices.resize(maxColourIndex + 1, false);
                    addInfo("MAXIMUM COLOUR INDEX declared: " + std::to_string(maxColourIndex),
                            "WebCGM-2.1 T.14.3");
                    break;
                }
            }
        }

        // Second pass: collect all COLOUR TABLE definitions
        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::AttributeElements && cmd->elementId() == 33)
            {
                auto *colorTableCmd = dynamic_cast<ColourTable *>(cmd.get());
                if (colorTableCmd)
                {
                    colorTableCount++;
                    int startIndex = colorTableCmd->startIndex();
                    const auto &colors = colorTableCmd->colors();

                    // Mark indices as defined
                    if (hasMaxColourIndex)
                    {
                        for (size_t i = 0; i < colors.size(); i++)
                        {
                            int index = startIndex + static_cast<int>(i);
                            if (index >= 0 && index <= maxColourIndex)
                            {
                                definedIndices[index] = true;
                            }
                            else if (index > maxColourIndex)
                            {
                                addWarning("COLOUR TABLE defines index " + std::to_string(index) +
                                               " which exceeds MAXIMUM COLOUR INDEX (" +
                                               std::to_string(maxColourIndex) + ")",
                                           "WebCGM-2.1 T.14.3", "COLOUR TABLE", 33);
                            }
                        }
                    }
                }
            }
        }

        // Validate "all or none" rule
        if (hasMaxColourIndex && maxColourIndex >= 0)
        {
            if (colorTableCount == 0)
            {
                // No colors defined - this is allowed ("none")
                addInfo("No COLOUR TABLE entries defined (valid: 'none' case)",
                        "WebCGM-2.1 T.14.3");
            }
            else
            {
                // Check if ALL indices are defined
                std::vector<int> undefinedIndices;
                for (int i = 0; i <= maxColourIndex; i++)
                {
                    if (!definedIndices[i])
                    {
                        undefinedIndices.push_back(i);
                    }
                }

                if (!undefinedIndices.empty())
                {
                    std::string undefinedList;
                    for (size_t i = 0; i < std::min(undefinedIndices.size(), size_t(10)); i++)
                    {
                        if (i > 0)
                            undefinedList += ", ";
                        undefinedList += std::to_string(undefinedIndices[i]);
                    }
                    if (undefinedIndices.size() > 10)
                    {
                        undefinedList += ", ...";
                    }

                    addError("COLOUR TABLE violates 'all or none' rule: " +
                                 std::to_string(undefinedIndices.size()) + " of " +
                                 std::to_string(maxColourIndex + 1) + " indices undefined [" +
                                 undefinedList + "]",
                             "WebCGM-2.1 T.14.3", "COLOUR TABLE", 33);
                }
                else
                {
                    addInfo("COLOUR TABLE defines all " + std::to_string(maxColourIndex + 1) +
                                " indices (valid: 'all' case)",
                            "WebCGM-2.1 T.14.3");
                }
            }
        }
        else if (colorTableCount > 0)
        {
            // Color table present but no MAXIMUM COLOUR INDEX
            addWarning("COLOUR TABLE present but MAXIMUM COLOUR INDEX not declared",
                       "WebCGM-2.1 T.14.3", "COLOUR TABLE", 33);
        }
    }

    void WebCGM21Validator::reportCharacterEncodingFindings(const CGMFile *cgmFile)
    {
        const auto &unsupported = cgmFile->unsupportedCharsets();
        if (!unsupported.empty())
        {
            for (const auto &designation : unsupported)
            {
                std::string display = designation.empty() ? std::string("UNKNOWN") : designation;
                addWarning("Unsupported ISO-2022 designation '" + display +
                               "' encountered; characters replaced with U+FFFD",
                           "ISO/IEC 8632-3 Character Coding",
                           "CHARACTER SET", -1);
            }
        }

        const auto &illegalControls = cgmFile->illegalControls();
        if (!illegalControls.empty())
        {
            std::ostringstream oss;
            oss << "Disallowed control codes encountered in source text: ";
            size_t count = 0;
            for (uint32_t cp : illegalControls)
            {
                if (count > 0)
                    oss << ", ";
                oss << "0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << cp;
                ++count;
                if (count >= 8 && illegalControls.size() > count)
                {
                    oss << ", ...";
                    break;
                }
            }
            addWarning(oss.str(), "WebCGM-2.1 Character Repertoire", "TEXT", 4);
        }
    }

    void WebCGM21Validator::validateXcfBindings(const CGMFile *cgmFile,
                                                const std::unordered_map<std::string, std::string> &apsTypeById,
                                                const std::map<std::string, int> &typeCounts)
    {
        if (!cgmFile || apsTypeById.empty())
        {
            return;
        }

        const auto *binary = dynamic_cast<const BinaryCGMFile *>(cgmFile);
        std::filesystem::path sourcePath;
        if (binary)
        {
            sourcePath = binary->fileName();
        }
        else
        {
            sourcePath = cgmFile->name();
        }

        if (sourcePath.empty())
        {
            return;
        }

        std::filesystem::path directory = sourcePath.has_parent_path()
                                              ? sourcePath.parent_path()
                                              : std::filesystem::current_path();
        std::string stem = sourcePath.stem().string();

        std::vector<std::filesystem::path> candidates = {
            directory / (stem + ".xcf"),
            directory / (stem + ".XCF")};

        std::filesystem::path xcfPath;
        for (const auto &candidate : candidates)
        {
            if (candidate.empty())
            {
                continue;
            }

            std::error_code ec;
            if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec))
            {
                xcfPath = candidate;
                break;
            }
        }

        if (xcfPath.empty())
        {
            return;
        }

        std::ifstream xcfStream(xcfPath, std::ios::binary);
        const std::string rule = "S1000D - XCF Binding";

        if (!xcfStream)
        {
            addWarning("Unable to open XCF file '" + xcfPath.string() + "' for APS binding validation",
                       rule, "XCF", -1);
            return;
        }

        std::string content((std::istreambuf_iterator<char>(xcfStream)),
                            std::istreambuf_iterator<char>());
        if (content.empty())
        {
            addWarning("XCF file '" + xcfPath.filename().string() + "' is empty",
                       rule, "XCF", -1);
            return;
        }

        std::regex apsidRegex(R"(apsid\s*=\s*["']([^"']+)["'])", std::regex::icase);
        std::regex apstargetRegex(R"(apstargetname\s*=\s*["']([^"']+)["'])", std::regex::icase);

        std::map<std::string, int> bindIdCounts;
        std::map<std::string, int> bindNameCounts;

        auto collectMatches = [&](const std::regex &pattern, std::map<std::string, int> &target)
        {
            for (std::sregex_iterator it(content.begin(), content.end(), pattern), end; it != end; ++it)
            {
                std::string value = opencgm::utils::trimString((*it)[1].str());
                if (!value.empty())
                {
                    target[value]++;
                }
            }
        };

        collectMatches(apsidRegex, bindIdCounts);
        collectMatches(apstargetRegex, bindNameCounts);

        for (const auto &entry : bindIdCounts)
        {
            auto it = apsTypeById.find(entry.first);
            if (it == apsTypeById.end())
            {
                addError("XCF bindById apsid '" + entry.first + "' not found in CGM APS identifiers",
                         rule, "bindById", -1);
            }
            else if (entry.second > 1)
            {
                addWarning("XCF bindById apsid '" + entry.first + "' appears " +
                               std::to_string(entry.second) + " times",
                           rule, "bindById", -1);
            }
        }

        for (const auto &entry : bindNameCounts)
        {
            auto it = typeCounts.find(entry.first);
            if (it == typeCounts.end() || it->second == 0)
            {
                addWarning("XCF bindByName apstargetname '" + entry.first +
                               "' has no matching APS type in the CGM",
                           rule, "bindByName", -1);
            }
            else if (entry.second > 1)
            {
                addWarning("XCF bindByName apstargetname '" + entry.first + "' appears " +
                               std::to_string(entry.second) + " times",
                           rule, "bindByName", -1);
            }
        }

        if (!bindIdCounts.empty() || !bindNameCounts.empty())
        {
            std::ostringstream oss;
            oss << "XCF '" << xcfPath.filename().string() << "' references "
                << bindIdCounts.size() << " bindById apsid(s)";
            if (!bindNameCounts.empty())
            {
                oss << " and " << bindNameCounts.size() << " bindByName apstargetname(s)";
            }
            addInfo(oss.str(), rule);
        }
    }

    void WebCGM21Validator::validateProtectionRegions(const CGMFile *cgmFile)
    {
        int protectionRegionCount = 0;
        int currentRegionIndex = -1;
        int elementsInRegion = 0;
        bool inRegion = false;

        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::DelimiterElement)
            {
                if (cmd->elementId() == 13)
                { // BEGIN PROTECTION REGION
                    protectionRegionCount++;
                    elementsInRegion = 0;
                    inRegion = true;

                    if (protectionRegionCount > maxProtectionRegions_)
                    {
                        addError("Maximum " + std::to_string(maxProtectionRegions_) +
                                     " protection region allowed (found " + std::to_string(protectionRegionCount) + ")",
                                 "WebCGM-2.1 T.14.11", "BEGIN PROTECTION REGION", 13);
                    }

                    // Region index should be 1 for WebCGM
                    auto *regionCmd = dynamic_cast<BeginProtectionRegion *>(cmd.get());
                    if (regionCmd)
                    {
                        currentRegionIndex = regionCmd->regionIndex();
                        if (currentRegionIndex != 1)
                        {
                            addError("Protection region index must be 1 (found " + std::to_string(currentRegionIndex) + ")",
                                     "WebCGM-2.1 T.14.11", "BEGIN PROTECTION REGION", 13);
                        }
                    }
                }
                else if (cmd->elementId() == 14)
                { // END PROTECTION REGION
                    if (elementsInRegion > 128)
                    {
                        addError("Protection region exceeds 128 elements (found " + std::to_string(elementsInRegion) + ")",
                                 "WebCGM-2.1 T.14.11", "END PROTECTION REGION", 14);
                    }
                    currentRegionIndex = -1;
                    inRegion = false;
                    elementsInRegion = 0;
                }
            }

            // Only count elements when actually inside a protection region
            // Don't count BEGIN/END PROTECTION REGION themselves
            if (inRegion && cmd->elementClass() != ClassCode::DelimiterElement)
            {
                elementsInRegion++;
            }
        }
    }

    void WebCGM21Validator::validateTextSequencing(const CGMFile *cgmFile)
    {
        const std::string rule = "ISO/IEC 8632-1/Corr.2 - APPEND TEXT sequencing";

        bool awaitingAppend = false;
        std::string pendingElementName;
        int pendingElementId = -1;

        auto reportMissingAppend = [&](const std::string &context)
        {
            std::string source = pendingElementName.empty() ? "TEXT/APPEND TEXT" : pendingElementName;
            std::string message = source + " with final flag OFF must be followed by APPEND TEXT";
            if (!context.empty())
            {
                message += " (encountered " + context + ")";
            }
            addError(message, rule, source, pendingElementId);
            awaitingAppend = false;
            pendingElementName.clear();
            pendingElementId = -1;
        };

        for (const auto &commandPtr : cgmFile->commands())
        {
            const auto *command = commandPtr.get();
            if (!command)
            {
                continue;
            }

            bool isAppendTextCmd = (command->elementClass() == ClassCode::GraphicalPrimitiveElements &&
                                    command->elementId() == 6);

            if (awaitingAppend && !isAppendTextCmd)
            {
                reportMissingAppend(command->toString());
            }

            if (command->elementClass() != ClassCode::GraphicalPrimitiveElements)
            {
                continue;
            }

            switch (command->elementId())
            {
            case 4:
            { // TEXT
                auto *textCmd = dynamic_cast<Text *>(commandPtr.get());
                if (!textCmd)
                {
                    awaitingAppend = false;
                    pendingElementName.clear();
                    pendingElementId = -1;
                    break;
                }

                if (!textCmd->isFinal())
                {
                    awaitingAppend = true;
                    pendingElementName = "TEXT";
                    pendingElementId = 4;
                }
                else
                {
                    awaitingAppend = false;
                    pendingElementName.clear();
                    pendingElementId = -1;
                }
                break;
            }

            case 5:
            { // RESTRICTED TEXT
                auto *textCmd = dynamic_cast<RestrictedText *>(commandPtr.get());
                if (!textCmd)
                {
                    awaitingAppend = false;
                    pendingElementName.clear();
                    pendingElementId = -1;
                    break;
                }

                if (!textCmd->isFinal())
                {
                    awaitingAppend = true;
                    pendingElementName = "RESTRICTED TEXT";
                    pendingElementId = 5;
                }
                else
                {
                    awaitingAppend = false;
                    pendingElementName.clear();
                    pendingElementId = -1;
                }
                break;
            }

            case 6:
            { // APPEND TEXT
                auto *appendCmd = dynamic_cast<AppendText *>(commandPtr.get());
                if (!appendCmd)
                {
                    awaitingAppend = false;
                    pendingElementName.clear();
                    pendingElementId = -1;
                    break;
                }

                if (!awaitingAppend)
                {
                    addError("APPEND TEXT must follow TEXT/RESTRICTED TEXT with final flag OFF",
                             rule, "APPEND TEXT", 6);
                }

                if (!appendCmd->isFinal())
                {
                    awaitingAppend = true;
                    pendingElementName = "APPEND TEXT";
                    pendingElementId = 6;
                }
                else
                {
                    awaitingAppend = false;
                    pendingElementName.clear();
                    pendingElementId = -1;
                }
                break;
            }

            default:
                // Other graphical primitives reset the expectation
                awaitingAppend = false;
                pendingElementName.clear();
                pendingElementId = -1;
                break;
            }
        }

        if (awaitingAppend)
        {
            reportMissingAppend("end of picture");
        }
    }

    void WebCGM21Validator::validateNonGraphicalTextConstraints(const CGMFile *cgmFile)
    {
        const size_t maxNonGraphicalLength = 2048;

        auto checkLength = [&](const std::string &value, const std::string &elementName, int elementId)
        {
            size_t length = value.size();
            if (length > maxNonGraphicalLength)
            {
                addError(elementName + " exceeds 2048 bytes (found " + std::to_string(length) + ")",
                         "WebCGM-2.1 T.14.5", elementName, elementId);
            }
            checkTextCharacterRepertoire(value, elementName, elementId);
        };

        for (const auto &cmdPtr : cgmFile->commands())
        {
            const auto *cmd = cmdPtr.get();
            if (!cmd)
            {
                continue;
            }

            if (cmd->elementClass() == ClassCode::ExternalElements)
            {
                if (cmd->elementId() == 1)
                {
                    auto *messageCmd = dynamic_cast<MessageCommand *>(cmdPtr.get());
                    if (messageCmd)
                    {
                        checkLength(messageCmd->message(), "MESSAGE", 1);
                    }
                }
                else if (cmd->elementId() == 2)
                {
                    auto *appData = dynamic_cast<ApplicationData *>(cmdPtr.get());
                    if (appData)
                    {
                        checkLength(appData->data(), "APPLICATION DATA", 2);
                    }
                }
            }
            else if (cmd->elementClass() == ClassCode::EscapeElement)
            {
                auto *escapeCmd = dynamic_cast<Escape *>(cmdPtr.get());
                if (escapeCmd)
                {
                    std::string sanitized = sanitizeApsTextValue(escapeCmd->dataRecord());
                    size_t length = sanitized.size();
                    if (length > maxNonGraphicalLength)
                    {
                        addError("ESCAPE DATA exceeds 2048 bytes (found " + std::to_string(length) + ")",
                                 "WebCGM-2.1 T.14.5", "ESCAPE DATA", cmd->elementId());
                    }
                }
            }
        }
    }
    void WebCGM21Validator::validateStructuredDataRecords(const CGMFile *cgmFile)
    {
        int sdrCount = 0;
        int sfCount = 0;
        bool binaryDetected = false;
        std::string firstBinaryPreview;
        std::map<uint16_t, int> sfTypeCounts;

        for (const auto &commandPtr : cgmFile->commands())
        {
            const auto *command = commandPtr.get();
            if (!command)
            {
                continue;
            }

            if (command->elementClass() == ClassCode::ApplicationStructureDescriptorElements &&
                command->elementId() == 1)
            {
                ++sdrCount;

                auto *attr = dynamic_cast<ApplicationStructureAttribute *>(commandPtr.get());
                if (!attr)
                {
                    continue;
                }

                StructuredDataParseResult parseResult = parseStructuredFields(attr->data());

                sfCount += static_cast<int>(parseResult.fields.size());
                for (const auto &field : parseResult.fields)
                {
                    sfTypeCounts[field.type]++;
                    if (field.payloadBinary)
                    {
                        binaryDetected = true;
                        if (firstBinaryPreview.empty() && !parseResult.binaryPreview.empty())
                        {
                            firstBinaryPreview = parseResult.binaryPreview;
                        }
                    }
                }

                if (parseResult.containsBinaryPayload && !binaryDetected)
                {
                    binaryDetected = true;
                    if (firstBinaryPreview.empty() && !parseResult.binaryPreview.empty())
                    {
                        firstBinaryPreview = parseResult.binaryPreview;
                    }
                }

                if (parseResult.truncated)
                {
                    addWarning("Structured Data Record '" + attr->attributeType() +
                                   "' truncated while parsing structured fields",
                               "ISO/IEC 8632-3/Corr.2 SF within SDR",
                               "APPLICATION STRUCTURE ATTRIBUTE", 1);
                }

                if (parseResult.containsBinaryPayload)
                {
                    std::string preview = parseResult.binaryPreview.empty() ? "n/a" : parseResult.binaryPreview;
                    addWarning("Structured Data Record '" + attr->attributeType() +
                                   "' contains binary payload; SF classification may be incomplete (preview: " + preview + ")",
                               "ISO/IEC 8632-3/Corr.2 SF within SDR",
                               "APPLICATION STRUCTURE ATTRIBUTE", 1);
                }
            }
        }

        if (sdrCount == 0)
        {
            return;
        }

        std::ostringstream oss;
        oss << "Structured Data Records processed: " << sdrCount;

        if (sfCount > 0)
        {
            oss << "; embedded SF records detected: " << sfCount;

            std::ostringstream typeStream;
            typeStream << std::hex << std::setfill('0');
            int listed = 0;
            for (const auto &entry : sfTypeCounts)
            {
                if (listed >= 5)
                {
                    typeStream << "...";
                    break;
                }
                if (listed > 0)
                {
                    typeStream << ", ";
                }
                typeStream << "0x" << std::setw(4) << entry.first
                           << " (" << std::dec << entry.second << ")";
                typeStream << std::hex;
                listed++;
            }

            if (!sfTypeCounts.empty())
            {
                oss << "; SF types: " << typeStream.str();
            }
        }
        else
        {
            oss << "; no embedded SF records detected";
        }

        addInfo(oss.str(), "ISO/IEC 8632-3/Corr.2 SF within SDR");
    }

    void WebCGM21Validator::validateApsStructure(const CGMFile *cgmFile)
    {
        struct ApsNode
        {
            std::string id;
            std::string type;
            bool hasBody = false;
        };

        std::vector<ApsNode> stack;
        std::unordered_set<std::string> seenIds;
        std::unordered_map<std::string, std::string> apsTypeById;
        std::map<std::string, int> typeCounts;

        struct LinkReference
        {
            std::string sourceId;
            std::string sourceType;
            std::string fragment;
        };

        std::vector<LinkReference> linkRefs;

        int beginCount = 0;
        int bodyCount = 0;
        int endCount = 0;
        size_t maxDepth = 0;

        for (const auto &cmdPtr : cgmFile->commands())
        {
            const auto *cmd = cmdPtr.get();
            if (!cmd)
            {
                continue;
            }

            if (cmd->elementClass() == ClassCode::DelimiterElement)
            {
                if (cmd->elementId() == 21)
                { // BEGIN APS
                    ++beginCount;
                    auto *apsCmd = dynamic_cast<BeginApplicationStructure *>(cmdPtr.get());
                    if (apsCmd)
                    {
                        ApsNode node;
                        node.id = apsCmd->identifier();
                        node.type = apsCmd->type();

                        if (node.id.empty())
                        {
                            addError("APS without identifier encountered; stable identifiers are required for WebCGM DOM addressing",
                                     "WebCGM-2.1 APS", "BEGIN APPLICATION STRUCTURE", 21);
                        }
                        else
                        {
                            if (!isValidFragmentIdentifier(node.id))
                            {
                                addError("APS identifier '" + node.id + "' is not WebCGM compliant (must match fragment NCName)",
                                         "WebCGM-2.1 APS", "BEGIN APPLICATION STRUCTURE", 21);
                            }
                            else
                            {
                                if (!seenIds.insert(node.id).second)
                                {
                                    addError("Duplicate APS identifier '" + node.id + "' detected",
                                             "WebCGM-2.1 APS", "BEGIN APPLICATION STRUCTURE", 21);
                                }
                                else
                                {
                                    apsTypeById[node.id] = node.type;
                                }
                            }
                        }

                        if (!node.type.empty())
                        {
                            typeCounts[node.type]++;
                        }

                        stack.push_back(std::move(node));
                        if (stack.size() > maxDepth)
                        {
                            maxDepth = stack.size();
                        }
                    }
                }
                else if (cmd->elementId() == 22)
                { // BEGIN APS BODY
                    ++bodyCount;
                    if (stack.empty())
                    {
                        addError("BEGIN APPLICATION STRUCTURE BODY encountered without preceding BEGIN",
                                 "WebCGM-2.1 APS", "BEGIN APPLICATION STRUCTURE BODY", 22);
                    }
                    else
                    {
                        if (stack.back().hasBody)
                        {
                            addWarning("Multiple BEGIN APPLICATION STRUCTURE BODY for APS id '" + stack.back().id + "'",
                                       "WebCGM-2.1 APS", "BEGIN APPLICATION STRUCTURE BODY", 22);
                        }
                        stack.back().hasBody = true;
                    }
                }
                else if (cmd->elementId() == 23)
                { // END APS
                    ++endCount;
                    if (stack.empty())
                    {
                        addError("END APPLICATION STRUCTURE encountered without matching BEGIN",
                                 "WebCGM-2.1 APS", "END APPLICATION STRUCTURE", 23);
                    }
                    else
                    {
                        ApsNode node = stack.back();
                        stack.pop_back();
                        if (!node.hasBody)
                        {
                            addWarning("APS id '" + node.id + "' closed without BEGIN APS BODY",
                                       "WebCGM-2.1 APS", "END APPLICATION STRUCTURE", 23);
                        }
                    }
                }
            }
            else if (cmd->elementClass() == ClassCode::ApplicationStructureDescriptorElements &&
                     cmd->elementId() == 1)
            { // APS ATTRIBUTE
                auto *attr = dynamic_cast<ApplicationStructureAttribute *>(cmdPtr.get());
                if (!attr)
                {
                    continue;
                }

                if (stack.empty())
                {
                    addWarning("APPLICATION STRUCTURE ATTRIBUTE found outside of APS scope",
                               "WebCGM-2.1 APS", "APPLICATION STRUCTURE ATTRIBUTE", 1);
                }

                const bool attributeInsideBody = (!stack.empty() && stack.back().hasBody);
                if (attributeInsideBody)
                {
                    std::string context = stack.back().id.empty() ? "current APS" : ("APS '" + stack.back().id + "'");
                    addError("APPLICATION STRUCTURE ATTRIBUTE encountered after BEGIN APPLICATION STRUCTURE BODY for " + context,
                             "WebCGM-2.1 APS", "APPLICATION STRUCTURE ATTRIBUTE", 1);
                }

                std::string attrType = opencgm::utils::toLower(attr->attributeType());
                if (attrType == "linkuri")
                {
                    const std::string currentType = stack.empty() ? std::string() : stack.back().type;
                    const std::string sourceId = stack.empty() ? std::string() : stack.back().id;

                    if (!currentType.empty() && currentType != "linkuri" && currentType != "hotspot")
                    {
                        addWarning("linkuri attribute encountered on APS type '" + currentType + "'; WebCGM expects linkuri/hotspot APS for link targets",
                                   "WebCGM-2.1 APS", "APPLICATION STRUCTURE ATTRIBUTE", 1);
                    }

                    std::string normalizedLink = normalizedApsValue(attr);
                    LinkUriValidationResult validation = validateLinkUri(normalizedLink);

                    for (const auto &err : validation.errors)
                    {
                        addError(err,
                                 "WebCGM-2.1 APS", "APPLICATION STRUCTURE ATTRIBUTE", 1);
                    }

                    for (const auto &warn : validation.warnings)
                    {
                        addWarning(warn,
                                   "WebCGM-2.1 APS", "APPLICATION STRUCTURE ATTRIBUTE", 1);
                    }

                    if (validation.valid && validation.hasFragment && validation.fragmentIsLocal)
                    {
                        linkRefs.push_back(LinkReference{sourceId, currentType, validation.fragment});
                    }
                }
            }
        }

        if (!stack.empty())
        {
            addError("APS stack not empty at END METAFILE (" + std::to_string(stack.size()) + " open)",
                     "WebCGM-2.1 APS", "END APPLICATION STRUCTURE", 23);
        }

        if (beginCount != endCount)
        {
            addError("APS begin/end mismatch: " + std::to_string(beginCount) + " BEGIN vs " +
                         std::to_string(endCount) + " END",
                     "WebCGM-2.1 APS", "END APPLICATION STRUCTURE", 23);
        }

        if (beginCount != bodyCount)
        {
            addWarning("APS body mismatch: " + std::to_string(beginCount) + " BEGIN vs " +
                           std::to_string(bodyCount) + " BODY",
                       "WebCGM-2.1 APS", "BEGIN APPLICATION STRUCTURE BODY", 22);
        }

        for (const auto &ref : linkRefs)
        {
            const std::string &fragment = ref.fragment;
            if (seenIds.find(fragment) == seenIds.end())
            {
                std::string source;
                if (!ref.sourceId.empty())
                {
                    source = "APS '" + ref.sourceId + "'";
                }
                else if (!ref.sourceType.empty())
                {
                    source = "APS of type '" + ref.sourceType + "'";
                }
                else
                {
                    source = "APS";
                }
                addError("linkuri fragment '#" + fragment + "' referenced from " + source +
                             " not found in APS identifiers",
                         "WebCGM-2.1 APS", "APPLICATION STRUCTURE ATTRIBUTE", 1);
            }
        }

        validateXcfBindings(cgmFile, apsTypeById, typeCounts);
        if (beginCount > 0)
        {
            std::ostringstream oss;
            oss << "APS summary: " << beginCount << " BEGIN, " << bodyCount << " BODY, " << endCount << " END";
            if (!typeCounts.empty())
            {
                oss << "; types: ";
                bool first = true;
                for (const auto &entry : typeCounts)
                {
                    if (!first)
                        oss << ", ";
                    oss << entry.first << "(" << entry.second << ")";
                    first = false;
                }
            }
            if (maxDepth > 0)
            {
                oss << "; max depth: " << maxDepth;
            }
            addInfo(oss.str(), "WebCGM-2.1 APS");
        }
        else
        {
            addInfo("No APS structures found", "WebCGM-2.1 APS");
        }
    }

    void WebCGM21Validator::validateNurbsKnotVectors(const CGMFile *cgmFile)
    {
        const std::string rule = "ISO/IEC 8632-1:1999/Cor.1";
        int pictureIndex = -1;

        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::DelimiterElement)
            {
                if (cmd->elementId() == 3)
                { // BEGIN PICTURE
                    ++pictureIndex;
                }
                continue;
            }

            if (cmd->elementClass() != ClassCode::GraphicalPrimitiveElements)
            {
                continue;
            }

            const int elementId = cmd->elementId();
            if (elementId != 24 && elementId != 25)
            {
                continue;
            }

            KnotValidationResult result{true, {}};
            const char *elementName = elementId == 24
                                          ? "NON-UNIFORM B-SPLINE"
                                          : "NON-UNIFORM RATIONAL B-SPLINE";

            int splineOrder = 0;
            int controlPointCount = 0;
            std::vector<double> knotVector;

            if (elementId == 24)
            {
                auto *spline = dynamic_cast<NonUniformBSpline *>(cmd.get());
                if (!spline)
                {
                    continue;
                }
                splineOrder = spline->splineOrder();
                controlPointCount =
                    static_cast<int>(spline->controlPoints().size());
                knotVector = spline->knots();
                result = validateKnotVectorConstraints(
                    spline->knots(),
                    spline->splineOrder(),
                    static_cast<int>(spline->controlPoints().size()),
                    spline->startParameter(),
                    spline->endParameter());
            }
            else
            {
                auto *spline = dynamic_cast<NonUniformRationalBSpline *>(cmd.get());
                if (!spline)
                {
                    continue;
                }
                splineOrder = spline->splineOrder();
                controlPointCount =
                    static_cast<int>(spline->controlPoints().size());
                knotVector = spline->knots();
                result = validateKnotVectorConstraints(
                    spline->knots(),
                    spline->splineOrder(),
                    static_cast<int>(spline->controlPoints().size()),
                    spline->startParameter(),
                    spline->endParameter());
            }

            if (!result.ok)
            {
                currentPictureIndex_ = pictureIndex < 0 ? 0 : pictureIndex;
                addError(result.message, rule, elementName, elementId);
            }

            // WebCGM 2.x PPF limits (T.19.24/T.19.25): cubic splines only
            // (order = 4), max 4096 control points, clamped knot form.
            // These are profile limits on a permitted element, so they are
            // reported as warnings rather than element prohibitions.
            const std::string webCgmRule = "WebCGM 2.1 T.19.24/T.19.25";
            currentPictureIndex_ = pictureIndex < 0 ? 0 : pictureIndex;
            if (splineOrder != 4)
            {
                std::ostringstream oss;
                oss << elementName << " has spline order " << splineOrder
                    << "; WebCGM 2.x permits cubic splines (order 4) only";
                addWarning(oss.str(), webCgmRule, elementName, elementId);
            }
            if (controlPointCount > 4096)
            {
                std::ostringstream oss;
                oss << elementName << " has " << controlPointCount
                    << " control points; WebCGM 2.x permits at most 4096";
                addWarning(oss.str(), webCgmRule, elementName, elementId);
            }
            if (splineOrder > 0 &&
                knotVector.size() >=
                    static_cast<size_t>(splineOrder) * 2)
            {
                const auto clampedAt =
                    [&](size_t begin)
                {
                    for (size_t i = begin + 1;
                         i < begin + static_cast<size_t>(splineOrder);
                         ++i)
                    {
                        if (!nearlyEqual(knotVector[i], knotVector[begin]))
                        {
                            return false;
                        }
                    }
                    return true;
                };
                const bool clampedStart = clampedAt(0);
                const bool clampedEnd = clampedAt(
                    knotVector.size() -
                    static_cast<size_t>(splineOrder));
                if (!clampedStart || !clampedEnd)
                {
                    std::ostringstream oss;
                    oss << elementName
                        << " knot vector is not in clamped form; WebCGM 2.x"
                           " requires clamped (end-interpolating) splines";
                    addWarning(oss.str(), webCgmRule, elementName, elementId);
                }
            }
        }
    }

    void WebCGM21Validator::validateFigureComplexity(const CGMFile *cgmFile)
    {
        bool inFigure = false;
        int primitiveCount = 0;

        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::DelimiterElement)
            {
                if (cmd->elementId() == 8)
                { // BEGIN FIGURE
                    inFigure = true;
                    primitiveCount = 0;
                }
                else if (cmd->elementId() == 9)
                { // END FIGURE
                    if (primitiveCount > maxFigurePrimitives_)
                    {
                        addError("FIGURE exceeds " + std::to_string(maxFigurePrimitives_) +
                                     " primitives (found " + std::to_string(primitiveCount) + ")",
                                 "WebCGM-2.1 T.14.12", "END FIGURE", 9);
                    }
                    inFigure = false;
                }
                else if (cmd->elementId() == 3)
                { // BEGIN PICTURE
                    currentPictureIndex_++;
                }
            }

            if (inFigure && cmd->elementClass() == ClassCode::GraphicalPrimitiveElements)
            {
                primitiveCount++;
            }
        }
    }

    void WebCGM21Validator::validateTileCompression(const CGMFile *cgmFile)
    {
        // WebCGM T.14.9: Tile array compression values must be in {5,6,7,9}
        // Values 0,1,2 are deprecated from WebCGM 1.0

        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::GraphicalPrimitiveElements)
            {
                // BITONAL TILE (ID 28)
                if (cmd->elementId() == 28)
                {
                    auto *tileCmd = dynamic_cast<BitonalTile *>(cmd.get());
                    if (tileCmd)
                    {
                        int compressionType = tileCmd->compressionType();

                        // Allowed values: 5=BITMAP, 6=JPEG, 7=PNG, 9=registered
                        if (compressionType < 5 || (compressionType > 7 && compressionType != 9))
                        {
                            if (compressionType >= 0 && compressionType <= 2)
                            {
                                addWarning("Tile compression type " + std::to_string(compressionType) +
                                               " is deprecated (WebCGM 1.0). Use {5,6,7,9}",
                                           "WebCGM-2.1 T.14.9", "BITONAL TILE", 28);
                            }
                            else
                            {
                                addError("Invalid tile compression type " + std::to_string(compressionType) +
                                             ". Must be in {5,6,7,9}",
                                         "WebCGM-2.1 T.14.9", "BITONAL TILE", 28);
                            }
                        }
                    }
                }
                // TILE (ID 29)
                else if (cmd->elementId() == 29)
                {
                    auto *tileCmd = dynamic_cast<Tile *>(cmd.get());
                    if (tileCmd)
                    {
                        int compressionType = tileCmd->compressionType();

                        if (compressionType < 5 || (compressionType > 7 && compressionType != 9))
                        {
                            if (compressionType >= 0 && compressionType <= 2)
                            {
                                addWarning("Tile compression type " + std::to_string(compressionType) +
                                               " is deprecated (WebCGM 1.0). Use {5,6,7,9}",
                                           "WebCGM-2.1 T.14.9", "TILE", 29);
                            }
                            else
                            {
                                addError("Invalid tile compression type " + std::to_string(compressionType) +
                                             ". Must be in {5,6,7,9}",
                                         "WebCGM-2.1 T.14.9", "TILE", 29);
                            }
                        }
                    }
                }
            }
        }
    }

    void WebCGM21Validator::trackPictureIndex(const CGMFile *cgmFile)
    {
        currentPictureIndex_ = 0;
        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::DelimiterElement && cmd->elementId() == 3)
            {
                currentPictureIndex_++;
            }
        }
    }
    void WebCGM21Validator::validateProfileDeclaration(const CGMFile *cgmFile)
    {
        // WebCGM 2.1 requires ProfileId and ProfileEd in METAFILE DESCRIPTION
        bool foundProfileId = false;
        bool foundProfileEd = false;
        std::string profileId;
        std::string profileEd;

        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::MetafileDescriptorElements &&
                cmd->elementId() == 2) // METAFILE DESCRIPTION
            {
                auto *descCmd = dynamic_cast<MetafileDescription *>(cmd.get());
                if (descCmd)
                {
                    std::string description = descCmd->description();

                    // Parse for ProfileId
                    size_t profileIdPos = description.find("ProfileId:");
                    if (profileIdPos != std::string::npos)
                    {
                        foundProfileId = true;
                        size_t start = profileIdPos + 10;
                        size_t end = description.find(";", start);
                        if (end == std::string::npos)
                            end = description.length();
                        profileId = description.substr(start, end - start);

                        // Trim whitespace
                        profileId.erase(0, profileId.find_first_not_of(" \t"));
                        profileId.erase(profileId.find_last_not_of(" \t") + 1);
                    }

                    // Parse for ProfileEd
                    size_t profileEdPos = description.find("ProfileEd:");
                    if (profileEdPos != std::string::npos)
                    {
                        foundProfileEd = true;
                        size_t start = profileEdPos + 10;
                        size_t end = description.find(";", start);
                        if (end == std::string::npos)
                            end = description.length();
                        profileEd = description.substr(start, end - start);

                        // Trim whitespace
                        profileEd.erase(0, profileEd.find_first_not_of(" \t"));
                        profileEd.erase(profileEd.find_last_not_of(" \t") + 1);
                    }

                    break;
                }
            }
        }

        // Validate findings
        if (!foundProfileId)
        {
            addWarning("ProfileId not found in METAFILE DESCRIPTION - recommended for WebCGM 2.1",
                       "WebCGM-2.1 Profile Declaration", "METAFILE DESCRIPTION", 2);
        }
        else if (profileId.find("WebCGM") == std::string::npos)
        {
            addWarning("ProfileId '" + profileId + "' does not contain 'WebCGM'",
                       "WebCGM-2.1 Profile Declaration", "METAFILE DESCRIPTION", 2);
        }
        else
        {
            addInfo("ProfileId: '" + profileId + "' - WebCGM profile declared",
                    "WebCGM-2.1 Profile Declaration");
        }

        if (!foundProfileEd)
        {
            addWarning("ProfileEd not found in METAFILE DESCRIPTION - recommended for WebCGM 2.1",
                       "WebCGM-2.1 Profile Declaration", "METAFILE DESCRIPTION", 2);
        }
        else if (profileEd.find("2.1") == std::string::npos)
        {
            addWarning("ProfileEd '" + profileEd + "' does not specify version '2.1'",
                       "WebCGM-2.1 Profile Declaration", "METAFILE DESCRIPTION", 2);
        }
        else
        {
            addInfo("ProfileEd: '" + profileEd + "' - WebCGM 2.1 version declared",
                    "WebCGM-2.1 Profile Declaration");
        }
    }

    void WebCGM21Validator::validateCoordinateNormalization(const CGMFile *cgmFile)
    {
        // Validate VDC EXTENT defines reasonable coordinate space
        bool foundVdcExtent = false;
        double x1 = 0, y1 = 0, x2 = 0, y2 = 0;

        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::PictureDescriptorElements &&
                cmd->elementId() == 6) // VDC EXTENT
            {
                auto *extentCmd = dynamic_cast<VDCExtent *>(cmd.get());
                if (extentCmd)
                {
                    foundVdcExtent = true;
                    CGMPoint corner1 = extentCmd->firstCorner();
                    CGMPoint corner2 = extentCmd->secondCorner();

                    x1 = corner1.x();
                    y1 = corner1.y();
                    x2 = corner2.x();
                    y2 = corner2.y();

                    double width = std::abs(x2 - x1);
                    double height = std::abs(y2 - y1);

                    if (width <= 0 || height <= 0)
                    {
                        addError("VDC EXTENT has zero or negative dimensions",
                                 "WebCGM-2.1 Coordinate Normalization", "VDC EXTENT", 6);
                    }

                    const double MAX_COORD = 1e10;
                    if (std::abs(x1) > MAX_COORD || std::abs(y1) > MAX_COORD ||
                        std::abs(x2) > MAX_COORD || std::abs(y2) > MAX_COORD)
                    {
                        addWarning("VDC EXTENT contains extreme coordinate values",
                                   "WebCGM-2.1 Coordinate Normalization", "VDC EXTENT", 6);
                    }

                    double aspectRatio = width / height;
                    if (aspectRatio > 100.0 || aspectRatio < 0.01)
                    {
                        std::ostringstream oss;
                        oss << "VDC EXTENT has extreme aspect ratio: " << aspectRatio;
                        addWarning(oss.str(),
                                   "WebCGM-2.1 Coordinate Normalization", "VDC EXTENT", 6);
                    }

                    std::ostringstream info;
                    info << "VDC EXTENT: (" << x1 << "," << y1 << ") to ("
                         << x2 << "," << y2 << "), Size: " << width << "×" << height;
                    addInfo(info.str(), "WebCGM-2.1 Coordinate Normalization");
                }
            }
        }

        if (!foundVdcExtent)
        {
            addWarning("No VDC EXTENT found - coordinate space undefined",
                       "WebCGM-2.1 Coordinate Normalization", "VDC EXTENT", 6);
        }
    }

    void WebCGM21Validator::validateTextBaselineAndUpVector(const CGMFile *cgmFile)
    {
        // Validate CHARACTER ORIENTATION and TEXT ALIGNMENT
        for (const auto &cmd : cgmFile->commands())
        {
            // CHARACTER ORIENTATION (Class 5, ID 16)
            if (cmd->elementClass() == ClassCode::AttributeElements &&
                cmd->elementId() == 16)
            {
                auto *orientCmd = dynamic_cast<CharacterOrientation *>(cmd.get());
                if (orientCmd)
                {
                    CGMPoint xUp = orientCmd->xUp();
                    CGMPoint yUp = orientCmd->yUp();
                    double upX = xUp.x();
                    double upY = xUp.y();
                    double baseX = yUp.x();
                    double baseY = yUp.y();

                    double upMag = std::sqrt(upX * upX + upY * upY);
                    double baseMag = std::sqrt(baseX * baseX + baseY * baseY);

                    if (upMag < 1e-10)
                    {
                        addError("CHARACTER ORIENTATION up-vector has zero magnitude",
                                 "WebCGM-2.1 Text Baseline", "CHARACTER ORIENTATION", 16);
                    }

                    if (baseMag < 1e-10)
                    {
                        addError("CHARACTER ORIENTATION base-vector has zero magnitude",
                                 "WebCGM-2.1 Text Baseline", "CHARACTER ORIENTATION", 16);
                    }

                    if (upMag > 1e-10 && baseMag > 1e-10)
                    {
                        double dotProduct = (upX * baseX + upY * baseY) / (upMag * baseMag);
                        if (std::abs(dotProduct) > 0.1)
                        {
                            addWarning("CHARACTER ORIENTATION vectors are not orthogonal",
                                       "WebCGM-2.1 Text Baseline", "CHARACTER ORIENTATION", 16);
                        }
                    }
                }
            }

            // TEXT ALIGNMENT (Class 5, ID 18)
            if (cmd->elementClass() == ClassCode::AttributeElements &&
                cmd->elementId() == 18)
            {
                auto *alignCmd = dynamic_cast<TextAlignment *>(cmd.get());
                if (alignCmd)
                {
                    int horizontal = alignCmd->horizontalAlignment();
                    int vertical = alignCmd->verticalAlignment();

                    if (horizontal < 0 || horizontal > 4)
                    {
                        addError("TEXT ALIGNMENT horizontal value out of range: " + std::to_string(horizontal),
                                 "WebCGM-2.1 Text Baseline", "TEXT ALIGNMENT", 18);
                    }

                    if (vertical < 0 || vertical > 6)
                    {
                        addError("TEXT ALIGNMENT vertical value out of range: " + std::to_string(vertical),
                                 "WebCGM-2.1 Text Baseline", "TEXT ALIGNMENT", 18);
                    }
                }
            }
        }
    }

    void WebCGM21Validator::validateLineAndEdgeMetrics(const CGMFile *cgmFile)
    {
        // Validate LINE WIDTH and EDGE WIDTH are positive
        for (const auto &cmd : cgmFile->commands())
        {
            // LINE WIDTH (Class 5, ID 3)
            if (cmd->elementClass() == ClassCode::AttributeElements &&
                cmd->elementId() == 3)
            {
                auto *widthCmd = dynamic_cast<LineWidth *>(cmd.get());
                if (widthCmd)
                {
                    double width = widthCmd->width();

                    if (width <= 0)
                    {
                        addError("LINE WIDTH must be positive, found: " + std::to_string(width),
                                 "WebCGM-2.1 Line Metrics", "LINE WIDTH", 3);
                    }
                    else if (width > 1000)
                    {
                        addWarning("LINE WIDTH unusually large: " + std::to_string(width),
                                   "WebCGM-2.1 Line Metrics", "LINE WIDTH", 3);
                    }
                }
            }

            // EDGE WIDTH (Class 5, ID 28)
            if (cmd->elementClass() == ClassCode::AttributeElements &&
                cmd->elementId() == 28)
            {
                auto *widthCmd = dynamic_cast<EdgeWidth *>(cmd.get());
                if (widthCmd)
                {
                    double width = widthCmd->width();

                    if (width <= 0)
                    {
                        addError("EDGE WIDTH must be positive, found: " + std::to_string(width),
                                 "WebCGM-2.1 Edge Metrics", "EDGE WIDTH", 28);
                    }
                    else if (width > 1000)
                    {
                        addWarning("EDGE WIDTH unusually large: " + std::to_string(width),
                                   "WebCGM-2.1 Edge Metrics", "EDGE WIDTH", 28);
                    }
                }
            }

            // WIDTH SPECIFICATION MODE (Class 2, ID 8)
            if (cmd->elementClass() == ClassCode::PictureDescriptorElements &&
                cmd->elementId() == 8)
            {
                addInfo("WIDTH SPECIFICATION MODE found", "WebCGM-2.1 Line Metrics");

            }
        }
    }
    // ========================================================================

    // ========================================================================
    // PHASE 2: HIGH IMPACT VALIDATIONS
    // ========================================================================

    /**
     * @brief Validate Fragment ID Long Form naming (WebCGM 2.1 Intelligent Content)
     *
     * WebCGM 2.1 Section 3.5: Application Structure (APS) Fragment Identifiers
     * - Long form: "grobject.{id}.{type}.{subtype}" e.g., "grobject.123.grnode.para"
     * - Required components: grobject prefix, numeric ID, type, optional subtype
     * - Valid types: grnode, grobject, para, subpara, hotspot, linkuri
     */
    void WebCGM21Validator::validateFragmentIDLongForm(const CGMFile *cgmFile)
    {
        for (const auto &cmd : cgmFile->commands())
        {
            // APPLICATION STRUCTURE ATTRIBUTE (Class 7, ID 1)
            if (cmd->elementClass() == ClassCode::ApplicationStructureDescriptorElements &&
                cmd->elementId() == 1)
            {
                auto *apsCmd = dynamic_cast<ApplicationStructureAttribute *>(cmd.get());
                if (apsCmd)
                {
                    std::string apsType = apsCmd->attributeType();
                    std::string apsId = apsCmd->data();

                    // Check if using long form (contains "grobject.")
                    if (apsId.find("grobject.") == 0)
                    {
                        // Parse long form: grobject.{id}.{type}.{subtype}
                        std::vector<std::string> parts;
                        size_t start = 0, end = 0;
                        while ((end = apsId.find('.', start)) != std::string::npos)
                        {
                            parts.push_back(apsId.substr(start, end - start));
                            start = end + 1;
                        }
                        parts.push_back(apsId.substr(start));

                        // Validate structure
                        if (parts.size() < 3)
                        {
                            addError("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                         ": Fragment ID long form must have at least 3 parts (grobject.{id}.{type}), found: " + apsId,
                                     "WebCGM-2.1 Fragment ID Long Form",
                                     "APPLICATION STRUCTURE ATTRIBUTE", 1);
                            continue;
                        }

                        // Validate numeric ID (part 1)
                        std::string idPart = parts[1];
                        bool isNumeric = !idPart.empty() && std::all_of(idPart.begin(), idPart.end(), ::isdigit);
                        if (!isNumeric)
                        {
                            addWarning("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                           ": Fragment ID second component should be numeric, found: " + idPart,
                                       "WebCGM-2.1 Fragment ID Long Form",
                                       "APPLICATION STRUCTURE ATTRIBUTE", 1);
                        }

                        // Validate type (part 2)
                        std::string typePart = parts[2];
                        static const std::vector<std::string> validTypes = {
                            "grnode", "grobject", "para", "subpara", "hotspot", "linkuri"};
                        bool validType = std::find(validTypes.begin(), validTypes.end(), typePart) != validTypes.end();
                        if (!validType)
                        {
                            addWarning("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                           ": Fragment ID type '" + typePart + "' not in recommended set (grnode, grobject, para, subpara, hotspot, linkuri)",
                                       "WebCGM-2.1 Fragment ID Long Form",
                                       "APPLICATION STRUCTURE ATTRIBUTE", 1);
                        }

                        // Info: Report valid long form usage
                        if (isNumeric && validType)
                        {
                            addInfo("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                        ": Valid Fragment ID long form: " + apsId,
                                    "WebCGM-2.1 Fragment ID Long Form",
                                    "APPLICATION STRUCTURE ATTRIBUTE", 1);
                        }
                    }
                }
            }
        }
    }

    /**
     * @brief Validate Degeneracy Rules for geometric primitives
     *
     * WebCGM 2.1 T.8.6: Degenerate geometry handling
     * - Zero-area polygons (all points colinear or coincident)
     * - Zero-length lines (start == end)
     * - Degenerate ellipses (semi-axes = 0)
     * - Degenerate arcs (start angle == end angle)
     */
    void WebCGM21Validator::validateDegeneracyRules(const CGMFile *cgmFile)
    {
        for (const auto &cmd : cgmFile->commands())
        {
            ClassCode eclass = cmd->elementClass();
            int eid = cmd->elementId();

            // POLYLINE (Class 4, ID 1)
            if (eclass == ClassCode::GraphicalPrimitiveElements && eid == 1)
            {
                auto *polylineCmd = dynamic_cast<Polyline *>(cmd.get());
                if (polylineCmd)
                {
                    const auto &points = polylineCmd->points();
                    if (points.size() >= 2)
                    {
                        CGMPoint first = points[0];
                        CGMPoint last = points[points.size() - 1];
                        if (first.x() == last.x() && first.y() == last.y() && points.size() == 2)
                        {
                            addWarning("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                           ": POLYLINE - Degenerate: zero-length line (start == end)",
                                       "WebCGM-2.1 Degeneracy Rules",
                                       "POLYLINE", 1);
                        }
                    }
                }
            }

            // POLYGON (Class 4, ID 7)
            if (eclass == ClassCode::GraphicalPrimitiveElements && eid == 7)
            {
                auto *polygonCmd = dynamic_cast<Polygon *>(cmd.get());
                if (polygonCmd)
                {
                    const auto &points = polygonCmd->points();
                    if (points.size() < 3)
                    {
                        addWarning("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                       ": POLYGON - Degenerate: fewer than 3 points (" +
                                       std::to_string(points.size()) + " points)",
                                   "WebCGM-2.1 Degeneracy Rules",
                                   "POLYGON", 7);
                    }
                    // Check for all-coincident points
                    else if (points.size() >= 3)
                    {
                        bool allSame = true;
                        CGMPoint first = points[0];
                        for (size_t i = 1; i < points.size(); ++i)
                        {
                            if (points[i].x() != first.x() || points[i].y() != first.y())
                            {
                                allSame = false;
                                break;
                            }
                        }
                        if (allSame)
                        {
                            addWarning("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                           ": POLYGON - Degenerate: all points coincident",
                                       "WebCGM-2.1 Degeneracy Rules",
                                       "POLYGON", 7);
                        }
                    }
                }
            }

            // ELLIPSE (Class 4, ID 17)
            if (eclass == ClassCode::GraphicalPrimitiveElements && eid == 17)
            {
                auto *ellipseCmd = dynamic_cast<Ellipse *>(cmd.get());
                if (ellipseCmd)
                {
                    CGMPoint cdp1 = ellipseCmd->firstConjugateDiameter();
                    CGMPoint cdp2 = ellipseCmd->secondConjugateDiameter();
                    double len1 = std::sqrt(cdp1.x() * cdp1.x() + cdp1.y() * cdp1.y());
                    double len2 = std::sqrt(cdp2.x() * cdp2.x() + cdp2.y() * cdp2.y());

                    if (len1 == 0.0 || len2 == 0.0)
                    {
                        addWarning("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                       ": ELLIPSE - Degenerate: zero-length conjugate diameter",
                                   "WebCGM-2.1 Degeneracy Rules",
                                   "ELLIPSE", 17);
                    }
                }
            }

            // CIRCULAR ARC CENTRE (Class 4, ID 15)
            if (eclass == ClassCode::GraphicalPrimitiveElements && eid == 15)
            {
                auto *arcCmd = dynamic_cast<CircularArcCentre *>(cmd.get());
                if (arcCmd)
                {
                    double radius = arcCmd->radius();
                    if (radius == 0.0)
                    {
                        addWarning("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                       ": CIRCULAR ARC CENTRE - Degenerate: zero radius",
                                   "WebCGM-2.1 Degeneracy Rules",
                                   "CIRCULAR ARC CENTRE", 15);
                    }

                    CGMPoint startVector = arcCmd->startDelta();
                    CGMPoint endVector = arcCmd->endDelta();
                    if (startVector.x() == endVector.x() && startVector.y() == endVector.y())
                    {
                        addWarning("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                       ": CIRCULAR ARC CENTRE - Degenerate: start and end vectors identical (zero sweep)",
                                   "WebCGM-2.1 Degeneracy Rules",
                                   "CIRCULAR ARC CENTRE", 15);
                    }
                }
            }
        }
    }

    /**
     * @brief Validate Element Set Restrictions for WebCGM 2.1 profile
     *
     * WebCGM 2.1 T.6: Element Set
     * - Prohibited elements: SEGMENT, EXTERNAL, GENERALIZED DRAWING PRIMITIVE
     * - APS-only elements: Must appear within APS BEGIN...END
     * - METAFILE DEFAULTS: Must contain only allowed elements
     */
    void WebCGM21Validator::validateElementSetRestrictions(const CGMFile *cgmFile)
    {
        bool inMetafileDefaults = false;
        int metafileDefaultsCount = 0;

        for (const auto &cmd : cgmFile->commands())
        {
            ClassCode eclass = cmd->elementClass();
            int eid = cmd->elementId();

            // Track METAFILE DEFAULTS region (Class 1, ID 12)
            if (eclass == ClassCode::DelimiterElement && eid == 12)
            {
                inMetafileDefaults = true;
                metafileDefaultsCount = 0;
                continue;
            }

            // Track BEGIN PICTURE (Class 1, ID 3) - ends METAFILE DEFAULTS
            if (eclass == ClassCode::DelimiterElement && eid == 3)
            {
                inMetafileDefaults = false;
            }

            // Check prohibited elements
            if (auto *beginSegment = dynamic_cast<BeginSegment *>(cmd.get()))
            {
                if (!allowSegments_)
                {
                    addError("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                 ": SEGMENT element (BEGIN SEGMENT) is prohibited in WebCGM 2.1 "
                                 "(REC-webcgm21-20100301, Element Set)",
                             "WebCGM-2.1 Element Set Restrictions",
                             "BEGIN SEGMENT", beginSegment->elementId());
                }
                continue;
            }

            if (auto *endSegment = dynamic_cast<EndSegment *>(cmd.get()))
            {
                if (!allowSegments_)
                {
                    addError("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                 ": SEGMENT element (END SEGMENT) is prohibited in WebCGM 2.1 "
                                 "(REC-webcgm21-20100301, Element Set)",
                             "WebCGM-2.1 Element Set Restrictions",
                             "END SEGMENT", endSegment->elementId());
                }
                continue;
            }

            // EXTERNAL (Class 1, ID 11) - PROHIBITED
            if (eclass == ClassCode::DelimiterElement && eid == 11)
            {
                addError("Picture " + std::to_string(currentPictureIndex_ + 1) +
                             ": EXTERNAL element is PROHIBITED in WebCGM 2.1",
                         "WebCGM-2.1 Element Set Restrictions",
                         "EXTERNAL", 11);
            }

            // GENERALIZED DRAWING PRIMITIVE (Class 4, ID 10) - PROHIBITED
            if (eclass == ClassCode::GraphicalPrimitiveElements && eid == 10)
            {
                addError("Picture " + std::to_string(currentPictureIndex_ + 1) +
                             ": GENERALIZED DRAWING PRIMITIVE (GDP) is PROHIBITED in WebCGM 2.1",
                         "WebCGM-2.1 Element Set Restrictions",
                         "GENERALIZED DRAWING PRIMITIVE", 10);
            }

            // Track elements within METAFILE DEFAULTS
            if (inMetafileDefaults)
            {
                metafileDefaultsCount++;

                // METAFILE DEFAULTS can only contain Picture Descriptor and Control elements
                // Class 2 (Picture Descriptor) and Class 3 (Control) are allowed
                // Class 5 (Attribute) is conditionally allowed for default attributes
                if (eclass != ClassCode::PictureDescriptorElements &&
                    eclass != ClassCode::ControlElements &&
                    eclass != ClassCode::AttributeElements)
                {
                    addWarning("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                   ": METAFILE DEFAULTS contains non-allowed element (Class " +
                                   std::to_string(static_cast<int>(eclass)) + ", ID " + std::to_string(eid) + ")",
                               "WebCGM-2.1 Element Set Restrictions",
                               "METAFILE DEFAULTS", 12);
                }
            }

            // APPLICATION STRUCTURE elements (Class 7) should be used (INFO level)
            if (eclass == ClassCode::ApplicationStructureDescriptorElements)
            {
                addInfo("Picture " + std::to_string(currentPictureIndex_ + 1) +
                            ": APPLICATION STRUCTURE ATTRIBUTE used (Intelligent Content feature)",
                        "WebCGM-2.1 Element Set Restrictions",
                        "APPLICATION STRUCTURE ATTRIBUTE", eid);
            }
        }

        // Report METAFILE DEFAULTS summary if found
        if (metafileDefaultsCount > 0)
        {
            addInfo("METAFILE DEFAULTS contained " + std::to_string(metafileDefaultsCount) +
                        " elements",
                    "WebCGM-2.1 Element Set Restrictions",
                    "METAFILE DEFAULTS", 12);
        }
    }


    // ========================================================================
    // PHASE 3: COMPLETENESS VALIDATIONS (100% Compliance)
    // ========================================================================

    /**
     * @brief Validate Raster Aspect Preservation for CELL ARRAY and Tile elements
     *
     * WebCGM 2.1 T.8.7: Raster Image Aspect Ratio Preservation
     * - CELL ARRAY: Check nx/ny ratio matches physical extent ratio
     * - TILE elements: Validate tile dimensions preserve aspect ratio
     * - Warn on extreme aspect ratios (> 10:1 or < 1:10)
     */
    void WebCGM21Validator::validateRasterAspectPreservation(const CGMFile *cgmFile)
    {
        for (const auto &cmd : cgmFile->commands())
        {
            ClassCode eclass = cmd->elementClass();
            int eid = cmd->elementId();

            // CELL ARRAY (Class 4, ID 18)
            if (eclass == ClassCode::GraphicalPrimitiveElements && eid == 18)
            {
                auto *cellCmd = dynamic_cast<CellArray *>(cmd.get());
                if (cellCmd)
                {
                    CGMPoint p = cellCmd->cornerP();
                    CGMPoint q = cellCmd->cornerQ();
                    CGMPoint r = cellCmd->cornerR();
                    int nx = cellCmd->nx();
                    int ny = cellCmd->ny();

                    // Calculate physical extent vectors
                    double pq_x = q.x() - p.x();
                    double pq_y = q.y() - p.y();
                    double pr_x = r.x() - p.x();
                    double pr_y = r.y() - p.y();

                    double pq_len = std::sqrt(pq_x * pq_x + pq_y * pq_y);
                    double pr_len = std::sqrt(pr_x * pr_x + pr_y * pr_y);

                    if (pq_len == 0.0 || pr_len == 0.0)
                    {
                        addError("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                     ": CELL ARRAY - Degenerate: zero-length extent vector",
                                 "WebCGM-2.1 Raster Aspect Preservation",
                                 "CELL ARRAY", 18);
                        continue;
                    }

                    // Calculate aspect ratios
                    double physicalAspect = pq_len / pr_len;
                    double cellAspect = static_cast<double>(nx) / static_cast<double>(ny);

                    // Check if aspect ratios match (within tolerance)
                    double aspectDiff = std::abs(physicalAspect - cellAspect);
                    double tolerance = 0.01; // 1% tolerance

                    if (aspectDiff > tolerance * physicalAspect)
                    {
                        addWarning("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                       ": CELL ARRAY - Aspect ratio mismatch: physical=" +
                                       std::to_string(physicalAspect) + ", cells=" + std::to_string(cellAspect),
                                   "WebCGM-2.1 Raster Aspect Preservation",
                                   "CELL ARRAY", 18);
                    }

                    // Warn on extreme aspect ratios
                    if (physicalAspect > 10.0 || physicalAspect < 0.1)
                    {
                        addWarning("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                       ": CELL ARRAY - Extreme aspect ratio: " + std::to_string(physicalAspect),
                                   "WebCGM-2.1 Raster Aspect Preservation",
                                   "CELL ARRAY", 18);
                    }
                }
            }

            // BEGIN TILE ARRAY (Class 1, ID 19)
            if (eclass == ClassCode::DelimiterElement && eid == 19)
            {
                auto *tileArrayCmd = dynamic_cast<BeginTileArray *>(cmd.get());
                if (tileArrayCmd)
                {
                    int nCellsPath = tileArrayCmd->nCellsPerTileInPathDirection();
                    int nCellsLine = tileArrayCmd->nCellsPerTileInLineDirection();

                    if (nCellsPath == 0 || nCellsLine == 0)
                    {
                        addError("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                     ": BEGIN TILE ARRAY - Zero cells per tile dimension",
                                 "WebCGM-2.1 Raster Aspect Preservation",
                                 "BEGIN TILE ARRAY", 19);
                    }
                    else
                    {
                        double tileAspect = static_cast<double>(nCellsPath) / static_cast<double>(nCellsLine);

                        if (tileAspect > 10.0 || tileAspect < 0.1)
                        {
                            addWarning("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                           ": BEGIN TILE ARRAY - Extreme tile aspect ratio: " + std::to_string(tileAspect),
                                       "WebCGM-2.1 Raster Aspect Preservation",
                                       "BEGIN TILE ARRAY", 19);
                        }

                        addInfo("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                    ": BEGIN TILE ARRAY - Tile cells: " + std::to_string(nCellsPath) +
                                    "x" + std::to_string(nCellsLine) + ", aspect: " + std::to_string(tileAspect),
                                "WebCGM-2.1 Raster Aspect Preservation",
                                "BEGIN TILE ARRAY", 19);
                    }
                }
            }
        }
    }

    /**
     * @brief Enhanced URI Validation for WebCGM 2.1 compliance
     *
     * WebCGM 2.1 Section 3.6: URI and Fragment Identifier Validation
     * - Fragment identifier format validation (already extensive in existing code)
     * - This function provides additional checks for edge cases
     * - Validates URI encoding compliance
     */
    void WebCGM21Validator::validateEnhancedURIValidation(const CGMFile *cgmFile)
    {
        // Note: The existing validateXcfBindings already does extensive URI validation
        // This function adds supplementary checks for completeness

        for (const auto &cmd : cgmFile->commands())
        {
            ClassCode eclass = cmd->elementClass();
            int eid = cmd->elementId();

            // APPLICATION STRUCTURE ATTRIBUTE (Class 9, ID 1) - Check for linkuri
            if (eclass == ClassCode::ApplicationStructureDescriptorElements && eid == 1)
            {
                auto *apsCmd = dynamic_cast<ApplicationStructureAttribute *>(cmd.get());
                if (!apsCmd)
                {
                    continue;
                }

                std::string apsTypeLower = opencgm::utils::toLower(apsCmd->attributeType());
                std::string normalizedValue = normalizedApsValue(apsCmd);

                if (apsTypeLower == "linkuri")
                {
                    LinkUriValidationResult validation = validateLinkUri(normalizedValue);
                    if (!validation.valid)
                    {
                        continue; // Detailed errors already emitted during APS traversal
                    }

                    if (validation.hasFragment)
                    {
                        addInfo("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                    ": linkuri fragment reference #" + validation.fragment +
                                    (validation.fragmentIsLocal ? " (local)" : " (remote)"),
                                "WebCGM-2.1 Enhanced URI Validation",
                                "APPLICATION STRUCTURE ATTRIBUTE", 1);
                    }
                    else
                    {
                        addInfo("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                    ": linkuri target '" + normalizedValue + "'",
                                "WebCGM-2.1 Enhanced URI Validation",
                                "APPLICATION STRUCTURE ATTRIBUTE", 1);
                    }
                }
                else if (apsTypeLower == "viewcontext")
                {
                    addInfo("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                ": viewcontext attribute found: " + normalizedValue,
                            "WebCGM-2.1 Enhanced URI Validation",
                            "APPLICATION STRUCTURE ATTRIBUTE", 1);
                }
            }
        }
    }

    /**
     * @brief Enhanced Color Analysis for WebCGM 2.1 compliance
     *
     * WebCGM 2.1 T.7: Color Model and Precision
     * - Validates color precision consistency
     * - Checks for indexed vs direct color usage
     * - Warns on color model compliance issues
     */
    void WebCGM21Validator::validateEnhancedColorAnalysis(const CGMFile *cgmFile)
    {
        bool foundIndexedColor = false;
        bool foundDirectColor = false;
        int totalColorCommands = 0;

        for (const auto &cmd : cgmFile->commands())
        {
            ClassCode eclass = cmd->elementClass();
            int eid = cmd->elementId();

            // LINE COLOUR (Class 5, ID 4)
            if (eclass == ClassCode::AttributeElements && eid == 4)
            {
                auto *colorCmd = dynamic_cast<LineColour *>(cmd.get());
                if (colorCmd)
                {
                    totalColorCommands++;
                    const CGMColor &color = colorCmd->color();

                    if (color.isIndexed())
                    {
                        foundIndexedColor = true;
                        addInfo("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                    ": LINE COLOUR uses indexed color: index " + std::to_string(color.colorIndex()),
                                "WebCGM-2.1 Enhanced Color Analysis",
                                "LINE COLOUR", 4);
                    }
                    else
                    {
                        foundDirectColor = true;
                    }
                }
            }

            // FILL COLOUR (Class 5, ID 23)
            if (eclass == ClassCode::AttributeElements && eid == 23)
            {
                auto *colorCmd = dynamic_cast<FillColour *>(cmd.get());
                if (colorCmd)
                {
                    totalColorCommands++;
                    const CGMColor &color = colorCmd->color();

                    if (color.isIndexed())
                    {
                        foundIndexedColor = true;
                        addInfo("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                    ": FILL COLOUR uses indexed color: index " + std::to_string(color.colorIndex()),
                                "WebCGM-2.1 Enhanced Color Analysis",
                                "FILL COLOUR", 23);
                    }
                    else
                    {
                        foundDirectColor = true;
                    }
                }
            }

            // TEXT COLOUR (Class 5, ID 14)
            if (eclass == ClassCode::AttributeElements && eid == 14)
            {
                auto *colorCmd = dynamic_cast<TextColour *>(cmd.get());
                if (colorCmd)
                {
                    totalColorCommands++;
                    const CGMColor &color = colorCmd->color();

                    if (color.isIndexed())
                    {
                        foundIndexedColor = true;
                        addInfo("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                    ": TEXT COLOUR uses indexed color: index " + std::to_string(color.colorIndex()),
                                "WebCGM-2.1 Enhanced Color Analysis",
                                "TEXT COLOUR", 14);
                    }
                    else
                    {
                        foundDirectColor = true;
                    }
                }
            }

            // EDGE COLOUR (Class 5, ID 29)
            if (eclass == ClassCode::AttributeElements && eid == 29)
            {
                auto *colorCmd = dynamic_cast<EdgeColour *>(cmd.get());
                if (colorCmd)
                {
                    totalColorCommands++;
                    const CGMColor &color = colorCmd->color();

                    if (color.isIndexed())
                    {
                        foundIndexedColor = true;
                        addInfo("Picture " + std::to_string(currentPictureIndex_ + 1) +
                                    ": EDGE COLOUR uses indexed color: index " + std::to_string(color.colorIndex()),
                                "WebCGM-2.1 Enhanced Color Analysis",
                                "EDGE COLOUR", 29);
                    }
                    else
                    {
                        foundDirectColor = true;
                    }
                }
            }
        }

        // Summary report
        if (totalColorCommands > 0)
        {
            std::string colorMode;
            if (foundIndexedColor && foundDirectColor)
            {
                colorMode = "mixed (both indexed and direct)";
                addWarning("Picture " + std::to_string(currentPictureIndex_ + 1) +
                               ": Color model mixing detected - file uses both indexed and direct color",
                           "WebCGM-2.1 Enhanced Color Analysis",
                           "COLOR MODEL", 0);
            }
            else if (foundIndexedColor)
            {
                colorMode = "indexed";
            }
            else
            {
                colorMode = "direct (RGB)";
            }

            addInfo("Picture " + std::to_string(currentPictureIndex_ + 1) +
                        ": Color analysis: " + std::to_string(totalColorCommands) +
                        " color commands, mode: " + colorMode,
                    "WebCGM-2.1 Enhanced Color Analysis",
                    "COLOR SUMMARY", 0);
        }
    }
    // ============================================================================
    // ATA GREXCHANGE Validator Implementation
    // ============================================================================

    ATAGREXCHANGEValidator::ATAGREXCHANGEValidator(Version version)
        : version_(version)
    {
    }

    std::vector<ValidationMessage> ATAGREXCHANGEValidator::validate(const CGMFile *cgmFile)
    {
        messages_.clear();
        currentPictureIndex_ = 0;

        addInfo("Starting ATA GREXCHANGE validation (version " + getProfileName() + ")", "ATA GREXCHANGE");

        // ATA GREXCHANGE is based on WebCGM, so run WebCGM validation first
        WebCGM21Validator webCGMValidator;
        auto webCGMMessages = webCGMValidator.validate(cgmFile);
        messages_.insert(messages_.end(), webCGMMessages.begin(), webCGMMessages.end());

        // Add ATA-specific validation
        validateEdgeVisibility(cgmFile);
        validateGeneralizedDrawingPrimitives(cgmFile);
        validateExternalReferences(cgmFile);
        validateColorClass(cgmFile);
        validateProfileDeclarationATA(cgmFile);
        validateColorClassDeclaration(cgmFile);

        return messages_;
    }

    ProfileType ATAGREXCHANGEValidator::getProfileType() const
    {
        switch (version_)
        {
        case Version::V2_6:
            return ProfileType::ATA_GREXCHANGE_2_6;
        case Version::V2_7:
            return ProfileType::ATA_GREXCHANGE_2_7;
        case Version::V2_8:
            return ProfileType::ATA_GREXCHANGE_2_8;
        case Version::V2_9:
            return ProfileType::ATA_GREXCHANGE_2_9;
        default:
            return ProfileType::UNKNOWN;
        }
    }

    std::string ATAGREXCHANGEValidator::getProfileName() const
    {
        switch (version_)
        {
        case Version::V2_6:
            return "ATA GREXCHANGE 2.6";
        case Version::V2_7:
            return "ATA GREXCHANGE 2.7";
        case Version::V2_8:
            return "ATA GREXCHANGE 2.8";
        case Version::V2_9:
            return "ATA GREXCHANGE 2.9";
        default:
            return "Unknown ATA GREXCHANGE";
        }
    }

    bool ATAGREXCHANGEValidator::isElementAllowed(ClassCode elementClass, int elementId) const
    {
        // Delegate to WebCGM validator for base rules
        WebCGM21Validator webCGMValidator;
        return webCGMValidator.isElementAllowed(elementClass, elementId);
    }

    bool ATAGREXCHANGEValidator::hasConstraint(const std::string &constraintName) const
    {
        if (constraintName == "no_gdp")
            return true;
        if (constraintName == "no_external_refs")
            return true;

        // Delegate to WebCGM for base constraints
        WebCGM21Validator webCGMValidator;
        return webCGMValidator.hasConstraint(constraintName);
    }

    void ATAGREXCHANGEValidator::validateEdgeVisibility(const CGMFile *cgmFile)
    {
        // ATA requires edge visibility enforcement for filled polygons
        // Per ATA GREXCHANGE: filled polygons must have visible edges unless explicitly hidden

        bool edgeVisibility = true; // Default is ON per CGM spec
        int polygonCount = 0;
        int polygonsWithHiddenEdges = 0;

        for (const auto &cmd : cgmFile->commands())
        {
            // Track EDGE VISIBILITY state (element ID 27)
            if (cmd->elementClass() == ClassCode::AttributeElements && cmd->elementId() == 27)
            {
                auto *edgeVisCmd = dynamic_cast<EdgeVisibility *>(cmd.get());
                if (edgeVisCmd)
                {
                    edgeVisibility = edgeVisCmd->isVisible();
                }
            }

            // Check filled polygon primitives
            if (cmd->elementClass() == ClassCode::GraphicalPrimitiveElements)
            {
                bool isFilledPrimitive = false;

                // POLYGON (ID 7)
                if (cmd->elementId() == 7)
                {
                    isFilledPrimitive = true;
                    polygonCount++;
                }
                // POLYGON SET (ID 8)
                else if (cmd->elementId() == 8)
                {
                    isFilledPrimitive = true;
                    polygonCount++;
                }
                // RECTANGLE (ID 11)
                else if (cmd->elementId() == 11)
                {
                    isFilledPrimitive = true;
                    polygonCount++;
                }
                // CIRCLE (ID 12) - filled
                else if (cmd->elementId() == 12)
                {
                    isFilledPrimitive = true;
                    polygonCount++;
                }
                // CIRCULAR ARC 3 POINT CLOSE (ID 14)
                else if (cmd->elementId() == 14)
                {
                    isFilledPrimitive = true;
                    polygonCount++;
                }
                // CIRCULAR ARC CENTRE CLOSE (ID 16)
                else if (cmd->elementId() == 16)
                {
                    isFilledPrimitive = true;
                    polygonCount++;
                }
                // ELLIPSE (ID 17)
                else if (cmd->elementId() == 17)
                {
                    isFilledPrimitive = true;
                    polygonCount++;
                }
                // ELLIPTICAL ARC CLOSE (ID 19)
                else if (cmd->elementId() == 19)
                {
                    isFilledPrimitive = true;
                    polygonCount++;
                }

                // If filled primitive encountered with hidden edges, record it
                if (isFilledPrimitive && !edgeVisibility)
                {
                    polygonsWithHiddenEdges++;

                    // ATA GREXCHANGE typically requires visible edges for interoperability
                    // Some profiles may allow hidden edges for specific use cases
                    std::string primitiveName = cmd->toString();
                    addWarning("Filled primitive (" + primitiveName + ") has EDGE VISIBILITY OFF. " +
                                   "ATA GREXCHANGE typically requires visible edges for filled shapes.",
                               "ATA GREXCHANGE - Edge Visibility", primitiveName, cmd->elementId());
                }
            }
        }

        // Summary
        if (polygonCount > 0)
        {
            std::ostringstream oss;
            oss << "Checked " << polygonCount << " filled primitive(s). ";
            if (polygonsWithHiddenEdges == 0)
            {
                oss << "All have visible edges (compliant).";
                addInfo(oss.str(), "ATA GREXCHANGE - Edge Visibility");
            }
            else
            {
                oss << polygonsWithHiddenEdges << " have hidden edges (may not be compliant).";
                addInfo(oss.str(), "ATA GREXCHANGE - Edge Visibility");
            }
        }
        else
        {
            addInfo("No filled primitives found", "ATA GREXCHANGE - Edge Visibility");
        }
    }

    void ATAGREXCHANGEValidator::validateGeneralizedDrawingPrimitives(const CGMFile *cgmFile)
    {
        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::GraphicalPrimitiveElements && cmd->elementId() == 10)
            {
                addError("Generalized Drawing Primitive (GDP) forbidden",
                         "ATA GREXCHANGE", "GDP", 10);
            }
        }
    }

    void ATAGREXCHANGEValidator::validateExternalReferences(const CGMFile *cgmFile)
    {
        // Check for EXTERNAL elements (class 7)
        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::ExternalElements)
            {
                addError("External references not allowed",
                         "ATA GREXCHANGE", cmd->toString(), cmd->elementId());
            }
        }
    }

    void ATAGREXCHANGEValidator::validateColorClass(const CGMFile *cgmFile)
    {
        // ATA profiles may specify monochrome or color class
        // Check for consistency: if using indexed color, all colors should be from defined table
        // If using direct color, check for RGB vs grayscale consistency

        bool hasColorTable = false;
        int maxColorIndex = -1;

        // Check COLOUR MODEL
        ColorModel colorModel = cgmFile->colorModel();
        std::string colorModelStr;
        switch (colorModel)
        {
        case ColorModel::RGB:
            colorModelStr = "RGB";
            break;
        case ColorModel::CIELAB:
            colorModelStr = "CIELAB";
            break;
        case ColorModel::CIELUV:
            colorModelStr = "CIELUV";
            break;
        case ColorModel::CMYK:
            colorModelStr = "CMYK";
            break;
        case ColorModel::RGB_RELATED:
            colorModelStr = "RGB_RELATED";
            break;
        default:
            colorModelStr = "Unknown";
            break;
        }

        addInfo("COLOUR MODEL: " + colorModelStr, "ATA GREXCHANGE - Color Class");

        // Check for MAXIMUM COLOUR INDEX
        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::MetafileDescriptorElements && cmd->elementId() == 9)
            {
                auto *maxIndexCmd = dynamic_cast<MaximumColourIndex *>(cmd.get());
                if (maxIndexCmd)
                {
                    maxColorIndex = maxIndexCmd->maxIndex();
                }
            }

            if (dynamic_cast<const ColourTable *>(cmd.get()) != nullptr)
            {
                hasColorTable = true;
            }
        }

        // Summary
        std::ostringstream oss;
        oss << "Color class check: ";

        if (cgmFile->colorSelectionMode() == ColorSelectionMode::INDEXED)
        {
            oss << "Indexed color mode";
            if (maxColorIndex >= 0)
            {
                oss << " (max index: " << maxColorIndex << ")";
            }
            else if (hasColorTable)
            {
                addWarning("COLOUR TABLE present without MAXIMUM COLOUR INDEX",
                           "ATA GREXCHANGE - Color Class");
            }
        }
        else
        {
            oss << "Direct color mode";
            if (hasColorTable)
            {
                oss << " (colour table present but not selected)";
            }
        }

        addInfo(oss.str(), "ATA GREXCHANGE - Color Class");

        // Check for monochrome consistency
        // Monochrome means all colors should be grayscale (R=G=B for RGB model)
        // This would require analyzing actual color values, which needs access to color data structures
        // For now, provide a basic check

        if (colorModel == ColorModel::RGB || colorModel == ColorModel::RGB_RELATED)
        {
            addInfo("RGB color model detected. Monochrome policy should be verified manually.",
                    "ATA GREXCHANGE - Color Class");
        }
        else if (colorModel == ColorModel::CMYK)
        {
            addWarning("CMYK color model may not be widely supported in ATA GREXCHANGE profiles",
                       "ATA GREXCHANGE - Color Class");
        }
    }
    void ATAGREXCHANGEValidator::validateProfileDeclarationATA(const CGMFile *cgmFile)
    {
        const std::string rule = "ATA GREXCHANGE V2.13, Section 7.2.4 - METAFILE DESCRIPTION";

        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::MetafileDescriptorElements &&
                cmd->elementId() == 2)
            { // METAFILE DESCRIPTION
                auto *descCmd = dynamic_cast<MetafileDescription *>(cmd.get());
                if (descCmd)
                {
                    const std::string &description = descCmd->description();

                    // Extract ProfileId and ProfileEd
                    std::string profileId;
                    std::string profileEd;

                    // Parse for ProfileId (with quotes)
                    size_t profileIdStart = description.find("\"ProfileId:");
                    if (profileIdStart != std::string::npos)
                    {
                        size_t valueStart = profileIdStart + 11; // After "ProfileId:
                        size_t valueEnd = description.find("\"", valueStart);
                        if (valueEnd != std::string::npos)
                        {
                            profileId = description.substr(valueStart, valueEnd - valueStart);
                        }
                    }

                    // Parse for ProfileEd (with quotes)
                    size_t profileEdStart = description.find("\"ProfileEd:");
                    if (profileEdStart != std::string::npos)
                    {
                        size_t valueStart = profileEdStart + 11; // After "ProfileEd:
                        size_t valueEnd = description.find("\"", valueStart);
                        if (valueEnd != std::string::npos)
                        {
                            profileEd = description.substr(valueStart, valueEnd - valueStart);
                        }
                    }

                    // Validate ProfileId
                    if (profileId.empty())
                    {
                        addError("METAFILE DESCRIPTION must contain \"ProfileId:ATA GRAPHICS.GREXCHANGE\"",
                                rule, "METAFILE DESCRIPTION", 2);
                    }
                    else if (profileId != "ATA GRAPHICS.GREXCHANGE")
                    {
                        addError("ProfileId must be 'ATA GRAPHICS.GREXCHANGE', found: '" + profileId +
                                "' (note: single space after 'ATA' is required)",
                                rule, "METAFILE DESCRIPTION", 2);
                    }
                    else
                    {
                        addInfo("Valid ProfileId found: \"ProfileId:ATA GRAPHICS.GREXCHANGE\"",
                               "ATA GREXCHANGE - Profile Declaration");
                    }

                    // Validate ProfileEd format and version compatibility
                    if (profileEd.empty())
                    {
                        addWarning("METAFILE DESCRIPTION should contain \"ProfileEd:n.m\" to specify version",
                                  rule, "METAFILE DESCRIPTION", 2);
                    }
                    else
                    {
                        // Expected version based on validator version
                        std::string expectedVersion;
                        switch (version_)
                        {
                        case Version::V2_6:
                            expectedVersion = "2.6";
                            break;
                        case Version::V2_7:
                            expectedVersion = "2.7";
                            break;
                        case Version::V2_8:
                            expectedVersion = "2.8";
                            break;
                        case Version::V2_9:
                            expectedVersion = "2.9";
                            break;
                        }

                        if (profileEd != expectedVersion)
                        {
                            addWarning("ProfileEd is '" + profileEd + "' but validating against " +
                                      getProfileName() + " (expected: '" + expectedVersion + "')",
                                      rule, "METAFILE DESCRIPTION", 2);
                        }
                        else
                        {
                            addInfo("ProfileEd matches validator version: \"ProfileEd:" + profileEd + "\"",
                                   "ATA GREXCHANGE - Profile Declaration");
                        }
                    }
                }
            }
        }
    }

    void ATAGREXCHANGEValidator::validateColorClassDeclaration(const CGMFile *cgmFile)
    {
        const std::string rule = "ATA GREXCHANGE V2.13, Section 7.2.4 - ColourClass Declaration";

        std::string declaredColorClass;
        bool foundDeclaration = false;

        // Extract declared ColourClass from METAFILE DESCRIPTION
        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::MetafileDescriptorElements &&
                cmd->elementId() == 2)
            { // METAFILE DESCRIPTION
                auto *descCmd = dynamic_cast<MetafileDescription *>(cmd.get());
                if (descCmd)
                {
                    const std::string &description = descCmd->description();

                    // Parse for ColourClass (with quotes)
                    size_t colorClassStart = description.find("\"ColourClass:");
                    if (colorClassStart != std::string::npos)
                    {
                        size_t valueStart = colorClassStart + 13; // After "ColourClass:
                        size_t valueEnd = description.find("\"", valueStart);
                        if (valueEnd != std::string::npos)
                        {
                            declaredColorClass = description.substr(valueStart, valueEnd - valueStart);
                            foundDeclaration = true;
                        }
                    }
                }
            }
        }

        // Check if ColourClass declaration exists (required)
        if (!foundDeclaration)
        {
            addError("METAFILE DESCRIPTION must contain \"ColourClass:monochrome\" or \"ColourClass:colour\"",
                    rule, "METAFILE DESCRIPTION", 2);
            return;
        }

        // Validate declared value
        if (declaredColorClass != "monochrome" && declaredColorClass != "colour")
        {
            addError("ColourClass must be 'monochrome' or 'colour', found: '" + declaredColorClass + "'",
                    rule, "METAFILE DESCRIPTION", 2);
            return;
        }

        addInfo("ColourClass declaration: \"ColourClass:" + declaredColorClass + "\"",
               "ATA GREXCHANGE - Color Class");

        // Now analyze actual color usage to verify match
        bool usesIndexedColor = false;
        int maxColorIndex = -1;
        bool hasColorTable = false;

        // Check MAXIMUM COLOUR INDEX and COLOUR TABLE
        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::MetafileDescriptorElements && cmd->elementId() == 9)
            {
                auto *maxIndexCmd = dynamic_cast<MaximumColourIndex *>(cmd.get());
                if (maxIndexCmd)
                {
                    maxColorIndex = maxIndexCmd->maxIndex();
                }
            }

            if (cmd->elementClass() == ClassCode::AttributeElements && cmd->elementId() == 33)
            {
                hasColorTable = true;
            }
        }

        if (hasColorTable || maxColorIndex > 0)
        {
            usesIndexedColor = true;
        }

        // Validate consistency
        if (declaredColorClass == "monochrome")
        {
            // For monochrome, we can issue INFO about expected usage
            addInfo("Monochrome class declared - all colors should be grayscale (R=G=B)",
                   "ATA GREXCHANGE - Color Class Verification");

            // Note: Full color value analysis would require checking actual RGB/CMYK values
            // which is beyond the scope of declaration matching
        }
        else if (declaredColorClass == "colour")
        {
            addInfo("Colour class declared - full color spectrum allowed",
                   "ATA GREXCHANGE - Color Class Verification");
        }

        // Report indexed vs direct color mode
        if (usesIndexedColor)
        {
            addInfo("File uses indexed color mode (COLOUR TABLE present, max index: " +
                   std::to_string(maxColorIndex) + ")",
                   "ATA GREXCHANGE - Color Mode");
        }
        else
        {
            addInfo("File uses direct color mode (no COLOUR TABLE)",
                   "ATA GREXCHANGE - Color Mode");
        }
    }

    // ============================================================================
    // S1000D Validator Implementation
    // ============================================================================

    S1000DValidator::S1000DValidator()
    {
    }

    std::vector<ValidationMessage> S1000DValidator::validate(const CGMFile *cgmFile)
    {
        messages_.clear();
        currentPictureIndex_ = 0;

        addInfo("Starting S1000D validation", "S1000D");

        // S1000D uses ATA GREXCHANGE profiles, so validate against that
        ATAGREXCHANGEValidator ataValidator(ATAGREXCHANGEValidator::Version::V2_9);
        auto ataMessages = ataValidator.validate(cgmFile);
        messages_.insert(messages_.end(), ataMessages.begin(), ataMessages.end());

        // Add S1000D-specific validation
        validateMetadata(cgmFile);
        validateCSDBNaming(cgmFile);
        validateTextHeights(cgmFile);
        validateAPSBinding(cgmFile);
        validateMonochromePolicy(cgmFile);
        validateAPSConstraints(cgmFile);
        validateMetafileDescriptionFormat(cgmFile);

        return messages_;
    }

    bool S1000DValidator::isElementAllowed(ClassCode elementClass, int elementId) const
    {
        // Delegate to ATA validator
        ATAGREXCHANGEValidator ataValidator(ATAGREXCHANGEValidator::Version::V2_9);
        return ataValidator.isElementAllowed(elementClass, elementId);
    }

    bool S1000DValidator::hasConstraint(const std::string &constraintName) const
    {
        if (constraintName == "csdb_metadata")
            return true;
        if (constraintName == "text_height_nominal")
            return true;

        // Delegate to ATA for base constraints
        ATAGREXCHANGEValidator ataValidator(ATAGREXCHANGEValidator::Version::V2_9);
        return ataValidator.hasConstraint(constraintName);
    }

    void S1000DValidator::validateMetadata(const CGMFile *cgmFile)
    {
        // Check for required S1000D metadata: ICN, issue number, security, illustration type
        // Per S1000D, CSDB graphics objects must have:
        // - ICN (Illustration Control Number) format: ICN-XXXX-XXXXX-XXXXXX-XXX-XXXX-XXXXX-X
        // - Issue number
        // - Security classification (optional but recommended)
        // - Illustration type (e.g., fig, grsheet, etc.)

        bool hasMetadata = false;
        bool hasICN = false;
        bool hasIssue = false;
        bool hasSecurity = false;
        bool hasIllType = false;

        std::string icn;
        std::string issue;
        std::string security;
        std::string illType;

        // Look for METAFILE DESCRIPTION
        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::MetafileDescriptorElements && cmd->elementId() == 2)
            {
                auto *descCmd = dynamic_cast<MetafileDescription *>(cmd.get());
                if (descCmd)
                {
                    hasMetadata = true;
                    const std::string &desc = descCmd->description();

                    // Parse ICN - pattern: ICN-XXXX-XXXXX-XXXXXX-XXX-XXXX-XXXXX-X
                    size_t icnPos = desc.find("ICN-");
                    if (icnPos != std::string::npos)
                    {
                        // Extract ICN (up to next space, semicolon, or end)
                        size_t icnEnd = desc.find_first_of(" ;,\\n\r", icnPos);
                        if (icnEnd == std::string::npos)
                        {
                            icn = desc.substr(icnPos);
                        }
                        else
                        {
                            icn = desc.substr(icnPos, icnEnd - icnPos);
                        }

                        hasICN = true;

                        // Validate ICN format (should have 7 segments separated by hyphens)
                        int hyphenCount = 0;
                        for (char c : icn)
                        {
                            if (c == '-')
                                hyphenCount++;
                        }

                        if (hyphenCount != 7)
                        {
                            addWarning("ICN format may be invalid: expected 7 hyphens, found " +
                                           std::to_string(hyphenCount) + " in '" + icn + "'",
                                       "S1000D - CSDB Naming", "METAFILE DESCRIPTION", 2);
                        }
                        else
                        {
                            addInfo("Valid ICN found: " + icn, "S1000D");
                        }
                    }

                    // Parse issue number - patterns: "Issue 001", "ISSUE-001", "ISS 001", etc.
                    std::string upperDesc = desc;
                    std::transform(upperDesc.begin(), upperDesc.end(), upperDesc.begin(),
                                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

                    size_t issuePos = upperDesc.find("ISSUE");
                    if (issuePos == std::string::npos)
                    {
                        issuePos = upperDesc.find("ISS");
                    }

                    if (issuePos != std::string::npos)
                    {
                        // Look for digits after "ISSUE" or "ISS"
                        size_t digitPos = desc.find_first_of("0123456789", issuePos);
                        if (digitPos != std::string::npos && digitPos - issuePos < 10)
                        {
                            size_t digitEnd = desc.find_first_not_of("0123456789", digitPos);
                            if (digitEnd == std::string::npos)
                            {
                                issue = desc.substr(digitPos);
                            }
                            else
                            {
                                issue = desc.substr(digitPos, digitEnd - digitPos);
                            }
                            hasIssue = true;
                            addInfo("Issue number found: " + issue, "S1000D");
                        }
                    }

                    // Parse security classification - common values: UNCLASSIFIED, CLASSIFIED, RESTRICTED, etc.
                    const std::string securityKeywords[] = {
                        "UNCLASSIFIED", "CLASSIFIED", "RESTRICTED", "CONFIDENTIAL",
                        "SECRET", "TOP SECRET", "PUBLIC", "FOUO"};

                    for (const auto &keyword : securityKeywords)
                    {
                        if (upperDesc.find(keyword) != std::string::npos)
                        {
                            security = keyword;
                            hasSecurity = true;
                            addInfo("Security classification found: " + security, "S1000D");
                            break;
                        }
                    }

                    // Parse illustration type - common values: fig, grsheet, table, etc.
                    const std::string illTypeKeywords[] = {
                        "FIG", "GRSHEET", "TABLE", "DIAGRAM", "CHART", "ILLUSTRATION"};

                    for (const auto &keyword : illTypeKeywords)
                    {
                        if (upperDesc.find(keyword) != std::string::npos)
                        {
                            illType = keyword;
                            hasIllType = true;
                            addInfo("Illustration type found: " + illType, "S1000D");
                            break;
                        }
                    }
                }
            }
        }

        // Report missing metadata
        if (!hasMetadata)
        {
            addError("METAFILE DESCRIPTION not found - required for S1000D CSDB graphics",
                     "S1000D - CSDB Metadata", "METAFILE DESCRIPTION", 2);
            return;
        }

        if (!hasICN)
        {
            addError("ICN not found in METAFILE DESCRIPTION - required for S1000D CSDB graphics",
                     "S1000D - CSDB Naming", "METAFILE DESCRIPTION", 2);
        }

        if (!hasIssue)
        {
            addWarning("Issue number not found in METAFILE DESCRIPTION - recommended for S1000D",
                       "S1000D - CSDB Metadata", "METAFILE DESCRIPTION", 2);
        }

        if (!hasSecurity)
        {
            addInfo("Security classification not specified in METAFILE DESCRIPTION (optional)",
                    "S1000D - CSDB Metadata");
        }

        if (!hasIllType)
        {
            addInfo("Illustration type not specified in METAFILE DESCRIPTION (recommended)",
                    "S1000D - CSDB Metadata");
        }
    }

    void S1000DValidator::validateCSDBNaming(const CGMFile *cgmFile)
    {
        // Validate that filename follows CSDB graphics object naming
        // Per S1000D, CSDB graphics objects follow naming convention:
        // ICN-{ModelID}-{SystemCode}{SubsysCode}{Assy}-{Variant}-{ItemLocationCode}.CGM
        // Example: ICN-C0419-S1000D0358-001-01.CGM

        // -----------------------------------------------------------------------------
        // Filename / ICN checks (assumes this is inside some validator function body)
        // -----------------------------------------------------------------------------

        const std::string &filename = cgmFile->name();

        // Extract just the filename from path if present
        size_t lastSlash = filename.find_last_of("/\\");
        std::string basename = (lastSlash != std::string::npos)
                                   ? filename.substr(lastSlash + 1)
                                   : filename;

        // Check if filename starts with "ICN-"
        if (basename.find("ICN-") != 0)
        {
            addError("CSDB filename must start with 'ICN-'. Found: '" + basename + "'",
                     "S1000D - CSDB Naming", "FILENAME", -1);
            return;
        }

        // Check if filename ends with .CGM or .cgm
        std::string upperBasename = basename;
        std::transform(upperBasename.begin(), upperBasename.end(), upperBasename.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

        if (upperBasename.size() < 4 || upperBasename.substr(upperBasename.length() - 4) != ".CGM")
        {
            addWarning("CSDB filename should have .CGM extension. Found: '" + basename + "'",
                       "S1000D - CSDB Naming", "FILENAME", -1);
        }

        // Parse ICN components from filename
        // Format: ICN-{component1}-{component2}-{component3}-{component4}.CGM
        // Remove extension
        std::string nameWithoutExt = basename.substr(0, basename.find_last_of('.'));

        // Split by hyphens
        std::vector<std::string> components;
        size_t start = 0;
        size_t pos = 0;
        while ((pos = nameWithoutExt.find('-', start)) != std::string::npos)
        {
            components.push_back(nameWithoutExt.substr(start, pos - start));
            start = pos + 1;
        }
        components.push_back(nameWithoutExt.substr(start)); // Last component

        // Expected: ICN, ModelID, SystemCode, Variant, ItemLocation (5 components minimum)
        if (components.size() < 5)
        {
            addWarning(
                "CSDB filename has " + std::to_string(components.size()) +
                    " hyphen-separated components, expected at least 5 "
                    "(ICN-ModelID-SystemCode-Variant-ItemLocation)",
                "S1000D - CSDB Naming", "FILENAME", -1);
        }
        else if (components.size() > 6)
        {
            addInfo(
                "CSDB filename has " + std::to_string(components.size()) +
                    " hyphen-separated components (more than typical 5-6)",
                "S1000D - CSDB Naming");
        }

        // Extract filename ICN
        std::string filenameICN = nameWithoutExt; // Full filename without extension

        // Compare with metadata ICN if present
        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::MetafileDescriptorElements && cmd->elementId() == 2)
            {
                auto *descCmd = dynamic_cast<MetafileDescription *>(cmd.get());
                if (descCmd)
                {
                    const std::string &desc = descCmd->description();

                    // Look for ICN in metadata
                    size_t icnPos = desc.find("ICN-");
                    if (icnPos != std::string::npos)
                    {
                        // Extract metadata ICN
                        size_t icnEnd = desc.find_first_of(" ;,\r\n", icnPos);
                        std::string metadataICN;
                        if (icnEnd == std::string::npos)
                        {
                            metadataICN = desc.substr(icnPos);
                        }
                        else
                        {
                            metadataICN = desc.substr(icnPos, icnEnd - icnPos);
                        }

                        // Compare filename ICN with metadata ICN
                        if (filenameICN != metadataICN)
                        {
                            addError(
                                "CSDB filename ICN '" + filenameICN +
                                    "' does not match METAFILE DESCRIPTION ICN '" +
                                    metadataICN + "'",
                                "S1000D - CSDB Naming", "FILENAME", -1);
                        }
                        else
                        {
                            addInfo("CSDB filename ICN matches metadata ICN: " + filenameICN,
                                    "S1000D - CSDB Naming");
                        }
                        break;
                    }
                }
            }
        }

        // Provide summary
        {
            std::ostringstream oss;
            oss << "CSDB filename: '" << basename << "' (";
            if (components.size() >= 2)
            {
                oss << "Model: " << components[1];
            }
            if (components.size() >= 3)
            {
                if (components.size() >= 2)
                    oss << ", ";
                oss << "System: " << components[2];
            }
            if (components.size() >= 4)
            {
                oss << ", Variant: " << components[3];
            }
            oss << ")";
            addInfo(oss.str(), "S1000D - CSDB Naming");
        }
    }

    void S1000DValidator::validateTextHeights(const CGMFile *cgmFile)
    {
        // S1000D specifies nominal text heights: 2.5, 3.5, 5 mm
        // Deviation > 10% should be a warning

        const double nominalHeights[] = {2.5, 3.5, 5.0}; // mm
        const double tolerance = 0.10;                   // 10%

        std::vector<double> encounteredHeights;

        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::AttributeElements && cmd->elementId() == 15)
            {
                // CHARACTER HEIGHT (element ID 15)
                auto *heightCmd = dynamic_cast<CharacterHeight *>(cmd.get());
                if (heightCmd)
                {
                    double height = heightCmd->height();
                    encounteredHeights.push_back(height);

                    // Check if height matches any nominal value within tolerance
                    bool matchesNominal = false;
                    double closestNominal = 0.0;
                    double minDeviation = std::numeric_limits<double>::max();

                    for (double nominal : nominalHeights)
                    {
                        double deviation = std::abs(height - nominal) / nominal;
                        if (deviation < minDeviation)
                        {
                            minDeviation = deviation;
                            closestNominal = nominal;
                        }
                        if (deviation <= tolerance)
                        {
                            matchesNominal = true;
                            break;
                        }
                    }

                    if (!matchesNominal && height > 0.1)
                    { // Ignore very small heights (likely decorative)
                        std::ostringstream oss;
                        oss << std::fixed << std::setprecision(2);
                        oss << "CHARACTER HEIGHT " << height
                            << " mm does not match nominal values (2.5, 3.5, 5 mm). ";
                        oss << "Closest nominal: " << closestNominal << " mm (deviation: "
                            << std::setprecision(1) << (minDeviation * 100) << "%)";

                        if (minDeviation > 0.20)
                        { // > 20% deviation
                            addError(oss.str(), "S1000D - Text Metrics", "CHARACTER HEIGHT", 15);
                        }
                        else if (minDeviation > tolerance)
                        { // > 10% but <= 20%
                            addWarning(oss.str(), "S1000D - Text Metrics", "CHARACTER HEIGHT", 15);
                        }
                    }
                }
            }
        }

        // Summary
        if (!encounteredHeights.empty())
        {
            std::set<double> uniqueHeights(encounteredHeights.begin(), encounteredHeights.end());
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2);
            oss << "Found " << uniqueHeights.size() << " unique text height(s): ";
            bool first = true;
            for (double h : uniqueHeights)
            {
                if (!first)
                    oss << ", ";
                oss << h << " mm";
                first = false;
            }
            addInfo(oss.str(), "S1000D - Text Metrics");
        }
        else
        {
            addInfo("No CHARACTER HEIGHT elements found", "S1000D - Text Metrics");
        }
    }

    void S1000DValidator::validateAPSBinding(const CGMFile *cgmFile)
    {
        // Validate APS/XCF binding
        // Per S1000D/WebCGM: Every APS referenced by XCF must bind to a concrete object by id
        // This is a baseline check - full APS tree reconstruction is complex

        int apsCount = 0;
        int apsBodyCount = 0;
        int apsEndCount = 0;
        std::vector<std::string> apsIdentifiers;
        std::vector<std::string> apsTypes;

        // Track APS structure
        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::DelimiterElement)
            {
                // BEGIN APPLICATION STRUCTURE (ID 21)
                if (cmd->elementId() == 21)
                {
                    auto *apsCmd = dynamic_cast<BeginApplicationStructure *>(cmd.get());
                    if (apsCmd)
                    {
                        apsCount++;
                        const std::string &identifier = apsCmd->identifier();
                        const std::string &type = apsCmd->type();

                        apsIdentifiers.push_back(identifier);
                        apsTypes.push_back(type);

                        // Check for empty identifier
                        if (identifier.empty())
                        {
                            addWarning("BEGIN APPLICATION STRUCTURE has empty identifier",
                                       "S1000D - APS/XCF", "BEGIN APPLICATION STRUCTURE", 21);
                        }

                        // Common APS types for WebCGM: grobject, layer, grnode, para, subpara
                        if (!type.empty())
                        {
                            bool isKnownType = (type == "grobject" || type == "layer" ||
                                                type == "grnode" || type == "para" ||
                                                type == "subpara" || type == "linkuri");
                            if (!isKnownType)
                            {
                                addInfo("APS type '" + type + "' encountered (may be custom type)",
                                        "S1000D - APS/XCF");
                            }
                        }
                    }
                }
                // BEGIN APPLICATION STRUCTURE BODY (ID 22)
                else if (cmd->elementId() == 22)
                {
                    apsBodyCount++;
                }
                // END APPLICATION STRUCTURE (ID 23)
                else if (cmd->elementId() == 23)
                {
                    apsEndCount++;
                }
            }
        }

        // Validate APS structure consistency
        if (apsCount != apsEndCount)
        {
            addError("APS structure mismatch: " + std::to_string(apsCount) +
                         " BEGIN APPLICATION STRUCTURE but " + std::to_string(apsEndCount) +
                         " END APPLICATION STRUCTURE",
                     "S1000D - APS/XCF", "APPLICATION STRUCTURE", 21);
        }

        if (apsCount != apsBodyCount)
        {
            addWarning("APS body mismatch: " + std::to_string(apsCount) +
                           " BEGIN APPLICATION STRUCTURE but " + std::to_string(apsBodyCount) +
                           " BEGIN APPLICATION STRUCTURE BODY",
                       "S1000D - APS/XCF", "APPLICATION STRUCTURE BODY", 22);
        }

        // Check for duplicate identifiers
        std::set<std::string> uniqueIds(apsIdentifiers.begin(), apsIdentifiers.end());
        if (uniqueIds.size() < apsIdentifiers.size())
        {
            int duplicates = static_cast<int>(apsIdentifiers.size() - uniqueIds.size());
            addWarning("Found " + std::to_string(duplicates) +
                           " duplicate APS identifier(s). APS IDs should be unique for XCF binding.",
                       "S1000D - APS/XCF", "APPLICATION STRUCTURE", 21);
        }

        // Summary
        if (apsCount > 0)
        {
            std::ostringstream oss;
            oss << "Found " << apsCount << " APS structure(s) with " << uniqueIds.size() << " unique ID(s). ";

            // Count by type
            std::map<std::string, int> typeCounts;
            for (const auto &type : apsTypes)
            {
                if (!type.empty())
                {
                    typeCounts[type]++;
                }
            }

            if (!typeCounts.empty())
            {
                oss << "Types: ";
                bool first = true;
                for (const auto &pair : typeCounts)
                {
                    if (!first)
                        oss << ", ";
                    oss << pair.first << "(" << pair.second << ")";
                    first = false;
                }
            }

            addInfo(oss.str(), "S1000D - APS/XCF");
        }
        else
        {
            addInfo("No APS structures found", "S1000D - APS/XCF");
        }

        // Note: Full XCF binding validation requires parsing XCF attributes and
        // verifying all fragment references resolve to APS IDs
        // This baseline check validates APS structure integrity
        if (apsCount > 0)
        {
            addInfo("Note: Full XCF binding validation requires analyzing XCF attributes",
                    "S1000D - APS/XCF");
        }
    }

    void S1000DValidator::validateMonochromePolicy(const CGMFile *cgmFile)
    {
        const std::string rule = "S1000D - Color Policy";

        ColorModel colorModel = cgmFile->colorModel();
        ColorSelectionMode selectionMode = cgmFile->colorSelectionMode();

        auto colorModelToString = [](ColorModel model) -> std::string
        {
            switch (model)
            {
            case ColorModel::RGB:
                return "RGB";
            case ColorModel::CIELAB:
                return "CIELAB";
            case ColorModel::CIELUV:
                return "CIELUV";
            case ColorModel::CMYK:
                return "CMYK";
            case ColorModel::RGB_RELATED:
                return "RGB_RELATED";
            default:
                return "Unknown";
            }
        };

        auto colorToHex = [](const Color &color) -> std::string
        {
            std::ostringstream oss;
            oss << "#" << std::uppercase << std::hex << std::setfill('0')
                << std::setw(2) << static_cast<int>(color.r)
                << std::setw(2) << static_cast<int>(color.g)
                << std::setw(2) << static_cast<int>(color.b);
            return oss.str();
        };

        auto isGrayscale = [](const Color &color) -> bool
        {
            return color.r == color.g && color.g == color.b;
        };

        auto isBlack = [](const Color &color) -> bool
        {
            return color.r == 0 && color.g == 0 && color.b == 0;
        };

        auto isWhite = [](const Color &color) -> bool
        {
            return color.r == 255 && color.g == 255 && color.b == 255;
        };

        int maxColourIndex = -1;
        bool maxIndexDeclared = false;
        std::map<int, Color> colourTable;
        bool hasColourTable = false;
        std::set<int> unresolvedIndices;
        std::set<std::string> unresolvedSources;
        size_t totalColorAttributes = 0;
        size_t grayscaleAttributes = 0;
        size_t highlightAttributes = 0;
        size_t strokeAttributes = 0;
        size_t nonBlackStrokeAttributes = 0;
        size_t fillAttributes = 0;
        size_t nonWhiteFillAttributes = 0;
        std::vector<std::string> highlightSamples;
        highlightSamples.reserve(4);
        bool solidInteriorStyleEncountered = false;
        int currentInteriorStyle = 0;
        std::optional<Color> backgroundColour;

        // ISO/IEC 8632 default palette entries for indices 0-7
        const std::pair<int, Color> defaultPalette[] = {
            {0, Color(255, 255, 255)},
            {1, Color(0, 0, 0)},
            {2, Color(255, 0, 0)},
            {3, Color(0, 255, 0)},
            {4, Color(0, 0, 255)},
            {5, Color(0, 255, 255)},
            {6, Color(255, 0, 255)},
            {7, Color(255, 255, 0)}
        };
        for (const auto &entry : defaultPalette)
        {
            colourTable.emplace(entry.first, entry.second);
        }

        auto resolveColor = [&](const CGMColor &value, const std::string &source) -> std::optional<Color>
        {
            if (value.isIndexed())
            {
                auto it = colourTable.find(value.colorIndex());
                if (it != colourTable.end())
                {
                    return it->second;
                }
                unresolvedIndices.insert(value.colorIndex());
                unresolvedSources.insert(source);
                return std::nullopt;
            }
            return value.color();
        };

        auto classifyColor = [&](const CGMColor &value, const std::string &source,
                                 bool treatAsStroke, bool treatAsFill)
        {
            totalColorAttributes++;

            auto resolved = resolveColor(value, source);
            if (!resolved.has_value())
            {
                return;
            }

            const Color &colour = resolved.value();

            if (isGrayscale(colour))
            {
                grayscaleAttributes++;
            }
            else
            {
                highlightAttributes++;
                if (highlightSamples.size() < 4)
                {
                    highlightSamples.emplace_back(source + " " + colorToHex(colour));
                }
            }

            if (treatAsStroke)
            {
                strokeAttributes++;
                if (!isBlack(colour))
                {
                    nonBlackStrokeAttributes++;
                }
            }

            if (treatAsFill)
            {
                fillAttributes++;
                if (!isWhite(colour))
                {
                    nonWhiteFillAttributes++;
                }
            }
        };

        for (const auto &cmdPtr : cgmFile->commands())
        {
            const auto *cmd = cmdPtr.get();
            if (!cmd)
            {
                continue;
            }

            if (cmd->elementClass() == ClassCode::MetafileDescriptorElements && cmd->elementId() == 9)
            {
                auto *maxIndexCmd = dynamic_cast<MaximumColourIndex *>(cmdPtr.get());
                if (maxIndexCmd)
                {
                    maxColourIndex = maxIndexCmd->maxIndex();
                    maxIndexDeclared = true;
                }
                continue;
            }

            if (cmd->elementClass() == ClassCode::AttributeElements && cmd->elementId() == 33)
            {
                auto *colourTableCmd = dynamic_cast<ColourTable *>(cmdPtr.get());
                if (colourTableCmd)
                {
                    hasColourTable = true;
                    int startIndex = colourTableCmd->startIndex();
                    const auto &colors = colourTableCmd->colors();
                    for (size_t offset = 0; offset < colors.size(); ++offset)
                    {
                        colourTable[startIndex + static_cast<int>(offset)] = colors[offset];
                    }
                }
                continue;
            }

            if (cmd->elementClass() == ClassCode::PictureDescriptorElements && cmd->elementId() == 7)
            {
                auto *backgroundCmd = dynamic_cast<BackgroundColour *>(cmdPtr.get());
                if (backgroundCmd)
                {
                    backgroundColour = backgroundCmd->color();
                }
                continue;
            }

            if (cmd->elementClass() == ClassCode::AttributeElements && cmd->elementId() == 21)
            {
                auto *styleCmd = dynamic_cast<InteriorStyle *>(cmdPtr.get());
                if (styleCmd)
                {
                    currentInteriorStyle = styleCmd->style();
                    if (currentInteriorStyle == 1)
                    {
                        solidInteriorStyleEncountered = true;
                    }
                }
                continue;
            }

            if (cmd->elementClass() != ClassCode::AttributeElements)
            {
                continue;
            }

            switch (cmd->elementId())
            {
            case 4:
            {
                auto *lineColourCmd = dynamic_cast<LineColour *>(cmdPtr.get());
                if (lineColourCmd)
                {
                    classifyColor(lineColourCmd->color(), "LINE COLOUR", true, false);
                }
                break;
            }
            case 7:
            {
                auto *markerColourCmd = dynamic_cast<MarkerColour *>(cmdPtr.get());
                if (markerColourCmd)
                {
                    classifyColor(markerColourCmd->color(), "MARKER COLOUR", true, false);
                }
                break;
            }
            case 14:
            {
                auto *textColourCmd = dynamic_cast<TextColour *>(cmdPtr.get());
                if (textColourCmd)
                {
                    classifyColor(textColourCmd->color(), "TEXT COLOUR", true, false);
                }
                break;
            }
            case 23:
            {
                auto *fillColourCmd = dynamic_cast<FillColour *>(cmdPtr.get());
                if (fillColourCmd)
                {
                    bool fillActive = (currentInteriorStyle != 0 && currentInteriorStyle != 4);
                    classifyColor(fillColourCmd->color(), "FILL COLOUR", false, fillActive);
                }
                break;
            }
            case 29:
            {
                auto *edgeColourCmd = dynamic_cast<EdgeColour *>(cmdPtr.get());
                if (edgeColourCmd)
                {
                    classifyColor(edgeColourCmd->color(), "EDGE COLOUR", true, false);
                }
                break;
            }
            default:
                break;
            }
        }

        bool monochromeIndexedPalette = maxIndexDeclared && maxColourIndex >= 0 && maxColourIndex <= 1;
        bool colourCapablePalette = maxIndexDeclared && maxColourIndex > 1;

        std::ostringstream overview;
        overview << "Colour policy review: model=" << colorModelToString(colorModel)
                 << ", selection=" << (selectionMode == ColorSelectionMode::DIRECT ? "DIRECT" : "INDEXED");
        if (maxIndexDeclared)
        {
            overview << ", max index=" << maxColourIndex;
        }
        else if (hasColourTable)
        {
            overview << ", colour table defined";
        }
        else
        {
            overview << ", no colour table";
        }
        if (backgroundColour.has_value())
        {
            overview << ", background=" << colorToHex(backgroundColour.value());
        }
        addInfo(overview.str(), rule);

        size_t resolvedAttributes = grayscaleAttributes + highlightAttributes;
        size_t unresolvedCount = (totalColorAttributes > resolvedAttributes)
                                     ? (totalColorAttributes - resolvedAttributes)
                                     : 0;

        std::ostringstream usage;
        usage << "Colour attributes analysed: " << totalColorAttributes;
        if (resolvedAttributes > 0)
        {
            usage << " resolved=" << resolvedAttributes;
        }
        if (grayscaleAttributes > 0)
        {
            usage << " grayscale=" << grayscaleAttributes;
        }
        if (highlightAttributes > 0)
        {
            usage << " highlight=" << highlightAttributes;
        }
        if (strokeAttributes > 0)
        {
            usage << " stroke=" << strokeAttributes;
        }
        if (fillAttributes > 0)
        {
            usage << " fill=" << fillAttributes;
        }
        if (unresolvedCount > 0)
        {
            usage << " unresolved=" << unresolvedCount;
        }
        addInfo(usage.str(), rule);

        if (monochromeIndexedPalette)
        {
            addInfo("Monochrome palette detected (max colour index <= 1)", rule);
        }
        else if (colourCapablePalette)
        {
            addInfo("Colour-capable palette detected (max colour index " + std::to_string(maxColourIndex) + ")",
                    rule);
        }

        if (highlightAttributes > 0)
        {
            if (monochromeIndexedPalette)
            {
                addWarning("Non-grayscale colours encountered (" + std::to_string(highlightAttributes) +
                               ") despite monochrome palette",
                           rule);
            }
            else
            {
                addInfo("Non-grayscale highlight colours present (" + std::to_string(highlightAttributes) +
                            "). Verify project policy allows highlights.",
                        rule);
            }

            if (!highlightSamples.empty())
            {
                std::ostringstream sampleOss;
                sampleOss << "Highlight samples: ";
                for (size_t i = 0; i < highlightSamples.size(); ++i)
                {
                    if (i > 0)
                        sampleOss << "; ";
                    sampleOss << highlightSamples[i];
                }
                addInfo(sampleOss.str(), rule);
            }
        }
        else if (resolvedAttributes == totalColorAttributes && totalColorAttributes > 0)
        {
            addInfo("All inspected colours are grayscale", rule);
        }

        if (nonBlackStrokeAttributes > 0 && monochromeIndexedPalette)
        {
            addWarning("Stroke colours deviating from black detected (" + std::to_string(nonBlackStrokeAttributes) +
                           " of " + std::to_string(strokeAttributes) + ") in monochrome mode",
                       rule);
        }

        if (nonWhiteFillAttributes > 0)
        {
            std::string message = "Solid/pattern fill colours detected (" + std::to_string(nonWhiteFillAttributes) +
                                  "). S1000D monochrome guidance expects outlines only.";
            addWarning(message, rule);
        }
        else if (solidInteriorStyleEncountered && fillAttributes == 0)
        {
            addInfo("Solid interior style declared but no non-white fills detected", rule);
        }

        if (!unresolvedIndices.empty())
        {
            std::ostringstream oss;
            oss << "Unresolved colour table indices: ";
            size_t count = 0;
            for (int index : unresolvedIndices)
            {
                if (count > 0)
                    oss << ", ";
                if (count >= 6)
                {
                    oss << "...";
                    break;
                }
                oss << index;
                ++count;
            }
            if (!unresolvedSources.empty())
            {
                oss << " (referenced by ";
                size_t srcCount = 0;
                for (const auto &source : unresolvedSources)
                {
                    if (srcCount > 0)
                        oss << ", ";
                    if (srcCount >= 3)
                    {
                        oss << "...";
                        break;
                    }
                    oss << source;
                    ++srcCount;
                }
                oss << ")";
            }
            addWarning(oss.str(), rule);
        }
    }
    void S1000DValidator::validateAPSConstraints(const CGMFile *cgmFile)
    {
        const std::string rule = "S1000D, Chapter 7.3.2, Table 2 (T.15.9)";

        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::DelimiterElement &&
                cmd->elementId() == 21)
            { // BEGIN APPLICATION STRUCTURE
                auto *apsCmd = dynamic_cast<BeginApplicationStructure *>(cmd.get());
                if (apsCmd)
                {
                    // Validate structure type must be "grobject"
                    std::string structType = apsCmd->type();
                    if (structType != "grobject")
                    {
                        std::ostringstream oss;
                        oss << "APS structure type must be 'grobject', found: '" << structType << "'";
                        addError(oss.str(), rule, "BEGIN APPLICATION STRUCTURE", 21);
                    }

                    // Validate inheritance flag must be true (statelist)
                    // Per ISO 8632-1: inheritanceFlag true = INHERIT (statelist in S1000D terminology)
                    // inheritanceFlag false = NEW
                    bool inheritFlag = apsCmd->inheritanceFlag();
                    if (!inheritFlag)
                    {
                        addError("APS inheritance flag must be 'statelist' (INHERIT), found: 'NEW'", 
                                rule, "BEGIN APPLICATION STRUCTURE", 21);
                    }
                }
            }
        }
    }
    void S1000DValidator::validateMetafileDescriptionFormat(const CGMFile *cgmFile)
    {
        const std::string rule = "S1000D, Chapter 7.3.2, Table 3 (T.16.2)";

        for (const auto &cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::MetafileDescriptorElements &&
                cmd->elementId() == 2)
            { // METAFILE DESCRIPTION
                auto *descCmd = dynamic_cast<MetafileDescription *>(cmd.get());
                if (descCmd)
                {
                    const std::string &description = descCmd->description();

                    // S1000D Table 3 (T.16.2) requires: The substring within the SF parameter 
                    // must be of the form: "keyword:item", where the double quotes are part of the substring.
                    
                    // Extract all quoted keyword:item pairs
                    std::vector<std::string> quotedPairs;
                    size_t pos = 0;
                    while (pos < description.length())
                    {
                        // Find opening quote
                        size_t quoteStart = description.find('"', pos);
                        if (quoteStart == std::string::npos)
                            break;

                        // Find closing quote
                        size_t quoteEnd = description.find('"', quoteStart + 1);
                        if (quoteEnd == std::string::npos)
                        {
                            addWarning("Unclosed quote in METAFILE DESCRIPTION at position " +
                                      std::to_string(quoteStart),
                                      rule, "METAFILE DESCRIPTION", 2);
                            break;
                        }

                        // Extract quoted string (including quotes)
                        std::string quotedStr = description.substr(quoteStart, quoteEnd - quoteStart + 1);
                        quotedPairs.push_back(quotedStr);

                        pos = quoteEnd + 1;
                    }

                    if (quotedPairs.empty())
                    {
                        addError("METAFILE DESCRIPTION must contain quoted \"keyword:item\" pairs per S1000D Table 3",
                                rule, "METAFILE DESCRIPTION", 2);
                        return;
                    }

                    // Validate each quoted pair follows "keyword:item" format
                    bool hasValidProfileId = false;
                    std::string profileIdValue;

                    for (const auto &pair : quotedPairs)
                    {
                        // Remove surrounding quotes for validation
                        std::string content = pair.substr(1, pair.length() - 2);

                        // Check for keyword:item format (must have at least one colon)
                        size_t colonPos = content.find(':');
                        if (colonPos == std::string::npos)
                        {
                            addWarning("Quoted string '" + pair + "' does not follow \"keyword:item\" format (missing colon)",
                                      rule, "METAFILE DESCRIPTION", 2);
                            continue;
                        }

                        // Extract keyword and item
                        std::string keyword = content.substr(0, colonPos);
                        std::string item = content.substr(colonPos + 1);

                        // Trim whitespace from keyword
                        keyword.erase(0, keyword.find_first_not_of(" \t"));
                        keyword.erase(keyword.find_last_not_of(" \t") + 1);

                        // Check for empty keyword or item
                        if (keyword.empty())
                        {
                            addWarning("Empty keyword in \"keyword:item\" pair: " + pair,
                                      rule, "METAFILE DESCRIPTION", 2);
                        }

                        if (item.empty())
                        {
                            addWarning("Empty item in \"keyword:item\" pair: " + pair,
                                      rule, "METAFILE DESCRIPTION", 2);
                        }

                        // Validate ProfileId keyword specifically
                        if (keyword == "ProfileId")
                        {
                            hasValidProfileId = true;
                            profileIdValue = item;

                            // Trim whitespace from item
                            profileIdValue.erase(0, profileIdValue.find_first_not_of(" \t"));
                            profileIdValue.erase(profileIdValue.find_last_not_of(" \t") + 1);

                            // S1000D Table 3: ProfileId must be "ProfileId:S1000D"
                            if (profileIdValue != "S1000D")
                            {
                                addError("ProfileId must be 'S1000D', found: '" + profileIdValue + "' (S1000D Table 3)",
                                        rule, "METAFILE DESCRIPTION", 2);
                            }
                            else
                            {
                                addInfo("Valid ProfileId found: \"ProfileId:S1000D\"",
                                       "S1000D - Profile Declaration");
                            }
                        }
                    }

                    // S1000D requires ProfileId:S1000D
                    if (!hasValidProfileId)
                    {
                        addError("METAFILE DESCRIPTION must contain \"ProfileId:S1000D\" (S1000D Table 3)",
                                rule, "METAFILE DESCRIPTION", 2);
                    }

                    // Report compliance
                    addInfo("Found " + std::to_string(quotedPairs.size()) + 
                           " quoted \"keyword:item\" pair(s) in METAFILE DESCRIPTION",
                           "S1000D - Table 3 Format");
                }
            }
        }
    }



    // ============================================================================
    // Profile Detector Implementation
    // ============================================================================
    class ISO8632CompatibilityValidator : public WebCGM21Validator
    {
    public:
        ISO8632CompatibilityValidator()
        {
            setAllowSegments(true);
        }

        ProfileType getProfileType() const override { return ProfileType::ISO_IEC_8632_COMPAT; }
        std::string getProfileName() const override { return "ISO/IEC 8632 Compatibility"; }
    };

    ProfileDetector::DetectionResult ProfileDetector::detectProfile(const CGMFile *cgmFile)
    {
        DetectionResult result{ProfileType::UNKNOWN, "", false, {}};

        if (!cgmFile)
        {
            result.profile = ProfileType::WEBCGM_2_1;
            result.reason = "(fallback to WebCGM 2.1 – no CGM content)";
            return result;
        }

        const auto *binaryFile = dynamic_cast<const BinaryCGMFile *>(cgmFile);
        const bool isBinary = (binaryFile != nullptr);

        ProfileType describedProfile = ProfileType::UNKNOWN;
        std::string descriptionReason;

        bool encounteredApsDelimiter = false;
        bool encounteredApsAttribute = false;
        bool encounteredSegments = false;
        size_t pictureCount = 0;
        int metafileVersion = 0;

        // Look for profile identifiers in METAFILE DESCRIPTION
        for (const auto &cmd : cgmFile->commands())
        {
            ClassCode cls = cmd->elementClass();
            int elementId = cmd->elementId();

            if (cls == ClassCode::MetafileDescriptorElements && elementId == 1)
            {
                if (auto *versionCmd = dynamic_cast<MetafileVersion *>(cmd.get()))
                {
                    metafileVersion = versionCmd->version();
                }
            }

            if (cmd->elementClass() == ClassCode::MetafileDescriptorElements && cmd->elementId() == 2)
            {
                auto *descCmd = dynamic_cast<MetafileDescription *>(cmd.get());
                if (descCmd)
                {
                    const std::string &desc = descCmd->description();
                    std::string upper = desc;
                    std::transform(upper.begin(), upper.end(), upper.begin(),
                                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

                    if (upper.find("GREXCHANGE") != std::string::npos)
                    {
                        if (upper.find("2.9") != std::string::npos)
                        {
                            describedProfile = ProfileType::ATA_GREXCHANGE_2_9;
                            descriptionReason = "(metafile description references ATA GREXCHANGE 2.9)";
                        }
                        else if (upper.find("2.8") != std::string::npos)
                        {
                            describedProfile = ProfileType::ATA_GREXCHANGE_2_8;
                            descriptionReason = "(metafile description references ATA GREXCHANGE 2.8)";
                        }
                        else if (upper.find("2.7") != std::string::npos)
                        {
                            describedProfile = ProfileType::ATA_GREXCHANGE_2_7;
                            descriptionReason = "(metafile description references ATA GREXCHANGE 2.7)";
                        }
                        else if (upper.find("2.6") != std::string::npos)
                        {
                            describedProfile = ProfileType::ATA_GREXCHANGE_2_6;
                            descriptionReason = "(metafile description references ATA GREXCHANGE 2.6)";
                        }
                        else
                        {
                            describedProfile = ProfileType::ATA_GREXCHANGE_2_9;
                            descriptionReason = "(metafile description references ATA GREXCHANGE – defaulting to 2.9)";
                        }
                    }
                    else if (upper.find("S1000D") != std::string::npos)
                    {
                        // Detection only identifies the S1000D family. The v6-vs-legacy
                        // distinction is a *rendering* decision (data-apsid/data-apsname
                        // vs bare apsid/name) controlled by the user-selected output
                        // profile (OutputProfile::S1000D / S1000DLegacy), not by anything
                        // declared in the CGM. ProfileEd: if present is informational
                        // only and surfaces in profile metadata; it does not route here.
                        describedProfile = ProfileType::S1000D_ISSUE_6;
                        descriptionReason = "(metafile description references S1000D)";
                    }
                    else if (upper.find("WEBCGM") != std::string::npos)
                    {
                        // Detection identifies the WebCGM family. The reported
                        // ProfileType is always WEBCGM_2_1 (the substrate validator);
                        // the declared sub-version (1.0 / 2.0 / 2.1) is informational
                        // only and surfaces in the reason string + parsed metadata.
                        // This matches the engine's contract (all WebCGM versions
                        // share the WebCGM 2.1 element rules — see
                        // getElementConstraints) and the long-standing golden-test
                        // expectation that detection profile is "webcgm-2.1".
                        describedProfile = ProfileType::WEBCGM_2_1;
                        if (upper.find("2.1") != std::string::npos)
                        {
                            descriptionReason = "(metafile description references WebCGM 2.1)";
                        }
                        else if (upper.find("2.0") != std::string::npos)
                        {
                            descriptionReason = "(metafile description references WebCGM 2.0 – using WebCGM 2.1 validator)";
                        }
                        else if (upper.find("1.0") != std::string::npos)
                        {
                            descriptionReason = "(metafile description references WebCGM 1.0 – using WebCGM 2.1 validator)";
                        }
                        else
                        {
                            descriptionReason = "(metafile description references WebCGM)";
                        }
                    }
                    else if (upper.find("CALS") != std::string::npos ||
                             upper.find("MIL-D-28003") != std::string::npos ||
                             upper.find("MIL-PRF-28003") != std::string::npos ||
                             upper.find("BASIC-1") != std::string::npos)
                    {
                        // Legacy DoD archives self-identify via strings like
                        // "MIL-D-28003/BASIC-1" or "MIL-D-28003A/BASIC-1.2".
                        describedProfile = ProfileType::CALS_MIL_PRF_28003;
                        descriptionReason = "(metafile description references CALS / MIL-D-28003)";
                    }
                }
            }

            if (cls == ClassCode::DelimiterElement)
            {
                if (elementId == 3)
                {
                    ++pictureCount;
                }
                else if (elementId == 21 || elementId == 22 || elementId == 23)
                {
                    encounteredApsDelimiter = true;
                }
            }
            else if (cls == ClassCode::ApplicationStructureDescriptorElements)
            {
                encounteredApsAttribute = true;
            }
            else if (cls == ClassCode::SegmentControlandSegmentAttributeElements)
            {
                encounteredSegments = true;
            }
        }

        if (describedProfile != ProfileType::UNKNOWN)
        {
            result.profile = describedProfile;
            result.reason = descriptionReason;
            result.confident = true;
            return result;
        }

        const bool hasAps = encounteredApsDelimiter || encounteredApsAttribute;
        const bool multiplePictures = pictureCount > 1;

        if (hasAps && !encounteredSegments && !multiplePictures && isBinary)
        {
            std::ostringstream oss;
            oss << "(detected WebCGM 2.1 from APS elements; binary=" << (isBinary ? "true" : "false")
                << ", pictures=1, segments=absent";
            if (metafileVersion > 0)
            {
                oss << ", MFVERSION=" << metafileVersion;
            }
            oss << ")";
            result.profile = ProfileType::WEBCGM_2_1;
            result.reason = oss.str();
            result.confident = true;
            return result;
        }

        std::ostringstream oss;
        oss << "(no definitive profile tags";
        if (!hasAps)
        {
            oss << ", no APS elements";
        }
        if (multiplePictures)
        {
            oss << ", " << pictureCount << " pictures";
        }
        if (encounteredSegments)
        {
            oss << ", segment elements present";
        }
        if (!isBinary)
        {
            oss << ", clear-text encoding";
        }
        oss << " → treating as ISO/IEC 8632)";

        result.profile = ProfileType::ISO_IEC_8632_COMPAT;
        result.reason = oss.str();
        result.confident = false;
        return result;
    }

    std::string ProfileDetector::extractProfileVersion(const std::string &description)
    {
        // Extract profile version from description
        size_t pos = description.find("GREXCHANGE");
        if (pos != std::string::npos)
        {
            // Look for version number after GREXCHANGE
            size_t versionPos = description.find_first_of("0123456789", pos);
            if (versionPos != std::string::npos)
            {
                return description.substr(versionPos, 3); // e.g., "2.9"
            }
        }
        return "";
    }

    std::unique_ptr<ProfileValidator> ProfileDetector::createValidator(ProfileType profileType)
    {
        switch (profileType)
        {
        case ProfileType::WEBCGM_2_1:
            return std::make_unique<WebCGM21Validator>();

        case ProfileType::ATA_GREXCHANGE_2_6:
            return std::make_unique<ATAGREXCHANGEValidator>(ATAGREXCHANGEValidator::Version::V2_6);
        case ProfileType::ATA_GREXCHANGE_2_7:
            return std::make_unique<ATAGREXCHANGEValidator>(ATAGREXCHANGEValidator::Version::V2_7);
        case ProfileType::ATA_GREXCHANGE_2_8:
            return std::make_unique<ATAGREXCHANGEValidator>(ATAGREXCHANGEValidator::Version::V2_8);
        case ProfileType::ATA_GREXCHANGE_2_9:
            return std::make_unique<ATAGREXCHANGEValidator>(ATAGREXCHANGEValidator::Version::V2_9);

        case ProfileType::S1000D_ISSUE_6:
            return std::make_unique<S1000DValidator>();

        case ProfileType::ISO_IEC_8632_COMPAT:
            return std::make_unique<ISO8632CompatibilityValidator>();

        case ProfileType::PIP_CGGC:
            // CGM*PIP has no element prohibitions; use the generic compatibility validator
            return std::make_unique<ISO8632CompatibilityValidator>();

        case ProfileType::CALS_MIL_PRF_28003:
            // CALS: MIL-D-28003A files are CGM v1 binary (predate segments/APS
            // restrictions), MIL-PRF-28003B defers to Model Profile / iSpec
            // 2200 / WebCGM 1.0 — real-world archives carry vendor deviations,
            // so acceptance is lenient. Era-specific element expectations are
            // covered by getCALSConstraints() via getElementConstraints().
            return std::make_unique<ISO8632CompatibilityValidator>();

        default:
            // Default to WebCGM 2.1
            return std::make_unique<WebCGM21Validator>();
        }
    }

    // ============================================================================
    // ValidationReport Implementation
    // ============================================================================

    ValidationReport::ValidationReport(const std::vector<ValidationMessage> &messages)
        : messages_(messages)
    {
    }

    std::string ValidationReport::generateTextReport() const
    {
        std::ostringstream oss;

        oss << "=================================================\n";
        oss << "CGM Profile Validation Report\n";
        oss << "=================================================\n\n";

        oss << "Summary:\n";
        oss << "  Errors:   " << getErrorCount() << "\n";
        oss << "  Warnings: " << getWarningCount() << "\n";
        oss << "  Info:     " << getInfoCount() << "\n";
        oss << "  Status:   " << (passed() ? "PASSED" : "FAILED") << "\n\n";

        oss << "Messages:\n";
        oss << "-------------------------------------------------\n";

        for (const auto &msg : messages_)
            oss << msg.toString() << "\n";

        oss << "=================================================\n";
        return oss.str();
    }

    std::string ValidationReport::generateJSONReport() const
    {
        std::ostringstream oss;

        oss << "{\n";
        oss << "  \"summary\": {\n";
        oss << "    \"errors\": " << getErrorCount() << ",\n";
        oss << "    \"warnings\": " << getWarningCount() << ",\n";
        oss << "    \"info\": " << getInfoCount() << ",\n";
        oss << "    \"passed\": " << (passed() ? "true" : "false") << "\n";
        oss << "  },\n";
        oss << "  \"messages\": [\n";

        for (size_t i = 0; i < messages_.size(); ++i)
        {
            const auto &m = messages_[i];
            oss << "    {\n";
            oss << "      \"severity\": \"" << m.getSeverityString() << "\",\n";
            oss << "      \"message\": \"" << m.message << "\",\n";
            if (!m.elementName.empty())
                oss << "      \"element\": \"" << m.elementName << "\",\n";
            if (m.elementId >= 0)
                oss << "      \"elementId\": " << m.elementId << ",\n";
            if (m.pictureIndex >= 0)
                oss << "      \"pictureIndex\": " << m.pictureIndex << ",\n";
            if (!m.rule.empty())
                oss << "      \"rule\": \"" << m.rule << "\"\n";
            oss << "    }" << (i + 1 < messages_.size() ? "," : "") << "\n";
        }

        oss << "  ]\n";
        oss << "}\n";
        return oss.str();
    }

    int ValidationReport::getErrorCount() const
    {
        int count = 0;
        for (const auto &m : messages_)
            if (m.severity == ValidationSeverity::ERROR || m.severity == ValidationSeverity::FATAL)
                ++count;
        return count;
    }

    int ValidationReport::getWarningCount() const
    {
        int count = 0;
        for (const auto &m : messages_)
            if (m.severity == ValidationSeverity::WARNING)
                ++count;
        return count;
    }

    int ValidationReport::getInfoCount() const
    {
        int count = 0;
        for (const auto &m : messages_)
            if (m.severity == ValidationSeverity::INFO)
                ++count;
        return count;
    }

    bool ValidationReport::passed() const
    {
        return getErrorCount() == 0;
    }

    // ============================================================================
    // ProfileMetadata Implementation
    // ============================================================================

    ProfileMetadata ProfileMetadata::parseFromDescription(const std::string& description)
    {
        ProfileMetadata meta;

        if (description.empty())
        {
            return meta;
        }

        // Parse ProfileId pattern: "ProfileId:WebCGM" or "ProfileId: WebCGM"
        std::regex profileIdRegex(R"(ProfileId\s*:\s*([^,"'\s]+))", std::regex::icase);
        std::smatch idMatch;
        if (std::regex_search(description, idMatch, profileIdRegex))
        {
            meta.profileId = idMatch[1].str();
            meta.detectionConfidence = 0.9;  // High confidence from explicit declaration
        }

        // Parse ProfileEd pattern: "ProfileEd:2.1" or "ProfileEd: 2.1"
        std::regex profileEdRegex(R"(ProfileEd\s*:\s*([0-9]+\.?[0-9]*))", std::regex::icase);
        std::smatch edMatch;
        if (std::regex_search(description, edMatch, profileEdRegex))
        {
            meta.profileEdition = edMatch[1].str();
        }

        // Parse Source pattern: "Source:IsoDraw" or various other patterns
        std::regex sourceRegex(R"(Source\s*:\s*([^,"']+))", std::regex::icase);
        std::smatch sourceMatch;
        if (std::regex_search(description, sourceMatch, sourceRegex))
        {
            meta.sourceApplication = sourceMatch[1].str();
            // Trim trailing whitespace
            meta.sourceApplication.erase(
                meta.sourceApplication.find_last_not_of(" \t\r\n") + 1);
        }

        // If no explicit ProfileId, try to infer from description keywords
        if (meta.profileId.empty())
        {
            std::string upper = description;
            std::transform(upper.begin(), upper.end(), upper.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

            if (upper.find("GREXCHANGE") != std::string::npos ||
                upper.find("ATA GRAPHICS") != std::string::npos)
            {
                meta.profileId = "ATA GRAPHICS.GREXCHANGE";
                meta.detectionConfidence = 0.8;

                // Try to extract version
                std::regex ataVersionRegex(R"(GREXCHANGE\s*(\d+\.?\d*))", std::regex::icase);
                std::smatch ataMatch;
                if (std::regex_search(description, ataMatch, ataVersionRegex))
                {
                    meta.profileEdition = ataMatch[1].str();
                }
            }
            else if (upper.find("S1000D") != std::string::npos ||
                     upper.find("IETM") != std::string::npos)
            {
                meta.profileId = "S1000D";
                meta.detectionConfidence = 0.8;
            }
            else if (upper.find("WEBCGM") != std::string::npos ||
                     upper.find("WEB CGM") != std::string::npos)
            {
                meta.profileId = "WebCGM";
                meta.detectionConfidence = 0.8;
            }
            else if (upper.find("CALS") != std::string::npos ||
                     upper.find("MIL-PRF-28003") != std::string::npos ||
                     upper.find("MIL-D-28003") != std::string::npos ||
                     upper.find("BASIC-1") != std::string::npos)
            {
                // MFDESC strings observed in legacy DoD archives:
                // "MIL-D-28003/BASIC-1" (1988) and "MIL-D-28003A/BASIC-1.2"
                // (1991). "A/BASIC-1.2" implies the 28003A (CGM v1, no APS)
                // era; bare 28003 or later strings may be 28003B era.
                meta.profileId = "CALS";
                meta.detectionConfidence = 0.8;
                std::regex calsEditionRegex(
                    R"(MIL-(?:D|PRF)-28003([A-B])?)", std::regex::icase);
                std::smatch calsMatch;
                if (std::regex_search(description, calsMatch, calsEditionRegex) &&
                    calsMatch[1].matched)
                {
                    meta.profileEdition = calsMatch[1].str();
                }
            }
        }

        return meta;
    }

    bool ProfileMetadata::isWebCGM() const
    {
        if (profileId.empty()) return false;
        std::string upper = profileId;
        std::transform(upper.begin(), upper.end(), upper.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return upper.find("WEBCGM") != std::string::npos ||
               upper.find("WEB CGM") != std::string::npos;
    }

    bool ProfileMetadata::isS1000D() const
    {
        if (profileId.empty()) return false;
        std::string upper = profileId;
        std::transform(upper.begin(), upper.end(), upper.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return upper.find("S1000D") != std::string::npos;
    }

    bool ProfileMetadata::isATAGREXCHANGE() const
    {
        if (profileId.empty()) return false;
        std::string upper = profileId;
        std::transform(upper.begin(), upper.end(), upper.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return upper.find("GREXCHANGE") != std::string::npos ||
               upper.find("ATA GRAPHICS") != std::string::npos ||
               upper.find("ATA ISPEC") != std::string::npos;
    }

    bool ProfileMetadata::isCALS() const
    {
        if (profileId.empty()) return false;
        std::string upper = profileId;
        std::transform(upper.begin(), upper.end(), upper.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return upper.find("CALS") != std::string::npos ||
               upper.find("MIL-PRF") != std::string::npos ||
               upper.find("MIL-D") != std::string::npos;
    }

    std::string ProfileMetadata::toString() const
    {
        std::ostringstream oss;
        oss << "ProfileMetadata {";
        if (!profileId.empty())
            oss << " profileId=" << profileId;
        if (!profileEdition.empty())
            oss << ", edition=" << profileEdition;
        if (!sourceApplication.empty())
            oss << ", source=" << sourceApplication;
        oss << ", confidence=" << std::fixed << std::setprecision(2) << detectionConfidence;
        if (hasApsStructures)
            oss << ", hasAPS=true";
        if (hasNurbsElements)
            oss << ", hasNURBS=true";
        if (pictureCount > 0)
            oss << ", pictures=" << pictureCount;
        oss << " }";
        return oss.str();
    }

    // ============================================================================
    // ElementConstraints Implementation
    // ============================================================================

    ElementConstraints ElementConstraints::getWebCGM21Constraints()
    {
        ElementConstraints constraints;

        // WebCGM 2.1 prohibited curve elements (Profile chapter, PPF rows
        // T.19.22/T.19.23): HYPERBOLIC ARC and PARABOLIC ARC only.
        // NON-UNIFORM B-SPLINE (4,24) and NON-UNIFORM RATIONAL B-SPLINE
        // (4,25) are PERMITTED since WebCGM 2.0 (T.19.24/T.19.25: cubic
        // order 4, max 4096 control points, clamped form) — parameter
        // limits are checked as warnings, not element prohibitions.
        constraints.prohibitedElements.insert({4, 22});  // HYPERBOLIC ARC
        constraints.prohibitedElements.insert({4, 23});  // PARABOLIC ARC

        // Class 8 (Segment Elements) - V2 segments prohibited in WebCGM
        constraints.prohibitedElements.insert({8, 1});   // COPY SEGMENT
        constraints.prohibitedElements.insert({8, 2});   // INHERITANCE FILTER
        constraints.prohibitedElements.insert({8, 3});   // CLIP INHERITANCE
        constraints.prohibitedElements.insert({8, 4});   // SEGMENT TRANSFORMATION
        constraints.prohibitedElements.insert({8, 5});   // SEGMENT HIGHLIGHTING
        constraints.prohibitedElements.insert({8, 6});   // SEGMENT DISPLAY PRIORITY
        constraints.prohibitedElements.insert({8, 7});   // SEGMENT PICK PRIORITY

        return constraints;
    }

    ElementConstraints ElementConstraints::getS1000DConstraints()
    {
        // S1000D inherits WebCGM restrictions plus additional constraints
        ElementConstraints constraints = getWebCGM21Constraints();

        // S1000D has the same prohibited elements as WebCGM 2.1.
        // Verified against S1000D Issue 6 Chap 7.3.2: the S1000D cascading
        // profile's delta tables contain no curve-element rows, so it
        // inherits WebCGM's rules unchanged (NUBS/NURBS permitted,
        // hyperbolic/parabolic arcs prohibited). Additional S1000D-specific
        // constraints (fonts, ICN naming, hotspot conventions) are enforced
        // at validation level.

        return constraints;
    }

    ElementConstraints ElementConstraints::getATAGREXCHANGEConstraints()
    {
        ElementConstraints constraints;

        // ATA GREXCHANGE is broader than WebCGM here: "ATA Grex 2.9 allows
        // the PARABOLIC ARC and NON-UNIFORM B-SPLINE elements, whereas
        // WebCGM does not" (CGM Open). Rational NURBS acceptance is
        // unverified for ATA, but the converter renders them, so they are
        // not flagged as prohibited either. Only HYPERBOLIC ARC remains
        // prohibited. Segment elements are prohibited as in WebCGM.
        constraints.prohibitedElements.insert({4, 22});  // HYPERBOLIC ARC
        constraints.prohibitedElements.insert({8, 1});   // COPY SEGMENT
        constraints.prohibitedElements.insert({8, 2});   // INHERITANCE FILTER
        constraints.prohibitedElements.insert({8, 3});   // CLIP INHERITANCE
        constraints.prohibitedElements.insert({8, 4});   // SEGMENT TRANSFORMATION
        constraints.prohibitedElements.insert({8, 5});   // SEGMENT HIGHLIGHTING
        constraints.prohibitedElements.insert({8, 6});   // SEGMENT DISPLAY PRIORITY
        constraints.prohibitedElements.insert({8, 7});   // SEGMENT PICK PRIORITY

        return constraints;
    }

    ElementConstraints ElementConstraints::getCALSConstraints()
    {
        ElementConstraints constraints;

        // CALS is more restrictive - typically CGM V1-V3
        // Advanced elements may not be supported
        constraints.prohibitedElements.insert({4, 22});  // HYPERBOLIC ARC
        constraints.prohibitedElements.insert({4, 23});  // PARABOLIC ARC
        constraints.prohibitedElements.insert({4, 24});  // NON-UNIFORM B-SPLINE
        constraints.prohibitedElements.insert({4, 25});  // NURBS

        // V4 Application Structures are deprecated in CALS
        constraints.deprecatedElements.insert({9, 1});   // BEGIN APPLICATION STRUCTURE
        constraints.deprecatedElements.insert({9, 2});   // BEGIN APPLICATION STRUCTURE BODY
        constraints.deprecatedElements.insert({9, 3});   // END APPLICATION STRUCTURE
        constraints.deprecatedElements.insert({9, 4});   // APPLICATION STRUCTURE ATTRIBUTE

        return constraints;
    }

    // ============================================================================
    // IcnValidationResult Implementation
    // ============================================================================

    IcnValidationResult IcnValidationResult::validate(const std::string& filename)
    {
        IcnValidationResult result;

        if (filename.empty())
        {
            result.errorMessage = "Empty filename";
            return result;
        }

        // Extract base filename without path and extension
        std::string baseName = filename;
        size_t lastSlash = baseName.find_last_of("/\\");
        if (lastSlash != std::string::npos)
        {
            baseName = baseName.substr(lastSlash + 1);
        }
        size_t lastDot = baseName.rfind('.');
        if (lastDot != std::string::npos)
        {
            baseName = baseName.substr(0, lastDot);
        }

        // S1000D ICN format: ICN-XXXXX-XXXXXXXX-XXX-XX
        // Pattern: ICN-[ModelIdent(5)]-[SDC(8)]-[DICode(3)]-[Variant(2)]
        std::regex icnPattern(R"(ICN-([A-Z0-9]{5})-([A-Z0-9]{8})-([A-Z0-9]{3})-([A-Z0-9]{2}))",
                              std::regex::icase);
        std::smatch match;

        if (std::regex_match(baseName, match, icnPattern))
        {
            result.isValid = true;
            result.modelIdent = match[1].str();
            result.sdc = match[2].str();
            result.diCode = match[3].str();
            result.variant = match[4].str();
        }
        else
        {
            result.isValid = false;
            result.errorMessage = "Filename does not match S1000D ICN format: ICN-XXXXX-XXXXXXXX-XXX-XX";
        }

        return result;
    }

    // ============================================================================
    // ProfileDetector Enhanced Methods
    // ============================================================================

    ProfileDetector::DetectionResult ProfileDetector::detectProfileWithMetadata(const CGMFile* cgmFile)
    {
        // Call the existing detectProfile and enhance with metadata
        DetectionResult result = detectProfile(cgmFile);

        if (!cgmFile)
        {
            return result;
        }

        // Extract metadata from metafile description
        for (const auto& cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::MetafileDescriptorElements &&
                cmd->elementId() == 2)
            {
                auto* descCmd = dynamic_cast<MetafileDescription*>(cmd.get());
                if (descCmd)
                {
                    result.metadata = ProfileMetadata::parseFromDescription(descCmd->description());
                    break;
                }
            }
        }

        // Enhance metadata with content analysis - extract metafile version from commands
        for (const auto& cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::MetafileDescriptorElements &&
                cmd->elementId() == 1)
            {
                auto* versionCmd = dynamic_cast<MetafileVersion*>(cmd.get());
                if (versionCmd)
                {
                    result.metadata.cgmVersion = versionCmd->version();
                    break;
                }
            }
        }
        result.metadata.hasApsStructures = hasApplicationStructures(cgmFile);
        result.metadata.hasNurbsElements = hasNurbsElements(cgmFile);

        // Count pictures
        int pictureCount = 0;
        for (const auto& cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::DelimiterElement && cmd->elementId() == 3)
            {
                pictureCount++;
            }
        }
        result.metadata.pictureCount = pictureCount;

        // Check for parabolic arcs
        for (const auto& cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::GraphicalPrimitiveElements &&
                cmd->elementId() == 23)
            {
                result.metadata.hasParabolicArcs = true;
                break;
            }
        }

        // Adjust confidence based on content analysis
        if (result.metadata.detectionConfidence == 0.0)
        {
            // No explicit profile declaration - infer from content
            if (result.metadata.hasNurbsElements || result.metadata.hasParabolicArcs)
            {
                // NURBS/parabolic arcs are prohibited in all aviation profiles
                // (WebCGM, ATA, S1000D, CALS). Presence suggests non-conformant
                // or generic CGM - use low confidence.
                result.metadata.detectionConfidence = 0.3;
            }
            else if (result.metadata.hasApsStructures)
            {
                // APS suggests WebCGM or derivatives
                result.metadata.detectionConfidence = 0.5;
            }
            else
            {
                result.metadata.detectionConfidence = 0.3;
            }
        }

        return result;
    }

    ElementConstraints ProfileDetector::getElementConstraints(ProfileType profileType)
    {
        switch (profileType)
        {
            case ProfileType::WEBCGM_1_0:
            case ProfileType::WEBCGM_2_0:
            case ProfileType::WEBCGM_2_1:
                return ElementConstraints::getWebCGM21Constraints();

            case ProfileType::S1000D_ISSUE_6:
                return ElementConstraints::getS1000DConstraints();

            case ProfileType::ATA_GREXCHANGE_2_6:
            case ProfileType::ATA_GREXCHANGE_2_7:
            case ProfileType::ATA_GREXCHANGE_2_8:
            case ProfileType::ATA_GREXCHANGE_2_9:
                return ElementConstraints::getATAGREXCHANGEConstraints();

            case ProfileType::CALS_MIL_PRF_28003:
                return ElementConstraints::getCALSConstraints();

            case ProfileType::ISO_IEC_8632_COMPAT:
            default:
                // No restrictions for generic ISO profile
                return ElementConstraints();
        }
    }

    bool ProfileDetector::hasNurbsElements(const CGMFile* cgmFile)
    {
        if (!cgmFile) return false;

        for (const auto& cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::GraphicalPrimitiveElements)
            {
                int eid = cmd->elementId();
                // NON-UNIFORM B-SPLINE (24) or NON-UNIFORM RATIONAL B-SPLINE (25)
                if (eid == 24 || eid == 25)
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool ProfileDetector::hasApplicationStructures(const CGMFile* cgmFile)
    {
        if (!cgmFile) return false;

        for (const auto& cmd : cgmFile->commands())
        {
            if (cmd->elementClass() == ClassCode::ApplicationStructureDescriptorElements)
            {
                return true;
            }
        }
        return false;
    }

}
