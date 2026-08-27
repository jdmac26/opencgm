#include "opencgm/commands/metafile_descriptor_commands.h"
#include "opencgm/interfaces.h"
#include "opencgm/cgm_file.h"
#include "opencgm/binary_reader.h"
#include "opencgm/command_factory.h"
#include <sstream>
#include <cstring>
#include <iomanip>

namespace {

std::string formatDataRecord(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    oss << "X'";
    oss << std::uppercase << std::hex << std::setfill('0');
    for (uint8_t byte : data) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    oss << "'";
    return oss.str();
}

double readFloat32BE(const uint8_t* bytes) {
    // Map big-endian bytes to host order before memcpy into float
    uint8_t host[4];
    const uint16_t one = 1;
    const bool little = (*reinterpret_cast<const uint8_t*>(&one) == 1);
    if (little) {
        host[0] = bytes[3]; host[1] = bytes[2]; host[2] = bytes[1]; host[3] = bytes[0];
    } else {
        host[0] = bytes[0]; host[1] = bytes[1]; host[2] = bytes[2]; host[3] = bytes[3];
    }
    float value = 0.0f;
    std::memcpy(&value, host, sizeof(host));
    return static_cast<double>(value);
}

} // namespace

namespace opencgm {

// ============================================================================
// METAFILE VERSION
// ============================================================================

MetafileVersion::MetafileVersion(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 1, container)),
      version_(1) {}

void MetafileVersion::readFromBinary(IBinaryReader& reader) {
    version_ = reader.readInt();

    // Sanitize version: CGM versions are 1-4
    // If out of range, clamp to valid range with warning
    if (version_ < 1 || version_ > 4) {
        reader.unsupported("Invalid CGM version " + std::to_string(version_) +
                          ", assuming version 1");
        version_ = (version_ < 1) ? 1 : 4;
    }
}

void MetafileVersion::writeAsBinary(IBinaryWriter& writer) {
    writer.writeInt(version_);
}

void MetafileVersion::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("MFVERSION ");
    writer.writeString(std::to_string(version_));
    writer.writeString(";");
}

std::string MetafileVersion::toString() const {
    return "MetafileVersion[" + std::to_string(version_) + "]";
}

// ============================================================================
// METAFILE DESCRIPTION
// ============================================================================

MetafileDescription::MetafileDescription(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 2, container)) {}

void MetafileDescription::readFromBinary(IBinaryReader& reader) {
    description_ = reader.readString();
}

void MetafileDescription::writeAsBinary(IBinaryWriter& writer) {
    writer.writeString(description_);
}

void MetafileDescription::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("MFDESC ");
    writer.writeString(writeString(description_));
    writer.writeString(";");
}

std::string MetafileDescription::toString() const {
    return "MetafileDescription[\"" + description_ + "\"]";
}

// ============================================================================
// VDC TYPE
// ============================================================================

VDCTypeCommand::VDCTypeCommand(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 3, container)),
      vdcType_(VDCType::Integer) {}

void VDCTypeCommand::readFromBinary(IBinaryReader& reader) {
    int type = reader.readEnum();
    vdcType_ = (type == 0) ? VDCType::Integer : VDCType::Real;

    // Update container's VDC type
    if (container_) {
        container_->setVdcType(vdcType_);
    }
}

void VDCTypeCommand::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(vdcType_ == VDCType::Integer ? 0 : 1);
}

void VDCTypeCommand::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("VDCTYPE ");
    writer.writeString(vdcType_ == VDCType::Integer ? "INTEGER" : "REAL");
    writer.writeString(";");
}

std::string VDCTypeCommand::toString() const {
    return std::string("VDCType[") +
           (vdcType_ == VDCType::Integer ? "INTEGER" : "REAL") + "]";
}

// ============================================================================
// INTEGER PRECISION
// ============================================================================

IntegerPrecision::IntegerPrecision(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 4, container)),
      precision_(16) {}

void IntegerPrecision::readFromBinary(IBinaryReader& reader) {
    precision_ = reader.readInt();

    // Update container's integer precision
    if (container_) {
        container_->setIntegerPrecision(precision_);
    }
}

void IntegerPrecision::writeAsBinary(IBinaryWriter& writer) {
    writer.writeInt(precision_);
}

void IntegerPrecision::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("INTEGERPREC ");
    writer.writeString(std::to_string(precision_));
    writer.writeString(";");
}

std::string IntegerPrecision::toString() const {
    return "IntegerPrecision[" + std::to_string(precision_) + " bits]";
}

// ============================================================================
// REAL PRECISION
// ============================================================================

RealPrecision::RealPrecision(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 5, container)),
      precision_(Precision::Floating_32) {}

void RealPrecision::readFromBinary(IBinaryReader& reader) {
    // ISO 8632-3 §6.3.5: parameters are (form, exponent_or_whole_bits,
    // fraction_bits) — there is no "field width" parameter. The total bit
    // width is the SUM of the two integer parameters. WebCGM 1.0 standard
    // values are (Floating, 9, 23) → 32-bit IEEE single, and
    // (Floating, 12, 52) → 64-bit IEEE double; (Fixed, 16, 16) → 32-bit
    // fixed-point and (Fixed, 32, 32) → 64-bit fixed-point.
    int form = reader.readEnum(); // 0=floating, 1=fixed
    int expOrWholeBits = reader.readInt();
    int fractionBits = reader.readInt();
    int totalBits = expOrWholeBits + fractionBits;

    if (form == 0) { // Floating point
        precision_ = (totalBits <= 32) ? Precision::Floating_32 : Precision::Floating_64;
    } else { // Fixed point
        precision_ = (totalBits <= 32) ? Precision::Fixed_32 : Precision::Fixed_64;
    }

    // Update container's real precision
    if (container_) {
        container_->setRealPrecision(precision_);
    }
}

void RealPrecision::writeAsBinary(IBinaryWriter& writer) {
    // Determine parameters from precision
    int form = (precision_ == Precision::Floating_32 || precision_ == Precision::Floating_64) ? 0 : 1;
    int fieldWidth = (precision_ == Precision::Floating_32 || precision_ == Precision::Fixed_32) ? 32 : 64;
    int exponentWidth = (fieldWidth == 32) ? 9 : 12;

    writer.writeEnum(form);
    writer.writeInt(fieldWidth);
    writer.writeInt(exponentWidth);
}

void RealPrecision::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("REALPREC ");
    switch (precision_) {
        case Precision::Floating_32: writer.writeString("32,9,23"); break;
        case Precision::Floating_64: writer.writeString("64,12,52"); break;
        case Precision::Fixed_32: writer.writeString("FIXED 32,16,16"); break;
        case Precision::Fixed_64: writer.writeString("FIXED 64,32,32"); break;
    }
    writer.writeString(";");
}

std::string RealPrecision::toString() const {
    std::string precStr;
    switch (precision_) {
        case Precision::Floating_32: precStr = "Float32"; break;
        case Precision::Floating_64: precStr = "Float64"; break;
        case Precision::Fixed_32: precStr = "Fixed32"; break;
        case Precision::Fixed_64: precStr = "Fixed64"; break;
    }
    return "RealPrecision[" + precStr + "]";
}

// ============================================================================
// INDEX PRECISION
// ============================================================================

IndexPrecision::IndexPrecision(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 6, container)),
      precision_(16) {}

void IndexPrecision::readFromBinary(IBinaryReader& reader) {
    precision_ = reader.readInt();

    // Update container's index precision
    if (container_) {
        container_->setIndexPrecision(precision_);
    }
}

void IndexPrecision::writeAsBinary(IBinaryWriter& writer) {
    writer.writeInt(precision_);
}

void IndexPrecision::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("INDEXPREC ");
    writer.writeString(std::to_string(precision_));
    writer.writeString(";");
}

std::string IndexPrecision::toString() const {
    return "IndexPrecision[" + std::to_string(precision_) + " bits]";
}

// ============================================================================
// COLOUR PRECISION
// ============================================================================

ColourPrecision::ColourPrecision(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 7, container)),
      precision_(8) {}

void ColourPrecision::readFromBinary(IBinaryReader& reader) {
    precision_ = reader.readInt();

    // Update container's colour precision
    if (container_) {
        container_->setColourPrecision(precision_);
    }
}

void ColourPrecision::writeAsBinary(IBinaryWriter& writer) {
    writer.writeInt(precision_);
}

void ColourPrecision::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("COLRPREC ");
    writer.writeString(std::to_string(precision_));
    writer.writeString(";");
}

std::string ColourPrecision::toString() const {
    return "ColourPrecision[" + std::to_string(precision_) + " bits]";
}

// ============================================================================
// COLOUR INDEX PRECISION
// ============================================================================

ColourIndexPrecision::ColourIndexPrecision(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 8, container)),
      precision_(8) {}

void ColourIndexPrecision::readFromBinary(IBinaryReader& reader) {
    precision_ = reader.readInt();

    // Update container's colour index precision
    if (container_) {
        container_->setColourIndexPrecision(precision_);
    }
}

void ColourIndexPrecision::writeAsBinary(IBinaryWriter& writer) {
    writer.writeInt(precision_);
}

void ColourIndexPrecision::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("COLRINDEXPREC ");
    writer.writeString(std::to_string(precision_));
    writer.writeString(";");
}

std::string ColourIndexPrecision::toString() const {
    return "ColourIndexPrecision[" + std::to_string(precision_) + " bits]";
}

// ============================================================================
// MAXIMUM COLOUR INDEX
// ============================================================================

MaximumColourIndex::MaximumColourIndex(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 9, container)),
      maxIndex_(255) {}

void MaximumColourIndex::readFromBinary(IBinaryReader& reader) {
    maxIndex_ = reader.readColorIndex();
}

void MaximumColourIndex::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(maxIndex_);
}

void MaximumColourIndex::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("MAXCOLRINDEX ");
    writer.writeString(std::to_string(maxIndex_));
    writer.writeString(";");
}

std::string MaximumColourIndex::toString() const {
    return "MaximumColourIndex[" + std::to_string(maxIndex_) + "]";
}

// ============================================================================
// COLOUR VALUE EXTENT
// ============================================================================

ColourValueExtent::ColourValueExtent(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 10, container)),
      minColor_(Color(0, 0, 0)),
      maxColor_(Color(255, 255, 255)) {}

void ColourValueExtent::readFromBinary(IBinaryReader& reader) {
    minColor_ = reader.readDirectColor();
    maxColor_ = reader.readDirectColor();
}

void ColourValueExtent::writeAsBinary(IBinaryWriter& writer) {
    writer.writeColor(minColor_);
    writer.writeColor(maxColor_);
}

void ColourValueExtent::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("COLRVALUEEXT (");
    writer.writeString(std::to_string(minColor_.color().r) + "," +
                      std::to_string(minColor_.color().g) + "," +
                      std::to_string(minColor_.color().b));
    writer.writeString(") (");
    writer.writeString(std::to_string(maxColor_.color().r) + "," +
                      std::to_string(maxColor_.color().g) + "," +
                      std::to_string(maxColor_.color().b));
    writer.writeString(");");
}

std::string ColourValueExtent::toString() const {
    return "ColourValueExtent[min=(" +
           std::to_string(minColor_.color().r) + "," +
           std::to_string(minColor_.color().g) + "," +
           std::to_string(minColor_.color().b) + ") max=(" +
           std::to_string(maxColor_.color().r) + "," +
           std::to_string(maxColor_.color().g) + "," +
           std::to_string(maxColor_.color().b) + ")]";
}

// ============================================================================
// METAFILE ELEMENT LIST
// ============================================================================

MetafileElementList::MetafileElementList(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 11, container)) {}

void MetafileElementList::readFromBinary(IBinaryReader& reader) {
    while (reader.hasMoreData()) {
        int numElements = reader.readInt();
        int elementClass = reader.readIndex();

        for (int i = 0; i < numElements; i++) {
            int elementId = reader.readIndex();
            elements_.push_back({elementClass, elementId});
        }
    }
}

void MetafileElementList::writeAsBinary(IBinaryWriter& writer) {
    // Group by element class for efficient encoding
    // Simplified version: just write all elements
    for (const auto& elem : elements_) {
        writer.writeInt(1); // One element at a time
        writer.writeIndex(elem.first);
        writer.writeIndex(elem.second);
    }
}

void MetafileElementList::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("MFELEMLIST");
    for (const auto& elem : elements_) {
        writer.writeString(" (");
        writer.writeString(writeInt(elem.first));
        writer.writeString(", ");
        writer.writeString(writeInt(elem.second));
        writer.writeString(")");
    }
    writer.writeString(";");
}

std::string MetafileElementList::toString() const {
    return "MetafileElementList[" + std::to_string(elements_.size()) + " elements]";
}

// ============================================================================
// METAFILE DEFAULTS REPLACEMENT
// ============================================================================

MetafileDefaultsReplacement::MetafileDefaultsReplacement(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 12, container)) {}

void MetafileDefaultsReplacement::readFromBinary(IBinaryReader& reader) {
    // Read all remaining data as defaults replacement
    defaultsData_.clear();
    while (reader.hasMoreData()) {
        defaultsData_.push_back(static_cast<uint8_t>(reader.readChar()));
    }

    if (!container_ || defaultsData_.empty()) {
        if (container_) {
            container_->setDefaultsReplacementCommands({});
        }
        return;
    }

    std::string buffer(reinterpret_cast<const char*>(defaultsData_.data()), defaultsData_.size());
    std::istringstream stream(buffer, std::ios::binary);

    try {
        DefaultCommandFactory factory;
        BinaryCGMFile temp;
        DefaultBinaryReader defaultsReader(stream, &temp, &factory);
        defaultsReader.readCommands();

        auto& parsed = temp.commands();
        std::vector<CommandPtr> defaults;
        defaults.reserve(parsed.size());
        for (auto& cmd : parsed) {
            defaults.push_back(std::move(cmd));
        }
        parsed.clear();

        container_->setDefaultsReplacementCommands(std::move(defaults));
    } catch (const std::exception&) {
        container_->setDefaultsReplacementCommands({});
    }
}

void MetafileDefaultsReplacement::writeAsBinary(IBinaryWriter& writer) {
    for (uint8_t byte : defaultsData_) {
        writer.writeChar(static_cast<char>(byte));
    }
}

void MetafileDefaultsReplacement::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("MFDEFAULTS");
    if (!defaultsData_.empty()) {
        writer.writeString(" ");
        writer.writeString(formatDataRecord(defaultsData_));
    }
    writer.writeString(";");
}

std::string MetafileDefaultsReplacement::toString() const {
    return "MetafileDefaultsReplacement[" + std::to_string(defaultsData_.size()) + " bytes]";
}

// ============================================================================
// FONT LIST
// ============================================================================

FontList::FontList(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 13, container)) {}

void FontList::readFromBinary(IBinaryReader& reader) {
    while (reader.hasMoreData()) {
        fonts_.push_back(reader.readString());
    }
}

void FontList::writeAsBinary(IBinaryWriter& writer) {
    for (const auto& font : fonts_) {
        writer.writeString(font);
    }
}

void FontList::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("FONTLIST");
    for (const auto& font : fonts_) {
        writer.writeString(" '");
        writer.writeString(font);
        writer.writeString("'");
    }
    writer.writeString(";");
}

std::string FontList::toString() const {
    std::string result = "FontList[";
    for (size_t i = 0; i < fonts_.size(); i++) {
        if (i > 0) result += ", ";
        result += "\"" + fonts_[i] + "\"";
    }
    result += "]";
    return result;
}

// ============================================================================
// CHARACTER SET LIST
// ============================================================================

CharacterSetList::CharacterSetList(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 14, container)) {}

void CharacterSetList::readFromBinary(IBinaryReader& reader) {
    while (reader.hasMoreData()) {
        CharacterSet cs;
        cs.type = reader.readEnum();
        cs.designation = reader.readString();
        characterSets_.push_back(cs);
    }

    if (container_) {
        std::vector<std::pair<int, std::string>> sets;
        sets.reserve(characterSets_.size());
        for (const auto& cs : characterSets_) {
            sets.emplace_back(cs.type, cs.designation);
        }
        container_->setCharacterSetList(sets);
    }
}

void CharacterSetList::writeAsBinary(IBinaryWriter& writer) {
    for (const auto& cs : characterSets_) {
        writer.writeEnum(cs.type);
        writer.writeString(cs.designation);
    }
}

void CharacterSetList::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("CHARSETLIST");
    for (const auto& cs : characterSets_) {
        writer.writeString(" (");
        writer.writeString(writeEnum(cs.type));
        writer.writeString(", ");
        writer.writeString(writeString(cs.designation));
        writer.writeString(")");
    }
    writer.writeString(";");
}

std::string CharacterSetList::toString() const {
    return "CharacterSetList[" + std::to_string(characterSets_.size()) + " sets]";
}

// ============================================================================
// CHARACTER CODING ANNOUNCER
// ============================================================================

CharacterCodingAnnouncer::CharacterCodingAnnouncer(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 15, container)),
      codingType_(0) {}

void CharacterCodingAnnouncer::readFromBinary(IBinaryReader& reader) {
    codingType_ = reader.readEnum();
    if (container_) {
        container_->setCharacterCoding(codingType_);
    }
}

void CharacterCodingAnnouncer::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(codingType_);
}

void CharacterCodingAnnouncer::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("CHARCODING ");
    writer.writeString(std::to_string(codingType_));
    writer.writeString(";");
}

std::string CharacterCodingAnnouncer::toString() const {
    return "CharacterCodingAnnouncer[type=" + std::to_string(codingType_) + "]";
}

// ============================================================================
// NAME PRECISION
// ============================================================================

NamePrecision::NamePrecision(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 16, container)),
      precision_(16) {}

void NamePrecision::readFromBinary(IBinaryReader& reader) {
    precision_ = reader.readInt();
}

void NamePrecision::writeAsBinary(IBinaryWriter& writer) {
    writer.writeInt(precision_);
}

void NamePrecision::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("NAMEPREC ");
    writer.writeString(std::to_string(precision_));
    writer.writeString(";");
}

std::string NamePrecision::toString() const {
    return "NamePrecision[" + std::to_string(precision_) + " bits]";
}

// ============================================================================
// MAXIMUM VDC EXTENT
// ============================================================================

MaximumVDCExtent::MaximumVDCExtent(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 17, container)),
      firstCorner_(0, 0),
      secondCorner_(0, 0) {}

void MaximumVDCExtent::readFromBinary(IBinaryReader& reader) {
    firstCorner_ = reader.readPoint();
    secondCorner_ = reader.readPoint();
}

void MaximumVDCExtent::writeAsBinary(IBinaryWriter& writer) {
    writer.writePoint(firstCorner_);
    writer.writePoint(secondCorner_);
}

void MaximumVDCExtent::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("MAXVDCEXT (");
    writer.writeString(std::to_string(firstCorner_.x()) + "," +
                      std::to_string(firstCorner_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(secondCorner_.x()) + "," +
                      std::to_string(secondCorner_.y()));
    writer.writeString(");");
}

std::string MaximumVDCExtent::toString() const {
    return "MaximumVDCExtent[(" +
           std::to_string(firstCorner_.x()) + "," +
           std::to_string(firstCorner_.y()) + ") (" +
           std::to_string(secondCorner_.x()) + "," +
           std::to_string(secondCorner_.y()) + ")]";
}

// ============================================================================
// SEGMENT PRIORITY EXTENT
// ============================================================================

SegmentPriorityExtent::SegmentPriorityExtent(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 18, container)),
      minPriority_(0),
      maxPriority_(0) {}

void SegmentPriorityExtent::readFromBinary(IBinaryReader& reader) {
    minPriority_ = reader.readInt();
    maxPriority_ = reader.readInt();
}

void SegmentPriorityExtent::writeAsBinary(IBinaryWriter& writer) {
    writer.writeInt(minPriority_);
    writer.writeInt(maxPriority_);
}

void SegmentPriorityExtent::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("SEGPRIEXT ");
    writer.writeString(std::to_string(minPriority_));
    writer.writeString(" ");
    writer.writeString(std::to_string(maxPriority_));
    writer.writeString(";");
}

std::string SegmentPriorityExtent::toString() const {
    return "SegmentPriorityExtent[" +
           std::to_string(minPriority_) + ".." +
           std::to_string(maxPriority_) + "]";
}

// ============================================================================
// COLOUR MODEL
// ============================================================================

ColourModel::ColourModel(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 19, container)),
      model_(ColorModel::RGB) {}

void ColourModel::readFromBinary(IBinaryReader& reader) {
    int modelIndex = reader.readIndex();
    switch (modelIndex) {
        case 1: model_ = ColorModel::RGB; break;
        case 2: model_ = ColorModel::CIELAB; break;
        case 3: model_ = ColorModel::CIELUV; break;
        case 4: model_ = ColorModel::CMYK; break;
        case 5: model_ = ColorModel::RGB_RELATED; break;
        default: model_ = ColorModel::RGB; break;
    }

    if (container_) {
        container_->setColorModel(model_);
    }
}

void ColourModel::writeAsBinary(IBinaryWriter& writer) {
    int modelIndex = 1; // Default to RGB
    switch (model_) {
        case ColorModel::RGB: modelIndex = 1; break;
        case ColorModel::CIELAB: modelIndex = 2; break;
        case ColorModel::CIELUV: modelIndex = 3; break;
        case ColorModel::CMYK: modelIndex = 4; break;
        case ColorModel::RGB_RELATED: modelIndex = 5; break;
    }
    writer.writeIndex(modelIndex);
}

void ColourModel::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("COLRMODEL ");
    switch (model_) {
        case ColorModel::RGB: writer.writeString("RGB"); break;
        case ColorModel::CIELAB: writer.writeString("CIELAB"); break;
        case ColorModel::CIELUV: writer.writeString("CIELUV"); break;
        case ColorModel::CMYK: writer.writeString("CMYK"); break;
        case ColorModel::RGB_RELATED: writer.writeString("RGB-RELATED"); break;
    }
    writer.writeString(";");
}

std::string ColourModel::toString() const {
    std::string modelStr;
    switch (model_) {
        case ColorModel::RGB: modelStr = "RGB"; break;
        case ColorModel::CIELAB: modelStr = "CIELAB"; break;
        case ColorModel::CIELUV: modelStr = "CIELUV"; break;
        case ColorModel::CMYK: modelStr = "CMYK"; break;
        case ColorModel::RGB_RELATED: modelStr = "RGB_RELATED"; break;
    }
    return "ColourModel[" + modelStr + "]";
}

// ============================================================================
// COLOUR CALIBRATION
// ============================================================================

ColourCalibration::ColourCalibration(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 20, container)) {}

void ColourCalibration::readFromBinary(IBinaryReader& reader) {
    calibrationData_.clear();
    while (reader.hasMoreData()) {
        calibrationData_.push_back(static_cast<uint8_t>(reader.readChar()));
    }

    std::vector<double> floats;
    if (calibrationData_.size() % 4 == 0) {
        floats.reserve(calibrationData_.size() / 4);
        for (size_t i = 0; i < calibrationData_.size(); i += 4) {
            floats.push_back(readFloat32BE(&calibrationData_[i]));
        }
    }

    if (container_) {
        container_->setColourCalibrationData(std::move(floats));
    }
}

void ColourCalibration::writeAsBinary(IBinaryWriter& writer) {
    for (uint8_t byte : calibrationData_) {
        writer.writeChar(static_cast<char>(byte));
    }
}

void ColourCalibration::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("COLRCALIB");
    if (!calibrationData_.empty()) {
        writer.writeString(" ");
        writer.writeString(formatDataRecord(calibrationData_));
    }
    writer.writeString(";");
}

std::string ColourCalibration::toString() const {
    return "ColourCalibration[" + std::to_string(calibrationData_.size()) + " bytes]";
}

// ============================================================================
// FONT PROPERTIES
// ============================================================================

FontProperties::FontProperties(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 21, container)) {}

void FontProperties::readFromBinary(IBinaryReader& reader) {
    while (reader.hasMoreData()) {
        Property prop;
        prop.propertyIndicator = reader.readIndex();
        prop.priority = reader.readInt();
        prop.value = reader.readString();
        properties_.push_back(prop);
    }
}

void FontProperties::writeAsBinary(IBinaryWriter& writer) {
    for (const auto& prop : properties_) {
        writer.writeIndex(prop.propertyIndicator);
        writer.writeInt(prop.priority);
        writer.writeString(prop.value);
    }
}

void FontProperties::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("FONTPROP");
    for (const auto& prop : properties_) {
        writer.writeString(" (");
        writer.writeString(writeIndex(prop.propertyIndicator));
        writer.writeString(", ");
        writer.writeString(writeInt(prop.priority));
        writer.writeString(", ");
        writer.writeString(writeString(prop.value));
        writer.writeString(")");
    }
    writer.writeString(";");
}

std::string FontProperties::toString() const {
    return "FontProperties[" + std::to_string(properties_.size()) + " properties]";
}

// ============================================================================
// GLYPH MAPPING
// ============================================================================

GlyphMapping::GlyphMapping(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 22, container)),
      characterSetIndex_(0) {}

void GlyphMapping::readFromBinary(IBinaryReader& reader) {
    characterSetIndex_ = reader.readIndex();
    while (reader.hasMoreData()) {
        mappingData_.push_back(static_cast<uint8_t>(reader.readChar()));
    }
}

void GlyphMapping::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(characterSetIndex_);
    for (uint8_t byte : mappingData_) {
        writer.writeChar(static_cast<char>(byte));
    }
}

void GlyphMapping::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("GLYPHMAP ");
    writer.writeString(writeIndex(characterSetIndex_));
    if (!mappingData_.empty()) {
        writer.writeString(", ");
        writer.writeString(formatDataRecord(mappingData_));
    }
    writer.writeString(";");
}

std::string GlyphMapping::toString() const {
    return "GlyphMapping[charset=" + std::to_string(characterSetIndex_) +
           ", " + std::to_string(mappingData_.size()) + " bytes]";
}

// ============================================================================
// SYMBOL LIBRARY LIST
// ============================================================================

SymbolLibraryList::SymbolLibraryList(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 23, container)) {}

void SymbolLibraryList::readFromBinary(IBinaryReader& reader) {
    while (reader.hasMoreData()) {
        libraries_.push_back(reader.readString());
    }
}

void SymbolLibraryList::writeAsBinary(IBinaryWriter& writer) {
    for (const auto& lib : libraries_) {
        writer.writeString(lib);
    }
}

void SymbolLibraryList::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("SYMBOLLIBLIST");
    for (const auto& lib : libraries_) {
        writer.writeString(" '");
        writer.writeString(lib);
        writer.writeString("'");
    }
    writer.writeString(";");
}

std::string SymbolLibraryList::toString() const {
    return "SymbolLibraryList[" + std::to_string(libraries_.size()) + " libraries]";
}

// ============================================================================
// PICTURE DIRECTORY
// ============================================================================

PictureDirectory::PictureDirectory(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::MetafileDescriptorElements, 24, container)),
      type_(Type::UI8) {}

void PictureDirectory::readFromBinary(IBinaryReader& reader) {
    int typeValue = reader.readEnum();
    switch (typeValue) {
        case 0: type_ = Type::UI8; break;
        case 1: type_ = Type::UI16; break;
        case 2: type_ = Type::UI32; break;
        default:
            reader.unsupported("unsupported picture directory type " + std::to_string(typeValue));
            type_ = Type::UI8;
            break;
    }

    // Cast to concrete type to access readFixedString
    auto* concreteReader = dynamic_cast<DefaultBinaryReader*>(&reader);
    if (!concreteReader) {
        reader.unsupported("PictureDirectory requires DefaultBinaryReader for readFixedString");
        return;
    }

    while (reader.hasMoreData()) {
        PDInfo info;
        info.identifier = concreteReader->readFixedString();
        info.location = reader.readInt();
        info.directory = reader.readInt();
        infos_.push_back(info);
    }
}

void PictureDirectory::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(static_cast<int>(type_));

    // Note: Writing fixed strings requires special handling
    // For now, just write as regular strings
    for (const auto& info : infos_) {
        writer.writeString(info.identifier);
        writer.writeInt(info.location);
        writer.writeInt(info.directory);
    }
}

void PictureDirectory::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("PICDIR ");
    writer.writeString(writeEnum(static_cast<int>(type_)));
    for (const auto& info : infos_) {
        writer.writeString(" ");
        writer.writeString(writeString(info.identifier));
        writer.writeString(" ");
        writer.writeString(writeInt(info.location));
        writer.writeString(" ");
        writer.writeString(writeInt(info.directory));
    }
    writer.writeString(";");
}

std::string PictureDirectory::toString() const {
    return "PictureDirectory[type=" + std::to_string(static_cast<int>(type_)) +
           ", " + std::to_string(infos_.size()) + " entries]";
}

} // namespace opencgm
