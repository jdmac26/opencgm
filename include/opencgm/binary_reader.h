#ifndef OPENCGM_BINARY_READER_H
#define OPENCGM_BINARY_READER_H

#include "opencgm/interfaces.h"
#include "opencgm/cgm_file.h"
#include <istream>
#include <vector>
#include <memory>
#include <array>

namespace opencgm {

/**
 * @brief Default implementation of binary CGM reader
 *
 * This class reads binary CGM data from a stream and parses commands
 * according to ISO/IEC 8632-3:1999 specification.
 */
class DefaultBinaryReader : public IBinaryReader {
public:
    DefaultBinaryReader(std::istream& stream, CGMFile* cgm, ICommandFactory* factory);
    ~DefaultBinaryReader() override = default;

    /**
     * @brief Read all commands from the stream
     */
    void readCommands();

    /**
     * @brief Read an embedded command (used within other commands)
     */
    std::unique_ptr<Command> readEmbeddedCommand();

    // IBinaryReader interface implementation
    int readEnum() override;
    std::string readString() override;
    std::string readRawString();
    int readIndex() override;
    int readName() override;
    int readInt() override;
    uint8_t readByte() override;
    char readChar() override;
    bool readBool() override;
    double readVdc() override;
    double readReal() override;
    CGMPoint readPoint() override;
    int readColorIndex() override;
    int readColorIndex(int localColorPrecision) override;
    CGMColor readColor() override;
    CGMColor readColor(int localColorPrecision) override;
    Color readDirectColor() override;
    Color readDirectColor(int localPrecision);
    double readSizeSpecification(SpecificationMode mode) override;
    int sizeOfInt() override;
    int sizeOfPoint() override;
    bool hasMoreData() const override;
    bool getFinalFlag() override;
    void alignOnWord() override;
    void unsupported(const std::string& message) override;

    // Methods for bit-packed cell array RLE decoding
    size_t getRemainingByteCount() const override;
    std::vector<uint8_t> getRemainingBytes() override;
    void skipBytes(size_t count) override;

    // Additional helper methods
    std::string readFixedString();
    std::string readRawString(int length);
    std::string decodeText(const std::string& raw) const;
    int readArgumentEnd();
    int getCurrentArg() const { return currentArg_; }
    int getArgumentsCount() const { return static_cast<int>(arguments_.size()); }
    const std::vector<Message>& getMessages() const { return messages_; }

    // Constants (public for use by writer)
    static constexpr double TWO_EX_16 = 65536.0;      // 2^16
    static constexpr double TWO_EX_32 = 4294967296.0; // 2^32

private:
    // Core reading methods
    std::unique_ptr<Command> readCommand();
    void readArguments(int argumentsCount);
    void readShortFormCommandArguments(int argumentsCount);
    void readLongFormCommandArguments();

    // Integer reading
    int readSignedInt8();
    int readSignedInt16();
    int readSignedInt24();
    int readSignedInt32();
    int readUInt(int precision);
    int readUInt8();
    int readUInt16();
    int readUInt24();
    int readUInt32();
    int readUInt1();
    int readUInt2();
    int readUInt4();
    int readUIntBit(int numBits);

    // Real number reading
    double readFixedPoint();
    double readFixedPoint32();
    double readFixedPoint64();
    double readFloatingPoint();
    double readFloatingPoint32();
    double readFloatingPoint64();

    // Size calculation
    int sizeOfVdc();
    int sizeOfFixedPoint32() { return 4; }
    int sizeOfFixedPoint64() { return 8; }
    int sizeOfFloatingPoint32() { return 4; }
    int sizeOfFloatingPoint64() { return 8; }
    int sizeOfEnum() { return 2; }
    int sizeOfDirectColor();

    // Helper methods
    int readInt(int precision);
    std::string readString(int length);
    int getStringCount();
    void skipBits();
    int readInt16Direct();
    int readLocalColorPrecision();
    std::array<int, 3> scaleColorValueRGB(int r, int g, int b, int precisionBits);
    std::string getErrorMessage(const std::string& callingMethod = "");
    void ensureAllArgumentsWereRead();

    // Member variables
    std::istream& reader_;
    CGMFile* cgm_;
    ICommandFactory* commandFactory_;
    std::vector<uint8_t> arguments_;
    int currentArg_;
    int positionInCurrentArgument_;
    Command* currentCommand_;
    bool finalFlag_;
    std::vector<Message> messages_;
};

} // namespace opencgm

#endif // OPENCGM_BINARY_READER_H
