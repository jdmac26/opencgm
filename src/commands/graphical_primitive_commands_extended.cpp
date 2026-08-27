// Extended graphical primitive implementations - to be merged into graphical_primitive_commands.cpp

#include "opencgm/commands/graphical_primitive_commands.h"
#include "opencgm/interfaces.h"
#include "opencgm/cgm_file.h"
#include "opencgm/security_limits.h"
#include <sstream>

namespace opencgm {

// ============================================================================
// CIRCULAR ARC CENTRE REVERSED
// ============================================================================

CircularArcCentreReversed::CircularArcCentreReversed(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 20, container)),
      center_(0, 0),
      startDelta_(0, 0),
      endDelta_(0, 0),
      radius_(0) {}

void CircularArcCentreReversed::readFromBinary(IBinaryReader& reader) {
    center_ = reader.readPoint();
    startDelta_ = reader.readPoint();
    endDelta_ = reader.readPoint();
    radius_ = reader.readVdc();
}

void CircularArcCentreReversed::writeAsBinary(IBinaryWriter& writer) {
    writer.writePoint(center_);
    writer.writePoint(startDelta_);
    writer.writePoint(endDelta_);
    writer.writeVdc(radius_);
}

void CircularArcCentreReversed::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("ARCCENTREREV (");
    writer.writeString(std::to_string(center_.x()) + "," + std::to_string(center_.y()));
    writer.writeString(") ");
    writer.writeString(std::to_string(radius_));
    writer.writeString(" (");
    writer.writeString(std::to_string(startDelta_.x()) + "," + std::to_string(startDelta_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(endDelta_.x()) + "," + std::to_string(endDelta_.y()));
    writer.writeString(");");
}

std::string CircularArcCentreReversed::toString() const {
    return "CircularArcCentreReversed[radius=" + std::to_string(radius_) + "]";
}

// ============================================================================
// CONNECTING EDGE
// ============================================================================

ConnectingEdge::ConnectingEdge(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 21, container)) {}

void ConnectingEdge::readFromBinary(IBinaryReader& /* reader */) {
    // No parameters
}

void ConnectingEdge::writeAsBinary(IBinaryWriter& /* writer */) {
    // No parameters
}

void ConnectingEdge::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("CONNEDGE;");
}

std::string ConnectingEdge::toString() const {
    return "ConnectingEdge";
}

// ============================================================================
// HYPERBOLIC ARC
// ============================================================================

HyperbolicArc::HyperbolicArc(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 22, container)),
      center_(0, 0),
      transverseRadius_(0, 0),
      conjugateRadius_(0, 0),
      startPoint_(0, 0),
      endPoint_(0, 0) {}

void HyperbolicArc::readFromBinary(IBinaryReader& reader) {
    center_ = reader.readPoint();
    transverseRadius_ = reader.readPoint();
    conjugateRadius_ = reader.readPoint();
    startPoint_ = reader.readPoint();
    endPoint_ = reader.readPoint();
}

void HyperbolicArc::writeAsBinary(IBinaryWriter& writer) {
    writer.writePoint(center_);
    writer.writePoint(transverseRadius_);
    writer.writePoint(conjugateRadius_);
    writer.writePoint(startPoint_);
    writer.writePoint(endPoint_);
}

void HyperbolicArc::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("HYPERARC (");
    writer.writeString(std::to_string(center_.x()) + "," + std::to_string(center_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(transverseRadius_.x()) + "," + std::to_string(transverseRadius_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(conjugateRadius_.x()) + "," + std::to_string(conjugateRadius_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(startPoint_.x()) + "," + std::to_string(startPoint_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(endPoint_.x()) + "," + std::to_string(endPoint_.y()));
    writer.writeString(");");
}

std::string HyperbolicArc::toString() const {
    return "HyperbolicArc[center=(" +
           std::to_string(center_.x()) + "," + std::to_string(center_.y()) + ")]";
}

// ============================================================================
// PARABOLIC ARC
// ============================================================================

ParabolicArc::ParabolicArc(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 23, container)),
      tangentIntersection_(0, 0),
      startPoint_(0, 0),
      endPoint_(0, 0) {}

void ParabolicArc::readFromBinary(IBinaryReader& reader) {
    tangentIntersection_ = reader.readPoint();
    startPoint_ = reader.readPoint();
    endPoint_ = reader.readPoint();
}

void ParabolicArc::writeAsBinary(IBinaryWriter& writer) {
    writer.writePoint(tangentIntersection_);
    writer.writePoint(startPoint_);
    writer.writePoint(endPoint_);
}

void ParabolicArc::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("PARAARC (");
    writer.writeString(std::to_string(startPoint_.x()) + "," + std::to_string(startPoint_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(tangentIntersection_.x()) + "," + std::to_string(tangentIntersection_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(endPoint_.x()) + "," + std::to_string(endPoint_.y()));
    writer.writeString(");");
}

std::string ParabolicArc::toString() const {
    return "ParabolicArc[tangent=(" +
           std::to_string(tangentIntersection_.x()) + "," +
           std::to_string(tangentIntersection_.y()) + ")]";
}

// ============================================================================
// NON-UNIFORM B-SPLINE
// ============================================================================

NonUniformBSpline::NonUniformBSpline(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 24, container)),
      splineOrder_(0),
      startParameter_(0.0),
      endParameter_(1.0) {}

void NonUniformBSpline::readFromBinary(IBinaryReader& reader) {
    splineOrder_ = reader.readInt();

    int numControlPoints = reader.readInt();
    // SECURITY: Validate control point count before allocation
    security::validateAllocationSize(
        static_cast<size_t>(numControlPoints),
        security::MAX_SPLINE_CONTROL_POINTS,
        "B-Spline control points"
    );
    controlPoints_.reserve(numControlPoints);
    for (int i = 0; i < numControlPoints; i++) {
        controlPoints_.push_back(reader.readPoint());
    }

    int numKnots = reader.readInt();
    // SECURITY: Validate knot count before allocation
    security::validateAllocationSize(
        static_cast<size_t>(numKnots),
        security::MAX_SPLINE_KNOTS,
        "B-Spline knots"
    );
    knots_.reserve(numKnots);
    for (int i = 0; i < numKnots; i++) {
        knots_.push_back(reader.readReal());
    }

    startParameter_ = reader.readReal();
    endParameter_ = reader.readReal();
}

void NonUniformBSpline::writeAsBinary(IBinaryWriter& writer) {
    writer.writeInt(splineOrder_);

    writer.writeInt(static_cast<int>(controlPoints_.size()));
    for (const auto& pt : controlPoints_) {
        writer.writePoint(pt);
    }

    writer.writeInt(static_cast<int>(knots_.size()));
    for (double knot : knots_) {
        writer.writeReal(knot);
    }

    writer.writeReal(startParameter_);
    writer.writeReal(endParameter_);
}

void NonUniformBSpline::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("NUBSPLINE ");
    writer.writeString("ORDER ");
    writer.writeString(std::to_string(splineOrder_));
    writer.writeString(" CP ");
    writer.writeString(std::to_string(controlPoints_.size()));
    for (const auto &pt : controlPoints_) {
        writer.writeString(" (");
        writer.writeString(std::to_string(pt.x()) + "," + std::to_string(pt.y()));
        writer.writeString(")");
    }
    writer.writeString(" KNOTS ");
    writer.writeString(std::to_string(knots_.size()));
    for (double k : knots_) {
        writer.writeString(" ");
        writer.writeString(std::to_string(k));
    }
    writer.writeString(" RANGE ");
    writer.writeString(std::to_string(startParameter_));
    writer.writeString(" ");
    writer.writeString(std::to_string(endParameter_));
    writer.writeString(";");
}

std::string NonUniformBSpline::toString() const {
    return "NonUniformBSpline[order=" + std::to_string(splineOrder_) +
           ", points=" + std::to_string(controlPoints_.size()) +
           ", knots=" + std::to_string(knots_.size()) + "]";
}

// ============================================================================
// NON-UNIFORM RATIONAL B-SPLINE
// ============================================================================

NonUniformRationalBSpline::NonUniformRationalBSpline(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 25, container)),
      splineOrder_(0),
      startParameter_(0.0),
      endParameter_(1.0) {}

void NonUniformRationalBSpline::readFromBinary(IBinaryReader& reader) {
    splineOrder_ = reader.readInt();

    int numControlPoints = reader.readInt();
    // SECURITY: Validate control point count before allocation
    security::validateAllocationSize(
        static_cast<size_t>(numControlPoints),
        security::MAX_SPLINE_CONTROL_POINTS,
        "Rational B-Spline control points"
    );
    controlPoints_.reserve(numControlPoints);
    for (int i = 0; i < numControlPoints; i++) {
        controlPoints_.push_back(reader.readPoint());
    }

    weights_.reserve(numControlPoints);
    for (int i = 0; i < numControlPoints; i++) {
        weights_.push_back(reader.readReal());
    }

    int numKnots = reader.readInt();
    // SECURITY: Validate knot count before allocation
    security::validateAllocationSize(
        static_cast<size_t>(numKnots),
        security::MAX_SPLINE_KNOTS,
        "Rational B-Spline knots"
    );
    knots_.reserve(numKnots);
    for (int i = 0; i < numKnots; i++) {
        knots_.push_back(reader.readReal());
    }

    startParameter_ = reader.readReal();
    endParameter_ = reader.readReal();
}

void NonUniformRationalBSpline::writeAsBinary(IBinaryWriter& writer) {
    writer.writeInt(splineOrder_);

    writer.writeInt(static_cast<int>(controlPoints_.size()));
    for (const auto& pt : controlPoints_) {
        writer.writePoint(pt);
    }

    for (double weight : weights_) {
        writer.writeReal(weight);
    }

    writer.writeInt(static_cast<int>(knots_.size()));
    for (double knot : knots_) {
        writer.writeReal(knot);
    }

    writer.writeReal(startParameter_);
    writer.writeReal(endParameter_);
}

void NonUniformRationalBSpline::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("NURBS ");
    writer.writeString("ORDER ");
    writer.writeString(std::to_string(splineOrder_));
    writer.writeString(" CP ");
    writer.writeString(std::to_string(controlPoints_.size()));
    for (const auto &pt : controlPoints_) {
        writer.writeString(" (");
        writer.writeString(std::to_string(pt.x()) + "," + std::to_string(pt.y()));
        writer.writeString(")");
    }
    writer.writeString(" WEIGHTS ");
    for (double w : weights_) {
        writer.writeString(" ");
        writer.writeString(std::to_string(w));
    }
    writer.writeString(" KNOTS ");
    writer.writeString(std::to_string(knots_.size()));
    for (double k : knots_) {
        writer.writeString(" ");
        writer.writeString(std::to_string(k));
    }
    writer.writeString(" RANGE ");
    writer.writeString(std::to_string(startParameter_));
    writer.writeString(" ");
    writer.writeString(std::to_string(endParameter_));
    writer.writeString(";");
}

std::string NonUniformRationalBSpline::toString() const {
    return "NonUniformRationalBSpline[order=" + std::to_string(splineOrder_) +
           ", points=" + std::to_string(controlPoints_.size()) + "]";
}

// ============================================================================
// POLYBEZIER
// ============================================================================

PolyBezier::PolyBezier(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::GraphicalPrimitiveElements, 26, container)),
      continuityIndicator_(1) {}

void PolyBezier::readFromBinary(IBinaryReader& reader) {
    continuityIndicator_ = reader.readEnum();

    while (reader.hasMoreData()) {
        controlPoints_.push_back(reader.readPoint());
    }
}

void PolyBezier::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(continuityIndicator_);

    for (const auto& pt : controlPoints_) {
        writer.writePoint(pt);
    }
}

void PolyBezier::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("POLYBEZIER ");
    writer.writeString("CONT ");
    writer.writeString(std::to_string(continuityIndicator_));
    for (const auto &pt : controlPoints_) {
        writer.writeString(" (");
        writer.writeString(std::to_string(pt.x()) + "," + std::to_string(pt.y()));
        writer.writeString(")");
    }
    writer.writeString(";");
}

std::string PolyBezier::toString() const {
    return "PolyBezier[" + std::to_string(controlPoints_.size()) + " points, " +
           (continuityIndicator_ == 1 ? "discontinuous" : "continuous") + "]";
}} // namespace opencgm
