#include <gtest/gtest.h>

#include "opencgm/svg/output_compactor.h"

#include <string>

using opencgm::svg::OutputCompactor;

namespace
{
    const std::string kStyle = R"( stroke="#000000" stroke-width="0.3" fill="none")";

    std::string polyline(const std::string &points, const std::string &style = kStyle, const std::string &indent = "  ")
    {
        return indent + "<polyline points=\"" + points + "\"" + style + " />";
    }
}

TEST(OutputCompactor, GroupsARunOfIdenticallyStyledPrimitives)
{
    const std::string svg = "<svg>\n" + polyline("0,0 1,1") + "\n" + polyline("2,2 3,3") + "\n</svg>\n";
    EXPECT_EQ(OutputCompactor::hoistSharedStyles(svg),
              "<svg>\n"
              "  <g stroke=\"#000000\" stroke-width=\"0.3\" fill=\"none\">\n"
              "   <polyline points=\"0,0 1,1\" />\n"
              "   <polyline points=\"2,2 3,3\" />\n"
              "  </g>\n"
              "</svg>\n");
}

TEST(OutputCompactor, LeavesLoneAndDifferentlyStyledPrimitivesAsWritten)
{
    const std::string red = R"( stroke="#FF0000" stroke-width="0.3" fill="none")";
    const std::string svg = "<svg>\n" + polyline("0,0 1,1") + "\n" + polyline("2,2 3,3", red) + "\n</svg>";
    EXPECT_EQ(OutputCompactor::hoistSharedStyles(svg), svg);
}

TEST(OutputCompactor, OtherElementsAndIndentationEndARun)
{
    const std::string svg = "<svg>\n" + polyline("0,0 1,1") + "\n  <text x=\"0\">a</text>\n" + polyline("2,2 3,3") +
                            "\n" + polyline("4,4 5,5", kStyle, "    ") + "\n</svg>";
    EXPECT_EQ(OutputCompactor::hoistSharedStyles(svg), svg);
}

TEST(OutputCompactor, NeverTouchesDefsClipPathsOrAddressedElements)
{
    const std::string inDefs = "<svg>\n  <defs>\n" + polyline("0,0 1,1") + "\n" + polyline("2,2 3,3") + "\n  </defs>\n</svg>";
    EXPECT_EQ(OutputCompactor::hoistSharedStyles(inDefs), inDefs);

    // <g> is not allowed inside <clipPath>.
    const std::string inClip = "<svg>\n  <clipPath id=\"c\">\n    <rect x=\"0\" fill=\"#000\" />\n"
                               "    <rect x=\"1\" fill=\"#000\" />\n  </clipPath>\n</svg>";
    EXPECT_EQ(OutputCompactor::hoistSharedStyles(inClip), inClip);

    const std::string addressed = "<svg>\n" + polyline("0,0 1,1", kStyle + R"( id="a")") + "\n" +
                                  polyline("2,2 3,3", kStyle + R"( class="aps-region-hit")") + "\n</svg>";
    EXPECT_EQ(OutputCompactor::hoistSharedStyles(addressed), addressed);
}

TEST(OutputCompactor, ResumesAfterAClosedDefs)
{
    const std::string svg = "<svg>\n  <defs>\n  </defs>\n" + polyline("0,0 1,1") + "\n" + polyline("2,2 3,3") + "\n</svg>";
    EXPECT_NE(OutputCompactor::hoistSharedStyles(svg).find("<g stroke="), std::string::npos);
}

TEST(OutputCompactor, RoundsPointsAndKeepsSeparators)
{
    const std::string svg = R"(<polyline points="136.94,74.7913 136.907,74.6801 -0.0001,2" fill="none" />)";
    EXPECT_EQ(OutputCompactor::roundPoints(svg, 2),
              R"(<polyline points="136.94,74.79 136.91,74.68 0,2" fill="none" />)");
    EXPECT_EQ(OutputCompactor::roundPoints(R"(<path d="M 1.23456 2" />)", 2), R"(<path d="M 1.23456 2" />)")
        << "only polyline/polygon points";
}

TEST(OutputCompactor, DecimalsKeepRoundingUnderATenThousandthOfThePicture)
{
    EXPECT_EQ(OutputCompactor::decimalsForViewBox(R"(<svg viewBox="0 0 32767 32767">)", 0), 0);
    EXPECT_EQ(OutputCompactor::decimalsForViewBox(R"(<svg viewBox="0 0 167.687 132.385">)", 0), 2);
    EXPECT_EQ(OutputCompactor::decimalsForViewBox(R"(<svg viewBox="0 0 3.4 69.6">)", 0), 3);
    EXPECT_EQ(OutputCompactor::decimalsForViewBox(R"(<svg viewBox="0 0 1 1">)", 0), 4);
    EXPECT_EQ(OutputCompactor::decimalsForViewBox(R"(<svg viewBox="0 0 32767 32767">)", 2), 2) << "requested minimum";
    EXPECT_EQ(OutputCompactor::decimalsForViewBox("<svg>", 3), 3);
}
