#ifndef OPENCGM_PICTURE_DESCRIPTOR_COMMANDS_H
#define OPENCGM_PICTURE_DESCRIPTOR_COMMANDS_H

#include "../command.h"
#include "../enums.h"
#include "../cgm_color.h"
#include "../cgm_point.h"
#include <string>
#include <vector>

namespace opencgm {

// Element ID 1: SCALING MODE
class ScalingMode : public Command {
public:
    explicit ScalingMode(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    SpecificationMode mode() const { return mode_; }
    double metricScaleFactor() const { return metricScaleFactor_; }

private:
    SpecificationMode mode_;
    double metricScaleFactor_;
};

// Element ID 2: COLOUR SELECTION MODE
class ColourSelectionMode : public Command {
public:
    explicit ColourSelectionMode(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    ColorSelectionMode mode() const { return mode_; }

private:
    ColorSelectionMode mode_;
};

// Element ID 3: LINE WIDTH SPECIFICATION MODE
class LineWidthSpecificationMode : public Command {
public:
    explicit LineWidthSpecificationMode(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    SpecificationMode mode() const { return mode_; }

private:
    SpecificationMode mode_;
};

// Element ID 4: MARKER SIZE SPECIFICATION MODE
class MarkerSizeSpecificationMode : public Command {
public:
    explicit MarkerSizeSpecificationMode(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    SpecificationMode mode() const { return mode_; }

private:
    SpecificationMode mode_;
};

// Element ID 5: EDGE WIDTH SPECIFICATION MODE
class EdgeWidthSpecificationMode : public Command {
public:
    explicit EdgeWidthSpecificationMode(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    SpecificationMode mode() const { return mode_; }

private:
    SpecificationMode mode_;
};

// Element ID 6: VDC EXTENT
class VDCExtent : public Command {
public:
    explicit VDCExtent(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMPoint& firstCorner() const { return firstCorner_; }
    const CGMPoint& secondCorner() const { return secondCorner_; }
    void setExtent(const CGMPoint& first, const CGMPoint& second) {
        firstCorner_ = first;
        secondCorner_ = second;
    }

private:
    CGMPoint firstCorner_;
    CGMPoint secondCorner_;
};

// Element ID 7: BACKGROUND COLOUR
class BackgroundColour : public Command {
public:
    explicit BackgroundColour(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const Color& color() const { return color_; }

private:
    Color color_;
};

// Element ID 8: DEVICE VIEWPORT
class DeviceViewport : public Command {
public:
    explicit DeviceViewport(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMPoint& firstCorner() const { return firstCorner_; }
    const CGMPoint& secondCorner() const { return secondCorner_; }

private:
    CGMPoint firstCorner_;
    CGMPoint secondCorner_;
};

// Element ID 9: DEVICE VIEWPORT SPECIFICATION MODE
class DeviceViewportSpecificationMode : public Command {
public:
    explicit DeviceViewportSpecificationMode(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int mode() const { return mode_; }
    double scaleFactor() const { return scaleFactor_; }

private:
    int mode_; // 0=fraction of drawing surface, 1=millimeters, 2=physical device
    double scaleFactor_;
};

// Element ID 10: DEVICE VIEWPORT MAPPING
class DeviceViewportMapping : public Command {
public:
    explicit DeviceViewportMapping(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int horizontalAlignment() const { return horizontalAlignment_; }
    int verticalAlignment() const { return verticalAlignment_; }
    int mapping() const { return mapping_; }

private:
    int horizontalAlignment_; // 0=left, 1=center, 2=right
    int verticalAlignment_;   // 0=bottom, 1=center, 2=top
    int mapping_;             // 0=not forced, 1=forced
};

// Element ID 11: LINE REPRESENTATION
class LineRepresentation : public Command {
public:
    explicit LineRepresentation(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int bundleIndex() const { return bundleIndex_; }
    int lineType() const { return lineType_; }
    double lineWidth() const { return lineWidth_; }
    const CGMColor& color() const { return color_; }

private:
    int bundleIndex_;
    int lineType_;
    double lineWidth_;
    CGMColor color_;
};

// Element ID 12: MARKER REPRESENTATION
class MarkerRepresentation : public Command {
public:
    explicit MarkerRepresentation(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int bundleIndex() const { return bundleIndex_; }
    int markerType() const { return markerType_; }
    double markerSize() const { return markerSize_; }
    const CGMColor& color() const { return color_; }

private:
    int bundleIndex_;
    int markerType_;
    double markerSize_;
    CGMColor color_;
};

// Element ID 13: TEXT REPRESENTATION
class TextRepresentation : public Command {
public:
    explicit TextRepresentation(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int bundleIndex() const { return bundleIndex_; }
    int fontIndex() const { return fontIndex_; }
    int textPrecision() const { return textPrecision_; }
    double characterExpansion() const { return characterExpansion_; }
    double characterSpacing() const { return characterSpacing_; }
    const CGMColor& color() const { return color_; }

private:
    int bundleIndex_;
    int fontIndex_;
    int textPrecision_;
    double characterExpansion_;
    double characterSpacing_;
    CGMColor color_;
};

// Element ID 14: FILL REPRESENTATION
class FillRepresentation : public Command {
public:
    explicit FillRepresentation(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int bundleIndex() const { return bundleIndex_; }
    int interiorStyle() const { return interiorStyle_; }
    const CGMColor& color() const { return color_; }
    int hatchIndex() const { return hatchIndex_; }
    int patternIndex() const { return patternIndex_; }

private:
    int bundleIndex_;
    int interiorStyle_;
    CGMColor color_;
    int hatchIndex_;
    int patternIndex_;
};

// Element ID 15: EDGE REPRESENTATION
class EdgeRepresentation : public Command {
public:
    explicit EdgeRepresentation(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int bundleIndex() const { return bundleIndex_; }
    int edgeType() const { return edgeType_; }
    double edgeWidth() const { return edgeWidth_; }
    const CGMColor& color() const { return color_; }

private:
    int bundleIndex_;
    int edgeType_;
    double edgeWidth_;
    CGMColor color_;
};

// Element ID 16: INTERIOR STYLE SPECIFICATION MODE
class InteriorStyleSpecificationMode : public Command {
public:
    explicit InteriorStyleSpecificationMode(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    SpecificationMode mode() const { return mode_; }

private:
    SpecificationMode mode_;
};

// Element ID 17: LINE AND EDGE TYPE DEFINITION
class LineAndEdgeTypeDefinition : public Command {
public:
    explicit LineAndEdgeTypeDefinition(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int lineType() const { return lineType_; }
    const std::vector<int>& dashPattern() const { return dashPattern_; }

private:
    int lineType_;
    std::vector<int> dashPattern_;
};

// Element ID 18: HATCH STYLE DEFINITION
class HatchStyleDefinition : public Command {
public:
    explicit HatchStyleDefinition(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int hatchIndex() const { return hatchIndex_; }
    int styleIndicator() const { return styleIndicator_; }
    const CGMPoint& direction() const { return direction_; }
    const CGMPoint& spacing() const { return spacing_; }

private:
    int hatchIndex_;
    int styleIndicator_;
    CGMPoint direction_;
    CGMPoint spacing_;
};

// Element ID 19: GEOMETRIC PATTERN DEFINITION
class GeometricPatternDefinition : public Command {
public:
    explicit GeometricPatternDefinition(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

private:
    int patternIndex_;
    int segmentIdentifier_;
    CGMPoint firstCorner_;
    CGMPoint secondCorner_;
};

} // namespace opencgm

#endif // OPENCGM_PICTURE_DESCRIPTOR_COMMANDS_H
