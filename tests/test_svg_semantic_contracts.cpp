#include <gtest/gtest.h>

#include "opencgm/c_api.h"
#include "opencgm/svg_converter.h"
#include "svg_semantic_harness.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    class EmptyCgmFile final : public opencgm::CGMFile
    {
    public:
        EmptyCgmFile() = default;
    };

    TEST(SvgSemanticHarnessTest, DetectsStructureAndDuplicateAttributes)
    {
        const auto valid = opencgm::tests::inspectSvgSemantics(
            "<svg viewBox=\"0 0 1 1\"><g id=\"a\"><path d=\"M0 0\" />"
            "</g></svg>");
        EXPECT_TRUE(valid.wellFormed());
        EXPECT_EQ(valid.element_counts.at("path"), 1U);
        EXPECT_EQ(valid.attribute_counts.at("viewBox"), 1U);

        const auto invalid = opencgm::tests::inspectSvgSemantics(
            "<svg><g id=\"a\" id=\"b\"></svg>");
        EXPECT_FALSE(invalid.wellFormed());
        EXPECT_GE(invalid.errors.size(), 2U);
    }

    TEST(ResolvedConversionPlanTest, PresetsResolveTargetAndAttributeFormatTogether)
    {
        const auto ata = opencgm::ResolvedConversionPlan::forProfile(
            opencgm::OutputProfile::ATA2200);
        EXPECT_EQ(
            ata.attribute_format,
            opencgm::svg::AttributeManager::OutputFormat::S1000D_Issue6);
        EXPECT_EQ(
            ata.output_target.hotspot_encoding,
            opencgm::HotspotEncodingMode::Both);
        EXPECT_TRUE(ata.output_target.emit_webcgm_namespace);

        const auto standard = opencgm::ResolvedConversionPlan::forProfile(
            opencgm::OutputProfile::StandardSVG);
        EXPECT_EQ(
            standard.attribute_format,
            opencgm::svg::AttributeManager::OutputFormat::WebCGM);
        EXPECT_EQ(
            standard.output_target.hotspot_encoding,
            opencgm::HotspotEncodingMode::SvgAnchorTitle);
        EXPECT_FALSE(standard.output_target.emit_webcgm_namespace);
    }

    TEST(ResolvedConversionPlanTest, LegacySettersUpdateOnePendingPlan)
    {
        EmptyCgmFile file;
        opencgm::SVGConverter converter(&file);
        converter.setOutputProfile(opencgm::OutputProfile::ATA2200);

        auto target = converter.getOutputTargetConfig();
        target.region_handling = opencgm::RegionHandlingMode::BboxOnly;
        converter.setOutputTargetConfig(target);
        converter.setHotspotProfile(opencgm::HotspotProfile::Strict);

        const auto &plan = converter.getConversionPlan();
        EXPECT_EQ(plan.output_profile, opencgm::OutputProfile::ATA2200);
        EXPECT_EQ(
            plan.attribute_format,
            opencgm::svg::AttributeManager::OutputFormat::S1000D_Issue6);
        EXPECT_EQ(
            plan.output_target.region_handling,
            opencgm::RegionHandlingMode::BboxOnly);
        EXPECT_FALSE(plan.hotspot.emit_xlink_href);
    }

    TEST(ApsPolicyTest, ResolvesProfileGatesWithoutConverterState)
    {
        const auto standard = opencgm::ResolvedConversionPlan::forProfile(
            opencgm::OutputProfile::StandardSVG);
        const auto standardGates =
            opencgm::svg::ApsPolicy::emissionGates(standard);
        EXPECT_FALSE(standardGates.emit_id);
        EXPECT_FALSE(standardGates.emit_name);
        EXPECT_FALSE(standardGates.emit_link_uri);
        EXPECT_FALSE(standardGates.emit_webcgm_namespace);
        EXPECT_TRUE(opencgm::svg::ApsPolicy::shouldEmitAnchor(standard));
        EXPECT_FALSE(
            opencgm::svg::ApsPolicy::shouldEmitRegionOverlay(standard));

        const auto s1000d = opencgm::ResolvedConversionPlan::forProfile(
            opencgm::OutputProfile::S1000D);
        const auto s1000dGates =
            opencgm::svg::ApsPolicy::emissionGates(s1000d);
        EXPECT_TRUE(s1000dGates.emit_id);
        EXPECT_TRUE(s1000dGates.emit_name);
        EXPECT_TRUE(s1000dGates.emit_link_uri);
        EXPECT_TRUE(s1000dGates.emit_webcgm_namespace);
    }

    TEST(ApsPolicyTest, AnchorTitleEncodingDoesNotSuppressPreservedMetadata)
    {
        // Regression: hotspot_encoding selects link wrapping only. With
        // emit_attribute_metadata true (the default), SvgAnchorTitle must
        // honor the per-key preserve flags instead of zeroing every gate.
        auto plan = opencgm::ResolvedConversionPlan::forProfile(
            opencgm::OutputProfile::WebCGM21);
        plan.output_target.hotspot_encoding =
            opencgm::HotspotEncodingMode::SvgAnchorTitle;

        const auto gates = opencgm::svg::ApsPolicy::emissionGates(plan);
        EXPECT_TRUE(gates.emit_id);
        EXPECT_TRUE(gates.emit_name);
        EXPECT_TRUE(gates.emit_link_uri);
        EXPECT_TRUE(gates.emit_region);
        EXPECT_TRUE(gates.emit_viewcontext);
        EXPECT_TRUE(gates.emit_link_title);
        EXPECT_TRUE(gates.emit_screen_tip);
        EXPECT_TRUE(gates.emit_auxiliary);
        EXPECT_TRUE(opencgm::svg::ApsPolicy::shouldEmitAnchor(plan));

        // Unchecking the preserve flags is still honored in the same mode.
        plan.output_target.preserve_aps_id = false;
        plan.output_target.emit_data_name = false;
        plan.output_target.emit_data_content = false;
        const auto unchecked =
            opencgm::svg::ApsPolicy::emissionGates(plan);
        EXPECT_FALSE(unchecked.emit_id);
        EXPECT_FALSE(unchecked.emit_name);
        EXPECT_FALSE(unchecked.emit_link_uri);

        // The clean-output master switch (StandardSVG) suppresses all
        // attribute metadata regardless of preserve flags.
        auto standard = opencgm::ResolvedConversionPlan::forProfile(
            opencgm::OutputProfile::StandardSVG);
        standard.output_target.preserve_aps_id = true;
        standard.output_target.emit_data_name = true;
        const auto cleanGates =
            opencgm::svg::ApsPolicy::emissionGates(standard);
        EXPECT_FALSE(cleanGates.emit_id);
        EXPECT_FALSE(cleanGates.emit_name);
        EXPECT_FALSE(cleanGates.emit_auxiliary);
    }

    TEST(ApsPolicyTest, FiltersAndScopesCustomAttributes)
    {
        auto plan = opencgm::ResolvedConversionPlan::forProfile(
            opencgm::OutputProfile::S1000D);
        plan.hotspot.custom_attributes = {
            {"data-review", "ready", opencgm::CustomAttributeScope::ApsId,
             "hot-1"},
            {"data-other", "no", opencgm::CustomAttributeScope::ApsId,
             "hot-2"},
            {"onload", "bad", opencgm::CustomAttributeScope::All, "*"},
            {"id", "duplicate", opencgm::CustomAttributeScope::All, "*"}
        };

        opencgm::svg::ApsNode node;
        node.identifier = "hot-1";
        node.resolved_identifier = "hot-1";
        node.type = "grobject";
        node.merged_attributes["linkuri"] = "https://example.invalid/";

        EXPECT_TRUE(opencgm::svg::ApsPolicy::isLink(
            node, node.merged_attributes));
        const auto attributes = opencgm::svg::ApsPolicy::customAttributes(
            plan, node, {node}, {"id"});
        ASSERT_EQ(attributes.size(), 1U);
        EXPECT_EQ(attributes.at("data-review"), "ready");
    }

    struct ProfileSemanticContract
    {
        std::string name;
        int outputProfile;
        std::map<std::string, size_t> requiredElements;
        std::map<std::string, size_t> requiredAttributes;
        std::vector<std::string> forbiddenAttributes;
        std::vector<std::string> forbiddenFragments;
    };

    std::string sanitizeTestName(const std::string &value)
    {
        std::string sanitized = value;
        std::replace_if(
            sanitized.begin(), sanitized.end(),
            [](char ch)
            {
                return !std::isalnum(static_cast<unsigned char>(ch));
            },
            '_');
        return sanitized;
    }

    fs::path getSamplesPath()
    {
        const fs::path sourceFile(__FILE__);
        const auto samples = sourceFile.parent_path().parent_path() / "samples";
        return fs::exists(samples) ? samples : fs::path();
    }

    std::string readFile(const fs::path &path)
    {
        std::ifstream stream(path, std::ios::binary);
        std::ostringstream output;
        output << stream.rdbuf();
        return output.str();
    }

    class Context
    {
    public:
        Context() : value(opencgm_create()) {}
        ~Context()
        {
            if (value)
            {
                opencgm_destroy(value);
            }
        }

        opencgm_ctx_t *value = nullptr;
    };

    class SvgProfileSemanticContractTest
        : public ::testing::TestWithParam<ProfileSemanticContract>
    {
    };

    TEST_P(SvgProfileSemanticContractTest, SatisfiesOutputContract)
    {
        const auto samples = getSamplesPath();
        if (samples.empty())
        {
            GTEST_SKIP() << "Samples directory not found";
        }

        const auto input =
            samples / "webcgm21-ts" / "20tests" / "AppStructure-linkuri.cgm";
        ASSERT_TRUE(fs::exists(input));

        Context context;
        ASSERT_NE(context.value, nullptr);
        opencgm_set_profile(context.value, "webcgm");
        opencgm_set_output_profile(context.value, GetParam().outputProfile);
        opencgm_set_embed_shim(context.value, OPENCGM_SHIM_OFF);
        opencgm_set_pretty_print(context.value, 1);

#ifdef _WIN32
        const auto tempDirectory = fs::temp_directory_path();
#else
        const auto tempDirectory = fs::path("/tmp");
#endif
        const auto output =
            tempDirectory /
            ("opencgm-semantic-" + sanitizeTestName(GetParam().name) + ".svg");
        std::error_code ignored;
        fs::remove(output, ignored);

        ASSERT_EQ(
            opencgm_convert_cgm_to_svg(
                context.value, input.string().c_str(), output.string().c_str()),
            OPENCGM_OK)
            << opencgm_last_error();

        const std::string svg = readFile(output);
        const auto summary = opencgm::tests::inspectSvgSemantics(svg);
        EXPECT_TRUE(summary.wellFormed())
            << testing::PrintToString(summary.errors);

        for (const auto &[name, minimum] : GetParam().requiredElements)
        {
            const auto it = summary.element_counts.find(name);
            const size_t count =
                it == summary.element_counts.end() ? 0 : it->second;
            EXPECT_GE(count, minimum)
                << "Required element <" << name << "> missing for "
                << GetParam().name;
        }
        for (const auto &[name, minimum] : GetParam().requiredAttributes)
        {
            const auto it = summary.attribute_counts.find(name);
            const size_t count =
                it == summary.attribute_counts.end() ? 0 : it->second;
            EXPECT_GE(count, minimum)
                << "Required attribute '" << name << "' missing for "
                << GetParam().name;
        }
        for (const std::string &name : GetParam().forbiddenAttributes)
        {
            EXPECT_EQ(summary.attribute_counts.count(name), 0U)
                << "Forbidden attribute '" << name << "' leaked into "
                << GetParam().name;
        }
        for (const std::string &fragment : GetParam().forbiddenFragments)
        {
            EXPECT_EQ(svg.find(fragment), std::string::npos)
                << "Forbidden fragment '" << fragment << "' leaked into "
                << GetParam().name;
        }
    }

    const std::vector<ProfileSemanticContract> contracts = {
        {
            "WebCGM21",
            OPENCGM_OUTPUT_WEBCGM21,
            {{"svg", 1}, {"g", 1}, {"a", 1}, {"title", 1}, {"polygon", 1}},
            {{"viewBox", 1}, {"xmlns:webcgm", 1}, {"webcgm:type", 1},
             {"webcgm:linkuri", 1}, {"xlink:href", 1}},
            {"data-apsid"},
            {" onclick="}
        },
        {
            "S1000DIssue6",
            OPENCGM_OUTPUT_S1000D,
            {{"svg", 1}, {"g", 1}, {"a", 1}, {"title", 1}, {"polygon", 1}},
            {{"viewBox", 1}, {"xmlns:webcgm", 1}, {"webcgm:type", 1},
             {"data-apsid", 1}, {"xlink:href", 1}},
            {},
            {" onclick="}
        },
        {
            "ATA2200",
            OPENCGM_OUTPUT_ATA2200,
            {{"svg", 1}, {"g", 1}, {"a", 1}, {"title", 1}, {"polygon", 1}},
            {{"viewBox", 1}, {"xmlns:webcgm", 1}, {"webcgm:type", 1},
             {"data-apsid", 1}, {"xlink:href", 1}},
            {},
            {" onclick="}
        },
        {
            "StandardSVG",
            OPENCGM_OUTPUT_STANDARD_SVG,
            {{"svg", 1}, {"g", 1}, {"a", 1}, {"title", 1}},
            {{"viewBox", 1}, {"xlink:href", 1}},
            {"xmlns:webcgm", "webcgm:type", "webcgm:linkuri", "data-apsid"},
            {"webcgm:", "data-aps-", "class=\"aps-region-hit\"", " onclick="}
        },
        {
            // Pre-Issue-6 S1000D: bare-attribute superset so every known
            // viewer convention matches (apsid + name historic, apsname per
            // RWS LiveContent). viewBox is required for LiveContent scaling.
            "S1000DLegacy",
            OPENCGM_OUTPUT_S1000D_LEGACY,
            {{"svg", 1}, {"g", 1}, {"a", 1}, {"title", 1}, {"polygon", 1}},
            {{"viewBox", 1}, {"xmlns:webcgm", 1}, {"webcgm:type", 1},
             {"apsid", 1}, {"apsname", 1}, {"name", 1}, {"xlink:href", 1}},
            {"data-apsid"},
            {" onclick="}
        }
    };

    INSTANTIATE_TEST_SUITE_P(
        SemanticOutputProfiles,
        SvgProfileSemanticContractTest,
        ::testing::ValuesIn(contracts),
        [](const ::testing::TestParamInfo<ProfileSemanticContract> &info)
        {
            return sanitizeTestName(info.param.name);
        });
}
