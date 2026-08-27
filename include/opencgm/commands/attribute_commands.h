#ifndef OPENCGM_ATTRIBUTE_COMMANDS_H
#define OPENCGM_ATTRIBUTE_COMMANDS_H

#include "../command.h"
#include "../cgm_color.h"
#include "../cgm_point.h"
#include <vector>
#include <string>

namespace opencgm {

// Element ID 2: LINE TYPE
class LineType : public Command {
public:
    explicit LineType(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int type() const { return type_; }

private:
    int type_; // 1=solid, 2=dash, 3=dot, 4=dash-dot, 5=dash-dot-dot
};

// Element ID 3: LINE WIDTH
class LineWidth : public Command {
public:
    explicit LineWidth(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    double width() const { return width_; }

private:
    double width_;
};

// Element ID 4: LINE COLOUR
class LineColour : public Command {
public:
    explicit LineColour(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMColor& color() const { return color_; }

private:
    CGMColor color_;
};

// Element ID 5: MARKER TYPE
class MarkerType : public Command {
public:
    explicit MarkerType(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int type() const { return type_; }

private:
    int type_; // 1=dot, 2=plus, 3=asterisk, 4=circle, 5=cross
};

// Element ID 6: MARKER SIZE
class MarkerSize : public Command {
public:
    explicit MarkerSize(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    double size() const { return size_; }

private:
    double size_;
};

// Element ID 7: MARKER COLOUR
class MarkerColour : public Command {
public:
    explicit MarkerColour(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMColor& color() const { return color_; }

private:
    CGMColor color_;
};

// Element ID 10: TEXT FONT INDEX
class TextFontIndex : public Command {
public:
    explicit TextFontIndex(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int index() const { return index_; }

private:
    int index_;
};

// Element ID 12: CHARACTER EXPANSION FACTOR
class CharacterExpansionFactor : public Command {
public:
    explicit CharacterExpansionFactor(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    double factor() const { return factor_; }

private:
    double factor_;
};

// Element ID 13: CHARACTER SPACING
class CharacterSpacing : public Command {
public:
    explicit CharacterSpacing(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    double spacing() const { return spacing_; }

private:
    double spacing_;
};

// Element ID 14: TEXT COLOUR
class TextColour : public Command {
public:
    explicit TextColour(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMColor& color() const { return color_; }

private:
    CGMColor color_;
};

// Element ID 15: CHARACTER HEIGHT
class CharacterHeight : public Command {
public:
    explicit CharacterHeight(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    double height() const { return height_; }

private:
    double height_;
};

// Element ID 16: CHARACTER ORIENTATION
class CharacterOrientation : public Command {
public:
    explicit CharacterOrientation(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMPoint& xUp() const { return xUp_; }
    const CGMPoint& yUp() const { return yUp_; }

private:
    CGMPoint xUp_;
    CGMPoint yUp_;
};

// Element ID 18: TEXT ALIGNMENT
class TextAlignment : public Command {
public:
    explicit TextAlignment(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int horizontalAlignment() const { return horizontalAlignment_; }
    int verticalAlignment() const { return verticalAlignment_; }
    double continuousHorizontal() const { return continuousHorizontal_; }
    double continuousVertical() const { return continuousVertical_; }

private:
    int horizontalAlignment_; // 0=normal, 1=left, 2=center, 3=right, 4=continuous
    int verticalAlignment_;   // 0=normal, 1=top, 2=cap, 3=half, 4=base, 5=bottom, 6=continuous
    double continuousHorizontal_;
    double continuousVertical_;
};

// Element ID 21: INTERIOR STYLE
class InteriorStyle : public Command {
public:
    explicit InteriorStyle(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int style() const { return style_; }

private:
    int style_; // 0=hollow, 1=solid, 2=pattern, 3=hatch, 4=empty
};

// Element ID 22: FILL COLOUR
class FillColour : public Command {
public:
    explicit FillColour(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMColor& color() const { return color_; }

private:
    CGMColor color_;
};

// Element ID 23: HATCH INDEX
class HatchIndex : public Command {
public:
    explicit HatchIndex(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int index() const { return index_; }

private:
    int index_; // 1=horizontal, 2=vertical, 3=positive, 4=negative, 5=horiz/vert, 6=pos/neg
};

// Element ID 24: PATTERN INDEX
class PatternIndex : public Command {
public:
    explicit PatternIndex(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int index() const { return index_; }

private:
    int index_;
};

// Element ID 27: EDGE TYPE
class EdgeType : public Command {
public:
    explicit EdgeType(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int type() const { return type_; }

private:
    int type_;
};

// Element ID 28: EDGE WIDTH
class EdgeWidth : public Command {
public:
    explicit EdgeWidth(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    double width() const { return width_; }

private:
    double width_;
};

// Element ID 29: EDGE COLOUR
class EdgeColour : public Command {
public:
    explicit EdgeColour(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMColor& color() const { return color_; }

private:
    CGMColor color_;
};

// Element ID 30: EDGE VISIBILITY
class EdgeVisibility : public Command {
public:
    explicit EdgeVisibility(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    bool isVisible() const { return isVisible_; }

private:
    bool isVisible_;
};

// Element ID 33: COLOUR TABLE
class ColourTable : public Command {
public:
    explicit ColourTable(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int startIndex() const { return startIndex_; }
    const std::vector<Color>& colors() const { return colors_; }

private:
    int startIndex_;
    std::vector<Color> colors_;
};

// ============================================================================
// HIGH PRIORITY MISSING COMMANDS - BUNDLE INDICES
// ============================================================================

// Element ID 1: LINE BUNDLE INDEX
class LineBundleIndex : public Command {
public:
    explicit LineBundleIndex(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int index() const { return index_; }

private:
    int index_;
};

// Element ID 5: MARKER BUNDLE INDEX
class MarkerBundleIndex : public Command {
public:
    explicit MarkerBundleIndex(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int index() const { return index_; }

private:
    int index_;
};

// Element ID 9: TEXT BUNDLE INDEX
class TextBundleIndex : public Command {
public:
    explicit TextBundleIndex(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int index() const { return index_; }

private:
    int index_;
};

// Element ID 25: FILL BUNDLE INDEX
class FillBundleIndex : public Command {
public:
    explicit FillBundleIndex(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int index() const { return index_; }

private:
    int index_;
};

// Element ID 26: EDGE BUNDLE INDEX
class EdgeBundleIndex : public Command {
public:
    explicit EdgeBundleIndex(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int index() const { return index_; }

private:
    int index_;
};

// ============================================================================
// HIGH PRIORITY MISSING COMMANDS - TEXT ATTRIBUTES
// ============================================================================

// Element ID 11: TEXT PRECISION
class TextPrecision : public Command {
public:
    explicit TextPrecision(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    TextPrecisionType value() const { return value_; }

private:
    TextPrecisionType value_;
};

// Element ID 17: TEXT PATH
class TextPath : public Command {
public:
    explicit TextPath(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    TextPathType path() const { return path_; }

private:
    TextPathType path_;
};

// Element ID 19: CHARACTER SET INDEX
class CharacterSetIndex : public Command {
public:
    explicit CharacterSetIndex(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int index() const { return index_; }

private:
    int index_;
};

// Element ID 20: ALTERNATE CHARACTER SET INDEX
class AlternateCharacterSetIndex : public Command {
public:
    explicit AlternateCharacterSetIndex(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int index() const { return index_; }

private:
    int index_;
};

// ============================================================================
// HIGH PRIORITY MISSING COMMANDS - PATTERN/FILL
// ============================================================================

// Element ID 31: FILL REFERENCE POINT
class FillReferencePoint : public Command {
public:
    explicit FillReferencePoint(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMPoint& point() const { return point_; }

private:
    CGMPoint point_;
};

// Element ID 32: PATTERN TABLE
class PatternTable : public Command {
public:
    explicit PatternTable(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int index() const { return index_; }
    int nx() const { return nx_; }
    int ny() const { return ny_; }
    int localColorPrecision() const { return localColorPrecision_; }
    const std::vector<CGMColor>& colors() const { return colors_; }

private:
    int index_;
    int nx_;
    int ny_;
    int localColorPrecision_;
    std::vector<CGMColor> colors_;
};

// Element ID 33: PATTERN SIZE
class PatternSize : public Command {
public:
    explicit PatternSize(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    double heightX() const { return heightX_; }
    double heightY() const { return heightY_; }
    double widthX() const { return widthX_; }
    double widthY() const { return widthY_; }

private:
    double heightX_;
    double heightY_;
    double widthX_;
    double widthY_;
};

// ============================================================================
// MEDIUM PRIORITY MISSING COMMANDS - LINE/EDGE STYLING
// ============================================================================

// Element ID 37: LINE CAP
class LineCap : public Command {
public:
    explicit LineCap(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    LineCapIndicator lineIndicator() const { return lineIndicator_; }
    DashCapIndicator dashIndicator() const { return dashIndicator_; }

private:
    LineCapIndicator lineIndicator_;
    DashCapIndicator dashIndicator_;
};

// Element ID 38: LINE JOIN
class LineJoin : public Command {
public:
    explicit LineJoin(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    JoinIndicator type() const { return type_; }

private:
    JoinIndicator type_;
};

// Element ID 39: LINE TYPE CONTINUATION
class LineTypeContinuation : public Command {
public:
    explicit LineTypeContinuation(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int mode() const { return mode_; }

private:
    int mode_;
};

// Element ID 40: LINE TYPE INITIAL OFFSET
class LineTypeInitialOffset : public Command {
public:
    explicit LineTypeInitialOffset(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    double offset() const { return offset_; }

private:
    double offset_;
};

// Element ID 44: EDGE CAP
class EdgeCap : public Command {
public:
    explicit EdgeCap(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    LineCapIndicator lineIndicator() const { return lineIndicator_; }
    DashCapIndicator dashIndicator() const { return dashIndicator_; }

private:
    LineCapIndicator lineIndicator_;
    DashCapIndicator dashIndicator_;
};

// Element ID 45: EDGE JOIN
class EdgeJoin : public Command {
public:
    explicit EdgeJoin(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    JoinIndicator type() const { return type_; }

private:
    JoinIndicator type_;
};

// Element ID 46: EDGE TYPE CONTINUATION
class EdgeTypeContinuation : public Command {
public:
    explicit EdgeTypeContinuation(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int mode() const { return mode_; }

private:
    int mode_;
};

// Element ID 47: EDGE TYPE INITIAL OFFSET
class EdgeTypeInitialOffset : public Command {
public:
    explicit EdgeTypeInitialOffset(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    double offset() const { return offset_; }

private:
    double offset_;
};

// ============================================================================
// REMAINING ATTRIBUTE COMMANDS
// ============================================================================

// Element ID 35: ASPECT SOURCE FLAGS
class AspectSourceFlags : public Command {
public:
    struct AspectSourceFlagsInfo {
        AspectSourceFlagType type;
        AspectSourceFlagValue value;
    };

    explicit AspectSourceFlags(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const std::vector<AspectSourceFlagsInfo>& flags() const { return flags_; }

private:
    std::vector<AspectSourceFlagsInfo> flags_;
};

// Element ID 36: PICK IDENTIFIER
class PickIdentifier : public Command {
public:
    explicit PickIdentifier(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int identifier() const { return identifier_; }

private:
    int identifier_;
};

// Element ID 41: TEXT SCORE TYPE
class TextScoreType : public Command {
public:
    struct TSInfo {
        TextScoreTypeValue type;
        int indicator;
    };

    explicit TextScoreType(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const std::vector<TSInfo>& scoreTypes() const { return scoreTypes_; }

private:
    std::vector<TSInfo> scoreTypes_;
};

// Element ID 42: RESTRICTED TEXT TYPE
class RestrictedTextType : public Command {
public:
    explicit RestrictedTextType(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    RestrictedTextTypeValue type() const { return type_; }

private:
    RestrictedTextTypeValue type_;
};

// Element ID 43: INTERPOLATED INTERIOR
class InterpolatedInterior : public Command {
public:
    explicit InterpolatedInterior(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int style() const { return style_; }
    double hatchDirectionX() const { return hatchDirectionX_; }
    double hatchDirectionY() const { return hatchDirectionY_; }
    const std::vector<CGMPoint>& referenceGeometry() const { return referenceGeometry_; }
    const std::vector<int>& stageDesignators() const { return stageDesignators_; }
    const std::vector<CGMColor>& colors() const { return colors_; }

private:
    int style_;
    double hatchDirectionX_;
    double hatchDirectionY_;
    std::vector<CGMPoint> referenceGeometry_;
    std::vector<int> stageDesignators_;
    std::vector<CGMColor> colors_;
};

} // namespace opencgm

#endif // OPENCGM_ATTRIBUTE_COMMANDS_H