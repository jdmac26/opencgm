#pragma once

#include "../core/command.h"
#include "../import/binary_reader.h"
#include "../export/binary_writer.h"
#include "../classes/cgm_point.h"
#include <vector>
#include <string>
#include <sstream>

namespace opencgm {

/**
 * @brief Template for point-based shape commands (Polyline, Polygon, Polymarker, etc.)
 *
 * This template eliminates duplication for commands that read/write a series of points.
 * All point-based commands follow the same pattern:
 * - readFromBinary: Read points until no more data
 * - writeAsBinary: Write all points
 * - writeAsClearText: Format points as text
 * - toString: Show command name and point count
 *
 * @tparam CommandID The CGM command ID
 * @tparam CommandName The human-readable command name
 */
template<int CommandID, const char* CommandName>
class PointBasedCommand : public Command {
protected:
    std::vector<Point> points_;

public:
    PointBasedCommand(CGMFile* container) : Command(container) {}

    void readFromBinary(IBinaryReader& reader) override {
        while (reader.hasMoreData()) {
            points_.push_back(reader.readPoint());
        }
    }

    void writeAsBinary(IBinaryWriter& writer) const override {
        for (const auto& point : points_) {
            writer.writePoint(point);
        }
    }

    void writeAsClearText(IClearTextWriter& writer) const override {
        writer.writeString(CommandName);
        writer.writeString("\n");

        for (size_t i = 0; i < points_.size(); ++i) {
            writer.writeString("  ");
            writer.writePoint(points_[i]);
            if (i < points_.size() - 1) {
                writer.writeString(",\n");
            }
        }
        writer.writeString(";\n");
    }

    std::string toString() const override {
        std::ostringstream oss;
        oss << CommandName << "[" << points_.size() << " points]";
        return oss.str();
    }

    // Accessors
    const std::vector<Point>& getPoints() const { return points_; }
    void setPoints(const std::vector<Point>& points) { points_ = points; }
    void addPoint(const Point& point) { points_.push_back(point); }
    void clearPoints() { points_.clear(); }
    size_t getPointCount() const { return points_.size(); }
};

/**
 * @brief Template for two-point geometric shapes (Rectangle, Ellipse, etc.)
 *
 * @tparam CommandID The CGM command ID
 * @tparam CommandName The human-readable command name
 */
template<int CommandID, const char* CommandName>
class TwoPointCommand : public Command {
protected:
    Point firstPoint_;
    Point secondPoint_;

public:
    TwoPointCommand(CGMFile* container) : Command(container) {}

    void readFromBinary(IBinaryReader& reader) override {
        firstPoint_ = reader.readPoint();
        secondPoint_ = reader.readPoint();
    }

    void writeAsBinary(IBinaryWriter& writer) const override {
        writer.writePoint(firstPoint_);
        writer.writePoint(secondPoint_);
    }

    void writeAsClearText(IClearTextWriter& writer) const override {
        writer.writeString(CommandName);
        writer.writeString("\n  ");
        writer.writePoint(firstPoint_);
        writer.writeString(",\n  ");
        writer.writePoint(secondPoint_);
        writer.writeString(";\n");
    }

    std::string toString() const override {
        std::ostringstream oss;
        oss << CommandName << "[(" << firstPoint_.x() << "," << firstPoint_.y()
            << ") - (" << secondPoint_.x() << "," << secondPoint_.y() << ")]";
        return oss.str();
    }

    // Accessors
    Point getFirstPoint() const { return firstPoint_; }
    Point getSecondPoint() const { return secondPoint_; }
    void setFirstPoint(const Point& point) { firstPoint_ = point; }
    void setSecondPoint(const Point& point) { secondPoint_ = point; }
};

} // namespace opencgm
