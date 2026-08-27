#include "opencgm/commands/attribute_commands.h"
#include "opencgm/interfaces.h"
#include "opencgm/cgm_file.h"
#include <sstream>
#include <string>

namespace opencgm {

// ============================================================================
// AspectSourceFlags - Element ID 35
// ============================================================================

AspectSourceFlags::AspectSourceFlags(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 35, container)) {}

void AspectSourceFlags::readFromBinary(IBinaryReader& reader) {
    flags_.clear();

    while (reader.hasMoreData()) {
        AspectSourceFlagsInfo info;

        int typeValue = reader.readEnum();
        switch (typeValue) {
            case 0: info.type = AspectSourceFlagType::LINE_TYPE; break;
            case 1: info.type = AspectSourceFlagType::LINE_WIDTH; break;
            case 2: info.type = AspectSourceFlagType::LINE_COLOUR; break;
            case 3: info.type = AspectSourceFlagType::MARKER_TYPE; break;
            case 4: info.type = AspectSourceFlagType::MARKER_SIZE; break;
            case 5: info.type = AspectSourceFlagType::MARKER_COLOUR; break;
            case 6: info.type = AspectSourceFlagType::TEXT_FONT_INDEX; break;
            case 7: info.type = AspectSourceFlagType::TEXT_PRECISION; break;
            case 8: info.type = AspectSourceFlagType::CHARACTER_EXPANSION_FACTOR; break;
            case 9: info.type = AspectSourceFlagType::CHARACTER_SPACING; break;
            case 10: info.type = AspectSourceFlagType::TEXT_COLOUR; break;
            case 11: info.type = AspectSourceFlagType::INTERIOR_STYLE; break;
            case 12: info.type = AspectSourceFlagType::FILL_COLOUR; break;
            case 13: info.type = AspectSourceFlagType::HATCH_INDEX; break;
            case 14: info.type = AspectSourceFlagType::PATTERN_INDEX; break;
            case 15: info.type = AspectSourceFlagType::EDGE_TYPE; break;
            case 16: info.type = AspectSourceFlagType::EDGE_WIDTH; break;
            case 17: info.type = AspectSourceFlagType::EDGE_COLOUR; break;
            default:
                reader.unsupported("unsupported aspect source flag type " + std::to_string(typeValue));
                info.type = AspectSourceFlagType::LINE_TYPE;
                break;
        }

        int valueValue = reader.readEnum();
        info.value = (valueValue == 0) ? AspectSourceFlagValue::INDIVIDUAL : AspectSourceFlagValue::BUNDLED;

        flags_.push_back(info);
    }
}

void AspectSourceFlags::writeAsBinary(IBinaryWriter& writer) {
    for (const auto& flag : flags_) {
        writer.writeEnum(static_cast<int>(flag.type));
        writer.writeEnum(static_cast<int>(flag.value));
    }
}

void AspectSourceFlags::writeAsClearText(IClearTextWriter& writer) {
    std::ostringstream oss;
    oss << " ASF";

    for (const auto& flag : flags_) {
        oss << " " << static_cast<int>(flag.type);
        oss << " " << (flag.value == AspectSourceFlagValue::INDIVIDUAL ? "individual" : "bundled");
    }

    oss << ";";
    writer.writeLine(oss.str());
}

std::string AspectSourceFlags::toString() const {
    std::ostringstream oss;
    oss << "AspectSourceFlags[";
    for (size_t i = 0; i < flags_.size(); i++) {
        if (i > 0) oss << ", ";
        oss << static_cast<int>(flags_[i].type) << "="
            << (flags_[i].value == AspectSourceFlagValue::INDIVIDUAL ? "individual" : "bundled");
    }
    oss << "]";
    return oss.str();
}

// ============================================================================
// PickIdentifier - Element ID 36
// ============================================================================

PickIdentifier::PickIdentifier(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 36, container)),
      identifier_(0) {}

void PickIdentifier::readFromBinary(IBinaryReader& reader) {
    identifier_ = reader.readName();
}

void PickIdentifier::writeAsBinary(IBinaryWriter& writer) {
    writer.writeName(identifier_);
}

void PickIdentifier::writeAsClearText(IClearTextWriter& writer) {
    std::ostringstream oss;
    oss << " PICKID " << identifier_ << ";";
    writer.writeLine(oss.str());
}

std::string PickIdentifier::toString() const {
    return "PickIdentifier[" + std::to_string(identifier_) + "]";
}

// ============================================================================
// TextScoreType - Element ID 41
// ============================================================================

TextScoreType::TextScoreType(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 41, container)) {}

void TextScoreType::readFromBinary(IBinaryReader& reader) {
    scoreTypes_.clear();

    while (reader.hasMoreData()) {
        TSInfo info;

        int typeValue = reader.readEnum();
        switch (typeValue) {
            case 1: info.type = TextScoreTypeValue::NOT_UNDERLINED; break;
            case 2: info.type = TextScoreTypeValue::UNDERLINED; break;
            case 3: info.type = TextScoreTypeValue::NOT_OVERLINED; break;
            case 4: info.type = TextScoreTypeValue::OVERLINED; break;
            case 5: info.type = TextScoreTypeValue::NOT_STRIKETHROUGH; break;
            case 6: info.type = TextScoreTypeValue::STRIKETHROUGH; break;
            default:
                reader.unsupported("unsupported text score type " + std::to_string(typeValue));
                info.type = TextScoreTypeValue::NOT_UNDERLINED;
                break;
        }

        info.indicator = reader.readIndex();
        scoreTypes_.push_back(info);
    }
}

void TextScoreType::writeAsBinary(IBinaryWriter& writer) {
    for (const auto& score : scoreTypes_) {
        writer.writeEnum(static_cast<int>(score.type));
        writer.writeIndex(score.indicator);
    }
}

void TextScoreType::writeAsClearText(IClearTextWriter& writer) {
    std::ostringstream oss;
    oss << " TEXTSCORE";

    for (const auto& score : scoreTypes_) {
        oss << " " << static_cast<int>(score.type);
        oss << " " << score.indicator;
    }

    oss << ";";
    writer.writeLine(oss.str());
}

std::string TextScoreType::toString() const {
    std::ostringstream oss;
    oss << "TextScoreType[";
    for (size_t i = 0; i < scoreTypes_.size(); i++) {
        if (i > 0) oss << ", ";
        oss << static_cast<int>(scoreTypes_[i].type) << ":" << scoreTypes_[i].indicator;
    }
    oss << "]";
    return oss.str();
}

// ============================================================================
// RestrictedTextType - Element ID 42
// ============================================================================

RestrictedTextType::RestrictedTextType(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 42, container)),
      type_(RestrictedTextTypeValue::BASIC) {}

void RestrictedTextType::readFromBinary(IBinaryReader& reader) {
    int typeValue = reader.readIndex();

    switch (typeValue) {
        case 1: type_ = RestrictedTextTypeValue::BASIC; break;
        case 2: type_ = RestrictedTextTypeValue::BOXED_CAP; break;
        case 3: type_ = RestrictedTextTypeValue::BOXED_ALL; break;
        case 4: type_ = RestrictedTextTypeValue::ISOTROPIC_CAP; break;
        case 5: type_ = RestrictedTextTypeValue::ISOTROPIC_ALL; break;
        case 6: type_ = RestrictedTextTypeValue::JUSTIFIED; break;
        default:
            reader.unsupported("unsupported restricted text type " + std::to_string(typeValue));
            type_ = RestrictedTextTypeValue::BASIC;
            break;
    }

    // TODO: Update container state when CGMFile adds restrictedTextType property
    // container_->setRestrictedTextType(static_cast<int>(type_));
}

void RestrictedTextType::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(static_cast<int>(type_));
}

void RestrictedTextType::writeAsClearText(IClearTextWriter& writer) {
    std::ostringstream oss;
    oss << " RESTRTEXT " << static_cast<int>(type_) << ";";
    writer.writeLine(oss.str());
}

std::string RestrictedTextType::toString() const {
    return "RestrictedTextType[" + std::to_string(static_cast<int>(type_)) + "]";
}

// ============================================================================
// InterpolatedInterior - Element ID 43
// ============================================================================

InterpolatedInterior::InterpolatedInterior(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 43, container)),
      style_(0), hatchDirectionX_(0.0), hatchDirectionY_(0.0) {}

void InterpolatedInterior::readFromBinary(IBinaryReader& reader) {
    style_ = reader.readIndex();
    hatchDirectionX_ = reader.readVdc();
    hatchDirectionY_ = reader.readVdc();

    // Read number of reference geometry points
    int nRefGeo = reader.readInt();
    referenceGeometry_.clear();
    referenceGeometry_.reserve(nRefGeo);
    for (int i = 0; i < nRefGeo; i++) {
        referenceGeometry_.push_back(reader.readPoint());
    }

    // Read stage designators
    stageDesignators_.clear();
    int nStage = reader.readInt();
    stageDesignators_.reserve(nStage);
    for (int i = 0; i < nStage; i++) {
        stageDesignators_.push_back(reader.readInt());
    }

    // Read colors
    colors_.clear();
    while (reader.hasMoreData()) {
        colors_.push_back(reader.readColor());
    }
}

void InterpolatedInterior::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(style_);
    writer.writeVdc(hatchDirectionX_);
    writer.writeVdc(hatchDirectionY_);

    writer.writeInt(static_cast<int>(referenceGeometry_.size()));
    for (const auto& point : referenceGeometry_) {
        writer.writePoint(point);
    }

    writer.writeInt(static_cast<int>(stageDesignators_.size()));
    for (int designator : stageDesignators_) {
        writer.writeInt(designator);
    }

    for (const auto& color : colors_) {
        writer.writeColor(color);
    }
}

void InterpolatedInterior::writeAsClearText(IClearTextWriter& writer) {
    std::ostringstream oss;
    oss << " INTERPINTERIOR " << style_;
    oss << " (" << hatchDirectionX_ << "," << hatchDirectionY_ << ")";

    oss << " " << referenceGeometry_.size();
    for (const auto& point : referenceGeometry_) {
        oss << " (" << point.x() << "," << point.y() << ")";
    }

    oss << " " << stageDesignators_.size();
    for (int designator : stageDesignators_) {
        oss << " " << designator;
    }

    for (const auto& color : colors_) {
        if (color.isIndexed()) {
            oss << " " << color.colorIndex();
        } else {
            const Color& c = color.color();
            oss << " (" << static_cast<int>(c.r) << ","
                << static_cast<int>(c.g) << "," << static_cast<int>(c.b) << ")";
        }
    }

    oss << ";";
    writer.writeLine(oss.str());
}

std::string InterpolatedInterior::toString() const {
    std::ostringstream oss;
    oss << "InterpolatedInterior[style=" << style_
        << ", hatchDir=(" << hatchDirectionX_ << "," << hatchDirectionY_ << ")"
        << ", refGeo=" << referenceGeometry_.size()
        << ", stages=" << stageDesignators_.size()
        << ", colors=" << colors_.size() << "]";
    return oss.str();
}

} // namespace opencgm
