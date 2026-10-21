// End-to-end checks that elements are decoded with the layout and precisions
// ISO/IEC 8632-3 gives them, using small hand-encoded metafiles.

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

    // Header word for an element embedded in METAFILE DEFAULTS REPLACEMENT.
    std::vector<uint8_t> header(int cls, int id, int length)
    {
        return B::i16((cls << 12) | (id << 5) | length);
    }

    std::string firstPathData(const std::string &svg)
    {
        std::smatch m;
        return std::regex_search(svg, m, std::regex("<path d=\"([^\"]*)\"")) ? m[1].str() : "<no path>";
    }
}

TEST(MetafileEncoding, NurbsReadsKnotsRangeThenWeights)
{
    // A straight NURBS from (100,100) to (900,900): order 2, two control
    // points, knots 0 0 1 1, range 0..1, then the weights. Reading the
    // weights straight after the points took the knots 0 0 as weights, and
    // zero-weighted spans were drawn from the origin.
    CgmBuilder b;
    b.element(4, 25, B::cat({B::i16(2), B::i16(2),
                             B::i16(100), B::i16(100), B::i16(900), B::i16(900),
                             B::fixed32(0), B::fixed32(0), B::fixed32(1), B::fixed32(1),
                             B::fixed32(0), B::fixed32(1),
                             B::fixed32(1), B::fixed32(1)}));
    const std::string d = firstPathData(b.convert());

    EXPECT_EQ(d.rfind("M 100.0000 900.0000", 0), 0u) << d;
    EXPECT_NE(d.find("900.0000 100.0000"), std::string::npos) << d;
}

TEST(MetafileEncoding, NubHasNoKnotCount)
{
    CgmBuilder b;
    b.element(4, 24, B::cat({B::i16(2), B::i16(2),
                             B::i16(100), B::i16(100), B::i16(900), B::i16(900),
                             B::fixed32(0), B::fixed32(0), B::fixed32(1), B::fixed32(1),
                             B::fixed32(0), B::fixed32(1)}));
    const std::string d = firstPathData(b.convert());

    EXPECT_EQ(d.rfind("M 100.0000 900.0000", 0), 0u) << d;
}

TEST(MetafileEncoding, DefaultsReplacementSetsThePrecisionThePictureUses)
{
    // Real VDC, with VDC REAL PRECISION set to IEEE single only inside
    // METAFILE DEFAULTS REPLACEMENT, as Auto-trol's converter writes it. The
    // precision used to stay with a scratch copy, so the picture's floats were
    // decoded as 16.16 fixed point.
    CgmBuilder b{CgmBuilder::Bare{}};
    b.element(0, 1, B::str("T"));   // BEGIN METAFILE
    b.element(1, 1, B::i16(1));     // METAFILE VERSION
    b.element(1, 3, B::i16(1));     // VDC TYPE real
    b.element(1, 12, B::cat({header(3, 2, 6), B::i16(0), B::i16(9), B::i16(23)}));  // VDC REAL PRECISION
    b.element(0, 3, B::str("P"));   // BEGIN PICTURE
    b.element(2, 6, B::cat({B::f32(0), B::f32(0), B::f32(10), B::f32(10)}));  // VDC EXTENT
    b.element(0, 4, {});            // BEGIN PICTURE BODY
    b.element(4, 1, B::cat({B::f32(1), B::f32(1), B::f32(9), B::f32(9)}));   // POLYLINE
    const std::string svg = b.convert();

    EXPECT_NE(svg.find("viewBox=\"0 0 10 10\""), std::string::npos) << svg.substr(0, 400);
    EXPECT_NE(svg.find("points=\"1,9 9,1\""), std::string::npos) << svg;
}

TEST(MetafileEncoding, SuccessiveDefaultsReplacementsAccumulate)
{
    // The second replacement is decoded with the precision the first set,
    // and does not discard it.
    CgmBuilder b{CgmBuilder::Bare{}};
    b.element(0, 1, B::str("T"));
    b.element(1, 1, B::i16(1));
    b.element(1, 3, B::i16(1));
    b.element(1, 12, B::cat({header(3, 2, 6), B::i16(0), B::i16(9), B::i16(23)}));
    b.element(1, 12, B::cat({header(2, 6, 16), B::f32(0), B::f32(0), B::f32(10), B::f32(10)}));  // VDC EXTENT
    b.element(0, 3, B::str("P"));
    b.element(0, 4, {});
    b.element(4, 1, B::cat({B::f32(1), B::f32(1), B::f32(9), B::f32(9)}));
    const std::string svg = b.convert();

    EXPECT_NE(svg.find("viewBox=\"0 0 10 10\""), std::string::npos) << svg.substr(0, 400);
    EXPECT_NE(svg.find("points=\"1,9 9,1\""), std::string::npos) << svg;
}

namespace
{
    // An SDR in its binary form, as a string parameter: length, then members
    // of (data type, count, values). 11 is IX (index), 16 VDC.
    std::vector<uint8_t> sdr(const std::vector<uint8_t> &members)
    {
        std::vector<uint8_t> out;
        if (members.size() < 255)
        {
            out.push_back(static_cast<uint8_t>(members.size()));
        }
        else
        {
            out.push_back(255);
            const auto length = B::i16(static_cast<int>(members.size()));
            out.insert(out.end(), length.begin(), length.end());
        }
        out.insert(out.end(), members.begin(), members.end());
        return out;
    }

    // A hotspot whose label sits at (9100, 6100) inside a rectangular region
    // 9000..9600 x 6000..6600, in 16-bit integer VDC.
    void drawIntegerHotspot(CgmBuilder &b, int regionType, const std::vector<int> &coordinates)
    {
        std::vector<uint8_t> vdc;
        for (int c : coordinates)
        {
            const auto v = B::i16(c);
            vdc.insert(vdc.end(), v.begin(), v.end());
        }
        b.element(0, 21, B::cat({B::str("hot-150"), B::str("grobject"), B::i16(1)}));  // BEGIN APS
        b.element(9, 1, B::cat({B::str("region"),
                                sdr(B::cat({B::i16(11), B::i16(1), B::i16(regionType),
                                            B::i16(16), B::i16(static_cast<int>(coordinates.size())), vdc}))}));
        b.element(0, 22, {});  // BEGIN APS BODY
        b.element(4, 4, B::cat({B::i16(9100), B::i16(6100), B::i16(1), B::str("150")}));
        b.element(0, 23, {});  // END APS
    }

    struct Bounds { double minX = 1e300, minY = 1e300, maxX = -1e300, maxY = -1e300; };

    Bounds hitRegion(const std::string &svg)
    {
        Bounds b;
        std::smatch m;
        if (!std::regex_search(svg, m, std::regex("class=\"aps-region-hit\"[^>]*?points=\"([^\"]*)\"|points=\"([^\"]*)\"[^>]*class=\"aps-region-hit\"")))
            return b;
        std::string list = m[1].matched ? m[1].str() : m[2].str();
        std::replace(list.begin(), list.end(), ',', ' ');
        std::istringstream in(list);
        double x, y;
        while (in >> x >> y)
        {
            b.minX = std::min(b.minX, x); b.maxX = std::max(b.maxX, x);
            b.minY = std::min(b.minY, y); b.maxY = std::max(b.maxY, y);
        }
        return b;
    }
}

TEST(MetafileEncoding, RegionCoordinatesUseTheFilesVdcFormat)
{
    // Region points are VDC values, so a file with integer VDC stores them
    // as integers. They were always read as 32-bit floats, which put every
    // hotspot's hit area somewhere other than its label.
    CgmBuilder b(0, 0, 32767, 32767);
    drawIntegerHotspot(b, 3, {9000, 6000, 9600, 6000, 9600, 6600, 9000, 6600});  // polygon
    const std::string svg = b.convert("webcgm");

    const Bounds r = hitRegion(svg);
    // SVG y runs down from the top of the extent.
    EXPECT_NEAR(r.minX, 9000, 1) << svg;
    EXPECT_NEAR(r.maxX, 9600, 1);
    EXPECT_NEAR(r.minY, 32767 - 6600, 1);
    EXPECT_NEAR(r.maxY, 32767 - 6000, 1);
}

TEST(MetafileEncoding, RectangleRegionIsTwoCorners)
{
    CgmBuilder b(0, 0, 32767, 32767);
    drawIntegerHotspot(b, 1, {9000, 6000, 9600, 6600});
    const Bounds r = hitRegion(b.convert("webcgm"));

    EXPECT_NEAR(r.minX, 9000, 1);
    EXPECT_NEAR(r.maxX, 9600, 1);
    EXPECT_NEAR(r.minY, 32767 - 6600, 1);
    EXPECT_NEAR(r.maxY, 32767 - 6000, 1);
}

TEST(MetafileEncoding, EllipseRegionCoversTheEllipse)
{
    // Centre, then the ends of two conjugate diameters.
    CgmBuilder b(0, 0, 32767, 32767);
    drawIntegerHotspot(b, 2, {9300, 6300, 9600, 6300, 9300, 6600});
    const Bounds r = hitRegion(b.convert("webcgm"));

    EXPECT_NEAR(r.minX, 9000, 5);
    EXPECT_NEAR(r.maxX, 9600, 5);
    EXPECT_NEAR(r.minY, 32767 - 6600, 5);
    EXPECT_NEAR(r.maxY, 32767 - 6000, 5);
}
