#include "opencgm/svg/tile_raster.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace opencgm::svg
{
    namespace
    {
        constexpr std::size_t fileHeaderSize = 14;
        constexpr std::size_t infoHeaderSize = 40;
        constexpr std::size_t bitmapPixelOffset =
            fileHeaderSize + infoHeaderSize;

        bool checkedAdd(
            std::size_t left,
            std::size_t right,
            std::size_t &result)
        {
            if (left >
                std::numeric_limits<std::size_t>::max() - right)
            {
                return false;
            }
            result = left + right;
            return true;
        }

        bool checkedMultiply(
            std::size_t left,
            std::size_t right,
            std::size_t &result)
        {
            if (left != 0 &&
                right >
                    std::numeric_limits<std::size_t>::max() / left)
            {
                return false;
            }
            result = left * right;
            return true;
        }

        bool alignToFour(
            std::size_t value,
            std::size_t &result)
        {
            std::size_t withPadding = 0;
            if (!checkedAdd(value, 3, withPadding))
            {
                return false;
            }
            result = withPadding & ~std::size_t{3};
            return true;
        }

        void write16(
            std::vector<uint8_t> &buffer,
            std::size_t offset,
            uint16_t value)
        {
            buffer[offset] =
                static_cast<uint8_t>(value & 0xFFu);
            buffer[offset + 1] =
                static_cast<uint8_t>((value >> 8) & 0xFFu);
        }

        void write32(
            std::vector<uint8_t> &buffer,
            std::size_t offset,
            uint32_t value)
        {
            buffer[offset] =
                static_cast<uint8_t>(value & 0xFFu);
            buffer[offset + 1] =
                static_cast<uint8_t>((value >> 8) & 0xFFu);
            buffer[offset + 2] =
                static_cast<uint8_t>((value >> 16) & 0xFFu);
            buffer[offset + 3] =
                static_cast<uint8_t>((value >> 24) & 0xFFu);
        }

        bool initializeBmp(
            int width,
            int height,
            uint16_t bitsPerPixel,
            std::size_t paletteSize,
            std::size_t rowStride,
            std::vector<uint8_t> &buffer,
            std::size_t &pixelOffset)
        {
            if (width <= 0 || height <= 0)
            {
                return false;
            }

            std::size_t imageSize = 0;
            if (!checkedMultiply(
                    rowStride,
                    static_cast<std::size_t>(height),
                    imageSize))
            {
                return false;
            }
            if (!checkedAdd(
                    bitmapPixelOffset,
                    paletteSize,
                    pixelOffset))
            {
                return false;
            }

            std::size_t fileSize = 0;
            if (!checkedAdd(pixelOffset, imageSize, fileSize) ||
                fileSize >
                    std::numeric_limits<uint32_t>::max())
            {
                return false;
            }

            buffer.assign(fileSize, 0);
            buffer[0] = 'B';
            buffer[1] = 'M';
            write32(
                buffer,
                2,
                static_cast<uint32_t>(fileSize));
            write32(
                buffer,
                10,
                static_cast<uint32_t>(pixelOffset));
            write32(
                buffer,
                14,
                static_cast<uint32_t>(infoHeaderSize));
            write32(
                buffer,
                18,
                static_cast<uint32_t>(width));
            write32(
                buffer,
                22,
                static_cast<uint32_t>(height));
            write16(buffer, 26, 1);
            write16(buffer, 28, bitsPerPixel);
            write32(buffer, 30, 0);
            write32(
                buffer,
                34,
                static_cast<uint32_t>(imageSize));
            write32(buffer, 38, 2835);
            write32(buffer, 42, 2835);
            return true;
        }

        std::optional<std::size_t> resolveSourceRowBytes(
            const std::vector<uint8_t> &source,
            int height,
            std::size_t minimumRowBytes)
        {
            if (height <= 0 || minimumRowBytes == 0)
            {
                return std::nullopt;
            }

            const std::size_t heightValue =
                static_cast<std::size_t>(height);
            std::size_t sourceRowBytes = minimumRowBytes;
            if (source.size() % heightValue == 0)
            {
                const std::size_t candidate =
                    source.size() / heightValue;
                if (candidate >= minimumRowBytes)
                {
                    sourceRowBytes = candidate;
                }
            }

            std::size_t requiredSize = 0;
            if (!checkedMultiply(
                    sourceRowBytes,
                    heightValue,
                    requiredSize) ||
                source.size() < requiredSize)
            {
                return std::nullopt;
            }
            return sourceRowBytes;
        }

        class BitReader
        {
        public:
            explicit BitReader(
                const std::vector<uint8_t> &source)
                : source_(source)
            {
            }

            bool setPosition(std::size_t bitPosition)
            {
                std::size_t totalBits = 0;
                if (!checkedMultiply(
                        source_.size(),
                        std::size_t{8},
                        totalBits) ||
                    bitPosition > totalBits)
                {
                    return false;
                }
                bit_position_ = bitPosition;
                return true;
            }

            std::optional<uint32_t> readBits(int count)
            {
                if (count <= 0 || count > 32)
                {
                    return std::nullopt;
                }

                std::size_t endPosition = 0;
                std::size_t totalBits = 0;
                if (!checkedAdd(
                        bit_position_,
                        static_cast<std::size_t>(count),
                        endPosition) ||
                    !checkedMultiply(
                        source_.size(),
                        std::size_t{8},
                        totalBits) ||
                    endPosition > totalBits)
                {
                    return std::nullopt;
                }

                uint32_t value = 0;
                for (int bit = 0; bit < count; ++bit)
                {
                    const std::size_t byteIndex =
                        bit_position_ >> 3;
                    const int bitIndex =
                        7 -
                        static_cast<int>(
                            bit_position_ & std::size_t{7});
                    value =
                        static_cast<uint32_t>(
                            (value << 1) |
                            ((source_[byteIndex] >> bitIndex) & 1u));
                    ++bit_position_;
                }
                return value;
            }

        private:
            const std::vector<uint8_t> &source_;
            std::size_t bit_position_ = 0;
        };

        uint64_t maximumForBits(int bits)
        {
            return bits == 32
                       ? std::numeric_limits<uint32_t>::max()
                       : (uint64_t{1} << bits) - 1u;
        }

        uint8_t scaleToByte(uint32_t value, int bits)
        {
            const uint64_t maximum = maximumForBits(bits);
            const uint64_t scaled =
                (static_cast<uint64_t>(value) * 255u +
                 maximum / 2u) /
                maximum;
            return static_cast<uint8_t>(
                std::min<uint64_t>(scaled, 255u));
        }

        int64_t signExtend(uint32_t value, int bits)
        {
            const uint64_t sign =
                uint64_t{1} << (bits - 1);
            const uint64_t extended = value;
            return (extended & sign) != 0
                       ? static_cast<int64_t>(
                             extended -
                             (uint64_t{1} << bits))
                       : static_cast<int64_t>(extended);
        }

        double clampUnit(double value)
        {
            return std::clamp(value, 0.0, 1.0);
        }

        uint8_t unitToByte(double value)
        {
            return static_cast<uint8_t>(
                std::lround(clampUnit(value) * 255.0));
        }

        std::array<uint8_t, 3> xyzToRgb(
            double xValue,
            double yValue,
            double zValue)
        {
            const double x = xValue / 100.0;
            const double y = yValue / 100.0;
            const double z = zValue / 100.0;
            const auto gamma = [](double component) {
                component = std::max(0.0, component);
                return component <= 0.0031308
                           ? 12.92 * component
                           : 1.055 *
                                     std::pow(
                                         component,
                                         1.0 / 2.4) -
                                 0.055;
            };
            return {
                unitToByte(gamma(
                    3.2406 * x -
                    1.5372 * y -
                    0.4986 * z)),
                unitToByte(gamma(
                    -0.9689 * x +
                    1.8758 * y +
                    0.0415 * z)),
                unitToByte(gamma(
                    0.0557 * x -
                    0.2040 * y +
                    1.0570 * z))};
        }

        std::array<uint8_t, 3> labToRgb(
            double luminance,
            double a,
            double b)
        {
            constexpr double referenceX = 95.047;
            constexpr double referenceY = 100.0;
            constexpr double referenceZ = 108.883;
            constexpr double delta = 6.0 / 29.0;
            const auto inverse = [delta](double value) {
                return value > delta
                           ? value * value * value
                           : 3.0 * delta * delta *
                                 (value - 4.0 / 29.0);
            };

            const double fy = (luminance + 16.0) / 116.0;
            const double fx = fy + a / 500.0;
            const double fz = fy - b / 200.0;
            return xyzToRgb(
                referenceX * inverse(fx),
                referenceY * inverse(fy),
                referenceZ * inverse(fz));
        }

        std::array<uint8_t, 3> luvToRgb(
            double luminance,
            double uStar,
            double vStar)
        {
            constexpr double referenceX = 95.047;
            constexpr double referenceY = 100.0;
            constexpr double referenceZ = 108.883;
            if (luminance <= 0.0)
            {
                return {0, 0, 0};
            }

            const double referenceDenominator =
                referenceX +
                15.0 * referenceY +
                3.0 * referenceZ;
            const double referenceU =
                4.0 * referenceX / referenceDenominator;
            const double referenceV =
                9.0 * referenceY / referenceDenominator;
            const double uPrime =
                uStar / (13.0 * luminance) + referenceU;
            const double vPrime =
                vStar / (13.0 * luminance) + referenceV;
            const double y =
                luminance > 8.0
                    ? std::pow(
                          (luminance + 16.0) / 116.0,
                          3.0)
                    : luminance / 903.3;
            if (std::abs(vPrime) < 1e-9)
            {
                return {
                    0,
                    unitToByte(y),
                    0};
            }

            const double denominator =
                (uPrime - 4.0) * vPrime -
                uPrime * vPrime;
            if (std::abs(denominator) < 1e-9)
            {
                return {
                    0,
                    unitToByte(y),
                    0};
            }

            const double x =
                -(9.0 * y * uPrime) / denominator;
            const double z =
                (9.0 * y -
                 15.0 * vPrime * y -
                 vPrime * x) /
                (3.0 * vPrime);
            return xyzToRgb(
                x * referenceY,
                y * referenceY,
                z * referenceY);
        }

        std::array<uint8_t, 3> cmykToRgb(
            uint8_t cyan,
            uint8_t magenta,
            uint8_t yellow,
            uint8_t black)
        {
            const double c = cyan / 255.0;
            const double m = magenta / 255.0;
            const double y = yellow / 255.0;
            const double k = black / 255.0;
            return {
                unitToByte(
                    1.0 - std::min(1.0, c + k)),
                unitToByte(
                    1.0 - std::min(1.0, m + k)),
                unitToByte(
                    1.0 - std::min(1.0, y + k))};
        }

        std::optional<std::array<uint8_t, 3>>
        readDirectColor(
            BitReader &reader,
            ColorModel model,
            int bitsPerComponent)
        {
            const auto first =
                reader.readBits(bitsPerComponent);
            const auto second =
                reader.readBits(bitsPerComponent);
            const auto third =
                reader.readBits(bitsPerComponent);
            if (!first || !second || !third)
            {
                return std::nullopt;
            }

            if (model == ColorModel::CMYK)
            {
                const auto fourth =
                    reader.readBits(bitsPerComponent);
                if (!fourth)
                {
                    return std::nullopt;
                }
                return cmykToRgb(
                    scaleToByte(*first, bitsPerComponent),
                    scaleToByte(*second, bitsPerComponent),
                    scaleToByte(*third, bitsPerComponent),
                    scaleToByte(*fourth, bitsPerComponent));
            }

            if (model == ColorModel::CIELAB ||
                model == ColorModel::CIELUV)
            {
                const double luminance =
                    static_cast<double>(*first) /
                    static_cast<double>(
                        maximumForBits(bitsPerComponent)) *
                    100.0;
                const double signedMaximum =
                    bitsPerComponent > 1
                        ? static_cast<double>(
                              (uint64_t{1}
                               << (bitsPerComponent - 1)) -
                              1u)
                        : 0.0;
                const double firstSigned =
                    signedMaximum > 0.0
                        ? static_cast<double>(
                              signExtend(
                                  *second,
                                  bitsPerComponent)) /
                              signedMaximum
                        : 0.0;
                const double secondSigned =
                    signedMaximum > 0.0
                        ? static_cast<double>(
                              signExtend(
                                  *third,
                                  bitsPerComponent)) /
                              signedMaximum
                        : 0.0;
                if (model == ColorModel::CIELAB)
                {
                    return labToRgb(
                        luminance,
                        firstSigned * 128.0,
                        secondSigned * 128.0);
                }
                return luvToRgb(
                    luminance,
                    firstSigned * 134.0,
                    secondSigned * 140.0);
            }

            return std::array<uint8_t, 3>{
                scaleToByte(*first, bitsPerComponent),
                scaleToByte(*second, bitsPerComponent),
                scaleToByte(*third, bitsPerComponent)};
        }
    }

    std::optional<std::vector<uint8_t>>
    TileRasterDecoder::buildBitonalBmp(
        const std::vector<uint8_t> &source,
        int width,
        int height,
        const Color &background,
        const Color &foreground)
    {
        if (width <= 0 || height <= 0)
        {
            return std::nullopt;
        }

        std::size_t widthWithPadding = 0;
        if (!checkedAdd(
                static_cast<std::size_t>(width),
                std::size_t{7},
                widthWithPadding))
        {
            return std::nullopt;
        }
        const std::size_t minimumRowBytes =
            widthWithPadding / 8;
        const auto sourceRowBytes =
            resolveSourceRowBytes(
                source,
                height,
                minimumRowBytes);
        if (!sourceRowBytes)
        {
            return std::nullopt;
        }

        std::size_t rowStride = 0;
        if (!alignToFour(minimumRowBytes, rowStride))
        {
            return std::nullopt;
        }

        constexpr std::size_t paletteSize = 8;
        std::vector<uint8_t> bmp;
        std::size_t pixelOffset = 0;
        if (!initializeBmp(
                width,
                height,
                1,
                paletteSize,
                rowStride,
                bmp,
                pixelOffset))
        {
            return std::nullopt;
        }
        write32(bmp, 46, 2);
        write32(bmp, 50, 2);
        bmp[54] = background.b;
        bmp[55] = background.g;
        bmp[56] = background.r;
        bmp[58] = foreground.b;
        bmp[59] = foreground.g;
        bmp[60] = foreground.r;

        for (int row = 0; row < height; ++row)
        {
            const std::size_t sourceOffset =
                static_cast<std::size_t>(row) *
                *sourceRowBytes;
            const std::size_t targetRow =
                static_cast<std::size_t>(height - 1 - row);
            const std::size_t targetOffset =
                pixelOffset + targetRow * rowStride;
            std::copy_n(
                source.begin() +
                    static_cast<std::ptrdiff_t>(sourceOffset),
                minimumRowBytes,
                bmp.begin() +
                    static_cast<std::ptrdiff_t>(targetOffset));
        }
        return bmp;
    }

    std::optional<std::vector<uint8_t>>
    TileRasterDecoder::decodeBitmapToBmp(
        const std::vector<uint8_t> &source,
        const TileBitmapFormat &format,
        const std::map<int, Color> &colorTable)
    {
        if (format.width <= 0 || format.height <= 0)
        {
            return std::nullopt;
        }

        const int precision =
            format.local_color_precision > 0
                ? format.local_color_precision
                : (format.selection_mode ==
                           ColorSelectionMode::INDEXED
                       ? format.color_index_precision
                       : format.direct_color_precision);
        if (precision <= 0 || precision > 32)
        {
            return std::nullopt;
        }

        const int componentCount =
            format.selection_mode == ColorSelectionMode::DIRECT &&
                    format.color_model == ColorModel::CMYK
                ? 4
                : (format.selection_mode ==
                           ColorSelectionMode::DIRECT
                       ? 3
                       : 1);
        const std::size_t bitsPerPixel =
            static_cast<std::size_t>(precision) *
            static_cast<std::size_t>(componentCount);
        std::size_t rowBits = 0;
        if (!checkedMultiply(
                static_cast<std::size_t>(format.width),
                bitsPerPixel,
                rowBits))
        {
            return std::nullopt;
        }
        std::size_t paddedRowBits = 0;
        if (!checkedAdd(rowBits, 7, paddedRowBits))
        {
            return std::nullopt;
        }
        const std::size_t minimumRowBytes =
            paddedRowBits / 8;
        const auto sourceRowBytes =
            resolveSourceRowBytes(
                source,
                format.height,
                minimumRowBytes);
        if (!sourceRowBytes)
        {
            return std::nullopt;
        }

        std::size_t rawOutputRowBytes = 0;
        std::size_t outputRowStride = 0;
        if (!checkedMultiply(
                static_cast<std::size_t>(format.width),
                std::size_t{3},
                rawOutputRowBytes) ||
            !alignToFour(
                rawOutputRowBytes,
                outputRowStride))
        {
            return std::nullopt;
        }

        std::vector<uint8_t> bmp;
        std::size_t pixelOffset = 0;
        if (!initializeBmp(
                format.width,
                format.height,
                24,
                0,
                outputRowStride,
                bmp,
                pixelOffset))
        {
            return std::nullopt;
        }

        BitReader reader(source);
        for (int row = 0; row < format.height; ++row)
        {
            std::size_t rowStartBytes = 0;
            std::size_t rowStartBits = 0;
            if (!checkedMultiply(
                    static_cast<std::size_t>(row),
                    *sourceRowBytes,
                    rowStartBytes) ||
                !checkedMultiply(
                    rowStartBytes,
                    std::size_t{8},
                    rowStartBits) ||
                !reader.setPosition(rowStartBits))
            {
                return std::nullopt;
            }

            const std::size_t targetRow =
                static_cast<std::size_t>(
                    format.height - 1 - row);
            const std::size_t targetOffset =
                pixelOffset +
                targetRow * outputRowStride;
            for (int column = 0;
                 column < format.width;
                 ++column)
            {
                std::array<uint8_t, 3> rgb{};
                if (format.selection_mode ==
                    ColorSelectionMode::INDEXED)
                {
                    const auto index =
                        reader.readBits(precision);
                    if (!index ||
                        *index >
                            static_cast<uint32_t>(
                                std::numeric_limits<int>::max()))
                    {
                        return std::nullopt;
                    }
                    const auto found =
                        colorTable.find(
                            static_cast<int>(*index));
                    const Color color =
                        found != colorTable.end()
                            ? found->second
                            : Color::Black();
                    rgb = {color.r, color.g, color.b};
                }
                else
                {
                    const auto decoded =
                        readDirectColor(
                            reader,
                            format.color_model,
                            precision);
                    if (!decoded)
                    {
                        return std::nullopt;
                    }
                    rgb = *decoded;
                }

                const std::size_t target =
                    targetOffset +
                    static_cast<std::size_t>(column) * 3;
                bmp[target] = rgb[2];
                bmp[target + 1] = rgb[1];
                bmp[target + 2] = rgb[0];
            }
        }
        return bmp;
    }
}
