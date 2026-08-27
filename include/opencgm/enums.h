#ifndef OPENCGM_ENUMS_H
#define OPENCGM_ENUMS_H

namespace opencgm {

/**
 * @brief CGM Element Class Codes
 */
enum class ClassCode {
    DelimiterElement = 0,
    MetafileDescriptorElements = 1,
    PictureDescriptorElements = 2,
    ControlElements = 3,
    GraphicalPrimitiveElements = 4,
    AttributeElements = 5,
    EscapeElement = 6,
    ExternalElements = 7,
    SegmentControlandSegmentAttributeElements = 8,
    ApplicationStructureDescriptorElements = 9
};

/**
 * @brief Specification modes for line width, marker size, etc.
 */
enum class SpecificationMode {
    ABS,     // Absolute
    SCALED   // Scaled
};

/**
 * @brief Precision for real numbers
 */
enum class Precision {
    Fixed_32,
    Fixed_64,
    Floating_32,
    Floating_64
};

/**
 * @brief VDC Type
 */
enum class VDCType {
    Integer,
    Real
};

/**
 * @brief Closure types for arcs
 */
enum class ClosureType {
    PIE = 0,
    CHORD = 1
};

/**
 * @brief Dash types for lines
 */
enum class DashType {
    SOLID = 1,
    DASH = 2,
    DOT = 3,
    DASH_DOT = 4,
    DASH_DOT_DOT = 5
};

/**
 * @brief Line cap indicators
 */
enum class LineCapIndicator {
    UNSPECIFIED = 1,
    BUTT = 2,
    ROUND = 3,
    PROJECTING_SQUARE = 4,
    TRIANGLE = 5
};

/**
 * @brief Join indicators for lines
 */
enum class JoinIndicator {
    UNSPECIFIED = 1,
    MITER = 2,
    ROUND = 3,
    BEVEL = 4
};

/**
 * @brief Message severity levels
 */
enum class Severity {
    Unsupported = 0,
    Unimplemented = 1,
    Fatal = 2
};

/**
 * @brief Clipping modes
 */
enum class ClippingMode {
    LOCUS = 0,
    SHAPE = 1,
    LOCUSTHENSHAPE = 2
};

/**
 * @brief Color selection modes
 */
enum class ColorSelectionMode {
    INDEXED = 0,
    DIRECT = 1
};

/**
 * @brief Color model types
 */
enum class ColorModel {
    RGB = 1,
    CIELAB = 2,
    CIELUV = 3,
    CMYK = 4,
    RGB_RELATED = 5
};

/**
 * @brief Compression types
 */
enum class CompressionType {
    NULL_BACKGROUND = 0,
    RUN_LENGTH = 1,
    PACKED = 2,
    T4 = 3,
    T6 = 4,
    BITMAP = 5,
    JPEG = 6,
    PNG = 7
};

/**
 * @brief Text precision types
 */
enum class TextPrecisionType {
    STRING = 0,
    CHAR = 1,
    STROKE = 2
};

/**
 * @brief Text path directions
 */
enum class TextPathType {
    RIGHT = 0,
    LEFT = 1,
    UP = 2,
    DOWN = 3
};

/**
 * @brief Dash cap indicators
 */
enum class DashCapIndicator {
    UNSPECIFIED = 1,
    BUTT = 2,
    MATCH = 3
};

/**
 * @brief Clip inheritance values
 */
enum class ClipInheritance {
    STATE_LIST = 0,
    INTERSECTION = 1
};

/**
 * @brief Restricted text types
 */
enum class RestrictedTextTypeValue {
    BASIC = 1,
    BOXED_CAP = 2,
    BOXED_ALL = 3,
    ISOTROPIC_CAP = 4,
    ISOTROPIC_ALL = 5,
    JUSTIFIED = 6
};

/**
 * @brief Text score types
 */
enum class TextScoreTypeValue {
    NOT_UNDERLINED = 1,
    UNDERLINED = 2,
    NOT_OVERLINED = 3,
    OVERLINED = 4,
    NOT_STRIKETHROUGH = 5,
    STRIKETHROUGH = 6
};

/**
 * @brief Aspect source flags
 */
enum class AspectSourceFlagType {
    LINE_TYPE = 0,
    LINE_WIDTH = 1,
    LINE_COLOUR = 2,
    MARKER_TYPE = 3,
    MARKER_SIZE = 4,
    MARKER_COLOUR = 5,
    TEXT_FONT_INDEX = 6,
    TEXT_PRECISION = 7,
    CHARACTER_EXPANSION_FACTOR = 8,
    CHARACTER_SPACING = 9,
    TEXT_COLOUR = 10,
    INTERIOR_STYLE = 11,
    FILL_COLOUR = 12,
    HATCH_INDEX = 13,
    PATTERN_INDEX = 14,
    EDGE_TYPE = 15,
    EDGE_WIDTH = 16,
    EDGE_COLOUR = 17
};

/**
 * @brief Aspect source flag values
 */
enum class AspectSourceFlagValue {
    INDIVIDUAL = 0,
    BUNDLED = 1
};

/**
 * @brief Segment highlighting values
 */
enum class SegmentHighlightingValue {
    NORMAL = 0,
    HIGHL = 1
};

/**
 * @brief Message action types
 */
enum class MessageAction {
    NO_ACTION = 0,
    ACTION = 1
};

/**
 * @brief Application structure directory data type selector
 */
enum class ApplicationStructureDataTypeSelector {
    UI8 = 0,
    UI16 = 1,
    UI32 = 2
};

} // namespace opencgm

#endif // OPENCGM_ENUMS_H