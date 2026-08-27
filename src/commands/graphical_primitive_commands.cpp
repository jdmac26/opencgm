#include "opencgm/commands/graphical_primitive_commands.h"
#include "opencgm/interfaces.h"
#include "opencgm/cgm_file.h"
#include "opencgm/security_limits.h"
#include <sstream>
#include <cstdint>

namespace opencgm {

// ============================================================================
// POINT-BASED COMMAND IMPLEMENTATION HELPERS
// ============================================================================
// Macros to reduce boilerplate for commands that store/process point lists
//
// NOTE: These macros codify the common pattern seen in Polyline, DisjointPolyline,
// Polymarker, Polygon, etc. While we keep the existing implementations as-is for
// stability, these macros document the standard pattern and can be used for:
// - New point-based commands
// - Code review reference
// - Understanding the standard implementation pattern
//
// Standard pattern for point-based primitives:
// 1. Read: Loop reader.readPoint() while hasMoreData()
// 2. Write binary: Loop writer.writePoint() for each point
// 3. Write clear text: Output command name + formatted points + semicolon
// 4. toString: Return "ClassName[N points]"

#define IMPLEMENT_POINT_COMMAND_READ_BINARY() \
    void readFromBinary(IBinaryReader& reader) override { \
        while (reader.hasMoreData()) { \
            points_.push_back(reader.readPoint()); \
        } \
    }

#define IMPLEMENT_POINT_COMMAND_WRITE_BINARY() \
    void writeAsBinary(IBinaryWriter& writer) override { \
        for (const auto& point : points_) { \
            writer.writePoint(point); \
        } \
    }

#define IMPLEMENT_POINT_COMMAND_WRITE_CLEARTEXT(CMD_NAME, CLASS_NAME) \
    void writeAsClearText(IClearTextWriter& writer) override { \
        writer.writeString(CMD_NAME); \
        for (const auto& point : points_) { \
            writer.writeString(" ("); \
            writer.writeString(std::to_string(point.x()) + "," + std::to_string(point.y())); \
            writer.writeString(")"); \
        } \
        writer.writeString(";"); \
    } \
    std::string toString() const override { \
        return CLASS_NAME "[" + std::to_string(points_.size()) + " points]"; \
    }

namespace {
    inline void enforcePointLimit(size_t count, const char* elementName);
}

// ============================================================================
// POLYLINE
// ============================================================================

Polyline::Polyline(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 1, container)) {}

void Polyline::readFromBinary(IBinaryReader& reader) {
    while (reader.hasMoreData()) {
        enforcePointLimit(points_.size() + 1, "Polyline points");
        points_.push_back(reader.readPoint());
    }
}

void Polyline::writeAsBinary(IBinaryWriter& writer) {
    for (const auto& point : points_) {
        writer.writePoint(point);
    }
}

void Polyline::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("LINE");
    for (const auto& point : points_) {
        writer.writeString(" (");
        writer.writeString(std::to_string(point.x()) + "," + std::to_string(point.y()));
        writer.writeString(")");
    }
    writer.writeString(";");
}

std::string Polyline::toString() const {
    return "Polyline[" + std::to_string(points_.size()) + " points]";
}

namespace {
    inline void enforcePointLimit(size_t count, const char* elementName) {
        security::validateAllocationSize(count, security::MAX_POLYGON_POINTS, elementName);
    }

    static std::string escapeClearTextString(const std::string &s) {
        std::string out;
        out.reserve(s.size() + 4);
        for (char c : s) {
            if (c == '\'') {
                out.push_back('\'');
                out.push_back('\'');
            } else if (static_cast<unsigned char>(c) < 0x20 && c != '\t' && c != '\n' && c != '\r') {
                // skip disallowed control characters
            } else {
                out.push_back(c);
            }
        }
        return out;
    }
}

// ============================================================================
// DISJOINT POLYLINE
// ============================================================================

DisjointPolyline::DisjointPolyline(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 2, container)) {}

void DisjointPolyline::readFromBinary(IBinaryReader& reader) {
    while (reader.hasMoreData()) {
        enforcePointLimit(points_.size() + 1, "DisjointPolyline points");
        points_.push_back(reader.readPoint());
    }
}

void DisjointPolyline::writeAsBinary(IBinaryWriter& writer) {
    for (const auto& point : points_) {
        writer.writePoint(point);
    }
}

void DisjointPolyline::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("DISJTLINE");
    for (const auto& point : points_) {
        writer.writeString(" (");
        writer.writeString(std::to_string(point.x()) + "," + std::to_string(point.y()));
        writer.writeString(")");
    }
    writer.writeString(";");
}

std::string DisjointPolyline::toString() const {
    return "DisjointPolyline[" + std::to_string(points_.size()) + " points]";
}

// ============================================================================
// POLYMARKER
// ============================================================================

Polymarker::Polymarker(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 3, container)) {}

void Polymarker::readFromBinary(IBinaryReader& reader) {
    while (reader.hasMoreData()) {
        enforcePointLimit(points_.size() + 1, "Polymarker points");
        points_.push_back(reader.readPoint());
    }
}

void Polymarker::writeAsBinary(IBinaryWriter& writer) {
    for (const auto& point : points_) {
        writer.writePoint(point);
    }
}

void Polymarker::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("MARKER");
    for (const auto& point : points_) {
        writer.writeString(" (");
        writer.writeString(std::to_string(point.x()) + "," + std::to_string(point.y()));
        writer.writeString(")");
    }
    writer.writeString(";");
}

std::string Polymarker::toString() const {
    return "Polymarker[" + std::to_string(points_.size()) + " points]";
}

// ============================================================================
// TEXT
// ============================================================================

Text::Text(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 4, container)),
      position_(0, 0),
      isFinal_(true) {}

void Text::readFromBinary(IBinaryReader& reader) {
    position_ = reader.readPoint();
    isFinal_ = (reader.readEnum() != 0);  // Read final flag BEFORE text
    text_ = reader.readString();
}

void Text::writeAsBinary(IBinaryWriter& writer) {
    writer.writePoint(position_);
    writer.writeEnum(isFinal_ ? 1 : 0);
    writer.writeString(text_);
}

void Text::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("TEXT (");
    writer.writeString(std::to_string(position_.x()) + "," + std::to_string(position_.y()));
    writer.writeString(") ");
    writer.writeString(isFinal_ ? "1 " : "0 ");
    writer.writeString("'");
    writer.writeString(escapeClearTextString(text_));
    writer.writeString("';");
}

std::string Text::toString() const {
    return "Text[pos=(" + std::to_string(position_.x()) + "," +
           std::to_string(position_.y()) + "), text=\"" + text_ + "\"]";
}

// ============================================================================
// RESTRICTED TEXT
// ============================================================================

RestrictedText::RestrictedText(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 5, container)),
      deltaWidth_(0),
      deltaHeight_(0),
      position_(0, 0),
      isFinal_(true) {}

void RestrictedText::readFromBinary(IBinaryReader& reader) {
    deltaWidth_ = reader.readVdc();
    deltaHeight_ = reader.readVdc();
    position_ = reader.readPoint();
    isFinal_ = reader.readBool();  // Read final flag BEFORE text
    text_ = reader.readString();
}

void RestrictedText::writeAsBinary(IBinaryWriter& writer) {
    writer.writeVdc(deltaWidth_);
    writer.writeVdc(deltaHeight_);
    writer.writePoint(position_);
    writer.writeBool(isFinal_);
    writer.writeString(text_);
}

void RestrictedText::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("RESTRTEXT ");
    writer.writeString(std::to_string(deltaWidth_) + " " + std::to_string(deltaHeight_));
    writer.writeString(" (");
    writer.writeString(std::to_string(position_.x()) + "," + std::to_string(position_.y()));
    writer.writeString(") ");
    writer.writeString(isFinal_ ? "1 " : "0 ");
    writer.writeString("'");
    writer.writeString(escapeClearTextString(text_));
    writer.writeString("';");
}

std::string RestrictedText::toString() const {
    return "RestrictedText[bounds=" + std::to_string(deltaWidth_) + "x" +
           std::to_string(deltaHeight_) + ", text=\"" + text_ + "\"]";
}

// ============================================================================
// APPEND TEXT
// ============================================================================

AppendText::AppendText(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 6, container)),
      isFinal_(true) {}

void AppendText::readFromBinary(IBinaryReader& reader) {
    isFinal_ = (reader.readEnum() != 0);  // Read final flag BEFORE text
    text_ = reader.readString();
}

void AppendText::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(isFinal_ ? 1 : 0);
    writer.writeString(text_);
}

void AppendText::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("APNDTEXT ");
    writer.writeString(isFinal_ ? "1 " : "0 ");
    writer.writeString("'");
    writer.writeString(escapeClearTextString(text_));
    writer.writeString("';");
}

std::string AppendText::toString() const {
    return "AppendText[\"" + text_ + "\"]";
}

// ============================================================================
// POLYGON
// ============================================================================

Polygon::Polygon(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 7, container)) {}

void Polygon::readFromBinary(IBinaryReader& reader) {
    while (reader.hasMoreData()) {
        enforcePointLimit(points_.size() + 1, "Polygon points");
        points_.push_back(reader.readPoint());
    }
}

void Polygon::writeAsBinary(IBinaryWriter& writer) {
    for (const auto& point : points_) {
        writer.writePoint(point);
    }
}

void Polygon::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("POLYGON");
    for (const auto& point : points_) {
        writer.writeString(" (");
        writer.writeString(std::to_string(point.x()) + "," + std::to_string(point.y()));
        writer.writeString(")");
    }
    writer.writeString(";");
}

std::string Polygon::toString() const {
    return "Polygon[" + std::to_string(points_.size()) + " points]";
}

// ============================================================================
// POLYGON SET
// ============================================================================

PolygonSet::PolygonSet(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 8, container)) {}

void PolygonSet::readFromBinary(IBinaryReader& reader) {
    while (reader.hasMoreData()) {
        PolygonEdge edge;
        edge.point = reader.readPoint();
        edge.edgeOutFlag = reader.readEnum();
        enforcePointLimit(edges_.size() + 1, "PolygonSet edges");
        edges_.push_back(edge);
    }
}

void PolygonSet::writeAsBinary(IBinaryWriter& writer) {
    for (const auto& edge : edges_) {
        writer.writePoint(edge.point);
        writer.writeEnum(edge.edgeOutFlag);
    }
}

void PolygonSet::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("POLYGONSET");
    for (const auto& edge : edges_) {
        writer.writeString(" (");
        writer.writeString(std::to_string(edge.point.x()) + "," + std::to_string(edge.point.y()) + "," + std::to_string(edge.edgeOutFlag));
        writer.writeString(")");
    }
    writer.writeString(";");
}

std::string PolygonSet::toString() const {
    return "PolygonSet[" + std::to_string(edges_.size()) + " edges]";
}

// ============================================================================
// CELL ARRAY
// ============================================================================

CellArray::CellArray(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 9, container)),
      cornerP_(0, 0),
      cornerQ_(0, 0),
      cornerR_(0, 0),
      nx_(0),
      ny_(0),
      localColorPrecision_(-1) {}

void CellArray::readFromBinary(IBinaryReader& reader) {
    cornerP_ = reader.readPoint();
    cornerQ_ = reader.readPoint();
    cornerR_ = reader.readPoint();
    nx_ = reader.readInt();
    ny_ = reader.readInt();

    // SECURITY: Validate dimensions before allocation to prevent DoS attacks
    security::validateCellArrayDimensions(nx_, ny_);

    // ISO/IEC 8632-3: P6 - local colour precision (always present, 0 means use default)
    localColorPrecision_ = reader.readInt();

    // ISO/IEC 8632-3: P7 - cell representation mode (0=run-length, 1=packed)
    int representationMode = reader.readEnum();

    // Read cell colors based on representation mode
    colorArray_.resize(ny_);
    for (int y = 0; y < ny_; y++) {
        colorArray_[y].resize(nx_);
    }

    if (representationMode == 0) {
        // Run-length encoding per ISO 8632-3.
        //
        // Per row: a sequence of (count : 16 bits, color : N bits) pairs
        // packed contiguously at the bit level, where the runs cover all nx
        // cells. Rows align on word (2-byte) boundaries — padding bits at the
        // end of a row's last byte are ignored.
        //
        // N (color width) depends on color mode:
        //   INDEXED: localColorPrecision_ if > 0, else colourIndexPrecision()
        //   DIRECT:  3 * colourPrecision() (R/G/B components packed)
        ColorSelectionMode colorMode = ColorSelectionMode::INDEXED;
        int compPrecision = 8;                       // per-component bits (direct mode)
        int bitsPerColor  = 8;                       // total color-token width

        if (container_) {
            colorMode = container_->colorSelectionMode();
            if (colorMode == ColorSelectionMode::INDEXED) {
                bitsPerColor = (localColorPrecision_ > 0) ? localColorPrecision_ : container_->colourIndexPrecision();
                if (bitsPerColor <= 0) bitsPerColor = 8;
            } else {
                compPrecision = container_->colourPrecision();
                if (compPrecision <= 0) compPrecision = 8;
                bitsPerColor = 3 * compPrecision;
            }
        }

        // Construct a CGMColor from a colorVal of bitsPerColor bits. For
        // INDEXED, colorVal is a color-table index. For DIRECT, the bits pack
        // R | G | B (MSB first) at compPrecision bits per component. Scales
        // each component to 0..255 when compPrecision != 8 so downstream
        // raster encoders see standard 8-bit channels.
        auto makeCGMColor = [&](int colorVal) -> CGMColor {
            if (colorMode == ColorSelectionMode::DIRECT) {
                int mask = (compPrecision >= 31) ? 0x7FFFFFFF : ((1 << compPrecision) - 1);
                int rRaw = (colorVal >> (compPrecision * 2)) & mask;
                int gRaw = (colorVal >> compPrecision) & mask;
                int bRaw = colorVal & mask;
                int r = rRaw, g = gRaw, b = bRaw;
                if (mask != 0 && mask != 255) {
                    r = (rRaw * 255 + (mask / 2)) / mask;
                    g = (gRaw * 255 + (mask / 2)) / mask;
                    b = (bRaw * 255 + (mask / 2)) / mask;
                }
                if (r < 0) r = 0;
                if (r > 255) r = 255;
                if (g < 0) g = 0;
                if (g > 255) g = 255;
                if (b < 0) b = 0;
                if (b > 255) b = 255;
                Color c{static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), 255};
                return CGMColor(c);
            }
            return CGMColor(colorVal); // INDEXED: stash index, resolver looks up later
        };

        // Bit-reader over allData starting from a row's first byte. Reads MSB
        // first, big-endian. Out-of-bounds bits read as 0.
        std::vector<uint8_t> allData = reader.getRemainingBytes();
        auto readBits = [&](size_t startByte, size_t bitOffset, int nbits) -> int {
            int v = 0;
            for (int i = 0; i < nbits; ++i) {
                size_t cb = bitOffset + i;
                size_t bi = startByte + (cb >> 3);
                if (bi >= allData.size()) break;
                int bitInByte = 7 - (cb & 7);
                if (allData[bi] & (1 << bitInByte)) {
                    v |= (1 << (nbits - 1 - i));
                }
            }
            return v;
        };

        size_t dataOffset = 0;
        for (int y = 0; y < ny_ && dataOffset < allData.size(); y++) {
            // First pass: walk runs until cells filled, then word-align row size.
            size_t bitPos = 0;
            int tempCells = 0;
            while (tempCells < nx_) {
                size_t byteIdx = dataOffset + (bitPos >> 3);
                if (byteIdx >= allData.size()) break;
                int runCount = readBits(dataOffset, bitPos, 16);
                bitPos += 16 + bitsPerColor;
                if (runCount <= 0) {
                    // Defensive: a zero/negative count would loop forever.
                    break;
                }
                tempCells += runCount;
            }
            size_t rowBytes = (bitPos + 7) / 8;
            if (rowBytes & 1) rowBytes++;       // word-align

            // Second pass: decode and fill colorArray_[y][...].
            bitPos = 0;
            int cellsInRow = 0;
            while (cellsInRow < nx_) {
                size_t byteIdx = dataOffset + (bitPos >> 3);
                if (byteIdx >= allData.size()) break;
                int runCount = readBits(dataOffset, bitPos, 16);
                bitPos += 16;
                int colorVal = readBits(dataOffset, bitPos, bitsPerColor);
                bitPos += bitsPerColor;

                if (runCount <= 0) break;

                CGMColor color = makeCGMColor(colorVal);
                int upper = std::min<int>(cellsInRow + runCount, nx_);
                for (int x = cellsInRow; x < upper; ++x) {
                    colorArray_[y][x] = color;
                }
                cellsInRow = upper;
            }

            dataOffset += rowBytes;
        }
    } else {
        // Packed mode: each cell is a color value in sequence
        for (int y = 0; y < ny_; y++) {
            for (int x = 0; x < nx_; x++) {
                colorArray_[y][x] = reader.readColor(localColorPrecision_);
            }
            // Each row ends on a word boundary
            reader.alignOnWord();
        }
    }
}

void CellArray::writeAsBinary(IBinaryWriter& writer) {
    writer.writePoint(cornerP_);
    writer.writePoint(cornerQ_);
    writer.writePoint(cornerR_);
    writer.writeInt(nx_);
    writer.writeInt(ny_);

    const bool emitLocalPrecision =
        (container_ && container_->colorSelectionMode() == ColorSelectionMode::DIRECT && localColorPrecision_ >= 0);
    if (emitLocalPrecision) {
        writer.writeInt(localColorPrecision_);
    }

    for (int y = 0; y < ny_; y++) {
        for (int x = 0; x < nx_; x++) {
            writer.writeColor(colorArray_[y][x], localColorPrecision_);
        }
    }
}

void CellArray::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("CELLARRAY ");
    writer.writeString("(");
    writer.writeString(std::to_string(cornerP_.x()) + "," + std::to_string(cornerP_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(cornerQ_.x()) + "," + std::to_string(cornerQ_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(cornerR_.x()) + "," + std::to_string(cornerR_.y()));
    writer.writeString(") ");
    writer.writeString(std::to_string(nx_) + " " + std::to_string(ny_));
    writer.writeString(";");
}

std::string CellArray::toString() const {
    std::string precStr;
    if (localColorPrecision_ >= 0) {
        precStr = std::to_string(localColorPrecision_);
    } else if (container_) {
        int fallback = (container_->colorSelectionMode() == ColorSelectionMode::DIRECT)
                           ? container_->colourPrecision()
                           : container_->colourIndexPrecision();
        precStr = "default(" + std::to_string(fallback) + ")";
    } else {
        precStr = "default";
    }

    return "CellArray[" + std::to_string(nx_) + "x" + std::to_string(ny_) +
           " cells, prec=" + precStr + "]";
}

// ============================================================================
// RECTANGLE
// ============================================================================

Rectangle::Rectangle(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 11, container)),
      firstCorner_(0, 0),
      secondCorner_(0, 0) {}

void Rectangle::readFromBinary(IBinaryReader& reader) {
    firstCorner_ = reader.readPoint();
    secondCorner_ = reader.readPoint();
}

void Rectangle::writeAsBinary(IBinaryWriter& writer) {
    writer.writePoint(firstCorner_);
    writer.writePoint(secondCorner_);
}

void Rectangle::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("RECT (");
    writer.writeString(std::to_string(firstCorner_.x()) + "," + std::to_string(firstCorner_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(secondCorner_.x()) + "," + std::to_string(secondCorner_.y()));
    writer.writeString(");");
}

std::string Rectangle::toString() const {
    return "Rectangle[(" +
           std::to_string(firstCorner_.x()) + "," + std::to_string(firstCorner_.y()) +
           ") (" +
           std::to_string(secondCorner_.x()) + "," + std::to_string(secondCorner_.y()) +
           ")]";
}

// ============================================================================
// CIRCLE
// ============================================================================

Circle::Circle(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 12, container)),
      center_(0, 0),
      radius_(0) {}

void Circle::readFromBinary(IBinaryReader& reader) {
    center_ = reader.readPoint();
    radius_ = reader.readVdc();
}

void Circle::writeAsBinary(IBinaryWriter& writer) {
    writer.writePoint(center_);
    writer.writeVdc(radius_);
}

void Circle::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("CIRCLE (");
    writer.writeString(std::to_string(center_.x()) + "," + std::to_string(center_.y()));
    writer.writeString(") ");
    writer.writeString(std::to_string(radius_));
    writer.writeString(";");
}

std::string Circle::toString() const {
    return "Circle[center=(" +
           std::to_string(center_.x()) + "," + std::to_string(center_.y()) +
           "), radius=" + std::to_string(radius_) + "]";
}

// ============================================================================
// CIRCULAR ARC 3 POINT
// ============================================================================

CircularArc3Point::CircularArc3Point(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 13, container)),
      start_(0, 0),
      intermediate_(0, 0),
      end_(0, 0) {}

void CircularArc3Point::readFromBinary(IBinaryReader& reader) {
    start_ = reader.readPoint();
    intermediate_ = reader.readPoint();
    end_ = reader.readPoint();
}

void CircularArc3Point::writeAsBinary(IBinaryWriter& writer) {
    writer.writePoint(start_);
    writer.writePoint(intermediate_);
    writer.writePoint(end_);
}

void CircularArc3Point::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("ARC3PT (");
    writer.writeString(std::to_string(start_.x()) + "," + std::to_string(start_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(intermediate_.x()) + "," + std::to_string(intermediate_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(end_.x()) + "," + std::to_string(end_.y()));
    writer.writeString(");");
}

std::string CircularArc3Point::toString() const {
    return "CircularArc3Point[start=(" +
           std::to_string(start_.x()) + "," + std::to_string(start_.y()) +
           "), end=(" +
           std::to_string(end_.x()) + "," + std::to_string(end_.y()) + ")]";
}

// ============================================================================
// CIRCULAR ARC 3 POINT CLOSE
// ============================================================================

CircularArc3PointClose::CircularArc3PointClose(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 14, container)),
      start_(0, 0),
      intermediate_(0, 0),
      end_(0, 0),
      closure_(0) {}

void CircularArc3PointClose::readFromBinary(IBinaryReader& reader) {
    start_ = reader.readPoint();
    intermediate_ = reader.readPoint();
    end_ = reader.readPoint();
    closure_ = reader.readEnum();
}

void CircularArc3PointClose::writeAsBinary(IBinaryWriter& writer) {
    writer.writePoint(start_);
    writer.writePoint(intermediate_);
    writer.writePoint(end_);
    writer.writeEnum(closure_);
}

void CircularArc3PointClose::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("ARC3PTCLOSE (");
    writer.writeString(std::to_string(start_.x()) + "," + std::to_string(start_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(intermediate_.x()) + "," + std::to_string(intermediate_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(end_.x()) + "," + std::to_string(end_.y()));
    writer.writeString(") ");
    writer.writeString(closure_ == 0 ? "PIE" : "CHORD");
    writer.writeString(";");
}

std::string CircularArc3PointClose::toString() const {
    return "CircularArc3PointClose[closure=" +
           std::string(closure_ == 0 ? "PIE" : "CHORD") + "]";
}

// ============================================================================
// CIRCULAR ARC CENTRE
// ============================================================================

CircularArcCentre::CircularArcCentre(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 15, container)),
      center_(0, 0),
      startDelta_(0, 0),
      endDelta_(0, 0),
      radius_(0) {}

void CircularArcCentre::readFromBinary(IBinaryReader& reader) {
    center_ = reader.readPoint();
    startDelta_ = reader.readPoint();
    endDelta_ = reader.readPoint();
    radius_ = reader.readVdc();
}

void CircularArcCentre::writeAsBinary(IBinaryWriter& writer) {
    writer.writePoint(center_);
    writer.writePoint(startDelta_);
    writer.writePoint(endDelta_);
    writer.writeVdc(radius_);
}

void CircularArcCentre::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("ARCCENTRE (");
    writer.writeString(std::to_string(center_.x()) + "," + std::to_string(center_.y()));
    writer.writeString(") ");
    writer.writeString(std::to_string(radius_));
    writer.writeString(" (");
    writer.writeString(std::to_string(startDelta_.x()) + "," + std::to_string(startDelta_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(endDelta_.x()) + "," + std::to_string(endDelta_.y()));
    writer.writeString(");");
}

std::string CircularArcCentre::toString() const {
    return "CircularArcCentre[center=(" +
           std::to_string(center_.x()) + "," + std::to_string(center_.y()) +
           "), radius=" + std::to_string(radius_) + "]";
}

// ============================================================================
// CIRCULAR ARC CENTRE CLOSE
// ============================================================================

CircularArcCentreClose::CircularArcCentreClose(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 16, container)),
      center_(0, 0),
      startDelta_(0, 0),
      endDelta_(0, 0),
      radius_(0),
      closure_(0) {}

void CircularArcCentreClose::readFromBinary(IBinaryReader& reader) {
    center_ = reader.readPoint();
    startDelta_ = reader.readPoint();
    endDelta_ = reader.readPoint();
    radius_ = reader.readVdc();
    closure_ = reader.readEnum();
}

void CircularArcCentreClose::writeAsBinary(IBinaryWriter& writer) {
    writer.writePoint(center_);
    writer.writePoint(startDelta_);
    writer.writePoint(endDelta_);
    writer.writeVdc(radius_);
    writer.writeEnum(closure_);
}

void CircularArcCentreClose::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("ARCCENTRECLOSE (");
    writer.writeString(std::to_string(center_.x()) + "," + std::to_string(center_.y()));
    writer.writeString(") ");
    writer.writeString(std::to_string(radius_));
    writer.writeString(" (");
    writer.writeString(std::to_string(startDelta_.x()) + "," + std::to_string(startDelta_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(endDelta_.x()) + "," + std::to_string(endDelta_.y()));
    writer.writeString(") ");
    writer.writeString(closure_ == 0 ? "PIE" : "CHORD");
    writer.writeString(";");
}

std::string CircularArcCentreClose::toString() const {
    return "CircularArcCentreClose[radius=" + std::to_string(radius_) +
           ", closure=" + std::string(closure_ == 0 ? "PIE" : "CHORD") + "]";
}

// ============================================================================
// ELLIPSE
// ============================================================================

Ellipse::Ellipse(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 17, container)),
      center_(0, 0),
      firstConjugate_(0, 0),
      secondConjugate_(0, 0) {}

void Ellipse::readFromBinary(IBinaryReader& reader) {
    center_ = reader.readPoint();
    firstConjugate_ = reader.readPoint();
    secondConjugate_ = reader.readPoint();
}

void Ellipse::writeAsBinary(IBinaryWriter& writer) {
    writer.writePoint(center_);
    writer.writePoint(firstConjugate_);
    writer.writePoint(secondConjugate_);
}

void Ellipse::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("ELLIPSE (");
    writer.writeString(std::to_string(center_.x()) + "," + std::to_string(center_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(firstConjugate_.x()) + "," + std::to_string(firstConjugate_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(secondConjugate_.x()) + "," + std::to_string(secondConjugate_.y()));
    writer.writeString(");");
}

std::string Ellipse::toString() const {
    return "Ellipse[center=(" +
           std::to_string(center_.x()) + "," + std::to_string(center_.y()) + ")]";
}

// ============================================================================
// ELLIPTICAL ARC
// ============================================================================

EllipticalArc::EllipticalArc(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 18, container)),
      center_(0, 0),
      firstConjugate_(0, 0),
      secondConjugate_(0, 0),
      startDelta_(0, 0),
      endDelta_(0, 0) {}

void EllipticalArc::readFromBinary(IBinaryReader& reader) {
    center_ = reader.readPoint();
    firstConjugate_ = reader.readPoint();
    secondConjugate_ = reader.readPoint();
    startDelta_ = reader.readPoint();
    endDelta_ = reader.readPoint();
}

void EllipticalArc::writeAsBinary(IBinaryWriter& writer) {
    writer.writePoint(center_);
    writer.writePoint(firstConjugate_);
    writer.writePoint(secondConjugate_);
    writer.writePoint(startDelta_);
    writer.writePoint(endDelta_);
}

void EllipticalArc::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("ELLIPARC (");
    writer.writeString(std::to_string(center_.x()) + "," + std::to_string(center_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(firstConjugate_.x()) + "," + std::to_string(firstConjugate_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(secondConjugate_.x()) + "," + std::to_string(secondConjugate_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(startDelta_.x()) + "," + std::to_string(startDelta_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(endDelta_.x()) + "," + std::to_string(endDelta_.y()));
    writer.writeString(");");
}

std::string EllipticalArc::toString() const {
    return "EllipticalArc[center=(" +
           std::to_string(center_.x()) + "," + std::to_string(center_.y()) + ")]";
}

// ============================================================================
// ELLIPTICAL ARC CLOSE
// ============================================================================

EllipticalArcClose::EllipticalArcClose(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 19, container)),
      center_(0, 0),
      firstConjugate_(0, 0),
      secondConjugate_(0, 0),
      startDelta_(0, 0),
      endDelta_(0, 0),
      closure_(0) {}

void EllipticalArcClose::readFromBinary(IBinaryReader& reader) {
    center_ = reader.readPoint();
    firstConjugate_ = reader.readPoint();
    secondConjugate_ = reader.readPoint();
    startDelta_ = reader.readPoint();
    endDelta_ = reader.readPoint();
    closure_ = reader.readEnum();
}

void EllipticalArcClose::writeAsBinary(IBinaryWriter& writer) {
    writer.writePoint(center_);
    writer.writePoint(firstConjugate_);
    writer.writePoint(secondConjugate_);
    writer.writePoint(startDelta_);
    writer.writePoint(endDelta_);
    writer.writeEnum(closure_);
}

void EllipticalArcClose::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("ELLIPARCCLOSE (");
    writer.writeString(std::to_string(center_.x()) + "," + std::to_string(center_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(firstConjugate_.x()) + "," + std::to_string(firstConjugate_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(secondConjugate_.x()) + "," + std::to_string(secondConjugate_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(startDelta_.x()) + "," + std::to_string(startDelta_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(endDelta_.x()) + "," + std::to_string(endDelta_.y()));
    writer.writeString(") ");
    writer.writeString(closure_ == 0 ? "PIE" : "CHORD");
    writer.writeString(";");
}

std::string EllipticalArcClose::toString() const {
    return "EllipticalArcClose[closure=" +
           std::string(closure_ == 0 ? "PIE" : "CHORD") + "]";
}

} // namespace opencgm
