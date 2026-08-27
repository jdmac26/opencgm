#include <gtest/gtest.h>
#include "opencgm/command_factory.h"
#include "opencgm/cgm_file.h"
#include "opencgm/command.h"
#include "opencgm/enums.h"

// ============================================================================
// Command Factory Tests
// ============================================================================

class CommandFactoryTest : public ::testing::Test {
protected:
    opencgm::DefaultCommandFactory factory;
    opencgm::BinaryCGMFile cgmFile;
};

// ============================================================================
// Delimiter Element Creation Tests (Class 0)
// ============================================================================

TEST_F(CommandFactoryTest, CreateNoOp) {
    auto cmd = factory.createCommand(0, 0, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(cmd->elementId(), 0);
}

TEST_F(CommandFactoryTest, CreateBeginMetafile) {
    auto cmd = factory.createCommand(1, 0, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(cmd->elementId(), 1);
}

TEST_F(CommandFactoryTest, CreateEndMetafile) {
    auto cmd = factory.createCommand(2, 0, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(cmd->elementId(), 2);
}

TEST_F(CommandFactoryTest, CreateBeginPicture) {
    auto cmd = factory.createCommand(3, 0, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(cmd->elementId(), 3);
}

TEST_F(CommandFactoryTest, CreateBeginPictureBody) {
    auto cmd = factory.createCommand(4, 0, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(cmd->elementId(), 4);
}

TEST_F(CommandFactoryTest, CreateEndPicture) {
    auto cmd = factory.createCommand(5, 0, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(cmd->elementId(), 5);
}

TEST_F(CommandFactoryTest, CreateBeginSegment) {
    auto cmd = factory.createCommand(6, 0, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(cmd->elementId(), 6);
}

TEST_F(CommandFactoryTest, CreateEndSegment) {
    auto cmd = factory.createCommand(7, 0, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(cmd->elementId(), 7);
}

TEST_F(CommandFactoryTest, CreateBeginFigure) {
    auto cmd = factory.createCommand(8, 0, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(cmd->elementId(), 8);
}

TEST_F(CommandFactoryTest, CreateEndFigure) {
    auto cmd = factory.createCommand(9, 0, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::DelimiterElement);
    EXPECT_EQ(cmd->elementId(), 9);
}

// ============================================================================
// Metafile Descriptor Element Creation Tests (Class 1)
// ============================================================================

TEST_F(CommandFactoryTest, CreateMetafileVersion) {
    auto cmd = factory.createCommand(1, 1, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::MetafileDescriptorElements);
    EXPECT_EQ(cmd->elementId(), 1);
}

TEST_F(CommandFactoryTest, CreateMetafileDescription) {
    auto cmd = factory.createCommand(2, 1, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::MetafileDescriptorElements);
    EXPECT_EQ(cmd->elementId(), 2);
}

TEST_F(CommandFactoryTest, CreateVDCType) {
    auto cmd = factory.createCommand(3, 1, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::MetafileDescriptorElements);
    EXPECT_EQ(cmd->elementId(), 3);
}

TEST_F(CommandFactoryTest, CreateIntegerPrecision) {
    auto cmd = factory.createCommand(4, 1, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::MetafileDescriptorElements);
    EXPECT_EQ(cmd->elementId(), 4);
}

TEST_F(CommandFactoryTest, CreateRealPrecision) {
    auto cmd = factory.createCommand(5, 1, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::MetafileDescriptorElements);
    EXPECT_EQ(cmd->elementId(), 5);
}

TEST_F(CommandFactoryTest, CreateColourPrecision) {
    auto cmd = factory.createCommand(7, 1, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::MetafileDescriptorElements);
    EXPECT_EQ(cmd->elementId(), 7);
}

TEST_F(CommandFactoryTest, CreateFontList) {
    auto cmd = factory.createCommand(13, 1, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::MetafileDescriptorElements);
    EXPECT_EQ(cmd->elementId(), 13);
}

// ============================================================================
// Picture Descriptor Element Creation Tests (Class 2)
// ============================================================================

TEST_F(CommandFactoryTest, CreateScalingMode) {
    auto cmd = factory.createCommand(1, 2, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::PictureDescriptorElements);
    EXPECT_EQ(cmd->elementId(), 1);
}

TEST_F(CommandFactoryTest, CreateColourSelectionMode) {
    auto cmd = factory.createCommand(2, 2, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::PictureDescriptorElements);
    EXPECT_EQ(cmd->elementId(), 2);
}

TEST_F(CommandFactoryTest, CreateVDCExtent) {
    auto cmd = factory.createCommand(6, 2, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::PictureDescriptorElements);
    EXPECT_EQ(cmd->elementId(), 6);
}

TEST_F(CommandFactoryTest, CreateBackgroundColour) {
    auto cmd = factory.createCommand(7, 2, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::PictureDescriptorElements);
    EXPECT_EQ(cmd->elementId(), 7);
}

// ============================================================================
// Control Element Creation Tests (Class 3)
// ============================================================================

TEST_F(CommandFactoryTest, CreateClipRectangle) {
    auto cmd = factory.createCommand(5, 3, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::ControlElements);
    EXPECT_EQ(cmd->elementId(), 5);
}

TEST_F(CommandFactoryTest, CreateClipIndicator) {
    auto cmd = factory.createCommand(6, 3, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::ControlElements);
    EXPECT_EQ(cmd->elementId(), 6);
}

// ============================================================================
// Graphical Primitive Element Creation Tests (Class 4)
// ============================================================================

TEST_F(CommandFactoryTest, CreatePolyline) {
    auto cmd = factory.createCommand(1, 4, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::GraphicalPrimitiveElements);
    EXPECT_EQ(cmd->elementId(), 1);
}

TEST_F(CommandFactoryTest, CreatePolymarker) {
    auto cmd = factory.createCommand(3, 4, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::GraphicalPrimitiveElements);
    EXPECT_EQ(cmd->elementId(), 3);
}

TEST_F(CommandFactoryTest, CreateText) {
    auto cmd = factory.createCommand(4, 4, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::GraphicalPrimitiveElements);
    EXPECT_EQ(cmd->elementId(), 4);
}

TEST_F(CommandFactoryTest, CreatePolygon) {
    auto cmd = factory.createCommand(7, 4, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::GraphicalPrimitiveElements);
    EXPECT_EQ(cmd->elementId(), 7);
}

TEST_F(CommandFactoryTest, CreateRectangle) {
    auto cmd = factory.createCommand(11, 4, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::GraphicalPrimitiveElements);
    EXPECT_EQ(cmd->elementId(), 11);
}

TEST_F(CommandFactoryTest, CreateCircle) {
    auto cmd = factory.createCommand(12, 4, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::GraphicalPrimitiveElements);
    EXPECT_EQ(cmd->elementId(), 12);
}

TEST_F(CommandFactoryTest, CreateEllipse) {
    auto cmd = factory.createCommand(17, 4, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::GraphicalPrimitiveElements);
    EXPECT_EQ(cmd->elementId(), 17);
}

TEST_F(CommandFactoryTest, CreatePolyBezier) {
    auto cmd = factory.createCommand(26, 4, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::GraphicalPrimitiveElements);
    EXPECT_EQ(cmd->elementId(), 26);
}

// ============================================================================
// Attribute Element Creation Tests (Class 5)
// ============================================================================

TEST_F(CommandFactoryTest, CreateLineType) {
    auto cmd = factory.createCommand(2, 5, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::AttributeElements);
    EXPECT_EQ(cmd->elementId(), 2);
}

TEST_F(CommandFactoryTest, CreateLineWidth) {
    auto cmd = factory.createCommand(3, 5, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::AttributeElements);
    EXPECT_EQ(cmd->elementId(), 3);
}

TEST_F(CommandFactoryTest, CreateLineColour) {
    auto cmd = factory.createCommand(4, 5, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::AttributeElements);
    EXPECT_EQ(cmd->elementId(), 4);
}

TEST_F(CommandFactoryTest, CreateTextFontIndex) {
    auto cmd = factory.createCommand(10, 5, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::AttributeElements);
    EXPECT_EQ(cmd->elementId(), 10);
}

TEST_F(CommandFactoryTest, CreateCharacterHeight) {
    auto cmd = factory.createCommand(15, 5, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::AttributeElements);
    EXPECT_EQ(cmd->elementId(), 15);
}

TEST_F(CommandFactoryTest, CreateTextColour) {
    auto cmd = factory.createCommand(14, 5, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::AttributeElements);
    EXPECT_EQ(cmd->elementId(), 14);
}

TEST_F(CommandFactoryTest, CreateInteriorStyle) {
    auto cmd = factory.createCommand(22, 5, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::AttributeElements);
    EXPECT_EQ(cmd->elementId(), 22);
}

TEST_F(CommandFactoryTest, CreateFillColour) {
    auto cmd = factory.createCommand(23, 5, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::AttributeElements);
    EXPECT_EQ(cmd->elementId(), 23);
}

TEST_F(CommandFactoryTest, CreateColourTable) {
    auto cmd = factory.createCommand(34, 5, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::AttributeElements);
    EXPECT_EQ(cmd->elementId(), 34);
}

// ============================================================================
// External Element Creation Tests (Class 7)
// ============================================================================

TEST_F(CommandFactoryTest, CreateMessageCommand) {
    auto cmd = factory.createCommand(1, 7, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::ExternalElements);
    EXPECT_EQ(cmd->elementId(), 1);
}

TEST_F(CommandFactoryTest, CreateApplicationData) {
    auto cmd = factory.createCommand(2, 7, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::ExternalElements);
    EXPECT_EQ(cmd->elementId(), 2);
}

// ============================================================================
// Segment Control Element Creation Tests (Class 8)
// ============================================================================

TEST_F(CommandFactoryTest, CreateCopySegment) {
    auto cmd = factory.createCommand(1, 8, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::SegmentControlandSegmentAttributeElements);
    EXPECT_EQ(cmd->elementId(), 1);
}

// ============================================================================
// Application Structure Element Creation Tests (Class 9)
// ============================================================================

TEST_F(CommandFactoryTest, CreateApplicationStructureAttribute) {
    auto cmd = factory.createCommand(1, 9, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::ApplicationStructureDescriptorElements);
    EXPECT_EQ(cmd->elementId(), 1);
}

// ============================================================================
// Unknown/Reserved Element Tests
// ============================================================================

TEST_F(CommandFactoryTest, CreateUnknownElement) {
    // Class 10-15 are reserved
    auto cmd = factory.createCommand(1, 10, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    // Should create an UnknownCommand
}

TEST_F(CommandFactoryTest, CreateUnknownElementInValidClass) {
    // Element ID 127 (max for 7 bits) in delimiter class
    auto cmd = factory.createCommand(127, 0, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    // Should create an UnknownCommand for unimplemented element
}

// ============================================================================
// Escape Element Tests (Class 6)
// ============================================================================

TEST_F(CommandFactoryTest, CreateEscapeElement) {
    auto cmd = factory.createCommand(0, 6, &cgmFile);
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->elementClass(), opencgm::ClassCode::EscapeElement);
}

// ============================================================================
// All Element Classes Present Test
// ============================================================================

TEST_F(CommandFactoryTest, AllElementClassesSupported) {
    // Test that factory can create commands for all standard element classes
    struct TestCase {
        int elementId;
        int elementClass;
        opencgm::ClassCode expectedClass;
    };

    std::vector<TestCase> cases = {
        {0, 0, opencgm::ClassCode::DelimiterElement},
        {1, 1, opencgm::ClassCode::MetafileDescriptorElements},
        {1, 2, opencgm::ClassCode::PictureDescriptorElements},
        {1, 3, opencgm::ClassCode::ControlElements},
        {1, 4, opencgm::ClassCode::GraphicalPrimitiveElements},
        {1, 5, opencgm::ClassCode::AttributeElements},
        {0, 6, opencgm::ClassCode::EscapeElement},
        {1, 7, opencgm::ClassCode::ExternalElements},
        {1, 8, opencgm::ClassCode::SegmentControlandSegmentAttributeElements},
        {1, 9, opencgm::ClassCode::ApplicationStructureDescriptorElements},
    };

    for (const auto& tc : cases) {
        auto cmd = factory.createCommand(tc.elementId, tc.elementClass, &cgmFile);
        ASSERT_NE(cmd, nullptr) << "Failed for class " << tc.elementClass;
        EXPECT_EQ(cmd->elementClass(), tc.expectedClass)
            << "Wrong class for element class " << tc.elementClass;
    }
}

// ============================================================================
// Command ToString Tests
// ============================================================================

TEST_F(CommandFactoryTest, CommandsHaveToString) {
    // All commands should have a non-empty toString representation
    auto cmd = factory.createCommand(0, 0, &cgmFile);  // NO-OP
    ASSERT_NE(cmd, nullptr);

    std::string str = cmd->toString();
    EXPECT_FALSE(str.empty());
}

TEST_F(CommandFactoryTest, DifferentCommandsHaveDifferentToString) {
    auto noOp = factory.createCommand(0, 0, &cgmFile);
    auto beginMf = factory.createCommand(1, 0, &cgmFile);

    ASSERT_NE(noOp, nullptr);
    ASSERT_NE(beginMf, nullptr);

    // Different commands should have different string representations
    EXPECT_NE(noOp->toString(), beginMf->toString());
}

// ============================================================================
// Factory Stress Test
// ============================================================================

TEST_F(CommandFactoryTest, CreateManyCommands) {
    // Test that factory can create many commands without issues
    std::vector<std::unique_ptr<opencgm::Command>> commands;

    for (int classId = 0; classId < 10; classId++) {
        for (int elementId = 0; elementId < 30; elementId++) {
            auto cmd = factory.createCommand(elementId, classId, &cgmFile);
            if (cmd) {
                commands.push_back(std::move(cmd));
            }
        }
    }

    // Should have created many commands
    EXPECT_GT(commands.size(), 50);
}
