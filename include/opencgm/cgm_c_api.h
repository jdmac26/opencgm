/**
 * @file cgm_c_api.h
 * @brief Legacy CGM C API (handle-based file operations)
 *
 * @deprecated This API is maintained for backward compatibility only.
 * **New code should use c_api.h (opencgm_* functions) instead.**
 *
 * The OpenCGM API (c_api.h) provides:
 * - Context-based architecture for better state management
 * - More comprehensive configuration options
 * - Better error handling with detailed messages
 * - XCF companion file generation
 * - Profile-based conversion (WebCGM, S1000D, ATA2200)
 *
 * Migration guide:
 * @code
 * // Legacy (this API):
 * CGMFileHandle file = cgm_load_file("input.cgm", &error);
 * SVGConverterHandle conv = cgm_create_svg_converter(file, &error);
 * cgm_save_svg_to_file(conv, "output.svg");
 * cgm_free_svg_converter(conv);
 * cgm_free_file(file);
 *
 * // Recommended (c_api.h):
 * opencgm_ctx_t* ctx = opencgm_create();
 * opencgm_set_profile(ctx, "webcgm");
 * opencgm_convert_cgm_to_svg(ctx, "input.cgm", "output.svg");
 * opencgm_destroy(ctx);
 * @endcode
 */

#ifndef OPENCGM_CGM_C_API_H
#define OPENCGM_CGM_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// Type Definitions
// ============================================================================

// Opaque handle to CGM file
typedef struct CGMFile_t* CGMFileHandle;

// Opaque handle to SVG converter
typedef struct SVGConverter_t* SVGConverterHandle;

// Command types (matches ClassCode enum)
typedef enum {
    CGM_CLASS_DELIMITER = 0,
    CGM_CLASS_METAFILE_DESCRIPTOR = 1,
    CGM_CLASS_PICTURE_DESCRIPTOR = 2,
    CGM_CLASS_CONTROL = 3,
    CGM_CLASS_GRAPHICAL_PRIMITIVE = 4,
    CGM_CLASS_ATTRIBUTE = 5,
    CGM_CLASS_ESCAPE = 6,
    CGM_CLASS_EXTERNAL = 7,
    CGM_CLASS_SEGMENT = 8,
    CGM_CLASS_APPLICATION_STRUCTURE = 9
} CGMClassCode;

// Error codes
typedef enum {
    CGM_SUCCESS = 0,
    CGM_ERROR_FILE_NOT_FOUND = -1,
    CGM_ERROR_INVALID_FORMAT = -2,
    CGM_ERROR_PARSE_ERROR = -3,
    CGM_ERROR_NULL_HANDLE = -4,
    CGM_ERROR_OUT_OF_BOUNDS = -5,
    CGM_ERROR_INVALID_PARAMETER = -6,
    CGM_ERROR_CONVERSION_FAILED = -7,
    CGM_ERROR_OUT_OF_MEMORY = -8
} CGMErrorCode;

// Point structure
typedef struct {
    double x;
    double y;
} CGMPoint_t;

// Color structure
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} CGMColor_t;

// Rectangle structure
typedef struct {
    double x1;
    double y1;
    double x2;
    double y2;
} CGMRect_t;

// SVG conversion options
typedef struct {
    bool embed_fonts;             /**< ABI-reserved; font embedding requires an explicit font map */
    bool optimize_paths;          /**< ABI-reserved; use the opencgm_* conversion API for optimization */
    bool include_metadata;        /**< Emit the root SVG title and OpenCGM conversion description */
    double scale_factor;          /**< ABI-reserved; profile-aware SVG coordinates are preserved */
    const char* background_color; /**< #RGB/#RRGGBB, "white", "black", "none", or NULL */
    bool text_as_path;           // Convert text to paths for pixel-perfect output
    const char* font_substitution; // Fallback font (e.g., "Arial", "System Default")
} SVGOptions_t;

// ============================================================================
// File Loading and Management
// ============================================================================

/**
 * Load a CGM file from disk
 * @param filename Path to CGM file
 * @param error_code Pointer to receive error code (can be NULL)
 * @return Handle to loaded CGM file, or NULL on error
 */
CGMFileHandle cgm_load_file(const char* filename, CGMErrorCode* error_code);

/**
 * Load a CGM file from memory buffer
 * @param data Pointer to CGM data in memory
 * @param size Size of data in bytes
 * @param error_code Pointer to receive error code (can be NULL)
 * @return Handle to loaded CGM file, or NULL on error
 */
CGMFileHandle cgm_load_from_memory(const uint8_t* data, size_t size, CGMErrorCode* error_code);

/**
 * Free a CGM file handle and release resources
 * @param handle CGM file handle to free
 */
void cgm_free_file(CGMFileHandle handle);

// ============================================================================
// File Information
// ============================================================================

/**
 * Get the number of commands in the CGM file
 * @param handle CGM file handle
 * @return Number of commands, or -1 on error
 */
int cgm_get_command_count(CGMFileHandle handle);

/**
 * Get the class of a specific command
 * @param handle CGM file handle
 * @param index Command index (0-based)
 * @return Command class code, or -1 on error
 */
int cgm_get_command_class(CGMFileHandle handle, int index);

/**
 * Get the element ID of a specific command
 * @param handle CGM file handle
 * @param index Command index (0-based)
 * @return Element ID, or -1 on error
 */
int cgm_get_command_id(CGMFileHandle handle, int index);

/**
 * Get string representation of a command
 * @param handle CGM file handle
 * @param index Command index (0-based)
 * @param buffer Buffer to receive string
 * @param buffer_size Size of buffer
 * @return Number of characters written (excluding null terminator), or -1 on error
 */
int cgm_get_command_string(CGMFileHandle handle, int index, char* buffer, size_t buffer_size);

// ============================================================================
// Metafile Properties
// ============================================================================

/**
 * Get metafile version
 * @param handle CGM file handle
 * @return Version number, or -1 on error
 */
int cgm_get_version(CGMFileHandle handle);

/**
 * Get metafile description
 * @param handle CGM file handle
 * @param buffer Buffer to receive description
 * @param buffer_size Size of buffer
 * @return Number of characters written, or -1 on error
 */
int cgm_get_description(CGMFileHandle handle, char* buffer, size_t buffer_size);

/**
 * Get VDC (Virtual Device Coordinate) extent
 * @param handle CGM file handle
 * @param rect Pointer to rectangle structure to receive extent
 * @return CGM_SUCCESS on success, error code on failure
 */
CGMErrorCode cgm_get_vdc_extent(CGMFileHandle handle, CGMRect_t* rect);

/**
 * Get background color
 * @param handle CGM file handle
 * @param color Pointer to color structure to receive color
 * @return CGM_SUCCESS on success, error code on failure
 */
CGMErrorCode cgm_get_background_color(CGMFileHandle handle, CGMColor_t* color);

/**
 * Get integer precision in bits
 * @param handle CGM file handle
 * @return Precision in bits (8, 16, 24, 32), or -1 on error
 */
int cgm_get_integer_precision(CGMFileHandle handle);

/**
 * Get color precision in bits
 * @param handle CGM file handle
 * @return Precision in bits (8, 16, 24, 32), or -1 on error
 */
int cgm_get_color_precision(CGMFileHandle handle);

/**
 * Check if file uses indexed colors
 * @param handle CGM file handle
 * @return true if indexed, false if direct colors, or false on error
 */
bool cgm_uses_indexed_colors(CGMFileHandle handle);

// ============================================================================
// SVG Conversion
// ============================================================================

/**
 * Create an SVG converter with default options
 * @param handle CGM file handle
 * @param error_code Pointer to receive error code (can be NULL)
 * @return Handle to SVG converter, or NULL on error
 */
SVGConverterHandle cgm_create_svg_converter(CGMFileHandle handle, CGMErrorCode* error_code);

/**
 * Create an SVG converter with custom options
 * @param handle CGM file handle
 * @param options Pointer to SVG options structure
 * @param error_code Pointer to receive error code (can be NULL)
 * @return Handle to SVG converter, or NULL on error
 */
SVGConverterHandle cgm_create_svg_converter_with_options(
    CGMFileHandle handle,
    const SVGOptions_t* options,
    CGMErrorCode* error_code
);

/**
 * Convert CGM to SVG string
 * @param converter SVG converter handle
 * @param buffer Buffer to receive SVG string (can be NULL to query size)
 * @param buffer_size Size of buffer
 * @return Required buffer size (including null terminator), or -1 on error
 */
int cgm_convert_to_svg(SVGConverterHandle converter, char* buffer, size_t buffer_size);

/**
 * Save SVG to file
 * @param converter SVG converter handle
 * @param filename Path to output SVG file
 * @return CGM_SUCCESS on success, error code on failure
 */
CGMErrorCode cgm_save_svg_to_file(SVGConverterHandle converter, const char* filename);

/**
 * Free an SVG converter handle
 * @param converter SVG converter handle to free
 */
void cgm_free_svg_converter(SVGConverterHandle converter);

// ============================================================================
// Error Handling
// ============================================================================

/**
 * Get error message for error code
 * @param error_code Error code
 * @return Error message string (static, do not free)
 */
const char* cgm_get_error_message(CGMErrorCode error_code);

/**
 * Get last error code from thread-local storage
 * @return Last error code
 */
CGMErrorCode cgm_get_last_error();

/**
 * Clear last error
 */
void cgm_clear_last_error();

// ============================================================================
// Native Raster Rendering (CGM Preview)
// ============================================================================

/**
 * Options for native CGM raster rendering
 */
typedef struct {
    int target_width;           /**< Target width (0 = auto from aspect ratio) */
    int target_height;          /**< Target height (0 = auto from aspect ratio) */
    double scale_factor;        /**< Scale factor for output (default 1.0) */
    uint8_t background_r;       /**< Background red component (default 255) */
    uint8_t background_g;       /**< Background green component (default 255) */
    uint8_t background_b;       /**< Background blue component (default 255) */
    uint8_t background_a;       /**< Background alpha component (default 255) */
    bool antialias;             /**< Enable antialiasing (default true) */
    bool fit_to_content;        /**< Fit viewbox to geometry bounds (default false) */
    int dpi;                    /**< DPI for text sizing (default 96) */
} CGMRenderOptions_t;

/**
 * Get default render options
 * @return Default options structure
 */
CGMRenderOptions_t cgm_get_default_render_options(void);

/**
 * Check if native rendering is available
 * @return true if Skia renderer is compiled in, false otherwise
 */
bool cgm_is_native_renderer_available(void);

/**
 * Render CGM to BGRA pixel buffer
 * @param handle CGM file handle
 * @param options Render options (can be NULL for defaults)
 * @param out_buffer Pointer to receive allocated pixel buffer (caller must free with cgm_free_render_buffer)
 * @param out_buffer_size Pointer to receive buffer size in bytes
 * @param out_width Pointer to receive rendered width
 * @param out_height Pointer to receive rendered height
 * @return CGM_SUCCESS on success, error code on failure
 *
 * The output buffer contains BGRA8888 pixels (4 bytes per pixel).
 * Row stride is width * 4 bytes.
 */
CGMErrorCode cgm_render_to_buffer(
    CGMFileHandle handle,
    const CGMRenderOptions_t* options,
    uint8_t** out_buffer,
    size_t* out_buffer_size,
    int* out_width,
    int* out_height
);

/**
 * Render CGM to BGRA pixel buffer with simple parameters
 * @param handle CGM file handle
 * @param target_width Target width (0 = auto)
 * @param target_height Target height (0 = auto)
 * @param scale_factor Scale multiplier
 * @param out_buffer Pointer to receive allocated pixel buffer
 * @param out_buffer_size Pointer to receive buffer size in bytes
 * @param out_width Pointer to receive rendered width
 * @param out_height Pointer to receive rendered height
 * @return CGM_SUCCESS on success, error code on failure
 */
CGMErrorCode cgm_render_to_buffer_simple(
    CGMFileHandle handle,
    int target_width,
    int target_height,
    double scale_factor,
    uint8_t** out_buffer,
    size_t* out_buffer_size,
    int* out_width,
    int* out_height
);

/**
 * Free a render buffer allocated by cgm_render_to_buffer
 * @param buffer Buffer to free
 */
void cgm_free_render_buffer(uint8_t* buffer);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Get library version string
 * @return Version string (static, do not free)
 */
const char* cgm_get_version_string();

/**
 * Get library build date
 * @return Build date string (static, do not free)
 */
const char* cgm_get_build_date();

/**
 * Check if a feature is supported
 * @param feature_name Name of feature to check
 * @return true if supported, false otherwise
 */
bool cgm_is_feature_supported(const char* feature_name);

// ============================================================================
// Platform-Specific Export Macros
// ============================================================================

#ifdef _WIN32
    #ifdef CGM_BUILD_SHARED
        #define CGM_API __declspec(dllexport)
    #else
        #define CGM_API __declspec(dllimport)
    #endif
#else
    #define CGM_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
}
#endif

#endif // OPENCGM_CGM_C_API_H
