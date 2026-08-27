#include <gtest/gtest.h>

#include "opencgm/c_api.h"
#include "../third_party/nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

struct ArtifactFixture
{
    std::string name;
    std::string corpus;
    std::string relativeInputPath;
    std::string relativeInputDirectory;
    std::string filePattern;
    std::string expectedDetectionProfile;
    std::vector<std::string> expectedSvgFragments;
    std::vector<std::string> expectedReportTextFragments;
    json minimumSummary = json::object();
    json expectedOutput = json::object();
};

std::string sanitizeTestName(const std::string& value)
{
    std::string sanitized = value;
    std::replace_if(sanitized.begin(), sanitized.end(),
        [](char ch) { return !std::isalnum(static_cast<unsigned char>(ch)); }, '_');
    return sanitized.empty() ? "Fixture" : sanitized;
}

fs::path getSamplesPath()
{
    // Allow the corpus location to be supplied explicitly. The public
    // repository does not vendor sample data; scripts/fetch-testdata.py
    // downloads it and points this variable at the result.
    if (const char* envDir = std::getenv("OPENCGM_SAMPLES_DIR"))
    {
        if (fs::exists(envDir))
        {
            return fs::absolute(envDir);
        }
    }

    const std::vector<fs::path> possiblePaths = {
        fs::path("../../../samples/"),
        fs::path("../samples/"),
        fs::path("../../samples/"),
        fs::path("samples/"),
    };

    for (const auto& path : possiblePaths)
    {
        if (fs::exists(path))
        {
            return fs::absolute(path);
        }
    }

    const fs::path sourceFile(__FILE__);
    const auto testDir = sourceFile.parent_path();
    const auto samplesFromSource = testDir.parent_path() / "samples";
    if (fs::exists(samplesFromSource))
    {
        return samplesFromSource;
    }

    return {};
}

fs::path getFixtureManifestPath()
{
    return fs::path(__FILE__).parent_path() / "golden" / "conversion_cases.json";
}

std::vector<std::string> readStringArray(const json& node, const char* key)
{
    std::vector<std::string> values;
    if (!node.contains(key) || !node.at(key).is_array())
    {
        return values;
    }

    for (const auto& entry : node.at(key))
    {
        if (entry.is_string())
        {
            values.push_back(entry.get<std::string>());
        }
    }

    return values;
}

std::vector<ArtifactFixture> loadFixtures()
{
    const auto manifestPath = getFixtureManifestPath();
    if (!fs::exists(manifestPath))
    {
        return {};
    }

    std::ifstream stream(manifestPath);
    if (!stream)
    {
        return {};
    }

    json manifest;
    stream >> manifest;

    std::vector<ArtifactFixture> fixtures;
    if (!manifest.contains("cases") || !manifest.at("cases").is_array())
    {
        return fixtures;
    }

    for (const auto& entry : manifest.at("cases"))
    {
        ArtifactFixture fixture;
        fixture.name = entry.value("name", "");
        fixture.corpus = entry.value("corpus", "");
        fixture.relativeInputPath = entry.value("relativeInputPath", "");
        fixture.relativeInputDirectory = entry.value("relativeInputDirectory", "");
        fixture.filePattern = entry.value("filePattern", "");
        fixture.expectedDetectionProfile = entry.value("expectedDetectionProfile", "");
        fixture.expectedSvgFragments = readStringArray(entry, "expectedSvgFragments");
        fixture.expectedReportTextFragments = readStringArray(entry, "expectedReportTextFragments");
        fixture.minimumSummary = entry.value("minimumSummary", json::object());
        fixture.expectedOutput = entry.value("expectedOutput", json::object());

        if (!fixture.name.empty())
        {
            fixtures.push_back(std::move(fixture));
        }
    }

    return fixtures;
}

fs::path resolveFixtureInputPath(const ArtifactFixture& fixture, const fs::path& samplesPath)
{
    if (!fixture.relativeInputPath.empty())
    {
        return samplesPath / fixture.relativeInputPath;
    }

    if (fixture.relativeInputDirectory.empty() || fixture.filePattern.empty())
    {
        return {};
    }

    const auto directory = samplesPath / fixture.relativeInputDirectory;
    if (!fs::exists(directory))
    {
        return {};
    }

    const std::regex pattern(fixture.filePattern, std::regex::icase);
    std::vector<fs::path> candidates;
    for (const auto& entry : fs::directory_iterator(directory))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        if (std::regex_match(entry.path().filename().string(), pattern))
        {
            candidates.push_back(entry.path());
        }
    }

    if (candidates.empty())
    {
        return {};
    }

    std::sort(candidates.begin(), candidates.end());
    return candidates.front();
}

std::string readFile(const fs::path& filePath)
{
    std::ifstream stream(filePath, std::ios::binary);
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

class OpenCGMContext
{
public:
    OpenCGMContext()
        : ctx_(opencgm_create())
    {
    }

    ~OpenCGMContext()
    {
        if (ctx_ != nullptr)
        {
            opencgm_destroy(ctx_);
        }
    }

    opencgm_ctx_t* get() const { return ctx_; }

private:
    opencgm_ctx_t* ctx_ = nullptr;
};

class ConversionArtifactFixtureTest : public ::testing::TestWithParam<ArtifactFixture>
{
};

TEST_P(ConversionArtifactFixtureTest, ProducesStableSvgAndReportArtifacts)
{
    const auto& fixture = GetParam();
    const auto samplesPath = getSamplesPath();
    if (samplesPath.empty())
    {
        GTEST_SKIP() << "Samples directory not found";
    }

    const auto inputPath = resolveFixtureInputPath(fixture, samplesPath);
    if (inputPath.empty() || !fs::exists(inputPath))
    {
        GTEST_SKIP() << "Fixture input not found: " << fixture.name;
    }

    OpenCGMContext ctx;
    ASSERT_NE(ctx.get(), nullptr);

    opencgm_set_profile(ctx.get(), "compat");
    opencgm_set_minify(ctx.get(), 1);
    opencgm_set_optimize_paths(ctx.get(), 1);
    opencgm_set_pretty_print(ctx.get(), 0);

    if (!fixture.expectedDetectionProfile.empty())
    {
        ASSERT_EQ(opencgm_detect_profile(ctx.get(), inputPath.string().c_str()), OPENCGM_OK)
            << opencgm_last_error();

        const char* detectionJson = opencgm_get_last_profile_detection_json(ctx.get());
        ASSERT_NE(detectionJson, nullptr);

        const auto detection = json::parse(detectionJson);
        ASSERT_TRUE(detection.contains("profile"));
        EXPECT_EQ(detection.at("profile").get<std::string>(), fixture.expectedDetectionProfile);
        ASSERT_TRUE(detection.contains("metadata"));
        EXPECT_GE(detection.at("metadata").value("pictureCount", 0), 1);
    }

    const auto tempDir = fs::temp_directory_path() / "opencgm-artifact-tests";
    fs::create_directories(tempDir);
    const auto outputPath = tempDir / (sanitizeTestName(fixture.name) + ".svg");
    std::error_code ignored;
    fs::remove(outputPath, ignored);

    ASSERT_EQ(opencgm_convert_cgm_to_svg(ctx.get(), inputPath.string().c_str(), outputPath.string().c_str()), OPENCGM_OK)
        << opencgm_last_error();
    ASSERT_TRUE(fs::exists(outputPath));

    const auto svgOutput = readFile(outputPath);
    ASSERT_FALSE(svgOutput.empty());
    for (const auto& fragment : fixture.expectedSvgFragments)
    {
        EXPECT_NE(svgOutput.find(fragment), std::string::npos)
            << "Missing SVG fragment '" << fragment << "' for fixture " << fixture.name;
    }

    const char* reportJson = opencgm_get_last_report_json(ctx.get());
    ASSERT_NE(reportJson, nullptr);

    const auto report = json::parse(reportJson);
    EXPECT_EQ(report.value("schemaVersion", 0), 1);
    ASSERT_TRUE(report.contains("output"));
    EXPECT_TRUE(report.at("output").value("success", false));

    if (fixture.expectedOutput.contains("minified"))
    {
        EXPECT_EQ(report.at("output").value("minified", false),
            fixture.expectedOutput.at("minified").get<bool>());
    }

    if (fixture.expectedOutput.contains("optimizedPaths"))
    {
        EXPECT_EQ(report.at("output").value("optimizedPaths", false),
            fixture.expectedOutput.at("optimizedPaths").get<bool>());
    }

    if (!fixture.expectedDetectionProfile.empty())
    {
        ASSERT_TRUE(report.contains("profile"));
        EXPECT_EQ(report.at("profile").value("detected", std::string()), fixture.expectedDetectionProfile);
    }

    ASSERT_TRUE(report.contains("summary"));
    for (auto it = fixture.minimumSummary.begin(); it != fixture.minimumSummary.end(); ++it)
    {
        ASSERT_TRUE(report.at("summary").contains(it.key()))
            << "Missing summary field '" << it.key() << "'";
        EXPECT_GE(report.at("summary").at(it.key()).get<int>(), it.value().get<int>())
            << "Summary field '" << it.key() << "' below expected minimum for " << fixture.name;
    }

    const char* reportText = opencgm_get_last_report_text(ctx.get());
    ASSERT_NE(reportText, nullptr);
    const std::string reportTextValue(reportText);
    for (const auto& fragment : fixture.expectedReportTextFragments)
    {
        EXPECT_NE(reportTextValue.find(fragment), std::string::npos)
            << "Missing report text fragment '" << fragment << "' for fixture " << fixture.name;
    }
}

INSTANTIATE_TEST_SUITE_P(
    GoldenConversionArtifacts,
    ConversionArtifactFixtureTest,
    ::testing::ValuesIn(loadFixtures()),
    [](const ::testing::TestParamInfo<ArtifactFixture>& info) {
        return sanitizeTestName(info.param.name);
    });

} // namespace
