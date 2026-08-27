#include "opencgm/binary_writer.h"
#include "opencgm/command.h"
#include "opencgm/binary_reader.h"
#include <stdexcept>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <cstddef>
#include <limits>

namespace opencgm {

DefaultBinaryWriter::DefaultBinaryWriter(std::ostream& stream, CGMFile* cgm)
    : stream_(stream)
    , cgm_(cgm)
    , currentCommand_(nullptr)
    , positionInCurrentArgument_(0) {
}

// ============================================================================
// Command writing
// ============================================================================

void DefaultBinaryWriter::writeCommand(Command& command) {
    Command* oldCommand = currentCommand_;
    currentCommand_ = &command;

    // Write command data to bucket
    command.writeAsBinary(*this);

    const size_t originalCount = bucket_.count();
    const bool isLongForm = originalCount >= 31;

    if (isLongForm) {
        constexpr int MAX_PARTITION_SIZE = 32767;
        const auto& rawData = bucket_.data();
        std::vector<uint8_t> partitioned;
        size_t partitionEstimate = (rawData.size() / static_cast<size_t>(MAX_PARTITION_SIZE)) + 1;
        partitioned.reserve(rawData.size() + partitionEstimate * 3);

        size_t offset = 0;
        size_t remaining = rawData.size();

        while (remaining > 0) {
            int chunk = static_cast<int>(std::min(remaining, static_cast<size_t>(MAX_PARTITION_SIZE)));
            remaining -= chunk;
            bool hasMore = remaining > 0;

            uint16_t lengthWord = static_cast<uint16_t>(chunk);
            if (hasMore) {
                lengthWord |= 0x8000;
            }

            partitioned.push_back(static_cast<uint8_t>((lengthWord >> 8) & 0xFF));
            partitioned.push_back(static_cast<uint8_t>(lengthWord & 0xFF));

            partitioned.insert(
                partitioned.end(),
                rawData.begin() + offset,
                rawData.begin() + offset + chunk);

            if (chunk % 2 == 1) {
                partitioned.push_back(0);
            }

            offset += static_cast<size_t>(chunk);
        }

        bucket_.clear();
        bucket_.addRange(partitioned);
    }

    // Write header
    uint32_t argumentField = isLongForm ? 31u : static_cast<uint32_t>(originalCount);
    writeHeader(command, argumentField);

    // Align on word boundary
    if (bucket_.count() % 2 == 1) {
        bucket_.add(0);
    }

    // Write to stream
    bucket_.saveToStream(stream_);
    bucket_.clear();

    currentCommand_ = oldCommand;
}

void DefaultBinaryWriter::writeHeader(Command& command, uint32_t argumentField) {
    uint32_t elementClass = static_cast<uint32_t>(command.elementClass()) << 12;
    uint32_t elementId = static_cast<uint32_t>(command.elementId()) << 5;
    uint32_t argumentCount = argumentField & 0x1F;

    int cmd = static_cast<int>(elementClass | elementId | argumentCount);
    writeInt16Direct(cmd);
}

void DefaultBinaryWriter::writeInt16Direct(int value) {
    stream_.put(static_cast<char>((value >> 8) & 0xff));
    stream_.put(static_cast<char>(value & 0xff));
}

// ============================================================================
// Basic writing methods
// ============================================================================

void DefaultBinaryWriter::writeByte(uint8_t data) {
    bucket_.add(data);
}

void DefaultBinaryWriter::writeChar(char data) {
    bucket_.add(static_cast<uint8_t>(data));
}

void DefaultBinaryWriter::writeSignedInt8(int data) {
    positionInCurrentArgument_ = 0;
    bucket_.add(static_cast<uint8_t>(data));
}

void DefaultBinaryWriter::writeSignedInt16(int data) {
    positionInCurrentArgument_ = 0;
    bucket_.add(static_cast<uint8_t>((data >> 8) & 0xff));
    bucket_.add(static_cast<uint8_t>(data & 0xff));
}

void DefaultBinaryWriter::writeSignedInt16(int data, size_t index) {
    positionInCurrentArgument_ = 0;
    bucket_.insert(index, static_cast<uint8_t>((data >> 8) & 0xff));
    bucket_.insert(index + 1, static_cast<uint8_t>(data & 0xff));
}

void DefaultBinaryWriter::writeSignedInt24(int data) {
    positionInCurrentArgument_ = 0;
    bucket_.add(static_cast<uint8_t>((data >> 16) & 0xff));
    bucket_.add(static_cast<uint8_t>((data >> 8) & 0xff));
    bucket_.add(static_cast<uint8_t>(data & 0xff));
}

void DefaultBinaryWriter::writeSignedInt32(int data) {
    positionInCurrentArgument_ = 0;
    bucket_.add(static_cast<uint8_t>((data >> 24) & 0xff));
    bucket_.add(static_cast<uint8_t>((data >> 16) & 0xff));
    bucket_.add(static_cast<uint8_t>((data >> 8) & 0xff));
    bucket_.add(static_cast<uint8_t>(data & 0xff));
}

void DefaultBinaryWriter::writeInt(int data, int precision) {
    switch (precision) {
        case 8:  writeSignedInt8(data); break;
        case 16: writeSignedInt16(data); break;
        case 24: writeSignedInt24(data); break;
        case 32: writeSignedInt32(data); break;
        default:
            throw std::runtime_error("Unsupported precision: " + std::to_string(precision));
    }
}

void DefaultBinaryWriter::writeUInt8(int data) {
    writeSignedInt8(data);
}

void DefaultBinaryWriter::writeUInt16(int data) {
    writeSignedInt16(data);
}

void DefaultBinaryWriter::writeUInt24(int data) {
    writeSignedInt24(data);
}

void DefaultBinaryWriter::writeUInt32(int data) {
    writeSignedInt32(data);
}

void DefaultBinaryWriter::writeUIntBit(int data, int numBits) {
    if (bucket_.count() == 0 || positionInCurrentArgument_ == 0) {
        bucket_.add(0);
        positionInCurrentArgument_ = 0;
    }

    int bitsPosition = 8 - numBits - positionInCurrentArgument_;
    uint8_t currentVal = bucket_[bucket_.count() - 1];
    bucket_[bucket_.count() - 1] = currentVal | static_cast<uint8_t>(data << bitsPosition);

    positionInCurrentArgument_ += numBits;
    if (positionInCurrentArgument_ % 8 == 0) {
        positionInCurrentArgument_ = 0;
    }
}

void DefaultBinaryWriter::writeUInt1(int data) {
    writeUIntBit(data, 1);
}

void DefaultBinaryWriter::writeUInt2(int data) {
    writeUIntBit(data, 2);
}

void DefaultBinaryWriter::writeUInt4(int data) {
    writeUIntBit(data, 4);
}

void DefaultBinaryWriter::writeUInt(int data, int precision) {
    switch (precision) {
        case 1:  writeUInt1(data); break;
        case 2:  writeUInt2(data); break;
        case 4:  writeUInt4(data); break;
        case 8:  writeUInt8(data); break;
        case 16: writeUInt16(data); break;
        case 24: writeUInt24(data); break;
        case 32: writeUInt32(data); break;
        default:
            throw std::runtime_error("Unsupported UInt precision: " + std::to_string(precision));
    }
}

// ============================================================================
// IBinaryWriter interface implementation
// ============================================================================

void DefaultBinaryWriter::writeInt(int data) {
    writeInt(data, cgm_->integerPrecision());
}

void DefaultBinaryWriter::writeEnum(int data) {
    writeSignedInt16(data);
}

void DefaultBinaryWriter::writeBool(bool data) {
    writeEnum(data ? 1 : 0);
}

void DefaultBinaryWriter::writeIndex(int data) {
    writeInt(data, cgm_->indexPrecision());
}

void DefaultBinaryWriter::writeName(int data) {
    writeInt(data, cgm_->namePrecision());
}

void DefaultBinaryWriter::writeString(const std::string& data) {
    if (!data.empty()) {
        writeDataLength(static_cast<int>(data.length()));
        bucket_.writeString(data);
    } else {
        bucket_.add(0);
    }
}

void DefaultBinaryWriter::writeFixedString(const std::string& data) {
    writeString(data);
}

void DefaultBinaryWriter::writeDataLength(int length) {
    if (length >= 255) {
        writeUInt8(255);

        int remaining = length;
        while (remaining > 0) {
            int chunk = std::min(remaining, 32767);
            remaining -= chunk;

            int value = chunk;
            if (remaining > 0) {
                value |= 0x8000; // continuation flag (bit 15)
            }

            writeUInt16(value);
        }
    } else {
        writeUInt8(length);
    }
}

// ============================================================================
// Real number writing
// ============================================================================

void DefaultBinaryWriter::writeReal(double data) {
    Precision precision = cgm_->realPrecision();

    switch (precision) {
        case Precision::Fixed_32:
            writeFixedPoint32(data);
            break;
        case Precision::Fixed_64:
            writeFixedPoint64(data);
            break;
        case Precision::Floating_32:
            writeFloatingPoint32(data);
            break;
        case Precision::Floating_64:
            writeFloatingPoint64(data);
            break;
        default:
            throw std::runtime_error("Unsupported real precision");
    }
}

void DefaultBinaryWriter::writeFixedPoint32(double value) {
    int wholePart = static_cast<int>(value);
    double fraction = value - wholePart;

    if (value < 0 && fraction != 0) {
        wholePart -= 1;
        fraction = 1.0 + fraction;
    }

    writeSignedInt16(wholePart);
    writeUInt16(static_cast<int>(fraction * DefaultBinaryReader::TWO_EX_16));
}

void DefaultBinaryWriter::writeFixedPoint64(double value) {
    int wholePart = static_cast<int>(value);
    double fraction = value - wholePart;

    if (value < 0 && fraction != 0) {
        wholePart -= 1;
        fraction = 1.0 + fraction;
    }

    writeSignedInt32(wholePart);
    writeUInt32(static_cast<int>(fraction * DefaultBinaryReader::TWO_EX_32));
}

void DefaultBinaryWriter::writeFloatingPoint32(double data) {
    float value = static_cast<float>(data);
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(float));

    // Write in big-endian order
    bucket_.add(static_cast<uint8_t>((bits >> 24) & 0xff));
    bucket_.add(static_cast<uint8_t>((bits >> 16) & 0xff));
    bucket_.add(static_cast<uint8_t>((bits >> 8) & 0xff));
    bucket_.add(static_cast<uint8_t>(bits & 0xff));
}

void DefaultBinaryWriter::writeFloatingPoint64(double value) {
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(double));

    // Write in big-endian order
    for (int i = 7; i >= 0; i--) {
        bucket_.add(static_cast<uint8_t>((bits >> (i * 8)) & 0xff));
    }
}

void DefaultBinaryWriter::writeFloatingPoint(double data) {
    if (cgm_->realPrecision() == Precision::Floating_64) {
        writeFloatingPoint64(data);
    } else {
        writeFloatingPoint32(data);
    }
}

// ============================================================================
// VDC and Point writing
// ============================================================================

void DefaultBinaryWriter::writeVdc(double data) {
    if (cgm_->vdcType() == VDCType::Real) {
        Precision precision = cgm_->vdcRealPrecision();

        switch (precision) {
            case Precision::Fixed_32:
                writeFixedPoint32(data);
                break;
            case Precision::Fixed_64:
                writeFixedPoint64(data);
                break;
            case Precision::Floating_32:
                writeFloatingPoint32(data);
                break;
            case Precision::Floating_64:
                writeFloatingPoint64(data);
                break;
            default:
                throw std::runtime_error("Unsupported VDC real precision");
        }
    } else {
        int precision = cgm_->vdcIntegerPrecision();
        int value = static_cast<int>(data);

        switch (precision) {
            case 16: writeSignedInt16(value); break;
            case 24: writeSignedInt24(value); break;
            case 32: writeSignedInt32(value); break;
            default:
                throw std::runtime_error("Unsupported VDC integer precision: " +
                                       std::to_string(precision));
        }
    }
}

void DefaultBinaryWriter::writePoint(const CGMPoint& point) {
    writeVdc(point.x());
    writeVdc(point.y());
}

void DefaultBinaryWriter::writeSizeSpecification(double data, SpecificationMode mode) {
    if (mode == SpecificationMode::ABS) {
        writeVdc(data);
    } else {
        writeReal(data);
    }
}

// ============================================================================
// Color writing
// ============================================================================

void DefaultBinaryWriter::writeColorIndex(int index) {
    writeColorIndex(index, cgm_->colourIndexPrecision());
}

void DefaultBinaryWriter::writeColorIndex(int index, int localColorPrecision) {
    int precision = (localColorPrecision <= 0) ? cgm_->colourIndexPrecision() : localColorPrecision;
    writeUInt(index, precision);
}

void DefaultBinaryWriter::writeDirectColor(const Color& color, int localColorPrecision) {
    int precision = (localColorPrecision > 0) ? localColorPrecision : cgm_->colourPrecision();

    // Only RGB is represented internally; if color model is not RGB, we still write RGB components.
    auto scaled = scaleColorValueRGB(color.r, color.g, color.b, precision);
    writeUInt(scaled[0], precision);
    writeUInt(scaled[1], precision);
    writeUInt(scaled[2], precision);
}

void DefaultBinaryWriter::writeColor(const CGMColor& color, int localColorPrecision) {
    // Check the ColorSelectionMode to determine how to write the color
    if (cgm_->colorSelectionMode() == ColorSelectionMode::DIRECT) {
        writeDirectColor(color.color(), localColorPrecision);
    } else {
        // INDEXED mode - write color index if available, fallback to direct if not
        if (color.colorIndex() >= 0) {
            writeColorIndex(color.colorIndex(), localColorPrecision);
        } else {
            // Fallback: write as direct color if no index available
            writeDirectColor(color.color(), localColorPrecision);
        }
    }
}

std::array<int, 3> DefaultBinaryWriter::scaleColorValueRGB(int r, int g, int b, int precisionBits) {
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
        int clamped = std::clamp(value, 0, 255);
        if (maxValue == 255) {
            return clamped;
        }
        long long scaled = static_cast<long long>(clamped) * maxValue + 127;
        return static_cast<int>(scaled / 255);
    };

    return {convertComponent(r), convertComponent(g), convertComponent(b)};
}

// ============================================================================
// Helper methods
// ============================================================================

void DefaultBinaryWriter::fillToWord() {
    if (bucket_.count() % 2 == 0 && positionInCurrentArgument_ > 0) {
        bucket_.add(0);
        bucket_.add(0);
        positionInCurrentArgument_ = 0;
    } else if (bucket_.count() % 2 == 1) {
        bucket_.add(0);
        positionInCurrentArgument_ = 0;
    }
}

void DefaultBinaryWriter::writeEmbeddedCommand([[maybe_unused]] Command& command) {
    // Create temporary stream and writer
    std::vector<uint8_t> tempBuffer;
    // TODO: Implement embedded command writing
    // For now, just add empty data
    bucket_.addRange(tempBuffer);
}

void DefaultBinaryWriter::unsupported(const std::string& message) {
    if (currentCommand_) {
        messages_.emplace_back(
            Severity::Unsupported,
            currentCommand_->elementClass(),
            currentCommand_->elementId(),
            message,
            currentCommand_->toString()
        );
    }
}

} // namespace opencgm
