#pragma once

#include "../core/command.h"
#include "../import/binary_reader.h"
#include "../export/binary_writer.h"
#include <string>

namespace opencgm {

/**
 * @brief Template for simple index-based attribute commands (LineType, MarkerType, etc.)
 *
 * This template eliminates duplication for commands that simply read/write an index value.
 *
 * @tparam CommandID The CGM command ID
 * @tparam CommandName The human-readable command name
 */
template<int CommandID, const char* CommandName>
class IndexAttributeCommand : public Command {
private:
    int index_ = 0;

public:
    IndexAttributeCommand(CGMFile* container) : Command(container) {}

    void readFromBinary(IBinaryReader& reader) override {
        index_ = reader.readIndex();
    }

    void writeAsBinary(IBinaryWriter& writer) const override {
        writer.writeIndex(index_);
    }

    void writeAsClearText(IClearTextWriter& writer) const override {
        writer.writeString(CommandName);
        writer.writeString(" ");
        writer.writeInt(index_);
        writer.writeString(";\n");
    }

    std::string toString() const override {
        return std::string(CommandName) + "[" + std::to_string(index_) + "]";
    }

    int getIndex() const { return index_; }
    void setIndex(int value) { index_ = value; }
};

/**
 * @brief Template for size specification attribute commands (LineWidth, MarkerSize, etc.)
 *
 * @tparam CommandID The CGM command ID
 * @tparam CommandName The human-readable command name
 * @tparam ModeGetter Pointer to member function that returns the specification mode
 */
template<int CommandID, const char* CommandName>
class SizeAttributeCommand : public Command {
private:
    double value_ = 0.0;

public:
    SizeAttributeCommand(CGMFile* container) : Command(container) {}

    void readFromBinary(IBinaryReader& reader) override {
        // Subclass must implement to get the appropriate mode
        // value_ = reader.readSizeSpecification(container_->getMode());
    }

    void writeAsBinary(IBinaryWriter& writer) const override {
        // Subclass must implement to get the appropriate mode
        // writer.writeSizeSpecification(value_, container_->getMode());
    }

    void writeAsClearText(IClearTextWriter& writer) const override {
        writer.writeString(CommandName);
        writer.writeString(" ");
        writer.writeDouble(value_);
        writer.writeString(";\n");
    }

    std::string toString() const override {
        return std::string(CommandName) + "[" + std::to_string(value_) + "]";
    }

    double getValue() const { return value_; }
    void setValue(double value) { value_ = value; }

protected:
    // Allow subclasses to set value directly
    void readSize(IBinaryReader& reader, SpecificationMode mode) {
        value_ = reader.readSizeSpecification(mode);
    }

    void writeSize(IBinaryWriter& writer, SpecificationMode mode) const {
        writer.writeSizeSpecification(value_, mode);
    }
};

/**
 * @brief Template for color attribute commands (LineColour, MarkerColour, etc.)
 *
 * @tparam CommandID The CGM command ID
 * @tparam CommandName The human-readable command name
 */
template<int CommandID, const char* CommandName>
class ColorAttributeCommand : public Command {
private:
    Color color_;

public:
    ColorAttributeCommand(CGMFile* container) : Command(container) {}

    void readFromBinary(IBinaryReader& reader) override {
        color_ = reader.readColor();
    }

    void writeAsBinary(IBinaryWriter& writer) const override {
        writer.writeColor(color_);
    }

    void writeAsClearText(IClearTextWriter& writer) const override {
        writer.writeString(CommandName);
        writer.writeString(" ");
        writer.writeColor(color_);
        writer.writeString(";\n");
    }

    std::string toString() const override {
        return std::string(CommandName) + "[" + color_.toString() + "]";
    }

    Color getColor() const { return color_; }
    void setColor(const Color& color) { color_ = color; }
};

/**
 * @brief Template for empty parameter commands (EndMetafile, BeginPictureBody, etc.)
 *
 * These commands have no parameters and only need a name.
 *
 * @tparam CommandID The CGM command ID
 * @tparam CommandName The human-readable command name
 */
template<int CommandID, const char* CommandName>
class EmptyCommand : public Command {
public:
    EmptyCommand(CGMFile* container) : Command(container) {}

    void readFromBinary(IBinaryReader& reader) override {
        // No parameters to read
    }

    void writeAsBinary(IBinaryWriter& writer) const override {
        // No parameters to write
    }

    void writeAsClearText(IClearTextWriter& writer) const override {
        writer.writeString(CommandName);
        writer.writeString(";\n");
    }

    std::string toString() const override {
        return CommandName;
    }
};

} // namespace opencgm
