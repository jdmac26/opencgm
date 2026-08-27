#ifndef OPENCGM_CONTROL_COMMANDS_H
#define OPENCGM_CONTROL_COMMANDS_H

#include "../command.h"
#include "../cgm_point.h"
#include "../cgm_color.h"
#include "../enums.h"

namespace opencgm {

// Element ID 1: VDC INTEGER PRECISION
class VdcIntegerPrecision : public Command {
public:
    explicit VdcIntegerPrecision(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int precision() const { return precision_; }
    void setPrecision(int prec) { precision_ = prec; }

private:
    int precision_; // 8, 16, 24, or 32 bits
};

// Element ID 2: VDC REAL PRECISION
class VdcRealPrecision : public Command {
public:
    explicit VdcRealPrecision(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    Precision precision() const { return precision_; }
    void setPrecision(Precision prec) { precision_ = prec; }

private:
    Precision precision_;
};

// Element ID 3: AUXILIARY COLOUR
class AuxiliaryColour : public Command {
public:
    explicit AuxiliaryColour(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMColor& color() const { return color_; }
    void setColor(const CGMColor& color) { color_ = color; }

private:
    CGMColor color_;
};

// Element ID 4: TRANSPARENCY
class Transparency : public Command {
public:
    explicit Transparency(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int indicator() const { return indicator_; }
    void setIndicator(int ind) { indicator_ = ind; }

private:
    int indicator_; // 0=off, 1=on
};

// Element ID 5: CLIP RECTANGLE
class ClipRectangle : public Command {
public:
    explicit ClipRectangle(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMPoint& firstCorner() const { return firstCorner_; }
    const CGMPoint& secondCorner() const { return secondCorner_; }
    void setCorners(const CGMPoint& first, const CGMPoint& second) {
        firstCorner_ = first;
        secondCorner_ = second;
    }

private:
    CGMPoint firstCorner_;
    CGMPoint secondCorner_;
};

// Element ID 6: CLIP INDICATOR
class ClipIndicator : public Command {
public:
    explicit ClipIndicator(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int indicator() const { return indicator_; }
    void setIndicator(int ind) { indicator_ = ind; }

private:
    int indicator_; // 0=off, 1=on
};

// Element ID 7: LINE CLIPPING MODE
class LineClippingMode : public Command {
public:
    explicit LineClippingMode(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int mode() const { return mode_; }
    void setMode(int m) { mode_ = m; }

private:
    int mode_; // 0=locus, 1=shape, 2=locus then shape
};

// Element ID 8: MARKER CLIPPING MODE
class MarkerClippingMode : public Command {
public:
    explicit MarkerClippingMode(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int mode() const { return mode_; }

private:
    int mode_; // 0=locus, 1=shape
};

// Element ID 9: EDGE CLIPPING MODE
class EdgeClippingMode : public Command {
public:
    explicit EdgeClippingMode(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int mode() const { return mode_; }

private:
    int mode_; // 0=locus, 1=shape, 2=locus then shape
};

// Element ID 10: NEW REGION
class NewRegion : public Command {
public:
    explicit NewRegion(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    // No parameters
};

// Element ID 11: SAVE PRIMITIVE CONTEXT
class SavePrimitiveContext : public Command {
public:
    explicit SavePrimitiveContext(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int contextName() const { return contextName_; }

private:
    int contextName_;
};

// Element ID 12: RESTORE PRIMITIVE CONTEXT
class RestorePrimitiveContext : public Command {
public:
    explicit RestorePrimitiveContext(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int contextName() const { return contextName_; }

private:
    int contextName_;
};

// Element ID 17: PROTECTION REGION INDICATOR
class ProtectionRegionIndicator : public Command {
public:
    explicit ProtectionRegionIndicator(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int regionIndex() const { return regionIndex_; }
    int indicator() const { return indicator_; }

private:
    int regionIndex_;
    int indicator_; // 1=off, 2=clip, 3=shield (WebCGM restricts to 1 or 2)
};

// Element ID 18: GENERALIZED TEXT PATH MODE
class GeneralizedTextPathMode : public Command {
public:
    explicit GeneralizedTextPathMode(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    int mode() const { return mode_; }

private:
    int mode_; // 0=off, 1=non-tangential, 2=axis tangential
};

// Element ID 19: MITRE LIMIT
class MitreLimit : public Command {
public:
    explicit MitreLimit(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    double limit() const { return limit_; }

private:
    double limit_;
};

// Element ID 20: TRANSPARENT CELL COLOUR
class TransparentCellColour : public Command {
public:
    explicit TransparentCellColour(CGMFile* container);

    void readFromBinary(IBinaryReader& reader) override;
    void writeAsBinary(IBinaryWriter& writer) override;
    void writeAsClearText(IClearTextWriter& writer) override;
    std::string toString() const override;

    const CGMColor& color() const { return color_; }

private:
    CGMColor color_;
};

} // namespace opencgm

#endif // OPENCGM_CONTROL_COMMANDS_H
