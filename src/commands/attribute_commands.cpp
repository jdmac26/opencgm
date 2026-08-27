#include "opencgm/commands/attribute_commands.h"
#include "opencgm/interfaces.h"
#include "opencgm/cgm_file.h"

namespace opencgm {

// ============================================================================
// LINE TYPE
// ============================================================================

LineType::LineType(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 2, container)),
      type_(1) {}

void LineType::readFromBinary(IBinaryReader& reader) {
    type_ = reader.readIndex();
}

void LineType::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(type_);
}

void LineType::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("LINETYPE ");
    writer.writeString(std::to_string(type_));
    writer.writeString(";");
}

std::string LineType::toString() const {
    return "LineType[" + std::to_string(type_) + "]";
}

// ============================================================================
// LINE WIDTH
// ============================================================================

LineWidth::LineWidth(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 3, container)),
      width_(1.0) {}

void LineWidth::readFromBinary(IBinaryReader& reader) {
    width_ = reader.readSizeSpecification(container_->lineWidthSpecificationMode());
}

void LineWidth::writeAsBinary(IBinaryWriter& writer) {
    writer.writeSizeSpecification(width_, container_->lineWidthSpecificationMode());
}

void LineWidth::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("LINEWIDTH ");
    writer.writeString(std::to_string(width_));
    writer.writeString(";");
}

std::string LineWidth::toString() const {
    return "LineWidth[" + std::to_string(width_) + "]";
}

// ============================================================================
// LINE COLOUR
// ============================================================================

LineColour::LineColour(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 4, container)),
      color_(Color::Black()) {}

void LineColour::readFromBinary(IBinaryReader& reader) {
    color_ = reader.readColor();
}

void LineColour::writeAsBinary(IBinaryWriter& writer) {
    writer.writeColor(color_);
}

void LineColour::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("LINECOLR /* color */;");
}

std::string LineColour::toString() const {
    return "LineColour[" + color_.toString() + "]";
}

// ============================================================================
// MARKER TYPE
// ============================================================================

MarkerType::MarkerType(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 5, container)),
      type_(1) {}

void MarkerType::readFromBinary(IBinaryReader& reader) {
    type_ = reader.readIndex();
}

void MarkerType::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(type_);
}

void MarkerType::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("MARKERTYPE ");
    writer.writeString(std::to_string(type_));
    writer.writeString(";");
}

std::string MarkerType::toString() const {
    return "MarkerType[" + std::to_string(type_) + "]";
}

// ============================================================================
// MARKER SIZE
// ============================================================================

MarkerSize::MarkerSize(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 6, container)),
      size_(1.0) {}

void MarkerSize::readFromBinary(IBinaryReader& reader) {
    size_ = reader.readSizeSpecification(container_->markerSizeSpecificationMode());
}

void MarkerSize::writeAsBinary(IBinaryWriter& writer) {
    writer.writeSizeSpecification(size_, container_->markerSizeSpecificationMode());
}

void MarkerSize::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("MARKERSIZE ");
    writer.writeString(std::to_string(size_));
    writer.writeString(";");
}

std::string MarkerSize::toString() const {
    return "MarkerSize[" + std::to_string(size_) + "]";
}

// ============================================================================
// MARKER COLOUR
// ============================================================================

MarkerColour::MarkerColour(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 7, container)),
      color_(Color::Black()) {}

void MarkerColour::readFromBinary(IBinaryReader& reader) {
    color_ = reader.readColor();
}

void MarkerColour::writeAsBinary(IBinaryWriter& writer) {
    writer.writeColor(color_);
}

void MarkerColour::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("MARKERCOLR /* color */;");
}

std::string MarkerColour::toString() const {
    return "MarkerColour[" + color_.toString() + "]";
}

// ============================================================================
// TEXT FONT INDEX
// ============================================================================

TextFontIndex::TextFontIndex(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 10, container)),
      index_(1) {}

void TextFontIndex::readFromBinary(IBinaryReader& reader) {
    index_ = reader.readIndex();
}

void TextFontIndex::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(index_);
}

void TextFontIndex::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("TEXTFONTINDEX ");
    writer.writeString(std::to_string(index_));
    writer.writeString(";");
}

std::string TextFontIndex::toString() const {
    return "TextFontIndex[" + std::to_string(index_) + "]";
}

// ============================================================================
// CHARACTER EXPANSION FACTOR
// ============================================================================

CharacterExpansionFactor::CharacterExpansionFactor(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 12, container)),
      factor_(1.0) {}

void CharacterExpansionFactor::readFromBinary(IBinaryReader& reader) {
    factor_ = reader.readReal();
}

void CharacterExpansionFactor::writeAsBinary(IBinaryWriter& writer) {
    writer.writeReal(factor_);
}

void CharacterExpansionFactor::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("CHAREXPAN ");
    writer.writeString(std::to_string(factor_));
    writer.writeString(";");
}

std::string CharacterExpansionFactor::toString() const {
    return "CharacterExpansionFactor[" + std::to_string(factor_) + "]";
}

// ============================================================================
// CHARACTER SPACING
// ============================================================================

CharacterSpacing::CharacterSpacing(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 13, container)),
      spacing_(0.0) {}

void CharacterSpacing::readFromBinary(IBinaryReader& reader) {
    spacing_ = reader.readReal();
}

void CharacterSpacing::writeAsBinary(IBinaryWriter& writer) {
    writer.writeReal(spacing_);
}

void CharacterSpacing::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("CHARSPACE ");
    writer.writeString(std::to_string(spacing_));
    writer.writeString(";");
}

std::string CharacterSpacing::toString() const {
    return "CharacterSpacing[" + std::to_string(spacing_) + "]";
}

// ============================================================================
// TEXT COLOUR
// ============================================================================

TextColour::TextColour(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 14, container)),
      color_(Color::Black()) {}

void TextColour::readFromBinary(IBinaryReader& reader) {
    color_ = reader.readColor();
}

void TextColour::writeAsBinary(IBinaryWriter& writer) {
    writer.writeColor(color_);
}

void TextColour::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("TEXTCOLR /* color */;");
}

std::string TextColour::toString() const {
    return "TextColour[" + color_.toString() + "]";
}

// ============================================================================
// CHARACTER HEIGHT
// ============================================================================

CharacterHeight::CharacterHeight(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 15, container)),
      height_(1.0) {}

void CharacterHeight::readFromBinary(IBinaryReader& reader) {
    height_ = reader.readVdc();
}

void CharacterHeight::writeAsBinary(IBinaryWriter& writer) {
    writer.writeVdc(height_);
}

void CharacterHeight::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("CHARHEIGHT ");
    writer.writeString(std::to_string(height_));
    writer.writeString(";");
}

std::string CharacterHeight::toString() const {
    return "CharacterHeight[" + std::to_string(height_) + "]";
}

// ============================================================================
// CHARACTER ORIENTATION
// ============================================================================

CharacterOrientation::CharacterOrientation(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 16, container)),
      xUp_(0, 1),
      yUp_(-1, 0) {}

void CharacterOrientation::readFromBinary(IBinaryReader& reader) {
    xUp_ = reader.readPoint();
    yUp_ = reader.readPoint();
}

void CharacterOrientation::writeAsBinary(IBinaryWriter& writer) {
    writer.writePoint(xUp_);
    writer.writePoint(yUp_);
}

void CharacterOrientation::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("CHARORI /* orientation */;");
}

std::string CharacterOrientation::toString() const {
    return "CharacterOrientation[xUp=(" +
           std::to_string(xUp_.x()) + "," + std::to_string(xUp_.y()) +
           "), yUp=(" +
           std::to_string(yUp_.x()) + "," + std::to_string(yUp_.y()) + ")]";
}

// ============================================================================
// TEXT ALIGNMENT
// ============================================================================

TextAlignment::TextAlignment(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 18, container)),
      horizontalAlignment_(0),
      verticalAlignment_(0),
      continuousHorizontal_(0.0),
      continuousVertical_(0.0) {}

void TextAlignment::readFromBinary(IBinaryReader& reader) {
    horizontalAlignment_ = reader.readEnum();
    verticalAlignment_ = reader.readEnum();
    continuousHorizontal_ = reader.readReal();
    continuousVertical_ = reader.readReal();
}

void TextAlignment::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(horizontalAlignment_);
    writer.writeEnum(verticalAlignment_);
    writer.writeReal(continuousHorizontal_);
    writer.writeReal(continuousVertical_);
}

void TextAlignment::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("TEXTALIGN ");
    writer.writeString(std::to_string(horizontalAlignment_));
    writer.writeString(" ");
    writer.writeString(std::to_string(verticalAlignment_));
    writer.writeString(";");
}

std::string TextAlignment::toString() const {
    return "TextAlignment[h=" + std::to_string(horizontalAlignment_) +
           ", v=" + std::to_string(verticalAlignment_) + "]";
}

// ============================================================================
// INTERIOR STYLE
// ============================================================================

InteriorStyle::InteriorStyle(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 22, container)),  // ISO/IEC 8632-1
      style_(1) {}

void InteriorStyle::readFromBinary(IBinaryReader& reader) {
    style_ = reader.readEnum();
}

void InteriorStyle::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(style_);
}

void InteriorStyle::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("INTSTYLE ");
    switch (style_) {
        case 0: writer.writeString("HOLLOW"); break;
        case 1: writer.writeString("SOLID"); break;
        case 2: writer.writeString("PAT"); break;
        case 3: writer.writeString("HATCH"); break;
        case 4: writer.writeString("EMPTY"); break;
        default: writer.writeString(std::to_string(style_)); break;
    }
    writer.writeString(";");
}

std::string InteriorStyle::toString() const {
    std::string styleName;
    switch (style_) {
        case 0: styleName = "HOLLOW"; break;
        case 1: styleName = "SOLID"; break;
        case 2: styleName = "PATTERN"; break;
        case 3: styleName = "HATCH"; break;
        case 4: styleName = "EMPTY"; break;
        default: styleName = std::to_string(style_); break;
    }
    return "InteriorStyle[" + styleName + "]";
}

// ============================================================================
// FILL COLOUR
// ============================================================================

FillColour::FillColour(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 23, container)),  // ISO/IEC 8632-1
      color_(Color::White()) {}

void FillColour::readFromBinary(IBinaryReader& reader) {
    color_ = reader.readColor();
}

void FillColour::writeAsBinary(IBinaryWriter& writer) {
    writer.writeColor(color_);
}

void FillColour::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("FILLCOLR /* color */;");
}

std::string FillColour::toString() const {
    return "FillColour[" + color_.toString() + "]";
}

// ============================================================================
// HATCH INDEX
// ============================================================================

HatchIndex::HatchIndex(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 24, container)),  // ISO/IEC 8632-1
      index_(1) {}

void HatchIndex::readFromBinary(IBinaryReader& reader) {
    index_ = reader.readIndex();
}

void HatchIndex::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(index_);
}

void HatchIndex::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("HATCHINDEX ");
    writer.writeString(std::to_string(index_));
    writer.writeString(";");
}

std::string HatchIndex::toString() const {
    return "HatchIndex[" + std::to_string(index_) + "]";
}

// ============================================================================
// PATTERN INDEX
// ============================================================================

PatternIndex::PatternIndex(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 25, container)),  // ISO/IEC 8632-1
      index_(1) {}

void PatternIndex::readFromBinary(IBinaryReader& reader) {
    index_ = reader.readIndex();
}

void PatternIndex::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(index_);
}

void PatternIndex::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("PATINDEX ");
    writer.writeString(std::to_string(index_));
    writer.writeString(";");
}

std::string PatternIndex::toString() const {
    return "PatternIndex[" + std::to_string(index_) + "]";
}

// ============================================================================
// EDGE TYPE
// ============================================================================

EdgeType::EdgeType(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 27, container)),
      type_(1) {}

void EdgeType::readFromBinary(IBinaryReader& reader) {
    type_ = reader.readIndex();
}

void EdgeType::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(type_);
}

void EdgeType::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("EDGETYPE ");
    writer.writeString(std::to_string(type_));
    writer.writeString(";");
}

std::string EdgeType::toString() const {
    return "EdgeType[" + std::to_string(type_) + "]";
}

// ============================================================================
// EDGE WIDTH
// ============================================================================

EdgeWidth::EdgeWidth(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 28, container)),
      width_(1.0) {}

void EdgeWidth::readFromBinary(IBinaryReader& reader) {
    width_ = reader.readSizeSpecification(container_->edgeWidthSpecificationMode());
}

void EdgeWidth::writeAsBinary(IBinaryWriter& writer) {
    writer.writeSizeSpecification(width_, container_->edgeWidthSpecificationMode());
}

void EdgeWidth::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("EDGEWIDTH ");
    writer.writeString(std::to_string(width_));
    writer.writeString(";");
}

std::string EdgeWidth::toString() const {
    return "EdgeWidth[" + std::to_string(width_) + "]";
}

// ============================================================================
// EDGE COLOUR
// ============================================================================

EdgeColour::EdgeColour(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 29, container)),
      color_(Color::Black()) {}

void EdgeColour::readFromBinary(IBinaryReader& reader) {
    color_ = reader.readColor();
}

void EdgeColour::writeAsBinary(IBinaryWriter& writer) {
    writer.writeColor(color_);
}

void EdgeColour::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("EDGECOLR /* color */;");
}

std::string EdgeColour::toString() const {
    return "EdgeColour[" + color_.toString() + "]";
}

// ============================================================================
// EDGE VISIBILITY
// ============================================================================

EdgeVisibility::EdgeVisibility(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 30, container)),
      isVisible_(true) {}

void EdgeVisibility::readFromBinary(IBinaryReader& reader) {
    isVisible_ = (reader.readEnum() != 0);
}

void EdgeVisibility::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(isVisible_ ? 1 : 0);
}

void EdgeVisibility::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("EDGEVIS ");
    writer.writeString(isVisible_ ? "ON" : "OFF");
    writer.writeString(";");
}

std::string EdgeVisibility::toString() const {
    return std::string("EdgeVisibility[") + (isVisible_ ? "ON" : "OFF") + "]";
}

// ============================================================================
// COLOUR TABLE
// ============================================================================

ColourTable::ColourTable(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 34, container)),  // ISO 8632-3: Element 34
      startIndex_(0) {}

void ColourTable::readFromBinary(IBinaryReader& reader) {
    startIndex_ = reader.readColorIndex();

    while (reader.hasMoreData()) {
        colors_.push_back(reader.readDirectColor());
    }
}

void ColourTable::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(startIndex_);

    for (const auto& color : colors_) {
        // Write direct color - writeColor handles both indexed and direct
        writer.writeColor(CGMColor(color));
    }
}

void ColourTable::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("COLRTABLE ");
    writer.writeString(std::to_string(startIndex_));
    writer.writeString(" /* ");
    writer.writeString(std::to_string(colors_.size()));
    writer.writeString(" colors */;");
}

std::string ColourTable::toString() const {
    return "ColourTable[start=" + std::to_string(startIndex_) +
           ", count=" + std::to_string(colors_.size()) + "]";
}

// ============================================================================
// LINE BUNDLE INDEX
// ============================================================================

LineBundleIndex::LineBundleIndex(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 1, container)),
      index_(1) {}

void LineBundleIndex::readFromBinary(IBinaryReader& reader) {
    index_ = reader.readIndex();
}

void LineBundleIndex::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(index_);
}

void LineBundleIndex::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("LINEINDEX ");
    writer.writeString(std::to_string(index_));
    writer.writeString(";");
}

std::string LineBundleIndex::toString() const {
    return "LineBundleIndex[" + std::to_string(index_) + "]";
}

// ============================================================================
// MARKER BUNDLE INDEX
// ============================================================================

MarkerBundleIndex::MarkerBundleIndex(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 5, container)),
      index_(1) {}

void MarkerBundleIndex::readFromBinary(IBinaryReader& reader) {
    index_ = reader.readIndex();
}

void MarkerBundleIndex::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(index_);
}

void MarkerBundleIndex::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("MARKERINDEX ");
    writer.writeString(std::to_string(index_));
    writer.writeString(";");
}

std::string MarkerBundleIndex::toString() const {
    return "MarkerBundleIndex[" + std::to_string(index_) + "]";
}

// ============================================================================
// TEXT BUNDLE INDEX
// ============================================================================

TextBundleIndex::TextBundleIndex(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 9, container)),
      index_(1) {}

void TextBundleIndex::readFromBinary(IBinaryReader& reader) {
    index_ = reader.readIndex();
}

void TextBundleIndex::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(index_);
}

void TextBundleIndex::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("TEXTINDEX ");
    writer.writeString(std::to_string(index_));
    writer.writeString(";");
}

std::string TextBundleIndex::toString() const {
    return "TextBundleIndex[" + std::to_string(index_) + "]";
}

// ============================================================================
// FILL BUNDLE INDEX
// ============================================================================

FillBundleIndex::FillBundleIndex(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 21, container)),  // ISO/IEC 8632-1
      index_(1) {}

void FillBundleIndex::readFromBinary(IBinaryReader& reader) {
    index_ = reader.readIndex();
}

void FillBundleIndex::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(index_);
}

void FillBundleIndex::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("FILLINDEX ");
    writer.writeString(std::to_string(index_));
    writer.writeString(";");
}

std::string FillBundleIndex::toString() const {
    return "FillBundleIndex[" + std::to_string(index_) + "]";
}

// ============================================================================
// EDGE BUNDLE INDEX
// ============================================================================

EdgeBundleIndex::EdgeBundleIndex(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 26, container)),
      index_(1) {}

void EdgeBundleIndex::readFromBinary(IBinaryReader& reader) {
    index_ = reader.readIndex();
}

void EdgeBundleIndex::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(index_);
}

void EdgeBundleIndex::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("EDGEINDEX ");
    writer.writeString(std::to_string(index_));
    writer.writeString(";");
}

std::string EdgeBundleIndex::toString() const {
    return "EdgeBundleIndex[" + std::to_string(index_) + "]";
}

// ============================================================================
// TEXT PRECISION
// ============================================================================

TextPrecision::TextPrecision(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 11, container)),
      value_(TextPrecisionType::STRING) {}

void TextPrecision::readFromBinary(IBinaryReader& reader) {
    int enumValue = reader.readEnum();
    switch (enumValue) {
        case 0:
            value_ = TextPrecisionType::STRING;
            break;
        case 1:
            value_ = TextPrecisionType::CHAR;
            break;
        case 2:
            value_ = TextPrecisionType::STROKE;
            break;
        default:
            value_ = TextPrecisionType::STRING;
            reader.unsupported("unsupported text precision " + std::to_string(enumValue));
            break;
    }
}

void TextPrecision::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(static_cast<int>(value_));
}

void TextPrecision::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("TEXTPREC ");
    switch (value_) {
        case TextPrecisionType::STRING: writer.writeString("STRING"); break;
        case TextPrecisionType::CHAR: writer.writeString("CHAR"); break;
        case TextPrecisionType::STROKE: writer.writeString("STROKE"); break;
    }
    writer.writeString(";");
}

std::string TextPrecision::toString() const {
    std::string typeStr;
    switch (value_) {
        case TextPrecisionType::STRING: typeStr = "STRING"; break;
        case TextPrecisionType::CHAR: typeStr = "CHAR"; break;
        case TextPrecisionType::STROKE: typeStr = "STROKE"; break;
    }
    return "TextPrecision[" + typeStr + "]";
}

// ============================================================================
// TEXT PATH
// ============================================================================

TextPath::TextPath(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 17, container)),
      path_(TextPathType::RIGHT) {}

void TextPath::readFromBinary(IBinaryReader& reader) {
    int enumValue = reader.readEnum();
    switch (enumValue) {
        case 0: path_ = TextPathType::RIGHT; break;
        case 1: path_ = TextPathType::LEFT; break;
        case 2: path_ = TextPathType::UP; break;
        case 3: path_ = TextPathType::DOWN; break;
        default: path_ = TextPathType::RIGHT; break;
    }
}

void TextPath::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(static_cast<int>(path_));
}

void TextPath::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("TEXTPATH ");
    switch (path_) {
        case TextPathType::RIGHT: writer.writeString("RIGHT"); break;
        case TextPathType::LEFT: writer.writeString("LEFT"); break;
        case TextPathType::UP: writer.writeString("UP"); break;
        case TextPathType::DOWN: writer.writeString("DOWN"); break;
    }
    writer.writeString(";");
}

std::string TextPath::toString() const {
    std::string pathStr;
    switch (path_) {
        case TextPathType::RIGHT: pathStr = "RIGHT"; break;
        case TextPathType::LEFT: pathStr = "LEFT"; break;
        case TextPathType::UP: pathStr = "UP"; break;
        case TextPathType::DOWN: pathStr = "DOWN"; break;
    }
    return "TextPath[" + pathStr + "]";
}

// ============================================================================
// CHARACTER SET INDEX
// ============================================================================

CharacterSetIndex::CharacterSetIndex(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 19, container)),
      index_(1) {}

void CharacterSetIndex::readFromBinary(IBinaryReader& reader) {
    index_ = reader.readIndex();
    if (container_) {
        container_->setCharacterSetIndex(index_);
    }
}

void CharacterSetIndex::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(index_);
}

void CharacterSetIndex::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("CHARSETINDEX ");
    writer.writeString(std::to_string(index_));
    writer.writeString(";");
}

std::string CharacterSetIndex::toString() const {
    return "CharacterSetIndex[" + std::to_string(index_) + "]";
}

// ============================================================================
// ALTERNATE CHARACTER SET INDEX
// ============================================================================

AlternateCharacterSetIndex::AlternateCharacterSetIndex(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 20, container)),
      index_(1) {}

void AlternateCharacterSetIndex::readFromBinary(IBinaryReader& reader) {
    index_ = reader.readIndex();
    if (container_) {
        container_->setAlternateCharacterSetIndex(index_);
    }
}

void AlternateCharacterSetIndex::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(index_);
}

void AlternateCharacterSetIndex::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("ALTCHARSETINDEX ");
    writer.writeString(std::to_string(index_));
    writer.writeString(";");
}

std::string AlternateCharacterSetIndex::toString() const {
    return "AlternateCharacterSetIndex[" + std::to_string(index_) + "]";
}

// ============================================================================
// FILL REFERENCE POINT
// ============================================================================

FillReferencePoint::FillReferencePoint(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 31, container)),
      point_(0.0, 0.0) {}

void FillReferencePoint::readFromBinary(IBinaryReader& reader) {
    point_ = reader.readPoint();
}

void FillReferencePoint::writeAsBinary(IBinaryWriter& writer) {
    writer.writePoint(point_);
}

void FillReferencePoint::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("FILLREFPT ");
    writer.writeString(std::to_string(point_.x()));
    writer.writeString(" ");
    writer.writeString(std::to_string(point_.y()));
    writer.writeString(";");
}

std::string FillReferencePoint::toString() const {
    return "FillReferencePoint[" + std::to_string(point_.x()) + "," +
           std::to_string(point_.y()) + "]";
}

// ============================================================================
// PATTERN TABLE
// ============================================================================

PatternTable::PatternTable(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 32, container)),
      index_(1), nx_(0), ny_(0), localColorPrecision_(0) {}

void PatternTable::readFromBinary(IBinaryReader& reader) {
    index_ = reader.readIndex();
    nx_ = reader.readInt();
    ny_ = reader.readInt();
    localColorPrecision_ = reader.readInt();

    int nColor = nx_ * ny_;
    colors_.reserve(nColor);
    for (int i = 0; i < nColor; i++) {
        colors_.push_back(reader.readColor(localColorPrecision_));
    }
}

void PatternTable::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(index_);
    writer.writeInt(nx_);
    writer.writeInt(ny_);
    writer.writeInt(localColorPrecision_);
    for (const auto& color : colors_) {
        writer.writeColor(color, localColorPrecision_);
    }
}

void PatternTable::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("PATTABLE ");
    writer.writeString(std::to_string(index_));
    writer.writeString(" ");
    writer.writeString(std::to_string(nx_));
    writer.writeString(" ");
    writer.writeString(std::to_string(ny_));
    writer.writeString(" /* ");
    writer.writeString(std::to_string(colors_.size()));
    writer.writeString(" colors */;");
}

std::string PatternTable::toString() const {
    return "PatternTable[index=" + std::to_string(index_) +
           ", " + std::to_string(nx_) + "x" + std::to_string(ny_) + "]";
}

// ============================================================================
// PATTERN SIZE
// ============================================================================

PatternSize::PatternSize(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 33, container)),  // ISO 8632-3: Element 33
      heightX_(0.0), heightY_(0.0), widthX_(0.0), widthY_(0.0) {}

void PatternSize::readFromBinary(IBinaryReader& reader) {
    heightX_ = reader.readSizeSpecification(container_->interiorStyleSpecificationMode());
    heightY_ = reader.readSizeSpecification(container_->interiorStyleSpecificationMode());
    widthX_ = reader.readSizeSpecification(container_->interiorStyleSpecificationMode());
    widthY_ = reader.readSizeSpecification(container_->interiorStyleSpecificationMode());
}

void PatternSize::writeAsBinary(IBinaryWriter& writer) {
    writer.writeSizeSpecification(heightX_, container_->interiorStyleSpecificationMode());
    writer.writeSizeSpecification(heightY_, container_->interiorStyleSpecificationMode());
    writer.writeSizeSpecification(widthX_, container_->interiorStyleSpecificationMode());
    writer.writeSizeSpecification(widthY_, container_->interiorStyleSpecificationMode());
}

void PatternSize::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("PATSIZE ");
    writer.writeString(std::to_string(heightX_));
    writer.writeString(" ");
    writer.writeString(std::to_string(heightY_));
    writer.writeString(" ");
    writer.writeString(std::to_string(widthX_));
    writer.writeString(" ");
    writer.writeString(std::to_string(widthY_));
    writer.writeString(";");
}

std::string PatternSize::toString() const {
    return "PatternSize[h=(" + std::to_string(heightX_) + "," + std::to_string(heightY_) +
           "), w=(" + std::to_string(widthX_) + "," + std::to_string(widthY_) + ")]";
}

} // namespace opencgm
