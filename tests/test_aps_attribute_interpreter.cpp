#include <gtest/gtest.h>

#include "opencgm/svg/aps_attribute_interpreter.h"

#include <string>
#include <vector>

namespace
{
    TEST(ApsAttributeInterpreterTest, RecognizesKeysAndVisibilityValues)
    {
        using Interpreter =
            opencgm::svg::ApsAttributeInterpreter;

        EXPECT_TRUE(Interpreter::isReservedKey(" APSName "));
        EXPECT_TRUE(Interpreter::isReservedKey("linkuri"));
        EXPECT_FALSE(Interpreter::isReservedKey("panel-1"));

        EXPECT_FALSE(Interpreter::parseVisibility("hidden"));
        EXPECT_FALSE(Interpreter::parseVisibility(" NO "));
        EXPECT_TRUE(Interpreter::parseVisibility("visible"));
        EXPECT_TRUE(Interpreter::parseVisibility("1"));
        EXPECT_FALSE(Interpreter::parseVisibility("inherit", false));
        EXPECT_TRUE(Interpreter::parseVisibility("unknown", true));
    }

    TEST(ApsAttributeInterpreterTest, CollectsJoinsAndFallsBackToValues)
    {
        using Interpreter =
            opencgm::svg::ApsAttributeInterpreter;
        const std::vector<std::string> tokens = {
            "apsname", "Main", "Panel", "linkuri",
            "http://example.test/figure.svg"};

        const auto values =
            Interpreter::collectValueTokens(tokens, "apsname");
        EXPECT_EQ(
            values,
            (std::vector<std::string>{"Main", "Panel"}));
        EXPECT_EQ(Interpreter::joinValues(values), "Main Panel");
        EXPECT_EQ(
            Interpreter::fallbackValue(
                {"apsname", "Main", "linkuri"},
                "apsname"),
            "Main");

        EXPECT_EQ(
            Interpreter::collectValueTokens(
                {"apsname=\"Main Panel\""},
                "apsname"),
            (std::vector<std::string>{"Main Panel"}));
    }

    TEST(ApsAttributeInterpreterTest, SanitizesScalarsAndScoresTokens)
    {
        using Interpreter =
            opencgm::svg::ApsAttributeInterpreter;
        std::string scalar = "  Main";
        scalar.push_back('\x01');
        scalar += "\t Panel  ";

        EXPECT_EQ(
            Interpreter::sanitizeScalar(scalar),
            "Main Panel");
        EXPECT_GT(
            Interpreter::scoreTokens({"apsname", "Main Panel"}),
            Interpreter::scoreTokens(
                {std::string("\xEF\xBF\xBD")}));
    }

    TEST(ApsAttributeInterpreterTest, ParsesStructuredLinkuriFields)
    {
        using Interpreter =
            opencgm::svg::ApsAttributeInterpreter;
        const auto fields = Interpreter::parseLinkuri({
            "linkuri=http://example.test/a b.svg"
            "!behavior=new"
            "!target=_blank"
            "!content=Open Figure"
            "!highlight=flash"});

        EXPECT_EQ(
            fields.uri,
            "http://example.test/a%20b.svg");
        EXPECT_EQ(fields.behavior, "NewWindow");
        EXPECT_EQ(fields.target, "_blank");
        EXPECT_EQ(fields.content, "Open Figure");
        EXPECT_EQ(fields.highlight, "flash");
    }

    TEST(ApsAttributeInterpreterTest, RejectsUnsafeLinkSchemes)
    {
        using Interpreter =
            opencgm::svg::ApsAttributeInterpreter;

        EXPECT_EQ(
            Interpreter::sanitizeLinkHref(
                "https://example.test/figure.svg"),
            "https://example.test/figure.svg");
        EXPECT_EQ(
            Interpreter::sanitizeLinkHref("#panel-1"),
            "#panel-1");
        EXPECT_TRUE(
            Interpreter::sanitizeLinkHref(
                "javascript:alert(1)").empty());
        EXPECT_TRUE(
            Interpreter::sanitizeLinkHref(
                "//example.test").empty());
        EXPECT_TRUE(
            Interpreter::sanitizeLinkHref("//").empty());
    }
}
