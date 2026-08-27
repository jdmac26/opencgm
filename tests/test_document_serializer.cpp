#include <gtest/gtest.h>

#include "opencgm/svg/document_serializer.h"
#include "svg_semantic_harness.h"

#include <string>
#include <vector>

namespace
{
    using opencgm::OutputProfile;
    using opencgm::svg::ApsMetadataEntry;
    using opencgm::svg::DocumentFeatures;
    using opencgm::svg::DocumentHeader;
    using opencgm::svg::DocumentSerializer;
    using opencgm::svg::ViewerShimMode;

    TEST(DocumentSerializerTest, ResolvesProfileLabelsAndTitles)
    {
        EXPECT_EQ(
            DocumentSerializer::profileLabel(
                OutputProfile::S1000D,
                {}),
            "s1000d-issue-6");
        EXPECT_EQ(
            DocumentSerializer::profileLabel(
                OutputProfile::ATA2200,
                {}),
            "ata2200");
        EXPECT_EQ(
            DocumentSerializer::profileLabel(
                OutputProfile::StandardSVG,
                "compat"),
            "auto");
        EXPECT_EQ(
            DocumentSerializer::profileLabel(
                OutputProfile::WebCGM21,
                "customer-profile"),
            "customer-profile");

        EXPECT_EQ(
            DocumentSerializer::title("  Picture 1  ", "source.cgm"),
            "Picture 1");
        EXPECT_EQ(
            DocumentSerializer::title(" ", " source.cgm "),
            "source.cgm");
        EXPECT_EQ(
            DocumentSerializer::title({}, {}),
            "CGM Picture");
    }

    TEST(DocumentSerializerTest, BuildsCompatibilityDescription)
    {
        EXPECT_EQ(
            DocumentSerializer::description(
                "1.2.3",
                "s1000d-issue-6",
                true,
                true,
                "figure.cgm",
                "ABC123"),
            "Converted by OpenCGM 1.2.3 "
            "(profile=s1000d-issue-6, compatibility-mode: "
            "iso8632-segments); source=figure.cgm; md5=ABC123");

        EXPECT_EQ(
            DocumentSerializer::description(
                "1.2.3",
                "standard",
                false,
                true,
                {},
                {}),
            "Converted by OpenCGM 1.2.3 (profile=standard)");
    }

    TEST(DocumentSerializerTest, EscapesHeaderAndGatesWebCgmNamespace)
    {
        DocumentHeader document;
        document.viewbox_x = -1.5;
        document.viewbox_y = 2.0;
        document.viewbox_width = 30.0;
        document.viewbox_height = 40.5;
        document.emit_webcgm_namespace = true;
        document.title = "A < B & C";
        document.description = "\"quoted\" & described";
        document.background_color = "#AABBCC";

        const std::string output =
            DocumentSerializer::header(document) +
            DocumentSerializer::close();

        EXPECT_NE(
            output.find(
                "xmlns:webcgm=\"http://www.w3.org/Graphics/WebCGM\""),
            std::string::npos);
        EXPECT_NE(
            output.find("viewBox=\"-1.5 2 30 40.5\""),
            std::string::npos);
        EXPECT_NE(
            output.find("<title>A &lt; B &amp; C</title>"),
            std::string::npos);
        EXPECT_NE(
            output.find(
                "<desc>\"quoted\" &amp; described</desc>"),
            std::string::npos);
        EXPECT_NE(
            output.find("fill=\"#AABBCC\""),
            std::string::npos);

        const auto summary =
            opencgm::tests::inspectSvgSemantics(output);
        EXPECT_TRUE(summary.wellFormed())
            << testing::PrintToString(summary.errors)
            << "\n"
            << output;

        document.emit_webcgm_namespace = false;
        document.include_metadata = false;
        const std::string minimal =
            DocumentSerializer::header(document);
        EXPECT_EQ(
            minimal.find("xmlns:webcgm"),
            std::string::npos);
        EXPECT_EQ(minimal.find("<title>"), std::string::npos);
        EXPECT_EQ(minimal.find("<desc>"), std::string::npos);
    }

    TEST(DocumentSerializerTest, JsonEscapesApsMetadataBeforeXmlEscaping)
    {
        ApsMetadataEntry entry;
        entry.identifier = "raw\"id";
        entry.resolved_identifier = "resolved&<id";
        entry.type = "hot\nspot";
        entry.inherit = true;
        entry.attributes = {
            {"control", std::string(1, '\x0B')},
            {"quoted\"key", "line1\nline2\\end"},
            {"xml", "<A&B>"}
        };

        const std::string output =
            DocumentSerializer::apsMetadata({entry});

        EXPECT_NE(
            output.find(
                "{\"aps\":[{\"id\":\"resolved&amp;&lt;id\""),
            std::string::npos);
        EXPECT_NE(
            output.find("\"identifier\":\"raw\\\"id\""),
            std::string::npos);
        EXPECT_NE(
            output.find("\"type\":\"hot\\nspot\""),
            std::string::npos);
        EXPECT_NE(
            output.find("\"inherit\":true"),
            std::string::npos);
        EXPECT_NE(
            output.find("\"control\":\"\\u000B\""),
            std::string::npos);
        EXPECT_NE(
            output.find(
                "\"quoted\\\"key\":\"line1\\nline2\\\\end\""),
            std::string::npos);
        EXPECT_NE(
            output.find("\"xml\":\"&lt;A&amp;B&gt;\""),
            std::string::npos);
        EXPECT_EQ(DocumentSerializer::apsMetadata({}), "");
    }

    TEST(DocumentSerializerTest, AppliesViewerShimPolicyAndEscapesUrls)
    {
        const DocumentFeatures noFeatures;
        const DocumentFeatures layers{true, false, false};

        EXPECT_FALSE(
            DocumentSerializer::shouldEmbedViewerShim(
                ViewerShimMode::Auto,
                noFeatures));
        EXPECT_TRUE(
            DocumentSerializer::shouldEmbedViewerShim(
                ViewerShimMode::Auto,
                layers));
        EXPECT_TRUE(
            DocumentSerializer::shouldEmbedViewerShim(
                ViewerShimMode::Always,
                noFeatures));
        EXPECT_FALSE(
            DocumentSerializer::shouldEmbedViewerShim(
                ViewerShimMode::Never,
                layers));

        EXPECT_EQ(
            DocumentSerializer::viewerShim(
                ViewerShimMode::Auto,
                noFeatures,
                {},
                "ignored"),
            "");

        const std::string external =
            DocumentSerializer::viewerShim(
                ViewerShimMode::Always,
                noFeatures,
                "shim.js?a=1&title=\"x\"",
                {});
        EXPECT_NE(
            external.find(
                "xlink:href=\"shim.js?a=1&amp;title=&quot;x&quot;\""),
            std::string::npos);

        const std::string inlineShim =
            DocumentSerializer::viewerShim(
                ViewerShimMode::Auto,
                layers,
                {},
                "(function(){return \"]]>\";})();");
        EXPECT_NE(
            inlineShim.find("<![CDATA["),
            std::string::npos);
        EXPECT_NE(
            inlineShim.find(
                "(function(){return \"]]]]><![CDATA[>\";})();"),
            std::string::npos);
        EXPECT_NE(
            inlineShim.find("]]></script>"),
            std::string::npos);
    }
}
