// Assembles minimal binary metafiles for end-to-end tests: integer VDC with an
// extent of 0..1000, default precisions, one picture. Append body elements with
// element(), then convert() closes the picture, runs the C API and returns SVG.
#pragma once

#include <gtest/gtest.h>

#include "opencgm/c_api.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

namespace opencgm_test
{
    class CgmBuilder
    {
    public:
        CgmBuilder() : CgmBuilder(0, 0, 1000, 1000) {}

        // Corners are written as given, so an inverted extent stays inverted.
        CgmBuilder(int x1, int y1, int x2, int y2)
        {
            element(0, 1, str("T"));                     // BEGIN METAFILE
            element(1, 1, i16(1));                       // METAFILE VERSION
            element(0, 3, str("P"));                     // BEGIN PICTURE
            element(2, 6, cat({i16(x1), i16(y1), i16(x2), i16(y2)}));  // VDC EXTENT
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

        struct Result
        {
            int status = 0;
            std::string svg;
            std::string error;   // opencgm_last_error() when status is not OK
            std::string report;  // opencgm_get_last_report_json()
        };

        // Expects the conversion to succeed and returns the SVG.
        std::string convert(const char *profile = "webcgm")
        {
            Result result = run(profile);
            EXPECT_EQ(result.status, OPENCGM_OK) << result.error;
            return result.svg;
        }

        // Converts without judging the outcome, for tests of failures.
        Result run(const char *profile = "webcgm")
        {
            namespace fs = std::filesystem;
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

            Result result;
            opencgm_ctx_t *ctx = opencgm_create();
            opencgm_set_profile(ctx, profile);
            result.status = opencgm_convert_cgm_to_svg(ctx, in.string().c_str(), out.string().c_str());
            if (result.status != OPENCGM_OK)
            {
                const char *message = opencgm_last_error();
                result.error = message ? message : "";
            }
            if (const char *report = opencgm_get_last_report_json(ctx))
            {
                result.report = report;
            }
            opencgm_destroy(ctx);

            std::stringstream ss;
            {
                std::ifstream f(out, std::ios::binary);
                ss << f.rdbuf();
            }
            fs::remove(in);
            fs::remove(out);
            result.svg = ss.str();
            return result;
        }

    private:
        void append(const std::vector<uint8_t> &v) { bytes_.insert(bytes_.end(), v.begin(), v.end()); }
        std::vector<uint8_t> bytes_;
    };
}
