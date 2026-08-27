#include "opencgm/commands/attribute_commands.h"
#include "opencgm/interfaces.h"
#include "opencgm/cgm_file.h"
#include <sstream>
#include <string>

namespace opencgm {

// ============================================================================
// LINE CAP
// ============================================================================

LineCap::LineCap(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 37, container)),
      lineIndicator_(LineCapIndicator::UNSPECIFIED),
      dashIndicator_(DashCapIndicator::UNSPECIFIED) {}

void LineCap::readFromBinary(IBinaryReader& reader) {
    int lineIndic = reader.readIndex();
    switch (lineIndic) {
        case 1: lineIndicator_ = LineCapIndicator::UNSPECIFIED; break;
        case 2: lineIndicator_ = LineCapIndicator::BUTT; break;
        case 3: lineIndicator_ = LineCapIndicator::ROUND; break;
        case 4: lineIndicator_ = LineCapIndicator::PROJECTING_SQUARE; break;
        case 5: lineIndicator_ = LineCapIndicator::TRIANGLE; break;
        default:
            reader.unsupported("unsupported line cap indicator " + std::to_string(lineIndic));
            lineIndicator_ = LineCapIndicator::UNSPECIFIED;
            break;
    }

    int dashIndic = reader.readIndex();
    switch (dashIndic) {
        case 1: dashIndicator_ = DashCapIndicator::UNSPECIFIED; break;
        case 2: dashIndicator_ = DashCapIndicator::BUTT; break;
        case 3: dashIndicator_ = DashCapIndicator::MATCH; break;
        default:
            reader.unsupported("unsupported dash cap indicator " + std::to_string(dashIndic));
            dashIndicator_ = DashCapIndicator::UNSPECIFIED;
            break;
    }
}

void LineCap::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(static_cast<int>(lineIndicator_));
    writer.writeIndex(static_cast<int>(dashIndicator_));
}

void LineCap::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("LINECAP ");
    writer.writeString(std::to_string(static_cast<int>(lineIndicator_)));
    writer.writeString(" ");
    writer.writeString(std::to_string(static_cast<int>(dashIndicator_)));
    writer.writeString(";");
}

std::string LineCap::toString() const {
    return "LineCap[line=" + std::to_string(static_cast<int>(lineIndicator_)) +
           ", dash=" + std::to_string(static_cast<int>(dashIndicator_)) + "]";
}

// ============================================================================
// LINE JOIN
// ============================================================================

LineJoin::LineJoin(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 38, container)),
      type_(JoinIndicator::UNSPECIFIED) {}

void LineJoin::readFromBinary(IBinaryReader& reader) {
    int indexValue = reader.readIndex();
    switch (indexValue) {
        case 1: type_ = JoinIndicator::UNSPECIFIED; break;
        case 2: type_ = JoinIndicator::MITER; break;
        case 3: type_ = JoinIndicator::ROUND; break;
        case 4: type_ = JoinIndicator::BEVEL; break;
        default: type_ = JoinIndicator::UNSPECIFIED; break;
    }
}

void LineJoin::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(static_cast<int>(type_));
}

void LineJoin::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("LINEJOIN ");
    writer.writeString(std::to_string(static_cast<int>(type_)));
    writer.writeString(";");
}

std::string LineJoin::toString() const {
    return "LineJoin[" + std::to_string(static_cast<int>(type_)) + "]";
}

// ============================================================================
// LINE TYPE CONTINUATION
// ============================================================================

LineTypeContinuation::LineTypeContinuation(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 39, container)),
      mode_(0) {}

void LineTypeContinuation::readFromBinary(IBinaryReader& reader) {
    mode_ = reader.readIndex();
}

void LineTypeContinuation::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(mode_);
}

void LineTypeContinuation::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("LINETYPECONT ");
    writer.writeString(std::to_string(mode_));
    writer.writeString(";");
}

std::string LineTypeContinuation::toString() const {
    return "LineTypeContinuation[" + std::to_string(mode_) + "]";
}

// ============================================================================
// LINE TYPE INITIAL OFFSET
// ============================================================================

LineTypeInitialOffset::LineTypeInitialOffset(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 40, container)),
      offset_(0.0) {}

void LineTypeInitialOffset::readFromBinary(IBinaryReader& reader) {
    offset_ = reader.readReal();
}

void LineTypeInitialOffset::writeAsBinary(IBinaryWriter& writer) {
    writer.writeReal(offset_);
}

void LineTypeInitialOffset::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("LINETYPEINITOFFSET ");
    writer.writeString(std::to_string(offset_));
    writer.writeString(";");
}

std::string LineTypeInitialOffset::toString() const {
    return "LineTypeInitialOffset[" + std::to_string(offset_) + "]";
}

// ============================================================================
// EDGE CAP
// ============================================================================

EdgeCap::EdgeCap(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 44, container)),
      lineIndicator_(LineCapIndicator::UNSPECIFIED),
      dashIndicator_(DashCapIndicator::UNSPECIFIED) {}

void EdgeCap::readFromBinary(IBinaryReader& reader) {
    int lineIndic = reader.readIndex();
    switch (lineIndic) {
        case 1: lineIndicator_ = LineCapIndicator::UNSPECIFIED; break;
        case 2: lineIndicator_ = LineCapIndicator::BUTT; break;
        case 3: lineIndicator_ = LineCapIndicator::ROUND; break;
        case 4: lineIndicator_ = LineCapIndicator::PROJECTING_SQUARE; break;
        case 5: lineIndicator_ = LineCapIndicator::TRIANGLE; break;
        default:
            reader.unsupported("unsupported edge cap indicator " + std::to_string(lineIndic));
            lineIndicator_ = LineCapIndicator::UNSPECIFIED;
            break;
    }

    int dashIndic = reader.readIndex();
    switch (dashIndic) {
        case 1: dashIndicator_ = DashCapIndicator::UNSPECIFIED; break;
        case 2: dashIndicator_ = DashCapIndicator::BUTT; break;
        case 3: dashIndicator_ = DashCapIndicator::MATCH; break;
        default:
            reader.unsupported("unsupported edge dash cap indicator " + std::to_string(dashIndic));
            dashIndicator_ = DashCapIndicator::UNSPECIFIED;
            break;
    }
}

void EdgeCap::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(static_cast<int>(lineIndicator_));
    writer.writeIndex(static_cast<int>(dashIndicator_));
}

void EdgeCap::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("EDGECAP ");
    writer.writeString(std::to_string(static_cast<int>(lineIndicator_)));
    writer.writeString(" ");
    writer.writeString(std::to_string(static_cast<int>(dashIndicator_)));
    writer.writeString(";");
}

std::string EdgeCap::toString() const {
    return "EdgeCap[line=" + std::to_string(static_cast<int>(lineIndicator_)) +
           ", dash=" + std::to_string(static_cast<int>(dashIndicator_)) + "]";
}

// ============================================================================
// EDGE JOIN
// ============================================================================

EdgeJoin::EdgeJoin(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 45, container)),
      type_(JoinIndicator::UNSPECIFIED) {}

void EdgeJoin::readFromBinary(IBinaryReader& reader) {
    int indexValue = reader.readIndex();
    switch (indexValue) {
        case 1: type_ = JoinIndicator::UNSPECIFIED; break;
        case 2: type_ = JoinIndicator::MITER; break;
        case 3: type_ = JoinIndicator::ROUND; break;
        case 4: type_ = JoinIndicator::BEVEL; break;
        default: type_ = JoinIndicator::UNSPECIFIED; break;
    }
}

void EdgeJoin::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(static_cast<int>(type_));
}

void EdgeJoin::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("EDGEJOIN ");
    writer.writeString(std::to_string(static_cast<int>(type_)));
    writer.writeString(";");
}

std::string EdgeJoin::toString() const {
    return "EdgeJoin[" + std::to_string(static_cast<int>(type_)) + "]";
}

// ============================================================================
// EDGE TYPE CONTINUATION
// ============================================================================

EdgeTypeContinuation::EdgeTypeContinuation(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 46, container)),
      mode_(0) {}

void EdgeTypeContinuation::readFromBinary(IBinaryReader& reader) {
    mode_ = reader.readIndex();
}

void EdgeTypeContinuation::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(mode_);
}

void EdgeTypeContinuation::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("EDGETYPECONT ");
    writer.writeString(std::to_string(mode_));
    writer.writeString(";");
}

std::string EdgeTypeContinuation::toString() const {
    return "EdgeTypeContinuation[" + std::to_string(mode_) + "]";
}

// ============================================================================
// EDGE TYPE INITIAL OFFSET
// ============================================================================

EdgeTypeInitialOffset::EdgeTypeInitialOffset(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::AttributeElements, 47, container)),
      offset_(0.0) {}

void EdgeTypeInitialOffset::readFromBinary(IBinaryReader& reader) {
    offset_ = reader.readReal();
}

void EdgeTypeInitialOffset::writeAsBinary(IBinaryWriter& writer) {
    writer.writeReal(offset_);
}

void EdgeTypeInitialOffset::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("EDGETYPEINITOFFSET ");
    writer.writeString(std::to_string(offset_));
    writer.writeString(";");
}

std::string EdgeTypeInitialOffset::toString() const {
    return "EdgeTypeInitialOffset[" + std::to_string(offset_) + "]";
}

} // namespace opencgm
