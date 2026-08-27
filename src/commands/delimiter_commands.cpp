#include "opencgm/commands/delimiter_commands.h"
#include "opencgm/interfaces.h"
#include "opencgm/binary_reader.h"
#include "opencgm/utils/sdr_parser.h"
#include <iostream>
#include "command_macros.h"

namespace opencgm {

// ============================================================================
// BEGIN METAFILE
// ============================================================================

IMPL_STRING_COMMAND(BeginMetafile, DelimiterElement, 1, "BEGMF", name_)

// ============================================================================
// END METAFILE
// ============================================================================

IMPL_EMPTY_COMMAND(EndMetafile, DelimiterElement, 2, "ENDMF")

// ============================================================================
// BEGIN PICTURE
// ============================================================================

IMPL_STRING_COMMAND(BeginPicture, DelimiterElement, 3, "BEGPIC", name_)

// ============================================================================
// BEGIN PICTURE BODY
// ============================================================================

IMPL_EMPTY_COMMAND(BeginPictureBody, DelimiterElement, 4, "BEGPICBODY")

// ============================================================================
// END PICTURE
// ============================================================================

IMPL_EMPTY_COMMAND(EndPicture, DelimiterElement, 5, "ENDPIC")

// ============================================================================
// BEGIN SEGMENT
// ============================================================================

IMPL_INT_COMMAND(BeginSegment, DelimiterElement, 6, "BEGSEG", segmentIdentifier_)

// ============================================================================
// END SEGMENT
// ============================================================================

IMPL_EMPTY_COMMAND(EndSegment, DelimiterElement, 7, "ENDSEG")

// ============================================================================
// BEGIN FIGURE
// ============================================================================

IMPL_EMPTY_COMMAND(BeginFigure, DelimiterElement, 8, "BEGFIG")

// ============================================================================
// END FIGURE
// ============================================================================

IMPL_EMPTY_COMMAND(EndFigure, DelimiterElement, 9, "ENDFIG")

// ============================================================================
// BEGIN PROTECTION REGION
// ============================================================================

IMPL_INT_COMMAND(BeginProtectionRegion, DelimiterElement, 13, "BEGPRTRGN", regionIndex_)

// ============================================================================
// END PROTECTION REGION
// ============================================================================

IMPL_EMPTY_COMMAND(EndProtectionRegion, DelimiterElement, 14, "ENDPRTRGN")

// ============================================================================
// BEGIN COMPOUND LINE
// ============================================================================

IMPL_EMPTY_COMMAND(BeginCompoundLine, DelimiterElement, 15, "BEGCPDLN")

// ============================================================================
// END COMPOUND LINE
// ============================================================================

IMPL_EMPTY_COMMAND(EndCompoundLine, DelimiterElement, 16, "ENDCPDLN")

// ============================================================================
// BEGIN COMPOUND TEXT PATH
// ============================================================================

IMPL_EMPTY_COMMAND(BeginCompoundTextPath, DelimiterElement, 17, "BEGCPDTXTPTH")

// ============================================================================
// END COMPOUND TEXT PATH
// ============================================================================

IMPL_EMPTY_COMMAND(EndCompoundTextPath, DelimiterElement, 18, "ENDCPDTXTPTH")

// ============================================================================
// BEGIN TILE ARRAY
// ============================================================================

BeginTileArray::BeginTileArray(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::DelimiterElement, 19, container)),
      position_(0, 0),
      cellPathDirection_(0),
      lineProgressionDirection_(0),
      nTilesInPathDirection_(0),
      nTilesInLineDirection_(0),
      nCellsPerTileInPathDirection_(0),
      nCellsPerTileInLineDirection_(0),
      cellSizeInPathDirection_(0.0),
      cellSizeInLineDirection_(0.0),
      imageOffsetInPathDirection_(0),
      imageOffsetInLineDirection_(0),
      imageCellsInPathDirection_(0),
      imageCellsInLineDirection_(0) {}

void BeginTileArray::readFromBinary(IBinaryReader& reader) {
    position_ = reader.readPoint();
    cellPathDirection_ = reader.readEnum();
    lineProgressionDirection_ = reader.readEnum();
    nTilesInPathDirection_ = reader.readInt();
    nTilesInLineDirection_ = reader.readInt();
    nCellsPerTileInPathDirection_ = reader.readInt();
    nCellsPerTileInLineDirection_ = reader.readInt();
    // ISO 8632-1 §5.4.5: cell-size parameters are type R (REAL), not VDC.
    // The asymmetry with the writer (which already uses writeReal) silently
    // misaligned the next IX fields whenever VDC TYPE differed in width from
    // REAL precision (e.g. VDC=int16, REAL=float32 in WebCGM 1.0 monochrome
    // tile-array CGMs), pushing the image origin tens of thousands of units
    // off-canvas.
    cellSizeInPathDirection_ = reader.readReal();
    cellSizeInLineDirection_ = reader.readReal();
    imageOffsetInPathDirection_ = reader.readInt();
    imageOffsetInLineDirection_ = reader.readInt();
    imageCellsInPathDirection_ = reader.readInt();
    imageCellsInLineDirection_ = reader.readInt();
}

void BeginTileArray::writeAsBinary(IBinaryWriter& writer) {
    writer.writePoint(position_);
    writer.writeEnum(cellPathDirection_);
    writer.writeEnum(lineProgressionDirection_);
    writer.writeInt(nTilesInPathDirection_);
    writer.writeInt(nTilesInLineDirection_);
    writer.writeInt(nCellsPerTileInPathDirection_);
    writer.writeInt(nCellsPerTileInLineDirection_);
    writer.writeReal(cellSizeInPathDirection_);
    writer.writeReal(cellSizeInLineDirection_);
    writer.writeInt(imageOffsetInPathDirection_);
    writer.writeInt(imageOffsetInLineDirection_);
    writer.writeInt(imageCellsInPathDirection_);
    writer.writeInt(imageCellsInLineDirection_);
}

void BeginTileArray::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("BEGTILEARRAY ");
    writer.writeString(writePoint(position_.x(), position_.y()));
    writer.writeString(", ");
    writer.writeString(writeEnum(cellPathDirection_));
    writer.writeString(", ");
    writer.writeString(writeEnum(lineProgressionDirection_));
    writer.writeString(", ");
    writer.writeString(writeInt(nTilesInPathDirection_));
    writer.writeString(", ");
    writer.writeString(writeInt(nTilesInLineDirection_));
    writer.writeString(", ");
    writer.writeString(writeInt(nCellsPerTileInPathDirection_));
    writer.writeString(", ");
    writer.writeString(writeInt(nCellsPerTileInLineDirection_));
    writer.writeString(", ");
    writer.writeString(writeReal(cellSizeInPathDirection_));
    writer.writeString(", ");
    writer.writeString(writeReal(cellSizeInLineDirection_));
    writer.writeString(", ");
    writer.writeString(writeInt(imageOffsetInPathDirection_));
    writer.writeString(", ");
    writer.writeString(writeInt(imageOffsetInLineDirection_));
    writer.writeString(", ");
    writer.writeString(writeInt(imageCellsInPathDirection_));
    writer.writeString(", ");
    writer.writeString(writeInt(imageCellsInLineDirection_));
    writer.writeString(";");
}

std::string BeginTileArray::toString() const {
    return "BeginTileArray[pos=" + std::to_string(position_.x()) + "," +
           std::to_string(position_.y()) + " tiles=" +
           std::to_string(nTilesInPathDirection_) + "x" +
           std::to_string(nTilesInLineDirection_) + "]";
}

// ============================================================================
// END TILE ARRAY
// ============================================================================

IMPL_EMPTY_COMMAND(EndTileArray, DelimiterElement, 20, "ENDTILEARRAY")

// ============================================================================
// BEGIN APPLICATION STRUCTURE
// ============================================================================

BeginApplicationStructure::BeginApplicationStructure(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::DelimiterElement, 21, container)),
      inheritanceFlag_(false) {}

void BeginApplicationStructure::readFromBinary(IBinaryReader& reader) {
    if (auto *concrete = dynamic_cast<DefaultBinaryReader*>(&reader)) {
        std::string rawId = concrete->readRawString();
        if (auto decodedId = SDRParser::decodeStructuredText(rawId)) {
            identifier_ = *decodedId;
        } else {
            identifier_ = concrete->decodeText(rawId);
        }

        std::string rawType = concrete->readRawString();
        if (auto decodedType = SDRParser::decodeStructuredText(rawType)) {
            type_ = *decodedType;
        } else {
            type_ = concrete->decodeText(rawType);
        }
    } else {
        identifier_ = reader.readString();
        type_ = reader.readString();
    }
    inheritanceFlag_ = (reader.readEnum() == 0); // 0=inherit, 1=new
}

void BeginApplicationStructure::writeAsBinary(IBinaryWriter& writer) {
    writer.writeString(identifier_);
    writer.writeString(type_);
    writer.writeEnum(inheritanceFlag_ ? 0 : 1);
}

void BeginApplicationStructure::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("BEGAPS ");
    writer.writeString(writeString(identifier_));
    writer.writeString(", ");
    writer.writeString(writeString(type_));
    writer.writeString(", ");
    writer.writeString(inheritanceFlag_ ? "INHERIT" : "NEW");
    writer.writeString(";");
}

std::string BeginApplicationStructure::toString() const {
    return "BeginApplicationStructure[id=\"" + identifier_ +
           "\", type=\"" + type_ + "\", inherit=" +
           (inheritanceFlag_ ? "true" : "false") + "]";
}

// ============================================================================
// BEGIN APPLICATION STRUCTURE BODY
// ============================================================================

IMPL_EMPTY_COMMAND(BeginApplicationStructureBody, DelimiterElement, 22, "BEGAPSBODY")

// ============================================================================
// END APPLICATION STRUCTURE
// ============================================================================

IMPL_EMPTY_COMMAND(EndApplicationStructure, DelimiterElement, 23, "ENDAPS")

} // namespace opencgm
