#ifndef OPENCGM_APPLICATION_STRUCTURE_COMMANDS_H
#define OPENCGM_APPLICATION_STRUCTURE_COMMANDS_H

#include "../command.h"
#include <optional>
#include <string>
#include <vector>

namespace opencgm {

// Element ID 1: APPLICATION STRUCTURE ATTRIBUTE
class ApplicationStructureAttribute : public Command {
public:
    explicit ApplicationStructureAttribute(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const std::string& attributeType() const { return attributeType_; }
    const std::string& data() const { return data_; }
    const std::optional<std::string>& structuredText() const { return structuredText_; }
    bool isBinaryData() const { return isBinary_; }

private:
    std::string attributeType_;
    std::string data_; // SDR represented as string for now
    std::optional<std::string> structuredText_;
    bool isBinary_ = false;
};

// Element ID 2: APPLICATION STRUCTURE DIRECTORY (in PictureDescriptorElements, ID=20)
class ApplicationStructureDirectory : public Command {
public:
    struct ApplicationStructureInfo {
        std::string identifier;
        int location;
    };

    explicit ApplicationStructureDirectory(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int typeSelector() const { return typeSelector_; } // 0=UI8, 1=UI16, 2=UI32
    const std::vector<ApplicationStructureInfo>& infos() const { return infos_; }

private:
    int typeSelector_;
    std::vector<ApplicationStructureInfo> infos_;
};

} // namespace opencgm

#endif // OPENCGM_APPLICATION_STRUCTURE_COMMANDS_H
