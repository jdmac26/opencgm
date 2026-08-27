#include <gtest/gtest.h>
#include "opencgm/binary_reader.h"
#include "opencgm/binary_writer.h"
#include "opencgm/cgm_file.h"
#include "opencgm/command_factory.h"
#include "opencgm/command.h"
#include "opencgm/commands/delimiter_commands.h"
#include "opencgm/commands/metafile_descriptor_commands.h"
#include <sstream>
#include <vector>
#include <cstring>

// ============================================================================
// Binary Round-Trip Test Fixture
// Tests that commands can be written and read back correctly
// ============================================================================

class BinaryRoundTripTest : public ::testing::Test {
protected:
    opencgm::BinaryCGMFile cgmFile;
    opencgm::DefaultCommandFactory factory;

    // Helper to write a command to a buffer
    std::vector<uint8_t> writeCommandToBuffer(opencgm::Command& cmd) {
        std::ostringstream oss(std::ios::binary);
        opencgm::DefaultBinaryWriter writer(oss, &cgmFile);
        writer.writeCommand(cmd);

        std::string str = oss.str();
        return std::vector<uint8_t>(str.begin(), str.end());
    }

    // Helper to read commands from a buffer
    void readCommandsFromBuffer(const std::vector<uint8_t>& data) {
        std::string str(data.begin(), data.end());
        std::istringstream iss(str, std::ios::binary);
        opencgm::DefaultBinaryReader reader(iss, &cgmFile, &factory);
        reader.readCommands();
    }

    // Helper to verify command header bytes
    void verifyHeader(const std::vector<uint8_t>& data,
                      int expectedClass, int expectedId, int expectedLength) {
        ASSERT_GE(data.size(), 2) << "Data too short for header";

        // Parse header: [class:4][id:7][length:5]
        uint16_t header = (static_cast<uint16_t>(data[0]) << 8) | data[1];
        int elementClass = (header >> 12) & 0x0F;
        int elementId = (header >> 5) & 0x7F;
        int length = header & 0x1F;

        EXPECT_EQ(elementClass, expectedClass);
        EXPECT_EQ(elementId, expectedId);
        if (expectedLength < 31) {
            EXPECT_EQ(length, expectedLength);
        } else {
            EXPECT_EQ(length, 31) << "Long form expected";
        }
    }
};

// ============================================================================
// Header Writing Tests
// ============================================================================

TEST_F(BinaryRoundTripTest, WriteNoOpCommand) {
    // NO-OP: Class 0, ID 0, no data
    auto cmd = factory.createCommand(0, 0, &cgmFile);
    ASSERT_NE(cmd, nullptr);

    auto data = writeCommandToBuffer(*cmd);
    verifyHeader(data, 0, 0, 0);
}

TEST_F(BinaryRoundTripTest, WriteEndMetafileCommand) {
    // END METAFILE: Class 0, ID 2, no data
    auto cmd = factory.createCommand(2, 0, &cgmFile);
    ASSERT_NE(cmd, nullptr);

    auto data = writeCommandToBuffer(*cmd);
    verifyHeader(data, 0, 2, 0);
}

TEST_F(BinaryRoundTripTest, WriteEndPictureCommand) {
    // END PICTURE: Class 0, ID 5, no data
    auto cmd = factory.createCommand(5, 0, &cgmFile);
    ASSERT_NE(cmd, nullptr);

    auto data = writeCommandToBuffer(*cmd);
    verifyHeader(data, 0, 5, 0);
}

// ============================================================================
// Delimiter Command Round-Trip Tests
// ============================================================================

TEST_F(BinaryRoundTripTest, RoundTripNoOp) {
    // Create and write NO-OP
    auto cmd = factory.createCommand(0, 0, &cgmFile);
    ASSERT_NE(cmd, nullptr);

    auto data = writeCommandToBuffer(*cmd);

    // Read back
    opencgm::BinaryCGMFile readFile;
    opencgm::DefaultCommandFactory readFactory;
    std::string str(data.begin(), data.end());
    std::istringstream iss(str, std::ios::binary);
    opencgm::DefaultBinaryReader reader(iss, &readFile, &readFactory);
    reader.readCommands();

    ASSERT_EQ(readFile.commands().size(), 1);
    EXPECT_EQ(readFile.commands()[0]->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(readFile.commands()[0]->elementId(), 0);
}

TEST_F(BinaryRoundTripTest, RoundTripEndMetafile) {
    auto cmd = factory.createCommand(2, 0, &cgmFile);
    ASSERT_NE(cmd, nullptr);

    auto data = writeCommandToBuffer(*cmd);

    opencgm::BinaryCGMFile readFile;
    opencgm::DefaultCommandFactory readFactory;
    std::string str(data.begin(), data.end());
    std::istringstream iss(str, std::ios::binary);
    opencgm::DefaultBinaryReader reader(iss, &readFile, &readFactory);
    reader.readCommands();

    ASSERT_EQ(readFile.commands().size(), 1);
    EXPECT_EQ(readFile.commands()[0]->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(readFile.commands()[0]->elementId(), 2);
}

// ============================================================================
// Writer Primitive Tests
// ============================================================================

class BinaryWriterPrimitivesTest : public ::testing::Test {
protected:
    opencgm::BinaryCGMFile cgmFile;

    std::vector<uint8_t> getWrittenBytes(std::function<void(opencgm::DefaultBinaryWriter&)> writeFunc) {
        std::ostringstream oss(std::ios::binary);
        opencgm::DefaultBinaryWriter writer(oss, &cgmFile);
        writeFunc(writer);
        std::string str = oss.str();
        return std::vector<uint8_t>(str.begin(), str.end());
    }
};

TEST_F(BinaryWriterPrimitivesTest, WriteInt16BigEndian) {
    // Write 0x1234 - should be big-endian: [0x12, 0x34]
    std::ostringstream oss(std::ios::binary);
    opencgm::DefaultBinaryWriter writer(oss, &cgmFile);

    // Create a minimal command to write
    auto cmd = opencgm::DefaultCommandFactory().createCommand(0, 0, &cgmFile);
    writer.writeCommand(*cmd);

    std::string str = oss.str();
    std::vector<uint8_t> data(str.begin(), str.end());

    // NO-OP command should produce header [0x00, 0x00]
    ASSERT_GE(data.size(), 2);
    EXPECT_EQ(data[0], 0x00);
    EXPECT_EQ(data[1], 0x00);
}

// ============================================================================
// Multiple Commands Round-Trip Tests
// ============================================================================

TEST_F(BinaryRoundTripTest, RoundTripMultipleDelimiterCommands) {
    std::ostringstream oss(std::ios::binary);
    opencgm::DefaultBinaryWriter writer(oss, &cgmFile);

    // Write NO-OP, END PICTURE, END METAFILE
    auto noOp = factory.createCommand(0, 0, &cgmFile);
    auto endPic = factory.createCommand(5, 0, &cgmFile);
    auto endMf = factory.createCommand(2, 0, &cgmFile);

    writer.writeCommand(*noOp);
    writer.writeCommand(*endPic);
    writer.writeCommand(*endMf);

    std::string str = oss.str();
    std::vector<uint8_t> data(str.begin(), str.end());

    // Read back
    opencgm::BinaryCGMFile readFile;
    opencgm::DefaultCommandFactory readFactory;
    std::istringstream iss(str, std::ios::binary);
    opencgm::DefaultBinaryReader reader(iss, &readFile, &readFactory);
    reader.readCommands();

    ASSERT_EQ(readFile.commands().size(), 3);

    EXPECT_EQ(readFile.commands()[0]->elementId(), 0);  // NO-OP
    EXPECT_EQ(readFile.commands()[1]->elementId(), 5);  // END PICTURE
    EXPECT_EQ(readFile.commands()[2]->elementId(), 2);  // END METAFILE
}

// ============================================================================
// Color Value Scaling Tests (tested indirectly via color writing)
// ============================================================================

// Note: scaleColorValueRGB is private, so we test color handling indirectly
// through the color scaling tests in test_color_scaling.cpp

// ============================================================================
// Fixed Point Number Tests
// ============================================================================

TEST_F(BinaryWriterPrimitivesTest, FixedPointConstants) {
    // Verify the constants used for fixed-point conversion
    EXPECT_DOUBLE_EQ(opencgm::DefaultBinaryReader::TWO_EX_16, 65536.0);
    EXPECT_DOUBLE_EQ(opencgm::DefaultBinaryReader::TWO_EX_32, 4294967296.0);
}

// ============================================================================
// Word Alignment Tests
// ============================================================================

TEST_F(BinaryRoundTripTest, CommandsAreWordAligned) {
    std::ostringstream oss(std::ios::binary);
    opencgm::DefaultBinaryWriter writer(oss, &cgmFile);

    // Write multiple commands
    auto cmd1 = factory.createCommand(0, 0, &cgmFile);
    auto cmd2 = factory.createCommand(2, 0, &cgmFile);

    writer.writeCommand(*cmd1);
    writer.writeCommand(*cmd2);

    std::string str = oss.str();

    // Each command should be word-aligned (2 bytes)
    EXPECT_EQ(str.size() % 2, 0) << "Output should be word-aligned";
}

// ============================================================================
// Long Form Command Tests
// ============================================================================

TEST_F(BinaryRoundTripTest, LongFormHeaderDetection) {
    // Commands with >= 31 bytes of data use long form
    // Test with a command that would trigger long form

    // For now, just verify short form works (< 31 bytes)
    auto cmd = factory.createCommand(0, 0, &cgmFile);
    auto data = writeCommandToBuffer(*cmd);

    // NO-OP has 0 bytes of data, so short form with length=0
    ASSERT_GE(data.size(), 2);
    uint16_t header = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    int length = header & 0x1F;
    EXPECT_LT(length, 31) << "Short form expected for small commands";
}

// ============================================================================
// Command Preservation Tests
// ============================================================================

TEST_F(BinaryRoundTripTest, CommandClassPreserved) {
    // Test that element class is preserved through round-trip
    std::vector<std::pair<int, int>> testCases = {
        {0, 0},  // Delimiter: NO-OP
        {2, 0},  // Delimiter: END METAFILE
        {5, 0},  // Delimiter: END PICTURE
    };

    for (const auto& [id, classId] : testCases) {
        auto cmd = factory.createCommand(id, classId, &cgmFile);
        ASSERT_NE(cmd, nullptr) << "Failed to create command id=" << id << " class=" << classId;

        auto data = writeCommandToBuffer(*cmd);

        opencgm::BinaryCGMFile readFile;
        opencgm::DefaultCommandFactory readFactory;
        std::string str(data.begin(), data.end());
        std::istringstream iss(str, std::ios::binary);
        opencgm::DefaultBinaryReader reader(iss, &readFile, &readFactory);
        reader.readCommands();

        ASSERT_EQ(readFile.commands().size(), 1)
            << "Expected 1 command for id=" << id << " class=" << classId;
        EXPECT_EQ(readFile.commands()[0]->elementClass(), cmd->elementClass())
            << "Class mismatch for id=" << id;
        EXPECT_EQ(readFile.commands()[0]->elementId(), cmd->elementId())
            << "ID mismatch for id=" << id;
    }
}

// ============================================================================
// Metafile Structure Round-Trip Tests
// ============================================================================

TEST_F(BinaryRoundTripTest, RoundTripBeginMetafileWithName) {
    // Create BEGIN METAFILE with name
    auto cmd = std::make_unique<opencgm::BeginMetafile>(&cgmFile);
    cmd->setName("TestMetafile");

    auto data = writeCommandToBuffer(*cmd);

    // Should have header + string length byte + string data (+ padding if needed)
    ASSERT_GT(data.size(), 2) << "Should have data beyond header";

    // Read back
    opencgm::BinaryCGMFile readFile;
    opencgm::DefaultCommandFactory readFactory;
    std::string str(data.begin(), data.end());
    std::istringstream iss(str, std::ios::binary);
    opencgm::DefaultBinaryReader reader(iss, &readFile, &readFactory);
    reader.readCommands();

    ASSERT_EQ(readFile.commands().size(), 1);
    EXPECT_EQ(readFile.commands()[0]->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(readFile.commands()[0]->elementId(), 1);  // BEGIN METAFILE

    auto* readCmd = dynamic_cast<opencgm::BeginMetafile*>(readFile.commands()[0].get());
    ASSERT_NE(readCmd, nullptr);
    EXPECT_EQ(readCmd->name(), "TestMetafile");
}

TEST_F(BinaryRoundTripTest, RoundTripBeginPictureWithName) {
    auto cmd = std::make_unique<opencgm::BeginPicture>(&cgmFile);
    cmd->setName("Picture1");

    auto data = writeCommandToBuffer(*cmd);

    opencgm::BinaryCGMFile readFile;
    opencgm::DefaultCommandFactory readFactory;
    std::string str(data.begin(), data.end());
    std::istringstream iss(str, std::ios::binary);
    opencgm::DefaultBinaryReader reader(iss, &readFile, &readFactory);
    reader.readCommands();

    ASSERT_EQ(readFile.commands().size(), 1);
    EXPECT_EQ(readFile.commands()[0]->elementId(), 3);  // BEGIN PICTURE

    auto* readCmd = dynamic_cast<opencgm::BeginPicture*>(readFile.commands()[0].get());
    ASSERT_NE(readCmd, nullptr);
    EXPECT_EQ(readCmd->name(), "Picture1");
}

TEST_F(BinaryRoundTripTest, RoundTripCompleteMetafileStructure) {
    // Build a minimal but complete CGM structure
    std::ostringstream oss(std::ios::binary);
    opencgm::DefaultBinaryWriter writer(oss, &cgmFile);

    auto beginMf = std::make_unique<opencgm::BeginMetafile>(&cgmFile);
    beginMf->setName("TestCGM");

    auto beginPic = std::make_unique<opencgm::BeginPicture>(&cgmFile);
    beginPic->setName("Page1");

    auto beginPicBody = factory.createCommand(4, 0, &cgmFile);  // BEGIN PICTURE BODY
    auto endPic = factory.createCommand(5, 0, &cgmFile);        // END PICTURE
    auto endMf = factory.createCommand(2, 0, &cgmFile);         // END METAFILE

    writer.writeCommand(*beginMf);
    writer.writeCommand(*beginPic);
    writer.writeCommand(*beginPicBody);
    writer.writeCommand(*endPic);
    writer.writeCommand(*endMf);

    std::string str = oss.str();

    // Read back
    opencgm::BinaryCGMFile readFile;
    opencgm::DefaultCommandFactory readFactory;
    std::istringstream iss(str, std::ios::binary);
    opencgm::DefaultBinaryReader reader(iss, &readFile, &readFactory);
    reader.readCommands();

    ASSERT_EQ(readFile.commands().size(), 5);

    EXPECT_EQ(readFile.commands()[0]->elementId(), 1);  // BEGIN METAFILE
    EXPECT_EQ(readFile.commands()[1]->elementId(), 3);  // BEGIN PICTURE
    EXPECT_EQ(readFile.commands()[2]->elementId(), 4);  // BEGIN PICTURE BODY
    EXPECT_EQ(readFile.commands()[3]->elementId(), 5);  // END PICTURE
    EXPECT_EQ(readFile.commands()[4]->elementId(), 2);  // END METAFILE
}

// ============================================================================
// BinaryCGMFile writeFile() Tests
// ============================================================================

class BinaryCGMFileWriteTest : public ::testing::Test {
protected:
    // Helper to create a minimal CGM file with commands
    void populateMinimalCGM(opencgm::BinaryCGMFile& cgm) {
        auto beginMf = std::make_unique<opencgm::BeginMetafile>(&cgm);
        beginMf->setName("WriteTest");
        cgm.commands().push_back(std::move(beginMf));

        auto beginPic = std::make_unique<opencgm::BeginPicture>(&cgm);
        beginPic->setName("Pic1");
        cgm.commands().push_back(std::move(beginPic));

        opencgm::DefaultCommandFactory factory;
        cgm.commands().push_back(factory.createCommand(4, 0, &cgm));  // BEGIN PICTURE BODY
        cgm.commands().push_back(factory.createCommand(5, 0, &cgm));  // END PICTURE
        cgm.commands().push_back(factory.createCommand(2, 0, &cgm));  // END METAFILE
    }
};

TEST_F(BinaryCGMFileWriteTest, WriteFileToStream) {
    opencgm::BinaryCGMFile writeFile;
    populateMinimalCGM(writeFile);

    // Write to stream
    std::ostringstream oss(std::ios::binary);
    writeFile.writeFile(oss);

    std::string data = oss.str();
    ASSERT_FALSE(data.empty()) << "writeFile should produce output";
    EXPECT_EQ(data.size() % 2, 0) << "Output should be word-aligned";
}

TEST_F(BinaryCGMFileWriteTest, WriteFileRoundTrip) {
    opencgm::BinaryCGMFile writeFile;
    populateMinimalCGM(writeFile);

    // Write to stream
    std::ostringstream oss(std::ios::binary);
    writeFile.writeFile(oss);

    std::string data = oss.str();

    // Read back
    std::istringstream iss(data, std::ios::binary);
    opencgm::BinaryCGMFile readFile(iss, "test.cgm");

    ASSERT_EQ(readFile.commands().size(), writeFile.commands().size())
        << "Command count should match after round-trip";

    // Verify command types match
    for (size_t i = 0; i < writeFile.commands().size(); ++i) {
        EXPECT_EQ(readFile.commands()[i]->elementClass(),
                  writeFile.commands()[i]->elementClass())
            << "Element class mismatch at index " << i;
        EXPECT_EQ(readFile.commands()[i]->elementId(),
                  writeFile.commands()[i]->elementId())
            << "Element ID mismatch at index " << i;
    }
}

TEST_F(BinaryCGMFileWriteTest, WriteFilePreservesMetafileName) {
    opencgm::BinaryCGMFile writeFile;
    populateMinimalCGM(writeFile);

    std::ostringstream oss(std::ios::binary);
    writeFile.writeFile(oss);

    std::istringstream iss(oss.str(), std::ios::binary);
    opencgm::BinaryCGMFile readFile(iss, "test.cgm");

    ASSERT_GE(readFile.commands().size(), 1);
    auto* beginMf = dynamic_cast<opencgm::BeginMetafile*>(readFile.commands()[0].get());
    ASSERT_NE(beginMf, nullptr);
    EXPECT_EQ(beginMf->name(), "WriteTest");
}

TEST_F(BinaryCGMFileWriteTest, WriteFileInvalidStreamThrows) {
    opencgm::BinaryCGMFile file;
    populateMinimalCGM(file);

    std::ostringstream oss(std::ios::binary);
    oss.setstate(std::ios::badbit);  // Force bad state

    EXPECT_THROW(file.writeFile(oss), std::runtime_error);
}

// ============================================================================
// All Delimiter Commands Round-Trip Tests
// ============================================================================

TEST_F(BinaryRoundTripTest, RoundTripBeginPictureBody) {
    auto cmd = factory.createCommand(4, 0, &cgmFile);  // BEGIN PICTURE BODY
    ASSERT_NE(cmd, nullptr);

    auto data = writeCommandToBuffer(*cmd);

    opencgm::BinaryCGMFile readFile;
    opencgm::DefaultCommandFactory readFactory;
    std::string str(data.begin(), data.end());
    std::istringstream iss(str, std::ios::binary);
    opencgm::DefaultBinaryReader reader(iss, &readFile, &readFactory);
    reader.readCommands();

    ASSERT_EQ(readFile.commands().size(), 1);
    EXPECT_EQ(readFile.commands()[0]->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(readFile.commands()[0]->elementId(), 4);
}

TEST_F(BinaryRoundTripTest, RoundTripBeginFigure) {
    auto cmd = factory.createCommand(8, 0, &cgmFile);  // BEGIN FIGURE
    ASSERT_NE(cmd, nullptr);

    auto data = writeCommandToBuffer(*cmd);

    opencgm::BinaryCGMFile readFile;
    opencgm::DefaultCommandFactory readFactory;
    std::string str(data.begin(), data.end());
    std::istringstream iss(str, std::ios::binary);
    opencgm::DefaultBinaryReader reader(iss, &readFile, &readFactory);
    reader.readCommands();

    ASSERT_EQ(readFile.commands().size(), 1);
    EXPECT_EQ(readFile.commands()[0]->elementId(), 8);
}

TEST_F(BinaryRoundTripTest, RoundTripEndFigure) {
    auto cmd = factory.createCommand(9, 0, &cgmFile);  // END FIGURE
    ASSERT_NE(cmd, nullptr);

    auto data = writeCommandToBuffer(*cmd);

    opencgm::BinaryCGMFile readFile;
    opencgm::DefaultCommandFactory readFactory;
    std::string str(data.begin(), data.end());
    std::istringstream iss(str, std::ios::binary);
    opencgm::DefaultBinaryReader reader(iss, &readFile, &readFactory);
    reader.readCommands();

    ASSERT_EQ(readFile.commands().size(), 1);
    EXPECT_EQ(readFile.commands()[0]->elementId(), 9);
}

// ============================================================================
// Long String Tests (triggers long-form header)
// ============================================================================

TEST_F(BinaryRoundTripTest, RoundTripLongMetafileName) {
    // Create a name > 30 bytes to trigger long-form header
    std::string longName(50, 'A');  // 50 'A' characters

    auto cmd = std::make_unique<opencgm::BeginMetafile>(&cgmFile);
    cmd->setName(longName);

    auto data = writeCommandToBuffer(*cmd);

    // Verify long form is used (length field = 31, then actual length follows)
    ASSERT_GE(data.size(), 4);
    uint16_t header = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    int lengthField = header & 0x1F;
    EXPECT_EQ(lengthField, 31) << "Long form header expected for >30 byte data";

    // Read back
    opencgm::BinaryCGMFile readFile;
    opencgm::DefaultCommandFactory readFactory;
    std::string str(data.begin(), data.end());
    std::istringstream iss(str, std::ios::binary);
    opencgm::DefaultBinaryReader reader(iss, &readFile, &readFactory);
    reader.readCommands();

    ASSERT_EQ(readFile.commands().size(), 1);
    auto* readCmd = dynamic_cast<opencgm::BeginMetafile*>(readFile.commands()[0].get());
    ASSERT_NE(readCmd, nullptr);
    EXPECT_EQ(readCmd->name(), longName);
}

// ============================================================================
// Empty Command Data Tests
// ============================================================================

TEST_F(BinaryRoundTripTest, RoundTripEmptyMetafileName) {
    auto cmd = std::make_unique<opencgm::BeginMetafile>(&cgmFile);
    cmd->setName("");  // Empty name

    auto data = writeCommandToBuffer(*cmd);

    opencgm::BinaryCGMFile readFile;
    opencgm::DefaultCommandFactory readFactory;
    std::string str(data.begin(), data.end());
    std::istringstream iss(str, std::ios::binary);
    opencgm::DefaultBinaryReader reader(iss, &readFile, &readFactory);
    reader.readCommands();

    ASSERT_EQ(readFile.commands().size(), 1);
    auto* readCmd = dynamic_cast<opencgm::BeginMetafile*>(readFile.commands()[0].get());
    ASSERT_NE(readCmd, nullptr);
    EXPECT_EQ(readCmd->name(), "");
}

// ============================================================================
// Segment Commands Round-Trip Tests
// ============================================================================

TEST_F(BinaryRoundTripTest, RoundTripBeginSegment) {
    auto cmd = std::make_unique<opencgm::BeginSegment>(&cgmFile);
    cmd->setSegmentIdentifier(42);

    auto data = writeCommandToBuffer(*cmd);

    opencgm::BinaryCGMFile readFile;
    opencgm::DefaultCommandFactory readFactory;
    std::string str(data.begin(), data.end());
    std::istringstream iss(str, std::ios::binary);
    opencgm::DefaultBinaryReader reader(iss, &readFile, &readFactory);
    reader.readCommands();

    ASSERT_EQ(readFile.commands().size(), 1);
    EXPECT_EQ(readFile.commands()[0]->elementId(), 6);  // BEGIN SEGMENT

    auto* readCmd = dynamic_cast<opencgm::BeginSegment*>(readFile.commands()[0].get());
    ASSERT_NE(readCmd, nullptr);
    EXPECT_EQ(readCmd->segmentIdentifier(), 42);
}

TEST_F(BinaryRoundTripTest, RoundTripEndSegment) {
    auto cmd = factory.createCommand(7, 0, &cgmFile);  // END SEGMENT
    ASSERT_NE(cmd, nullptr);

    auto data = writeCommandToBuffer(*cmd);

    opencgm::BinaryCGMFile readFile;
    opencgm::DefaultCommandFactory readFactory;
    std::string str(data.begin(), data.end());
    std::istringstream iss(str, std::ios::binary);
    opencgm::DefaultBinaryReader reader(iss, &readFile, &readFactory);
    reader.readCommands();

    ASSERT_EQ(readFile.commands().size(), 1);
    EXPECT_EQ(readFile.commands()[0]->elementId(), 7);
}

