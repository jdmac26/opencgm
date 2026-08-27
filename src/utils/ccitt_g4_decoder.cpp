#include "opencgm/utils/ccitt_g4_decoder.h"
#include <cstring>
#include <iostream>
#include <iomanip>
#include <cstdlib>

namespace opencgm
{

bool CCITTGroup4Decoder::debug_ = false;

// =============================================================================
// BitReader implementation
// =============================================================================

CCITTGroup4Decoder::BitReader::BitReader(const uint8_t *data, size_t len)
    : data_(data), len_(len), bytePos_(0), bitPos_(0)
{
}

int CCITTGroup4Decoder::BitReader::readBit()
{
    if (bytePos_ >= len_)
        return -1;

    // MSB first (bit 7 is first) - standard CCITT G4 bit ordering
    int bit = (data_[bytePos_] >> (7 - bitPos_)) & 1;
    bitPos_++;
    if (bitPos_ >= 8)
    {
        bitPos_ = 0;
        bytePos_++;
    }
    return bit;
}

unsigned int CCITTGroup4Decoder::BitReader::readBits(int n)
{
    unsigned int value = 0;
    for (int i = 0; i < n; i++)
    {
        int bit = readBit();
        if (bit < 0)
            return value; // EOF
        value = (value << 1) | bit;
    }
    return value;
}

int CCITTGroup4Decoder::BitReader::peekBit()
{
    if (bytePos_ >= len_)
        return -1;
    return (data_[bytePos_] >> (7 - bitPos_)) & 1;
}

bool CCITTGroup4Decoder::BitReader::hasMore() const
{
    return bytePos_ < len_;
}

// =============================================================================
// Modified Huffman Tables (from ITU-T T.4)
// =============================================================================

// White run terminating codes (0-63)
// Format: {code, length, run}
struct HuffmanEntry
{
    unsigned int code;
    int length;
    int run;
};

// White terminating codes (run lengths 0-63)
static const HuffmanEntry whiteTermCodes[] = {
    {0x35, 8, 0},   // 00110101
    {0x07, 6, 1},   // 000111
    {0x07, 4, 2},   // 0111
    {0x08, 4, 3},   // 1000
    {0x0B, 4, 4},   // 1011
    {0x0C, 4, 5},   // 1100
    {0x0E, 4, 6},   // 1110
    {0x0F, 4, 7},   // 1111
    {0x13, 5, 8},   // 10011
    {0x14, 5, 9},   // 10100
    {0x07, 5, 10},  // 00111
    {0x08, 5, 11},  // 01000
    {0x08, 6, 12},  // 001000
    {0x03, 6, 13},  // 000011
    {0x34, 6, 14},  // 110100
    {0x35, 6, 15},  // 110101
    {0x2A, 6, 16},  // 101010
    {0x2B, 6, 17},  // 101011
    {0x27, 7, 18},  // 0100111
    {0x0C, 7, 19},  // 0001100
    {0x08, 7, 20},  // 0001000
    {0x17, 7, 21},  // 0010111
    {0x03, 7, 22},  // 0000011
    {0x04, 7, 23},  // 0000100
    {0x28, 7, 24},  // 0101000
    {0x2B, 7, 25},  // 0101011
    {0x13, 7, 26},  // 0010011
    {0x24, 7, 27},  // 0100100
    {0x18, 7, 28},  // 0011000
    {0x02, 8, 29},  // 00000010
    {0x03, 8, 30},  // 00000011
    {0x1A, 8, 31},  // 00011010
    {0x1B, 8, 32},  // 00011011
    {0x12, 8, 33},  // 00010010
    {0x13, 8, 34},  // 00010011
    {0x14, 8, 35},  // 00010100
    {0x15, 8, 36},  // 00010101
    {0x16, 8, 37},  // 00010110
    {0x17, 8, 38},  // 00010111
    {0x28, 8, 39},  // 00101000
    {0x29, 8, 40},  // 00101001
    {0x2A, 8, 41},  // 00101010
    {0x2B, 8, 42},  // 00101011
    {0x2C, 8, 43},  // 00101100
    {0x2D, 8, 44},  // 00101101
    {0x04, 8, 45},  // 00000100
    {0x05, 8, 46},  // 00000101
    {0x0A, 8, 47},  // 00001010
    {0x0B, 8, 48},  // 00001011
    {0x52, 8, 49},  // 01010010
    {0x53, 8, 50},  // 01010011
    {0x54, 8, 51},  // 01010100
    {0x55, 8, 52},  // 01010101
    {0x24, 8, 53},  // 00100100
    {0x25, 8, 54},  // 00100101
    {0x58, 8, 55},  // 01011000
    {0x59, 8, 56},  // 01011001
    {0x5A, 8, 57},  // 01011010
    {0x5B, 8, 58},  // 01011011
    {0x4A, 8, 59},  // 01001010
    {0x4B, 8, 60},  // 01001011
    {0x32, 8, 61},  // 00110010
    {0x33, 8, 62},  // 00110011
    {0x34, 8, 63},  // 00110100
};

// White make-up codes (run lengths 64, 128, ... 1728, 1792, 1856, 1920, 1984, 2048, 2112, 2176, 2240, 2304, 2368, 2432, 2496, 2560)
static const HuffmanEntry whiteMakeupCodes[] = {
    {0x1B, 5, 64},     // 11011
    {0x12, 5, 128},    // 10010
    {0x17, 6, 192},    // 010111
    {0x37, 7, 256},    // 0110111
    {0x36, 8, 320},    // 00110110
    {0x37, 8, 384},    // 00110111
    {0x64, 8, 448},    // 01100100
    {0x65, 8, 512},    // 01100101
    {0x68, 8, 576},    // 01101000
    {0x67, 8, 640},    // 01100111
    {0xCC, 9, 704},    // 011001100
    {0xCD, 9, 768},    // 011001101
    {0xD2, 9, 832},    // 011010010
    {0xD3, 9, 896},    // 011010011
    {0xD4, 9, 960},    // 011010100
    {0xD5, 9, 1024},   // 011010101
    {0xD6, 9, 1088},   // 011010110
    {0xD7, 9, 1152},   // 011010111
    {0xD8, 9, 1216},   // 011011000
    {0xD9, 9, 1280},   // 011011001
    {0xDA, 9, 1344},   // 011011010
    {0xDB, 9, 1408},   // 011011011
    {0x98, 9, 1472},   // 010011000
    {0x99, 9, 1536},   // 010011001
    {0x9A, 9, 1600},   // 010011010
    {0x18, 6, 1664},   // 011000
    {0x9B, 9, 1728},   // 010011011
    // Extended codes (common to white and black)
    {0x08, 11, 1792},  // 00000001000
    {0x0C, 11, 1856},  // 00000001100
    {0x0D, 11, 1920},  // 00000001101
    {0x12, 12, 1984},  // 000000010010
    {0x13, 12, 2048},  // 000000010011
    {0x14, 12, 2112},  // 000000010100
    {0x15, 12, 2176},  // 000000010101
    {0x16, 12, 2240},  // 000000010110
    {0x17, 12, 2304},  // 000000010111
    {0x1C, 12, 2368},  // 000000011100
    {0x1D, 12, 2432},  // 000000011101
    {0x1E, 12, 2496},  // 000000011110
    {0x1F, 12, 2560},  // 000000011111
};

// Black terminating codes (0-63)
static const HuffmanEntry blackTermCodes[] = {
    {0x37, 10, 0},    // 0000110111
    {0x02, 3, 1},     // 010
    {0x03, 2, 2},     // 11
    {0x02, 2, 3},     // 10
    {0x03, 3, 4},     // 011
    {0x03, 4, 5},     // 0011
    {0x02, 4, 6},     // 0010
    {0x03, 5, 7},     // 00011
    {0x05, 6, 8},     // 000101
    {0x04, 6, 9},     // 000100
    {0x04, 7, 10},    // 0000100
    {0x05, 7, 11},    // 0000101
    {0x07, 7, 12},    // 0000111
    {0x04, 8, 13},    // 00000100
    {0x07, 8, 14},    // 00000111
    {0x18, 9, 15},    // 000011000
    {0x17, 10, 16},   // 0000010111
    {0x18, 10, 17},   // 0000011000
    {0x08, 10, 18},   // 0000001000
    {0x67, 11, 19},   // 00001100111
    {0x68, 11, 20},   // 00001101000
    {0x6C, 11, 21},   // 00001101100
    {0x37, 11, 22},   // 00000110111
    {0x28, 11, 23},   // 00000101000
    {0x17, 11, 24},   // 00000010111
    {0x18, 11, 25},   // 00000011000
    {0xCA, 12, 26},   // 000011001010
    {0xCB, 12, 27},   // 000011001011
    {0xCC, 12, 28},   // 000011001100
    {0xCD, 12, 29},   // 000011001101
    {0x68, 12, 30},   // 000001101000
    {0x69, 12, 31},   // 000001101001
    {0x6A, 12, 32},   // 000001101010
    {0x6B, 12, 33},   // 000001101011
    {0xD2, 12, 34},   // 000011010010
    {0xD3, 12, 35},   // 000011010011
    {0xD4, 12, 36},   // 000011010100
    {0xD5, 12, 37},   // 000011010101
    {0xD6, 12, 38},   // 000011010110
    {0xD7, 12, 39},   // 000011010111
    {0x6C, 12, 40},   // 000001101100
    {0x6D, 12, 41},   // 000001101101
    {0xDA, 12, 42},   // 000011011010
    {0xDB, 12, 43},   // 000011011011
    {0x54, 12, 44},   // 000001010100
    {0x55, 12, 45},   // 000001010101
    {0x56, 12, 46},   // 000001010110
    {0x57, 12, 47},   // 000001010111
    {0x64, 12, 48},   // 000001100100
    {0x65, 12, 49},   // 000001100101
    {0x52, 12, 50},   // 000001010010
    {0x53, 12, 51},   // 000001010011
    {0x24, 12, 52},   // 000000100100
    {0x37, 12, 53},   // 000000110111
    {0x38, 12, 54},   // 000000111000
    {0x27, 12, 55},   // 000000100111
    {0x28, 12, 56},   // 000000101000
    {0x58, 12, 57},   // 000001011000
    {0x59, 12, 58},   // 000001011001
    {0x2B, 12, 59},   // 000000101011
    {0x2C, 12, 60},   // 000000101100
    {0x5A, 12, 61},   // 000001011010
    {0x66, 12, 62},   // 000001100110
    {0x67, 12, 63},   // 000001100111
};

// Black make-up codes
static const HuffmanEntry blackMakeupCodes[] = {
    {0x0F, 10, 64},    // 0000001111
    {0xC8, 12, 128},   // 000011001000
    {0xC9, 12, 192},   // 000011001001
    {0x5B, 12, 256},   // 000001011011
    {0x33, 12, 320},   // 000000110011
    {0x34, 12, 384},   // 000000110100
    {0x35, 12, 448},   // 000000110101
    {0x6C, 13, 512},   // 0000001101100
    {0x6D, 13, 576},   // 0000001101101
    {0x4A, 13, 640},   // 0000001001010
    {0x4B, 13, 704},   // 0000001001011
    {0x4C, 13, 768},   // 0000001001100
    {0x4D, 13, 832},   // 0000001001101
    {0x72, 13, 896},   // 0000001110010
    {0x73, 13, 960},   // 0000001110011
    {0x74, 13, 1024},  // 0000001110100
    {0x75, 13, 1088},  // 0000001110101
    {0x76, 13, 1152},  // 0000001110110
    {0x77, 13, 1216},  // 0000001110111
    {0x52, 13, 1280},  // 0000001010010
    {0x53, 13, 1344},  // 0000001010011
    {0x54, 13, 1408},  // 0000001010100
    {0x55, 13, 1472},  // 0000001010101
    {0x5A, 13, 1536},  // 0000001011010
    {0x5B, 13, 1600},  // 0000001011011
    {0x64, 13, 1664},  // 0000001100100
    {0x65, 13, 1728},  // 0000001100101
    // Extended codes (same as white)
    {0x08, 11, 1792},
    {0x0C, 11, 1856},
    {0x0D, 11, 1920},
    {0x12, 12, 1984},
    {0x13, 12, 2048},
    {0x14, 12, 2112},
    {0x15, 12, 2176},
    {0x16, 12, 2240},
    {0x17, 12, 2304},
    {0x1C, 12, 2368},
    {0x1D, 12, 2432},
    {0x1E, 12, 2496},
    {0x1F, 12, 2560},
};

// =============================================================================
// Huffman decoding
// =============================================================================

int CCITTGroup4Decoder::decodeWhiteRun(BitReader &reader)
{
    int totalRun = 0;

    // First check for make-up code
    while (true)
    {
        // Try make-up codes first (they have longer codes for large runs)
        unsigned int code = 0;
        int bitsRead = 0;
        bool foundMakeup = false;

        // Read up to 13 bits for make-up codes
        while (bitsRead < 13 && reader.hasMore())
        {
            int bit = reader.readBit();
            if (bit < 0)
                break;

            code = (code << 1) | bit;
            bitsRead++;

            // Check make-up table
            int tableSize = sizeof(whiteMakeupCodes) / sizeof(whiteMakeupCodes[0]);
            for (int i = 0; i < tableSize; i++)
            {
                if (whiteMakeupCodes[i].length == bitsRead &&
                    whiteMakeupCodes[i].code == code)
                {
                    totalRun += whiteMakeupCodes[i].run;
                    foundMakeup = true;
                    break;
                }
            }
            if (foundMakeup)
                break;
        }

        if (!foundMakeup)
        {
            // Need to restore position and try terminating code
            // Since we can't easily restore, we'll handle this differently
            break;
        }
    }

    // Now read terminating code
    unsigned int code = 0;
    int bitsRead = 0;

    while (bitsRead < 13 && reader.hasMore())
    {
        int bit = reader.readBit();
        if (bit < 0)
            return totalRun > 0 ? totalRun : -1;

        code = (code << 1) | bit;
        bitsRead++;

        // Check terminating table
        int tableSize = sizeof(whiteTermCodes) / sizeof(whiteTermCodes[0]);
        for (int i = 0; i < tableSize; i++)
        {
            if (whiteTermCodes[i].length == bitsRead &&
                whiteTermCodes[i].code == code)
            {
                return totalRun + whiteTermCodes[i].run;
            }
        }

        // Also check make-up codes (for runs > 63)
        tableSize = sizeof(whiteMakeupCodes) / sizeof(whiteMakeupCodes[0]);
        for (int i = 0; i < tableSize; i++)
        {
            if (whiteMakeupCodes[i].length == bitsRead &&
                whiteMakeupCodes[i].code == code)
            {
                totalRun += whiteMakeupCodes[i].run;
                // After make-up, need to read terminating
                code = 0;
                bitsRead = 0;
                break;
            }
        }
    }

    return totalRun > 0 ? totalRun : -1;
}

int CCITTGroup4Decoder::decodeBlackRun(BitReader &reader)
{
    int totalRun = 0;

    // Read terminating or make-up code
    unsigned int code = 0;
    int bitsRead = 0;

    while (bitsRead < 13 && reader.hasMore())
    {
        int bit = reader.readBit();
        if (bit < 0)
            return totalRun > 0 ? totalRun : -1;

        code = (code << 1) | bit;
        bitsRead++;

        // Check terminating table first
        int tableSize = sizeof(blackTermCodes) / sizeof(blackTermCodes[0]);
        for (int i = 0; i < tableSize; i++)
        {
            if (blackTermCodes[i].length == bitsRead &&
                blackTermCodes[i].code == code)
            {
                return totalRun + blackTermCodes[i].run;
            }
        }

        // Check make-up codes
        tableSize = sizeof(blackMakeupCodes) / sizeof(blackMakeupCodes[0]);
        for (int i = 0; i < tableSize; i++)
        {
            if (blackMakeupCodes[i].length == bitsRead &&
                blackMakeupCodes[i].code == code)
            {
                totalRun += blackMakeupCodes[i].run;
                // After make-up, need to read terminating
                code = 0;
                bitsRead = 0;
                break;
            }
        }
    }

    return totalRun > 0 ? totalRun : -1;
}

// =============================================================================
// Mode decoding
// =============================================================================

CCITTGroup4Decoder::Mode CCITTGroup4Decoder::decodeMode(BitReader &reader)
{
    // Read bits to determine mode
    // V(0):     1
    // VL(1):    010
    // VR(1):    011
    // H:        001
    // P:        0001
    // VL(2):    000010
    // VR(2):    000011
    // VL(3):    0000010
    // VR(3):    0000011
    // Extension:0000001xxx

    int bit = reader.readBit();
    if (bit < 0)
        return Mode::ERROR;

    if (bit == 1)
    {
        return Mode::VERTICAL_0;
    }

    // bit == 0
    bit = reader.readBit();
    if (bit < 0)
        return Mode::ERROR;

    if (bit == 1)
    {
        // 01x
        bit = reader.readBit();
        if (bit < 0)
            return Mode::ERROR;
        if (bit == 0)
            return Mode::VERTICAL_L1;
        else
            return Mode::VERTICAL_R1;
    }

    // 00x
    bit = reader.readBit();
    if (bit < 0)
        return Mode::ERROR;

    if (bit == 1)
    {
        // 001 = Horizontal
        return Mode::HORIZONTAL;
    }

    // 000x
    bit = reader.readBit();
    if (bit < 0)
        return Mode::ERROR;

    if (bit == 1)
    {
        // 0001 = Pass
        return Mode::PASS;
    }

    // 0000xx
    bit = reader.readBit();
    if (bit < 0)
        return Mode::ERROR;

    if (bit == 1)
    {
        // 00001x
        bit = reader.readBit();
        if (bit < 0)
            return Mode::ERROR;
        if (bit == 0)
            return Mode::VERTICAL_L2;
        else
            return Mode::VERTICAL_R2;
    }

    // 00000x
    bit = reader.readBit();
    if (bit < 0)
        return Mode::ERROR;

    if (bit == 1)
    {
        // 000001x
        bit = reader.readBit();
        if (bit < 0)
            return Mode::ERROR;
        if (bit == 0)
            return Mode::VERTICAL_L3;
        else
            return Mode::VERTICAL_R3;
    }

    // 000000x - could be extension or EOL
    bit = reader.readBit();
    if (bit < 0)
        return Mode::ERROR;

    if (bit == 1)
    {
        // 0000001 = Extension
        return Mode::EXTENSION;
    }

    // More zeros - could be EOL (000000000001)
    // For now, treat as error
    return Mode::ERROR;
}

// =============================================================================
// Find changing element
// =============================================================================

int CCITTGroup4Decoder::findChangingElement(const std::vector<uint8_t> &line,
                                             int start, int width)
{
    if (start >= width)
        return width;

    uint8_t startColor = (start < 0) ? 0 : line[start];

    for (int i = start + 1; i < width; i++)
    {
        if (line[i] != startColor)
            return i;
    }
    return width;
}

// =============================================================================
// Main decoder
// =============================================================================

bool CCITTGroup4Decoder::decode(const uint8_t *data, size_t dataLen,
                                 int width, int height,
                                 std::vector<uint8_t> &output)
{
    debug_ = std::getenv("SVG_DEBUG_G4") != nullptr;

    if (width <= 0 || height <= 0 || data == nullptr || dataLen == 0)
    {
        return false;
    }

    output.resize(width * height, 0);

    // Reference line (starts as all white / zeros)
    std::vector<uint8_t> refLine(width, 0);
    std::vector<uint8_t> curLine(width, 0);

    BitReader reader(data, dataLen);

    if (debug_)
    {
        std::cerr << "[g4] Starting decode: " << width << "x" << height
                  << ", dataLen=" << dataLen << "\n";
        // Show first 16 bytes in hex
        std::cerr << "[g4] First bytes: ";
        for (size_t i = 0; i < std::min(dataLen, (size_t)16); i++) {
            std::cerr << std::hex << std::setfill('0') << std::setw(2) << (int)data[i] << " ";
        }
        std::cerr << std::dec << "\n";
        // Show first 16 bits
        std::cerr << "[g4] First 16 bits (LSB-first from each byte): ";
        for (int i = 0; i < 16 && i/8 < (int)dataLen; i++) {
            int byteIdx = i / 8;
            int bitIdx = i % 8;
            int bit = (data[byteIdx] >> bitIdx) & 1;
            std::cerr << bit;
        }
        std::cerr << "\n";
    }

    for (int row = 0; row < height; row++)
    {
        // Reset current line to white
        std::fill(curLine.begin(), curLine.end(), 0);

        int a0 = -1;        // Current position on coding line (-1 = imaginary start)
        uint8_t a0Color = 0; // Color at a0 (starts white)

        while (a0 < width)
        {
            // Find b1: first changing element on reference line to the right of a0
            int b1 = a0 + 1;
            while (b1 < width && refLine[b1] == a0Color)
            {
                b1++;
            }
            if (b1 < width)
            {
                // b1 is at a color change, find where the color we want starts
                while (b1 < width && refLine[b1] != (1 - a0Color))
                {
                    b1++;
                }
            }

            // Actually: b1 should be first changing element to the right of a0
            // with opposite color to a0Color
            b1 = a0 + 1;
            if (b1 < 0) b1 = 0;
            while (b1 < width)
            {
                if (refLine[b1] != a0Color)
                    break;
                b1++;
            }

            // Find b2: next changing element after b1
            int b2 = b1 + 1;
            if (b1 < width)
            {
                while (b2 < width && refLine[b2] == refLine[b1])
                {
                    b2++;
                }
            }
            else
            {
                b2 = width;
            }

            Mode mode = decodeMode(reader);

            if (debug_ && row < 5)
            {
                std::cerr << "[g4] row=" << row << " a0=" << a0
                          << " b1=" << b1 << " b2=" << b2
                          << " mode=" << (int)mode << "\n";
            }

            switch (mode)
            {
            case Mode::PASS:
            {
                // Move a0 to position under b2
                a0 = b2;
                // Color doesn't change
                break;
            }
            case Mode::HORIZONTAL:
            {
                // Read two run lengths
                int run1, run2;
                if (a0Color == 0)
                {
                    run1 = decodeWhiteRun(reader);
                    run2 = decodeBlackRun(reader);
                }
                else
                {
                    run1 = decodeBlackRun(reader);
                    run2 = decodeWhiteRun(reader);
                }

                if (run1 < 0 || run2 < 0)
                {
                    if (debug_)
                    {
                        std::cerr << "[g4] Horizontal mode decode failed at row "
                                  << row << ", run1=" << run1 << ", run2=" << run2
                                  << ", a0=" << a0 << ", a0Color=" << (int)a0Color << "\n";
                    }
                    return false;
                }

                // Fill first run with current color
                int startPos = (a0 < 0) ? 0 : a0;
                for (int i = 0; i < run1 && startPos + i < width; i++)
                {
                    curLine[startPos + i] = a0Color;
                }

                // Fill second run with opposite color
                for (int i = 0; i < run2 && startPos + run1 + i < width; i++)
                {
                    curLine[startPos + run1 + i] = 1 - a0Color;
                }

                a0 = startPos + run1 + run2;
                // Color stays as it was (we've just done two complete runs)
                break;
            }
            case Mode::VERTICAL_0:
            case Mode::VERTICAL_R1:
            case Mode::VERTICAL_R2:
            case Mode::VERTICAL_R3:
            case Mode::VERTICAL_L1:
            case Mode::VERTICAL_L2:
            case Mode::VERTICAL_L3:
            {
                int offset = 0;
                switch (mode)
                {
                case Mode::VERTICAL_0:
                    offset = 0;
                    break;
                case Mode::VERTICAL_R1:
                    offset = 1;
                    break;
                case Mode::VERTICAL_R2:
                    offset = 2;
                    break;
                case Mode::VERTICAL_R3:
                    offset = 3;
                    break;
                case Mode::VERTICAL_L1:
                    offset = -1;
                    break;
                case Mode::VERTICAL_L2:
                    offset = -2;
                    break;
                case Mode::VERTICAL_L3:
                    offset = -3;
                    break;
                default:
                    break;
                }

                // a1 = b1 + offset
                int a1 = b1 + offset;
                if (a1 < 0)
                    a1 = 0;
                if (a1 > width)
                    a1 = width;

                // Fill from a0+1 to a1-1 with current color
                int startPos = (a0 < 0) ? 0 : a0;
                for (int i = startPos; i < a1 && i < width; i++)
                {
                    curLine[i] = a0Color;
                }

                a0 = a1;
                a0Color = 1 - a0Color; // Toggle color
                break;
            }
            case Mode::EXTENSION:
                // Skip extension (read 3 more bits and ignore)
                reader.readBits(3);
                break;

            case Mode::EOL:
            case Mode::ERROR:
            default:
                // End of line or error
                if (debug_ && mode == Mode::ERROR)
                {
                    std::cerr << "[g4] Decode error at row " << row
                              << ", a0=" << a0 << "\n";
                }
                // Fill rest with current color and move to next line
                for (int i = (a0 < 0 ? 0 : a0); i < width; i++)
                {
                    curLine[i] = a0Color;
                }
                a0 = width; // Force exit
                break;
            }

            if (a0 >= width)
                break;
        }

        // Copy current line to output
        std::memcpy(&output[row * width], curLine.data(), width);

        // Current line becomes reference line for next row
        std::swap(refLine, curLine);
    }

    if (debug_)
    {
        // Count non-white pixels
        int blackCount = 0;
        for (size_t i = 0; i < output.size(); i++)
        {
            if (output[i] != 0)
                blackCount++;
        }
        std::cerr << "[g4] Decode complete: " << blackCount << "/"
                  << output.size() << " black pixels\n";
    }

    return true;
}

} // namespace opencgm
