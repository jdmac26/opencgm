#include "opencgm/cgm_file.h"
#include "opencgm/binary_reader.h"
#include "opencgm/binary_writer.h"
#include "opencgm/command_factory.h"
#include "opencgm/commands/delimiter_commands.h"
#include "opencgm/utils/gzip_utils.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <vector>

namespace opencgm {

CGMFile::CGMFile() {
    resetMetaDefinitions();
}

void CGMFile::addMessage(const Message& message) {
    messages_.push_back(message);
}

void CGMFile::resetMetaDefinitions() {
    colourIndexPrecision_ = 8;
    colourPrecision_ = 8;
    indexPrecision_ = 16;
    integerPrecision_ = 16;
    namePrecision_ = 16;
    vdcIntegerPrecision_ = 16;
    vdcRealPrecision_ = Precision::Fixed_32;
    realPrecision_ = Precision::Fixed_32;
    realPrecisionProcessed_ = false;
    edgeWidthSpecificationMode_ = SpecificationMode::ABS;
    lineWidthSpecificationMode_ = SpecificationMode::ABS;
    markerSizeSpecificationMode_ = SpecificationMode::ABS;
    interiorStyleSpecificationMode_ = SpecificationMode::ABS;
    vdcType_ = VDCType::Integer;
    colorModel_ = ColorModel::RGB;
    scalingMode_ = SpecificationMode::ABS;
    metricScaleFactor_ = 1.0;
    colorSelectionMode_ = ColorSelectionMode::INDEXED;
    vdcExtentFirstCorner_ = CGMPoint(0.0, 0.0);
    vdcExtentSecondCorner_ = CGMPoint(1.0, 1.0);
    characterCodingAnnouncer_ = 1;
    characterSetIndex_ = 1;
    alternateCharacterSetIndex_ = 1;
    characterSets_.clear();
    unsupportedCharsets_.clear();
    illegalControls_.clear();
    colourCalibrationData_.clear();
    defaultsReplacementCommands_.clear();
}

// BinaryCGMFile implementation

BinaryCGMFile::BinaryCGMFile() {
    name_ = "new";
}

BinaryCGMFile::BinaryCGMFile(const std::string& fileName)
    : fileName_(fileName) {
    // Extract name from path
    size_t pos = fileName.find_last_of("/\\");
    name_ = (pos == std::string::npos) ? fileName : fileName.substr(pos + 1);

    readData(fileName);
}

BinaryCGMFile::BinaryCGMFile(std::istream& stream, const std::string& name) {
    name_ = name;
    readData(stream);
}

void BinaryCGMFile::writeFile(const std::string& fileName) {
    std::ofstream stream(fileName, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Failed to open file for writing: " + fileName);
    }
    writeFile(stream);
}

void BinaryCGMFile::writeFile(std::ostream& stream) {
    if (!stream) {
        throw std::runtime_error("Invalid output stream for CGM writing");
    }

    try {
        DefaultBinaryWriter writer(stream, this);
        for (auto& command : commands_) {
            if (command) {
                writer.writeCommand(*command);
            }
        }
    }
    catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to write CGM file: ") + e.what());
    }
}

void BinaryCGMFile::readData(std::istream& stream) {
    resetMetaDefinitions();

    // Create command factory
    DefaultCommandFactory factory;

    // Create binary reader and parse the file
    DefaultBinaryReader reader(stream, this, &factory);
    reader.readCommands();

    if (!defaultsReplacementCommands_.empty()) {
        // Insert defaults replacement after MF descriptors and BEGMF,
        // but before any picture content per ISO/IEC 8632-1.
        size_t insertPos = 0;
        // Skip initial BEGMF if present
        while (insertPos < commands_.size()) {
            const auto& cmd = commands_[insertPos];
            if (cmd->elementClass() == ClassCode::DelimiterElement && cmd->elementId() == 1) {
                ++insertPos; // BEGMF
                break;
            }
            break;
        }
        // Skip all Metafile Descriptor Elements
        while (insertPos < commands_.size() &&
               commands_[insertPos]->elementClass() == ClassCode::MetafileDescriptorElements) {
            ++insertPos;
        }

        commands_.insert(commands_.begin() + static_cast<std::ptrdiff_t>(insertPos),
            std::make_move_iterator(defaultsReplacementCommands_.begin()),
            std::make_move_iterator(defaultsReplacementCommands_.end()));
        defaultsReplacementCommands_.clear();
    }
}

void BinaryCGMFile::readData(const std::string& fileName) {
    std::ifstream stream(fileName, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Failed to open file: " + fileName);
    }

    uint8_t header[2] = {0, 0};
    stream.read(reinterpret_cast<char*>(header), sizeof(header));
    std::streamsize readCount = stream.gcount();
    stream.clear();
    stream.seekg(0, std::ios::beg);

    if (readCount == 2 && isGzipStream(header, sizeof(header))) {
        std::vector<uint8_t> compressed((std::istreambuf_iterator<char>(stream)),
                                        std::istreambuf_iterator<char>());
        std::vector<uint8_t> decompressed;
        if (!gzipDecompress(compressed.data(), compressed.size(), decompressed)) {
            throw std::runtime_error("Failed to decompress CGM stream: " + fileName);
        }
        std::string buffer(reinterpret_cast<const char*>(decompressed.data()), decompressed.size());
        std::istringstream decompressedStream(buffer);
        readData(decompressedStream);
        return;
    }

    readData(stream);
}

void CGMFile::reportUnsupportedCharset(const std::string& designation) const {
    if (!designation.empty()) {
        unsupportedCharsets_.insert(designation);
    }
}

void CGMFile::reportIllegalControl(uint32_t codePoint) const {
    illegalControls_.insert(codePoint);
}

std::vector<PictureRange> CGMFile::getPictureRanges() const {
    std::vector<PictureRange> ranges;
    size_t currentStart = 0;
    std::string currentName;
    bool inPicture = false;

    for (size_t i = 0; i < commands_.size(); ++i) {
        const auto& cmd = commands_[i];
        if (cmd->elementClass() == ClassCode::DelimiterElement) {
            if (cmd->elementId() == 3) {  // BEGIN PICTURE
                inPicture = true;
                currentStart = i;
                if (auto* bp = dynamic_cast<BeginPicture*>(cmd.get())) {
                    currentName = bp->name();
                } else {
                    currentName.clear();
                }
            } else if (cmd->elementId() == 5 && inPicture) {  // END PICTURE
                PictureRange range;
                range.startCommandIndex = currentStart;
                range.endCommandIndex = i;
                range.name = currentName;
                ranges.push_back(range);
                inPicture = false;
            }
        }
    }

    return ranges;
}

} // namespace opencgm
