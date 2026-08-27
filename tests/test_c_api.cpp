#include <gtest/gtest.h>
#include "opencgm/c_api.h"
#include "opencgm/profile_presets_embedded.h"
#include "../third_party/nlohmann/json.hpp"
#include "opencgm/cgm_c_api.h"
#include "opencgm/cgm_file.h"
#include "opencgm/command_factory.h"
#include "opencgm/profile_validator.h"
#include "opencgm/commands/attribute_commands.h"
#include "opencgm/commands/delimiter_commands.h"
#include "opencgm/commands/metafile_descriptor_commands.h"
#include "opencgm/commands/picture_descriptor_commands.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <vector>

namespace {

std::filesystem::path createMetadataCgm(
    int pictureCount = 1,
    bool includeUnknownCommand = false) {
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::current_path() /
        ("opencgm_c_api_" + std::to_string(unique) + ".cgm");

    opencgm::BinaryCGMFile file;

    auto beginMetafile = std::make_unique<opencgm::BeginMetafile>(&file);
    beginMetafile->setName("metadata-test");
    file.commands().push_back(std::move(beginMetafile));

    auto version = std::make_unique<opencgm::MetafileVersion>(&file);
    version->setVersion(3);
    file.commands().push_back(std::move(version));

    auto description = std::make_unique<opencgm::MetafileDescription>(&file);
    description->setDescription("ProfileId:S1000D_6.0; Source:OpenCGMTest");
    file.commands().push_back(std::move(description));

    for (int index = 0; index < pictureCount; ++index) {
        auto beginPicture = std::make_unique<opencgm::BeginPicture>(&file);
        beginPicture->setName("picture-" + std::to_string(index + 1));
        file.commands().push_back(std::move(beginPicture));

        auto extent = std::make_unique<opencgm::VDCExtent>(&file);
        extent->setExtent(opencgm::CGMPoint(10.0, 20.0), opencgm::CGMPoint(1010.0, 2020.0));
        file.commands().push_back(std::move(extent));

        file.commands().push_back(std::make_unique<opencgm::BeginPictureBody>(&file));
        if (includeUnknownCommand && index == 0) {
            file.commands().push_back(
                std::make_unique<opencgm::UnknownCommand>(99, 4, &file));
        }
        file.commands().push_back(std::make_unique<opencgm::EndPicture>(&file));
    }

    file.commands().push_back(std::make_unique<opencgm::EndMetafile>(&file));
    file.writeFile(path.string());
    return path;
}

std::string getAtaColorClassSummary(opencgm::ColorSelectionMode mode) {
    opencgm::BinaryCGMFile file;
    file.setColorSelectionMode(mode);

    auto maxIndex = std::make_unique<opencgm::MaximumColourIndex>(&file);
    maxIndex->setMaxIndex(255);
    file.commands().push_back(std::move(maxIndex));
    file.commands().push_back(std::make_unique<opencgm::ColourTable>(&file));

    opencgm::ATAGREXCHANGEValidator validator(
        opencgm::ATAGREXCHANGEValidator::Version::V2_9);
    const auto messages = validator.validate(&file);
    const auto summary = std::find_if(
        messages.begin(),
        messages.end(),
        [](const opencgm::ValidationMessage& message) {
            return message.message.rfind("Color class check:", 0) == 0;
        });

    return summary == messages.end() ? std::string() : summary->message;
}

} // namespace

// ============================================================================
// C API Tests - OpenCGM Context Management
// ============================================================================

class OpenCGMApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx = opencgm_create();
    }

    void TearDown() override {
        if (ctx) {
            opencgm_destroy(ctx);
        }
    }

    opencgm_ctx_t* ctx = nullptr;
};

// ============================================================================
// Context Creation/Destruction Tests
// ============================================================================

TEST_F(OpenCGMApiTest, CreateContext) {
    EXPECT_NE(ctx, nullptr);
}

TEST_F(OpenCGMApiTest, DestroyNullContextDoesNotCrash) {
    EXPECT_NO_THROW({
        opencgm_destroy(nullptr);
    });
}

TEST_F(OpenCGMApiTest, CreateMultipleContexts) {
    auto ctx2 = opencgm_create();
    auto ctx3 = opencgm_create();

    EXPECT_NE(ctx2, nullptr);
    EXPECT_NE(ctx3, nullptr);
    EXPECT_NE(ctx, ctx2);
    EXPECT_NE(ctx2, ctx3);

    opencgm_destroy(ctx2);
    opencgm_destroy(ctx3);
}

// ============================================================================
// Y-Flip Mode Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetYFlipModeAuto) {
    EXPECT_NO_THROW({
        opencgm_set_yflip_mode(ctx, OPENCGM_YFLIP_AUTO);
    });
}

TEST_F(OpenCGMApiTest, SetYFlipModeOn) {
    EXPECT_NO_THROW({
        opencgm_set_yflip_mode(ctx, OPENCGM_YFLIP_FORCE_ON);
    });
}

TEST_F(OpenCGMApiTest, SetYFlipModeOff) {
    EXPECT_NO_THROW({
        opencgm_set_yflip_mode(ctx, OPENCGM_YFLIP_FORCE_OFF);
    });
}

TEST_F(OpenCGMApiTest, SetYFlipModeNullContext) {
    EXPECT_NO_THROW({
        opencgm_set_yflip_mode(nullptr, OPENCGM_YFLIP_AUTO);
    });
}

// ============================================================================
// Fit to Content Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetFitToContentTrue) {
    EXPECT_NO_THROW({
        opencgm_set_fit_to_content(ctx, 1);
    });
}

TEST_F(OpenCGMApiTest, SetFitToContentFalse) {
    EXPECT_NO_THROW({
        opencgm_set_fit_to_content(ctx, 0);
    });
}

// ============================================================================
// Scale Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetScalePositive) {
    EXPECT_NO_THROW({
        opencgm_set_scale(ctx, 2.0);
    });
}

TEST_F(OpenCGMApiTest, SetScaleOne) {
    EXPECT_NO_THROW({
        opencgm_set_scale(ctx, 1.0);
    });
}

TEST_F(OpenCGMApiTest, SetScaleFractional) {
    EXPECT_NO_THROW({
        opencgm_set_scale(ctx, 0.5);
    });
}

// ============================================================================
// DPI Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetDpiStandard) {
    EXPECT_NO_THROW({
        opencgm_set_dpi(ctx, 96);
    });
}

TEST_F(OpenCGMApiTest, SetDpiHigh) {
    EXPECT_NO_THROW({
        opencgm_set_dpi(ctx, 300);
    });
}

// ============================================================================
// Verbose/Quiet Mode Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetVerboseTrue) {
    EXPECT_NO_THROW({
        opencgm_set_verbose(ctx, 1);
    });
}

TEST_F(OpenCGMApiTest, SetVerboseFalse) {
    EXPECT_NO_THROW({
        opencgm_set_verbose(ctx, 0);
    });
}

TEST_F(OpenCGMApiTest, SetQuietTrue) {
    EXPECT_NO_THROW({
        opencgm_set_quiet(ctx, 1);
    });
}

TEST_F(OpenCGMApiTest, SetQuietFalse) {
    EXPECT_NO_THROW({
        opencgm_set_quiet(ctx, 0);
    });
}

// ============================================================================
// Style Mode Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetStyleModeInline) {
    EXPECT_NO_THROW({
        opencgm_set_style_mode(ctx, 0);
    });
}

TEST_F(OpenCGMApiTest, SetStyleModeCss) {
    EXPECT_NO_THROW({
        opencgm_set_style_mode(ctx, 1);
    });
}

// ============================================================================
// Minify/Pretty Print Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetMinifyTrue) {
    EXPECT_NO_THROW({
        opencgm_set_minify(ctx, 1);
    });
}

TEST_F(OpenCGMApiTest, SetPrettyPrintTrue) {
    EXPECT_NO_THROW({
        opencgm_set_pretty_print(ctx, 1);
    });
}

// ============================================================================
// Precision Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetPrecision) {
    EXPECT_NO_THROW({
        opencgm_set_precision(ctx, 4.0);
    });
}

// ============================================================================
// Profile Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetProfileS1000D) {
    EXPECT_NO_THROW({
        opencgm_set_profile(ctx, "s1000d");
    });
}

TEST_F(OpenCGMApiTest, SetProfileWebCGM) {
    EXPECT_NO_THROW({
        opencgm_set_profile(ctx, "webcgm");
    });
}

TEST_F(OpenCGMApiTest, SetProfileCompat) {
    EXPECT_NO_THROW({
        opencgm_set_profile(ctx, "compat");
    });
}

TEST_F(OpenCGMApiTest, SetProfileNullProfile) {
    EXPECT_NO_THROW({
        opencgm_set_profile(ctx, nullptr);
    });
}

// ============================================================================
// Text Rendering Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetTextAsPathTrue) {
    EXPECT_NO_THROW({
        opencgm_set_text_as_path(ctx, 1);
    });
}

TEST_F(OpenCGMApiTest, SetTextPathThreshold) {
    EXPECT_NO_THROW({
        opencgm_set_text_path_threshold(ctx, 12.0);
    });
}

TEST_F(OpenCGMApiTest, SetTextPathThresholdNegative) {
    // Negative threshold should be clamped to 0
    EXPECT_NO_THROW({
        opencgm_set_text_path_threshold(ctx, -5.0);
    });
}

// ============================================================================
// Logging Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetRasterLogging) {
    EXPECT_NO_THROW({
        opencgm_set_raster_logging(ctx, 1);
    });
}

TEST_F(OpenCGMApiTest, SetGeometryLogging) {
    EXPECT_NO_THROW({
        opencgm_set_geometry_logging(ctx, 1);
    });
}

TEST_F(OpenCGMApiTest, SetColorLogging) {
    EXPECT_NO_THROW({
        opencgm_set_color_logging(ctx, 1);
    });
}

TEST_F(OpenCGMApiTest, SetWidthLogging) {
    EXPECT_NO_THROW({
        opencgm_set_width_logging(ctx, 1);
    });
}

// ============================================================================
// Geometry Validation Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetGeometryValidation) {
    EXPECT_NO_THROW({
        opencgm_set_geometry_validation(ctx, 1);
    });
}

TEST_F(OpenCGMApiTest, SetGeometryTolerance) {
    EXPECT_NO_THROW({
        opencgm_set_geometry_tolerance(ctx, 0.001);
    });
}

TEST_F(OpenCGMApiTest, SetGeometryToleranceNegative) {
    // Negative tolerance should be set to default
    EXPECT_NO_THROW({
        opencgm_set_geometry_tolerance(ctx, -0.1);
    });
}

// ============================================================================
// Shim Mode Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetShimModeAuto) {
    EXPECT_NO_THROW({
        opencgm_set_embed_shim(ctx, OPENCGM_SHIM_AUTO);
    });
}

TEST_F(OpenCGMApiTest, SetShimModeOn) {
    EXPECT_NO_THROW({
        opencgm_set_embed_shim(ctx, OPENCGM_SHIM_ON);
    });
}

TEST_F(OpenCGMApiTest, SetShimModeOff) {
    EXPECT_NO_THROW({
        opencgm_set_embed_shim(ctx, OPENCGM_SHIM_OFF);
    });
}

TEST_F(OpenCGMApiTest, SetShimUrl) {
    EXPECT_NO_THROW({
        opencgm_set_shim_url(ctx, "https://example.com/shim.js");
    });
}

TEST_F(OpenCGMApiTest, SetShimUrlNull) {
    EXPECT_NO_THROW({
        opencgm_set_shim_url(ctx, nullptr);
    });
}

// ============================================================================
// Hotspot Profile Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetHotspotProfileGeneric) {
    EXPECT_NO_THROW({
        opencgm_set_hotspot_profile(ctx, 0);
    });
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(OpenCGMApiTest, LastErrorInitiallyEmpty) {
    const char* error = opencgm_last_error();
    // Should be empty or null initially
    EXPECT_TRUE(error == nullptr || strlen(error) == 0);
}

// ============================================================================
// Convert with Invalid Input Tests
// ============================================================================

TEST_F(OpenCGMApiTest, ConvertNullContext) {
    int result = opencgm_convert_cgm_to_svg(nullptr, "input.cgm", "output.svg");
    EXPECT_EQ(result, OPENCGM_ERR_INVALID_ARG);
}

TEST_F(OpenCGMApiTest, ConvertNullInputPath) {
    int result = opencgm_convert_cgm_to_svg(ctx, nullptr, "output.svg");
    EXPECT_EQ(result, OPENCGM_ERR_INVALID_ARG);
}

TEST_F(OpenCGMApiTest, ConvertNullOutputPath) {
    int result = opencgm_convert_cgm_to_svg(ctx, "input.cgm", nullptr);
    EXPECT_EQ(result, OPENCGM_ERR_INVALID_ARG);
}

TEST_F(OpenCGMApiTest, ConvertNonExistentFile) {
    int result = opencgm_convert_cgm_to_svg(ctx, "nonexistent_file_12345.cgm", "output.svg");
    // Should fail with parse error (file not found)
    EXPECT_NE(result, OPENCGM_OK);
}

// ============================================================================
// Trace CGM Tests
// ============================================================================

TEST_F(OpenCGMApiTest, TraceNullContext) {
    int result = opencgm_trace_cgm(nullptr, "input.cgm", "trace.json");
    EXPECT_EQ(result, OPENCGM_ERR_INVALID_ARG);
}

TEST_F(OpenCGMApiTest, TraceNullInputPath) {
    int result = opencgm_trace_cgm(ctx, nullptr, "trace.json");
    EXPECT_EQ(result, OPENCGM_ERR_INVALID_ARG);
}

TEST_F(OpenCGMApiTest, TraceNullOutputPath) {
    int result = opencgm_trace_cgm(ctx, "input.cgm", nullptr);
    EXPECT_EQ(result, OPENCGM_ERR_INVALID_ARG);
}

// ============================================================================
// QA Function Tests
// ============================================================================

TEST_F(OpenCGMApiTest, RunQANullContext) {
    int result = opencgm_run_builtin_qa(nullptr, "file.svg");
    EXPECT_EQ(result, -1);  // Error indicator
}

TEST_F(OpenCGMApiTest, RunQANullPath) {
    int result = opencgm_run_builtin_qa(ctx, nullptr);
    EXPECT_EQ(result, -1);  // Error indicator
}

TEST_F(OpenCGMApiTest, RunQANonexistentFile) {
    // Non-existent file should return -1 (error)
    int result = opencgm_run_builtin_qa(ctx, "nonexistent_test_file.svg");
    EXPECT_EQ(result, -1);
}

// ============================================================================
// Font Map Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetFontMapNullPath) {
    int result = opencgm_set_font_map(ctx, nullptr);
    EXPECT_EQ(result, OPENCGM_ERR_INVALID_ARG);
}

TEST_F(OpenCGMApiTest, SetFontMapNonExistent) {
    int result = opencgm_set_font_map(ctx, "nonexistent_font_map.json");
    EXPECT_EQ(result, OPENCGM_ERR_IO);
}

TEST_F(OpenCGMApiTest, SetFontMapAbsolutePathNonExistent) {
    auto missingPath = (std::filesystem::temp_directory_path() / "opencgm_missing_font_map_qa.json").string();
    int result = opencgm_set_font_map(ctx, missingPath.c_str());
    EXPECT_EQ(result, OPENCGM_ERR_IO);
}

// ============================================================================
// Log File Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetLogFileNullPath) {
    int result = opencgm_set_log_file(ctx, nullptr);
    EXPECT_EQ(result, OPENCGM_ERR_INVALID_ARG);
}

// ============================================================================
// Output Format Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetOutputFormatLegacy) {
    int result = opencgm_set_output_format(ctx, "legacy");
    EXPECT_EQ(result, OPENCGM_OK);
}

TEST_F(OpenCGMApiTest, SetOutputFormatS1000D6) {
    int result = opencgm_set_output_format(ctx, "s1000d6");
    EXPECT_EQ(result, OPENCGM_OK);
}

TEST_F(OpenCGMApiTest, SetOutputFormatCombined) {
    int result = opencgm_set_output_format(ctx, "combined");
    EXPECT_EQ(result, OPENCGM_OK);
}

TEST_F(OpenCGMApiTest, SetOutputFormatInvalid) {
    int result = opencgm_set_output_format(ctx, "not-a-format");
    EXPECT_EQ(result, OPENCGM_ERR_INVALID_ARG);
}

// ============================================================================
// Write Report Tests
// ============================================================================

TEST_F(OpenCGMApiTest, WriteReportNullContext) {
    int result = opencgm_write_report(nullptr, "report.json");
    EXPECT_EQ(result, OPENCGM_ERR_INVALID_ARG);
}

TEST_F(OpenCGMApiTest, WriteReportNullPath) {
    int result = opencgm_write_report(ctx, nullptr);
    EXPECT_EQ(result, OPENCGM_ERR_INVALID_ARG);
}

// ============================================================================
// Combined Settings Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetMultipleOptions) {
    // Set multiple options in sequence
    opencgm_set_yflip_mode(ctx, OPENCGM_YFLIP_FORCE_ON);
    opencgm_set_fit_to_content(ctx, 1);
    opencgm_set_scale(ctx, 1.5);
    opencgm_set_dpi(ctx, 150);
    opencgm_set_verbose(ctx, 1);
    opencgm_set_profile(ctx, "webcgm");
    opencgm_set_geometry_validation(ctx, 1);
    opencgm_set_geometry_tolerance(ctx, 0.005);

    // Should not crash and context should still be valid
    EXPECT_NE(ctx, nullptr);
}

// ============================================================================
// Custom Attribute Tests
// ============================================================================

TEST_F(OpenCGMApiTest, AddCustomAttribute) {
    EXPECT_NO_THROW({
        opencgm_add_custom_attribute(ctx, "data-id", "test-value", 0, "*");
    });
}

TEST_F(OpenCGMApiTest, AddCustomAttributeNullKey) {
    // Should not crash with null key
    EXPECT_NO_THROW({
        opencgm_add_custom_attribute(ctx, nullptr, "value", 0, "*");
    });
}

TEST_F(OpenCGMApiTest, AddCustomAttributeNullValue) {
    // Should not crash with null value
    EXPECT_NO_THROW({
        opencgm_add_custom_attribute(ctx, "key", nullptr, 0, "*");
    });
}

// ============================================================================
// TCC (Transparent Cell Colour) Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetTccEnabled) {
    EXPECT_NO_THROW({
        opencgm_set_tcc_enabled(ctx, 1);
    });
}

TEST_F(OpenCGMApiTest, SetTccDisabled) {
    EXPECT_NO_THROW({
        opencgm_set_tcc_enabled(ctx, 0);
    });
}

// ============================================================================
// PNG Quantization Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetQuantizePng) {
    EXPECT_NO_THROW({
        opencgm_set_quantize_png(ctx, 1);
    });
}

// ============================================================================
// ViewBox Padding Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetViewboxPadding) {
    EXPECT_NO_THROW({
        opencgm_set_viewbox_padding(ctx, 0.05);
    });
}

// ============================================================================
// Palette Override Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetPaletteOverrideModeNone) {
    EXPECT_NO_THROW({
        opencgm_set_palette_override_mode(ctx, 0);
    });
}

TEST_F(OpenCGMApiTest, SetPaletteOverrideModeMonochrome) {
    EXPECT_NO_THROW({
        opencgm_set_palette_override_mode(ctx, 1);
    });
}

// ============================================================================
// Adopt View On Load Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetAdoptViewOnLoadTrue) {
    EXPECT_NO_THROW({
        opencgm_set_adopt_view_on_load(ctx, 1);
    });
}

TEST_F(OpenCGMApiTest, SetAdoptViewOnLoadFalse) {
    EXPECT_NO_THROW({
        opencgm_set_adopt_view_on_load(ctx, 0);
    });
}

// ============================================================================
// Trace Unknown Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetTraceUnknownTrue) {
    EXPECT_NO_THROW({
        opencgm_set_trace_unknown(ctx, 1);
    });
}

TEST_F(OpenCGMApiTest, TraceUnknownAddsReportWarningAndStrictModeRejectsOutput) {
    const auto cgmPath = createMetadataCgm(1, true);
    auto svgPath = cgmPath;
    svgPath.replace_extension(".svg");

    opencgm_set_trace_unknown(ctx, 1);
    ASSERT_EQ(
        opencgm_convert_cgm_to_svg(
            ctx,
            cgmPath.string().c_str(),
            svgPath.string().c_str()),
        OPENCGM_OK);
    EXPECT_TRUE(std::filesystem::exists(svgPath));

    const std::string warningReport = opencgm_get_last_report_json(ctx);
    EXPECT_NE(warningReport.find("unknown-command"), std::string::npos);
    EXPECT_NE(
        warningReport.find("Unsupported CGM command class 4, element 99"),
        std::string::npos);

    std::filesystem::remove(svgPath);

    opencgm_set_fail_on_warn(ctx, 1);
    EXPECT_EQ(
        opencgm_convert_cgm_to_svg(
            ctx,
            cgmPath.string().c_str(),
            svgPath.string().c_str()),
        OPENCGM_ERR_GENERAL);
    EXPECT_FALSE(std::filesystem::exists(svgPath));

    const std::string failureReport = opencgm_get_last_report_json(ctx);
    EXPECT_NE(failureReport.find("unknown-command"), std::string::npos);

    std::filesystem::remove(cgmPath);
}

// ============================================================================
// Fail On Warn Tests
// ============================================================================

TEST_F(OpenCGMApiTest, SetFailOnWarnTrue) {
    EXPECT_NO_THROW({
        opencgm_set_fail_on_warn(ctx, 1);
    });
}

// ============================================================================
// End-to-end metadata, XCF, and multi-picture regression tests
// ============================================================================

TEST(LegacyCgmApiTest, MetadataGettersReturnParsedCommands) {
    const auto cgmPath = createMetadataCgm();
    CGMErrorCode error = CGM_SUCCESS;
    CGMFileHandle file = cgm_load_file(cgmPath.string().c_str(), &error);
    ASSERT_NE(file, nullptr);
    ASSERT_EQ(error, CGM_SUCCESS);

    EXPECT_EQ(cgm_get_version(file), 3);

    char description[128] = {};
    EXPECT_GT(cgm_get_description(file, description, sizeof(description)), 0);
    EXPECT_STREQ(description, "ProfileId:S1000D_6.0; Source:OpenCGMTest");

    CGMRect_t extent = {};
    EXPECT_EQ(cgm_get_vdc_extent(file, &extent), CGM_SUCCESS);
    EXPECT_DOUBLE_EQ(extent.x1, 10.0);
    EXPECT_DOUBLE_EQ(extent.y1, 20.0);
    EXPECT_DOUBLE_EQ(extent.x2, 1010.0);
    EXPECT_DOUBLE_EQ(extent.y2, 2020.0);
    EXPECT_TRUE(cgm_uses_indexed_colors(file));

    cgm_free_file(file);
    std::filesystem::remove(cgmPath);
}

TEST(LegacyCgmApiTest, SvgOptionsControlDocumentMetadataAndBackground) {
    const auto cgmPath = createMetadataCgm();
    CGMErrorCode error = CGM_SUCCESS;
    CGMFileHandle file = cgm_load_file(cgmPath.string().c_str(), &error);
    ASSERT_NE(file, nullptr);

    SVGOptions_t options = {
        true,
        true,
        false,
        1.0,
        "#123",
        false,
        nullptr
    };
    SVGConverterHandle converter =
        cgm_create_svg_converter_with_options(file, &options, &error);
    ASSERT_NE(converter, nullptr);
    ASSERT_EQ(error, CGM_SUCCESS);

    const int required = cgm_convert_to_svg(converter, nullptr, 0);
    ASSERT_GT(required, 1);
    std::vector<char> buffer(static_cast<size_t>(required));
    EXPECT_EQ(
        cgm_convert_to_svg(converter, buffer.data(), buffer.size()),
        required);

    const std::string svg(buffer.data());
    EXPECT_EQ(svg.find("Converted by OpenCGM"), std::string::npos);
    EXPECT_EQ(svg.find("<title>"), std::string::npos);
    EXPECT_NE(svg.find("fill=\"#112233\""), std::string::npos);
    EXPECT_NE(svg.find("pointer-events=\"none\""), std::string::npos);

    cgm_free_svg_converter(converter);

    SVGConverterHandle defaultConverter =
        cgm_create_svg_converter(file, &error);
    ASSERT_NE(defaultConverter, nullptr);
    const int defaultRequired =
        cgm_convert_to_svg(defaultConverter, nullptr, 0);
    ASSERT_GT(defaultRequired, 1);
    std::vector<char> defaultBuffer(
        static_cast<size_t>(defaultRequired));
    cgm_convert_to_svg(
        defaultConverter,
        defaultBuffer.data(),
        defaultBuffer.size());
    const std::string defaultSvg(defaultBuffer.data());
    EXPECT_NE(defaultSvg.find("Converted by OpenCGM"), std::string::npos);
    EXPECT_NE(defaultSvg.find("fill=\"#FFFFFF\""), std::string::npos);

    cgm_free_svg_converter(defaultConverter);
    cgm_free_file(file);
    std::filesystem::remove(cgmPath);
}

TEST(LegacyCgmApiTest, SvgOptionsRejectInvalidBackgroundColor) {
    const auto cgmPath = createMetadataCgm();
    CGMErrorCode error = CGM_SUCCESS;
    CGMFileHandle file = cgm_load_file(cgmPath.string().c_str(), &error);
    ASSERT_NE(file, nullptr);

    SVGOptions_t options = {
        true,
        true,
        true,
        1.0,
        "not-a-colour",
        false,
        nullptr
    };
    EXPECT_EQ(
        cgm_create_svg_converter_with_options(file, &options, &error),
        nullptr);
    EXPECT_EQ(error, CGM_ERROR_INVALID_PARAMETER);

    cgm_free_file(file);
    std::filesystem::remove(cgmPath);
}

TEST_F(OpenCGMApiTest, ConversionGeneratesRequestedXcfCompanion) {
    const auto cgmPath = createMetadataCgm();
    auto svgPath = cgmPath;
    svgPath.replace_extension(".svg");
    auto xcfPath = cgmPath;
    xcfPath.replace_extension(".xcf");

    opencgm_set_generate_xcf(ctx, 1);
    ASSERT_EQ(opencgm_convert_cgm_to_svg(ctx, cgmPath.string().c_str(), svgPath.string().c_str()), OPENCGM_OK);
    EXPECT_TRUE(std::filesystem::exists(svgPath));
    EXPECT_TRUE(std::filesystem::exists(xcfPath));

    std::ifstream xcf(xcfPath);
    const std::string content((std::istreambuf_iterator<char>(xcf)), std::istreambuf_iterator<char>());
    xcf.close();
    EXPECT_NE(content.find("ProfileId:S1000D_6.0"), std::string::npos);
    EXPECT_NE(content.find("source=\"" + svgPath.filename().string() + "\""), std::string::npos);

    std::filesystem::remove(cgmPath);
    std::filesystem::remove(svgPath);
    std::filesystem::remove(xcfPath);
}

TEST_F(OpenCGMApiTest, CombinedModeRejectsMultiPictureInputInsteadOfReturningLastPicture) {
    const auto cgmPath = createMetadataCgm(2);
    auto svgPath = cgmPath;
    svgPath.replace_extension(".svg");

    ASSERT_EQ(opencgm_get_picture_count(ctx, cgmPath.string().c_str()), 2);
    opencgm_set_multi_picture_mode(ctx, 2);
    EXPECT_EQ(
        opencgm_convert_cgm_to_svg(ctx, cgmPath.string().c_str(), svgPath.string().c_str()),
        OPENCGM_ERR_INVALID_ARG);
    EXPECT_FALSE(std::filesystem::exists(svgPath));

    std::filesystem::remove(cgmPath);
}

TEST_F(OpenCGMApiTest, PerPictureConversionUsesSameProfileConfigurationAsMainConversion) {
    const auto cgmPath = createMetadataCgm(2);
    const std::array<const char*, 3> profiles = {
        "ata",
        "cgmplus",
        "s1000d-legacy"
    };

    for (const char* profile : profiles) {
        auto mainPath = cgmPath;
        mainPath.replace_filename(
            cgmPath.stem().string() + "_" + profile + "_main.svg");
        auto picturePath = cgmPath;
        picturePath.replace_filename(
            cgmPath.stem().string() + "_" + profile + "_picture.svg");

        opencgm_set_profile(ctx, profile);
        ASSERT_EQ(
            opencgm_convert_cgm_to_svg(
                ctx,
                cgmPath.string().c_str(),
                mainPath.string().c_str()),
            OPENCGM_OK);
        ASSERT_EQ(
            opencgm_convert_picture_to_svg(
                ctx,
                cgmPath.string().c_str(),
                picturePath.string().c_str(),
                0),
            OPENCGM_OK);

        std::ifstream mainFile(mainPath);
        std::ifstream pictureFile(picturePath);
        const std::string mainSvg(
            (std::istreambuf_iterator<char>(mainFile)),
            std::istreambuf_iterator<char>());
        const std::string pictureSvg(
            (std::istreambuf_iterator<char>(pictureFile)),
            std::istreambuf_iterator<char>());
        mainFile.close();
        pictureFile.close();

        EXPECT_EQ(mainSvg, pictureSvg) << "Profile: " << profile;

        std::filesystem::remove(mainPath);
        std::filesystem::remove(picturePath);
    }

    std::filesystem::remove(cgmPath);
}

TEST(AtaValidatorTest, ColorClassUsesSelectionModeAndRecognizesColourTableElement) {
    EXPECT_EQ(
        getAtaColorClassSummary(opencgm::ColorSelectionMode::INDEXED),
        "Color class check: Indexed color mode (max index: 255)");
    EXPECT_EQ(
        getAtaColorClassSummary(opencgm::ColorSelectionMode::DIRECT),
        "Color class check: Direct color mode (colour table present but not selected)");
}

TEST(BuiltInProfileCatalog, EmbeddedCatalogMatchesSourceJsonAndParses) {
    // The embedded fallback is generated from config/profile-presets.json at
    // build time (cmake/profile_presets_embedded.h.in); this guard fails if
    // the catalog JSON breaks or the removed no-op flag resurfaces.
    const nlohmann::json embedded = nlohmann::json::parse(
        opencgm::generated::kBuiltInProfileCatalogJson, nullptr, false);
    ASSERT_FALSE(embedded.is_discarded());
    ASSERT_TRUE(embedded.contains("profiles"));

    std::vector<std::string> overrides;
    for (const auto& profile : embedded["profiles"]) {
        ASSERT_TRUE(profile.contains("settings"));
        overrides.push_back(
            profile["settings"].value("profileOverride", std::string()));
        EXPECT_FALSE(
            profile["settings"].contains("validateOutputAgainstS1000D6"));
    }
    const auto has = [&](const char* name) {
        return std::find(overrides.begin(), overrides.end(), name) !=
               overrides.end();
    };
    EXPECT_TRUE(has("S1000Dv6"));
    EXPECT_TRUE(has("WebCgm21"));
    EXPECT_TRUE(has("AtaISpec2200"));
    EXPECT_TRUE(has("Cals"));
    EXPECT_TRUE(has("Pip"));

    // Compare against the source-of-truth file when locatable from the test
    // source tree (robust to arbitrary CWD).
    const std::filesystem::path source =
        std::filesystem::path(__FILE__).parent_path().parent_path() /
        "config" / "profile-presets.json";
    if (std::filesystem::exists(source)) {
        std::ifstream in(source, std::ios::binary);
        std::stringstream buffer;
        buffer << in.rdbuf();
        const nlohmann::json onDisk =
            nlohmann::json::parse(buffer.str(), nullptr, false);
        ASSERT_FALSE(onDisk.is_discarded());
        EXPECT_EQ(embedded, onDisk)
            << "Embedded catalog drifted from config/profile-presets.json - "
               "re-run CMake configure to regenerate.";
    }

    // The C API surface returns a parseable catalog too (file or embedded).
    opencgm_ctx_t* ctx = opencgm_create();
    ASSERT_NE(ctx, nullptr);
    const char* apiCatalog = opencgm_get_builtin_profile_catalog_json(ctx);
    ASSERT_NE(apiCatalog, nullptr);
    EXPECT_FALSE(
        nlohmann::json::parse(apiCatalog, nullptr, false).is_discarded());
    opencgm_destroy(ctx);
}

TEST(CalsProfileTest, DetectsAndValidatesMilD28003Sources) {
    // Synthetic CALS fixture: legacy DoD archives self-identify in the
    // metafile description via strings like "MIL-D-28003A/BASIC-1.2".
    const auto unique =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::current_path() /
        ("opencgm_cals_" + std::to_string(unique) + ".cgm");

    {
        opencgm::BinaryCGMFile file;
        auto beginMetafile = std::make_unique<opencgm::BeginMetafile>(&file);
        beginMetafile->setName("cals-test");
        file.commands().push_back(std::move(beginMetafile));

        auto version = std::make_unique<opencgm::MetafileVersion>(&file);
        version->setVersion(1);
        file.commands().push_back(std::move(version));

        auto description =
            std::make_unique<opencgm::MetafileDescription>(&file);
        description->setDescription("MIL-D-28003A/BASIC-1.2");
        file.commands().push_back(std::move(description));

        auto beginPicture = std::make_unique<opencgm::BeginPicture>(&file);
        beginPicture->setName("picture-1");
        file.commands().push_back(std::move(beginPicture));
        auto extent = std::make_unique<opencgm::VDCExtent>(&file);
        extent->setExtent(
            opencgm::CGMPoint(0.0, 0.0), opencgm::CGMPoint(1000.0, 1000.0));
        file.commands().push_back(std::move(extent));
        file.commands().push_back(
            std::make_unique<opencgm::BeginPictureBody>(&file));
        file.commands().push_back(std::make_unique<opencgm::EndPicture>(&file));
        file.commands().push_back(
            std::make_unique<opencgm::EndMetafile>(&file));
        file.writeFile(path.string());
    }

    opencgm_ctx_t* ctx = opencgm_create();
    ASSERT_NE(ctx, nullptr);

    // Detection routes MIL-D-28003 metafile descriptions to the CALS profile.
    ASSERT_EQ(opencgm_detect_profile(ctx, path.string().c_str()), OPENCGM_OK);
    const char* detectionJson = opencgm_get_last_profile_detection_json(ctx);
    ASSERT_NE(detectionJson, nullptr);
    const std::string detection = detectionJson;
    EXPECT_NE(detection.find("\"cals\""), std::string::npos) << detection;
    EXPECT_NE(detection.find("CALS"), std::string::npos) << detection;

    // The dedicated "cals" validation contract is accepted and lenient:
    // a plain v1 static file passes.
    ASSERT_EQ(
        opencgm_validate_profile(ctx, path.string().c_str(), "cals"),
        OPENCGM_OK)
        << opencgm_last_error();
    const char* validationJson = opencgm_get_last_validation_json(ctx);
    ASSERT_NE(validationJson, nullptr);
    const std::string validation = validationJson;
    EXPECT_NE(validation.find("\"effectiveProfile\":\"cals\""), std::string::npos)
        << validation;
    EXPECT_NE(validation.find("\"passed\":true"), std::string::npos)
        << validation;

    opencgm_destroy(ctx);
    std::filesystem::remove(path);
}
