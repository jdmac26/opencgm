#include "opencgm/commands/picture_descriptor_commands.h"
#include "opencgm/interfaces.h"
#include "opencgm/cgm_file.h"

namespace opencgm {

// ============================================================================
// SCALING MODE
// ============================================================================

ScalingMode::ScalingMode(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::PictureDescriptorElements, 1, container)),
      mode_(SpecificationMode::ABS),
      metricScaleFactor_(1.0) {}

void ScalingMode::readFromBinary(IBinaryReader& reader) {
    int modeValue = reader.readEnum();
    mode_ = (modeValue == 0) ? SpecificationMode::ABS : SpecificationMode::SCALED;

    if (mode_ == SpecificationMode::SCALED) {
        metricScaleFactor_ = reader.readReal();
    }

    if (container_) {
        container_->setScalingMode(mode_);
        container_->setMetricScaleFactor(metricScaleFactor_);
    }
}

void ScalingMode::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(mode_ == SpecificationMode::ABS ? 0 : 1);
    if (mode_ == SpecificationMode::SCALED) {
        writer.writeReal(metricScaleFactor_);
    }
}

void ScalingMode::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("SCALEMODE ");
    if (mode_ == SpecificationMode::ABS) {
        writer.writeString("ABSTRACT");
    } else {
        writer.writeString("METRIC ");
        writer.writeString(std::to_string(metricScaleFactor_));
    }
    writer.writeString(";");
}

std::string ScalingMode::toString() const {
    std::string result = "ScalingMode[";
    result += (mode_ == SpecificationMode::ABS) ? "ABSTRACT" : "SCALED";
    if (mode_ == SpecificationMode::SCALED) {
        result += ", factor=" + std::to_string(metricScaleFactor_);
    }
    result += "]";
    return result;
}

// ============================================================================
// COLOUR SELECTION MODE
// ============================================================================

ColourSelectionMode::ColourSelectionMode(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::PictureDescriptorElements, 2, container)),
      mode_(ColorSelectionMode::INDEXED) {}

void ColourSelectionMode::readFromBinary(IBinaryReader& reader) {
    int modeValue = reader.readEnum();
    mode_ = (modeValue == 0) ? ColorSelectionMode::INDEXED : ColorSelectionMode::DIRECT;

    if (container_) {
        container_->setColorSelectionMode(mode_);
    }
}

void ColourSelectionMode::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(mode_ == ColorSelectionMode::INDEXED ? 0 : 1);
}

void ColourSelectionMode::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("COLRMODE ");
    writer.writeString(mode_ == ColorSelectionMode::INDEXED ? "INDEXED" : "DIRECT");
    writer.writeString(";");
}

std::string ColourSelectionMode::toString() const {
    return std::string("ColourSelectionMode[") +
           (mode_ == ColorSelectionMode::INDEXED ? "INDEXED" : "DIRECT") + "]";
}

// ============================================================================
// LINE WIDTH SPECIFICATION MODE
// ============================================================================

LineWidthSpecificationMode::LineWidthSpecificationMode(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::PictureDescriptorElements, 3, container)),
      mode_(SpecificationMode::ABS) {}

void LineWidthSpecificationMode::readFromBinary(IBinaryReader& reader) {
    int modeValue = reader.readEnum();
    mode_ = (modeValue == 0) ? SpecificationMode::ABS : SpecificationMode::SCALED;

    if (container_) {
        container_->setLineWidthSpecificationMode(mode_);
    }
}

void LineWidthSpecificationMode::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(mode_ == SpecificationMode::ABS ? 0 : 1);
}

void LineWidthSpecificationMode::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("LINEWIDTHMODE ");
    writer.writeString(mode_ == SpecificationMode::ABS ? "ABSTRACT" : "SCALED");
    writer.writeString(";");
}

std::string LineWidthSpecificationMode::toString() const {
    return std::string("LineWidthSpecificationMode[") +
           (mode_ == SpecificationMode::ABS ? "ABSTRACT" : "SCALED") + "]";
}

// ============================================================================
// MARKER SIZE SPECIFICATION MODE
// ============================================================================

MarkerSizeSpecificationMode::MarkerSizeSpecificationMode(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::PictureDescriptorElements, 4, container)),
      mode_(SpecificationMode::ABS) {}

void MarkerSizeSpecificationMode::readFromBinary(IBinaryReader& reader) {
    int modeValue = reader.readEnum();
    mode_ = (modeValue == 0) ? SpecificationMode::ABS : SpecificationMode::SCALED;

    if (container_) {
        container_->setMarkerSizeSpecificationMode(mode_);
    }
}

void MarkerSizeSpecificationMode::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(mode_ == SpecificationMode::ABS ? 0 : 1);
}

void MarkerSizeSpecificationMode::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("MARKERSIZEMODE ");
    writer.writeString(mode_ == SpecificationMode::ABS ? "ABSTRACT" : "SCALED");
    writer.writeString(";");
}

std::string MarkerSizeSpecificationMode::toString() const {
    return std::string("MarkerSizeSpecificationMode[") +
           (mode_ == SpecificationMode::ABS ? "ABSTRACT" : "SCALED") + "]";
}

// ============================================================================
// EDGE WIDTH SPECIFICATION MODE
// ============================================================================

EdgeWidthSpecificationMode::EdgeWidthSpecificationMode(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::PictureDescriptorElements, 5, container)),
      mode_(SpecificationMode::ABS) {}

void EdgeWidthSpecificationMode::readFromBinary(IBinaryReader& reader) {
    int modeValue = reader.readEnum();
    mode_ = (modeValue == 0) ? SpecificationMode::ABS : SpecificationMode::SCALED;

    if (container_) {
        container_->setEdgeWidthSpecificationMode(mode_);
    }
}

void EdgeWidthSpecificationMode::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(mode_ == SpecificationMode::ABS ? 0 : 1);
}

void EdgeWidthSpecificationMode::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("EDGEWIDTHMODE ");
    writer.writeString(mode_ == SpecificationMode::ABS ? "ABSTRACT" : "SCALED");
    writer.writeString(";");
}

std::string EdgeWidthSpecificationMode::toString() const {
    return std::string("EdgeWidthSpecificationMode[") +
           (mode_ == SpecificationMode::ABS ? "ABSTRACT" : "SCALED") + "]";
}

// ============================================================================
// VDC EXTENT
// ============================================================================

VDCExtent::VDCExtent(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::PictureDescriptorElements, 6, container)),
      firstCorner_(0, 0),
      secondCorner_(0, 0) {}

void VDCExtent::readFromBinary(IBinaryReader& reader) {
    firstCorner_ = reader.readPoint();
    secondCorner_ = reader.readPoint();

    if (container_) {
        container_->setVdcExtent(firstCorner_, secondCorner_);
    }
}

void VDCExtent::writeAsBinary(IBinaryWriter& writer) {
    writer.writePoint(firstCorner_);
    writer.writePoint(secondCorner_);
}

void VDCExtent::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("VDCEXT (");
    writer.writeString(std::to_string(firstCorner_.x()) + "," +
                      std::to_string(firstCorner_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(secondCorner_.x()) + "," +
                      std::to_string(secondCorner_.y()));
    writer.writeString(");");
}

std::string VDCExtent::toString() const {
    return "VDCExtent[(" +
           std::to_string(firstCorner_.x()) + "," +
           std::to_string(firstCorner_.y()) + ") (" +
           std::to_string(secondCorner_.x()) + "," +
           std::to_string(secondCorner_.y()) + ")]";
}

// ============================================================================
// BACKGROUND COLOUR
// ============================================================================

BackgroundColour::BackgroundColour(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::PictureDescriptorElements, 7, container)),
      color_(255, 255, 255) {}

void BackgroundColour::readFromBinary(IBinaryReader& reader) {
    color_ = reader.readDirectColor();
}

void BackgroundColour::writeAsBinary(IBinaryWriter& writer) {
    writer.writeColor(CGMColor(color_));
}

void BackgroundColour::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("BACKCOLR ");
    writer.writeString(std::to_string(color_.r) + " " +
                      std::to_string(color_.g) + " " +
                      std::to_string(color_.b));
    writer.writeString(";");
}

std::string BackgroundColour::toString() const {
    return "BackgroundColour[RGB(" +
           std::to_string(color_.r) + "," +
           std::to_string(color_.g) + "," +
           std::to_string(color_.b) + ")]";
}

// ============================================================================
// DEVICE VIEWPORT
// ============================================================================

DeviceViewport::DeviceViewport(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::PictureDescriptorElements, 8, container)),
      firstCorner_(0, 0),
      secondCorner_(0, 0) {}

void DeviceViewport::readFromBinary(IBinaryReader& reader) {
    firstCorner_ = reader.readPoint();
    secondCorner_ = reader.readPoint();
}

void DeviceViewport::writeAsBinary(IBinaryWriter& writer) {
    writer.writePoint(firstCorner_);
    writer.writePoint(secondCorner_);
}

void DeviceViewport::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("DEVVP (");
    writer.writeString(std::to_string(firstCorner_.x()) + "," +
                      std::to_string(firstCorner_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(secondCorner_.x()) + "," +
                      std::to_string(secondCorner_.y()));
    writer.writeString(");");
}

std::string DeviceViewport::toString() const {
    return "DeviceViewport[(" +
           std::to_string(firstCorner_.x()) + "," +
           std::to_string(firstCorner_.y()) + ") (" +
           std::to_string(secondCorner_.x()) + "," +
           std::to_string(secondCorner_.y()) + ")]";
}

// ============================================================================
// DEVICE VIEWPORT SPECIFICATION MODE
// ============================================================================

DeviceViewportSpecificationMode::DeviceViewportSpecificationMode(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::PictureDescriptorElements, 9, container)),
      mode_(0),
      scaleFactor_(1.0) {}

void DeviceViewportSpecificationMode::readFromBinary(IBinaryReader& reader) {
    mode_ = reader.readEnum();
    scaleFactor_ = reader.readReal();
}

void DeviceViewportSpecificationMode::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(mode_);
    writer.writeReal(scaleFactor_);
}

void DeviceViewportSpecificationMode::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("DEVVPMODE ");
    switch (mode_) {
        case 0: writer.writeString("FRACTION"); break;
        case 1: writer.writeString("MM"); break;
        case 2: writer.writeString("PHYSDEV"); break;
        default: writer.writeString(std::to_string(mode_)); break;
    }
    writer.writeString(" ");
    writer.writeString(std::to_string(scaleFactor_));
    writer.writeString(";");
}

std::string DeviceViewportSpecificationMode::toString() const {
    return "DeviceViewportSpecificationMode[mode=" +
           std::to_string(mode_) + ", scale=" +
           std::to_string(scaleFactor_) + "]";
}

// ============================================================================
// DEVICE VIEWPORT MAPPING
// ============================================================================

DeviceViewportMapping::DeviceViewportMapping(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::PictureDescriptorElements, 10, container)),
      horizontalAlignment_(0),
      verticalAlignment_(0),
      mapping_(0) {}

void DeviceViewportMapping::readFromBinary(IBinaryReader& reader) {
    horizontalAlignment_ = reader.readEnum();
    verticalAlignment_ = reader.readEnum();
    mapping_ = reader.readEnum();
}

void DeviceViewportMapping::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(horizontalAlignment_);
    writer.writeEnum(verticalAlignment_);
    writer.writeEnum(mapping_);
}

void DeviceViewportMapping::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("DEVVPMAP ");
    writer.writeString(std::to_string(horizontalAlignment_));
    writer.writeString(" ");
    writer.writeString(std::to_string(verticalAlignment_));
    writer.writeString(" ");
    writer.writeString(mapping_ == 0 ? "NOTFORCED" : "FORCED");
    writer.writeString(";");
}

std::string DeviceViewportMapping::toString() const {
    return "DeviceViewportMapping[h=" +
           std::to_string(horizontalAlignment_) + ", v=" +
           std::to_string(verticalAlignment_) + ", " +
           (mapping_ == 0 ? "not forced" : "forced") + "]";
}

// ============================================================================
// LINE REPRESENTATION
// ============================================================================

LineRepresentation::LineRepresentation(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::PictureDescriptorElements, 11, container)),
      bundleIndex_(1),
      lineType_(1),
      lineWidth_(1.0),
      color_(Color::Black()) {}

void LineRepresentation::readFromBinary(IBinaryReader& reader) {
    bundleIndex_ = reader.readIndex();
    lineType_ = reader.readIndex();
    lineWidth_ = reader.readVdc();
    color_ = reader.readColor();
}

void LineRepresentation::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(bundleIndex_);
    writer.writeIndex(lineType_);
    writer.writeVdc(lineWidth_);
    writer.writeColor(color_);
}

void LineRepresentation::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("LINEREP /* bundle */;");
}

std::string LineRepresentation::toString() const {
    return "LineRepresentation[bundle=" + std::to_string(bundleIndex_) +
           ", type=" + std::to_string(lineType_) +
           ", width=" + std::to_string(lineWidth_) + "]";
}

// ============================================================================
// MARKER REPRESENTATION
// ============================================================================

MarkerRepresentation::MarkerRepresentation(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::PictureDescriptorElements, 12, container)),
      bundleIndex_(1),
      markerType_(1),
      markerSize_(1.0),
      color_(Color::Black()) {}

void MarkerRepresentation::readFromBinary(IBinaryReader& reader) {
    bundleIndex_ = reader.readIndex();
    markerType_ = reader.readIndex();
    markerSize_ = reader.readVdc();
    color_ = reader.readColor();
}

void MarkerRepresentation::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(bundleIndex_);
    writer.writeIndex(markerType_);
    writer.writeVdc(markerSize_);
    writer.writeColor(color_);
}

void MarkerRepresentation::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("MARKERREP /* bundle */;");
}

std::string MarkerRepresentation::toString() const {
    return "MarkerRepresentation[bundle=" + std::to_string(bundleIndex_) +
           ", type=" + std::to_string(markerType_) +
           ", size=" + std::to_string(markerSize_) + "]";
}

// ============================================================================
// TEXT REPRESENTATION
// ============================================================================

TextRepresentation::TextRepresentation(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::PictureDescriptorElements, 13, container)),
      bundleIndex_(1),
      fontIndex_(1),
      textPrecision_(0),
      characterExpansion_(1.0),
      characterSpacing_(0.0),
      color_(Color::Black()) {}

void TextRepresentation::readFromBinary(IBinaryReader& reader) {
    bundleIndex_ = reader.readIndex();
    fontIndex_ = reader.readIndex();
    textPrecision_ = reader.readEnum();
    characterExpansion_ = reader.readReal();
    characterSpacing_ = reader.readReal();
    color_ = reader.readColor();
}

void TextRepresentation::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(bundleIndex_);
    writer.writeIndex(fontIndex_);
    writer.writeEnum(textPrecision_);
    writer.writeReal(characterExpansion_);
    writer.writeReal(characterSpacing_);
    writer.writeColor(color_);
}

void TextRepresentation::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("TEXTREP /* bundle */;");
}

std::string TextRepresentation::toString() const {
    return "TextRepresentation[bundle=" + std::to_string(bundleIndex_) +
           ", font=" + std::to_string(fontIndex_) + "]";
}

// ============================================================================
// FILL REPRESENTATION
// ============================================================================

FillRepresentation::FillRepresentation(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::PictureDescriptorElements, 14, container)),
      bundleIndex_(1),
      interiorStyle_(1),
      color_(Color::White()),
      hatchIndex_(1),
      patternIndex_(1) {}

void FillRepresentation::readFromBinary(IBinaryReader& reader) {
    bundleIndex_ = reader.readIndex();
    interiorStyle_ = reader.readEnum();
    color_ = reader.readColor();
    hatchIndex_ = reader.readIndex();
    patternIndex_ = reader.readIndex();
}

void FillRepresentation::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(bundleIndex_);
    writer.writeEnum(interiorStyle_);
    writer.writeColor(color_);
    writer.writeIndex(hatchIndex_);
    writer.writeIndex(patternIndex_);
}

void FillRepresentation::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("FILLREP /* bundle */;");
}

std::string FillRepresentation::toString() const {
    return "FillRepresentation[bundle=" + std::to_string(bundleIndex_) +
           ", style=" + std::to_string(interiorStyle_) + "]";
}

// ============================================================================
// EDGE REPRESENTATION
// ============================================================================

EdgeRepresentation::EdgeRepresentation(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::PictureDescriptorElements, 15, container)),
      bundleIndex_(1),
      edgeType_(1),
      edgeWidth_(1.0),
      color_(Color::Black()) {}

void EdgeRepresentation::readFromBinary(IBinaryReader& reader) {
    bundleIndex_ = reader.readIndex();
    edgeType_ = reader.readIndex();
    edgeWidth_ = reader.readVdc();
    color_ = reader.readColor();
}

void EdgeRepresentation::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(bundleIndex_);
    writer.writeIndex(edgeType_);
    writer.writeVdc(edgeWidth_);
    writer.writeColor(color_);
}

void EdgeRepresentation::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("EDGEREP /* bundle */;");
}

std::string EdgeRepresentation::toString() const {
    return "EdgeRepresentation[bundle=" + std::to_string(bundleIndex_) +
           ", type=" + std::to_string(edgeType_) +
           ", width=" + std::to_string(edgeWidth_) + "]";
}

// ============================================================================
// INTERIOR STYLE SPECIFICATION MODE
// ============================================================================

InteriorStyleSpecificationMode::InteriorStyleSpecificationMode(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::PictureDescriptorElements, 16, container)),
      mode_(SpecificationMode::ABS) {}

void InteriorStyleSpecificationMode::readFromBinary(IBinaryReader& reader) {
    int modeValue = reader.readEnum();
    mode_ = (modeValue == 0) ? SpecificationMode::ABS : SpecificationMode::SCALED;
}

void InteriorStyleSpecificationMode::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(mode_ == SpecificationMode::ABS ? 0 : 1);
}

void InteriorStyleSpecificationMode::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("INTSTYLEMODE ");
    writer.writeString(mode_ == SpecificationMode::ABS ? "ABSTRACT" : "SCALED");
    writer.writeString(";");
}

std::string InteriorStyleSpecificationMode::toString() const {
    return std::string("InteriorStyleSpecificationMode[") +
           (mode_ == SpecificationMode::ABS ? "ABSTRACT" : "SCALED") + "]";
}

// ============================================================================
// LINE AND EDGE TYPE DEFINITION
// ============================================================================

LineAndEdgeTypeDefinition::LineAndEdgeTypeDefinition(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::PictureDescriptorElements, 17, container)),
      lineType_(1) {}

void LineAndEdgeTypeDefinition::readFromBinary(IBinaryReader& reader) {
    lineType_ = reader.readIndex();

    while (reader.hasMoreData()) {
        dashPattern_.push_back(reader.readInt());
    }
}

void LineAndEdgeTypeDefinition::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(lineType_);
    for (int dash : dashPattern_) {
        writer.writeInt(dash);
    }
}

void LineAndEdgeTypeDefinition::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("LINETYPEDEF /* custom */;");
}

std::string LineAndEdgeTypeDefinition::toString() const {
    return "LineAndEdgeTypeDefinition[type=" + std::to_string(lineType_) +
           ", " + std::to_string(dashPattern_.size()) + " dash values]";
}

// ============================================================================
// HATCH STYLE DEFINITION
// ============================================================================

HatchStyleDefinition::HatchStyleDefinition(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::PictureDescriptorElements, 18, container)),
      hatchIndex_(1),
      styleIndicator_(0),
      direction_(0, 0),
      spacing_(1, 1) {}

void HatchStyleDefinition::readFromBinary(IBinaryReader& reader) {
    hatchIndex_ = reader.readIndex();
    styleIndicator_ = reader.readEnum();

    if (styleIndicator_ == 0) {
        // Parallel lines
        direction_ = reader.readPoint();
        spacing_ = reader.readPoint();
    }
    // Other styles would read more data here
}

void HatchStyleDefinition::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(hatchIndex_);
    writer.writeEnum(styleIndicator_);

    if (styleIndicator_ == 0) {
        writer.writePoint(direction_);
        writer.writePoint(spacing_);
    }
}

void HatchStyleDefinition::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("HATCHDEF /* custom */;");
}

std::string HatchStyleDefinition::toString() const {
    return "HatchStyleDefinition[index=" + std::to_string(hatchIndex_) + "]";
}

// ============================================================================
// GEOMETRIC PATTERN DEFINITION
// ============================================================================

GeometricPatternDefinition::GeometricPatternDefinition(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::PictureDescriptorElements, 19, container)),
      patternIndex_(1),
      segmentIdentifier_(0),
      firstCorner_(0, 0),
      secondCorner_(1, 1) {}

void GeometricPatternDefinition::readFromBinary(IBinaryReader& reader) {
    patternIndex_ = reader.readIndex();
    segmentIdentifier_ = reader.readInt();
    firstCorner_ = reader.readPoint();
    secondCorner_ = reader.readPoint();
}

void GeometricPatternDefinition::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(patternIndex_);
    writer.writeInt(segmentIdentifier_);
    writer.writePoint(firstCorner_);
    writer.writePoint(secondCorner_);
}

void GeometricPatternDefinition::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("PATDEF /* geometric */;");
}

std::string GeometricPatternDefinition::toString() const {
    return "GeometricPatternDefinition[index=" + std::to_string(patternIndex_) +
           ", segment=" + std::to_string(segmentIdentifier_) + "]";
}

} // namespace opencgm