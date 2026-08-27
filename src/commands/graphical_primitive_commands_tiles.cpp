#include "opencgm/commands/graphical_primitive_commands.h"
#include "opencgm/interfaces.h"
#include "opencgm/cgm_file.h"
#include "opencgm/cgm_color.h"
#include "opencgm/binary_reader.h"
#include "opencgm/security_limits.h"
#include "opencgm/utils/sdr_parser.h"
#include <stdexcept>

namespace opencgm {

// ============================================================================
// GENERALIZED DRAWING PRIMITIVE (GDP)
// ============================================================================

GeneralizedDrawingPrimitive::GeneralizedDrawingPrimitive(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 10, container)),
      identifier_(0), dataRecord_("") {}

void GeneralizedDrawingPrimitive::readFromBinary(IBinaryReader& reader) {
    identifier_ = reader.readInt();
    int numberOfPoints = reader.readInt();
    if (numberOfPoints < 0) {
        throw std::runtime_error("Negative GDP point count");
    }
    security::validateAllocationSize(
        static_cast<size_t>(numberOfPoints),
        security::MAX_POLYGON_POINTS,
        "GDP points");

    points_.clear();
    points_.reserve(numberOfPoints);
    for (int i = 0; i < numberOfPoints; i++) {
        points_.push_back(reader.readPoint());
    }

    if (auto* concreteReader = dynamic_cast<DefaultBinaryReader*>(&reader)) {
        dataRecord_ = concreteReader->readRawString();
    } else {
        dataRecord_ = reader.readString();
    }
}

void GeneralizedDrawingPrimitive::writeAsBinary(IBinaryWriter& writer) {
    writer.writeInt(identifier_);
    writer.writeInt(static_cast<int>(points_.size()));
    for (const auto& p : points_) {
        writer.writePoint(p);
    }
    writer.writeString(dataRecord_);
}

void GeneralizedDrawingPrimitive::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString(" GDP ");
    writer.writeString(writeInt(identifier_));

    for (const auto& p : points_) {
        writer.writeString(" ");
        writer.writeString(writePoint(p.x(), p.y()));
    }

    writer.writeString(" ");
    writer.writeString(writeString(dataRecord_));
    writer.writeString(";\n");
}

std::string GeneralizedDrawingPrimitive::toString() const {
    return "GeneralizedDrawingPrimitive[id=" + std::to_string(identifier_) + ", points=" + std::to_string(points_.size()) + "]";
}

// ============================================================================
// POLYSYMBOL
// ============================================================================

PolySymbol::PolySymbol(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 27, container)),
      index_(0) {}

void PolySymbol::readFromBinary(IBinaryReader& reader) {
    index_ = reader.readIndex();

    points_.clear();
    while (reader.hasMoreData()) {
        security::validateAllocationSize(
            points_.size() + 1,
            security::MAX_POLYGON_POINTS,
            "PolySymbol points");
        points_.push_back(reader.readPoint());
    }
}

void PolySymbol::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(index_);
    for (const auto& p : points_) {
        writer.writePoint(p);
    }
}

void PolySymbol::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString(" SYMBOL ");
    writer.writeString(writeIndex(index_));

    for (const auto& p : points_) {
        writer.writeString(" ");
        writer.writeString(writePoint(p.x(), p.y()));
    }

    writer.writeString(";\n");
}

std::string PolySymbol::toString() const {
    return "PolySymbol[index=" + std::to_string(index_) + ", points=" + std::to_string(points_.size()) + "]";
}

// ============================================================================
// TILE ELEMENT (Base Class)
// ============================================================================

TileElement::TileElement(const CommandConstructorArguments& args)
    : Command(args), compressionType_(0), rowPaddingIndicator_(0), dataRecord_("") {}

void TileElement::readSdrAndBitStream(IBinaryReader& reader) {
    // Read structured data record (simplified as string)
    if (auto* concreteReader = dynamic_cast<DefaultBinaryReader*>(&reader)) {
        dataRecord_ = concreteReader->readRawString();
    } else {
        dataRecord_ = reader.readString();
    }

    // Best-effort parse of SDR for width/height and auxiliary hints
    bitmapWidth_ = 0;
    bitmapHeight_ = 0;
    const auto sdrInfo = SDRParser::parseTileSdr(dataRecord_);
    if (sdrInfo.ok) {
        bitmapWidth_ = sdrInfo.width;
        bitmapHeight_ = sdrInfo.height;
        if (sdrInfo.localColorPrecision > 0) {
            // Prefer explicit per-tile precision if present (used by Tile)
            // Derived classes may override with their own fields
        }
        if (sdrInfo.rowPadding >= 0) {
            // Do not override explicit rowPaddingIndicator_ set by stream
        }
    }

    // Read image data based on compression type
    if (compressionType_ == 6 || compressionType_ == 7 || compressionType_ == 9) { // JPEG, PNG, PNG method 0
        imageData_.clear();
        while (reader.hasMoreData()) {
            imageData_.push_back(reader.readByte());
        }
    } else if (compressionType_ == 5 || compressionType_ == 2) { // BITMAP or CCITT
        readBitmap(reader);
    } else {
        // Unknown compression type – preserve payload for diagnostics
        imageData_.clear();
        while (reader.hasMoreData()) {
            imageData_.push_back(reader.readByte());
        }
    }
}

void TileElement::writeSdrAndBitStream(IBinaryWriter& writer) {
    writer.writeString(dataRecord_);

    if (compressionType_ == 6 || compressionType_ == 7 || compressionType_ == 9) { // JPEG, PNG, PNG method 0
        for (uint8_t byte : imageData_) {
            writer.writeByte(byte);
        }
    } else if (compressionType_ == 5 || compressionType_ == 2) { // BITMAP or CCITT
        writeBitmap(writer);
    }
}

// ============================================================================
// BITONAL TILE
// ============================================================================

BitonalTile::BitonalTile(CGMFile* container)
    : TileElement(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 28, container)),
      backgroundColor_(Color::White()), foregroundColor_(Color::Black()) {}

void BitonalTile::readFromBinary(IBinaryReader& reader) {
    compressionType_ = reader.readIndex();
    rowPaddingIndicator_ = reader.readInt();
    backgroundColor_ = reader.readColor();
    foregroundColor_ = reader.readColor();
    readSdrAndBitStream(reader);
}

void BitonalTile::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(compressionType_);
    writer.writeInt(rowPaddingIndicator_);
    writer.writeColor(backgroundColor_);
    writer.writeColor(foregroundColor_);
    writeSdrAndBitStream(writer);
}

void BitonalTile::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString(" BITONALTILE ");
    writer.writeString(writeInt(compressionType_));
    writer.writeString(" ");
    writer.writeString(writeInt(rowPaddingIndicator_));
    writer.writeString(" ");
    writer.writeString(backgroundColor_.toString());
    writer.writeString(" ");
    writer.writeString(foregroundColor_.toString());
    writer.writeString(" ");
    writer.writeString(writeString(dataRecord_));
    // Note: bitstream not emitted in clear text path (binary only)
    writer.writeString(";\n");
}

std::string BitonalTile::toString() const {
    return "BitonalTile[compressionType=" + std::to_string(compressionType_) + "]";
}

void BitonalTile::readBitmap(IBinaryReader& reader) {
    // Read raw CGM bitstream bytes (compression type 5: BITMAP)
    imageData_.clear();
    while (reader.hasMoreData()) {
        imageData_.push_back(reader.readByte());
    }
}

void BitonalTile::writeBitmap(IBinaryWriter& /* writer */) {
    // Bitmap writing for BitonalTile - currently unsupported
}

// ============================================================================
// TILE
// ============================================================================

Tile::Tile(CGMFile* container)
    : TileElement(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 29, container)),
      cellColorPrecision_(0) {}

void Tile::readFromBinary(IBinaryReader& reader) {
    compressionType_ = reader.readIndex();
    rowPaddingIndicator_ = reader.readInt();
    cellColorPrecision_ = reader.readInt();
    readSdrAndBitStream(reader);
}

void Tile::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(compressionType_);
    writer.writeInt(rowPaddingIndicator_);
    writer.writeInt(cellColorPrecision_);
    writeSdrAndBitStream(writer);
}

void Tile::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString(" TILE ");
    writer.writeString(writeInt(compressionType_));
    writer.writeString(" ");
    writer.writeString(writeInt(rowPaddingIndicator_));
    writer.writeString(" ");
    writer.writeString(writeInt(cellColorPrecision_));
    writer.writeString(" ");
    writer.writeString(writeString(dataRecord_));
    // Note: bitstream not emitted in clear text path (binary only)
    writer.writeString(";\n");
}

std::string Tile::toString() const {
    return "Tile[compressionType=" + std::to_string(compressionType_) + ", rowPadding=" + std::to_string(rowPaddingIndicator_) + "]";
}

void Tile::readBitmap(IBinaryReader& reader) {
    // Read raw CGM bitstream bytes (compression type 5: BITMAP)
    imageData_.clear();
    while (reader.hasMoreData()) {
        imageData_.push_back(reader.readByte());
    }
}

void Tile::writeBitmap(IBinaryWriter& /* writer */) {
    // Bitmap writing for Tile - currently unsupported
}

} // namespace opencgm
