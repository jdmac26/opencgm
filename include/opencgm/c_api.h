#pragma once

// Export macro
#if defined(_WIN32) || defined(_WIN64)
#if defined(OPENCGM_BUILD)
#define OPENCGM_API __declspec(dllexport)
#else
#define OPENCGM_API __declspec(dllimport)
#endif
#else
#if __GNUC__ >= 4
#define OPENCGM_API __attribute__((visibility("default")))
#else
#define OPENCGM_API
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

    // Public C ABI for OpenCGM Engine

    typedef struct opencgm_ctx_t opencgm_ctx_t;

    typedef enum opencgm_err_t {
        OPENCGM_OK = 0,
        OPENCGM_ERR_INVALID_ARG = 1,
        OPENCGM_ERR_PARSE = 2,
        OPENCGM_ERR_IO = 3,
        OPENCGM_ERR_GENERAL = 4
    } opencgm_err_t;

    typedef enum opencgm_yflip_mode {
        OPENCGM_YFLIP_AUTO = 0,
        OPENCGM_YFLIP_FORCE_ON = 1,
        OPENCGM_YFLIP_FORCE_OFF = 2
    } opencgm_yflip_mode;

    typedef enum opencgm_shim_mode {
        OPENCGM_SHIM_AUTO = 0,
        OPENCGM_SHIM_ON = 1,
        OPENCGM_SHIM_OFF = 2
    } opencgm_shim_mode;

    // Lifecycle
    OPENCGM_API opencgm_ctx_t* opencgm_create(void);
    OPENCGM_API void           opencgm_destroy(opencgm_ctx_t* ctx);

    // Retained for ABI compatibility. These pre-1.0 rendering controls never
    // affected SVG conversion; profile-aware coordinate handling is automatic.
    OPENCGM_API void           opencgm_set_yflip_mode(opencgm_ctx_t* ctx, int mode);
    OPENCGM_API void           opencgm_set_fit_to_content(opencgm_ctx_t* ctx, int fit);
    OPENCGM_API void           opencgm_set_scale(opencgm_ctx_t* ctx, double scale);

    // Pipeline
    OPENCGM_API void           opencgm_set_dpi(opencgm_ctx_t* ctx, int dpi);
    OPENCGM_API int            opencgm_set_font_map(opencgm_ctx_t* ctx, const char* json_path);

    // Diagnostic options
    OPENCGM_API void           opencgm_set_verbose(opencgm_ctx_t* ctx, int verbose);
    OPENCGM_API void           opencgm_set_quiet(opencgm_ctx_t* ctx, int quiet);
    OPENCGM_API void           opencgm_set_trace_unknown(opencgm_ctx_t* ctx, int trace);
    OPENCGM_API void           opencgm_set_fail_on_warn(opencgm_ctx_t* ctx, int fail);
    OPENCGM_API int            opencgm_set_log_file(opencgm_ctx_t* ctx, const char* log_path);

    // Retained for ABI compatibility. Style mode is no longer configurable;
    // the converter emits profile-compatible presentation attributes.
    OPENCGM_API void           opencgm_set_style_mode(opencgm_ctx_t* ctx, int mode);

    // Style options
    OPENCGM_API void           opencgm_set_minify(opencgm_ctx_t* ctx, int minify);
    OPENCGM_API void           opencgm_set_optimize_paths(opencgm_ctx_t* ctx, int optimize);
    OPENCGM_API void           opencgm_set_pretty_print(opencgm_ctx_t* ctx, int pretty);
    OPENCGM_API void           opencgm_set_precision(opencgm_ctx_t* ctx, double precision);

    // Profile & rendering options
    OPENCGM_API void           opencgm_set_profile(opencgm_ctx_t* ctx, const char* profile);
    OPENCGM_API void           opencgm_set_adopt_view_on_load(opencgm_ctx_t* ctx, int adopt);
    OPENCGM_API void           opencgm_set_text_as_path(opencgm_ctx_t* ctx, int enable);
    OPENCGM_API void           opencgm_set_text_path_threshold(opencgm_ctx_t* ctx, double threshold);
    OPENCGM_API void           opencgm_set_tcc_enabled(opencgm_ctx_t* ctx, int enabled);
    OPENCGM_API void           opencgm_set_raster_logging(opencgm_ctx_t* ctx, int enabled);
    OPENCGM_API void           opencgm_set_geometry_logging(opencgm_ctx_t* ctx, int enabled);
    OPENCGM_API void           opencgm_set_color_logging(opencgm_ctx_t* ctx, int enabled);
    OPENCGM_API void           opencgm_set_width_logging(opencgm_ctx_t* ctx, int enabled);
    OPENCGM_API void           opencgm_set_viewbox_padding(opencgm_ctx_t* ctx, double fraction);
    // Max deviation (SVG units) for NUBS/NURBS-to-Bezier approximation.
    // Clamped to [0.001, 10]; default 0.5.
    OPENCGM_API void           opencgm_set_nurbs_tolerance(opencgm_ctx_t* ctx, double svg_units);
    OPENCGM_API void           opencgm_set_palette_override_mode(opencgm_ctx_t* ctx, int mode);
    OPENCGM_API void           opencgm_set_quantize_png(opencgm_ctx_t* ctx, int enabled);
    OPENCGM_API void           opencgm_set_geometry_validation(opencgm_ctx_t* ctx, int enabled);
    OPENCGM_API void           opencgm_set_geometry_tolerance(opencgm_ctx_t* ctx, double tolerance);
    OPENCGM_API void           opencgm_set_embed_shim(opencgm_ctx_t* ctx, int mode);
    OPENCGM_API void           opencgm_set_shim_url(opencgm_ctx_t* ctx, const char* url);

    // Hotspot options
    OPENCGM_API void           opencgm_set_hotspot_profile(opencgm_ctx_t* ctx, int profile);
    OPENCGM_API void           opencgm_add_custom_attribute(opencgm_ctx_t* ctx, const char* key, const char* value, int scope, const char* selector);

    // ============================================================================
    // Output Target Configuration (Two-Tier Profile Model)
    // ============================================================================
    // Per the profile selection spec, Output Target controls SVG structure
    // for downstream delivery systems (IETP viewers, etc.)

    // Output profile enum values (maps to SVGConverter::OutputProfile)
    typedef enum opencgm_output_profile {
        OPENCGM_OUTPUT_WEBCGM21 = 0,    // WebCGM 2.1 compliant output
        OPENCGM_OUTPUT_S1000D = 1,      // S1000D Issue 6 IETP (data-apsid / data-apsname)
        OPENCGM_OUTPUT_ATA2200 = 2,     // ATA IETM optimized
        OPENCGM_OUTPUT_STANDARD_SVG = 3, // Clean SVG - no data-* attributes
        OPENCGM_OUTPUT_CUSTOM = 4,       // User-configurable
        OPENCGM_OUTPUT_S1000D_LEGACY = 5 // S1000D Issues 2.3 - 5.0 IETP (bare apsid / name)
    } opencgm_output_profile;

    // Hotspot encoding mode enum values
    typedef enum opencgm_hotspot_encoding {
        OPENCGM_HOTSPOT_SVG_ANCHOR_TITLE = 0,  // <a>/<title> only
        OPENCGM_HOTSPOT_DATA_ATTRIBUTES = 1,   // data-* attributes only
        OPENCGM_HOTSPOT_BOTH = 2               // Both methods (default for IETP)
    } opencgm_hotspot_encoding;

    // Region handling mode enum values
    typedef enum opencgm_region_handling {
        OPENCGM_REGION_OVERLAY_ONLY = 0,  // Invisible overlay shapes
        OPENCGM_REGION_BBOX_ONLY = 1,     // Bounding box calculation
        OPENCGM_REGION_BOTH = 2           // Both methods (default)
    } opencgm_region_handling;

    // Multi-link handling mode enum values
    typedef enum opencgm_multi_link_mode {
        OPENCGM_MULTILINK_FIRST_ONLY = 0,      // First link only
        OPENCGM_MULTILINK_JSON_DATA_ATTR = 1,  // JSON in data-linkuri
        OPENCGM_MULTILINK_JS_HANDLER = 2       // Inline onclick handler
    } opencgm_multi_link_mode;

    // Set output profile (preset: WebCGM21, S1000D, ATA2200, StandardSVG, Custom)
    OPENCGM_API void           opencgm_set_output_profile(opencgm_ctx_t* ctx, int profile);
    OPENCGM_API int            opencgm_get_output_profile(opencgm_ctx_t* ctx);

    // Output target configuration — sent unconditionally by the managed layer
    // so every built-in profile's hotspot/region/APS preservation intent reaches
    // the engine (not just the Custom profile).
    OPENCGM_API void           opencgm_set_hotspot_encoding(opencgm_ctx_t* ctx, int mode);
    OPENCGM_API void           opencgm_set_region_handling(opencgm_ctx_t* ctx, int mode);
    OPENCGM_API void           opencgm_set_multi_link_mode(opencgm_ctx_t* ctx, int mode);
    OPENCGM_API void           opencgm_set_emit_data_aps_type(opencgm_ctx_t* ctx, int enabled);
    OPENCGM_API void           opencgm_set_emit_data_name(opencgm_ctx_t* ctx, int enabled);
    OPENCGM_API void           opencgm_set_emit_data_content(opencgm_ctx_t* ctx, int enabled);
    OPENCGM_API void           opencgm_set_emit_data_viewcontext(opencgm_ctx_t* ctx, int enabled);
    OPENCGM_API void           opencgm_set_preserve_layer_hierarchy(opencgm_ctx_t* ctx, int enabled);

    // Granular APS preservation flags (complement the legacy emit_data_* block above).
    // Each toggles emission of one data-aps* attribute family on SVG hotspot elements.
    OPENCGM_API void           opencgm_set_preserve_aps_id(opencgm_ctx_t* ctx, int enabled);
    OPENCGM_API void           opencgm_set_preserve_aps_link_title(opencgm_ctx_t* ctx, int enabled);
    OPENCGM_API void           opencgm_set_preserve_aps_region(opencgm_ctx_t* ctx, int enabled);
    OPENCGM_API void           opencgm_set_preserve_aps_screen_tip(opencgm_ctx_t* ctx, int enabled);

    // Tier 0 WebCGM 2.1 namespace baseline: declare xmlns:webcgm on root <svg>
    // and emit webcgm:type / webcgm:layername / etc. per APS <g>. Enabled by default
    // for the WebCGM family. Disable for clean SVG / CALS-class outputs.
    OPENCGM_API void           opencgm_set_webcgm_namespace(opencgm_ctx_t* ctx, int enabled);

    // DEPRECATED no-op, retained for ABI stability. The stored flag was never
    // consumed by any engine path; do not call from new code.
    OPENCGM_API void           opencgm_set_validate_output_s1000d6(opencgm_ctx_t* ctx, int enabled);

    // Emit the WebCGM-style <metadata id="webcgm-aps"> JSON blob in the SVG
    // footer. Off by default -- no known consumer reads it.
    OPENCGM_API void           opencgm_set_embed_aps_metadata_json(opencgm_ctx_t* ctx, int enabled);

    // SVGZ compression output
    OPENCGM_API void           opencgm_set_compress_output(opencgm_ctx_t* ctx, int enabled);

    // Raster encoding options for embedded images
    // mode: 0=Auto (detect optimal format), 1=PNG (lossless), 2=JPEG (lossy)
    OPENCGM_API void           opencgm_set_raster_encoding(opencgm_ctx_t* ctx, int mode);
    OPENCGM_API int            opencgm_get_raster_encoding(opencgm_ctx_t* ctx);

    // JPEG quality (1-100, default 85)
    OPENCGM_API void           opencgm_set_jpeg_quality(opencgm_ctx_t* ctx, int quality);
    OPENCGM_API int            opencgm_get_jpeg_quality(opencgm_ctx_t* ctx);

    // JPEG 4:4:4 chroma subsampling (1=enabled for sharp edges, 0=4:2:0 for smaller files)
    OPENCGM_API void           opencgm_set_jpeg_444_subsampling(opencgm_ctx_t* ctx, int enabled);
    OPENCGM_API int            opencgm_get_jpeg_444_subsampling(opencgm_ctx_t* ctx);

    // Attribute output format options
    // Supported formats: "legacy", "s1000d6", "combined", "rws", "boeing", "r4i", "multi"
    // Default: "combined" (legacy + S1000D Issue 6)
    OPENCGM_API int            opencgm_set_output_format(opencgm_ctx_t* ctx, const char* format);
    OPENCGM_API const char*    opencgm_get_output_format(opencgm_ctx_t* ctx);

    // Multi-picture support
    // Get the number of pictures in the loaded CGM file
    OPENCGM_API int            opencgm_get_picture_count(opencgm_ctx_t* ctx, const char* input_path);

    // Convert a specific picture (0-indexed) to SVG
    // Returns OPENCGM_OK on success, error code otherwise
    OPENCGM_API int            opencgm_convert_picture_to_svg(opencgm_ctx_t* ctx,
                                                               const char* input_path,
                                                               const char* output_svg_path,
                                                               int picture_index);

    // Multi-picture mode: 0=first_only, 1=separate_files, 2=combined
    OPENCGM_API void           opencgm_set_multi_picture_mode(opencgm_ctx_t* ctx, int mode);

    // Main conversion function
    OPENCGM_API int            opencgm_convert_cgm_to_svg(opencgm_ctx_t* ctx,
                                                          const char* input_path,
                                                          const char* output_svg_path);

    // Native profile detection
    OPENCGM_API int            opencgm_detect_profile(opencgm_ctx_t* ctx,
                                                      const char* input_path);
    OPENCGM_API const char*    opencgm_get_last_profile_detection_json(opencgm_ctx_t* ctx);

    // Native profile validation
    OPENCGM_API int            opencgm_validate_profile(opencgm_ctx_t* ctx,
                                                        const char* input_path,
                                                        const char* requested_profile);
    OPENCGM_API const char*    opencgm_get_last_validation_json(opencgm_ctx_t* ctx);
    OPENCGM_API const char*    opencgm_get_builtin_profile_catalog_json(opencgm_ctx_t* ctx);

    // QA function (placeholder - returns 0 for now)
    OPENCGM_API int            opencgm_run_builtin_qa(opencgm_ctx_t* ctx,
                                                       const char* svg_path);

    // Report function (placeholder)
    OPENCGM_API int            opencgm_write_report(opencgm_ctx_t* ctx,
                                                     const char* report_json_path);
    OPENCGM_API const char*    opencgm_get_last_report_json(opencgm_ctx_t* ctx);
    OPENCGM_API const char*    opencgm_get_last_report_text(opencgm_ctx_t* ctx);

    // Diagnostics
    OPENCGM_API const char*    opencgm_last_error(void);

    // Trace function
    OPENCGM_API int            opencgm_trace_cgm(opencgm_ctx_t* ctx,
                                                  const char* input_path,
                                                  const char* output_json_path);

    // XCF (XML Companion File) Generation
    // Enable/disable XCF generation alongside SVG output
    OPENCGM_API void           opencgm_set_generate_xcf(opencgm_ctx_t* ctx, int enabled);

    // Set XCF DTD version (e.g., "2.1" for WebCGM 2.1)
    OPENCGM_API void           opencgm_set_xcf_dtd_version(opencgm_ctx_t* ctx, const char* version);

    // Set whether to include hotspots in XCF
    OPENCGM_API void           opencgm_set_xcf_include_hotspots(opencgm_ctx_t* ctx, int include);

    // Set whether to include metadata in XCF
    OPENCGM_API void           opencgm_set_xcf_include_metadata(opencgm_ctx_t* ctx, int include);

    // Generate XCF file from a loaded CGM
    // Returns OPENCGM_OK on success, error code otherwise
    OPENCGM_API int            opencgm_generate_xcf(opencgm_ctx_t* ctx,
                                                     const char* input_cgm_path,
                                                     const char* svg_filename,
                                                     const char* output_xcf_path);

    // Get hotspot count from a CGM file (for diagnostics)
    OPENCGM_API int            opencgm_get_hotspot_count(opencgm_ctx_t* ctx, const char* input_path);

    // ============================================================================
    // XCF (XML Companion File) Input / Merging
    // ============================================================================
    // Load and merge XCF metadata with CGM conversion output

    // Companion file handling mode enum values
    typedef enum opencgm_companion_mode {
        OPENCGM_COMPANION_AUTO = 0,     // Auto-detect and merge if found
        OPENCGM_COMPANION_IGNORE = 1,   // Ignore companion files
        OPENCGM_COMPANION_XCF_ONLY = 2, // Only use XCF (WebCGM)
        OPENCGM_COMPANION_IMF_ONLY = 3  // Only use IMF (S1000D) - reserved
    } opencgm_companion_mode;

    // Set companion file handling mode
    OPENCGM_API void           opencgm_set_companion_mode(opencgm_ctx_t* ctx, int mode);

    // Set XCF input file path for merging
    // If set, XCF metadata will be merged with CGM APS during conversion
    // Returns OPENCGM_OK on success, OPENCGM_ERR_PARSE if file is invalid
    OPENCGM_API int            opencgm_set_xcf_input(opencgm_ctx_t* ctx, const char* xcf_path);

    // Clear loaded XCF input (disable merging)
    OPENCGM_API void           opencgm_clear_xcf_input(opencgm_ctx_t* ctx);

    // Check if XCF input is loaded
    OPENCGM_API int            opencgm_has_xcf_input(opencgm_ctx_t* ctx);

    // Auto-detect companion XCF file for a CGM
    // Returns path to XCF file if found, empty string otherwise
    // Caller must NOT free the returned string
    OPENCGM_API const char*    opencgm_find_companion_xcf(const char* cgm_path);

    // Validate XCF file
    // Returns OPENCGM_OK if valid, OPENCGM_ERR_PARSE otherwise
    OPENCGM_API int            opencgm_validate_xcf(opencgm_ctx_t* ctx, const char* xcf_path);

#ifdef __cplusplus
} // extern "C"
#endif
