// End-to-end checks that every filled-area primitive resolves its interior the
// same way, so an INTERIOR STYLE the emitter can only approximate gives one
// consistent, announced result instead of a different silent one per shape.

#include "cgm_test_builder.h"

#include <regex>
#include <string>
#include <vector>

namespace
{
    using opencgm_test::CgmBuilder;
    using B = CgmBuilder;

    // The fill attribute of the first element with this tag, or "<missing>".
    std::string fillOf(const std::string &svg, const std::string &tag)
    {
        std::smatch element;
        if (!std::regex_search(svg, element, std::regex("<" + tag + "\\b[^>]*>")))
            return "<no " + tag + ">";
        std::smatch fill;
        const std::string text = element.str();
        if (!std::regex_search(text, fill, std::regex(" fill=\"([^\"]*)\"")))
            return "<missing>";
        return fill[1].str();
    }

    void drawShapes(CgmBuilder &b)
    {
        b.element(4, 11, B::cat({B::i16(100), B::i16(100), B::i16(300), B::i16(300)}));  // RECTANGLE
        b.element(4, 12, B::cat({B::i16(500), B::i16(500), B::i16(100)}));               // CIRCLE
        b.element(4, 17, B::cat({B::i16(800), B::i16(500), B::i16(900), B::i16(500),     // ELLIPSE
                                 B::i16(800), B::i16(550)}));
    }
}

TEST(SvgFills, UnrenderableInteriorStyleIsConsistentAcrossPrimitives)
{
    // GEOMETRIC PATTERN (5) has no SVG rendering yet. Rectangles used to fill it
    // solid while circles and ellipses silently dropped the fill (#3).
    CgmBuilder b;
    b.element(5, 22, B::i16(5));  // INTERIOR STYLE: geometric pattern
    drawShapes(b);

    ::testing::internal::CaptureStderr();
    const std::string svg = b.convert();
    const std::string log = ::testing::internal::GetCapturedStderr();

    const std::string rect = fillOf(svg, "rect");
    EXPECT_NE(rect, "none");
    EXPECT_NE(rect.front(), '<') << rect;
    EXPECT_EQ(fillOf(svg, "circle"), rect);
    EXPECT_EQ(fillOf(svg, "ellipse"), rect);

    // Announced, and only once per conversion however many shapes use it.
    const auto first = log.find("INTERIOR STYLE 5 (GEOMETRIC PATTERN)");
    EXPECT_NE(first, std::string::npos) << log;
    EXPECT_EQ(log.find("INTERIOR STYLE 5", first + 1), std::string::npos) << log;
}

namespace
{
    // Direct colour, so the stops can name exact RGB values.
    void useDirectColour(CgmBuilder &b) { b.element(2, 2, B::i16(1)); }  // COLOUR SELECTION MODE

    std::vector<uint8_t> rgb(int r, int g, int b)
    {
        return {static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)};
    }

    void interpolatedRect(CgmBuilder &b, const std::vector<uint8_t> &interpolatedInterior)
    {
        b.element(5, 31, B::cat({B::i16(200), B::i16(200)}));  // FILL REFERENCE POINT
        b.element(5, 43, interpolatedInterior);                // INTERPOLATED INTERIOR
        b.element(5, 22, B::i16(6));                           // INTERIOR STYLE: interpolated
        b.element(4, 11, B::cat({B::i16(100), B::i16(100), B::i16(300), B::i16(300)}));
    }

    std::vector<std::string> stopOffsets(const std::string &svg)
    {
        std::vector<std::string> out;
        const std::regex re("<stop offset=\"([^\"]*)\" stop-color=\"([^\"]*)\"");
        for (auto it = std::sregex_iterator(svg.begin(), svg.end(), re); it != std::sregex_iterator(); ++it)
            out.push_back((*it)[1].str() + " " + (*it)[2].str());
        return out;
    }
}

TEST(SvgFills, ParallelInterpolationIsALinearGradientFromTheFillReferencePoint)
{
    CgmBuilder b;
    useDirectColour(b);
    // Vector (100, 0); stages 0.5 and 1.0 place three colours along it.
    interpolatedRect(b, B::cat({B::i16(1), B::i16(100), B::i16(0), B::i16(2), B::fixed32(0.5), B::fixed32(1.0),
                                rgb(255, 0, 0), rgb(0, 255, 0), rgb(0, 0, 255)}));
    const std::string svg = b.convert();

    EXPECT_NE(svg.find("<linearGradient id=\"cgm-gradient-1\" gradientUnits=\"userSpaceOnUse\""
                       " x1=\"200\" y1=\"800\" x2=\"300\" y2=\"800\">"),
              std::string::npos) << svg;
    EXPECT_EQ(stopOffsets(svg), (std::vector<std::string>{"0 #FF0000", "0.5 #00FF00", "1 #0000FF"}));
    EXPECT_EQ(fillOf(svg, "rect"), "url(#cgm-gradient-1)");
}

TEST(SvgFills, EllipticalInterpolationMapsBothConjugateRadii)
{
    // Radii of 300 and 100: a radialGradient with a bare r would be circular,
    // so both radii must come through the gradient transform.
    CgmBuilder b;
    useDirectColour(b);
    interpolatedRect(b, B::cat({B::i16(2), B::i16(300), B::i16(0), B::i16(0), B::i16(100), B::i16(1), B::fixed32(1.0),
                                rgb(0, 0, 0), rgb(255, 255, 255)}));
    const std::string svg = b.convert();

    EXPECT_NE(svg.find("<radialGradient id=\"cgm-gradient-1\" gradientUnits=\"userSpaceOnUse\" cx=\"0\" cy=\"0\""
                       " r=\"1\" gradientTransform=\"matrix(300 0 0 -100 200 800)\">"),
              std::string::npos) << svg;
    EXPECT_EQ(stopOffsets(svg), (std::vector<std::string>{"0 #000000", "1 #FFFFFF"}));
}

TEST(SvgFills, TriangularInterpolationFallsBackToTheAverageColour)
{
    CgmBuilder b;
    useDirectColour(b);
    interpolatedRect(b, B::cat({B::i16(3), B::i16(0), B::i16(0), B::i16(0), B::i16(0), B::i16(0),
                                rgb(255, 0, 0), rgb(0, 255, 0), rgb(0, 0, 255)}));

    ::testing::internal::CaptureStderr();
    const std::string svg = b.convert();
    const std::string log = ::testing::internal::GetCapturedStderr();

    EXPECT_EQ(fillOf(svg, "rect"), "#555555");
    EXPECT_EQ(svg.find("Gradient"), std::string::npos);
    EXPECT_NE(log.find("INTERPOLATED INTERIOR style 3 has no SVG gradient equivalent"), std::string::npos) << log;
}

TEST(SvgFills, HollowAndEmptyStayUnfilled)
{
    for (const int style : {0, 4})
    {
        CgmBuilder b;
        b.element(5, 22, B::i16(style));
        drawShapes(b);
        const std::string svg = b.convert();
        EXPECT_EQ(fillOf(svg, "rect"), "none") << "style " << style;
        EXPECT_EQ(fillOf(svg, "circle"), "none") << "style " << style;
        EXPECT_EQ(fillOf(svg, "ellipse"), "none") << "style " << style;
    }
}
