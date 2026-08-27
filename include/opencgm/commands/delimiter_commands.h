#ifndef OPENCGM_DELIMITER_COMMANDS_H
#define OPENCGM_DELIMITER_COMMANDS_H

#include "../command.h"
#include "../cgm_point.h"
#include <string>
#include <vector>

namespace opencgm {

// Element ID 1: BEGIN METAFILE
class BeginMetafile : public Command {
public:
    explicit BeginMetafile(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const std::string& name() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

private:
    std::string name_;
};

// Element ID 2: END METAFILE
class EndMetafile : public Command {
public:
    explicit EndMetafile(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;
};

// Element ID 3: BEGIN PICTURE
class BeginPicture : public Command {
public:
    explicit BeginPicture(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const std::string& name() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

private:
    std::string name_;
};

// Element ID 4: BEGIN PICTURE BODY
class BeginPictureBody : public Command {
public:
    explicit BeginPictureBody(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;
};

// Element ID 5: END PICTURE
class EndPicture : public Command {
public:
    explicit EndPicture(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;
};

// Element ID 6: BEGIN SEGMENT
class BeginSegment : public Command {
public:
    explicit BeginSegment(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int segmentIdentifier() const { return segmentIdentifier_; }
    void setSegmentIdentifier(int id) { segmentIdentifier_ = id; }

private:
    int segmentIdentifier_;
};

// Element ID 7: END SEGMENT
class EndSegment : public Command {
public:
    explicit EndSegment(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;
};

// Element ID 8: BEGIN FIGURE
class BeginFigure : public Command {
public:
    explicit BeginFigure(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;
};

// Element ID 9: END FIGURE
class EndFigure : public Command {
public:
    explicit EndFigure(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;
};

// Element ID 13: BEGIN PROTECTION REGION
class BeginProtectionRegion : public Command {
public:
    explicit BeginProtectionRegion(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int regionIndex() const { return regionIndex_; }
    void setRegionIndex(int index) { regionIndex_ = index; }

private:
    int regionIndex_;
};

// Element ID 14: END PROTECTION REGION
class EndProtectionRegion : public Command {
public:
    explicit EndProtectionRegion(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;
};

// Element ID 15: BEGIN COMPOUND LINE
class BeginCompoundLine : public Command {
public:
    explicit BeginCompoundLine(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;
};

// Element ID 16: END COMPOUND LINE
class EndCompoundLine : public Command {
public:
    explicit EndCompoundLine(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;
};

// Element ID 17: BEGIN COMPOUND TEXT PATH
class BeginCompoundTextPath : public Command {
public:
    explicit BeginCompoundTextPath(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;
};

// Element ID 18: END COMPOUND TEXT PATH
class EndCompoundTextPath : public Command {
public:
    explicit EndCompoundTextPath(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;
};

// Element ID 19: BEGIN TILE ARRAY
class BeginTileArray : public Command {
public:
    explicit BeginTileArray(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMPoint& position() const { return position_; }
    int cellPathDirection() const { return cellPathDirection_; }
    int lineProgressionDirection() const { return lineProgressionDirection_; }
    int nTilesInPathDirection() const { return nTilesInPathDirection_; }
    int nTilesInLineDirection() const { return nTilesInLineDirection_; }
    int nCellsPerTileInPathDirection() const { return nCellsPerTileInPathDirection_; }
    int nCellsPerTileInLineDirection() const { return nCellsPerTileInLineDirection_; }
    double cellSizeInPathDirection() const { return cellSizeInPathDirection_; }
    double cellSizeInLineDirection() const { return cellSizeInLineDirection_; }
    int imageOffsetInPathDirection() const { return imageOffsetInPathDirection_; }
    int imageOffsetInLineDirection() const { return imageOffsetInLineDirection_; }
    int imageCellsInPathDirection() const { return imageCellsInPathDirection_; }
    int imageCellsInLineDirection() const { return imageCellsInLineDirection_; }

private:
    CGMPoint position_;
    int cellPathDirection_;
    int lineProgressionDirection_;
    int nTilesInPathDirection_;
    int nTilesInLineDirection_;
    int nCellsPerTileInPathDirection_;
    int nCellsPerTileInLineDirection_;
    double cellSizeInPathDirection_;
    double cellSizeInLineDirection_;
    int imageOffsetInPathDirection_;
    int imageOffsetInLineDirection_;
    int imageCellsInPathDirection_;
    int imageCellsInLineDirection_;
};

// Element ID 20: END TILE ARRAY
class EndTileArray : public Command {
public:
    explicit EndTileArray(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;
};

// Element ID 21: BEGIN APPLICATION STRUCTURE
class BeginApplicationStructure : public Command {
public:
    explicit BeginApplicationStructure(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const std::string& identifier() const { return identifier_; }
    const std::string& type() const { return type_; }
    bool inheritanceFlag() const { return inheritanceFlag_; }

private:
    std::string identifier_;
    std::string type_;
    bool inheritanceFlag_;
};

// Element ID 22: BEGIN APPLICATION STRUCTURE BODY
class BeginApplicationStructureBody : public Command {
public:
    explicit BeginApplicationStructureBody(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;
};

// Element ID 23: END APPLICATION STRUCTURE
class EndApplicationStructure : public Command {
public:
    explicit EndApplicationStructure(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;
};

} // namespace opencgm

#endif // OPENCGM_DELIMITER_COMMANDS_H