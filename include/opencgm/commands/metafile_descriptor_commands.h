#ifndef OPENCGM_METAFILE_DESCRIPTOR_COMMANDS_H
#define OPENCGM_METAFILE_DESCRIPTOR_COMMANDS_H

#include "../command.h"
#include "../enums.h"
#include "../cgm_color.h"
#include "../cgm_point.h"
#include <string>
#include <vector>
#include <cstdint>

namespace opencgm {

// Element ID 1: METAFILE VERSION
class MetafileVersion : public Command {
public:
    explicit MetafileVersion(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int version() const { return version_; }
    void setVersion(int version) { version_ = version; }

private:
    int version_;
};

// Element ID 2: METAFILE DESCRIPTION
class MetafileDescription : public Command {
public:
    explicit MetafileDescription(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const std::string& description() const { return description_; }
    void setDescription(const std::string& desc) { description_ = desc; }

private:
    std::string description_;
};

// Element ID 3: VDC TYPE
class VDCTypeCommand : public Command {
public:
    explicit VDCTypeCommand(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    VDCType vdcType() const { return vdcType_; }
    void setVdcType(VDCType type) { vdcType_ = type; }

private:
    VDCType vdcType_;
};

// Element ID 4: INTEGER PRECISION
class IntegerPrecision : public Command {
public:
    explicit IntegerPrecision(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int precision() const { return precision_; }
    void setPrecision(int prec) { precision_ = prec; }

private:
    int precision_;
};

// Element ID 5: REAL PRECISION
class RealPrecision : public Command {
public:
    explicit RealPrecision(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    Precision precision() const { return precision_; }
    void setPrecision(Precision prec) { precision_ = prec; }

private:
    Precision precision_;
};

// Element ID 6: INDEX PRECISION
class IndexPrecision : public Command {
public:
    explicit IndexPrecision(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int precision() const { return precision_; }
    void setPrecision(int prec) { precision_ = prec; }

private:
    int precision_;
};

// Element ID 7: COLOUR PRECISION
class ColourPrecision : public Command {
public:
    explicit ColourPrecision(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int precision() const { return precision_; }
    void setPrecision(int prec) { precision_ = prec; }

private:
    int precision_;
};

// Element ID 8: COLOUR INDEX PRECISION
class ColourIndexPrecision : public Command {
public:
    explicit ColourIndexPrecision(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int precision() const { return precision_; }
    void setPrecision(int prec) { precision_ = prec; }

private:
    int precision_;
};

// Element ID 9: MAXIMUM COLOUR INDEX
class MaximumColourIndex : public Command {
public:
    explicit MaximumColourIndex(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int maxIndex() const { return maxIndex_; }
    void setMaxIndex(int max) { maxIndex_ = max; }

private:
    int maxIndex_;
};

// Element ID 10: COLOUR VALUE EXTENT
class ColourValueExtent : public Command {
public:
    explicit ColourValueExtent(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMColor& minColor() const { return minColor_; }
    const CGMColor& maxColor() const { return maxColor_; }

private:
    CGMColor minColor_;
    CGMColor maxColor_;
};

// Element ID 11: METAFILE ELEMENT LIST
class MetafileElementList : public Command {
public:
    explicit MetafileElementList(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const std::vector<std::pair<int, int>>& elements() const { return elements_; }

private:
    std::vector<std::pair<int, int>> elements_; // pairs of (elementClass, elementId)
};

// Element ID 12: METAFILE DEFAULTS REPLACEMENT
class MetafileDefaultsReplacement : public Command {
public:
    explicit MetafileDefaultsReplacement(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

private:
    std::vector<uint8_t> defaultsData_;
};

// Element ID 13: FONT LIST
class FontList : public Command {
public:
    explicit FontList(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const std::vector<std::string>& fonts() const { return fonts_; }
    void addFont(const std::string& font) { fonts_.push_back(font); }

private:
    std::vector<std::string> fonts_;
};

// Element ID 14: CHARACTER SET LIST
class CharacterSetList : public Command {
public:
    explicit CharacterSetList(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    struct CharacterSet {
        int type; // 0 = 94-character G-set, 1 = 96-character G-set, etc.
        std::string designation;
    };

    const std::vector<CharacterSet>& characterSets() const { return characterSets_; }

private:
    std::vector<CharacterSet> characterSets_;
};

// Element ID 15: CHARACTER CODING ANNOUNCER
class CharacterCodingAnnouncer : public Command {
public:
    explicit CharacterCodingAnnouncer(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int codingType() const { return codingType_; }

private:
    int codingType_; // 0 = basic 7-bit, 1 = basic 8-bit, etc.
};

// Element ID 16: NAME PRECISION
class NamePrecision : public Command {
public:
    explicit NamePrecision(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int precision() const { return precision_; }

private:
    int precision_;
};

// Element ID 17: MAXIMUM VDC EXTENT
class MaximumVDCExtent : public Command {
public:
    explicit MaximumVDCExtent(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMPoint& firstCorner() const { return firstCorner_; }
    const CGMPoint& secondCorner() const { return secondCorner_; }
    void setExtent(const CGMPoint& first, const CGMPoint& second) {
        firstCorner_ = first;
        secondCorner_ = second;
    }

private:
    CGMPoint firstCorner_;
    CGMPoint secondCorner_;
};

// Element ID 18: SEGMENT PRIORITY EXTENT
class SegmentPriorityExtent : public Command {
public:
    explicit SegmentPriorityExtent(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int minPriority() const { return minPriority_; }
    int maxPriority() const { return maxPriority_; }

private:
    int minPriority_;
    int maxPriority_;
};

// Element ID 19: COLOUR MODEL
class ColourModel : public Command {
public:
    explicit ColourModel(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    ColorModel model() const { return model_; }

private:
    ColorModel model_;
};

// Element ID 20: COLOUR CALIBRATION
class ColourCalibration : public Command {
public:
    explicit ColourCalibration(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

private:
    std::vector<uint8_t> calibrationData_;
};

// Element ID 21: FONT PROPERTIES
class FontProperties : public Command {
public:
    explicit FontProperties(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    struct Property {
        int propertyIndicator;
        int priority;
        std::string value;
    };

private:
    std::vector<Property> properties_;
};

// Element ID 22: GLYPH MAPPING
class GlyphMapping : public Command {
public:
    explicit GlyphMapping(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

private:
    int characterSetIndex_;
    std::vector<uint8_t> mappingData_;
};

// Element ID 23: SYMBOL LIBRARY LIST
class SymbolLibraryList : public Command {
public:
    explicit SymbolLibraryList(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const std::vector<std::string>& libraries() const { return libraries_; }

private:
    std::vector<std::string> libraries_;
};

// Element ID 24: PICTURE DIRECTORY
class PictureDirectory : public Command {
public:
    enum class Type {
        UI8 = 0,
        UI16 = 1,
        UI32 = 2
    };

    struct PDInfo {
        std::string identifier;
        int location;
        int directory;
    };

    explicit PictureDirectory(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    Type type() const { return type_; }
    const std::vector<PDInfo>& infos() const { return infos_; }

private:
    Type type_;
    std::vector<PDInfo> infos_;
};

} // namespace opencgm

#endif // OPENCGM_METAFILE_DESCRIPTOR_COMMANDS_H
