#include <gtest/gtest.h>
#include "opencgm/cgm_file.h"
#include "opencgm/commands/delimiter_commands.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <string>
#include <cstdlib>

// ============================================================================
// Integration tests using real CGM sample files
// Tests binary writer round-trip with actual CGM files
// ============================================================================

namespace fs = std::filesystem;

// Base path for sample files (relative to test executable location)
static const std::string SAMPLES_BASE = "../../../samples/";

class BinaryFileRoundTripTest : public ::testing::Test {
protected:
    // Get the samples directory path
    static fs::path getSamplesPath() {
        // Try multiple possible locations for samples
        // Allow the corpus location to be supplied explicitly. The public
        // repository does not vendor sample data; scripts/fetch-testdata.py
        // downloads it and points this variable at the result.
        if (const char* envDir = std::getenv("OPENCGM_SAMPLES_DIR")) {
            if (fs::exists(envDir)) {
                return fs::absolute(envDir);
            }
        }

        std::vector<fs::path> possiblePaths = {
            fs::path(SAMPLES_BASE),
            fs::path("../samples/"),
            fs::path("../../samples/"),
            fs::path("samples/"),
        };

        for (const auto& path : possiblePaths) {
            if (fs::exists(path)) {
                return fs::absolute(path);
            }
        }

        // Also check using __FILE__ to find relative to source
        fs::path sourceFile(__FILE__);
        auto testDir = sourceFile.parent_path();
        auto samplesFromSource = testDir.parent_path() / "samples";
        if (fs::exists(samplesFromSource)) {
            return samplesFromSource;
        }

        return fs::path();  // Return empty if not found
    }

    // Helper to perform round-trip test on a CGM file
    // Note: Some CGM files may have "defaults replacement" commands that get
    // re-inserted during re-read, causing minor command count differences.
    // allowDefaultsVariance allows for this expected behavior.
    bool performRoundTrip(const fs::path& filePath,
                          size_t& originalCommandCount,
                          size_t& roundTripCommandCount,
                          std::string& errorMessage,
                          bool allowDefaultsVariance = true) {
        try {
            // Read original file
            opencgm::BinaryCGMFile original(filePath.string());
            originalCommandCount = original.commands().size();

            if (originalCommandCount == 0) {
                errorMessage = "Original file has no commands";
                return false;
            }

            // Write to memory buffer
            std::ostringstream oss(std::ios::binary);
            original.writeFile(oss);

            std::string data = oss.str();
            if (data.empty()) {
                errorMessage = "Write produced empty output";
                return false;
            }

            // Read back from buffer
            std::istringstream iss(data, std::ios::binary);
            opencgm::BinaryCGMFile roundTrip(iss, filePath.filename().string());
            roundTripCommandCount = roundTrip.commands().size();

            // Check command count - allow small variance for defaults replacement
            if (originalCommandCount != roundTripCommandCount) {
                if (allowDefaultsVariance) {
                    // Allow for defaults replacement commands being re-inserted
                    // (typically adds 1-3 commands after re-read)
                    size_t diff = (roundTripCommandCount > originalCommandCount)
                        ? (roundTripCommandCount - originalCommandCount)
                        : (originalCommandCount - roundTripCommandCount);
                    if (diff > 5) {
                        errorMessage = "Command count difference too large: original=" +
                                      std::to_string(originalCommandCount) +
                                      " roundtrip=" + std::to_string(roundTripCommandCount);
                        return false;
                    }
                } else {
                    errorMessage = "Command count mismatch: original=" +
                                  std::to_string(originalCommandCount) +
                                  " roundtrip=" + std::to_string(roundTripCommandCount);
                    return false;
                }
            }

            // Verify structure of commands (delimiter structure should match)
            size_t origDelim = 0, rtDelim = 0;
            for (const auto& cmd : original.commands()) {
                if (cmd->elementClass() == opencgm::ClassCode::DelimiterElement) origDelim++;
            }
            for (const auto& cmd : roundTrip.commands()) {
                if (cmd->elementClass() == opencgm::ClassCode::DelimiterElement) rtDelim++;
            }

            if (origDelim != rtDelim) {
                errorMessage = "Delimiter command count mismatch: original=" +
                              std::to_string(origDelim) +
                              " roundtrip=" + std::to_string(rtDelim);
                return false;
            }

            return true;
        }
        catch (const std::exception& e) {
            errorMessage = std::string("Exception: ") + e.what();
            return false;
        }
    }
};

// ============================================================================
// S1000D Sample Files Tests
// ============================================================================

TEST_F(BinaryFileRoundTripTest, S1000D_BikeSample) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    auto filePath = samplesPath / "input" / "ICN-S1000DBIKE-AAA-D000000-0-U8025-00502-A-04-1.CGM";
    if (!fs::exists(filePath)) {
        GTEST_SKIP() << "Sample file not found: " << filePath;
    }

    size_t originalCount, roundTripCount;
    std::string error;
    ASSERT_TRUE(performRoundTrip(filePath, originalCount, roundTripCount, error))
        << "Round-trip failed: " << error;

    EXPECT_GT(originalCount, 0) << "File should have commands";
}

TEST_F(BinaryFileRoundTripTest, S1000D_MultipleSamples) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    auto inputPath = samplesPath / "input";
    if (!fs::exists(inputPath)) {
        GTEST_SKIP() << "Input samples directory not found";
    }

    int testedCount = 0;
    int maxTests = 10;  // Limit to prevent long test times

    for (const auto& entry : fs::directory_iterator(inputPath)) {
        if (testedCount >= maxTests) break;

        if (!entry.is_regular_file()) continue;

        auto ext = entry.path().extension().string();
        // Case-insensitive extension check
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".cgm") continue;

        size_t originalCount, roundTripCount;
        std::string error;

        EXPECT_TRUE(performRoundTrip(entry.path(), originalCount, roundTripCount, error))
            << "Failed for " << entry.path().filename() << ": " << error;

        testedCount++;
    }

    EXPECT_GT(testedCount, 0) << "Should test at least one CGM file";
}

// ============================================================================
// WebCGM 2.1 Test Suite Files
// ============================================================================

TEST_F(BinaryFileRoundTripTest, WebCGM21_CGMMetafile) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    auto filePath = samplesPath / "webcgm21-ts" / "20tests" / "CGM_Metafile.cgm";
    if (!fs::exists(filePath)) {
        GTEST_SKIP() << "WebCGM sample file not found: " << filePath;
    }

    size_t originalCount, roundTripCount;
    std::string error;
    ASSERT_TRUE(performRoundTrip(filePath, originalCount, roundTripCount, error))
        << "Round-trip failed: " << error;
}

TEST_F(BinaryFileRoundTripTest, WebCGM21_ApplicationStructure) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    auto filePath = samplesPath / "webcgm21-ts" / "20tests" / "AppStructure-name.cgm";
    if (!fs::exists(filePath)) {
        GTEST_SKIP() << "WebCGM sample file not found";
    }

    size_t originalCount, roundTripCount;
    std::string error;
    ASSERT_TRUE(performRoundTrip(filePath, originalCount, roundTripCount, error))
        << "Round-trip failed: " << error;
}

TEST_F(BinaryFileRoundTripTest, WebCGM21_MultipleSamples) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    auto webCgmPath = samplesPath / "webcgm21-ts" / "20tests";
    if (!fs::exists(webCgmPath)) {
        GTEST_SKIP() << "WebCGM 20tests directory not found";
    }

    int testedCount = 0;
    int maxTests = 10;

    for (const auto& entry : fs::directory_iterator(webCgmPath)) {
        if (testedCount >= maxTests) break;

        if (!entry.is_regular_file()) continue;

        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".cgm") continue;

        size_t originalCount, roundTripCount;
        std::string error;

        EXPECT_TRUE(performRoundTrip(entry.path(), originalCount, roundTripCount, error))
            << "Failed for " << entry.path().filename() << ": " << error;

        testedCount++;
    }

    EXPECT_GT(testedCount, 0) << "Should test at least one WebCGM file";
}

// ============================================================================
// Output Verification Tests
// ============================================================================

TEST_F(BinaryFileRoundTripTest, OutputIsWordAligned) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    auto filePath = samplesPath / "webcgm21-ts" / "20tests" / "CGM_Metafile.cgm";
    if (!fs::exists(filePath)) {
        GTEST_SKIP() << "Sample file not found";
    }

    opencgm::BinaryCGMFile original(filePath.string());

    std::ostringstream oss(std::ios::binary);
    original.writeFile(oss);

    std::string data = oss.str();
    EXPECT_EQ(data.size() % 2, 0) << "Output must be word-aligned (multiple of 2 bytes)";
}

TEST_F(BinaryFileRoundTripTest, MetafileNamePreserved) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    auto filePath = samplesPath / "webcgm21-ts" / "20tests" / "CGM_Metafile.cgm";
    if (!fs::exists(filePath)) {
        GTEST_SKIP() << "Sample file not found";
    }

    opencgm::BinaryCGMFile original(filePath.string());

    // Find BEGIN METAFILE command and get its name
    std::string originalName;
    for (const auto& cmd : original.commands()) {
        if (cmd->elementClass() == opencgm::ClassCode::DelimiterElement &&
            cmd->elementId() == 1) {  // BEGIN METAFILE
            auto* beginMf = dynamic_cast<opencgm::BeginMetafile*>(cmd.get());
            if (beginMf) {
                originalName = beginMf->name();
                break;
            }
        }
    }

    // Write and read back
    std::ostringstream oss(std::ios::binary);
    original.writeFile(oss);

    std::istringstream iss(oss.str(), std::ios::binary);
    opencgm::BinaryCGMFile roundTrip(iss, "test.cgm");

    // Find the name in the round-trip file
    std::string roundTripName;
    for (const auto& cmd : roundTrip.commands()) {
        if (cmd->elementClass() == opencgm::ClassCode::DelimiterElement &&
            cmd->elementId() == 1) {
            auto* beginMf = dynamic_cast<opencgm::BeginMetafile*>(cmd.get());
            if (beginMf) {
                roundTripName = beginMf->name();
                break;
            }
        }
    }

    EXPECT_EQ(originalName, roundTripName) << "Metafile name should be preserved";
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(BinaryFileRoundTripTest, LargeFilesRoundTrip) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    auto inputPath = samplesPath / "input";
    if (!fs::exists(inputPath)) {
        GTEST_SKIP() << "Input samples directory not found";
    }

    // Find the largest CGM file
    fs::path largestFile;
    uintmax_t largestSize = 0;

    for (const auto& entry : fs::directory_iterator(inputPath)) {
        if (!entry.is_regular_file()) continue;

        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".cgm") continue;

        auto fileSize = entry.file_size();
        if (fileSize > largestSize) {
            largestSize = fileSize;
            largestFile = entry.path();
        }
    }

    if (largestFile.empty()) {
        GTEST_SKIP() << "No CGM files found";
    }

    size_t originalCount, roundTripCount;
    std::string error;

    EXPECT_TRUE(performRoundTrip(largestFile, originalCount, roundTripCount, error))
        << "Largest file (" << largestFile.filename() << ", "
        << largestSize << " bytes) failed: " << error;
}

// ============================================================================
// WebCGM 2.1 Advanced Features Tests
// ============================================================================

TEST_F(BinaryFileRoundTripTest, WebCGM21_NURBS) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    auto filePath = samplesPath / "webcgm21-ts" / "20tests" / "NURBS01.cgm";
    if (!fs::exists(filePath)) {
        GTEST_SKIP() << "NURBS sample file not found";
    }

    size_t originalCount, roundTripCount;
    std::string error;
    ASSERT_TRUE(performRoundTrip(filePath, originalCount, roundTripCount, error))
        << "NURBS round-trip failed: " << error;
}

TEST_F(BinaryFileRoundTripTest, WebCGM21_UTF8Chinese) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    auto filePath = samplesPath / "webcgm21-ts" / "20tests" / "utf8-chinese-v20.cgm";
    if (!fs::exists(filePath)) {
        GTEST_SKIP() << "UTF-8 Chinese sample file not found";
    }

    size_t originalCount, roundTripCount;
    std::string error;
    ASSERT_TRUE(performRoundTrip(filePath, originalCount, roundTripCount, error))
        << "UTF-8 Chinese round-trip failed: " << error;
}

TEST_F(BinaryFileRoundTripTest, WebCGM21_InterpolatedInterior) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    auto filePath = samplesPath / "webcgm21-ts" / "20tests" / "interpolated-interior-01.cgm";
    if (!fs::exists(filePath)) {
        GTEST_SKIP() << "Interpolated interior sample file not found";
    }

    size_t originalCount, roundTripCount;
    std::string error;
    ASSERT_TRUE(performRoundTrip(filePath, originalCount, roundTripCount, error))
        << "Interpolated interior round-trip failed: " << error;
}

