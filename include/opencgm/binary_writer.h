#ifndef OPENCGM_BINARY_WRITER_H
#define OPENCGM_BINARY_WRITER_H

#include "opencgm/interfaces.h"
#include "opencgm/cgm_file.h"
#include <ostream>
#include <vector>
#include <memory>
#include <array>

namespace opencgm {

/**
 * @brief Helper class for buffering binary data before writing
 */
class WriterBucket {
public:
    WriterBucket() = default;

    void add(uint8_t byte) { data_.push_back(byte); }
    void addRange(const std::vector<uint8_t>& bytes) {
        data_.insert(data_.end(), bytes.begin(), bytes.end());
    }
    void addRange(const WriterBucket& other) {
        data_.insert(data_.end(), other.data_.begin(), other.data_.end());
    }
    void writeString(const std::string& str) {
        for (char c : str) {
            data_.push_back(static_cast<uint8_t>(c));
        }
    }
    void insert(size_t index, uint8_t byte) {
        if (index <= data_.size()) {
            data_.insert(data_.begin() + index, byte);
        }
    }

    size_t count() const { return data_.size(); }
    uint8_t& operator[](size_t index) { return data_[index]; }
    const uint8_t& operator[](size_t index) const { return data_[index]; }

    void clear() { data_.clear(); }
    void saveToStream(std::ostream& stream) {
        stream.write(reinterpret_cast<const char*>(data_.data()), data_.size());
    }

    const std::vector<uint8_t>& data() const { return data_; }

private:
    std::vector<uint8_t> data_;
};

/**
 * @brief Default implementation of binary CGM writer
 */
class DefaultBinaryWriter : public IBinaryWriter {
public:
    DefaultBinaryWriter(std::ostream& stream, CGMFile* cgm);
    ~DefaultBinaryWriter() override = default;

    // IBinaryReader interface implementation
    void writeString(const std::string& data) override;
    void writeInt(int data) override;
    void writeEnum(int data) override;
    void writeBool(bool data) override;
    void writeIndex(int data) override;
    void writeName(int data) override;
    void writeByte(uint8_t data) override;
    void writeChar(char data) override;
    void writeColor(const CGMColor& color, int localColorPrecision = -1) override;
    void writePoint(const CGMPoint& point) override;
    void writeReal(double data) override;
    void writeVdc(double data) override;
    void writeSizeSpecification(double data, SpecificationMode mode) override;
    void fillToWord() override;
    void writeCommand(Command& command) override;
    void unsupported(const std::string& message) override;

    // Additional methods
    void writeFixedString(const std::string& data);
    void writeFloatingPoint(double data);
    void writeEmbeddedCommand(Command& command);
    void writeColorIndex(int index);
    void writeColorIndex(int index, int localColorPrecision);
    void writeDirectColor(const Color& color, int localColorPrecision = -1);
    void writeUInt(int data, int precision);

    const std::vector<Message>& getMessages() const { return messages_; }

private:
    // Integer writing
    void writeInt(int data, int precision);
    void writeSignedInt8(int data);
    void writeSignedInt16(int data);
    void writeSignedInt16(int data, size_t index);
    void writeSignedInt24(int data);
    void writeSignedInt32(int data);
    void writeUInt8(int data);
    void writeUInt16(int data);
    void writeUInt24(int data);
    void writeUInt32(int data);
    void writeUInt1(int data);
    void writeUInt2(int data);
    void writeUInt4(int data);
    void writeUIntBit(int data, int numBits);

    // Real number writing
    void writeFixedPoint32(double value);
    void writeFixedPoint64(double value);
    void writeFloatingPoint32(double data);
    void writeFloatingPoint64(double value);

    // Helper methods
    void writeDataLength(int length);
    void writeHeader(Command& command, uint32_t argumentField);
    void writeInt16Direct(int value);
    std::array<int, 3> scaleColorValueRGB(int r, int g, int b, int precisionBits);

    // Member variables
    std::ostream& stream_;
    CGMFile* cgm_;
    WriterBucket bucket_;
    Command* currentCommand_;
    int positionInCurrentArgument_;
    std::vector<Message> messages_;
};

} // namespace opencgm

#endif // OPENCGM_BINARY_WRITER_H
