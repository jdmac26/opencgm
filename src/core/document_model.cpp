#include "opencgm/document_model.h"

#include "opencgm/commands/application_structure_commands.h"
#include "opencgm/commands/delimiter_commands.h"
#include "opencgm/commands/graphical_primitive_commands.h"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace opencgm {

namespace {

std::string jsonEscape(const std::string& input)
{
    std::ostringstream oss;
    for (unsigned char ch : input)
    {
        switch (ch)
        {
        case '\\': oss << "\\\\"; break;
        case '"': oss << "\\\""; break;
        case '\b': oss << "\\b"; break;
        case '\f': oss << "\\f"; break;
        case '\n': oss << "\\n"; break;
        case '\r': oss << "\\r"; break;
        case '\t': oss << "\\t"; break;
        default:
            if (ch < 0x20)
            {
                oss << "\\u"
                    << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<int>(ch)
                    << std::dec << std::setfill(' ');
            }
            else
            {
                oss << static_cast<char>(ch);
            }
            break;
        }
    }
    return oss.str();
}

std::string truncateValue(const std::string& value, size_t maxLength = 120)
{
    if (value.size() <= maxLength)
    {
        return value;
    }
    return value.substr(0, maxLength - 3) + "...";
}

std::string profileTypeToString(ProfileType type)
{
    switch (type)
    {
    case ProfileType::WEBCGM_1_0: return "webcgm-1.0";
    case ProfileType::WEBCGM_2_0: return "webcgm-2.0";
    case ProfileType::WEBCGM_2_1: return "webcgm-2.1";
    case ProfileType::ISO_IEC_8632_COMPAT: return "iso-iec-8632";
    case ProfileType::ATA_GREXCHANGE_2_6: return "ata-grexchange-2.6";
    case ProfileType::ATA_GREXCHANGE_2_7: return "ata-grexchange-2.7";
    case ProfileType::ATA_GREXCHANGE_2_8: return "ata-grexchange-2.8";
    case ProfileType::ATA_GREXCHANGE_2_9: return "ata-grexchange-2.9";
    case ProfileType::S1000D_ISSUE_6: return "s1000d-issue-6";
    case ProfileType::PIP_CGGC: return "pip-cggc";
    case ProfileType::CALS_MIL_PRF_28003: return "cals";
    case ProfileType::UNKNOWN:
    default:
        return "unknown";
    }
}

std::string severityToString(ValidationSeverity severity)
{
    switch (severity)
    {
    case ValidationSeverity::INFO: return "info";
    case ValidationSeverity::WARNING: return "warning";
    case ValidationSeverity::ERROR: return "error";
    case ValidationSeverity::FATAL: return "fatal";
    default: return "info";
    }
}

std::string dispositionToString(PreservationDisposition disposition)
{
    switch (disposition)
    {
    case PreservationDisposition::Preserved: return "preserved";
    case PreservationDisposition::Degraded: return "degraded";
    case PreservationDisposition::Dropped: return "dropped";
    default: return "preserved";
    }
}

std::string sceneKindToString(SceneNodeKind kind)
{
    switch (kind)
    {
    case SceneNodeKind::Group: return "group";
    case SceneNodeKind::Geometry: return "geometry";
    case SceneNodeKind::Text: return "text";
    case SceneNodeKind::Raster: return "raster";
    case SceneNodeKind::Unknown:
    default:
        return "unknown";
    }
}

std::string semanticKindToString(SemanticNodeKind kind)
{
    switch (kind)
    {
    case SemanticNodeKind::Structure: return "structure";
    case SemanticNodeKind::Attribute: return "attribute";
    case SemanticNodeKind::Link: return "link";
    case SemanticNodeKind::Hotspot: return "hotspot";
    case SemanticNodeKind::ViewContext: return "viewcontext";
    case SemanticNodeKind::Unknown:
    default:
        return "unknown";
    }
}

PreservationDisposition dispositionFromSeverity(Severity severity)
{
    switch (severity)
    {
    case Severity::Unsupported:
        return PreservationDisposition::Dropped;
    case Severity::Unimplemented:
        return PreservationDisposition::Degraded;
    case Severity::Fatal:
        return PreservationDisposition::Dropped;
    default:
        return PreservationDisposition::Preserved;
    }
}

SourceTrace makeTrace(const Command& command, size_t commandIndex, int pictureIndex)
{
    return SourceTrace{
        commandIndex,
        pictureIndex,
        command.elementClass(),
        command.elementId(),
        truncateValue(command.toString())
    };
}

bool containsInsensitive(std::string haystack, const std::string& needle)
{
    std::transform(haystack.begin(), haystack.end(), haystack.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    std::string loweredNeedle = needle;
    std::transform(loweredNeedle.begin(), loweredNeedle.end(), loweredNeedle.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return haystack.find(loweredNeedle) != std::string::npos;
}

} // namespace

DocumentSummary ConversionDocumentModel::summarize() const
{
    DocumentSummary summary;
    summary.commandCount = commandCount;
    summary.pictureCount = pictureCount;
    summary.sceneNodeCount = sceneNodes.size();
    summary.semanticNodeCount = semanticNodes.size();
    summary.issueCount = issues.size();

    for (const auto& node : sceneNodes)
    {
        switch (node.kind)
        {
        case SceneNodeKind::Geometry: ++summary.geometryNodeCount; break;
        case SceneNodeKind::Text: ++summary.textNodeCount; break;
        case SceneNodeKind::Raster: ++summary.rasterNodeCount; break;
        default: break;
        }

        switch (node.disposition)
        {
        case PreservationDisposition::Preserved: ++summary.preservedCount; break;
        case PreservationDisposition::Degraded: ++summary.degradedCount; break;
        case PreservationDisposition::Dropped: ++summary.droppedCount; break;
        }
    }

    for (const auto& node : semanticNodes)
    {
        switch (node.disposition)
        {
        case PreservationDisposition::Preserved: ++summary.preservedCount; break;
        case PreservationDisposition::Degraded: ++summary.degradedCount; break;
        case PreservationDisposition::Dropped: ++summary.droppedCount; break;
        }
    }

    for (const auto& issue : issues)
    {
        switch (issue.disposition)
        {
        case PreservationDisposition::Preserved: ++summary.preservedCount; break;
        case PreservationDisposition::Degraded: ++summary.degradedCount; break;
        case PreservationDisposition::Dropped: ++summary.droppedCount; break;
        }
    }

    return summary;
}

ConversionDocumentModel ConversionDocumentBuilder::fromCgm(const CGMFile& cgmFile, const std::string& inputPath)
{
    ConversionDocumentModel document;
    document.inputPath = inputPath;
    document.logicalName = cgmFile.fileName().empty() ? cgmFile.name() : cgmFile.fileName();
    document.detectedProfile = ProfileDetector::detectProfileWithMetadata(&cgmFile);
    document.commandCount = cgmFile.commands().size();
    document.pictureCount = cgmFile.getPictureRanges().size();

    int pictureIndex = -1;

    const auto& commands = cgmFile.commands();
    for (size_t i = 0; i < commands.size(); ++i)
    {
        const auto& commandPtr = commands[i];
        if (!commandPtr)
        {
            continue;
        }

        const auto& command = *commandPtr;
        if (command.elementClass() == ClassCode::DelimiterElement && command.elementId() == 3)
        {
            ++pictureIndex;
            if (const auto* beginPicture = dynamic_cast<const BeginPicture*>(commandPtr.get()))
            {
                document.sceneNodes.push_back(SceneNode{
                    SceneNodeKind::Group,
                    PreservationDisposition::Preserved,
                    beginPicture->name().empty() ? "picture" : beginPicture->name(),
                    makeTrace(command, i, pictureIndex)
                });
            }
        }

        if (const auto* text = dynamic_cast<const Text*>(commandPtr.get()))
        {
            document.sceneNodes.push_back(SceneNode{
                SceneNodeKind::Text,
                PreservationDisposition::Preserved,
                truncateValue(text->text()),
                makeTrace(command, i, pictureIndex)
            });
            continue;
        }

        if (const auto* restrictedText = dynamic_cast<const RestrictedText*>(commandPtr.get()))
        {
            document.sceneNodes.push_back(SceneNode{
                SceneNodeKind::Text,
                PreservationDisposition::Preserved,
                truncateValue(restrictedText->text()),
                makeTrace(command, i, pictureIndex)
            });
            continue;
        }

        if (const auto* appendText = dynamic_cast<const AppendText*>(commandPtr.get()))
        {
            document.sceneNodes.push_back(SceneNode{
                SceneNodeKind::Text,
                PreservationDisposition::Preserved,
                truncateValue(appendText->text()),
                makeTrace(command, i, pictureIndex)
            });
            continue;
        }

        if (dynamic_cast<const CellArray*>(commandPtr.get()) != nullptr ||
            dynamic_cast<const BitonalTile*>(commandPtr.get()) != nullptr ||
            dynamic_cast<const Tile*>(commandPtr.get()) != nullptr)
        {
            document.sceneNodes.push_back(SceneNode{
                SceneNodeKind::Raster,
                PreservationDisposition::Preserved,
                truncateValue(command.toString()),
                makeTrace(command, i, pictureIndex)
            });
            continue;
        }

        if (const auto* beginStructure = dynamic_cast<const BeginApplicationStructure*>(commandPtr.get()))
        {
            SemanticNodeKind kind = containsInsensitive(beginStructure->type(), "hotspot")
                ? SemanticNodeKind::Hotspot
                : SemanticNodeKind::Structure;

            document.semanticNodes.push_back(SemanticNode{
                kind,
                PreservationDisposition::Preserved,
                beginStructure->identifier(),
                beginStructure->type(),
                beginStructure->identifier(),
                makeTrace(command, i, pictureIndex)
            });
            continue;
        }

        if (const auto* apsAttribute = dynamic_cast<const ApplicationStructureAttribute*>(commandPtr.get()))
        {
            SemanticNodeKind kind = SemanticNodeKind::Attribute;
            if (containsInsensitive(apsAttribute->attributeType(), "uri") ||
                containsInsensitive(apsAttribute->attributeType(), "link"))
            {
                kind = SemanticNodeKind::Link;
            }
            else if (containsInsensitive(apsAttribute->attributeType(), "viewcontext"))
            {
                kind = SemanticNodeKind::ViewContext;
            }

            document.semanticNodes.push_back(SemanticNode{
                kind,
                PreservationDisposition::Preserved,
                std::string(),
                apsAttribute->attributeType(),
                truncateValue(apsAttribute->structuredText().value_or(apsAttribute->data())),
                makeTrace(command, i, pictureIndex)
            });
            continue;
        }

        if (command.elementClass() == ClassCode::GraphicalPrimitiveElements)
        {
            document.sceneNodes.push_back(SceneNode{
                SceneNodeKind::Geometry,
                PreservationDisposition::Preserved,
                truncateValue(command.toString()),
                makeTrace(command, i, pictureIndex)
            });
        }
    }

    for (const auto& message : cgmFile.messages())
    {
        ConversionIssue issue;
        issue.category = "parser";
        issue.message = message.message;
        issue.disposition = dispositionFromSeverity(message.severity);
        issue.severity = issue.disposition == PreservationDisposition::Dropped
            ? ValidationSeverity::ERROR
            : (issue.disposition == PreservationDisposition::Degraded ? ValidationSeverity::WARNING : ValidationSeverity::INFO);
        issue.trace = SourceTrace{
            0,
            -1,
            message.elementClass,
            message.elementId,
            message.commandName
        };
        document.issues.push_back(std::move(issue));
    }

    return document;
}

std::string ConversionReport::generateJsonReport() const
{
    const auto summary = document.summarize();
    int infoCount = 0;
    int warningCount = 0;
    int errorCount = 0;
    int fatalCount = 0;

    auto accumulateSeverity = [&](const ConversionIssue& issue)
    {
        switch (issue.severity)
        {
        case ValidationSeverity::INFO: ++infoCount; break;
        case ValidationSeverity::WARNING: ++warningCount; break;
        case ValidationSeverity::ERROR: ++errorCount; break;
        case ValidationSeverity::FATAL: ++fatalCount; break;
        default: ++infoCount; break;
        }
    };

    for (const auto& issue : document.issues)
    {
        accumulateSeverity(issue);
    }
    for (const auto& issue : runtimeIssues)
    {
        accumulateSeverity(issue);
    }

    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"schemaVersion\": 1,\n";
    oss << "  \"input\": {\n";
    oss << "    \"path\": \"" << jsonEscape(document.inputPath) << "\",\n";
    oss << "    \"logicalName\": \"" << jsonEscape(document.logicalName) << "\"\n";
    oss << "  },\n";
    oss << "  \"output\": {\n";
    oss << "    \"path\": \"" << jsonEscape(outputPath) << "\",\n";
    oss << "    \"compressed\": " << (compressedOutput ? "true" : "false") << ",\n";
    oss << "    \"minified\": " << (minifiedOutput ? "true" : "false") << ",\n";
    oss << "    \"optimizedPaths\": " << (optimizedPaths ? "true" : "false") << ",\n";
    oss << "    \"success\": " << (success ? "true" : "false") << "\n";
    oss << "  },\n";
    oss << "  \"profile\": {\n";
    oss << "    \"detected\": \"" << jsonEscape(profileTypeToString(document.detectedProfile.profile)) << "\",\n";
    oss << "    \"reason\": \"" << jsonEscape(document.detectedProfile.reason) << "\",\n";
    oss << "    \"confident\": " << (document.detectedProfile.confident ? "true" : "false") << ",\n";
    oss << "    \"profileId\": \"" << jsonEscape(document.detectedProfile.metadata.profileId) << "\",\n";
    oss << "    \"profileEdition\": \"" << jsonEscape(document.detectedProfile.metadata.profileEdition) << "\",\n";
    oss << "    \"sourceApplication\": \"" << jsonEscape(document.detectedProfile.metadata.sourceApplication) << "\",\n";
    oss << "    \"cgmVersion\": " << document.detectedProfile.metadata.cgmVersion << "\n";
    oss << "  },\n";
    oss << "  \"rulePack\": {\n";
    oss << "    \"name\": \"" << jsonEscape(document.rulePack.name) << "\",\n";
    oss << "    \"version\": \"" << jsonEscape(document.rulePack.version) << "\",\n";
    oss << "    \"source\": \"" << jsonEscape(document.rulePack.source) << "\"\n";
    oss << "  },\n";
    oss << "  \"summary\": {\n";
    oss << "    \"commands\": " << summary.commandCount << ",\n";
    oss << "    \"pictures\": " << summary.pictureCount << ",\n";
    oss << "    \"sceneNodes\": " << summary.sceneNodeCount << ",\n";
    oss << "    \"semanticNodes\": " << summary.semanticNodeCount << ",\n";
    oss << "    \"geometryNodes\": " << summary.geometryNodeCount << ",\n";
    oss << "    \"textNodes\": " << summary.textNodeCount << ",\n";
    oss << "    \"rasterNodes\": " << summary.rasterNodeCount << ",\n";
    oss << "    \"preserved\": " << summary.preservedCount << ",\n";
    oss << "    \"degraded\": " << summary.degradedCount << ",\n";
    oss << "    \"dropped\": " << summary.droppedCount << ",\n";
    oss << "    \"issues\": " << (document.issues.size() + runtimeIssues.size()) << ",\n";
    oss << "    \"documentIssues\": " << document.issues.size() << ",\n";
    oss << "    \"runtimeIssues\": " << runtimeIssues.size() << ",\n";
    oss << "    \"infoCount\": " << infoCount << ",\n";
    oss << "    \"warningCount\": " << warningCount << ",\n";
    oss << "    \"errorCount\": " << errorCount << ",\n";
    oss << "    \"fatalCount\": " << fatalCount << "\n";
    oss << "  },\n";

    if (geometryMetrics.has_value())
    {
        const auto& metrics = *geometryMetrics;
        oss << "  \"geometry\": {\n";
        oss << "    \"hasGeometry\": " << (metrics.has_geometry ? "true" : "false") << ",\n";
        oss << "    \"viewContextPresent\": " << (metrics.view_context_present ? "true" : "false") << ",\n";
        oss << "    \"viewContextAdopted\": " << (metrics.view_context_adopted ? "true" : "false") << ",\n";
        oss << "    \"compatibilityMode\": " << (metrics.compatibility_mode ? "true" : "false") << ",\n";
        oss << "    \"viewBoxWidth\": " << metrics.viewbox_width << ",\n";
        oss << "    \"viewBoxHeight\": " << metrics.viewbox_height << "\n";
        oss << "  },\n";
    }

    oss << "  \"scene\": [\n";
    for (size_t i = 0; i < document.sceneNodes.size(); ++i)
    {
        const auto& node = document.sceneNodes[i];
        oss << "    {\n";
        oss << "      \"kind\": \"" << sceneKindToString(node.kind) << "\",\n";
        oss << "      \"label\": \"" << jsonEscape(node.label) << "\",\n";
        oss << "      \"disposition\": \"" << dispositionToString(node.disposition) << "\",\n";
        oss << "      \"trace\": {\n";
        oss << "        \"commandIndex\": " << node.trace.commandIndex << ",\n";
        oss << "        \"pictureIndex\": " << node.trace.pictureIndex << ",\n";
        oss << "        \"elementClass\": " << static_cast<int>(node.trace.elementClass) << ",\n";
        oss << "        \"elementId\": " << node.trace.elementId << "\n";
        oss << "      }\n";
        oss << "    }" << (i + 1 < document.sceneNodes.size() ? "," : "") << "\n";
    }
    oss << "  ],\n";

    oss << "  \"semantics\": [\n";
    for (size_t i = 0; i < document.semanticNodes.size(); ++i)
    {
        const auto& node = document.semanticNodes[i];
        oss << "    {\n";
        oss << "      \"kind\": \"" << semanticKindToString(node.kind) << "\",\n";
        oss << "      \"identifier\": \"" << jsonEscape(node.identifier) << "\",\n";
        oss << "      \"type\": \"" << jsonEscape(node.type) << "\",\n";
        oss << "      \"value\": \"" << jsonEscape(node.value) << "\",\n";
        oss << "      \"disposition\": \"" << dispositionToString(node.disposition) << "\"\n";
        oss << "    }" << (i + 1 < document.semanticNodes.size() ? "," : "") << "\n";
    }
    oss << "  ],\n";

    oss << "  \"issues\": [\n";
    auto writeIssue = [&](const ConversionIssue& issue, bool withComma)
    {
        oss << "    {\n";
        oss << "      \"severity\": \"" << severityToString(issue.severity) << "\",\n";
        oss << "      \"category\": \"" << jsonEscape(issue.category) << "\",\n";
        oss << "      \"disposition\": \"" << dispositionToString(issue.disposition) << "\",\n";
        oss << "      \"message\": \"" << jsonEscape(issue.message) << "\"";
        if (!issue.rule.empty())
        {
            oss << ",\n      \"rule\": \"" << jsonEscape(issue.rule) << "\"";
        }
        if (issue.trace.has_value())
        {
            oss << ",\n      \"trace\": {\n";
            oss << "        \"commandIndex\": " << issue.trace->commandIndex << ",\n";
            oss << "        \"pictureIndex\": " << issue.trace->pictureIndex << ",\n";
            oss << "        \"elementClass\": " << static_cast<int>(issue.trace->elementClass) << ",\n";
            oss << "        \"elementId\": " << issue.trace->elementId << "\n";
            oss << "      }";
        }
        oss << "\n    }" << (withComma ? "," : "") << "\n";
    };

    size_t totalIssueCount = document.issues.size() + runtimeIssues.size();
    size_t writtenIssueCount = 0;
    for (const auto& issue : document.issues)
    {
        ++writtenIssueCount;
        writeIssue(issue, writtenIssueCount < totalIssueCount);
    }
    for (const auto& issue : runtimeIssues)
    {
        ++writtenIssueCount;
        writeIssue(issue, writtenIssueCount < totalIssueCount);
    }

    oss << "  ]\n";
    oss << "}\n";
    return oss.str();
}

std::string ConversionReport::generateTextReport() const
{
    const auto summary = document.summarize();
    int infoCount = 0;
    int warningCount = 0;
    int errorCount = 0;
    int fatalCount = 0;

    auto accumulateSeverity = [&](const ConversionIssue& issue)
    {
        switch (issue.severity)
        {
        case ValidationSeverity::INFO: ++infoCount; break;
        case ValidationSeverity::WARNING: ++warningCount; break;
        case ValidationSeverity::ERROR: ++errorCount; break;
        case ValidationSeverity::FATAL: ++fatalCount; break;
        default: ++infoCount; break;
        }
    };

    for (const auto& issue : document.issues)
    {
        accumulateSeverity(issue);
    }
    for (const auto& issue : runtimeIssues)
    {
        accumulateSeverity(issue);
    }

    std::ostringstream oss;
    oss << "OpenCGM Conversion Report\n";
    oss << "=========================\n";
    oss << "Input:    " << document.inputPath << "\n";
    oss << "Output:   " << outputPath << "\n";
    oss << "Success:  " << (success ? "yes" : "no") << "\n";
    oss << "Minified: " << (minifiedOutput ? "yes" : "no") << "\n";
    oss << "Paths:    " << (optimizedPaths ? "optimized" : "unchanged") << "\n";
    oss << "Profile:  " << profileTypeToString(document.detectedProfile.profile) << "\n";
    if (!document.detectedProfile.reason.empty())
    {
        oss << "Reason:   " << document.detectedProfile.reason << "\n";
    }
    oss << "\nSummary\n";
    oss << "-------\n";
    oss << "Commands:      " << summary.commandCount << "\n";
    oss << "Pictures:      " << summary.pictureCount << "\n";
    oss << "Scene nodes:   " << summary.sceneNodeCount << "\n";
    oss << "Semantic nodes:" << summary.semanticNodeCount << "\n";
    oss << "Preserved:     " << summary.preservedCount << "\n";
    oss << "Degraded:      " << summary.degradedCount << "\n";
    oss << "Dropped:       " << summary.droppedCount << "\n";
    oss << "Info:          " << infoCount << "\n";
    oss << "Warnings:      " << warningCount << "\n";
    oss << "Errors:        " << errorCount << "\n";
    oss << "Fatal:         " << fatalCount << "\n";

    if (geometryMetrics.has_value())
    {
        oss << "\nGeometry\n";
        oss << "--------\n";
        oss << "Has geometry: " << (geometryMetrics->has_geometry ? "yes" : "no") << "\n";
        oss << "Viewcontext:  " << (geometryMetrics->view_context_present ? "present" : "absent") << "\n";
        oss << "ViewBox:      " << geometryMetrics->viewbox_width << " x " << geometryMetrics->viewbox_height << "\n";
    }

    if (!document.issues.empty() || !runtimeIssues.empty())
    {
        oss << "\nIssues\n";
        oss << "------\n";
        for (const auto& issue : document.issues)
        {
            oss << "[" << severityToString(issue.severity) << "] "
                << dispositionToString(issue.disposition) << " "
                << issue.category << ": " << issue.message << "\n";
        }
        for (const auto& issue : runtimeIssues)
        {
            oss << "[" << severityToString(issue.severity) << "] "
                << dispositionToString(issue.disposition) << " "
                << issue.category << ": " << issue.message << "\n";
        }
    }

    return oss.str();
}

} // namespace opencgm
