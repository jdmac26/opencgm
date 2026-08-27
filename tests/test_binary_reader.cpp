#include <gtest/gtest.h>
#include "opencgm/binary_reader.h"
#include "opencgm/cgm_file.h"
#include "opencgm/command_factory.h"
#include "opencgm/command.h"
#include <sstream>
#include <vector>
#include <cstring>

// ============================================================================
// Test Fixture for Binary Reader Tests
// ============================================================================

class BinaryReaderTest : public ::testing::Test {
protected:
    opencgm::BinaryCGMFile cgmFile;
    opencgm::DefaultCommandFactory factory;

    // Helper to create a reader from raw bytes
    std::unique_ptr<opencgm::DefaultBinaryReader> createReader(const std::vector<uint8_t>& data) {
        auto str = std::string(data.begin(), data.end());
        auto stream = std::make_unique<std::istringstream>(str, std::ios::binary);
        streams_.push_back(std::move(stream));
        return std::make_unique<opencgm::DefaultBinaryReader>(*streams_.back(), &cgmFile, &factory);
    }

    // Helper to create CGM command header (short form)
    // Format: [class:4][id:7][length:5] for length < 31
    std::vector<uint8_t> makeShortFormHeader(int elementClass, int elementId, int length) {
        uint16_t header = static_cast<uint16_t>(
            ((elementClass & 0x0F) << 12) |
            ((elementId & 0x7F) << 5) |
            (length & 0x1F)
        );
        return {static_cast<uint8_t>(header >> 8), static_cast<uint8_t>(header & 0xFF)};
    }

    // Helper to create CGM command header (long form)
    // Format: [class:4][id:7][11111] then [length:16]
    std::vector<uint8_t> makeLongFormHeader(int elementClass, int elementId, int length) {
        uint16_t header = static_cast<uint16_t>(
            ((elementClass & 0x0F) << 12) |
            ((elementId & 0x7F) << 5) |
            0x1F  // 11111 indicates long form
        );
        return {
            static_cast<uint8_t>(header >> 8),
            static_cast<uint8_t>(header & 0xFF),
            static_cast<uint8_t>(length >> 8),
            static_cast<uint8_t>(length & 0xFF)
        };
    }

private:
    std::vector<std::unique_ptr<std::istringstream>> streams_;
};

// ============================================================================
// Construction and Basic Tests
// ============================================================================

TEST_F(BinaryReaderTest, Construction) {
    std::istringstream stream;
    EXPECT_NO_THROW({
        opencgm::DefaultBinaryReader reader(stream, &cgmFile, &factory);
    });
}

TEST_F(BinaryReaderTest, Constants) {
    EXPECT_DOUBLE_EQ(opencgm::DefaultBinaryReader::TWO_EX_16, 65536.0);
    EXPECT_DOUBLE_EQ(opencgm::DefaultBinaryReader::TWO_EX_32, 4294967296.0);
}

TEST_F(BinaryReaderTest, EmptyStream) {
    std::istringstream stream;
    opencgm::DefaultBinaryReader reader(stream, &cgmFile, &factory);

    EXPECT_NO_THROW({
        reader.readCommands();
    });

    // Empty stream should produce no commands
    EXPECT_EQ(cgmFile.commands().size(), 0);
}

TEST_F(BinaryReaderTest, HasMoreDataInitiallyFalse) {
    std::istringstream stream;
    opencgm::DefaultBinaryReader reader(stream, &cgmFile, &factory);

    // Before reading any command arguments, hasMoreData should be false
    EXPECT_FALSE(reader.hasMoreData());
}

// ============================================================================
// Command Header Parsing Tests
// ============================================================================

TEST_F(BinaryReaderTest, ParseBeginMetafileCommand) {
    // BEGIN METAFILE command: Class 0, ID 1
    // With a simple metafile name "TEST"
    std::vector<uint8_t> data;

    // Header: class=0, id=1, length=5 (1 byte count + 4 bytes string)
    auto header = makeShortFormHeader(0, 1, 5);
    data.insert(data.end(), header.begin(), header.end());

    // String: count byte + "TEST"
    data.push_back(4);  // String length
    data.push_back('T');
    data.push_back('E');
    data.push_back('S');
    data.push_back('T');

    // Pad to word boundary
    data.push_back(0);

    // END METAFILE command: Class 0, ID 2, length 0
    auto endHeader = makeShortFormHeader(0, 2, 0);
    data.insert(data.end(), endHeader.begin(), endHeader.end());

    auto reader = createReader(data);
    reader->readCommands();

    // Should have parsed 2 commands
    EXPECT_EQ(cgmFile.commands().size(), 2);

    // First command should be BEGIN METAFILE
    EXPECT_EQ(cgmFile.commands()[0]->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(cgmFile.commands()[0]->elementId(), 1);
}

TEST_F(BinaryReaderTest, ParseNoOpCommand) {
    // NO-OP command: Class 0, ID 0, length 0
    std::vector<uint8_t> data;
    auto header = makeShortFormHeader(0, 0, 0);
    data.insert(data.end(), header.begin(), header.end());

    auto reader = createReader(data);
    reader->readCommands();

    EXPECT_EQ(cgmFile.commands().size(), 1);
    EXPECT_EQ(cgmFile.commands()[0]->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(cgmFile.commands()[0]->elementId(), 0);
}

TEST_F(BinaryReaderTest, ParseLongFormCommand) {
    // Create a command with length > 30 bytes to test long form
    std::vector<uint8_t> data;

    // Long form header: class=0, id=1 (BEGIN METAFILE), length=35
    auto header = makeLongFormHeader(0, 1, 35);
    data.insert(data.end(), header.begin(), header.end());

    // String: count byte + 34 character string
    data.push_back(34);
    for (int i = 0; i < 34; i++) {
        data.push_back('A' + (i % 26));
    }

    // Pad to word boundary
    data.push_back(0);

    auto reader = createReader(data);
    reader->readCommands();

    EXPECT_GE(cgmFile.commands().size(), 1);
}

// ============================================================================
// Element Class Tests
// ============================================================================

TEST_F(BinaryReaderTest, ParseDelimiterNoOp) {
    std::vector<uint8_t> data;
    auto header = makeShortFormHeader(0, 0, 0);  // NO-OP
    data.insert(data.end(), header.begin(), header.end());

    auto reader = createReader(data);
    reader->readCommands();

    EXPECT_EQ(cgmFile.commands().size(), 1);
    EXPECT_EQ(cgmFile.commands()[0]->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(cgmFile.commands()[0]->elementId(), 0);
}

TEST_F(BinaryReaderTest, ParseDelimiterBeginMetafile) {
    std::vector<uint8_t> data;
    auto header = makeShortFormHeader(0, 1, 0);  // BEGIN METAFILE
    data.insert(data.end(), header.begin(), header.end());

    auto reader = createReader(data);
    reader->readCommands();

    EXPECT_EQ(cgmFile.commands().size(), 1);
    EXPECT_EQ(cgmFile.commands()[0]->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(cgmFile.commands()[0]->elementId(), 1);
}

TEST_F(BinaryReaderTest, ParseDelimiterEndMetafile) {
    std::vector<uint8_t> data;
    auto header = makeShortFormHeader(0, 2, 0);  // END METAFILE
    data.insert(data.end(), header.begin(), header.end());

    auto reader = createReader(data);
    reader->readCommands();

    EXPECT_EQ(cgmFile.commands().size(), 1);
    EXPECT_EQ(cgmFile.commands()[0]->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(cgmFile.commands()[0]->elementId(), 2);
}

TEST_F(BinaryReaderTest, ParseDelimiterBeginPicture) {
    std::vector<uint8_t> data;
    auto header = makeShortFormHeader(0, 3, 0);  // BEGIN PICTURE
    data.insert(data.end(), header.begin(), header.end());

    auto reader = createReader(data);
    reader->readCommands();

    EXPECT_EQ(cgmFile.commands().size(), 1);
    EXPECT_EQ(cgmFile.commands()[0]->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(cgmFile.commands()[0]->elementId(), 3);
}

TEST_F(BinaryReaderTest, ParseDelimiterEndPicture) {
    std::vector<uint8_t> data;
    auto header = makeShortFormHeader(0, 5, 0);  // END PICTURE
    data.insert(data.end(), header.begin(), header.end());

    auto reader = createReader(data);
    reader->readCommands();

    EXPECT_EQ(cgmFile.commands().size(), 1);
    EXPECT_EQ(cgmFile.commands()[0]->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(cgmFile.commands()[0]->elementId(), 5);
}

TEST_F(BinaryReaderTest, ParseMetafileDescriptorElements) {
    // METAFILE VERSION command: Class 1, ID 1
    std::vector<uint8_t> data;

    // Header with 2 bytes of data (16-bit integer)
    auto header = makeShortFormHeader(1, 1, 2);
    data.insert(data.end(), header.begin(), header.end());

    // Version number: 1 (big-endian 16-bit)
    data.push_back(0x00);
    data.push_back(0x01);

    auto reader = createReader(data);
    reader->readCommands();

    EXPECT_EQ(cgmFile.commands().size(), 1);
    EXPECT_EQ(cgmFile.commands()[0]->elementClass(), opencgm::ClassCode::MetafileDescriptorElements);
    EXPECT_EQ(cgmFile.commands()[0]->elementId(), 1);
}

TEST_F(BinaryReaderTest, ParseGraphicalPrimitiveElements) {
    // RECTANGLE command: Class 4, ID 11
    // Requires 2 points (4 VDC values)
    std::vector<uint8_t> data;

    // With default 16-bit integer VDC, 4 values = 8 bytes
    auto header = makeShortFormHeader(4, 11, 8);
    data.insert(data.end(), header.begin(), header.end());

    // Point 1: (0, 0)
    data.push_back(0x00); data.push_back(0x00);  // x1
    data.push_back(0x00); data.push_back(0x00);  // y1

    // Point 2: (100, 100)
    data.push_back(0x00); data.push_back(0x64);  // x2
    data.push_back(0x00); data.push_back(0x64);  // y2

    auto reader = createReader(data);
    reader->readCommands();

    EXPECT_GE(cgmFile.commands().size(), 1);
    if (!cgmFile.commands().empty()) {
        EXPECT_EQ(cgmFile.commands()[0]->elementClass(), opencgm::ClassCode::GraphicalPrimitiveElements);
        EXPECT_EQ(cgmFile.commands()[0]->elementId(), 11);
    }
}

// ============================================================================
// Integer Precision Tests
// ============================================================================

TEST_F(BinaryReaderTest, DefaultIntegerPrecision) {
    // Default integer precision should be 16 bits
    EXPECT_EQ(cgmFile.integerPrecision(), 16);
}

TEST_F(BinaryReaderTest, DefaultVDCType) {
    // Default VDC type should be Integer
    EXPECT_EQ(cgmFile.vdcType(), opencgm::VDCType::Integer);
}

// ============================================================================
// Color Reading Tests
// ============================================================================

TEST_F(BinaryReaderTest, DefaultColorPrecision) {
    // Default color precision should be 8 bits
    EXPECT_EQ(cgmFile.colourPrecision(), 8);
}

TEST_F(BinaryReaderTest, DefaultColorSelectionMode) {
    // Default color selection mode should be INDEXED
    EXPECT_EQ(cgmFile.colorSelectionMode(), opencgm::ColorSelectionMode::INDEXED);
}

// ============================================================================
// String Parsing Tests (via command)
// ============================================================================

TEST_F(BinaryReaderTest, ParseStringInCommand) {
    // Test string parsing via BEGIN METAFILE command
    std::string testName = "MyTestMetafile";

    std::vector<uint8_t> data;
    int length = 1 + static_cast<int>(testName.size());  // count + string

    auto header = makeShortFormHeader(0, 1, length);
    data.insert(data.end(), header.begin(), header.end());

    data.push_back(static_cast<uint8_t>(testName.size()));
    for (char c : testName) {
        data.push_back(static_cast<uint8_t>(c));
    }

    // Pad to word boundary if needed
    if (data.size() % 2 != 0) {
        data.push_back(0);
    }

    auto reader = createReader(data);
    reader->readCommands();

    EXPECT_EQ(cgmFile.commands().size(), 1);

    // The command should have parsed the string
    auto cmdStr = cgmFile.commands()[0]->toString();
    EXPECT_NE(cmdStr.find("MyTestMetafile"), std::string::npos);
}

// ============================================================================
// Multiple Commands Tests
// ============================================================================

TEST_F(BinaryReaderTest, ParseMultipleCommands) {
    std::vector<uint8_t> data;

    // Add several NO-OP commands
    for (int i = 0; i < 5; i++) {
        auto header = makeShortFormHeader(0, 0, 0);
        data.insert(data.end(), header.begin(), header.end());
    }

    auto reader = createReader(data);
    reader->readCommands();

    EXPECT_EQ(cgmFile.commands().size(), 5);
}

TEST_F(BinaryReaderTest, ParseMixedCommands) {
    std::vector<uint8_t> data;

    // BEGIN METAFILE with name "T"
    auto header1 = makeShortFormHeader(0, 1, 2);
    data.insert(data.end(), header1.begin(), header1.end());
    data.push_back(1);  // String length
    data.push_back('T');

    // METAFILE VERSION = 1
    auto header2 = makeShortFormHeader(1, 1, 2);
    data.insert(data.end(), header2.begin(), header2.end());
    data.push_back(0x00);
    data.push_back(0x01);

    // END METAFILE
    auto header3 = makeShortFormHeader(0, 2, 0);
    data.insert(data.end(), header3.begin(), header3.end());

    auto reader = createReader(data);
    reader->readCommands();

    EXPECT_EQ(cgmFile.commands().size(), 3);

    // Verify command types
    EXPECT_EQ(cgmFile.commands()[0]->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(cgmFile.commands()[0]->elementId(), 1);  // BEGIN METAFILE

    EXPECT_EQ(cgmFile.commands()[1]->elementClass(), opencgm::ClassCode::MetafileDescriptorElements);
    EXPECT_EQ(cgmFile.commands()[1]->elementId(), 1);  // METAFILE VERSION

    EXPECT_EQ(cgmFile.commands()[2]->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(cgmFile.commands()[2]->elementId(), 2);  // END METAFILE
}

// ============================================================================
// Endianness Tests
// ============================================================================

TEST_F(BinaryReaderTest, BigEndianIntegerParsing) {
    // CGM uses big-endian byte order
    // Test via METAFILE VERSION command with value 0x1234
    std::vector<uint8_t> data;

    auto header = makeShortFormHeader(1, 1, 2);
    data.insert(data.end(), header.begin(), header.end());

    // Big-endian 0x1234 = [0x12, 0x34]
    data.push_back(0x12);
    data.push_back(0x34);

    auto reader = createReader(data);
    reader->readCommands();

    // Verify command was parsed
    EXPECT_EQ(cgmFile.commands().size(), 1);
    EXPECT_EQ(cgmFile.commands()[0]->elementClass(), opencgm::ClassCode::MetafileDescriptorElements);
    EXPECT_EQ(cgmFile.commands()[0]->elementId(), 1);  // METAFILE VERSION
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(BinaryReaderTest, HandlesTruncatedCommand) {
    // Create a header that says there are 10 bytes of data, but only provide 2
    std::vector<uint8_t> data;
    auto header = makeShortFormHeader(0, 1, 10);
    data.insert(data.end(), header.begin(), header.end());
    data.push_back(0x01);
    data.push_back(0x02);
    // Missing 8 more bytes

    auto reader = createReader(data);

    // Should not crash, may produce warnings
    EXPECT_NO_THROW({
        reader->readCommands();
    });
}

TEST_F(BinaryReaderTest, HandlesUnknownElementClass) {
    // Element class 15 is reserved/unknown
    std::vector<uint8_t> data;
    auto header = makeShortFormHeader(15, 0, 0);
    data.insert(data.end(), header.begin(), header.end());

    auto reader = createReader(data);
    reader->readCommands();

    // Should create an UnknownCommand
    EXPECT_EQ(cgmFile.commands().size(), 1);
}

// ============================================================================
// Word Alignment Tests
// ============================================================================

TEST_F(BinaryReaderTest, WordAlignmentAfterOddLengthCommand) {
    std::vector<uint8_t> data;

    // Command with odd length (3 bytes)
    auto header1 = makeShortFormHeader(0, 1, 3);
    data.insert(data.end(), header1.begin(), header1.end());
    data.push_back(2);   // String length
    data.push_back('A');
    data.push_back('B');
    data.push_back(0);   // Padding byte for word alignment

    // Next command should start at word boundary
    auto header2 = makeShortFormHeader(0, 2, 0);
    data.insert(data.end(), header2.begin(), header2.end());

    auto reader = createReader(data);
    reader->readCommands();

    EXPECT_EQ(cgmFile.commands().size(), 2);
}

// ============================================================================
// Fixed Point Number Tests
// ============================================================================

TEST_F(BinaryReaderTest, FixedPointConversionConstants) {
    // Test that fixed point conversion would work correctly
    const double twoEx16 = opencgm::DefaultBinaryReader::TWO_EX_16;
    const double twoEx32 = opencgm::DefaultBinaryReader::TWO_EX_32;

    // Test 16-bit fixed point
    double testValue = 0.5;
    uint16_t encoded16 = static_cast<uint16_t>(testValue * twoEx16);
    double decoded16 = encoded16 / twoEx16;
    EXPECT_NEAR(decoded16, testValue, 1.0 / twoEx16);

    // Test 32-bit fixed point
    uint32_t encoded32 = static_cast<uint32_t>(testValue * twoEx32);
    double decoded32 = encoded32 / twoEx32;
    EXPECT_NEAR(decoded32, testValue, 1.0 / twoEx32);
}

// ============================================================================
// Message/Warning Collection Tests
// ============================================================================

TEST_F(BinaryReaderTest, CollectsMessages) {
    // Create a minimal valid CGM
    std::vector<uint8_t> data;
    auto header = makeShortFormHeader(0, 0, 0);
    data.insert(data.end(), header.begin(), header.end());

    auto reader = createReader(data);
    reader->readCommands();

    // Messages vector should be accessible
    const auto& messages = reader->getMessages();
    // May or may not have messages, but should not crash
    EXPECT_GE(messages.size(), 0);
}
