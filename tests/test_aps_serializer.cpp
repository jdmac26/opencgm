#include <gtest/gtest.h>

#include "opencgm/svg/aps_serializer.h"
#include "svg_semantic_harness.h"

#include <map>
#include <string>

namespace
{
    TEST(ApsSerializerTest, DerivesLayerVisibilityAndMetadata)
    {
        opencgm::svg::ApsNode node;
        node.identifier = "layer-1";
        node.resolved_identifier = "layer-1";
        node.type = "Layer";

        const auto state = opencgm::svg::ApsSerializer::analyze(
            node,
            {{"visibility", "off"}},
            {
                {"name", "Hydraulics"},
                {"layerdesc", "Primary & standby"}
            });

        EXPECT_TRUE(state.is_layer);
        EXPECT_FALSE(state.is_link);
        EXPECT_EQ(state.aps_id, "layer-1");
        EXPECT_EQ(state.aps_name, "Hydraulics");
        EXPECT_EQ(state.layer_name, "Hydraulics");
        EXPECT_EQ(state.layer_title, "Primary & standby");
        EXPECT_TRUE(state.emit_visibility_state);
        EXPECT_FALSE(state.effective_visible);
        EXPECT_EQ(
            state.metadata_attributes.at("resolved-visibility"),
            "off");
        EXPECT_EQ(
            state.metadata_attributes.at("resolved-layer"),
            "Hydraulics");
        EXPECT_EQ(
            state.metadata_attributes.at("apsid"),
            "layer-1");
        EXPECT_EQ(
            state.serialization_attributes.count("apsid"),
            0U);
    }

    TEST(ApsSerializerTest, EmitsDerivedLayerAttributesByProfile)
    {
        opencgm::svg::ApsSerializationState state;
        state.is_layer = true;
        state.layer_name = "A&B";
        state.layer_title = "\"Main\"";
        state.emit_visibility_state = true;
        state.effective_visible = false;

        const auto s1000d =
            opencgm::ResolvedConversionPlan::forProfile(
                opencgm::OutputProfile::S1000D);
        const std::string attributes =
            opencgm::svg::ApsSerializer::derivedGroupAttributes(
                state,
                s1000d);
        EXPECT_NE(
            attributes.find("data-layer=\"A&amp;B\""),
            std::string::npos);
        EXPECT_NE(
            attributes.find(
                "data-layer-title=\"&quot;Main&quot;\""),
            std::string::npos);
        EXPECT_NE(
            attributes.find("data-aps-visible=\"false\""),
            std::string::npos);
        EXPECT_NE(
            attributes.find("style=\"display:none\""),
            std::string::npos);

        const auto standard =
            opencgm::ResolvedConversionPlan::forProfile(
                opencgm::OutputProfile::StandardSVG);
        EXPECT_EQ(
            opencgm::svg::ApsSerializer::derivedGroupAttributes(
                state,
                standard),
            " style=\"display:none\"");

        // Anchor/title link wrapping alone must not suppress preserved
        // layer/visibility metadata (emit_attribute_metadata stays true).
        auto anchorTitle = opencgm::ResolvedConversionPlan::forProfile(
            opencgm::OutputProfile::WebCGM21);
        anchorTitle.output_target.hotspot_encoding =
            opencgm::HotspotEncodingMode::SvgAnchorTitle;
        const std::string anchorTitleAttributes =
            opencgm::svg::ApsSerializer::derivedGroupAttributes(
                state,
                anchorTitle);
        EXPECT_NE(
            anchorTitleAttributes.find("data-layer=\"A&amp;B\""),
            std::string::npos);
        EXPECT_NE(
            anchorTitleAttributes.find("data-aps-visible=\"false\""),
            std::string::npos);
    }

    TEST(ApsSerializerTest, EscapesJavaScriptMultiLinkAsXml)
    {
        opencgm::svg::ApsNode node;
        node.identifier = "hot-1";
        node.resolved_identifier = "hot-1";
        node.type = "hotspot";
        node.linkuris = {
            {"https://example.invalid/?a=1&b=2",
             "Primary \"link\"",
             "_blank"},
            {"DMC-SECOND", "Second", ""}
        };

        const std::map<std::string, std::string> attributes = {
            {"linkuri", node.linkuris.front().uri},
            {"screentip", "Open <details>"},
            {"desc", "A & B"}
        };
        const auto state = opencgm::svg::ApsSerializer::analyze(
            node,
            attributes,
            attributes);
        auto plan = opencgm::ResolvedConversionPlan::forProfile(
            opencgm::OutputProfile::S1000D);
        plan.output_target.multi_link_mode =
            opencgm::MultiLinkMode::JsEventHandler;

        const auto open = opencgm::svg::ApsSerializer::bodyOpen(
            node,
            attributes,
            state,
            plan,
            "0,0 10,0 10,10",
            true);

        EXPECT_TRUE(open.emitted_anchor);
        EXPECT_EQ(open.markup.find("\\\""), std::string::npos);
        EXPECT_NE(
            open.markup.find(
                "&quot;Primary \\&quot;link\\&quot;&quot;"),
            std::string::npos);
        EXPECT_NE(
            open.markup.find("a=1&amp;b=2"),
            std::string::npos);
        EXPECT_NE(
            open.markup.find("<title>Open &lt;details&gt;</title>"),
            std::string::npos);
        EXPECT_NE(
            open.markup.find("<desc>A &amp; B</desc>"),
            std::string::npos);
        EXPECT_NE(
            open.markup.find("data-aps-region-owner=\"hot-1\""),
            std::string::npos);

        const std::string svg =
            "<svg xmlns:xlink=\"http://www.w3.org/1999/xlink\"><g" +
            open.markup +
            opencgm::svg::ApsSerializer::close(0, true) +
            "</svg>";
        const auto summary =
            opencgm::tests::inspectSvgSemantics(svg);
        EXPECT_TRUE(summary.wellFormed())
            << testing::PrintToString(summary.errors)
            << "\n"
            << svg;
    }

    TEST(ApsSerializerTest, SerializesJsonMultiLinkOnce)
    {
        opencgm::svg::ApsNode node;
        node.identifier = "hot-json";
        node.type = "hotspot";
        node.linkuris = {
            {"first?a=1&b=2", "First", "_blank"},
            {"second", "", ""}
        };
        const std::map<std::string, std::string> attributes = {
            {"linkuri", node.linkuris.front().uri}
        };
        const auto state = opencgm::svg::ApsSerializer::analyze(
            node,
            attributes,
            attributes);
        auto plan = opencgm::ResolvedConversionPlan::forProfile(
            opencgm::OutputProfile::S1000D);
        plan.output_target.multi_link_mode =
            opencgm::MultiLinkMode::JsonDataAttribute;

        const auto open = opencgm::svg::ApsSerializer::bodyOpen(
            node,
            attributes,
            state,
            plan,
            {},
            false);
        EXPECT_TRUE(open.emitted_anchor);
        EXPECT_NE(
            open.markup.find(
                "data-linkuri=\"[{&quot;uri&quot;:"
                "&quot;first?a=1&amp;b=2&quot;"),
            std::string::npos);
        EXPECT_NE(
            open.markup.find("data-link-count=\"2\""),
            std::string::npos);
        EXPECT_EQ(
            open.markup.find("&amp;amp;"),
            std::string::npos);
    }

    TEST(ApsSerializerTest, KeepsInlineMetadataForDataOnlyLinks)
    {
        opencgm::svg::ApsNode node;
        node.identifier = "data-only";
        node.type = "hotspot";
        const std::map<std::string, std::string> attributes = {
            {"linkuri", "#target"},
            {"screentip", "Data-only tip"}
        };
        const auto state = opencgm::svg::ApsSerializer::analyze(
            node,
            attributes,
            attributes);
        auto plan = opencgm::ResolvedConversionPlan::forProfile(
            opencgm::OutputProfile::S1000D);
        plan.output_target.hotspot_encoding =
            opencgm::HotspotEncodingMode::DataAttributes;

        const auto open = opencgm::svg::ApsSerializer::bodyOpen(
            node,
            attributes,
            state,
            plan,
            {},
            false);
        EXPECT_FALSE(open.emitted_anchor);
        EXPECT_NE(
            open.markup.find("<title>Data-only tip</title>"),
            std::string::npos);
    }
}
