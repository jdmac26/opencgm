#include <gtest/gtest.h>

#include "opencgm/svg/aps_text_decoder.h"

#include <string>
#include <vector>

namespace
{
    TEST(ApsTextDecoderTest, SplitsControlDelimitedTokens)
    {
        std::string raw = "apsid";
        raw.push_back('\0');
        raw += "panel-1";
        raw.push_back('\n');
        raw += "linkuri http://example.test/figure.svg";

        EXPECT_EQ(
            opencgm::svg::ApsTextDecoder::decodeTokens(raw),
            (std::vector<std::string>{
                "apsid",
                "panel-1",
                "linkuri http://example.test/figure.svg"}));
    }

    TEST(ApsTextDecoderTest, ConvertsLatin1ToUtf8)
    {
        std::string raw = "caf";
        raw.push_back(static_cast<char>(0xE9));

        EXPECT_EQ(
            opencgm::svg::ApsTextDecoder::decodeTokens(raw),
            (std::vector<std::string>{
                std::string("caf") + "\xC3\xA9"}));
    }

    TEST(ApsTextDecoderTest, HonorsUtf8EscapeMode)
    {
        std::string raw = "\x1B%Glabel ";
        raw.append("\xF0\x9F\x98\x80", 4);

        EXPECT_EQ(
            opencgm::svg::ApsTextDecoder::decodeTokens(raw),
            (std::vector<std::string>{
                std::string("label ") + "\xF0\x9F\x98\x80"}));
    }

    TEST(ApsTextDecoderTest, FallsBackToLatin1ForMalformedUtf8)
    {
        std::string raw = "\x1B%G";
        raw.append("\xC0\xAF", 2);

        EXPECT_EQ(
            opencgm::svg::ApsTextDecoder::decodeTokens(raw),
            (std::vector<std::string>{
                std::string("\xC3\x80\xC2\xAF", 4)}));
    }

    TEST(ApsTextDecoderTest, DecodesUtf16BeforeTokenSplitting)
    {
        const std::string bigEndian(
            "\xFE\xFF"
            "\x00\x61\x00\x70\x00\x73\x00\x69\x00\x64"
            "\x00\x0A"
            "\x00\x70\x00\x61\x00\x6E\x00\x65\x00\x6C",
            24);
        const std::string littleEndian(
            "\x61\x00\x70\x00\x73\x00\x69\x00\x64\x00",
            10);

        EXPECT_EQ(
            opencgm::svg::ApsTextDecoder::decodeTokens(bigEndian),
            (std::vector<std::string>{"apsid", "panel"}));
        EXPECT_EQ(
            opencgm::svg::ApsTextDecoder::decodeTokens(littleEndian),
            (std::vector<std::string>{"apsid"}));
    }

    TEST(ApsTextDecoderTest, RejectsUnusableTokens)
    {
        EXPECT_FALSE(
            opencgm::svg::ApsTextDecoder::isUsableToken(""));
        EXPECT_FALSE(
            opencgm::svg::ApsTextDecoder::isUsableToken("?"));
        EXPECT_FALSE(
            opencgm::svg::ApsTextDecoder::isUsableToken("==="));
        EXPECT_FALSE(
            opencgm::svg::ApsTextDecoder::isUsableToken(
                std::string("bad") + '\x01'));
        EXPECT_FALSE(
            opencgm::svg::ApsTextDecoder::isUsableToken(
                "\xEF\xBF\xBD"));
        EXPECT_TRUE(
            opencgm::svg::ApsTextDecoder::isUsableToken(
                "panel-1"));
    }
}
