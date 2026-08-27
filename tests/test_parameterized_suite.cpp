#include <gtest/gtest.h>
#include "opencgm/cgm_file.h"
#include "opencgm/svg_converter.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <regex>
#include <cstdlib>

// ============================================================================
// Parameterized Test Suite for CGM Files
// Flexible test runner for processing directories of CGM test files
// including NIST, WebCGM, and custom test suites.
// ============================================================================

namespace fs = std::filesystem;

// Utility class to collect CGM file paths from directories
class CGMTestFileCollector {
public:
    static std::vector<std::string> collectFromDirectory(
            const fs::path& directory,
            int maxFiles = -1,
            const std::string& pattern = "") {
        std::vector<std::string> files;

        if (!fs::exists(directory)) {
            return files;
        }

        std::regex patternRegex;
        bool usePattern = !pattern.empty();
        if (usePattern) {
            patternRegex = std::regex(pattern, std::regex::icase);
        }

        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            if (maxFiles > 0 && static_cast<int>(files.size()) >= maxFiles) break;

            if (!entry.is_regular_file()) continue;

            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext != ".cgm") continue;

            if (usePattern) {
                std::string filename = entry.path().filename().string();
                if (!std::regex_search(filename, patternRegex)) {
                    continue;
                }
            }

            files.push_back(entry.path().string());
        }

        return files;
    }

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

        fs::path sourceFile(__FILE__);
        auto testDir = sourceFile.parent_path();
        auto samplesFromSource = testDir.parent_path() / "samples";
        if (fs::exists(samplesFromSource)) {
            return samplesFromSource;
        }

        return fs::path();
    }
};

// ============================================================================
// Base Test Fixture for Parameterized CGM Tests
// ============================================================================

class CGMParameterizedTest : public ::testing::TestWithParam<std::string> {
protected:
    std::string filePath_;

    void SetUp() override {
        filePath_ = GetParam();
    }

    // Basic parsing test - file should parse without exception
    bool parseFile(std::string& errorMessage) {
        try {
            opencgm::BinaryCGMFile file(filePath_);
            return file.commands().size() > 0;
        }
        catch (const std::exception& e) {
            errorMessage = e.what();
            return false;
        }
    }

    // SVG conversion test - file should convert to valid SVG
    bool convertToSVG(std::string& svgOutput, std::string& errorMessage) {
        try {
            opencgm::BinaryCGMFile cgmFile(filePath_);
            opencgm::SVGConverter converter(&cgmFile);
            converter.setCompatibilityMode(true);  // Handle malformed tile dimensions
            svgOutput = converter.convert();
            return !svgOutput.empty() && svgOutput.find("<svg") != std::string::npos;
        }
        catch (const std::exception& e) {
            errorMessage = e.what();
            return false;
        }
    }

    // Round-trip test - file should survive write/read cycle
    bool roundTrip(std::string& errorMessage) {
        try {
            opencgm::BinaryCGMFile original(filePath_);
            size_t originalCount = original.commands().size();

            std::ostringstream oss(std::ios::binary);
            original.writeFile(oss);

            std::string data = oss.str();
            if (data.empty()) {
                errorMessage = "Write produced empty output";
                return false;
            }

            std::istringstream iss(data, std::ios::binary);
            opencgm::BinaryCGMFile roundTrip(iss, fs::path(filePath_).filename().string());
            size_t rtCount = roundTrip.commands().size();

            // Allow small variance for defaults replacement
            size_t diff = (rtCount > originalCount) ?
                (rtCount - originalCount) : (originalCount - rtCount);
            if (diff > 5) {
                errorMessage = "Command count difference too large: original=" +
                              std::to_string(originalCount) +
                              " roundtrip=" + std::to_string(rtCount);
                return false;
            }

            return true;
        }
        catch (const std::exception& e) {
            errorMessage = e.what();
            return false;
        }
    }

    // Validate SVG structure
    bool validateSVGStructure(const std::string& svg) {
        // Check for required SVG elements
        return svg.find("<svg") != std::string::npos &&
               svg.find("</svg>") != std::string::npos &&
               svg.find("xmlns") != std::string::npos;
    }
};

// ============================================================================
// WebCGM 2.0 Test Suite (20tests directory)
// ============================================================================

class WebCGM20Test : public CGMParameterizedTest {};

TEST_P(WebCGM20Test, ParsesWithoutCrash) {
    std::string error;
    EXPECT_TRUE(parseFile(error))
        << "Failed to parse " << fs::path(filePath_).filename() << ": " << error;
}

TEST_P(WebCGM20Test, GeneratesValidSVG) {
    std::string svgOutput, error;
    EXPECT_TRUE(convertToSVG(svgOutput, error))
        << "SVG conversion failed for " << fs::path(filePath_).filename() << ": " << error;

    if (!svgOutput.empty()) {
        EXPECT_TRUE(validateSVGStructure(svgOutput))
            << "Invalid SVG structure for " << fs::path(filePath_).filename();
    }
}

TEST_P(WebCGM20Test, RoundTrips) {
    std::string error;
    EXPECT_TRUE(roundTrip(error))
        << "Round-trip failed for " << fs::path(filePath_).filename() << ": " << error;
}

std::vector<std::string> GetWebCGM20Files() {
    auto samplesPath = CGMTestFileCollector::getSamplesPath();
    if (samplesPath.empty()) return {};
    return CGMTestFileCollector::collectFromDirectory(
        samplesPath / "webcgm21-ts" / "20tests", 30);
}

INSTANTIATE_TEST_SUITE_P(
    WebCGM20,
    WebCGM20Test,
    ::testing::ValuesIn(GetWebCGM20Files()),
    [](const ::testing::TestParamInfo<std::string>& info) {
        // Create a valid test name from the file path
        std::string name = fs::path(info.param).stem().string();
        // Replace invalid characters
        std::replace_if(name.begin(), name.end(),
            [](char c) { return !std::isalnum(c); }, '_');
        return name;
    });

// ============================================================================
// WebCGM 2.1 Test Suite (21tests directory)
// ============================================================================

class WebCGM21Test : public CGMParameterizedTest {};

TEST_P(WebCGM21Test, ParsesWithoutCrash) {
    std::string error;
    EXPECT_TRUE(parseFile(error))
        << "Failed to parse " << fs::path(filePath_).filename() << ": " << error;
}

TEST_P(WebCGM21Test, GeneratesValidSVG) {
    std::string svgOutput, error;
    EXPECT_TRUE(convertToSVG(svgOutput, error))
        << "SVG conversion failed for " << fs::path(filePath_).filename() << ": " << error;
}

std::vector<std::string> GetWebCGM21Files() {
    auto samplesPath = CGMTestFileCollector::getSamplesPath();
    if (samplesPath.empty()) return {};
    return CGMTestFileCollector::collectFromDirectory(
        samplesPath / "webcgm21-ts" / "21tests", 30);
}

INSTANTIATE_TEST_SUITE_P(
    WebCGM21,
    WebCGM21Test,
    ::testing::ValuesIn(GetWebCGM21Files()),
    [](const ::testing::TestParamInfo<std::string>& info) {
        std::string name = fs::path(info.param).stem().string();
        std::replace_if(name.begin(), name.end(),
            [](char c) { return !std::isalnum(c); }, '_');
        return name;
    });

// ============================================================================
// Static 1.0 Test Suite (static10 directory - NIST CGM conformance tests)
// ============================================================================

class Static10Test : public CGMParameterizedTest {};

TEST_P(Static10Test, ParsesWithoutCrash) {
    std::string error;
    EXPECT_TRUE(parseFile(error))
        << "Failed to parse " << fs::path(filePath_).filename() << ": " << error;
}

TEST_P(Static10Test, GeneratesValidSVG) {
    std::string svgOutput, error;
    EXPECT_TRUE(convertToSVG(svgOutput, error))
        << "SVG conversion failed for " << fs::path(filePath_).filename() << ": " << error;
}

std::vector<std::string> GetStatic10Files() {
    auto samplesPath = CGMTestFileCollector::getSamplesPath();
    if (samplesPath.empty()) return {};
    return CGMTestFileCollector::collectFromDirectory(
        samplesPath / "webcgm21-ts" / "static10", 50);
}

INSTANTIATE_TEST_SUITE_P(
    Static10,
    Static10Test,
    ::testing::ValuesIn(GetStatic10Files()),
    [](const ::testing::TestParamInfo<std::string>& info) {
        std::string name = fs::path(info.param).stem().string();
        std::replace_if(name.begin(), name.end(),
            [](char c) { return !std::isalnum(c); }, '_');
        return name;
    });

// ============================================================================
// S1000D Sample Files Test Suite
// ============================================================================

class S1000DTest : public CGMParameterizedTest {};

TEST_P(S1000DTest, ParsesWithoutCrash) {
    std::string error;
    EXPECT_TRUE(parseFile(error))
        << "Failed to parse " << fs::path(filePath_).filename() << ": " << error;
}

TEST_P(S1000DTest, GeneratesValidSVG) {
    std::string svgOutput, error;
    EXPECT_TRUE(convertToSVG(svgOutput, error))
        << "SVG conversion failed for " << fs::path(filePath_).filename() << ": " << error;
}

TEST_P(S1000DTest, RoundTrips) {
    std::string error;
    EXPECT_TRUE(roundTrip(error))
        << "Round-trip failed for " << fs::path(filePath_).filename() << ": " << error;
}

std::vector<std::string> GetS1000DFiles() {
    auto samplesPath = CGMTestFileCollector::getSamplesPath();
    if (samplesPath.empty()) return {};
    return CGMTestFileCollector::collectFromDirectory(
        samplesPath / "input", 30);
}

INSTANTIATE_TEST_SUITE_P(
    S1000D,
    S1000DTest,
    ::testing::ValuesIn(GetS1000DFiles()),
    [](const ::testing::TestParamInfo<std::string>& info) {
        std::string name = fs::path(info.param).stem().string();
        std::replace_if(name.begin(), name.end(),
            [](char c) { return !std::isalnum(c); }, '_');
        return name;
    });

// ============================================================================
// NIST Test Suite (if available)
// Placeholder for future NIST CGM Validation Suite integration
// ============================================================================

class NISTTest : public CGMParameterizedTest {};

TEST_P(NISTTest, ParsesWithoutCrash) {
    std::string error;
    EXPECT_TRUE(parseFile(error))
        << "Failed to parse " << fs::path(filePath_).filename() << ": " << error;
}

TEST_P(NISTTest, GeneratesValidSVG) {
    std::string svgOutput, error;
    EXPECT_TRUE(convertToSVG(svgOutput, error))
        << "SVG conversion failed for " << fs::path(filePath_).filename() << ": " << error;
}

std::vector<std::string> GetNISTFiles() {
    auto samplesPath = CGMTestFileCollector::getSamplesPath();
    if (samplesPath.empty()) return {};

    // Check for NIST test suite directory
    auto nistPath = samplesPath / "nist-ts";
    if (!fs::exists(nistPath)) {
        // NIST suite not installed - return empty to skip
        return {};
    }

    return CGMTestFileCollector::collectFromDirectory(nistPath, 100);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(NISTTest);

INSTANTIATE_TEST_SUITE_P(
    NIST,
    NISTTest,
    ::testing::ValuesIn(GetNISTFiles()),
    [](const ::testing::TestParamInfo<std::string>& info) {
        std::string name = fs::path(info.param).stem().string();
        std::replace_if(name.begin(), name.end(),
            [](char c) { return !std::isalnum(c); }, '_');
        return name;
    });

// ============================================================================
// Specific Element Category Tests
// ============================================================================

// Arc Tests
class ArcElementsTest : public CGMParameterizedTest {};

TEST_P(ArcElementsTest, ParsesWithoutCrash) {
    std::string error;
    EXPECT_TRUE(parseFile(error));
}

TEST_P(ArcElementsTest, GeneratesValidSVG) {
    std::string svgOutput, error;
    EXPECT_TRUE(convertToSVG(svgOutput, error));
}

std::vector<std::string> GetArcTestFiles() {
    auto samplesPath = CGMTestFileCollector::getSamplesPath();
    if (samplesPath.empty()) return {};
    return CGMTestFileCollector::collectFromDirectory(
        samplesPath / "webcgm21-ts" / "static10", -1, ".*ARC.*|.*CRAR.*|.*ELAR.*");
}

INSTANTIATE_TEST_SUITE_P(
    ArcElements,
    ArcElementsTest,
    ::testing::ValuesIn(GetArcTestFiles()),
    [](const ::testing::TestParamInfo<std::string>& info) {
        std::string name = fs::path(info.param).stem().string();
        std::replace_if(name.begin(), name.end(),
            [](char c) { return !std::isalnum(c); }, '_');
        return name;
    });

// Text Tests
class TextElementsTest : public CGMParameterizedTest {};

TEST_P(TextElementsTest, ParsesWithoutCrash) {
    std::string error;
    EXPECT_TRUE(parseFile(error));
}

TEST_P(TextElementsTest, GeneratesValidSVG) {
    std::string svgOutput, error;
    EXPECT_TRUE(convertToSVG(svgOutput, error));
}

std::vector<std::string> GetTextTestFiles() {
    auto samplesPath = CGMTestFileCollector::getSamplesPath();
    if (samplesPath.empty()) return {};
    return CGMTestFileCollector::collectFromDirectory(
        samplesPath / "webcgm21-ts" / "static10", -1, ".*TXT.*|.*CHR.*|.*FNT.*");
}

INSTANTIATE_TEST_SUITE_P(
    TextElements,
    TextElementsTest,
    ::testing::ValuesIn(GetTextTestFiles()),
    [](const ::testing::TestParamInfo<std::string>& info) {
        std::string name = fs::path(info.param).stem().string();
        std::replace_if(name.begin(), name.end(),
            [](char c) { return !std::isalnum(c); }, '_');
        return name;
    });

// Polygon Tests
class PolygonElementsTest : public CGMParameterizedTest {};

TEST_P(PolygonElementsTest, ParsesWithoutCrash) {
    std::string error;
    EXPECT_TRUE(parseFile(error));
}

TEST_P(PolygonElementsTest, GeneratesValidSVG) {
    std::string svgOutput, error;
    EXPECT_TRUE(convertToSVG(svgOutput, error));
}

std::vector<std::string> GetPolygonTestFiles() {
    auto samplesPath = CGMTestFileCollector::getSamplesPath();
    if (samplesPath.empty()) return {};
    return CGMTestFileCollector::collectFromDirectory(
        samplesPath / "webcgm21-ts" / "static10", -1, ".*POL.*|.*PLG.*");
}

INSTANTIATE_TEST_SUITE_P(
    PolygonElements,
    PolygonElementsTest,
    ::testing::ValuesIn(GetPolygonTestFiles()),
    [](const ::testing::TestParamInfo<std::string>& info) {
        std::string name = fs::path(info.param).stem().string();
        std::replace_if(name.begin(), name.end(),
            [](char c) { return !std::isalnum(c); }, '_');
        return name;
    });

// ============================================================================
// Performance/Stress Tests
// ============================================================================

TEST(ParameterizedSuiteInfo, ReportTestCounts) {
    auto samplesPath = CGMTestFileCollector::getSamplesPath();
    if (samplesPath.empty()) {
        std::cout << "Samples directory not found\n";
        return;
    }

    std::cout << "=== CGM Test File Counts ===\n";
    std::cout << "WebCGM 2.0 (20tests): " << GetWebCGM20Files().size() << " files\n";
    std::cout << "WebCGM 2.1 (21tests): " << GetWebCGM21Files().size() << " files\n";
    std::cout << "Static 1.0: " << GetStatic10Files().size() << " files\n";
    std::cout << "S1000D: " << GetS1000DFiles().size() << " files\n";
    std::cout << "NIST: " << GetNISTFiles().size() << " files\n";
    std::cout << "Arc elements: " << GetArcTestFiles().size() << " files\n";
    std::cout << "Text elements: " << GetTextTestFiles().size() << " files\n";
    std::cout << "Polygon elements: " << GetPolygonTestFiles().size() << " files\n";
}

// These suites are parameterised over a sample corpus that this repository
// does not vendor (see scripts/fetch-testdata.py). With no corpus the
// generators yield no values, and GoogleTest treats an uninstantiated
// parameterised suite as a failure unless told the case is expected --
// matching how NISTTest is handled above.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(WebCGM20Test);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(WebCGM21Test);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Static10Test);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(S1000DTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ArcElementsTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TextElementsTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(PolygonElementsTest);
