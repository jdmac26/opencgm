#ifndef OPENCGM_CCITT_G4_DECODER_H
#define OPENCGM_CCITT_G4_DECODER_H

#include <cstdint>
#include <vector>
#include <stdexcept>

namespace opencgm
{

/**
 * CCITT Group 4 (T.6 / MMR) bilevel image decoder
 *
 * Implements ITU-T Recommendation T.6 (Modified Modified READ) decoding
 * for bilevel images as used in ATA GREXCHANGE CGM files.
 *
 * The algorithm encodes "changing elements" (color transitions) using:
 * - Pass mode: b2 is to the left of a1
 * - Vertical mode: a1 is within 3 positions of b1
 * - Horizontal mode: a1 is more than 3 positions from b1
 */
class CCITTGroup4Decoder
{
public:
    /**
     * Decode CCITT Group 4 compressed data
     *
     * @param data       Compressed data bytes
     * @param dataLen    Length of compressed data
     * @param width      Image width in pixels
     * @param height     Image height in pixels
     * @param output     Output buffer (will be resized to width * height)
     * @return           true if decoding succeeded, false otherwise
     *
     * Output format: 0 = white, 1 = black (standard fax convention)
     */
    static bool decode(const uint8_t *data, size_t dataLen,
                       int width, int height,
                       std::vector<uint8_t> &output);

    /**
     * Exception thrown on decode errors
     */
    class DecodeError : public std::runtime_error
    {
    public:
        DecodeError(const std::string &msg) : std::runtime_error(msg) {}
    };

private:
    // Bit stream reader for compressed data
    class BitReader
    {
    public:
        BitReader(const uint8_t *data, size_t len);

        // Read next bit (0 or 1), returns -1 on EOF
        int readBit();

        // Read n bits as unsigned value
        unsigned int readBits(int n);

        // Peek at next bit without consuming
        int peekBit();

        // Check if more data available
        bool hasMore() const;

        // Get current bit position for debugging
        size_t bitPosition() const { return bytePos_ * 8 + bitPos_; }

    private:
        const uint8_t *data_;
        size_t len_;
        size_t bytePos_;
        int bitPos_; // 0-7, MSB first
    };

    // Modified Huffman run length decoder (from T.4)
    // Returns run length, or -1 on error
    static int decodeWhiteRun(BitReader &reader);
    static int decodeBlackRun(BitReader &reader);

    // Find next changing element position
    // Returns position of next color change after 'start', or 'width' if none
    static int findChangingElement(const std::vector<uint8_t> &line, int start, int width);

    // Decode mode code (Pass, Vertical, Horizontal)
    enum class Mode
    {
        PASS,
        HORIZONTAL,
        VERTICAL_0,
        VERTICAL_R1,
        VERTICAL_R2,
        VERTICAL_R3,
        VERTICAL_L1,
        VERTICAL_L2,
        VERTICAL_L3,
        EXTENSION,
        EOL,
        ERROR
    };

    static Mode decodeMode(BitReader &reader);

    // Debug flag
    static bool debug_;
};

} // namespace opencgm

#endif // OPENCGM_CCITT_G4_DECODER_H
