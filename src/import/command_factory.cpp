#include "opencgm/command_factory.h"
#include "opencgm/commands/delimiter_commands.h"
#include "opencgm/commands/metafile_descriptor_commands.h"
#include "opencgm/commands/picture_descriptor_commands.h"
#include "opencgm/commands/graphical_primitive_commands.h"
#include "opencgm/commands/attribute_commands.h"
#include "opencgm/commands/control_commands.h"
#include "opencgm/commands/segment_control_commands.h"
#include "opencgm/commands/escape_commands.h"
#include "opencgm/commands/external_commands.h"
#include "opencgm/commands/application_structure_commands.h"
#include <iostream>

namespace opencgm {

std::unique_ptr<Command> DefaultCommandFactory::createCommand(
    int elementId,
    int elementClass,
    CGMFile* container) {

    ClassCode classCode = static_cast<ClassCode>(elementClass);

    switch (classCode) {
        case ClassCode::DelimiterElement:
            return createDelimiterElement(elementId, container);

        case ClassCode::MetafileDescriptorElements:
            return createMetafileDescriptorElement(elementId, container);

        case ClassCode::PictureDescriptorElements:
            return createPictureDescriptorElement(elementId, container);

        case ClassCode::ControlElements:
            return createControlElement(elementId, container);

        case ClassCode::GraphicalPrimitiveElements:
            return createGraphicalPrimitiveElement(elementId, container);

        case ClassCode::AttributeElements:
            return createAttributeElement(elementId, container);

        case ClassCode::EscapeElement:
            return createEscapeElement(elementId, container);

        case ClassCode::ExternalElements:
            return createExternalElement(elementId, container);

        case ClassCode::SegmentControlandSegmentAttributeElements:
            return createSegmentControlElement(elementId, container);

        case ClassCode::ApplicationStructureDescriptorElements:
            return createApplicationStructureElement(elementId, container);

        default:
            // For reserved element classes (10-15)
            Command::verify(elementClass >= 10 && elementClass <= 15,
                "Unsupported element class: " + std::to_string(elementClass));
            return std::make_unique<UnknownCommand>(elementId, elementClass, container);
    }
}

// ============================================================================
// Element class factory methods (stubs for now)
// ============================================================================

std::unique_ptr<Command> DefaultCommandFactory::createDelimiterElement(
    int elementId, CGMFile* container) {
    switch (elementId) {
        case 0: return std::make_unique<NoOp>(container);
        case 1: return std::make_unique<BeginMetafile>(container);
        case 2: return std::make_unique<EndMetafile>(container);
        case 3: return std::make_unique<BeginPicture>(container);
        case 4: return std::make_unique<BeginPictureBody>(container);
        case 5: return std::make_unique<EndPicture>(container);
        case 6: return std::make_unique<BeginSegment>(container);
        case 7: return std::make_unique<EndSegment>(container);
        case 8: return std::make_unique<BeginFigure>(container);
        case 9: return std::make_unique<EndFigure>(container);
        case 13: return std::make_unique<BeginProtectionRegion>(container);
        case 14: return std::make_unique<EndProtectionRegion>(container);
        case 15: return std::make_unique<BeginCompoundLine>(container);
        case 16: return std::make_unique<EndCompoundLine>(container);
        case 17: return std::make_unique<BeginCompoundTextPath>(container);
        case 18: return std::make_unique<EndCompoundTextPath>(container);
        case 19: return std::make_unique<BeginTileArray>(container);
        case 20: return std::make_unique<EndTileArray>(container);
        case 21: return std::make_unique<BeginApplicationStructure>(container);
        case 22: return std::make_unique<BeginApplicationStructureBody>(container);
        case 23: return std::make_unique<EndApplicationStructure>(container);
        default:
            return std::make_unique<UnknownCommand>(
                elementId, static_cast<int>(ClassCode::DelimiterElement), container);
    }
}

std::unique_ptr<Command> DefaultCommandFactory::createMetafileDescriptorElement(
    int elementId, CGMFile* container) {
    switch (elementId) {
        case 1: return std::make_unique<MetafileVersion>(container);
        case 2: return std::make_unique<MetafileDescription>(container);
        case 3: return std::make_unique<VDCTypeCommand>(container);
        case 4: return std::make_unique<IntegerPrecision>(container);
        case 5: return std::make_unique<RealPrecision>(container);
        case 6: return std::make_unique<IndexPrecision>(container);
        case 7: return std::make_unique<ColourPrecision>(container);
        case 8: return std::make_unique<ColourIndexPrecision>(container);
        case 9: return std::make_unique<MaximumColourIndex>(container);
        case 10: return std::make_unique<ColourValueExtent>(container);
        case 11: return std::make_unique<MetafileElementList>(container);
        case 12: return std::make_unique<MetafileDefaultsReplacement>(container);
        case 13: return std::make_unique<FontList>(container);
        case 14: return std::make_unique<CharacterSetList>(container);
        case 15: return std::make_unique<CharacterCodingAnnouncer>(container);
        case 16: return std::make_unique<NamePrecision>(container);
        case 17: return std::make_unique<MaximumVDCExtent>(container);
        case 18: return std::make_unique<SegmentPriorityExtent>(container);
        case 19: return std::make_unique<ColourModel>(container);
        case 20: return std::make_unique<ColourCalibration>(container);
        case 21: return std::make_unique<FontProperties>(container);
        case 22: return std::make_unique<GlyphMapping>(container);
        case 23: return std::make_unique<SymbolLibraryList>(container);
        case 24: return std::make_unique<PictureDirectory>(container);
        default:
            return std::make_unique<UnknownCommand>(
                elementId, static_cast<int>(ClassCode::MetafileDescriptorElements), container);
    }
}

std::unique_ptr<Command> DefaultCommandFactory::createPictureDescriptorElement(
    int elementId, CGMFile* container) {
    switch (elementId) {
        case 1: return std::make_unique<ScalingMode>(container);
        case 2: return std::make_unique<ColourSelectionMode>(container);
        case 3: return std::make_unique<LineWidthSpecificationMode>(container);
        case 4: return std::make_unique<MarkerSizeSpecificationMode>(container);
        case 5: return std::make_unique<EdgeWidthSpecificationMode>(container);
        case 6: return std::make_unique<VDCExtent>(container);
        case 7: return std::make_unique<BackgroundColour>(container);
        case 8: return std::make_unique<DeviceViewport>(container);
        case 9: return std::make_unique<DeviceViewportSpecificationMode>(container);
        case 10: return std::make_unique<DeviceViewportMapping>(container);
        case 11: return std::make_unique<LineRepresentation>(container);
        case 12: return std::make_unique<MarkerRepresentation>(container);
        case 13: return std::make_unique<TextRepresentation>(container);
        case 14: return std::make_unique<FillRepresentation>(container);
        case 15: return std::make_unique<EdgeRepresentation>(container);
        case 16: return std::make_unique<InteriorStyleSpecificationMode>(container);
        case 17: return std::make_unique<LineAndEdgeTypeDefinition>(container);
        case 18: return std::make_unique<HatchStyleDefinition>(container);
        case 19: return std::make_unique<GeometricPatternDefinition>(container);
        case 20: return std::make_unique<ApplicationStructureDirectory>(container);
        default:
            return std::make_unique<UnknownCommand>(
                elementId, static_cast<int>(ClassCode::PictureDescriptorElements), container);
    }
}

std::unique_ptr<Command> DefaultCommandFactory::createControlElement(
    int elementId, CGMFile* container) {
    switch (elementId) {
        case 1: return std::make_unique<VdcIntegerPrecision>(container);
        case 2: return std::make_unique<VdcRealPrecision>(container);
        case 3: return std::make_unique<AuxiliaryColour>(container);
        case 4: return std::make_unique<Transparency>(container);
        case 5: return std::make_unique<ClipRectangle>(container);
        case 6: return std::make_unique<ClipIndicator>(container);
        case 7: return std::make_unique<LineClippingMode>(container);
        case 8: return std::make_unique<MarkerClippingMode>(container);
        case 9: return std::make_unique<EdgeClippingMode>(container);
        case 10: return std::make_unique<NewRegion>(container);
        case 11: return std::make_unique<SavePrimitiveContext>(container);
        case 12: return std::make_unique<RestorePrimitiveContext>(container);
        case 17: return std::make_unique<ProtectionRegionIndicator>(container);
        case 18: return std::make_unique<GeneralizedTextPathMode>(container);
        case 19: return std::make_unique<MitreLimit>(container);
        case 20: return std::make_unique<TransparentCellColour>(container);
        default:
            return std::make_unique<UnknownCommand>(
                elementId, static_cast<int>(ClassCode::ControlElements), container);
    }
}

std::unique_ptr<Command> DefaultCommandFactory::createGraphicalPrimitiveElement(
    int elementId, CGMFile* container) {
    switch (elementId) {
        case 1: return std::make_unique<Polyline>(container);
        case 2: return std::make_unique<DisjointPolyline>(container);
        case 3: return std::make_unique<Polymarker>(container);
        case 4: return std::make_unique<Text>(container);
        case 5: return std::make_unique<RestrictedText>(container);
        case 6: return std::make_unique<AppendText>(container);
        case 7: return std::make_unique<Polygon>(container);
        case 8: return std::make_unique<PolygonSet>(container);
        case 9: return std::make_unique<CellArray>(container);
        case 10: return std::make_unique<GeneralizedDrawingPrimitive>(container);
        case 11: return std::make_unique<Rectangle>(container);
        case 12: return std::make_unique<Circle>(container);
        case 13: return std::make_unique<CircularArc3Point>(container);
        case 14: return std::make_unique<CircularArc3PointClose>(container);
        case 15: return std::make_unique<CircularArcCentre>(container);
        case 16: return std::make_unique<CircularArcCentreClose>(container);
        case 17: return std::make_unique<Ellipse>(container);
        case 18: return std::make_unique<EllipticalArc>(container);
        case 19: return std::make_unique<EllipticalArcClose>(container);
        case 20: return std::make_unique<CircularArcCentreReversed>(container);
        case 21: return std::make_unique<ConnectingEdge>(container);
        case 22: return std::make_unique<HyperbolicArc>(container);
        case 23: return std::make_unique<ParabolicArc>(container);
        case 24: return std::make_unique<NonUniformBSpline>(container);
        case 25: return std::make_unique<NonUniformRationalBSpline>(container);
        case 26: return std::make_unique<PolyBezier>(container);
        case 27: return std::make_unique<PolySymbol>(container);
        case 28: return std::make_unique<BitonalTile>(container);
        case 29: return std::make_unique<Tile>(container);
        default:
            return std::make_unique<UnknownCommand>(
                elementId, static_cast<int>(ClassCode::GraphicalPrimitiveElements), container);
    }
}

std::unique_ptr<Command> DefaultCommandFactory::createAttributeElement(
    int elementId, CGMFile* container) {
    switch (elementId) {
        case 1: return std::make_unique<LineBundleIndex>(container);
        case 2: return std::make_unique<LineType>(container);
        case 3: return std::make_unique<LineWidth>(container);
        case 4: return std::make_unique<LineColour>(container);
        case 5: return std::make_unique<MarkerBundleIndex>(container);
        case 6: return std::make_unique<MarkerType>(container);
        case 7: return std::make_unique<MarkerSize>(container);
        case 8: return std::make_unique<MarkerColour>(container);
        case 9: return std::make_unique<TextBundleIndex>(container);
        case 10: return std::make_unique<TextFontIndex>(container);
        case 11: return std::make_unique<TextPrecision>(container);
        case 12: return std::make_unique<CharacterExpansionFactor>(container);
        case 13: return std::make_unique<CharacterSpacing>(container);
        case 14: return std::make_unique<TextColour>(container);
        case 15: return std::make_unique<CharacterHeight>(container);
        case 16: return std::make_unique<CharacterOrientation>(container);
        case 17: return std::make_unique<TextPath>(container);
        case 18: return std::make_unique<TextAlignment>(container);
        case 19: return std::make_unique<CharacterSetIndex>(container);
        case 20: return std::make_unique<AlternateCharacterSetIndex>(container);
        // ISO/IEC 8632-1 standard mapping for fill-related attributes
        case 21: return std::make_unique<FillBundleIndex>(container);
        case 22: return std::make_unique<InteriorStyle>(container);
        case 23: return std::make_unique<FillColour>(container);
        case 24: return std::make_unique<HatchIndex>(container);
        case 25: return std::make_unique<PatternIndex>(container);
        case 26: return std::make_unique<EdgeBundleIndex>(container);
        case 27: return std::make_unique<EdgeType>(container);
        case 28: return std::make_unique<EdgeWidth>(container);
        case 29: return std::make_unique<EdgeColour>(container);
        case 30: return std::make_unique<EdgeVisibility>(container);
        case 31: return std::make_unique<FillReferencePoint>(container);
        case 32: return std::make_unique<PatternTable>(container);
        case 33: return std::make_unique<PatternSize>(container);   // ISO 8632-3: Element 33
        case 34: return std::make_unique<ColourTable>(container);   // ISO 8632-3: Element 34
        case 35: return std::make_unique<AspectSourceFlags>(container);
        case 36: return std::make_unique<PickIdentifier>(container);
        case 37: return std::make_unique<LineCap>(container);
        case 38: return std::make_unique<LineJoin>(container);
        case 39: return std::make_unique<LineTypeContinuation>(container);
        case 40: return std::make_unique<LineTypeInitialOffset>(container);
        case 41: return std::make_unique<TextScoreType>(container);
        case 42: return std::make_unique<RestrictedTextType>(container);
        case 43: return std::make_unique<InterpolatedInterior>(container);
        case 44: return std::make_unique<EdgeCap>(container);
        case 45: return std::make_unique<EdgeJoin>(container);
        case 46: return std::make_unique<EdgeTypeContinuation>(container);
        case 47: return std::make_unique<EdgeTypeInitialOffset>(container);
        default:
            return std::make_unique<UnknownCommand>(
                elementId, static_cast<int>(ClassCode::AttributeElements), container);
    }
}

std::unique_ptr<Command> DefaultCommandFactory::createExternalElement(
    int elementId, CGMFile* container) {
    switch (elementId) {
        case 1: return std::make_unique<MessageCommand>(container);
        case 2: return std::make_unique<ApplicationData>(container);
        default:
            return std::make_unique<UnknownCommand>(
                elementId, static_cast<int>(ClassCode::ExternalElements), container);
    }
}

std::unique_ptr<Command> DefaultCommandFactory::createSegmentControlElement(
    int elementId, CGMFile* container) {
    switch (elementId) {
        case 1: return std::make_unique<CopySegment>(container);
        case 2: return std::make_unique<InheritanceFilter>(container);
        case 3: return std::make_unique<ClipInheritanceCommand>(container);
        case 4: return std::make_unique<SegmentTransformation>(container);
        case 5: return std::make_unique<SegmentHighlighting>(container);
        case 6: return std::make_unique<SegmentDisplayPriority>(container);
        case 7: return std::make_unique<SegmentPickPriority>(container);
        default:
            return std::make_unique<UnknownCommand>(
                elementId, static_cast<int>(ClassCode::SegmentControlandSegmentAttributeElements), container);
    }
}

std::unique_ptr<Command> DefaultCommandFactory::createApplicationStructureElement(
    int elementId, CGMFile* container) {
    switch (elementId) {
        case 1: return std::make_unique<ApplicationStructureAttribute>(container);
        default:
            return std::make_unique<UnknownCommand>(
                elementId, static_cast<int>(ClassCode::ApplicationStructureDescriptorElements), container);
    }
}

std::unique_ptr<Command> DefaultCommandFactory::createEscapeElement(
    [[maybe_unused]] int elementId, CGMFile* container) {
    // Escape elements can have any element ID
    return std::make_unique<Escape>(container);
}

} // namespace opencgm
