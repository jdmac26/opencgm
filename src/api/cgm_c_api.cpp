/**
 * @file cgm_c_api.cpp
 * @brief Legacy CGM C API implementation (handle-based)
 *
 * @deprecated This API is maintained for backward compatibility.
 * New code should use c_api.h (opencgm_* functions) instead.
 *
 * ## Thread Safety Model
 *
 * 1. **Thread-Local Error Storage**: g_last_error is thread_local, ensuring
 *    that cgm_get_last_error() returns the error from the calling thread only.
 *
 * 2. **Handle Isolation**: CGMFileHandle and SVGConverterHandle instances are
 *    independent. Do NOT share handles between threads without external
 *    synchronization.
 *
 * 3. **Exception Safety**: All C++ exceptions are caught and converted to
 *    CGMErrorCode values at the API boundary.
 */

#include "opencgm/cgm_c_api.h"
#include "opencgm/cgm_file.h"
#include "opencgm/binary_reader.h"
#include "opencgm/command_factory.h"
#include "opencgm/svg_converter.h"
#include "opencgm/skia_renderer.h"
#include "opencgm/version.h"
#include "opencgm/commands/metafile_descriptor_commands.h"
#include "opencgm/commands/picture_descriptor_commands.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cctype>
#include <memory>
#include <string>
#include <thread>
#include <cstdint>

// Thread-local error storage (see thread safety documentation above)
thread_local CGMErrorCode g_last_error = CGM_SUCCESS;

// Internal structures
struct CGMFile_t {
    std::shared_ptr<opencgm::BinaryCGMFile> file;
    std::string last_error_message;
};

struct SVGConverter_t {
    std::shared_ptr<opencgm::BinaryCGMFile> file_owner;
    std::string svg_output;
    opencgm::SVGConverter* converter;
};

// ============================================================================
// Helper Functions
// ============================================================================

static void set_last_error(CGMErrorCode code) {
    g_last_error = code;
}

static int hex_nibble(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    value = static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

static bool parse_background_color(
    const char* value,
    bool& has_background,
    opencgm::Color& color) {
    has_background = false;
    if (!value || value[0] == '\0') {
        return true;
    }

    std::string normalized(value);
    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });

    if (normalized == "none" || normalized == "transparent") {
        return true;
    }
    if (normalized == "white") {
        color = opencgm::Color::White();
        has_background = true;
        return true;
    }
    if (normalized == "black") {
        color = opencgm::Color::Black();
        has_background = true;
        return true;
    }

    if (normalized.size() == 4 && normalized[0] == '#') {
        const int red = hex_nibble(normalized[1]);
        const int green = hex_nibble(normalized[2]);
        const int blue = hex_nibble(normalized[3]);
        if (red < 0 || green < 0 || blue < 0) return false;
        color = opencgm::Color(
            static_cast<uint8_t>(red * 17),
            static_cast<uint8_t>(green * 17),
            static_cast<uint8_t>(blue * 17));
        has_background = true;
        return true;
    }

    if (normalized.size() == 7 && normalized[0] == '#') {
        int components[6];
        for (size_t index = 0; index < 6; ++index) {
            components[index] = hex_nibble(normalized[index + 1]);
            if (components[index] < 0) return false;
        }
        color = opencgm::Color(
            static_cast<uint8_t>(components[0] * 16 + components[1]),
            static_cast<uint8_t>(components[2] * 16 + components[3]),
            static_cast<uint8_t>(components[4] * 16 + components[5]));
        has_background = true;
        return true;
    }

    return false;
}

static bool validate_handle(CGMFileHandle handle) {
    if (!handle || !handle->file) {
        set_last_error(CGM_ERROR_NULL_HANDLE);
        return false;
    }
    return true;
}

static bool validate_converter(SVGConverterHandle converter) {
    if (!converter) {
        set_last_error(CGM_ERROR_NULL_HANDLE);
        return false;
    }
    return true;
}

// ============================================================================
// File Loading and Management
// ============================================================================

CGMFileHandle cgm_load_file(const char* filename, CGMErrorCode* error_code) {
    if (!filename) {
        set_last_error(CGM_ERROR_INVALID_PARAMETER);
        if (error_code) *error_code = CGM_ERROR_INVALID_PARAMETER;
        return nullptr;
    }

    // Open file and check if it exists first (before allocating)
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        set_last_error(CGM_ERROR_FILE_NOT_FOUND);
        if (error_code) *error_code = CGM_ERROR_FILE_NOT_FOUND;
        return nullptr;
    }
    file.close();

    try {
        // Use RAII pattern: allocate file first, then handle
        // If BinaryCGMFile throws, no cleanup needed
        auto cgm_file = std::make_shared<opencgm::BinaryCGMFile>(filename);
        auto handle = std::make_unique<CGMFile_t>();
        handle->file = std::move(cgm_file);

        set_last_error(CGM_SUCCESS);
        if (error_code) *error_code = CGM_SUCCESS;
        return handle.release();  // Caller owns via cgm_free_file
    }
    catch (const std::exception&) {
        set_last_error(CGM_ERROR_PARSE_ERROR);
        if (error_code) *error_code = CGM_ERROR_PARSE_ERROR;
        return nullptr;
    }
}

CGMFileHandle cgm_load_from_memory(const uint8_t* data, size_t size, CGMErrorCode* error_code) {
    if (!data || size == 0) {
        set_last_error(CGM_ERROR_INVALID_PARAMETER);
        if (error_code) *error_code = CGM_ERROR_INVALID_PARAMETER;
        return nullptr;
    }

    try {
        // Use RAII pattern for exception-safe allocation
        auto cgm_file = std::make_shared<opencgm::BinaryCGMFile>();

        // Create memory stream
        std::string str(reinterpret_cast<const char*>(data), size);
        std::istringstream stream(str, std::ios::binary);

        // Parse CGM from stream
        opencgm::DefaultCommandFactory factory;
        opencgm::DefaultBinaryReader reader(stream, cgm_file.get(), &factory);
        reader.readCommands();

        // Create handle and transfer ownership
        auto handle = std::make_unique<CGMFile_t>();
        handle->file = std::move(cgm_file);

        set_last_error(CGM_SUCCESS);
        if (error_code) *error_code = CGM_SUCCESS;
        return handle.release();  // Caller owns via cgm_free_file
    }
    catch (const std::exception&) {
        set_last_error(CGM_ERROR_PARSE_ERROR);
        if (error_code) *error_code = CGM_ERROR_PARSE_ERROR;
        return nullptr;
    }
}

void cgm_free_file(CGMFileHandle handle) {
    if (handle) {
        delete handle;
    }
}

// ============================================================================
// File Information
// ============================================================================

int cgm_get_command_count(CGMFileHandle handle) {
    if (!validate_handle(handle)) return -1;

    try {
        return static_cast<int>(handle->file->commands().size());
    }
    catch (...) {
        set_last_error(CGM_ERROR_PARSE_ERROR);
        return -1;
    }
}

int cgm_get_command_class(CGMFileHandle handle, int index) {
    if (!validate_handle(handle)) return -1;

    try {
        const auto& commands = handle->file->commands();
        if (index < 0 || index >= static_cast<int>(commands.size())) {
            set_last_error(CGM_ERROR_OUT_OF_BOUNDS);
            return -1;
        }

        return static_cast<int>(commands[index]->elementClass());
    }
    catch (...) {
        set_last_error(CGM_ERROR_PARSE_ERROR);
        return -1;
    }
}

int cgm_get_command_id(CGMFileHandle handle, int index) {
    if (!validate_handle(handle)) return -1;

    try {
        const auto& commands = handle->file->commands();
        if (index < 0 || index >= static_cast<int>(commands.size())) {
            set_last_error(CGM_ERROR_OUT_OF_BOUNDS);
            return -1;
        }

        return commands[index]->elementId();
    }
    catch (...) {
        set_last_error(CGM_ERROR_PARSE_ERROR);
        return -1;
    }
}

int cgm_get_command_string(CGMFileHandle handle, int index, char* buffer, size_t buffer_size) {
    if (!validate_handle(handle)) return -1;

    try {
        const auto& commands = handle->file->commands();
        if (index < 0 || index >= static_cast<int>(commands.size())) {
            set_last_error(CGM_ERROR_OUT_OF_BOUNDS);
            return -1;
        }

        std::string str = commands[index]->toString();

        // Only copy if we have a valid buffer with space
        if (buffer != nullptr && buffer_size > 0) {
            size_t copy_size = std::min(str.size(), buffer_size - 1);
            std::memcpy(buffer, str.c_str(), copy_size);
            buffer[copy_size] = '\0';
        }

        return static_cast<int>(str.size());
    }
    catch (...) {
        set_last_error(CGM_ERROR_PARSE_ERROR);
        return -1;
    }
}

// ============================================================================
// Metafile Properties
// ============================================================================

int cgm_get_version(CGMFileHandle handle) {
    if (!validate_handle(handle)) return -1;

    for (const auto& command : handle->file->commands()) {
        if (command->elementClass() == opencgm::ClassCode::MetafileDescriptorElements &&
            command->elementId() == 1) {
            if (const auto* version = dynamic_cast<const opencgm::MetafileVersion*>(command.get())) {
                set_last_error(CGM_SUCCESS);
                return version->version();
            }
        }
    }

    set_last_error(CGM_ERROR_INVALID_FORMAT);
    return -1;
}

int cgm_get_description(CGMFileHandle handle, char* buffer, size_t buffer_size) {
    if (!validate_handle(handle)) return -1;

    std::string description;
    for (const auto& command : handle->file->commands()) {
        if (command->elementClass() == opencgm::ClassCode::MetafileDescriptorElements &&
            command->elementId() == 2) {
            if (const auto* metadata = dynamic_cast<const opencgm::MetafileDescription*>(command.get())) {
                description = metadata->description();
                break;
            }
        }
    }

    if (buffer && buffer_size > 0) {
        const size_t copy_size = std::min(description.size(), buffer_size - 1);
        std::memcpy(buffer, description.data(), copy_size);
        buffer[copy_size] = '\0';
    }

    set_last_error(CGM_SUCCESS);
    return static_cast<int>(description.size());
}

CGMErrorCode cgm_get_vdc_extent(CGMFileHandle handle, CGMRect_t* rect) {
    if (!validate_handle(handle)) return CGM_ERROR_NULL_HANDLE;
    if (!rect) return CGM_ERROR_INVALID_PARAMETER;

    for (const auto& command : handle->file->commands()) {
        if (command->elementClass() == opencgm::ClassCode::PictureDescriptorElements &&
            command->elementId() == 6) {
            if (const auto* extent = dynamic_cast<const opencgm::VDCExtent*>(command.get())) {
                rect->x1 = extent->firstCorner().x();
                rect->y1 = extent->firstCorner().y();
                rect->x2 = extent->secondCorner().x();
                rect->y2 = extent->secondCorner().y();
                set_last_error(CGM_SUCCESS);
                return CGM_SUCCESS;
            }
        }
    }

    set_last_error(CGM_ERROR_INVALID_FORMAT);
    return CGM_ERROR_INVALID_FORMAT;
}

CGMErrorCode cgm_get_background_color(CGMFileHandle handle, CGMColor_t* color) {
    if (!validate_handle(handle)) return CGM_ERROR_NULL_HANDLE;
    if (!color) return CGM_ERROR_INVALID_PARAMETER;

    for (const auto& command : handle->file->commands()) {
        if (command->elementClass() == opencgm::ClassCode::PictureDescriptorElements &&
            command->elementId() == 7) {
            if (const auto* background = dynamic_cast<const opencgm::BackgroundColour*>(command.get())) {
                const auto& parsed = background->color();
                color->r = parsed.r;
                color->g = parsed.g;
                color->b = parsed.b;
                color->a = parsed.a;
                set_last_error(CGM_SUCCESS);
                return CGM_SUCCESS;
            }
        }
    }

    set_last_error(CGM_ERROR_INVALID_FORMAT);
    return CGM_ERROR_INVALID_FORMAT;
}

int cgm_get_integer_precision(CGMFileHandle handle) {
    if (!validate_handle(handle)) return -1;
    return handle->file->integerPrecision();
}

int cgm_get_color_precision(CGMFileHandle handle) {
    if (!validate_handle(handle)) return -1;
    return handle->file->colourPrecision();
}

bool cgm_uses_indexed_colors(CGMFileHandle handle) {
    if (!validate_handle(handle)) return false;
    set_last_error(CGM_SUCCESS);
    return handle->file->colorSelectionMode() == opencgm::ColorSelectionMode::INDEXED;
}

// ============================================================================
// SVG Conversion
// ============================================================================

SVGConverterHandle cgm_create_svg_converter(CGMFileHandle handle, CGMErrorCode* error_code) {
    SVGOptions_t default_options = {
        true,     // embed_fonts
        true,     // optimize_paths
        true,     // include_metadata
        1.0,      // scale_factor
        "#FFFFFF", // background_color
        false,    // text_as_path
        nullptr   // font_substitution
    };

    return cgm_create_svg_converter_with_options(handle, &default_options, error_code);
}

SVGConverterHandle cgm_create_svg_converter_with_options(
    CGMFileHandle handle,
    const SVGOptions_t* options,
    CGMErrorCode* error_code
) {
    if (!validate_handle(handle)) {
        if (error_code) *error_code = CGM_ERROR_NULL_HANDLE;
        return nullptr;
    }

    if (!options) {
        if (error_code) *error_code = CGM_ERROR_INVALID_PARAMETER;
        return nullptr;
    }

    try {
        bool has_background = false;
        opencgm::Color background;
        if (!parse_background_color(
                options->background_color,
                has_background,
                background)) {
            set_last_error(CGM_ERROR_INVALID_PARAMETER);
            if (error_code) *error_code = CGM_ERROR_INVALID_PARAMETER;
            return nullptr;
        }

        // Use RAII pattern: allocate SVGConverter first, then wrapper
        auto svg_converter = std::make_unique<opencgm::SVGConverter>(handle->file.get());
        svg_converter->setIncludeDocumentMetadata(options->include_metadata);
        if (has_background) {
            svg_converter->setOutputBackgroundColor(background);
        } else {
            svg_converter->clearOutputBackgroundColor();
        }

        // Apply text rendering options before creating wrapper
        if (options->text_as_path || options->font_substitution) {
            opencgm::SVGConverter::TextRenderOptions textOpts;
            textOpts.text_as_path = options->text_as_path;

            // Set font substitution as fallback stack if provided
            if (options->font_substitution && options->font_substitution[0] != '\0') {
                std::string fontSub = options->font_substitution;
                if (fontSub != "System Default") {
                    textOpts.font_fallback_stack.push_back(fontSub);
                    textOpts.font_fallback_stack.push_back("sans-serif");
                }
            }

            svg_converter->setTextRenderOptions(textOpts);
        }

        // Create wrapper and transfer ownership
        auto converter = std::make_unique<SVGConverter_t>();
        converter->file_owner = handle->file; // Keep file alive even if handle is freed.
        converter->converter = svg_converter.release();

        set_last_error(CGM_SUCCESS);
        if (error_code) *error_code = CGM_SUCCESS;
        return converter.release();  // Caller owns via cgm_free_svg_converter
    }
    catch (...) {
        if (error_code) *error_code = CGM_ERROR_OUT_OF_MEMORY;
        return nullptr;
    }
}

int cgm_convert_to_svg(SVGConverterHandle converter, char* buffer, size_t buffer_size) {
    if (!validate_converter(converter)) return -1;

    try {
        // Perform conversion if not already done
        if (converter->svg_output.empty() && converter->converter) {
            const auto picture_ranges =
                converter->file_owner->getPictureRanges();
            const int picture_index = picture_ranges.empty() ? -1 : 0;
            converter->svg_output =
                converter->converter->convert(picture_index);
        }

        if (buffer && buffer_size > 0) {
            size_t copy_size = std::min(converter->svg_output.size(), buffer_size - 1);
            std::memcpy(buffer, converter->svg_output.c_str(), copy_size);
            buffer[copy_size] = '\0';
        }

        return static_cast<int>(converter->svg_output.size() + 1);
    }
    catch (...) {
        set_last_error(CGM_ERROR_CONVERSION_FAILED);
        return -1;
    }
}

CGMErrorCode cgm_save_svg_to_file(SVGConverterHandle converter, const char* filename) {
    if (!validate_converter(converter)) return CGM_ERROR_NULL_HANDLE;
    if (!filename) return CGM_ERROR_INVALID_PARAMETER;

    try {
        // Generate SVG if not already done
        if (converter->svg_output.empty()) {
            const int conversion_result = cgm_convert_to_svg(converter, nullptr, 0);
            if (conversion_result < 0) {
                return CGM_ERROR_CONVERSION_FAILED;
            }
        }

        std::ofstream file(filename);
        if (!file.is_open()) {
            return CGM_ERROR_FILE_NOT_FOUND;
        }

        file << converter->svg_output;
        return CGM_SUCCESS;
    }
    catch (...) {
        return CGM_ERROR_CONVERSION_FAILED;
    }
}

void cgm_free_svg_converter(SVGConverterHandle converter) {
    if (converter) {
        delete converter->converter;
        delete converter;
    }
}

// ============================================================================
// Error Handling
// ============================================================================

const char* cgm_get_error_message(CGMErrorCode error_code) {
    switch (error_code) {
        case CGM_SUCCESS: return "Success";
        case CGM_ERROR_FILE_NOT_FOUND: return "File not found";
        case CGM_ERROR_INVALID_FORMAT: return "Invalid CGM format";
        case CGM_ERROR_PARSE_ERROR: return "Parse error";
        case CGM_ERROR_NULL_HANDLE: return "Null handle";
        case CGM_ERROR_OUT_OF_BOUNDS: return "Index out of bounds";
        case CGM_ERROR_INVALID_PARAMETER: return "Invalid parameter";
        case CGM_ERROR_CONVERSION_FAILED: return "SVG conversion failed";
        case CGM_ERROR_OUT_OF_MEMORY: return "Out of memory";
        default: return "Unknown error";
    }
}

CGMErrorCode cgm_get_last_error() {
    return g_last_error;
}

void cgm_clear_last_error() {
    g_last_error = CGM_SUCCESS;
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* cgm_get_version_string() {
    return opencgm::kEngineVersionString;
}

const char* cgm_get_build_date() {
    return __DATE__ " " __TIME__;
}

bool cgm_is_feature_supported(const char* feature_name) {
    if (!feature_name) return false;

    std::string feature(feature_name);

    if (feature == "binary_cgm") return true;
    if (feature == "svg_conversion") return true;
    if (feature == "text_cgm") return false;
    if (feature == "cgm_write") return true;
#ifdef CGM_SKIA_RENDERER_ENABLED
    if (feature == "native_rendering") return true;
#else
    if (feature == "native_rendering") return false;
#endif

    return false;
}

// ============================================================================
// Native Raster Rendering
// ============================================================================

CGMRenderOptions_t cgm_get_default_render_options(void) {
    CGMRenderOptions_t options = {};
    options.target_width = 0;
    options.target_height = 0;
    options.scale_factor = 1.0;
    options.background_r = 255;
    options.background_g = 255;
    options.background_b = 255;
    options.background_a = 255;
    options.antialias = true;
    options.fit_to_content = false;
    options.dpi = 96;
    return options;
}

bool cgm_is_native_renderer_available(void) {
#ifdef CGM_SKIA_RENDERER_ENABLED
    return true;
#else
    return false;
#endif
}

CGMErrorCode cgm_render_to_buffer(
    CGMFileHandle handle,
    const CGMRenderOptions_t* options,
    uint8_t** out_buffer,
    size_t* out_buffer_size,
    int* out_width,
    int* out_height)
{
    if (!validate_handle(handle)) {
        return CGM_ERROR_NULL_HANDLE;
    }

    if (!out_buffer || !out_buffer_size || !out_width || !out_height) {
        set_last_error(CGM_ERROR_INVALID_PARAMETER);
        return CGM_ERROR_INVALID_PARAMETER;
    }

    // Initialize outputs
    *out_buffer = nullptr;
    *out_buffer_size = 0;
    *out_width = 0;
    *out_height = 0;

    // `options` is only consumed when the native renderer is enabled.
    (void)options;

#ifdef CGM_SKIA_RENDERER_ENABLED
    try {
        opencgm::SkiaRenderer renderer(handle->file.get());

        // Apply options
        CGMRenderOptions_t opts = options ? *options : cgm_get_default_render_options();

        renderer.setBackgroundColor(opts.background_r, opts.background_g, opts.background_b, opts.background_a);
        renderer.setAntiAlias(opts.antialias);
        renderer.setDpi(opts.dpi);

        opencgm::SkiaRenderOptions renderOpts;
        renderOpts.target_width = opts.target_width;
        renderOpts.target_height = opts.target_height;
        renderOpts.scale_factor = opts.scale_factor;
        renderOpts.background_r = opts.background_r;
        renderOpts.background_g = opts.background_g;
        renderOpts.background_b = opts.background_b;
        renderOpts.background_a = opts.background_a;
        renderOpts.antialias = opts.antialias;
        renderOpts.fit_to_content = opts.fit_to_content;
        renderOpts.dpi = opts.dpi;

        opencgm::SkiaRenderResult result = renderer.render(renderOpts);

        if (!result.success) {
            set_last_error(CGM_ERROR_CONVERSION_FAILED);
            return CGM_ERROR_CONVERSION_FAILED;
        }

        // Allocate output buffer
        *out_buffer_size = result.pixel_data.size();
        *out_buffer = new (std::nothrow) uint8_t[*out_buffer_size];
        if (!*out_buffer) {
            set_last_error(CGM_ERROR_OUT_OF_MEMORY);
            return CGM_ERROR_OUT_OF_MEMORY;
        }

        std::memcpy(*out_buffer, result.pixel_data.data(), *out_buffer_size);
        *out_width = result.width;
        *out_height = result.height;

        set_last_error(CGM_SUCCESS);
        return CGM_SUCCESS;
    }
    catch (const std::exception&) {
        set_last_error(CGM_ERROR_CONVERSION_FAILED);
        return CGM_ERROR_CONVERSION_FAILED;
    }
#else
    // Native renderer not available
    set_last_error(CGM_ERROR_CONVERSION_FAILED);
    return CGM_ERROR_CONVERSION_FAILED;
#endif
}

CGMErrorCode cgm_render_to_buffer_simple(
    CGMFileHandle handle,
    int target_width,
    int target_height,
    double scale_factor,
    uint8_t** out_buffer,
    size_t* out_buffer_size,
    int* out_width,
    int* out_height)
{
    CGMRenderOptions_t options = cgm_get_default_render_options();
    options.target_width = target_width;
    options.target_height = target_height;
    options.scale_factor = scale_factor;

    return cgm_render_to_buffer(handle, &options, out_buffer, out_buffer_size, out_width, out_height);
}

void cgm_free_render_buffer(uint8_t* buffer) {
    delete[] buffer;
}
