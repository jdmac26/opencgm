#include "opencgm/commands/escape_commands.h"
#include "opencgm/interfaces.h"
#include "opencgm/cgm_file.h"
#include "opencgm/binary_reader.h"

namespace opencgm {

// ============================================================================
// NO-OP
// ============================================================================

NoOp::NoOp(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::DelimiterElement, 0, container)) {}

void NoOp::readFromBinary(IBinaryReader& /* reader */) {
    // No operation - nothing to read
}

void NoOp::writeAsBinary(IBinaryWriter& /* writer */) {
    // No operation - nothing to write
}

void NoOp::writeAsClearText(IClearTextWriter& /* writer */) {
    // No operation - nothing to write
}

std::string NoOp::toString() const {
    return "NoOp";
}

// ============================================================================
// ESCAPE
// ============================================================================

Escape::Escape(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::EscapeElement, 0, container)),
      identifier_(0), dataRecord_("") {}

void Escape::readFromBinary(IBinaryReader& reader) {
    identifier_ = reader.readInt();
    if (auto* concreteReader = dynamic_cast<DefaultBinaryReader*>(&reader)) {
        dataRecord_ = concreteReader->readRawString();
    } else {
        dataRecord_ = reader.readString();
    }
}

void Escape::writeAsBinary(IBinaryWriter& writer) {
    writer.writeInt(identifier_);
    writer.writeString(dataRecord_);
}

void Escape::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString(" ESCAPE ");
    writer.writeString(writeInt(identifier_));
    writer.writeString(" ");
    writer.writeString(writeString(dataRecord_));
    writer.writeString(";\n");
}

std::string Escape::toString() const {
    return "Escape[identifier=" + std::to_string(identifier_) + "]";
}

} // namespace opencgm
