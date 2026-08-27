#include <gtest/gtest.h>

#include "opencgm/svg/xml_utils.h"

#include <string>

namespace
{
    TEST(SvgXmlUtilsTest, EscapesAttributesAndTextByContext)
    {
        const std::string value = "\"'&<>plain";

        EXPECT_EQ(
            opencgm::svg::escapeXmlAttribute(value),
            "&quot;&apos;&amp;&lt;&gt;plain");
        EXPECT_EQ(
            opencgm::svg::escapeXmlText(value),
            "\"'&amp;&lt;&gt;plain");
    }

    TEST(SvgXmlUtilsTest, PreservesValidUtf8AndFiltersInvalidXml)
    {
        std::string value = "before";
        value.push_back('\x01');
        value.append("\xC3\xA9", 2);       // U+00E9
        value.append("\xC0\xAF", 2);       // Overlong UTF-8
        value.append("\xED\xA0\x80", 3);   // UTF-8 surrogate
        value.append("\xF4\x90\x80\x80", 4); // Above U+10FFFF
        value += "after";

        EXPECT_EQ(
            opencgm::svg::escapeXmlText(value),
            std::string("before") + "\xC3\xA9" + "after");
        EXPECT_EQ(
            opencgm::svg::escapeXmlAttribute(value),
            std::string("before") + "\xC3\xA9" + "after");
    }

    TEST(SvgXmlUtilsTest, SanitizesIdentifiersDeterministically)
    {
        const std::string value =
            std::string("123 part/") + "\xC3\xA9";

        EXPECT_EQ(
            opencgm::svg::sanitizeIdentifier(value),
            "_123_u20part_u2F_uE9");
        EXPECT_EQ(opencgm::svg::sanitizeIdentifier(""), "_");
        EXPECT_EQ(
            opencgm::svg::sanitizeIdentifier("valid:id.name-2"),
            "valid:id.name-2");
    }

    TEST(SvgXmlUtilsTest, AllocatesUniqueDocumentLocalIdentifiers)
    {
        opencgm::svg::UniqueIdAllocator allocator("aps");

        EXPECT_EQ(allocator.allocate("panel"), "panel");
        EXPECT_EQ(allocator.allocate("panel"), "panel_2");
        EXPECT_EQ(allocator.allocate(""), "aps");
        EXPECT_EQ(allocator.allocate(""), "aps_2");

        allocator.reset();
        EXPECT_EQ(allocator.allocate("panel"), "panel");
        EXPECT_EQ(allocator.allocate(""), "aps");
    }
}
