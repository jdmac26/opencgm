#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cctype>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <thread>
#include <chrono>
#include <atomic>
#include <set>
#include <csignal>

#include "opencgm/c_api.h"
#include "../../third_party/nlohmann/json.hpp"

namespace fs = std::filesystem;

namespace
{
// Global flag for graceful shutdown
static std::atomic<bool> g_shutdown_requested{false};

void signal_handler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        g_shutdown_requested = true;
    }
}

struct CliOptions
{
    bool verbose = false;
    bool quiet = false;
    std::string profile = "s1000d";
    bool profile_set = false;
    std::string preset;
    bool preset_set = false;
    bool adopt_view_on_load = false;
    bool adopt_view_on_load_set = false;
    bool text_as_path = false;
    bool text_as_path_set = false;
    double text_path_threshold = 0.0;
    bool text_path_threshold_set = false;
    bool tcc_enabled = true;
    bool raster_logging = false;
    bool geometry_logging = false;
    std::string font_map;
    bool font_map_set = false;
    bool validate_geometry = false;
    double geometry_tolerance = 0.01;
    bool geometry_tolerance_set = false;
    int embed_shim_mode = OPENCGM_SHIM_AUTO;
    bool embed_shim_mode_set = false;
    std::string shim_url;
    bool shim_url_set = false;
    bool quantize_png = false;
    bool quantize_png_set = false;
    int webcgm_namespace = 1;
    bool webcgm_namespace_set = false;
    // Watch folder options
    std::string watch_dir;
    std::string output_dir;
    int poll_interval = 2; // seconds
    bool recursive = false;
};

static std::string to_lower_copy(const std::string &value)
{
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
}

static bool parse_bool_token(const std::string &token, bool &out)
{
    std::string lower = to_lower_copy(token);
    if (lower == "1" || lower == "true" || lower == "yes" || lower == "on")
    {
        out = true;
        return true;
    }
    if (lower == "0" || lower == "false" || lower == "no" || lower == "off")
    {
        out = false;
        return true;
    }
    return false;
}

void print_usage(const char *exe)
{
    std::fprintf(stderr,
                 "Usage: %s [options] <input.cgm> <output.svg>\n"
                 "       %s --watch <dir> --output-dir <dir> [options]\n"
                 "\nOpenCGM CLI - CGM to SVG Converter\n"
                 "\nSingle File Conversion:\n"
                 "  %s input.cgm output.svg\n"
                 "\nWatch Folder Mode:\n"
                 "  %s --watch ./input --output-dir ./output\n"
                 "\nOptions:\n"
                 "  --profile <s1000d|s1000d-legacy|webcgm|compat>\n"
                 "                                     Select rendering profile (default: s1000d).\n"
                 "                                     s1000d-legacy targets pre-v6 IETP viewers\n"
                 "                                     (bare apsid / name SVG attrs).\n"
                 "  --preset <name>                    Apply a built-in profile preset from the shared\n"
                 "                                     catalog (config/profile-presets.json). Names:\n"
                 "                                       s1000d, s1000dlegacy (s1000dprev6),\n"
                 "                                       ataispec2200 (ata), webcgm21 (webcgm),\n"
                 "                                       cals, pip (cgmpip), cgmplus,\n"
                 "                                       or any catalog profile name.\n"
                 "                                     Individual flags override preset values.\n"
                 "  --webcgm-namespace <on|off>        Emit xmlns:webcgm + webcgm:* attrs (de-facto\n"
                 "                                     convention for traditional IETP viewers). Default:\n"
                 "                                     on for WebCGM-family presets, off for cals.\n"
                 "  --adopt-view-on-load [true|false]  Adopt viewcontext on load (default depends on profile)\n"
                 "  --text-as-path [true|false]        Render text as paths (default: false)\n"
                 "  --text-as-path-threshold <value>   Outline text below this height (SVG units)\n"
                 "  --font-map <file>                  Load font mapping JSON\n"
                 "  --no-tcc                           Ignore Transparent Cell Colour\n"
                 "  --geometry-log                     Emit geometry/viewBox diagnostics\n"
                 "  --validate-geometry                Run viewBox/geometry validation and fail on errors\n"
                 "  --geometry-tolerance <value>       Absolute tolerance for geometry validation (default: 0.01)\n"
                 "  --embed-shim <auto|on|off>         Control embedding of viewer shim script (default: auto)\n"
                 "  --shim-url <url>                   Reference external viewer shim instead of inline script\n"
                 "  --quantize-png                     Reduce raster PNG color precision\n"
                 "  --no-quantize-png                  Disable PNG quantization\n"
                 "  --raster-log                       Emit raster diagnostic logs\n"
                 "  --log <debug|info|quiet>           Set log verbosity\n"
                 "  --verbose                          Print conversion progress\n"
                 "  --help                             Show this message\n"
                 "\nWatch Folder Options:\n"
                 "  --watch <dir>                      Watch directory for new CGM files\n"
                 "  --output-dir <dir>                 Output directory for converted SVG files\n"
                 "  --poll-interval <seconds>          Polling interval in seconds (default: 2)\n"
                 "  --recursive                        Watch subdirectories recursively\n"
                 "\nPress Ctrl+C to stop watch mode gracefully.\n",
                 exe, exe, exe, exe);
}

// Apply one of the built-in profile presets by reading the shared catalog
// (engine/config/profile-presets.json; embedded copy is the build-time
// fallback). Mirrors the managed app's GetProfileName / DeriveOutputFormat /
// Configure mappings so CLI presets behave identically to app profiles.
// Returns false if the name is unrecognised.
bool apply_preset(opencgm_ctx_t *ctx, const std::string &raw_name)
{
    auto normalize = [](const std::string &s) {
        std::string out;
        for (char c : s) {
            if (std::isalnum(static_cast<unsigned char>(c))) out.push_back(static_cast<char>(std::tolower(c)));
        }
        return out;
    };
    const std::string key = normalize(raw_name);

    // Alias -> catalog profileOverride discriminator. Resolving through
    // profileOverride keeps aliases stable across catalog display renames.
    static const std::pair<const char*, const char*> kAliases[] = {
        {"s1000d", "S1000Dv6"}, {"s1000dissue6", "S1000Dv6"}, {"s1000dv6", "S1000Dv6"},
        {"s1000dlegacy", "S1000DLegacy"}, {"s1000dprev6", "S1000DLegacy"},
        {"s1000dissue5", "S1000DLegacy"}, {"s1000dissue4", "S1000DLegacy"},
        {"s1000dissue3", "S1000DLegacy"}, {"s1000dissue23", "S1000DLegacy"},
        {"ataispec2200", "AtaISpec2200"}, {"ata", "AtaISpec2200"}, {"ataispec", "AtaISpec2200"},
        {"webcgm21", "WebCgm21"}, {"webcgm", "WebCgm21"},
        {"cals", "Cals"},
        {"pipcggc", "Pip"}, {"pip", "Pip"}, {"cgmpip", "Pip"},
        {"cgmplus", "CgmPlus"},
    };
    std::string wantedOverride;
    for (const auto &alias : kAliases) {
        if (key == alias.first) {
            wantedOverride = alias.second;
            break;
        }
    }

    const char *catalogText = opencgm_get_builtin_profile_catalog_json(ctx);
    if (!catalogText) {
        return false;
    }
    const nlohmann::json catalog = nlohmann::json::parse(catalogText, nullptr, false);
    if (catalog.is_discarded() || !catalog.contains("profiles")) {
        return false;
    }

    // Aliases whose catalog entry was merged/retired resolve to the family
    // entry, with the requested override still driving the output dialect:
    // S1000DLegacy shares the merged S1000D preset; the retired CgmPlus
    // preset falls back to WebCGM 2.1 settings.
    std::string familyOverride = wantedOverride;
    if (wantedOverride == "S1000DLegacy") familyOverride = "S1000Dv6";
    else if (wantedOverride == "CgmPlus") familyOverride = "WebCgm21";

    const nlohmann::json *match = nullptr;
    const nlohmann::json *familyMatch = nullptr;
    for (const auto &profile : catalog["profiles"]) {
        if (!profile.contains("settings")) continue;
        const auto &settings = profile["settings"];
        const std::string overrideName = settings.value("profileOverride", std::string());
        if (!wantedOverride.empty()) {
            if (overrideName == wantedOverride) { match = &profile; break; }
            if (overrideName == familyOverride && !familyMatch) familyMatch = &profile;
        } else if (normalize(profile.value("name", std::string())) == key) {
            match = &profile;
            break;
        }
    }
    if (!match) {
        match = familyMatch;
    }
    if (!match) {
        return false;
    }

    const auto &s = (*match)["settings"];
    // The requested override wins over the matched entry's own override so
    // e.g. --preset s1000dlegacy keeps the legacy dialect on the merged
    // S1000D settings.
    const std::string ov = !wantedOverride.empty()
        ? wantedOverride
        : s.value("profileOverride", std::string());
    auto flag = [&](const char *k, bool def) { return s.value(k, def) ? 1 : 0; };
    auto enumIndex = [&](const char *k, std::initializer_list<const char *> names, int def) {
        const std::string v = s.value(k, std::string());
        int i = 0;
        for (const char *n : names) {
            if (v == n) return i;
            ++i;
        }
        return def;
    };

    // Engine source-profile string (parity with managed GetProfileName).
    std::string engineProfile = "compat";
    if (ov == "S1000Dv6") engineProfile = "s1000d";
    else if (ov == "S1000DLegacy") engineProfile = "s1000d-legacy";
    else if (ov == "AtaISpec2200") engineProfile = "ata";
    else if (ov == "WebCgm21") engineProfile = "webcgm";
    else if (ov == "Cals") engineProfile = "cals";
    else if (ov == "CgmPlus") engineProfile = "cgmplus";
    else if (ov == "Pip") engineProfile = "pip";
    opencgm_set_profile(ctx, engineProfile.c_str());

    // Attribute dialect (parity with managed DeriveOutputFormat).
    const char *outputFormat = "legacy";
    if (ov == "S1000Dv6" || ov == "AtaISpec2200" || ov == "CgmPlus") outputFormat = "s1000d6";
    else if (ov == "S1000DLegacy") outputFormat = "s1000d-legacy";
    opencgm_set_output_format(ctx, outputFormat);

    opencgm_set_dpi(ctx, s.value("dpiResolution", 300));
    opencgm_set_hotspot_encoding(ctx, enumIndex("hotspotEncoding",
        {"SvgAnchorTitle", "DataAttributes", "Both"}, OPENCGM_HOTSPOT_BOTH));
    opencgm_set_region_handling(ctx, enumIndex("regionHandling",
        {"OverlayOnly", "BboxOnly", "Both"}, OPENCGM_REGION_BOTH));
    opencgm_set_multi_link_mode(ctx, enumIndex("multiLinkMode",
        {"FirstLinkOnly", "JsonDataAttribute", "JsEventHandler"}, OPENCGM_MULTILINK_JSON_DATA_ATTR));
    opencgm_set_companion_mode(ctx, enumIndex("companionFileMode",
        {"AutoMerge", "Ignore", "XcfOnly", "ImfOnly"}, OPENCGM_COMPANION_AUTO));

    opencgm_set_preserve_aps_id(ctx, flag("preserveApsId", true));
    opencgm_set_emit_data_name(ctx, flag("preserveApsName", true));
    opencgm_set_emit_data_content(ctx, flag("preserveApsLinkUri", true));
    opencgm_set_preserve_aps_link_title(ctx, flag("preserveApsLinkTitle", true));
    opencgm_set_preserve_aps_region(ctx, flag("preserveApsRegion", true));
    opencgm_set_preserve_layer_hierarchy(ctx, flag("preserveApsLayer", true));
    opencgm_set_preserve_aps_screen_tip(ctx, flag("preserveApsScreenTip", true));
    opencgm_set_emit_data_viewcontext(ctx, flag("preserveApsViewContext", true));
    opencgm_set_emit_data_aps_type(ctx, 1);  // catalog does not override; model default is true
    opencgm_set_webcgm_namespace(ctx, flag("emitWebCgmNamespace", true));

    const bool textAsPath =
        s.value("convertTextToPaths", false) && !s.value("preserveEditableText", true);
    opencgm_set_text_as_path(ctx, textAsPath ? 1 : 0);
    opencgm_set_minify(ctx, flag("minifyOutput", false));
    opencgm_set_optimize_paths(ctx, flag("optimizeForWeb", false));

    if (s.value("strictComplianceMode", false)) {
        // Same composition the managed app uses for "strict compliance mode".
        opencgm_set_fail_on_warn(ctx, 1);
        opencgm_set_geometry_validation(ctx, 1);
        opencgm_set_trace_unknown(ctx, 1);
    }
    return true;
}

int convert_file(const CliOptions &options,
                 const std::string &input,
                 const std::string &output)
{
    if (options.verbose && !options.quiet)
    {
        std::fprintf(stdout, "Converting %s -> %s\n", input.c_str(), output.c_str());
        std::fflush(stdout);
    }

    opencgm_ctx_t *ctx = opencgm_create();
    if (!ctx)
    {
        const char *msg = opencgm_last_error();
        std::fprintf(stderr, "Error: failed to create context (%s)\n", msg ? msg : "unknown");
        return 2;
    }

    // Apply built-in preset first — subsequent --profile etc. flags still override.
    if (options.preset_set)
    {
        if (!apply_preset(ctx, options.preset))
        {
            std::fprintf(stderr,
                         "Error: unknown --preset '%s' (expected an alias like s1000d, s1000dlegacy, ata, webcgm, cals, pip, cgmplus, or a catalog profile name)\n",
                         options.preset.c_str());
            opencgm_destroy(ctx);
            return 2;
        }
    }
    if (options.profile_set)
    {
        opencgm_set_profile(ctx, options.profile.c_str());
    }
    if (options.adopt_view_on_load_set)
    {
        opencgm_set_adopt_view_on_load(ctx, options.adopt_view_on_load ? 1 : 0);
    }
    if (options.text_as_path_set)
    {
        opencgm_set_text_as_path(ctx, options.text_as_path ? 1 : 0);
    }
    if (options.text_path_threshold_set)
    {
        opencgm_set_text_path_threshold(ctx, options.text_path_threshold);
    }
    opencgm_set_tcc_enabled(ctx, options.tcc_enabled ? 1 : 0);
    opencgm_set_raster_logging(ctx, options.raster_logging ? 1 : 0);
    opencgm_set_geometry_logging(ctx, options.geometry_logging ? 1 : 0);
    if (options.quantize_png_set)
    {
        opencgm_set_quantize_png(ctx, options.quantize_png ? 1 : 0);
    }
    if (options.webcgm_namespace_set)
    {
        opencgm_set_webcgm_namespace(ctx, options.webcgm_namespace);
    }
    opencgm_set_geometry_validation(ctx, options.validate_geometry ? 1 : 0);
    if (options.geometry_tolerance_set)
    {
        opencgm_set_geometry_tolerance(ctx, options.geometry_tolerance);
    }
    opencgm_set_embed_shim(ctx, options.embed_shim_mode);
    if (options.shim_url_set)
    {
        opencgm_set_shim_url(ctx, options.shim_url.c_str());
    }

    if (options.font_map_set)
    {
        int status = opencgm_set_font_map(ctx, options.font_map.c_str());
        if (status != OPENCGM_OK)
        {
            const char *msg = opencgm_last_error();
            std::fprintf(stderr, "Failed to load font map (%d): %s\n", status, msg ? msg : "unknown error");
            opencgm_destroy(ctx);
            return status;
        }
    }

    if (options.verbose)
    {
        opencgm_set_verbose(ctx, 1);
    }
    if (options.quiet)
    {
        opencgm_set_quiet(ctx, 1);
    }

    int result = opencgm_convert_cgm_to_svg(ctx, input.c_str(), output.c_str());
    if (result != OPENCGM_OK)
    {
        const char *msg = opencgm_last_error();
        std::fprintf(stderr,
                     "Conversion failed (%d): %s\n",
                     result,
                     msg ? msg : "unknown error");
        opencgm_destroy(ctx);
        return result;
    }

    opencgm_destroy(ctx);
    return 0;
}

// Check if a file has a CGM extension (case-insensitive)
bool is_cgm_file(const fs::path &path)
{
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext == ".cgm";
}

// Generate output path preserving relative structure if needed
std::string get_output_path(const fs::path &input_file,
                            const fs::path &watch_dir,
                            const fs::path &output_dir)
{
    // Get the relative path from watch_dir to input_file
    fs::path relative = fs::relative(input_file.parent_path(), watch_dir);

    // Create output directory structure
    fs::path output_subdir = output_dir / relative;
    if (!fs::exists(output_subdir))
    {
        fs::create_directories(output_subdir);
    }

    // Replace .cgm extension with .svg
    fs::path output_file = output_subdir / input_file.filename();
    output_file.replace_extension(".svg");

    return output_file.string();
}

// Watch folder mode - monitors directory for new CGM files
int run_watch_mode(const CliOptions &options)
{
    fs::path watch_dir = fs::absolute(options.watch_dir);
    fs::path output_dir = fs::absolute(options.output_dir);

    // Validate directories
    if (!fs::exists(watch_dir) || !fs::is_directory(watch_dir))
    {
        std::fprintf(stderr, "Error: Watch directory does not exist: %s\n",
                     watch_dir.string().c_str());
        return 2;
    }

    if (!fs::exists(output_dir))
    {
        std::error_code ec;
        fs::create_directories(output_dir, ec);
        if (ec)
        {
            std::fprintf(stderr, "Error: Cannot create output directory: %s\n",
                         output_dir.string().c_str());
            return 2;
        }
    }

    // Setup signal handlers for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Track processed files to avoid re-processing
    std::set<std::string> processed_files;

    // Scan for existing files first
    auto scan_directory = [&]() -> std::vector<fs::path> {
        std::vector<fs::path> new_files;

        auto iterator_options = options.recursive
            ? fs::directory_options::follow_directory_symlink
            : fs::directory_options::none;

        try
        {
            if (options.recursive)
            {
                for (const auto &entry : fs::recursive_directory_iterator(watch_dir, iterator_options))
                {
                    if (entry.is_regular_file() && is_cgm_file(entry.path()))
                    {
                        std::string file_path = entry.path().string();
                        if (processed_files.find(file_path) == processed_files.end())
                        {
                            new_files.push_back(entry.path());
                        }
                    }
                }
            }
            else
            {
                for (const auto &entry : fs::directory_iterator(watch_dir))
                {
                    if (entry.is_regular_file() && is_cgm_file(entry.path()))
                    {
                        std::string file_path = entry.path().string();
                        if (processed_files.find(file_path) == processed_files.end())
                        {
                            new_files.push_back(entry.path());
                        }
                    }
                }
            }
        }
        catch (const fs::filesystem_error &e)
        {
            std::fprintf(stderr, "Warning: Directory scan error: %s\n", e.what());
        }

        return new_files;
    };

    std::fprintf(stdout, "Watch mode started.\n");
    std::fprintf(stdout, "  Input:  %s%s\n", watch_dir.string().c_str(),
                 options.recursive ? " (recursive)" : "");
    std::fprintf(stdout, "  Output: %s\n", output_dir.string().c_str());
    std::fprintf(stdout, "  Poll:   %d seconds\n", options.poll_interval);
    std::fprintf(stdout, "Press Ctrl+C to stop.\n\n");
    std::fflush(stdout);

    int total_converted = 0;
    int total_errors = 0;

    while (!g_shutdown_requested)
    {
        auto new_files = scan_directory();

        for (const auto &file : new_files)
        {
            if (g_shutdown_requested) break;

            std::string input_path = file.string();
            std::string output_path = get_output_path(file, watch_dir, output_dir);

            // Mark as processed before conversion to avoid re-processing on failure
            processed_files.insert(input_path);

            if (!options.quiet)
            {
                std::fprintf(stdout, "Converting: %s\n", file.filename().string().c_str());
                std::fflush(stdout);
            }

            int result = convert_file(options, input_path, output_path);

            if (result == 0)
            {
                total_converted++;
                if (!options.quiet)
                {
                    std::fprintf(stdout, "  -> %s\n", output_path.c_str());
                    std::fflush(stdout);
                }
            }
            else
            {
                total_errors++;
                std::fprintf(stderr, "  Error converting %s (code: %d)\n",
                             file.filename().string().c_str(), result);
            }
        }

        // Sleep for poll interval
        for (int i = 0; i < options.poll_interval * 10 && !g_shutdown_requested; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    std::fprintf(stdout, "\nWatch mode stopped.\n");
    std::fprintf(stdout, "  Total converted: %d\n", total_converted);
    std::fprintf(stdout, "  Total errors:    %d\n", total_errors);

    return total_errors > 0 ? 1 : 0;
}

} // namespace

int main(int argc, char **argv)
{
    CliOptions options;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i)
    {
        const char *arg = argv[i];
        if (std::strcmp(arg, "--profile") == 0)
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "Error: --profile requires a value\n");
                print_usage(argv[0]);
                return 2;
            }
            options.profile = to_lower_copy(argv[++i]);
            options.profile_set = true;
        }
        else if (std::strcmp(arg, "--webcgm-namespace") == 0)
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "Error: --webcgm-namespace requires a value (on|off)\n");
                print_usage(argv[0]);
                return 2;
            }
            std::string value = to_lower_copy(argv[++i]);
            if (value == "on" || value == "true" || value == "1") {
                options.webcgm_namespace = 1;
            } else if (value == "off" || value == "false" || value == "0") {
                options.webcgm_namespace = 0;
            } else {
                std::fprintf(stderr, "Error: invalid value for --webcgm-namespace (expected on|off)\n");
                return 2;
            }
            options.webcgm_namespace_set = true;
        }
        else if (std::strcmp(arg, "--preset") == 0)
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "Error: --preset requires a value\n");
                print_usage(argv[0]);
                return 2;
            }
            options.preset = argv[++i];
            options.preset_set = true;
        }
        else if (std::strcmp(arg, "--adopt-view-on-load") == 0)
        {
            bool value = true;
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                bool parsed = false;
                if (!parse_bool_token(argv[i + 1], parsed))
                {
                    std::fprintf(stderr, "Error: invalid boolean for --adopt-view-on-load\n");
                    return 2;
                }
                value = parsed;
                ++i;
            }
            options.adopt_view_on_load = value;
            options.adopt_view_on_load_set = true;
        }
        else if (std::strcmp(arg, "--no-adopt-view-on-load") == 0)
        {
            options.adopt_view_on_load = false;
            options.adopt_view_on_load_set = true;
        }
        else if (std::strcmp(arg, "--text-as-path") == 0)
        {
            bool value = true;
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                bool parsed = false;
                if (!parse_bool_token(argv[i + 1], parsed))
                {
                    std::fprintf(stderr, "Error: invalid boolean for --text-as-path\n");
                    return 2;
                }
                value = parsed;
                ++i;
            }
            options.text_as_path = value;
            options.text_as_path_set = true;
        }
        else if (std::strcmp(arg, "--no-text-as-path") == 0)
        {
            options.text_as_path = false;
            options.text_as_path_set = true;
        }
        else if (std::strcmp(arg, "--text-as-path-threshold") == 0)
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "Error: --text-as-path-threshold requires a value\n");
                return 2;
            }
            char *end = nullptr;
            double value = std::strtod(argv[++i], &end);
            if (end == argv[i])
            {
                std::fprintf(stderr, "Error: invalid number for --text-as-path-threshold\n");
                return 2;
            }
            if (value < 0.0)
            {
                value = 0.0;
            }
            options.text_path_threshold = value;
            options.text_path_threshold_set = true;
        }
        else if (std::strcmp(arg, "--font-map") == 0)
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "Error: --font-map requires a path\n");
                return 2;
            }
            options.font_map = argv[++i];
            options.font_map_set = true;
        }
        else if (std::strcmp(arg, "--no-tcc") == 0)
        {
            options.tcc_enabled = false;
        }
        else if (std::strcmp(arg, "--geometry-log") == 0)
        {
            options.geometry_logging = true;
        }
        else if (std::strcmp(arg, "--validate-geometry") == 0)
        {
            options.validate_geometry = true;
        }
        else if (std::strcmp(arg, "--geometry-tolerance") == 0)
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "Error: --geometry-tolerance requires a value\n");
                return 2;
            }
            char *end = nullptr;
            double value = std::strtod(argv[++i], &end);
            if (end == argv[i])
            {
                std::fprintf(stderr, "Error: invalid number for --geometry-tolerance\n");
                return 2;
            }
            if (value < 0.0)
            {
                value = 0.0;
            }
            options.geometry_tolerance = value;
            options.geometry_tolerance_set = true;
            options.validate_geometry = true;
        }
        else if (std::strcmp(arg, "--embed-shim") == 0)
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "Error: --embed-shim requires a value\n");
                return 2;
            }
            std::string modeValue = to_lower_copy(argv[++i]);
            if (modeValue == "auto")
            {
                options.embed_shim_mode = OPENCGM_SHIM_AUTO;
            }
            else if (modeValue == "on")
            {
                options.embed_shim_mode = OPENCGM_SHIM_ON;
            }
            else if (modeValue == "off")
            {
                options.embed_shim_mode = OPENCGM_SHIM_OFF;
            }
            else
            {
                std::fprintf(stderr, "Error: invalid value for --embed-shim (expected auto|on|off)\n");
                return 2;
            }
            options.embed_shim_mode_set = true;
        }
        else if (std::strcmp(arg, "--shim-url") == 0)
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "Error: --shim-url requires a value\n");
                return 2;
            }
            options.shim_url = argv[++i];
            options.shim_url_set = true;
        }
        else if (std::strcmp(arg, "--quantize-png") == 0)
        {
            options.quantize_png = true;
            options.quantize_png_set = true;
        }
        else if (std::strcmp(arg, "--no-quantize-png") == 0)
        {
            options.quantize_png = false;
            options.quantize_png_set = true;
        }
        else if (std::strcmp(arg, "--raster-log") == 0)
        {
            options.raster_logging = true;
        }
        else if (std::strcmp(arg, "--log") == 0)
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "Error: --log requires a value\n");
                return 2;
            }
            std::string level = to_lower_copy(argv[++i]);
            if (level == "debug")
            {
                options.verbose = true;
                options.raster_logging = true;
                options.geometry_logging = true;
            }
            else if (level == "info")
            {
                options.verbose = true;
            }
            else if (level == "quiet")
            {
                options.quiet = true;
            }
            else
            {
                std::fprintf(stderr, "Error: unknown log level '%s'\n", level.c_str());
                return 2;
            }
        }
        else if (std::strcmp(arg, "--verbose") == 0)
        {
            options.verbose = true;
        }
        else if (std::strcmp(arg, "--quiet") == 0)
        {
            options.quiet = true;
        }
        else if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0)
        {
            print_usage(argv[0]);
            return 0;
        }
        else if (std::strcmp(arg, "--watch") == 0)
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "Error: --watch requires a directory path\n");
                return 2;
            }
            options.watch_dir = argv[++i];
        }
        else if (std::strcmp(arg, "--output-dir") == 0)
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "Error: --output-dir requires a directory path\n");
                return 2;
            }
            options.output_dir = argv[++i];
        }
        else if (std::strcmp(arg, "--poll-interval") == 0)
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "Error: --poll-interval requires a value\n");
                return 2;
            }
            char *end = nullptr;
            long value = std::strtol(argv[++i], &end, 10);
            if (end == argv[i] || value < 1)
            {
                std::fprintf(stderr, "Error: invalid poll interval (must be >= 1)\n");
                return 2;
            }
            options.poll_interval = static_cast<int>(value);
        }
        else if (std::strcmp(arg, "--recursive") == 0)
        {
            options.recursive = true;
        }
        else
        {
            positional.emplace_back(arg);
        }
    }


    // Check for watch mode
    if (!options.watch_dir.empty())
    {
        if (options.output_dir.empty())
        {
            std::fprintf(stderr, "Error: --watch requires --output-dir\n");
            return 2;
        }
        return run_watch_mode(options);
    }

    // Single file conversion mode
    if (positional.size() != 2)
    {
        print_usage(argv[0]);
        return 2;
    }

    if (options.profile_set)
    {
        if (options.profile == "webcgm21")
        {
            options.profile = "webcgm";
        }
        // Accept the pre-v6 S1000D aliases here too (mirror the c_api dispatch list).
        if (options.profile == "s1000dlegacy" || options.profile == "s1000d-pre-v6" ||
            options.profile == "s1000dprev6")
        {
            options.profile = "s1000d-legacy";
        }
        if (options.profile != "s1000d" && options.profile != "s1000d-legacy" &&
            options.profile != "webcgm" && options.profile != "compat")
        {
            std::fprintf(stderr,
                         "Warning: profile '%s' is not recognised; defaulting to s1000d\n",
                         options.profile.c_str());
            options.profile = "s1000d";
        }
    }

    return convert_file(options, positional[0], positional[1]);
}
