#include "opencgm/commands/external_commands.h"
#include "opencgm/interfaces.h"
#include "opencgm/cgm_file.h"
#include "opencgm/binary_reader.h"

namespace opencgm {

// ============================================================================
// MESSAGE COMMAND
// ============================================================================

MessageCommand::MessageCommand(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::ExternalElements, 1, container)),
      action_(0), message_("") {}

void MessageCommand::readFromBinary(IBinaryReader& reader) {
    action_ = reader.readEnum();
    message_ = reader.readString();
}

void MessageCommand::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(action_);
    writer.writeString(message_);
}

void MessageCommand::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString(" MESSAGE ");
    writer.writeString(writeEnum(action_));
    writer.writeString(", ");
    writer.writeString(writeString(message_));
    writer.writeString(";\n");
}

std::string MessageCommand::toString() const {
    return "MessageCommand[action=" + std::to_string(action_) + ", message=" + message_ + "]";
}

// ============================================================================
// APPLICATION DATA
// ============================================================================

ApplicationData::ApplicationData(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::ExternalElements, 2, container)),
      identifier_(0), data_("") {}

void ApplicationData::readFromBinary(IBinaryReader& reader) {
    identifier_ = reader.readInt();
    if (auto* concreteReader = dynamic_cast<DefaultBinaryReader*>(&reader)) {
        data_ = concreteReader->readRawString();
    } else {
        data_ = reader.readString();
    }
}

void ApplicationData::writeAsBinary(IBinaryWriter& writer) {
    writer.writeInt(identifier_);
    writer.writeString(data_);
}

void ApplicationData::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString(" APPLDATA ");
    writer.writeString(writeInt(identifier_));
    writer.writeString(", ");
    writer.writeString(writeString(data_));
    writer.writeString(";\n");
}

std::string ApplicationData::toString() const {
    return "ApplicationData[id=" + std::to_string(identifier_) + "]";
}

} // namespace opencgm
