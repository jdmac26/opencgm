#ifndef OPENCGM_COMMAND_FACTORY_H
#define OPENCGM_COMMAND_FACTORY_H

#include "opencgm/interfaces.h"
#include "opencgm/command.h"
#include "opencgm/enums.h"

namespace opencgm {

/**
 * @brief Unknown/unimplemented command placeholder
 */
class UnknownCommand : public Command {
public:
    UnknownCommand(int elementId, int elementClass, CGMFile* container)
        : Command(CommandConstructorArguments(
            static_cast<ClassCode>(elementClass), elementId, container)) {}

    void readFromBinary(IBinaryReader& /* reader */) override {
        // Just consume all arguments
        // reader.readArgumentEnd();
    }

    void writeAsClearText(IClearTextWriter& writer) override {
        writer.writeLine("UNKNOWN_COMMAND");
    }

    void writeAsBinary(IBinaryWriter& /* writer */) override {
        // Do nothing
    }

    std::string toString() const override {
        return "UnknownCommand[class=" + std::to_string(static_cast<int>(elementClass_)) +
               ", id=" + std::to_string(elementId_) + "]";
    }
};

/**
 * @brief Default command factory implementation
 */
class DefaultCommandFactory : public ICommandFactory {
public:
    DefaultCommandFactory() = default;
    ~DefaultCommandFactory() override = default;

    std::unique_ptr<Command> createCommand(
        int elementId,
        int elementClass,
        CGMFile* container) override;

private:
    // Factory methods for each element class
    std::unique_ptr<Command> createDelimiterElement(int elementId, CGMFile* container);
    std::unique_ptr<Command> createMetafileDescriptorElement(int elementId, CGMFile* container);
    std::unique_ptr<Command> createPictureDescriptorElement(int elementId, CGMFile* container);
    std::unique_ptr<Command> createControlElement(int elementId, CGMFile* container);
    std::unique_ptr<Command> createGraphicalPrimitiveElement(int elementId, CGMFile* container);
    std::unique_ptr<Command> createAttributeElement(int elementId, CGMFile* container);
    std::unique_ptr<Command> createEscapeElement(int elementId, CGMFile* container);
    std::unique_ptr<Command> createExternalElement(int elementId, CGMFile* container);
    std::unique_ptr<Command> createSegmentControlElement(int elementId, CGMFile* container);
    std::unique_ptr<Command> createApplicationStructureElement(int elementId, CGMFile* container);
};

} // namespace opencgm

#endif // OPENCGM_COMMAND_FACTORY_H