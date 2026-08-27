#include <gtest/gtest.h>
#include <array>
#include <algorithm>

// Test the color scaling logic that was refactored
// This tests the algorithm without needing the full binary reader

namespace {

// Replicate the scaling logic for testing
std::array<int, 3> scaleColorValueRGB(int r, int g, int b, int precisionBits) {
    auto computeMaxValue = [](int bits) -> int {
        if (bits <= 0) {
            return 255;
        }
        if (bits >= static_cast<int>(std::numeric_limits<int>::digits)) {
            return std::numeric_limits<int>::max();
        }
        return static_cast<int>((1u << bits) - 1u);
    };

    const int maxValue = computeMaxValue(precisionBits);

    auto convertComponent = [maxValue](int value) -> int {
        int clamped = std::clamp(value, 0, maxValue);
        if (maxValue == 0) {
            return 0;
        }
        if (maxValue == 255) {
            return clamped;
        }
        long long scaled = static_cast<long long>(clamped) * 255 + maxValue / 2;
        return static_cast<int>(scaled / maxValue);
    };

    return {convertComponent(r), convertComponent(g), convertComponent(b)};
}

} // anonymous namespace

// Test basic functionality with 8-bit precision (no scaling needed)
TEST(ColorScalingTest, EightBitPrecision) {
    auto result = scaleColorValueRGB(128, 64, 192, 8);
    EXPECT_EQ(result[0], 128);
    EXPECT_EQ(result[1], 64);
    EXPECT_EQ(result[2], 192);
}

// Test 16-bit to 8-bit scaling
TEST(ColorScalingTest, SixteenBitPrecision) {
    // Max 16-bit value (65535) should map to 255
    auto result1 = scaleColorValueRGB(65535, 65535, 65535, 16);
    EXPECT_EQ(result1[0], 255);
    EXPECT_EQ(result1[1], 255);
    EXPECT_EQ(result1[2], 255);

    // Mid-range 16-bit value (~32768) should map to ~128
    auto result2 = scaleColorValueRGB(32768, 32768, 32768, 16);
    EXPECT_NEAR(result2[0], 128, 1);
    EXPECT_NEAR(result2[1], 128, 1);
    EXPECT_NEAR(result2[2], 128, 1);

    // Zero should map to zero
    auto result3 = scaleColorValueRGB(0, 0, 0, 16);
    EXPECT_EQ(result3[0], 0);
    EXPECT_EQ(result3[1], 0);
    EXPECT_EQ(result3[2], 0);
}

// Test 4-bit to 8-bit scaling
TEST(ColorScalingTest, FourBitPrecision) {
    // Max 4-bit value (15) should map to 255
    auto result1 = scaleColorValueRGB(15, 15, 15, 4);
    EXPECT_EQ(result1[0], 255);
    EXPECT_EQ(result1[1], 255);
    EXPECT_EQ(result1[2], 255);

    // Mid-range 4-bit value (7) should map to ~119
    auto result2 = scaleColorValueRGB(7, 7, 7, 4);
    EXPECT_NEAR(result2[0], 119, 2);
    EXPECT_NEAR(result2[1], 119, 2);
    EXPECT_NEAR(result2[2], 119, 2);
}

// Test clamping behavior
TEST(ColorScalingTest, ClampingBehavior) {
    // Values above max should be clamped
    auto result1 = scaleColorValueRGB(300, 400, 500, 8);
    EXPECT_EQ(result1[0], 255);
    EXPECT_EQ(result1[1], 255);
    EXPECT_EQ(result1[2], 255);

    // Negative values should be clamped to 0
    auto result2 = scaleColorValueRGB(-10, -20, -30, 8);
    EXPECT_EQ(result2[0], 0);
    EXPECT_EQ(result2[1], 0);
    EXPECT_EQ(result2[2], 0);
}

// Test edge case: zero precision
TEST(ColorScalingTest, ZeroPrecision) {
    // Zero or negative precision should default to 255
    auto result = scaleColorValueRGB(128, 64, 192, 0);
    EXPECT_EQ(result[0], 128);
    EXPECT_EQ(result[1], 64);
    EXPECT_EQ(result[2], 192);
}

// Test that the function returns std::array (memory safety)
TEST(ColorScalingTest, ReturnsArray) {
    auto result = scaleColorValueRGB(100, 150, 200, 8);

    // Verify we can use it as an array
    EXPECT_EQ(result.size(), 3);

    // Verify all components are accessible
    EXPECT_GE(result[0], 0);
    EXPECT_LE(result[0], 255);
    EXPECT_GE(result[1], 0);
    EXPECT_LE(result[1], 255);
    EXPECT_GE(result[2], 0);
    EXPECT_LE(result[2], 255);
}

// Test different precision values
TEST(ColorScalingTest, VariousPrecisions) {
    struct TestCase {
        int precision;
        int inputValue;
        int expectedMin;
        int expectedMax;
    };

    std::vector<TestCase> testCases = {
        {1, 1, 255, 255},      // 1-bit: 0 or 1 (max)
        {2, 3, 255, 255},      // 2-bit: 0-3 (max)
        {4, 8, 130, 140},      // 4-bit: mid-range
        {8, 128, 128, 128},    // 8-bit: no scaling
        {16, 32768, 127, 129}, // 16-bit: mid-range
    };

    for (const auto& tc : testCases) {
        auto result = scaleColorValueRGB(tc.inputValue, tc.inputValue, tc.inputValue, tc.precision);
        EXPECT_GE(result[0], tc.expectedMin);
        EXPECT_LE(result[0], tc.expectedMax);
    }
}
