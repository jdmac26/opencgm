#include "opencgm/commands/segment_control_commands.h"
#include "opencgm/interfaces.h"
#include "opencgm/cgm_file.h"

namespace opencgm {

// ============================================================================
// COPY SEGMENT
// ============================================================================

CopySegment::CopySegment(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::SegmentControlandSegmentAttributeElements, 1, container)),
      id_(0), xScale_(1.0), xRotation_(0.0), yRotation_(0.0), yScale_(1.0),
      xTranslation_(0.0), yTranslation_(0.0), flag_(false) {}

void CopySegment::readFromBinary(IBinaryReader& reader) {
    id_ = reader.readName();
    xScale_ = reader.readReal();
    xRotation_ = reader.readReal();
    yRotation_ = reader.readReal();
    yScale_ = reader.readReal();
    xTranslation_ = reader.readVdc();
    yTranslation_ = reader.readVdc();
    flag_ = reader.readBool();
}

void CopySegment::writeAsBinary(IBinaryWriter& writer) {
    writer.writeName(id_);
    writer.writeReal(xScale_);
    writer.writeReal(xRotation_);
    writer.writeReal(yRotation_);
    writer.writeReal(yScale_);
    writer.writeVdc(xTranslation_);
    writer.writeVdc(yTranslation_);
    writer.writeBool(flag_);
}

void CopySegment::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString(" COPYSEG ");
    writer.writeString(writeInt(id_));
    writer.writeString(" ");
    writer.writeString(writeReal(xScale_));
    writer.writeString(" ");
    writer.writeString(writeReal(xRotation_));
    writer.writeString(" ");
    writer.writeString(writeReal(yRotation_));
    writer.writeString(" ");
    writer.writeString(writeReal(yScale_));
    writer.writeString(" ");
    writer.writeString(writeVDC(xTranslation_));
    writer.writeString(" ");
    writer.writeString(writeVDC(yTranslation_));
    writer.writeString(" ");
    writer.writeString(flag_ ? "yes" : "no");
    writer.writeString(";\n");
}

std::string CopySegment::toString() const {
    return "CopySegment[id=" + std::to_string(id_) + "]";
}

// ============================================================================
// INHERITANCE FILTER
// ============================================================================

InheritanceFilter::InheritanceFilter(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::SegmentControlandSegmentAttributeElements, 2, container)),
      setting_(0) {}

void InheritanceFilter::readFromBinary(IBinaryReader& reader) {
    values_.clear();

    // Read all enums, saving the last one as the setting
    std::vector<int> allValues;
    while (reader.hasMoreData()) {
        allValues.push_back(reader.readEnum());
    }

    // The last value is the setting, the rest are filter values
    if (!allValues.empty()) {
        setting_ = allValues.back();
        values_.assign(allValues.begin(), allValues.end() - 1);
    }
}

void InheritanceFilter::writeAsBinary(IBinaryWriter& writer) {
    for (int val : values_) {
        writer.writeEnum(val);
    }
    writer.writeEnum(setting_);
}

void InheritanceFilter::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("  INHFILTER");

    for (int val : values_) {
        writer.writeString(" ");
        writer.writeString(writeEnum(val));
    }

    if (setting_ == 0) {
        writer.writeString(" stlist");
    } else {
        writer.writeString(" seg");
    }

    writer.writeString(";\n");
}

std::string InheritanceFilter::toString() const {
    return "InheritanceFilter[values=" + std::to_string(values_.size()) + ", setting=" + std::to_string(setting_) + "]";
}

// ============================================================================
// CLIP INHERITANCE
// ============================================================================

ClipInheritanceCommand::ClipInheritanceCommand(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::SegmentControlandSegmentAttributeElements, 3, container)),
      value_(ClipInheritance::STATE_LIST) {}

void ClipInheritanceCommand::readFromBinary(IBinaryReader& reader) {
    value_ = static_cast<ClipInheritance>(reader.readEnum());
}

void ClipInheritanceCommand::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(static_cast<int>(value_));
}

void ClipInheritanceCommand::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString(" CLIPINH ");
    writer.writeString(writeEnum(static_cast<int>(value_)));
    writer.writeString(";\n");
}

std::string ClipInheritanceCommand::toString() const {
    return "ClipInheritance[" + std::to_string(static_cast<int>(value_)) + "]";
}

// ============================================================================
// SEGMENT TRANSFORMATION
// ============================================================================

SegmentTransformation::SegmentTransformation(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::SegmentControlandSegmentAttributeElements, 4, container)),
      identifier_(0), scaleX_(1.0), rotationX_(0.0), rotationY_(0.0),
      scaleY_(1.0), translationX_(0.0), translationY_(0.0) {}

void SegmentTransformation::readFromBinary(IBinaryReader& reader) {
    identifier_ = reader.readName();
    scaleX_ = reader.readReal();
    rotationX_ = reader.readReal();
    rotationY_ = reader.readReal();
    scaleY_ = reader.readReal();
    translationX_ = reader.readVdc();
    translationY_ = reader.readVdc();
}

void SegmentTransformation::writeAsBinary(IBinaryWriter& writer) {
    writer.writeName(identifier_);
    writer.writeReal(scaleX_);
    writer.writeReal(rotationX_);
    writer.writeReal(rotationY_);
    writer.writeReal(scaleY_);
    writer.writeVdc(translationX_);
    writer.writeVdc(translationY_);
}

void SegmentTransformation::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString(" SEGTRAN ");
    writer.writeString(writeInt(identifier_));
    writer.writeString(" ");
    writer.writeString(writeReal(scaleX_));
    writer.writeString(" ");
    writer.writeString(writeReal(rotationX_));
    writer.writeString(" ");
    writer.writeString(writeReal(rotationY_));
    writer.writeString(" ");
    writer.writeString(writeReal(scaleY_));
    writer.writeString(" ");
    writer.writeString(writeVDC(translationX_));
    writer.writeString(" ");
    writer.writeString(writeVDC(translationY_));
    writer.writeString(";\n");
}

std::string SegmentTransformation::toString() const {
    return "SegmentTransformation[id=" + std::to_string(identifier_) + "]";
}

// ============================================================================
// SEGMENT HIGHLIGHTING
// ============================================================================

SegmentHighlighting::SegmentHighlighting(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::SegmentControlandSegmentAttributeElements, 5, container)),
      identifier_(0), value_(0) {}

void SegmentHighlighting::readFromBinary(IBinaryReader& reader) {
    identifier_ = reader.readName();
    value_ = reader.readEnum();
}

void SegmentHighlighting::writeAsBinary(IBinaryWriter& writer) {
    writer.writeName(identifier_);
    writer.writeEnum(value_);
}

void SegmentHighlighting::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString(" SEGHIGHL ");
    writer.writeString(writeInt(identifier_));
    writer.writeString(" ");
    writer.writeString(writeEnum(value_));
    writer.writeString(";\n");
}

std::string SegmentHighlighting::toString() const {
    return "SegmentHighlighting[id=" + std::to_string(identifier_) + ", value=" + std::to_string(value_) + "]";
}

// ============================================================================
// SEGMENT DISPLAY PRIORITY
// ============================================================================

SegmentDisplayPriority::SegmentDisplayPriority(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::SegmentControlandSegmentAttributeElements, 6, container)),
      name_(0), priority_(0) {}

void SegmentDisplayPriority::readFromBinary(IBinaryReader& reader) {
    name_ = reader.readName();
    priority_ = reader.readInt();
}

void SegmentDisplayPriority::writeAsBinary(IBinaryWriter& writer) {
    writer.writeName(name_);
    writer.writeInt(priority_);
}

void SegmentDisplayPriority::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString(" SEGDISPPRI ");
    writer.writeString(writeInt(name_));
    writer.writeString(" ");
    writer.writeString(writeInt(priority_));
    writer.writeString(";\n");
}

std::string SegmentDisplayPriority::toString() const {
    return "SegmentDisplayPriority[name=" + std::to_string(name_) + ", priority=" + std::to_string(priority_) + "]";
}

// ============================================================================
// SEGMENT PICK PRIORITY
// ============================================================================

SegmentPickPriority::SegmentPickPriority(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::SegmentControlandSegmentAttributeElements, 7, container)),
      identifier_(0), priority_(0) {}

void SegmentPickPriority::readFromBinary(IBinaryReader& reader) {
    identifier_ = reader.readName();
    priority_ = reader.readInt();
}

void SegmentPickPriority::writeAsBinary(IBinaryWriter& writer) {
    writer.writeName(identifier_);
    writer.writeInt(priority_);
}

void SegmentPickPriority::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString(" SEGPICKPRI ");
    writer.writeString(writeInt(identifier_));
    writer.writeString(" ");
    writer.writeString(writeInt(priority_));
    writer.writeString(";\n");
}

std::string SegmentPickPriority::toString() const {
    return "SegmentPickPriority[id=" + std::to_string(identifier_) + ", priority=" + std::to_string(priority_) + "]";
}

} // namespace opencgm
