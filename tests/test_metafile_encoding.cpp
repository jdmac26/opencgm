// End-to-end checks that elements are decoded with the layout and precisions
// ISO/IEC 8632-3 gives them, using small hand-encoded metafiles.

#include "cgm_test_builder.h"

#include <regex>
#include <string>

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
