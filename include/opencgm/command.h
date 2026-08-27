#ifndef OPENCGM_COMMAND_H
#define OPENCGM_COMMAND_H

#include "opencgm/enums.h"
#include <memory>
#include <string>

namespace opencgm {

// Forward declarations
class IBinaryReader;
class IBinaryWriter;
class IClearTextWriter;
class CGMFile;

/**
 * @brief Constructor arguments for Command objects
 */
struct CommandConstructorArguments {
    ClassCode elementClass;
    int elementId;
    CGMFile* container;

    CommandConstructorArguments(ClassCode ec, int eid, CGMFile* cont)
        : elementClass(ec), elementId(eid), container(cont) {}
};

/**
 * @brief Base class for all CGM commands
 */
class Command {
public:
    virtual ~Command() = default;

    /**
     * @brief Reads the binary data from the reader
     * @param reader Binary reader instance
     */
    virtual void readFromBinary(IBinaryReader& reader) = 0;

    /**
     * @brief Writes/exports the command as clear text mode
     * @param writer The text writer to write the clear text to
     */
    virtual void writeAsClearText(IClearTextWriter& writer) = 0;

    /**
     * @brief Writes/exports the command as binary mode
     * @param writer The writer to write the binary content to
     */
    virtual void writeAsBinary(IBinaryWriter& writer) = 0;

    /**
     * @brief Get the element class
     */
    ClassCode elementClass() const { return elementClass_; }

    /**
     * @brief Get the element ID
     */
    int elementId() const { return elementId_; }

    /**
     * @brief Get a string representation of this command
     */
    virtual std::string toString() const;

    /**
     * @brief Assert a condition and throw if false
     * @param condition The condition to check
     * @param errorMessage Error message if condition is false
     */
    // static void assert(bool condition, const std::string& errorMessage);
    static void verify(bool condition, const std::string& errorMessage);

protected:
    explicit Command(const CommandConstructorArguments& args);

    // Helper methods for writing
    static std::string writeDouble(double value);
    std::string writeReal(double value) const;
    std::string writeVDC(double value) const;
    std::string writePoint(double x, double y) const;
    std::string writeBool(bool value) const;
    std::string writeString(const std::string& value) const;
    std::string writeEnum(int value) const;
    std::string writeInt(int value) const;
    std::string writeIndex(int value) const;

    ClassCode elementClass_;
    int elementId_;
    CGMFile* container_;
};

using CommandPtr = std::unique_ptr<Command>;

} // namespace opencgm

#endif // OPENCGM_COMMAND_H
