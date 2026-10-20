// End-to-end checks of how the drawing sits in the viewBox when the VDC extent
// is much larger than the drawing, as in tech-pubs CGMs that keep a whole
// sheet as their extent.

#include "cgm_test_builder.h"

#include <algorithm>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    using opencgm_test::CgmBuilder;
    using B = CgmBuilder;

    struct Box { double x = 0, y = 0, w = 0, h = 0; };

    Box viewBox(const std::string &svg)
    {
        std::smatch m;
        Box box;
        if (std::regex_search(svg, m, std::regex("viewBox=\"([^\"]*)\"")))
        {
            std::istringstream in(m[1].str());
            in >> box.x >> box.y >> box.w >> box.h;
        }
        return box;
    }

    // Bounding box of every polyline point in the document.
    Box drawingBounds(const std::string &svg)
    {
        double minX = 1e300, minY = 1e300, maxX = -1e300, maxY = -1e300;
        const std::regex points("<polyline points=\"([^\"]*)\"");
        for (auto it = std::sregex_iterator(svg.begin(), svg.end(), points); it != std::sregex_iterator(); ++it)
        {
            std::string list = (*it)[1].str();
            std::replace(list.begin(), list.end(), ',', ' ');
            std::istringstream in(list);
            double x, y;
            while (in >> x >> y)
            {
                minX = std::min(minX, x); maxX = std::max(maxX, x);
                minY = std::min(minY, y); maxY = std::max(maxY, y);
            }
        }
        return {minX, minY, maxX - minX, maxY - minY};
    }

    // A 2000-unit square plus one diagonal, in the corner of the extent.
    void drawSmallSquare(CgmBuilder &b)
    {
        b.element(4, 1, B::cat({B::i16(1000), B::i16(1000), B::i16(3000), B::i16(1000), B::i16(3000), B::i16(3000),
                                B::i16(1000), B::i16(3000), B::i16(1000), B::i16(1000)}));
        b.element(4, 1, B::cat({B::i16(1000), B::i16(1000), B::i16(3000), B::i16(3000)}));
    }
}

TEST(SvgViewBox, AutofitPlacesTheDrawingInTheFittedViewBox)
{
    // The square covers ~6% of the extent on each axis, so S1000D output fits
    // the viewBox to it. The transform used to keep the full extent, which put
    // the square in a corner of the fitted viewBox at a few percent of its size.
    CgmBuilder b(0, 0, 32767, 32767);
    drawSmallSquare(b);
    const std::string svg = b.convert("s1000d");

    const Box vb = viewBox(svg);
    const Box drawing = drawingBounds(svg);
    ASSERT_GT(vb.w, 0.0);
    EXPECT_LT(vb.w, 32767.0 / 4) << "viewBox was not fitted";
    EXPECT_GT(drawing.w / vb.w, 0.8) << "drawing " << drawing.w << " wide in a viewBox " << vb.w << " wide";
    EXPECT_GT(drawing.h / vb.h, 0.8);
    // And it stays inside the viewBox.
    EXPECT_GE(drawing.x, vb.x);
    EXPECT_LE(drawing.x + drawing.w, vb.x + vb.w);
    EXPECT_GE(drawing.y, vb.y);
    EXPECT_LE(drawing.y + drawing.h, vb.y + vb.h);
}

TEST(SvgViewBox, AutofitKeepsAnInvertedYAxisFlipped)
{
    // Extent declared top-down: VDC y grows downward, so the diagonal from
    // (1000,1000) to (3000,3000) must run down-right in SVG as well.
    CgmBuilder b(0, 32767, 32767, 0);
    drawSmallSquare(b);
    const std::string svg = b.convert("s1000d");

    std::smatch m;
    ASSERT_TRUE(std::regex_search(svg, m, std::regex("<polyline points=\"([-0-9.e]+),([-0-9.e]+) ([-0-9.e]+),([-0-9.e]+)\"")));
    EXPECT_LT(std::stod(m[1].str()), std::stod(m[3].str()));
    EXPECT_LT(std::stod(m[2].str()), std::stod(m[4].str())) << "y should increase down the page";
    const Box vb = viewBox(svg);
    const Box drawing = drawingBounds(svg);
    EXPECT_GT(drawing.w / vb.w, 0.8);
}

TEST(SvgViewBox, AutofitCountsEveryPrimitiveBeforeCropping)
{
    // A page-spanning DISJOINT POLYLINE plus a small square. Bounds that
    // skipped disjoint polylines saw only the square, cropped to it, and cut
    // the rest of the drawing off (ATA dplyln01 and friends).
    CgmBuilder b(0, 0, 32767, 32767);
    drawSmallSquare(b);
    b.element(4, 2, B::cat({B::i16(500), B::i16(500), B::i16(32000), B::i16(32000)}));  // DISJOINT POLYLINE
    const std::string svg = b.convert("s1000d");

    EXPECT_DOUBLE_EQ(viewBox(svg).w, 32767.0) << "cropped to part of the drawing";
}

TEST(SvgViewBox, AutofitStandsDownWhenAnExtentIsUnknown)
{
    // Plain TEXT has no extent the bounds pass can measure, so a crop could
    // cut it off; keep the full picture instead.
    CgmBuilder b(0, 0, 32767, 32767);
    drawSmallSquare(b);
    b.element(4, 4, B::cat({B::i16(1500), B::i16(1500), B::i16(1), B::str("label")}));  // TEXT
    const std::string svg = b.convert("s1000d");

    EXPECT_DOUBLE_EQ(viewBox(svg).w, 32767.0);
}

TEST(SvgViewBox, WebCgmKeepsTheFullExtent)
{
    // WebCGM profiles do not autofit: the viewBox is the VDC extent.
    CgmBuilder b(0, 0, 32767, 32767);
    drawSmallSquare(b);
    const std::string svg = b.convert("webcgm");

    const Box vb = viewBox(svg);
    EXPECT_DOUBLE_EQ(vb.w, 32767.0);
    const Box drawing = drawingBounds(svg);
    EXPECT_DOUBLE_EQ(drawing.x, 1000.0);
    EXPECT_DOUBLE_EQ(drawing.w, 2000.0);
}
