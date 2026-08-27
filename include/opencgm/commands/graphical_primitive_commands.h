#ifndef OPENCGM_GRAPHICAL_PRIMITIVE_COMMANDS_H
#define OPENCGM_GRAPHICAL_PRIMITIVE_COMMANDS_H

#include "../command.h"
#include "../cgm_point.h"
#include "../cgm_color.h"
#include <vector>
#include <string>

namespace opencgm {

// Element ID 1: POLYLINE
class Polyline : public Command {
public:
    explicit Polyline(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const std::vector<CGMPoint>& points() const { return points_; }
    void addPoint(const CGMPoint& point) { points_.push_back(point); }

private:
    std::vector<CGMPoint> points_;
};

// Element ID 2: DISJOINT POLYLINE
class DisjointPolyline : public Command {
public:
    explicit DisjointPolyline(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const std::vector<CGMPoint>& points() const { return points_; }

private:
    std::vector<CGMPoint> points_;
};

// Element ID 3: POLYMARKER
class Polymarker : public Command {
public:
    explicit Polymarker(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const std::vector<CGMPoint>& points() const { return points_; }

private:
    std::vector<CGMPoint> points_;
};

// Element ID 4: TEXT
class Text : public Command {
public:
    explicit Text(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMPoint& position() const { return position_; }
    const std::string& text() const { return text_; }
    bool isFinal() const { return isFinal_; }

private:
    CGMPoint position_;
    std::string text_;
    bool isFinal_;
};

// Element ID 5: RESTRICTED TEXT
class RestrictedText : public Command {
public:
    explicit RestrictedText(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    double deltaWidth() const { return deltaWidth_; }
    double deltaHeight() const { return deltaHeight_; }
    const CGMPoint& position() const { return position_; }
    const std::string& text() const { return text_; }
    bool isFinal() const { return isFinal_; }

private:
    double deltaWidth_;
    double deltaHeight_;
    CGMPoint position_;
    std::string text_;
    bool isFinal_;
};

// Element ID 6: APPEND TEXT
class AppendText : public Command {
public:
    explicit AppendText(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const std::string& text() const { return text_; }
    bool isFinal() const { return isFinal_; }

private:
    std::string text_;
    bool isFinal_;
};

// Element ID 7: POLYGON
class Polygon : public Command {
public:
    explicit Polygon(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const std::vector<CGMPoint>& points() const { return points_; }

private:
    std::vector<CGMPoint> points_;
};

// Element ID 8: POLYGON SET
class PolygonSet : public Command {
public:
    explicit PolygonSet(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    struct PolygonEdge {
        CGMPoint point;
        int edgeOutFlag; // 0=invisible, 1=visible, 2=close invisible, 3=close visible
    };

    const std::vector<PolygonEdge>& edges() const { return edges_; }

private:
    std::vector<PolygonEdge> edges_;
};

// Element ID 9: CELL ARRAY
class CellArray : public Command {
public:
    explicit CellArray(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMPoint& cornerP() const { return cornerP_; }
    const CGMPoint& cornerQ() const { return cornerQ_; }
    const CGMPoint& cornerR() const { return cornerR_; }
    int nx() const { return nx_; }
    int ny() const { return ny_; }
    int localColorPrecision() const { return localColorPrecision_; }
    const std::vector<std::vector<CGMColor>>& colorArray() const { return colorArray_; }

private:
    CGMPoint cornerP_; // First corner
    CGMPoint cornerQ_; // Second corner
    CGMPoint cornerR_; // Third corner
    int nx_; // Number of cells in X direction
    int ny_; // Number of cells in Y direction
    int localColorPrecision_;
    std::vector<std::vector<CGMColor>> colorArray_;
};

// Element ID 11: RECTANGLE
class Rectangle : public Command {
public:
    explicit Rectangle(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMPoint& firstCorner() const { return firstCorner_; }
    const CGMPoint& secondCorner() const { return secondCorner_; }

private:
    CGMPoint firstCorner_;
    CGMPoint secondCorner_;
};

// Element ID 12: CIRCLE
class Circle : public Command {
public:
    explicit Circle(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMPoint& center() const { return center_; }
    double radius() const { return radius_; }

private:
    CGMPoint center_;
    double radius_;
};

// Element ID 13: CIRCULAR ARC 3 POINT
class CircularArc3Point : public Command {
public:
    explicit CircularArc3Point(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMPoint& start() const { return start_; }
    const CGMPoint& intermediate() const { return intermediate_; }
    const CGMPoint& end() const { return end_; }

private:
    CGMPoint start_;
    CGMPoint intermediate_;
    CGMPoint end_;
};

// Element ID 14: CIRCULAR ARC 3 POINT CLOSE
class CircularArc3PointClose : public Command {
public:
    explicit CircularArc3PointClose(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMPoint& start() const { return start_; }
    const CGMPoint& intermediate() const { return intermediate_; }
    const CGMPoint& end() const { return end_; }
    int closure() const { return closure_; } // 0=pie, 1=chord

private:
    CGMPoint start_;
    CGMPoint intermediate_;
    CGMPoint end_;
    int closure_;
};

// Element ID 15: CIRCULAR ARC CENTRE
class CircularArcCentre : public Command {
public:
    explicit CircularArcCentre(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMPoint& center() const { return center_; }
    const CGMPoint& startDelta() const { return startDelta_; }
    const CGMPoint& endDelta() const { return endDelta_; }
    double radius() const { return radius_; }

private:
    CGMPoint center_;
    CGMPoint startDelta_;
    CGMPoint endDelta_;
    double radius_;
};

// Element ID 16: CIRCULAR ARC CENTRE CLOSE
class CircularArcCentreClose : public Command {
public:
    explicit CircularArcCentreClose(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMPoint& center() const { return center_; }
    const CGMPoint& startDelta() const { return startDelta_; }
    const CGMPoint& endDelta() const { return endDelta_; }
    double radius() const { return radius_; }
    int closure() const { return closure_; }

private:
    CGMPoint center_;
    CGMPoint startDelta_;
    CGMPoint endDelta_;
    double radius_;
    int closure_;
};

// Element ID 17: ELLIPSE
class Ellipse : public Command {
public:
    explicit Ellipse(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMPoint& center() const { return center_; }
    const CGMPoint& firstConjugateDiameter() const { return firstConjugate_; }
    const CGMPoint& secondConjugateDiameter() const { return secondConjugate_; }

private:
    CGMPoint center_;
    CGMPoint firstConjugate_;
    CGMPoint secondConjugate_;
};

// Element ID 18: ELLIPTICAL ARC
class EllipticalArc : public Command {
public:
    explicit EllipticalArc(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMPoint& center() const { return center_; }
    const CGMPoint& firstConjugate() const { return firstConjugate_; }
    const CGMPoint& secondConjugate() const { return secondConjugate_; }
    const CGMPoint& startDelta() const { return startDelta_; }
    const CGMPoint& endDelta() const { return endDelta_; }

private:
    CGMPoint center_;
    CGMPoint firstConjugate_;
    CGMPoint secondConjugate_;
    CGMPoint startDelta_;
    CGMPoint endDelta_;
};

// Element ID 19: ELLIPTICAL ARC CLOSE
class EllipticalArcClose : public Command {
public:
    explicit EllipticalArcClose(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMPoint& center() const { return center_; }
    const CGMPoint& firstConjugate() const { return firstConjugate_; }
    const CGMPoint& secondConjugate() const { return secondConjugate_; }
    const CGMPoint& startDelta() const { return startDelta_; }
    const CGMPoint& endDelta() const { return endDelta_; }
    int closure() const { return closure_; }

private:
    CGMPoint center_;
    CGMPoint firstConjugate_;
    CGMPoint secondConjugate_;
    CGMPoint startDelta_;
    CGMPoint endDelta_;
    int closure_;
};

// Element ID 20: CIRCULAR ARC CENTRE REVERSED
class CircularArcCentreReversed : public Command {
public:
    explicit CircularArcCentreReversed(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMPoint& center() const { return center_; }
    const CGMPoint& startDelta() const { return startDelta_; }
    const CGMPoint& endDelta() const { return endDelta_; }
    double radius() const { return radius_; }

private:
    CGMPoint center_;
    CGMPoint startDelta_;
    CGMPoint endDelta_;
    double radius_;
};

// Element ID 21: CONNECTING EDGE
class ConnectingEdge : public Command {
public:
    explicit ConnectingEdge(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    // No parameters - connects current position to start of path
};

// Element ID 22: HYPERBOLIC ARC
class HyperbolicArc : public Command {
public:
    explicit HyperbolicArc(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMPoint& center() const { return center_; }
    const CGMPoint& transverseRadius() const { return transverseRadius_; }
    const CGMPoint& conjugateRadius() const { return conjugateRadius_; }
    const CGMPoint& startPoint() const { return startPoint_; }
    const CGMPoint& endPoint() const { return endPoint_; }

private:
    CGMPoint center_;
    CGMPoint transverseRadius_;
    CGMPoint conjugateRadius_;
    CGMPoint startPoint_;
    CGMPoint endPoint_;
};

// Element ID 23: PARABOLIC ARC
class ParabolicArc : public Command {
public:
    explicit ParabolicArc(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMPoint& tangentIntersection() const { return tangentIntersection_; }
    const CGMPoint& startPoint() const { return startPoint_; }
    const CGMPoint& endPoint() const { return endPoint_; }

private:
    CGMPoint tangentIntersection_;
    CGMPoint startPoint_;
    CGMPoint endPoint_;
};

// Element ID 24: NON-UNIFORM B-SPLINE
class NonUniformBSpline : public Command {
public:
    explicit NonUniformBSpline(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int splineOrder() const { return splineOrder_; }
    const std::vector<CGMPoint>& controlPoints() const { return controlPoints_; }
    const std::vector<double>& knots() const { return knots_; }
    double startParameter() const { return startParameter_; }
    double endParameter() const { return endParameter_; }

private:
    int splineOrder_;
    std::vector<CGMPoint> controlPoints_;
    std::vector<double> knots_;
    double startParameter_;
    double endParameter_;
};

// Element ID 25: NON-UNIFORM RATIONAL B-SPLINE
class NonUniformRationalBSpline : public Command {
public:
    explicit NonUniformRationalBSpline(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int splineOrder() const { return splineOrder_; }
    const std::vector<CGMPoint>& controlPoints() const { return controlPoints_; }
    const std::vector<double>& weights() const { return weights_; }
    const std::vector<double>& knots() const { return knots_; }
    double startParameter() const { return startParameter_; }
    double endParameter() const { return endParameter_; }

private:
    int splineOrder_;
    std::vector<CGMPoint> controlPoints_;
    std::vector<double> weights_;
    std::vector<double> knots_;
    double startParameter_;
    double endParameter_;
};

// Element ID 26: POLYBEZIER
class PolyBezier : public Command {
public:
    explicit PolyBezier(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int continuityIndicator() const { return continuityIndicator_; }
    const std::vector<CGMPoint>& controlPoints() const { return controlPoints_; }

private:
    int continuityIndicator_; // 1=discontinuous, 2=continuous
    std::vector<CGMPoint> controlPoints_;
};

// Element ID 10: GENERALIZED DRAWING PRIMITIVE (GDP)
class GeneralizedDrawingPrimitive : public Command {
public:
    explicit GeneralizedDrawingPrimitive(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int identifier() const { return identifier_; }
    const std::vector<CGMPoint>& points() const { return points_; }
    const std::string& dataRecord() const { return dataRecord_; }

private:
    int identifier_;
    std::vector<CGMPoint> points_;
    std::string dataRecord_;
};

// Element ID 27: POLYSYMBOL
class PolySymbol : public Command {
public:
    explicit PolySymbol(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int index() const { return index_; }
    const std::vector<CGMPoint>& points() const { return points_; }

private:
    int index_;
    std::vector<CGMPoint> points_;
};

// Base class for tile elements (BitonalTile and Tile)
class TileElement : public Command {
public:
    int compressionType() const { return compressionType_; }
    int rowPaddingIndicator() const { return rowPaddingIndicator_; }
    const std::string& dataRecord() const { return dataRecord_; }
    const std::vector<uint8_t>& imageData() const { return imageData_; }
    int bitmapWidth() const { return bitmapWidth_; }
    int bitmapHeight() const { return bitmapHeight_; }

protected:
    explicit TileElement(const CommandConstructorArguments& args);

    void readSdrAndBitStream(IBinaryReader& reader);
    void writeSdrAndBitStream(IBinaryWriter& writer);

    virtual void readBitmap(IBinaryReader& reader) = 0;
    virtual void writeBitmap(IBinaryWriter& writer) = 0;

    int compressionType_;
    int rowPaddingIndicator_;
    std::string dataRecord_; // SDR simplified to string
    std::vector<uint8_t> imageData_;
    int bitmapWidth_;
    int bitmapHeight_;
};

// Element ID 28: BITONAL TILE
class BitonalTile : public TileElement {
public:
    explicit BitonalTile(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMColor& backgroundColor() const { return backgroundColor_; }
    const CGMColor& foregroundColor() const { return foregroundColor_; }

protected:
    void readBitmap(IBinaryReader& reader) override;
    void writeBitmap(IBinaryWriter& writer) override;

private:
    CGMColor backgroundColor_;
    CGMColor foregroundColor_;
};

// Element ID 29: TILE
class Tile : public TileElement {
public:
    explicit Tile(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int cellColorPrecision() const { return cellColorPrecision_; }

protected:
    void readBitmap(IBinaryReader& reader) override;
    void writeBitmap(IBinaryWriter& writer) override;

private:
    int cellColorPrecision_;
};

} // namespace opencgm

#endif // OPENCGM_GRAPHICAL_PRIMITIVE_COMMANDS_H
