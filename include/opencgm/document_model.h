#ifndef OPENCGM_DOCUMENT_MODEL_H
#define OPENCGM_DOCUMENT_MODEL_H

#include "cgm_file.h"
#include "profile_validator.h"
#include "svg_converter.h"
#include <optional>
#include <string>
#include <vector>

namespace opencgm {

enum class SceneNodeKind {
    Unknown,
    Group,
    Geometry,
    Text,
    Raster
};

enum class SemanticNodeKind {
    Unknown,
    Structure,
    Attribute,
    Link,
    Hotspot,
    ViewContext
};

enum class PreservationDisposition {
    Preserved,
    Degraded,
    Dropped
};

struct SourceTrace {
    size_t commandIndex = 0;
    int pictureIndex = -1;
    ClassCode elementClass = ClassCode::DelimiterElement;
    int elementId = -1;
    std::string commandText;
};

struct SceneNode {
    SceneNodeKind kind = SceneNodeKind::Unknown;
    PreservationDisposition disposition = PreservationDisposition::Preserved;
    std::string label;
    SourceTrace trace;
};

struct SemanticNode {
    SemanticNodeKind kind = SemanticNodeKind::Unknown;
    PreservationDisposition disposition = PreservationDisposition::Preserved;
    std::string identifier;
    std::string type;
    std::string value;
    SourceTrace trace;
};

struct ConversionIssue {
    ValidationSeverity severity = ValidationSeverity::INFO;
    PreservationDisposition disposition = PreservationDisposition::Preserved;
    std::string category;
    std::string message;
    std::string rule;
    std::optional<SourceTrace> trace;
};

struct DocumentSummary {
    size_t commandCount = 0;
    size_t pictureCount = 0;
    size_t sceneNodeCount = 0;
    size_t semanticNodeCount = 0;
    size_t geometryNodeCount = 0;
    size_t textNodeCount = 0;
    size_t rasterNodeCount = 0;
    size_t preservedCount = 0;
    size_t degradedCount = 0;
    size_t droppedCount = 0;
    size_t issueCount = 0;
};

struct RulePackReference {
    std::string name = "builtin";
    std::string version = "phase2a";
    std::string source = "native-core";
};

struct ConversionDocumentModel {
    std::string inputPath;
    std::string logicalName;
    ProfileDetector::DetectionResult detectedProfile{ProfileType::UNKNOWN, "", false, ProfileMetadata{}};
    RulePackReference rulePack;
    size_t commandCount = 0;
    size_t pictureCount = 0;
    std::vector<SceneNode> sceneNodes;
    std::vector<SemanticNode> semanticNodes;
    std::vector<ConversionIssue> issues;

    DocumentSummary summarize() const;
};

struct ConversionReport {
    ConversionDocumentModel document;
    std::string outputPath;
    bool success = false;
    bool compressedOutput = false;
    bool minifiedOutput = false;
    bool optimizedPaths = false;
    std::optional<GeometryMetrics> geometryMetrics;
    std::vector<ConversionIssue> runtimeIssues;

    std::string generateJsonReport() const;
    std::string generateTextReport() const;
};

class ConversionDocumentBuilder {
public:
    static ConversionDocumentModel fromCgm(const CGMFile& cgmFile, const std::string& inputPath);
};

} // namespace opencgm

#endif // OPENCGM_DOCUMENT_MODEL_H
