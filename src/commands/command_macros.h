#pragma once

/**
 * @file command_macros.h
 * @brief Macros to reduce boilerplate in CGM command implementations
 *
 * These macros eliminate repetitive code for common command patterns while
 * maintaining compatibility with the existing CGM architecture.
 */

/**
 * @brief Define implementation for a command with no parameters (empty command)
 *
 * This macro generates all four required method implementations for commands
 * that have no data to read/write, such as delimiter commands.
 *
 * Usage in .cpp file:
 * @code
 * IMPL_EMPTY_COMMAND(EndMetafile, DelimiterElement, 2, "ENDMF")
 * @endcode
 *
 * Generates:
 * - Constructor with CommandConstructorArguments
 * - readFromBinary() - empty
 * - writeAsBinary() - empty
 * - writeAsClearText() - writes command name
 * - toString() - returns class name
 *
 * @param ClassName The C++ class name (e.g., EndMetafile)
 * @param ClassCodeValue The CGM class code enum value (e.g., DelimiterElement)
 * @param ElementID The element ID within the class (e.g., 2)
 * @param ClearTextName The clear text representation (e.g., "ENDMF")
 */
#define IMPL_EMPTY_COMMAND(ClassName, ClassCodeValue, ElementID, ClearTextName) \
    ClassName::ClassName(CGMFile* container) \
        : Command(CommandConstructorArguments(opencgm::ClassCode::ClassCodeValue, ElementID, container)) {} \
    \
    void ClassName::readFromBinary(IBinaryReader& /* reader */) { \
        /* No parameters */ \
    } \
    \
    void ClassName::writeAsBinary(IBinaryWriter& /* writer */) { \
        /* No parameters */ \
    } \
    \
    void ClassName::writeAsClearText(IClearTextWriter& writer) { \
        writer.writeString(ClearTextName ";"); \
    } \
    \
    std::string ClassName::toString() const { \
        return #ClassName; \
    }

/**
 * @brief Define implementation for a command with a single string parameter
 *
 * This macro generates implementations for commands that store one string,
 * such as BeginMetafile and BeginPicture which store a name.
 *
 * The class must have a member variable: std::string name_;
 * And accessor methods: name() and setName()
 *
 * Usage in .cpp file:
 * @code
 * IMPL_STRING_COMMAND(BeginMetafile, DelimiterElement, 1, "BEGMF", name_)
 * @endcode
 *
 * @param ClassName The C++ class name
 * @param ClassCodeValue The CGM class code enum value
 * @param ElementID The element ID within the class
 * @param ClearTextName The clear text prefix (e.g., "BEGMF")
 * @param MemberVar The member variable name (e.g., name_)
 */
#define IMPL_STRING_COMMAND(ClassName, ClassCodeValue, ElementID, ClearTextPrefix, MemberVar) \
    ClassName::ClassName(CGMFile* container) \
        : Command(CommandConstructorArguments(opencgm::ClassCode::ClassCodeValue, ElementID, container)) {} \
    \
    void ClassName::readFromBinary(IBinaryReader& reader) { \
        MemberVar = reader.readString(); \
    } \
    \
    void ClassName::writeAsBinary(IBinaryWriter& writer) { \
        writer.writeString(MemberVar); \
    } \
    \
    void ClassName::writeAsClearText(IClearTextWriter& writer) { \
        writer.writeString(ClearTextPrefix " "); \
        writer.writeString(writeString(MemberVar)); \
        writer.writeString(";"); \
    } \
    \
    std::string ClassName::toString() const { \
        return #ClassName "[name=\"" + MemberVar + "\"]"; \
    }

/**
 * @brief Define implementation for a command with a single integer parameter
 *
 * This macro generates implementations for commands that store one integer,
 * such as BeginSegment (segmentIdentifier) or BeginProtectionRegion (regionIndex).
 *
 * The class must have an int member variable and accessor methods.
 *
 * Usage in .cpp file:
 * @code
 * IMPL_INT_COMMAND(BeginSegment, DelimiterElement, 6, "BEGSEG", segmentIdentifier_)
 * @endcode
 *
 * @param ClassName The C++ class name
 * @param ClassCodeValue The CGM class code enum value
 * @param ElementID The element ID within the class
 * @param ClearTextName The clear text prefix
 * @param MemberVar The member variable name
 */
#define IMPL_INT_COMMAND(ClassName, ClassCodeValue, ElementID, ClearTextPrefix, MemberVar) \
    ClassName::ClassName(CGMFile* container) \
        : Command(CommandConstructorArguments(opencgm::ClassCode::ClassCodeValue, ElementID, container)) {} \
    \
    void ClassName::readFromBinary(IBinaryReader& reader) { \
        MemberVar = reader.readInt(); \
    } \
    \
    void ClassName::writeAsBinary(IBinaryWriter& writer) { \
        writer.writeInt(MemberVar); \
    } \
    \
    void ClassName::writeAsClearText(IClearTextWriter& writer) { \
        writer.writeString(ClearTextPrefix " "); \
        writer.writeString(std::to_string(MemberVar)); \
        writer.writeString(";"); \
    } \
    \
    std::string ClassName::toString() const { \
        return #ClassName "[" + std::to_string(MemberVar) + "]"; \
    }

/**
 * @brief Example usage and line savings
 *
 * BEFORE (17 lines):
 * ```cpp
 * EndMetafile::EndMetafile(CGMFile* container)
 *     : Command(CommandConstructorArguments(ClassCode::DelimiterElement, 2, container)) {}
 *
 * void EndMetafile::readFromBinary(IBinaryReader& reader) {
 *     // No parameters
 * }
 *
 * void EndMetafile::writeAsBinary(IBinaryWriter& writer) {
 *     // No parameters
 * }
 *
 * void EndMetafile::writeAsClearText(IClearTextWriter& writer) {
 *     writer.writeString("ENDMF;");
 * }
 *
 * std::string EndMetafile::toString() const {
 *     return "EndMetafile";
 * }
 * ```
 *
 * AFTER (1 line):
 * ```cpp
 * IMPL_EMPTY_COMMAND(EndMetafile, DelimiterElement, 2, "ENDMF")
 * ```
 *
 * SAVINGS: 16 lines per empty command
 * With 9 empty commands: 144 lines saved
 */
