#ifndef OPENCGM_INTERFACES_H
#define OPENCGM_INTERFACES_H

#include "opencgm/cgm_point.h"
#include "opencgm/cgm_color.h"
#include "opencgm/enums.h"
#include <string>
#include <cstdint>
#include <memory>
#include <vector>

namespace opencgm {

// Forward declarations
class Command;
class CGMFile;

/**
 * @brief Interface for reading binary CGM data
 */
class IBinaryReader {
public:
    virtual ~IBinaryReader() = default;

    virtual int readEnum() = 0;
    virtual std::string readString() = 0;
    virtual int readIndex() = 0;
    virtual int readName() = 0;
    virtual int readInt() = 0;
    virtual uint8_t readByte() = 0;
    virtual char readChar() = 0;
    virtual bool readBool() = 0;
    virtual double readVdc() = 0;
    virtual double readReal() = 0;
    virtual CGMPoint readPoint() = 0;
    virtual int readColorIndex() = 0;
    virtual int readColorIndex(int localColorPrecision) = 0;
    virtual CGMColor readColor() = 0;
    virtual CGMColor readColor(int localColorPrecision) = 0;
    virtual Color readDirectColor() = 0;
    virtual double readSizeSpecification(SpecificationMode mode) = 0;
    virtual void alignOnWord() = 0;
    virtual int sizeOfInt() = 0;
    virtual int sizeOfPoint() = 0;
    virtual bool hasMoreData() const = 0;
    virtual bool getFinalFlag() = 0;
    virtual void unsupported(const std::string& message) = 0;

    // Methods for bit-packed cell array RLE decoding
    virtual size_t getRemainingByteCount() const = 0;
    virtual std::vector<uint8_t> getRemainingBytes() = 0;
    virtual void skipBytes(size_t count) = 0;
};

/**
 * @brief Interface for writing binary CGM data
 */
class IBinaryWriter {
public:
    virtual ~IBinaryWriter() = default;

    virtual void writeString(const std::string& data) = 0;
    virtual void writeInt(int data) = 0;
    virtual void writeEnum(int data) = 0;
    virtual void writeBool(bool data) = 0;
    virtual void writeIndex(int data) = 0;
    virtual void writeName(int data) = 0;
    virtual void writeByte(uint8_t data) = 0;
    virtual void writeChar(char data) = 0;
    virtual void writeColor(const CGMColor& color, int localColorPrecision = -1) = 0;
    virtual void writePoint(const CGMPoint& point) = 0;
    virtual void writeReal(double data) = 0;
    virtual void writeVdc(double data) = 0;
    virtual void writeSizeSpecification(double data, SpecificationMode mode) = 0;
    virtual void fillToWord() = 0;
    virtual void writeCommand(Command& command) = 0;
    virtual void unsupported(const std::string& message) = 0;
};

/**
 * @brief Interface for writing clear text CGM data
 */
class IClearTextWriter {
public:
    virtual ~IClearTextWriter() = default;

    virtual void write(const std::string& text) = 0;
    virtual void writeLine(const std::string& text) = 0;
    virtual void writeString(const std::string& text) = 0;
    virtual void indent() = 0;
    virtual void unindent() = 0;
};

/**
 * @brief Interface for creating command instances
 */
class ICommandFactory {
public:
    virtual ~ICommandFactory() = default;

    /**
     * @brief Create a command instance
     * @param elementId Command element ID
     * @param elementClass Command class code
     * @param container Parent CGM file container
     * @return Unique pointer to the created command
     */
    virtual std::unique_ptr<Command> createCommand(
        int elementId,
        int elementClass,
        CGMFile* container) = 0;
};

} // namespace opencgm

#endif // OPENCGM_INTERFACES_H