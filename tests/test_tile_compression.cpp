#include <gtest/gtest.h>
#include "opencgm/cgm_file.h"
#include "opencgm/svg_converter.h"
#include "opencgm/svg/svg_utils.h"
#include "opencgm/commands/graphical_primitive_commands.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdlib>

// ============================================================================
// Tile and Cell Array Compression Tests
// Tests tile/cell array handling including various compression types:
//   - NULL_BACKGROUND (0)
//   - RUN_LENGTH (1)
//   - PACKED (2)
//   - T4/CCITT Group 3 (3)
//   - T6/CCITT Group 4 (4)
//   - BITMAP (5)
//   - JPEG (6)
//   - PNG (7)
// ============================================================================

namespace fs = std::filesystem;

class TileCompressionTest : public ::testing::Test {
protected:
    // Get the samples directory path
    static fs::path getSamplesPath() {
        // Allow the corpus location to be supplied explicitly. The public
        // repository does not vendor sample data; scripts/fetch-testdata.py
        // downloads it and points this variable at the result.
        if (const char* envDir = std::getenv("OPENCGM_SAMPLES_DIR")) {
            if (fs::exists(envDir)) {
                return fs::absolute(envDir);
            }
        }

        std::vector<fs::path> possiblePaths = {
            fs::path("../../../samples/"),
            fs::path("../samples/"),
            fs::path("../../samples/"),
            fs::path("samples/"),
        };

        for (const auto& path : possiblePaths) {
            if (fs::exists(path)) {
                return fs::absolute(path);
            }
        }

        // Check using __FILE__ to find relative to source
        fs::path sourceFile(__FILE__);
        auto testDir = sourceFile.parent_path();
        auto samplesFromSource = testDir.parent_path() / "samples";
        if (fs::exists(samplesFromSource)) {
            return samplesFromSource;
        }

        return fs::path();
    }

    // Helper to convert CGM file to SVG and check for image data
    bool convertToSVGWithImages(const fs::path& filePath,
                                 std::string& svgOutput,
                                 std::string& errorMessage) {
        try {
            opencgm::BinaryCGMFile cgmFile(filePath.string());

            opencgm::SVGConverter converter(&cgmFile);
            svgOutput = converter.convert();

            return !svgOutput.empty();
        }
        catch (const std::exception& e) {
            errorMessage = std::string("Exception: ") + e.what();
            return false;
        }
    }

    // Check if SVG contains embedded image data
    bool svgContainsEmbeddedImage(const std::string& svg) {
        // Check for data URI scheme for images
        return svg.find("data:image/png") != std::string::npos ||
               svg.find("data:image/jpeg") != std::string::npos ||
               svg.find("xlink:href=\"data:") != std::string::npos ||
               svg.find("href=\"data:") != std::string::npos;
    }

    // Check if SVG contains image element
    bool svgContainsImageElement(const std::string& svg) {
        return svg.find("<image") != std::string::npos;
    }

    // Helper to find CGM files containing Cell Array commands
    std::vector<fs::path> findCellArrayFiles(const fs::path& directory, int maxFiles = 10) {
        std::vector<fs::path> result;

        if (!fs::exists(directory)) {
            return result;
        }

        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            if (result.size() >= static_cast<size_t>(maxFiles)) break;

            if (!entry.is_regular_file()) continue;

            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext != ".cgm") continue;

            try {
                opencgm::BinaryCGMFile file(entry.path().string());

                // Check for Cell Array, Tile, or BitonalTile commands
                for (const auto& cmd : file.commands()) {
                    if (cmd->elementClass() == opencgm::ClassCode::GraphicalPrimitiveElements) {
                        int id = cmd->elementId();
                        // Cell Array (9), Tile (28), Bitonal Tile (29)
                        if (id == 9 || id == 28 || id == 29) {
                            result.push_back(entry.path());
                            break;
                        }
                    }
                }
            }
            catch (...) {
                // Skip files that can't be parsed
            }
        }

        return result;
    }
};

// ============================================================================
// Cell Array Tests (WebCGM Static10 CELARY files)
// ============================================================================

TEST_F(TileCompressionTest, CELARY_ParsesSuccessfully) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    auto celaryPath = samplesPath / "webcgm21-ts" / "static10" / "CELARY01.cgm";
    if (!fs::exists(celaryPath)) {
        GTEST_SKIP() << "CELARY01.cgm not found";
    }

    opencgm::BinaryCGMFile file(celaryPath.string());
    EXPECT_GT(file.commands().size(), 0) << "File should have commands";

    // Verify file contains Cell Array command
    bool hasCellArray = false;
    for (const auto& cmd : file.commands()) {
        if (cmd->elementClass() == opencgm::ClassCode::GraphicalPrimitiveElements &&
            cmd->elementId() == 9) {  // Cell Array
            hasCellArray = true;
            break;
        }
    }
    EXPECT_TRUE(hasCellArray) << "CELARY file should contain Cell Array command";
}

TEST_F(TileCompressionTest, CELARY_ConvertToSVG) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    auto celaryPath = samplesPath / "webcgm21-ts" / "static10" / "CELARY01.cgm";
    if (!fs::exists(celaryPath)) {
        GTEST_SKIP() << "CELARY01.cgm not found";
    }

    std::string svgOutput, error;
    ASSERT_TRUE(convertToSVGWithImages(celaryPath, svgOutput, error))
        << "Conversion failed: " << error;

    EXPECT_FALSE(svgOutput.empty()) << "SVG output should not be empty";
    EXPECT_TRUE(svgOutput.find("<svg") != std::string::npos)
        << "Output should be valid SVG";

    // Cell arrays should produce image elements in SVG
    EXPECT_TRUE(svgContainsImageElement(svgOutput))
        << "Cell Array should produce image element in SVG";
}

TEST_F(TileCompressionTest, CELARY_AllVariants) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    auto static10Path = samplesPath / "webcgm21-ts" / "static10";
    if (!fs::exists(static10Path)) {
        GTEST_SKIP() << "Static10 directory not found";
    }

    // Test all CELARY variants (01-07)
    for (int i = 1; i <= 7; ++i) {
        std::string filename = "CELARY0" + std::to_string(i) + ".cgm";
        auto filePath = static10Path / filename;

        if (!fs::exists(filePath)) continue;

        std::string svgOutput, error;
        EXPECT_TRUE(convertToSVGWithImages(filePath, svgOutput, error))
            << "Conversion failed for " << filename << ": " << error;

        EXPECT_FALSE(svgOutput.empty())
            << filename << " should produce non-empty SVG";
    }
}

// ============================================================================
// Compression Type Detection Tests
// ============================================================================

TEST_F(TileCompressionTest, DetectCompressionTypes) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    // Search for files with tiles/cell arrays
    auto cellArrayFiles = findCellArrayFiles(samplesPath, 20);

    if (cellArrayFiles.empty()) {
        GTEST_SKIP() << "No CGM files with Cell Array/Tile commands found";
    }

    std::map<int, int> compressionTypeCounts;

    for (const auto& filePath : cellArrayFiles) {
        try {
            opencgm::BinaryCGMFile file(filePath.string());

            for (const auto& cmd : file.commands()) {
                if (cmd->elementClass() == opencgm::ClassCode::GraphicalPrimitiveElements) {
                    // Try to get compression type from Tile commands
                    auto* tileCmd = dynamic_cast<opencgm::Tile*>(cmd.get());
                    if (tileCmd) {
                        compressionTypeCounts[tileCmd->compressionType()]++;
                    }
                }
            }
        }
        catch (...) {
            // Skip problematic files
        }
    }

    // Report found compression types
    for (const auto& [type, count] : compressionTypeCounts) {
        std::cout << "Compression type " << type << ": " << count << " occurrences\n";
    }
}

// ============================================================================
// JPEG Passthrough Tests
// ============================================================================

TEST_F(TileCompressionTest, JPEG_Passthrough) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    // Find files with JPEG tiles
    auto cellArrayFiles = findCellArrayFiles(samplesPath, 50);

    for (const auto& filePath : cellArrayFiles) {
        try {
            opencgm::BinaryCGMFile file(filePath.string());

            for (const auto& cmd : file.commands()) {
                if (cmd->elementClass() == opencgm::ClassCode::GraphicalPrimitiveElements) {
                    auto* tileCmd = dynamic_cast<opencgm::Tile*>(cmd.get());
                    if (tileCmd && tileCmd->compressionType() == 6) {  // JPEG
                        std::string svgOutput, error;
                        EXPECT_TRUE(convertToSVGWithImages(filePath, svgOutput, error))
                            << "JPEG tile conversion failed: " << error;

                        if (!svgOutput.empty()) {
                            EXPECT_TRUE(svgOutput.find("data:image/jpeg") != std::string::npos ||
                                       svgOutput.find("data:image/png") != std::string::npos)
                                << "JPEG tile should produce embedded image in SVG";
                        }
                        return;  // Found and tested one
                    }
                }
            }
        }
        catch (...) {
            // Skip problematic files
        }
    }

    GTEST_SKIP() << "No CGM files with JPEG tiles found in test corpus";
}

// ============================================================================
// PNG Passthrough Tests
// ============================================================================

TEST_F(TileCompressionTest, PNG_Passthrough) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    // Find files with tiles - compression type may not match actual content type
    // (PNG data can be stored with various compression type values but detected by magic bytes)
    auto cellArrayFiles = findCellArrayFiles(samplesPath, 50);

    for (const auto& filePath : cellArrayFiles) {
        try {
            std::string svgOutput, error;
            if (!convertToSVGWithImages(filePath, svgOutput, error)) {
                continue;  // Skip files that fail to convert
            }

            // Check if the converted SVG contains PNG image data
            if (svgOutput.find("data:image/png") != std::string::npos) {
                // Found a file that produces PNG output - verify it's valid
                EXPECT_TRUE(svgOutput.find(";base64,") != std::string::npos)
                    << "PNG data should be base64 encoded";
                EXPECT_TRUE(svgContainsImageElement(svgOutput))
                    << "PNG data should be in an image element";
                return;  // Found and tested one
            }
        }
        catch (...) {
            // Skip problematic files
        }
    }

    GTEST_SKIP() << "No CGM files producing PNG output found in test corpus";
}

// ============================================================================
// BITMAP (Type 5) Tests
// ============================================================================

TEST_F(TileCompressionTest, BITMAP_Decoding) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    auto cellArrayFiles = findCellArrayFiles(samplesPath, 50);

    for (const auto& filePath : cellArrayFiles) {
        try {
            opencgm::BinaryCGMFile file(filePath.string());

            for (const auto& cmd : file.commands()) {
                if (cmd->elementClass() == opencgm::ClassCode::GraphicalPrimitiveElements) {
                    auto* tileCmd = dynamic_cast<opencgm::Tile*>(cmd.get());
                    if (tileCmd && tileCmd->compressionType() == 5) {  // BITMAP
                        std::string svgOutput, error;
                        EXPECT_TRUE(convertToSVGWithImages(filePath, svgOutput, error))
                            << "BITMAP tile conversion failed: " << error;

                        if (!svgOutput.empty()) {
                            EXPECT_TRUE(svgContainsImageElement(svgOutput))
                                << "BITMAP tile should produce image in SVG";
                        }
                        return;  // Found and tested one
                    }
                }
            }
        }
        catch (...) {
            // Skip problematic files
        }
    }

    GTEST_SKIP() << "No CGM files with BITMAP tiles found in test corpus";
}

// ============================================================================
// CCITT Group 4 (T6) Tests - Windows only
// ============================================================================

#ifdef _WIN32
TEST_F(TileCompressionTest, CCITT_G4_Decoding) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    auto cellArrayFiles = findCellArrayFiles(samplesPath, 50);

    for (const auto& filePath : cellArrayFiles) {
        try {
            opencgm::BinaryCGMFile file(filePath.string());

            for (const auto& cmd : file.commands()) {
                if (cmd->elementClass() == opencgm::ClassCode::GraphicalPrimitiveElements) {
                    auto* tileCmd = dynamic_cast<opencgm::Tile*>(cmd.get());
                    if (tileCmd && tileCmd->compressionType() == 4) {  // T6/CCITT G4
                        std::string svgOutput, error;
                        EXPECT_TRUE(convertToSVGWithImages(filePath, svgOutput, error))
                            << "CCITT G4 tile conversion failed: " << error;

                        if (!svgOutput.empty()) {
                            EXPECT_TRUE(svgContainsImageElement(svgOutput))
                                << "CCITT G4 tile should produce image in SVG";
                        }
                        return;  // Found and tested one
                    }
                }
            }
        }
        catch (...) {
            // Skip problematic files
        }
    }

    GTEST_SKIP() << "No CGM files with CCITT G4 tiles found in test corpus";
}
#endif

// ============================================================================
// SVG Output Validation Tests
// ============================================================================

TEST_F(TileCompressionTest, ImageElementHasValidDimensions) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    auto celaryPath = samplesPath / "webcgm21-ts" / "static10" / "CELARY01.cgm";
    if (!fs::exists(celaryPath)) {
        GTEST_SKIP() << "CELARY01.cgm not found";
    }

    std::string svgOutput, error;
    ASSERT_TRUE(convertToSVGWithImages(celaryPath, svgOutput, error));

    // Check for width and height attributes on image elements
    if (svgContainsImageElement(svgOutput)) {
        EXPECT_TRUE(svgOutput.find("width=") != std::string::npos)
            << "Image element should have width attribute";
        EXPECT_TRUE(svgOutput.find("height=") != std::string::npos)
            << "Image element should have height attribute";
    }
}

TEST_F(TileCompressionTest, ImageDataIsBase64Encoded) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    auto celaryPath = samplesPath / "webcgm21-ts" / "static10" / "CELARY01.cgm";
    if (!fs::exists(celaryPath)) {
        GTEST_SKIP() << "CELARY01.cgm not found";
    }

    std::string svgOutput, error;
    ASSERT_TRUE(convertToSVGWithImages(celaryPath, svgOutput, error));

    // Check for base64 encoding marker
    if (svgContainsEmbeddedImage(svgOutput)) {
        EXPECT_TRUE(svgOutput.find(";base64,") != std::string::npos)
            << "Embedded image should be base64 encoded";
    }
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F(TileCompressionTest, EmptyTileHandling) {
    // Test that empty or zero-dimension tiles don't crash
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    auto cellArrayFiles = findCellArrayFiles(samplesPath, 30);

    for (const auto& filePath : cellArrayFiles) {
        std::string svgOutput, error;
        // Should not throw, even for edge cases
        EXPECT_NO_THROW({
            convertToSVGWithImages(filePath, svgOutput, error);
        }) << "Conversion should not throw for: " << filePath.filename();
    }
}

// ============================================================================
// Round-trip Tests for Tile Data
// ============================================================================

TEST_F(TileCompressionTest, TileDataRoundTrip) {
    auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    auto celaryPath = samplesPath / "webcgm21-ts" / "static10" / "CELARY01.cgm";
    if (!fs::exists(celaryPath)) {
        GTEST_SKIP() << "CELARY01.cgm not found";
    }

    // Read original
    opencgm::BinaryCGMFile original(celaryPath.string());

    // Write to buffer
    std::ostringstream oss(std::ios::binary);
    original.writeFile(oss);

    // Read back
    std::string data = oss.str();
    ASSERT_FALSE(data.empty()) << "Written data should not be empty";

    std::istringstream iss(data, std::ios::binary);
    opencgm::BinaryCGMFile roundTrip(iss, "test.cgm");

    // Verify cell array commands are preserved
    int origCellArrayCount = 0, rtCellArrayCount = 0;

    for (const auto& cmd : original.commands()) {
        if (cmd->elementClass() == opencgm::ClassCode::GraphicalPrimitiveElements &&
            cmd->elementId() == 9) {
            origCellArrayCount++;
        }
    }

    for (const auto& cmd : roundTrip.commands()) {
        if (cmd->elementClass() == opencgm::ClassCode::GraphicalPrimitiveElements &&
            cmd->elementId() == 9) {
            rtCellArrayCount++;
        }
    }

    EXPECT_EQ(origCellArrayCount, rtCellArrayCount)
        << "Cell Array count should be preserved in round-trip";
}

// ============================================================================
// Shared SVG/Raster Utility Tests
// ============================================================================

TEST(SvgUtilsTest, Base64EncodesKnownValues) {
    EXPECT_EQ(opencgm::svg::base64Encode(""), "");
    EXPECT_EQ(opencgm::svg::base64Encode("f"), "Zg==");
    EXPECT_EQ(opencgm::svg::base64Encode("fo"), "Zm8=");
    EXPECT_EQ(opencgm::svg::base64Encode("foo"), "Zm9v");
}

TEST(SvgUtilsTest, EnsurePngHeaderPreservesSignedPng) {
    const std::vector<uint8_t> png = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
        0x01, 0x02, 0x03};

    EXPECT_EQ(opencgm::svg::ensurePngHeader(png, 10, 20), png);
    EXPECT_TRUE(opencgm::svg::hasPngSignature(png));
}

TEST(SvgUtilsTest, EnsurePngHeaderBuildsExpectedIhdr) {
    const std::vector<uint8_t> payload = {0x01, 0x02};
    const auto png = opencgm::svg::ensurePngHeader(payload, 0x01020304u, 0x05060708u, 8, 6);

    ASSERT_GE(png.size(), 33u);
    EXPECT_TRUE(opencgm::svg::hasPngSignature(png));
    EXPECT_EQ(png[12], 'I');
    EXPECT_EQ(png[13], 'H');
    EXPECT_EQ(png[14], 'D');
    EXPECT_EQ(png[15], 'R');
    EXPECT_EQ(png[16], 0x01);
    EXPECT_EQ(png[17], 0x02);
    EXPECT_EQ(png[18], 0x03);
    EXPECT_EQ(png[19], 0x04);
    EXPECT_EQ(png[20], 0x05);
    EXPECT_EQ(png[21], 0x06);
    EXPECT_EQ(png[22], 0x07);
    EXPECT_EQ(png[23], 0x08);
    EXPECT_EQ(png[24], 8);
    EXPECT_EQ(png[25], 6);
}

TEST(SvgUtilsTest, BuildPngFromIdatAddsSignatureAndIend) {
    const std::vector<uint8_t> idatChunk = {
        0x00, 0x00, 0x00, 0x01,
        'I', 'D', 'A', 'T',
        0x00,
        0x35, 0xAF, 0x06, 0x1E};

    const auto png = opencgm::svg::buildPngFromIdat(idatChunk, 1, 1, 8, 2);

    ASSERT_GE(png.size(), 12u);
    EXPECT_TRUE(opencgm::svg::hasPngSignature(png));
    EXPECT_EQ(png.end()[-8], 'I');
    EXPECT_EQ(png.end()[-7], 'E');
    EXPECT_EQ(png.end()[-6], 'N');
    EXPECT_EQ(png.end()[-5], 'D');
}

TEST(SvgUtilsTest, GeometryAndColorHelpersRemainConsistent) {
    const opencgm::CGMPoint xAxis(3.0, 0.0);
    const opencgm::CGMPoint yAxis(0.0, 2.0);

    const auto [rx, ry, rotation] = opencgm::svg::conjugateDiametersToEllipse(xAxis, yAxis);
    EXPECT_NEAR(rx, 3.0, 1e-10);
    EXPECT_NEAR(ry, 2.0, 1e-10);
    EXPECT_NEAR(rotation, 0.0, 1e-10);

    EXPECT_TRUE(opencgm::svg::vectorsNearlyEqual(
        opencgm::CGMPoint(1.0, 2.0),
        opencgm::CGMPoint(1.0, 2.0)));
    EXPECT_TRUE(opencgm::svg::colorsEqualRgb(
        opencgm::Color(1, 2, 3),
        opencgm::Color(1, 2, 3)));
    EXPECT_TRUE(opencgm::svg::colorsNearlyEqualRgb(
        opencgm::Color(1, 2, 3),
        opencgm::Color(2, 3, 4),
        1));
    EXPECT_EQ(opencgm::svg::colorToHexString(opencgm::Color(1, 2, 3)), "#010203");
}

TEST(SvgUtilsTest, DerivePngFormatSupportsDirectRgb) {
    uint8_t bitDepth = 0;
    uint8_t colorType = 0;

    EXPECT_TRUE(opencgm::svg::derivePngFormat(
        opencgm::ColorSelectionMode::DIRECT,
        opencgm::ColorModel::RGB,
        16,
        8,
        8,
        bitDepth,
        colorType));
    EXPECT_EQ(bitDepth, 16);
    EXPECT_EQ(colorType, 2);

    EXPECT_FALSE(opencgm::svg::derivePngFormat(
        opencgm::ColorSelectionMode::INDEXED,
        opencgm::ColorModel::RGB,
        8,
        8,
        8,
        bitDepth,
        colorType));
}

TEST(SvgUtilsTest, SplitTextIntoLinesPreservesLineStructure) {
    EXPECT_EQ(
        opencgm::svg::splitTextIntoLines("line1\r\nline2\n"),
        (std::vector<std::string>{"line1", "line2", ""}));
    EXPECT_EQ(
        opencgm::svg::splitTextIntoLines(""),
        (std::vector<std::string>{""}));
}

TEST_F(TileCompressionTest, LinkUriAttributeOnGrobjectEmitsAnchor) {
    const auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    const auto sample = samplesPath / "webcgm21-ts" / "20tests" /
                        "AppStructure-linkuri.cgm";
    ASSERT_TRUE(fs::exists(sample));

    opencgm::BinaryCGMFile file(sample.string());
    opencgm::SVGConverter converter(&file);
    const std::string svg = converter.convert();

    EXPECT_NE(svg.find("<a xlink:href=\"http://www.cgmopen.org/\""),
              std::string::npos);
    EXPECT_NE(svg.find("<title>Plane 3 of 6</title>"), std::string::npos);
}

TEST_F(TileCompressionTest, HotspotEncodingAndRegionModesAreHonored) {
    const auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    const auto sample = samplesPath / "webcgm21-ts" / "20tests" /
                        "AppStructure-linkuri.cgm";
    ASSERT_TRUE(fs::exists(sample));

    opencgm::BinaryCGMFile file(sample.string());
    opencgm::SVGConverter converter(&file);
    auto config = opencgm::OutputTargetConfig::forS1000DIETP();
    config.hotspot_encoding = opencgm::HotspotEncodingMode::DataAttributes;
    config.region_handling = opencgm::RegionHandlingMode::BboxOnly;
    converter.setOutputTargetConfig(config);
    const std::string svg = converter.convert();

    EXPECT_EQ(svg.find("<a xlink:href="), std::string::npos);
    EXPECT_NE(svg.find("webcgm:linkuri=\"http://www.cgmopen.org/\""),
              std::string::npos);
    EXPECT_EQ(svg.find("class=\"aps-region-hit\""), std::string::npos);
}

TEST_F(TileCompressionTest, StrictHotspotProfileSuppressesAnchorAndMetadata) {
    const auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    const auto sample = samplesPath / "webcgm21-ts" / "20tests" /
                        "AppStructure-linkuri.cgm";
    ASSERT_TRUE(fs::exists(sample));

    opencgm::BinaryCGMFile file(sample.string());
    opencgm::SVGConverter converter(&file);
    converter.setHotspotProfile(opencgm::HotspotProfile::Strict);
    const std::string svg = converter.convert();

    EXPECT_EQ(svg.find("<a xlink:href="), std::string::npos);
    EXPECT_EQ(svg.find("<title>Plane 3 of 6</title>"), std::string::npos);
}

TEST_F(TileCompressionTest, CustomHotspotAttributesAreScopedAndEscaped) {
    const auto samplesPath = getSamplesPath();
    if (samplesPath.empty()) {
        GTEST_SKIP() << "Samples directory not found";
    }

    const auto sample = samplesPath / "webcgm21-ts" / "20tests" /
                        "AppStructure-linkuri.cgm";
    ASSERT_TRUE(fs::exists(sample));

    opencgm::BinaryCGMFile file(sample.string());
    opencgm::SVGConverter converter(&file);
    converter.addCustomAttribute({
        "data-review", "A&B", opencgm::CustomAttributeScope::ApsId, "plane_3"
    });
    converter.addCustomAttribute({
        "data-missing", "no", opencgm::CustomAttributeScope::ApsId, "other"
    });
    converter.addCustomAttribute({
        "onload", "alert(1)", opencgm::CustomAttributeScope::All, "*"
    });
    const std::string svg = converter.convert();

    EXPECT_NE(svg.find("data-review=\"A&amp;B\""), std::string::npos);
    EXPECT_EQ(svg.find("data-missing="), std::string::npos);
    EXPECT_EQ(svg.find("onload="), std::string::npos);
}
