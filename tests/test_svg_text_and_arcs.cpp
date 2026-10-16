// End-to-end checks for closed-arc geometry, RESTRICTED TEXT fitting, TEXT
// PATH and continuous TEXT ALIGNMENT. Each test assembles a minimal binary
// metafile (VDC extent 0..1000, integer VDC, default precisions), converts it
// through the C API and inspects the SVG.

#include <gtest/gtest.h>

#include "opencgm/c_api.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    class CgmBuilder
    {
    public:
        CgmBuilder()
        {
            element(0, 1, str("T"));                     // BEGIN METAFILE
            element(1, 1, i16(1));                       // METAFILE VERSION
            element(0, 3, str("P"));                     // BEGIN PICTURE
            element(2, 6, cat({i16(0), i16(0), i16(1000), i16(1000)}));  // VDC EXTENT
            element(0, 4, {});                           // BEGIN PICTURE BODY
        }

        static std::vector<uint8_t> i16(int v)
        {
            return {static_cast<uint8_t>((v >> 8) & 0xFF), static_cast<uint8_t>(v & 0xFF)};
        }

        // Default REAL PRECISION: 16-bit signed whole part, 16-bit fraction.
        static std::vector<uint8_t> fixed32(double v)
        {
            const int whole = static_cast<int>(std::floor(v));
            const int frac = static_cast<int>(std::lround((v - whole) * 65536.0)) & 0xFFFF;
            return cat({i16(whole), i16(frac)});
        }

        static std::vector<uint8_t> str(const std::string &s)
        {
            std::vector<uint8_t> out{static_cast<uint8_t>(s.size())};
            out.insert(out.end(), s.begin(), s.end());
            return out;
        }

        static std::vector<uint8_t> cat(std::initializer_list<std::vector<uint8_t>> parts)
        {
            std::vector<uint8_t> out;
            for (const auto &p : parts)
                out.insert(out.end(), p.begin(), p.end());
            return out;
        }

        void element(int cls, int id, const std::vector<uint8_t> &params)
        {
            const int len = static_cast<int>(params.size());
            const int shortLen = len < 31 ? len : 31;
            const uint16_t header = static_cast<uint16_t>((cls << 12) | (id << 5) | shortLen);
            append(i16(header));
            if (len >= 31)
                append(i16(len));
            append(params);
            if (len % 2 != 0)
                bytes_.push_back(0);
        }

        std::string convert()
        {
            element(0, 5, {});  // END PICTURE
            element(0, 2, {});  // END METAFILE

            // ctest runs each test in its own process, possibly in parallel,
            // so name the scratch files after the test rather than a counter.
            const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
            const auto dir = fs::temp_directory_path();
            const auto stem = std::string("opencgm_") + info->test_suite_name() + "_" + info->name();
            const auto in = dir / (stem + ".cgm");
            const auto out = dir / (stem + ".svg");
            {
                std::ofstream f(in, std::ios::binary);
                f.write(reinterpret_cast<const char *>(bytes_.data()), static_cast<std::streamsize>(bytes_.size()));
            }

            opencgm_ctx_t *ctx = opencgm_create();
            opencgm_set_profile(ctx, "webcgm");
            const int rc = opencgm_convert_cgm_to_svg(ctx, in.string().c_str(), out.string().c_str());
            opencgm_destroy(ctx);
            EXPECT_EQ(rc, OPENCGM_OK);

            std::stringstream ss;
            {
                std::ifstream f(out, std::ios::binary);
                ss << f.rdbuf();
            }
            fs::remove(in);
            fs::remove(out);
            return ss.str();
        }

    private:
        void append(const std::vector<uint8_t> &v) { bytes_.insert(bytes_.end(), v.begin(), v.end()); }
        std::vector<uint8_t> bytes_;
    };

    using B = CgmBuilder;

    std::vector<std::string> textElements(const std::string &svg)
    {
        std::vector<std::string> out;
        const std::regex re("<text[^>]*>[^<]*</text>");
        for (auto it = std::sregex_iterator(svg.begin(), svg.end(), re); it != std::sregex_iterator(); ++it)
            out.push_back(it->str());
        return out;
    }

    double attr(const std::string &element, const std::string &name)
    {
        std::smatch m;
        const std::regex re(" " + name + "=\"([-0-9.e]+)\"");
        if (!std::regex_search(element, m, re))
            return std::nan("");
        return std::stod(m[1].str());
    }

    std::string content(const std::string &element)
    {
        const auto open = element.find('>');
        const auto close = element.rfind("</text>");
        return element.substr(open + 1, close - open - 1);
    }

    void restrictedText(CgmBuilder &b, int w, int h, int x, int y, const std::string &text)
    {
        b.element(4, 5, B::cat({B::i16(w), B::i16(h), B::i16(x), B::i16(y), B::i16(1), B::str(text)}));
    }
}

TEST(SvgArcs, ThreePointArcWithCoincidentEndsIsFullCirclePie)
{
    // Start and end coincide: start and intermediate span a diameter.
    CgmBuilder b;
    b.element(4, 14, B::cat({B::i16(100), B::i16(500), B::i16(300), B::i16(500),
                             B::i16(100), B::i16(500), B::i16(0) /* PIE */}));
    const std::string svg = b.convert();

    EXPECT_EQ(svg.find("L 100 500 Z"), std::string::npos) << "collapsed to a zero-length path";
    EXPECT_NE(svg.find(" A 100 100 "), std::string::npos) << svg;
    EXPECT_NE(svg.find("L 200 500 Z"), std::string::npos) << "PIE radius to the centre missing\n" << svg;
}

TEST(SvgArcs, FullCircleCentreArcPieDrawsRadius)
{
    CgmBuilder b;
    b.element(4, 16, B::cat({B::i16(500), B::i16(500), B::i16(1), B::i16(0), B::i16(1), B::i16(0),
                             B::i16(100), B::i16(0) /* PIE */}));
    const std::string svg = b.convert();

    EXPECT_EQ(svg.find("<ellipse"), std::string::npos);
    EXPECT_NE(svg.find("M 600 500 A 100 100 0 0 1 400 500 A 100 100 0 0 1 600 500 L 500 500 Z"),
              std::string::npos) << svg;
}

TEST(SvgArcs, FullCircleCentreArcChordHasNoRadius)
{
    CgmBuilder b;
    b.element(4, 16, B::cat({B::i16(500), B::i16(500), B::i16(1), B::i16(0), B::i16(1), B::i16(0),
                             B::i16(100), B::i16(1) /* CHORD */}));
    const std::string svg = b.convert();

    EXPECT_NE(svg.find("A 100 100 0 0 1 600 500 Z"), std::string::npos) << svg;
    EXPECT_EQ(svg.find("L 500 500"), std::string::npos) << svg;
}

TEST(SvgArcs, FullEllipseCloseKeepsOrientation)
{
    // Major conjugate diameter vertical (200), minor horizontal (100).
    CgmBuilder b;
    b.element(4, 19, B::cat({B::i16(500), B::i16(500), B::i16(500), B::i16(700), B::i16(400), B::i16(500),
                             B::i16(0), B::i16(1), B::i16(0), B::i16(1), B::i16(1) /* CHORD */}));
    const std::string svg = b.convert();

    std::smatch m;
    const std::regex arc(" A ([-0-9.e]+) ([-0-9.e]+) ([-0-9.e]+) ");
    ASSERT_TRUE(std::regex_search(svg, m, arc)) << svg;
    const double rx = std::stod(m[1].str());
    const double ry = std::stod(m[2].str());
    const double rotation = std::fmod(std::fabs(std::stod(m[3].str())), 180.0);
    // Whatever the parameterisation, the long axis must end up vertical.
    const bool verticalMajor = (std::fabs(rotation - 90.0) < 1.0 && rx > ry) || (rotation < 1.0 && ry > rx);
    EXPECT_TRUE(verticalMajor) << "rx=" << rx << " ry=" << ry << " rotation=" << rotation;
}

TEST(SvgRestrictedText, DefaultsToBoxedCap)
{
    CgmBuilder b;
    b.element(5, 15, B::i16(15));  // CHARACTER HEIGHT
    restrictedText(b, 300, 30, 100, 700, "Hello");
    const auto texts = textElements(b.convert());

    ASSERT_EQ(texts.size(), 1u);
    EXPECT_NEAR(attr(texts[0], "font-size"), 30.0 / 0.7, 0.01);
    EXPECT_NEAR(attr(texts[0], "textLength"), 300.0, 1e-9);
    EXPECT_NE(texts[0].find("lengthAdjust=\"spacingAndGlyphs\""), std::string::npos);
}

TEST(SvgRestrictedText, BasicOnlyShrinks)
{
    CgmBuilder b;
    b.element(5, 42, B::i16(1));   // RESTRICTED TEXT TYPE: basic
    b.element(5, 15, B::i16(15));
    restrictedText(b, 300, 30, 100, 700, "Hello");
    const auto texts = textElements(b.convert());

    ASSERT_EQ(texts.size(), 1u);
    EXPECT_NEAR(attr(texts[0], "font-size"), 15.0, 1e-9);
    EXPECT_EQ(texts[0].find("textLength"), std::string::npos);
}

TEST(SvgTextPath, LeftReversesTheRun)
{
    CgmBuilder b;
    b.element(5, 15, B::i16(15));
    b.element(5, 17, B::i16(1));   // TEXT PATH LEFT
    restrictedText(b, 0, 0, 500, 500, "AB");
    const auto texts = textElements(b.convert());

    ASSERT_EQ(texts.size(), 1u);
    EXPECT_EQ(content(texts[0]), "BA");
    // NORMAL horizontal alignment for LEFT is RIGHT: the run ends at the anchor.
    EXPECT_LT(attr(texts[0], "x"), 500.0);
}

TEST(SvgTextPath, UpStacksCharactersUpward)
{
    CgmBuilder b;
    b.element(5, 15, B::i16(15));
    b.element(5, 17, B::i16(2));   // TEXT PATH UP
    restrictedText(b, 20, 100, 500, 500, "AB");
    const auto texts = textElements(b.convert());

    ASSERT_EQ(texts.size(), 2u);
    EXPECT_EQ(content(texts[0]), "A");
    EXPECT_EQ(content(texts[1]), "B");
    EXPECT_NEAR(attr(texts[0], "x"), 500.0, 1e-6);  // centred on the column
    EXPECT_LT(attr(texts[1], "y"), attr(texts[0], "y"));  // B above A
}

TEST(SvgTextPath, DownStacksCharactersDownward)
{
    CgmBuilder b;
    b.element(5, 15, B::i16(15));
    b.element(5, 17, B::i16(3));   // TEXT PATH DOWN
    restrictedText(b, 20, 100, 500, 500, "AB");
    const auto texts = textElements(b.convert());

    ASSERT_EQ(texts.size(), 2u);
    EXPECT_GT(attr(texts[1], "y"), attr(texts[0], "y"));  // B below A
    EXPECT_GT(attr(texts[0], "y"), 500.0);  // NORMAL vertical is TOP: hangs below
}

TEST(SvgTextAlignment, ContinuousAlignmentUsesFractions)
{
    CgmBuilder b;
    b.element(5, 15, B::i16(15));
    // TEXT ALIGNMENT: continuous horizontal 0.5, continuous vertical 1.0 (top)
    b.element(5, 18, B::cat({B::i16(4), B::i16(6), B::fixed32(0.5), B::fixed32(1.0)}));
    restrictedText(b, 300, 30, 100, 700, "Hello");
    const auto texts = textElements(b.convert());

    ASSERT_EQ(texts.size(), 1u);
    const double fontSize = 30.0 / 0.7;
    EXPECT_NEAR(attr(texts[0], "x"), 100.0 - 150.0, 1e-6);
    EXPECT_NEAR(attr(texts[0], "y"), 300.0 + 0.8 * fontSize, 1e-3);
}
