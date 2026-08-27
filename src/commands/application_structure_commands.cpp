#include "opencgm/commands/application_structure_commands.h"
#include "opencgm/interfaces.h"
#include "opencgm/cgm_file.h"
#include "opencgm/binary_reader.h"
#include "opencgm/utils/sdr_parser.h"
#include <string>

namespace opencgm {

// ============================================================================
// APPLICATION STRUCTURE ATTRIBUTE
// ============================================================================

ApplicationStructureAttribute::ApplicationStructureAttribute(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::ApplicationStructureDescriptorElements, 1, container)),
      attributeType_(""), data_("") {}

void ApplicationStructureAttribute::readFromBinary(IBinaryReader& reader) {
    // Cast to concrete type to access readFixedString
    auto* concreteReader = dynamic_cast<opencgm::DefaultBinaryReader*>(&reader);
    if (concreteReader) {
        std::string rawType = concreteReader->readRawString();
        if (auto decoded = SDRParser::decodeStructuredText(rawType)) {
            attributeType_ = *decoded;
        } else {
            attributeType_ = concreteReader->decodeText(rawType);
        }
    } else {
        attributeType_ = reader.readString();
    }
    if (concreteReader) {
        data_ = concreteReader->readRawString();
    } else {
        data_ = reader.readString();
    }

    structuredText_.reset();
    isBinary_ = SDRParser::isBinarySDR(data_);
    if (auto decodedData = SDRParser::decodeStructuredText(data_)) {
        structuredText_ = *decodedData;
        // If decode succeeded, treat this as textual even if initial heuristic said binary
        isBinary_ = false;
    }
}

void ApplicationStructureAttribute::writeAsBinary(IBinaryWriter& writer) {
    writer.writeString(attributeType_);
    writer.writeString(data_);
}

void ApplicationStructureAttribute::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString(" APSATTR ");
    writer.writeString(writeString(attributeType_));
    writer.writeString(" ");
    writer.writeString(writeString(data_));
    writer.writeString(";\n");
}

std::string ApplicationStructureAttribute::toString() const {
    return "ApplicationStructureAttribute[type=" + attributeType_ + "]";
}

// ============================================================================
// APPLICATION STRUCTURE DIRECTORY
// ============================================================================

ApplicationStructureDirectory::ApplicationStructureDirectory(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::PictureDescriptorElements, 20, container)),
      typeSelector_(0) {}

void ApplicationStructureDirectory::readFromBinary(IBinaryReader& reader) {
    typeSelector_ = reader.readEnum();

    // Cast to concrete type to access readFixedString
    auto* concreteReader = dynamic_cast<opencgm::DefaultBinaryReader*>(&reader);

    while (reader.hasMoreData()) {
        ApplicationStructureInfo info;
        if (concreteReader) {
            info.identifier = concreteReader->readFixedString();
        } else {
            info.identifier = reader.readString();
        }
        info.location = reader.readInt();
        infos_.push_back(info);
    }
}

void ApplicationStructureDirectory::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(typeSelector_);
    for (const auto& info : infos_) {
        writer.writeString(info.identifier);
        writer.writeInt(info.location);
    }
}

void ApplicationStructureDirectory::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString(" APSDIR ");
    writer.writeString(writeEnum(typeSelector_));

    for (const auto& info : infos_) {
        writer.writeString(" ");
        writer.writeString(writeString(info.identifier));
        writer.writeString(" ");
        writer.writeString(writeInt(info.location));
    }

    writer.writeString(";\n");
}

std::string ApplicationStructureDirectory::toString() const {
    return "ApplicationStructureDirectory[infos=" + std::to_string(infos_.size()) + "]";
}

} // namespace opencgm
