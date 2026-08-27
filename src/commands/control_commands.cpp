#include "opencgm/commands/control_commands.h"
#include "opencgm/interfaces.h"
#include "opencgm/cgm_file.h"

namespace opencgm {

// ============================================================================
// VDC INTEGER PRECISION
// ============================================================================

VdcIntegerPrecision::VdcIntegerPrecision(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::ControlElements, 1, container)),
      precision_(16) {}

void VdcIntegerPrecision::readFromBinary(IBinaryReader& reader) {
    precision_ = reader.readInt();

    if (container_) {
        container_->setVdcIntegerPrecision(precision_);
    }
}

void VdcIntegerPrecision::writeAsBinary(IBinaryWriter& writer) {
    writer.writeInt(precision_);
}

void VdcIntegerPrecision::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("VDCINTEGERPREC ");
    writer.writeString(std::to_string(precision_));
    writer.writeString(";");
}

std::string VdcIntegerPrecision::toString() const {
    return "VdcIntegerPrecision[" + std::to_string(precision_) + " bits]";
}

// ============================================================================
// VDC REAL PRECISION
// ============================================================================

VdcRealPrecision::VdcRealPrecision(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::ControlElements, 2, container)),
      precision_(Precision::Floating_32) {}

void VdcRealPrecision::readFromBinary(IBinaryReader& reader) {
    int form = reader.readEnum(); // 0=floating, 1=fixed
    int p2 = reader.readInt();     // For floating: exponent or field width
    int p3 = reader.readInt();     // For floating: mantissa or exponent width

    // Determine precision (matching C# logic exactly)
    if (form == 0) {
        // Floating point
        if (p2 == 9 && p3 == 23) {
            precision_ = Precision::Floating_32;
        } else if (p2 == 12 && p3 == 52) {
            precision_ = Precision::Floating_64;
        } else {
            // Default to 32 if unknown
            precision_ = Precision::Floating_32;
        }
    } else {
        // Fixed point
        if (p2 == 16 && p3 == 16) {
            precision_ = Precision::Fixed_32;
        } else if (p2 == 32 && p3 == 32) {
            precision_ = Precision::Fixed_64;
        } else {
            // Default
            precision_ = Precision::Fixed_32;
        }
    }

    if (container_) {
        container_->setVdcRealPrecision(precision_);
    }
}

void VdcRealPrecision::writeAsBinary(IBinaryWriter& writer) {
    int form = (precision_ == Precision::Floating_32 || precision_ == Precision::Floating_64) ? 0 : 1;
    int fieldWidth = (precision_ == Precision::Floating_32 || precision_ == Precision::Fixed_32) ? 32 : 64;
    int exponentWidth = (fieldWidth == 32) ? 9 : 12;

    writer.writeEnum(form);
    writer.writeInt(fieldWidth);
    writer.writeInt(exponentWidth);
}

void VdcRealPrecision::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("VDCREALPREC ");
    switch (precision_) {
        case Precision::Floating_32: writer.writeString("32,9,23"); break;
        case Precision::Floating_64: writer.writeString("64,12,52"); break;
        case Precision::Fixed_32: writer.writeString("FIXED 32,16,16"); break;
        case Precision::Fixed_64: writer.writeString("FIXED 64,32,32"); break;
    }
    writer.writeString(";");
}

std::string VdcRealPrecision::toString() const {
    std::string precStr;
    switch (precision_) {
        case Precision::Floating_32: precStr = "Float32"; break;
        case Precision::Floating_64: precStr = "Float64"; break;
        case Precision::Fixed_32: precStr = "Fixed32"; break;
        case Precision::Fixed_64: precStr = "Fixed64"; break;
    }
    return "VdcRealPrecision[" + precStr + "]";
}

// ============================================================================
// AUXILIARY COLOUR
// ============================================================================

AuxiliaryColour::AuxiliaryColour(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::ControlElements, 3, container)),
      color_(Color::White()) {}

void AuxiliaryColour::readFromBinary(IBinaryReader& reader) {
    color_ = reader.readColor();
}

void AuxiliaryColour::writeAsBinary(IBinaryWriter& writer) {
    writer.writeColor(color_);
}

void AuxiliaryColour::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("AUXCOLR /* color */;");
}

std::string AuxiliaryColour::toString() const {
    return "AuxiliaryColour[" + color_.toString() + "]";
}

// ============================================================================
// TRANSPARENCY
// ============================================================================

Transparency::Transparency(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::ControlElements, 4, container)),
      indicator_(0) {}

void Transparency::readFromBinary(IBinaryReader& reader) {
    indicator_ = reader.readEnum();
}

void Transparency::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(indicator_);
}

void Transparency::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("TRANSPARENCY ");
    writer.writeString(indicator_ == 0 ? "OFF" : "ON");
    writer.writeString(";");
}

std::string Transparency::toString() const {
    return std::string("Transparency[") + (indicator_ == 0 ? "OFF" : "ON") + "]";
}

// ============================================================================
// CLIP RECTANGLE
// ============================================================================

ClipRectangle::ClipRectangle(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::ControlElements, 5, container)),
      firstCorner_(0, 0),
      secondCorner_(0, 0) {}

void ClipRectangle::readFromBinary(IBinaryReader& reader) {
    firstCorner_ = reader.readPoint();
    secondCorner_ = reader.readPoint();
}

void ClipRectangle::writeAsBinary(IBinaryWriter& writer) {
    writer.writePoint(firstCorner_);
    writer.writePoint(secondCorner_);
}

void ClipRectangle::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("CLIPRECT (");
    writer.writeString(std::to_string(firstCorner_.x()) + "," + std::to_string(firstCorner_.y()));
    writer.writeString(") (");
    writer.writeString(std::to_string(secondCorner_.x()) + "," + std::to_string(secondCorner_.y()));
    writer.writeString(");");
}

std::string ClipRectangle::toString() const {
    return "ClipRectangle[(" +
           std::to_string(firstCorner_.x()) + "," + std::to_string(firstCorner_.y()) + ") (" +
           std::to_string(secondCorner_.x()) + "," + std::to_string(secondCorner_.y()) + ")]";
}

// ============================================================================
// CLIP INDICATOR
// ============================================================================

ClipIndicator::ClipIndicator(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::ControlElements, 6, container)),
      indicator_(0) {}

void ClipIndicator::readFromBinary(IBinaryReader& reader) {
    indicator_ = reader.readEnum();
}

void ClipIndicator::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(indicator_);
}

void ClipIndicator::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("CLIP ");
    writer.writeString(indicator_ == 0 ? "OFF" : "ON");
    writer.writeString(";");
}

std::string ClipIndicator::toString() const {
    return std::string("ClipIndicator[") + (indicator_ == 0 ? "OFF" : "ON") + "]";
}

// ============================================================================
// LINE CLIPPING MODE
// ============================================================================

LineClippingMode::LineClippingMode(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::ControlElements, 7, container)),
      mode_(0) {}

void LineClippingMode::readFromBinary(IBinaryReader& reader) {
    mode_ = reader.readEnum();
}

void LineClippingMode::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(mode_);
}

void LineClippingMode::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("LINECLIPMODE ");
    switch (mode_) {
        case 0: writer.writeString("LOCUS"); break;
        case 1: writer.writeString("SHAPE"); break;
        case 2: writer.writeString("LOCUSTHENSHAPE"); break;
        default: writer.writeString(std::to_string(mode_)); break;
    }
    writer.writeString(";");
}

std::string LineClippingMode::toString() const {
    std::string modeStr;
    switch (mode_) {
        case 0: modeStr = "LOCUS"; break;
        case 1: modeStr = "SHAPE"; break;
        case 2: modeStr = "LOCUS_THEN_SHAPE"; break;
        default: modeStr = std::to_string(mode_); break;
    }
    return "LineClippingMode[" + modeStr + "]";
}

// ============================================================================
// MARKER CLIPPING MODE
// ============================================================================

MarkerClippingMode::MarkerClippingMode(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::ControlElements, 8, container)),
      mode_(0) {}

void MarkerClippingMode::readFromBinary(IBinaryReader& reader) {
    mode_ = reader.readEnum();
}

void MarkerClippingMode::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(mode_);
}

void MarkerClippingMode::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("MARKERCLIPMODE ");
    writer.writeString(mode_ == 0 ? "LOCUS" : "SHAPE");
    writer.writeString(";");
}

std::string MarkerClippingMode::toString() const {
    return std::string("MarkerClippingMode[") + (mode_ == 0 ? "LOCUS" : "SHAPE") + "]";
}

// ============================================================================
// EDGE CLIPPING MODE
// ============================================================================

EdgeClippingMode::EdgeClippingMode(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::ControlElements, 9, container)),
      mode_(0) {}

void EdgeClippingMode::readFromBinary(IBinaryReader& reader) {
    mode_ = reader.readEnum();
}

void EdgeClippingMode::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(mode_);
}

void EdgeClippingMode::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("EDGECLIPMODE ");
    switch (mode_) {
        case 0: writer.writeString("LOCUS"); break;
        case 1: writer.writeString("SHAPE"); break;
        case 2: writer.writeString("LOCUSTHENSHAPE"); break;
        default: writer.writeString(std::to_string(mode_)); break;
    }
    writer.writeString(";");
}

std::string EdgeClippingMode::toString() const {
    std::string modeStr;
    switch (mode_) {
        case 0: modeStr = "LOCUS"; break;
        case 1: modeStr = "SHAPE"; break;
        case 2: modeStr = "LOCUS_THEN_SHAPE"; break;
        default: modeStr = std::to_string(mode_); break;
    }
    return "EdgeClippingMode[" + modeStr + "]";
}

// ============================================================================
// NEW REGION
// ============================================================================

NewRegion::NewRegion(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::ControlElements, 10, container)) {}

void NewRegion::readFromBinary(IBinaryReader& /* reader */) {
    // No parameters
}

void NewRegion::writeAsBinary(IBinaryWriter& /* writer */) {
    // No parameters
}

void NewRegion::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("NEWREGION;");
}

std::string NewRegion::toString() const {
    return "NewRegion";
}

// ============================================================================
// SAVE PRIMITIVE CONTEXT
// ============================================================================

SavePrimitiveContext::SavePrimitiveContext(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::ControlElements, 11, container)),
      contextName_(0) {}

void SavePrimitiveContext::readFromBinary(IBinaryReader& reader) {
    contextName_ = reader.readName();
}

void SavePrimitiveContext::writeAsBinary(IBinaryWriter& writer) {
    writer.writeName(contextName_);
}

void SavePrimitiveContext::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("SAVEPRIMCONTEXT ");
    writer.writeString(std::to_string(contextName_));
    writer.writeString(";");
}

std::string SavePrimitiveContext::toString() const {
    return "SavePrimitiveContext[" + std::to_string(contextName_) + "]";
}

// ============================================================================
// RESTORE PRIMITIVE CONTEXT
// ============================================================================

RestorePrimitiveContext::RestorePrimitiveContext(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::ControlElements, 12, container)),
      contextName_(0) {}

void RestorePrimitiveContext::readFromBinary(IBinaryReader& reader) {
    contextName_ = reader.readName();
}

void RestorePrimitiveContext::writeAsBinary(IBinaryWriter& writer) {
    writer.writeName(contextName_);
}

void RestorePrimitiveContext::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("RESTOREPRIMCONTEXT ");
    writer.writeString(std::to_string(contextName_));
    writer.writeString(";");
}

std::string RestorePrimitiveContext::toString() const {
    return "RestorePrimitiveContext[" + std::to_string(contextName_) + "]";
}

// ============================================================================
// PROTECTION REGION INDICATOR
// ============================================================================

ProtectionRegionIndicator::ProtectionRegionIndicator(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::ControlElements, 17, container)),
      regionIndex_(0),
      indicator_(1) {}

void ProtectionRegionIndicator::readFromBinary(IBinaryReader& reader) {
    regionIndex_ = reader.readIndex();
    indicator_ = reader.readEnum();
}

void ProtectionRegionIndicator::writeAsBinary(IBinaryWriter& writer) {
    writer.writeIndex(regionIndex_);
    writer.writeEnum(indicator_);
}

void ProtectionRegionIndicator::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("PRTREGIONIND ");
    writer.writeString(std::to_string(regionIndex_));
    writer.writeString(" ");
    const char* label = "OFF";
    if (indicator_ == 2) {
        label = "CLIP";
    } else if (indicator_ == 3) {
        label = "SHIELD";
    }
    writer.writeString(label);
    writer.writeString(";");
}

std::string ProtectionRegionIndicator::toString() const {
    std::string label = "OFF";
    if (indicator_ == 2) {
        label = "CLIP";
    } else if (indicator_ == 3) {
        label = "SHIELD";
    }
    return "ProtectionRegionIndicator[region=" + std::to_string(regionIndex_) +
           ", " + label + "]";
}

// ============================================================================
// GENERALIZED TEXT PATH MODE
// ============================================================================

GeneralizedTextPathMode::GeneralizedTextPathMode(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::ControlElements, 18, container)),
      mode_(0) {}

void GeneralizedTextPathMode::readFromBinary(IBinaryReader& reader) {
    mode_ = reader.readEnum();
}

void GeneralizedTextPathMode::writeAsBinary(IBinaryWriter& writer) {
    writer.writeEnum(mode_);
}

void GeneralizedTextPathMode::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("GENTEXTPATHMODE ");
    switch (mode_) {
        case 0: writer.writeString("OFF"); break;
        case 1: writer.writeString("NONTANGENTIAL"); break;
        case 2: writer.writeString("AXISTANGENTIAL"); break;
        default: writer.writeString(std::to_string(mode_)); break;
    }
    writer.writeString(";");
}

std::string GeneralizedTextPathMode::toString() const {
    std::string modeStr;
    switch (mode_) {
        case 0: modeStr = "OFF"; break;
        case 1: modeStr = "NON_TANGENTIAL"; break;
        case 2: modeStr = "AXIS_TANGENTIAL"; break;
        default: modeStr = std::to_string(mode_); break;
    }
    return "GeneralizedTextPathMode[" + modeStr + "]";
}

// ============================================================================
// MITRE LIMIT
// ============================================================================

MitreLimit::MitreLimit(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::ControlElements, 19, container)),
      limit_(10.0) {}

void MitreLimit::readFromBinary(IBinaryReader& reader) {
    limit_ = reader.readReal();
}

void MitreLimit::writeAsBinary(IBinaryWriter& writer) {
    writer.writeReal(limit_);
}

void MitreLimit::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("MITRELIMIT ");
    writer.writeString(std::to_string(limit_));
    writer.writeString(";");
}

std::string MitreLimit::toString() const {
    return "MitreLimit[" + std::to_string(limit_) + "]";
}

// ============================================================================
// TRANSPARENT CELL COLOUR
// ============================================================================

TransparentCellColour::TransparentCellColour(CGMFile* container)
    : Command(CommandConstructorArguments(ClassCode::ControlElements, 20, container)),
      color_(Color::White()) {}

void TransparentCellColour::readFromBinary(IBinaryReader& reader) {
    color_ = reader.readColor();
}

void TransparentCellColour::writeAsBinary(IBinaryWriter& writer) {
    writer.writeColor(color_);
}

void TransparentCellColour::writeAsClearText(IClearTextWriter& writer) {
    writer.writeString("TRANSPCELLCOLR /* color */;");
}

std::string TransparentCellColour::toString() const {
    return "TransparentCellColour[" + color_.toString() + "]";
}

} // namespace opencgm
