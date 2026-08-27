/**
 * @file c_api.cpp
 * @brief OpenCGM C API implementation for CGM to SVG conversion
 *
 * ## Thread Safety Model (for WPF/Managed Interop)
 *
 * This API is designed for safe use from managed code (C#/.NET) via P/Invoke:
 *
 * 1. **Thread-Local Error Storage**: Each thread has its own error string
 *    (g_last_error is thread_local). This means:
 *    - opencgm_last_error() returns the error from the calling thread only
 *    - Error strings do not race between threads
 *    - However, the error string is only valid until the next API call on that thread
 *
 * 2. **Context Isolation**: Each opencgm_ctx_t instance is independent.
 *    - Do NOT share a context between threads
 *    - Create one context per thread, or serialize access with external locks
 *    - Context destruction must happen on the same thread or after all operations complete
 *
 * 3. **Stateless Conversion**: The conversion process does not modify global state.
 *    - Multiple threads can convert different files simultaneously
 *    - Each thread should use its own context
 *
 * 4. **Exception Safety**: All C++ exceptions are caught and converted to error codes
 *    at the API boundary. Exceptions never propagate to managed code.
 *
 * ## Recommended Usage Pattern (C#)
 * @code
 * using (var ctx = new OpenCGMWrapper())  // Creates context
 * {
 *     ctx.Configure(settings);
 *     ctx.ConvertToSvg(input, output, out error);  // Thread-safe per context
 * }  // Context destroyed
 * @endcode
 */

#include "opencgm/c_api.h"
#include "opencgm/profile_presets_embedded.h"
#include "opencgm/cgm_file.h"
#include "opencgm/command_factory.h"
#include "opencgm/document_model.h"
#include "opencgm/svg_converter.h"
#include "opencgm/xcf_generator.h"
#include "opencgm/xcf_parser.h"
#include "opencgm/utils/gzip_utils.h"
#include <string>
#include <string_view>
#include <memory>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>
#include <algorithm>
#include <iterator>
#include <cctype>
#include <filesystem>
#include <cmath>
#include <cstdint>
#include <regex>
#include <stdexcept>

#include "../third_party/nlohmann/json.hpp"

using nlohmann::json;

struct FontProfileOverride {
    std::vector<std::string> fallback_stack;
    std::optional<double> text_path_threshold;
};

struct FontMapData {
    bool loaded = false;
    std::vector<std::string> fallback_stack;
    std::optional<double> default_text_path_threshold;
    std::unordered_map<std::string, std::string> family_web;
    std::unordered_map<std::string, std::string> family_embed;
    std::vector<std::pair<std::string, std::string>> substitutions;
    std::unordered_map<std::string, FontProfileOverride> profile_overrides;
};

static std::string to_lower_copy(const std::string &s)
{
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
}

// Context structure holding converter state
struct opencgm_ctx_t {
    std::string last_error;

    // Options
    int dpi = 96;
    std::string font_map_path;

    bool verbose = false;
    bool quiet = false;
    bool trace_unknown = false;
    bool fail_on_warn = false;
    std::string log_file_path;

    bool minify = false;
    bool optimize_paths = false;
    bool pretty = true;
    double precision = 2.0;

    std::string profile = "s1000d";
    bool profile_set = false;
    bool adopt_view_on_load = false;
    bool adopt_view_on_load_set = false;
    bool text_as_path = false;
    bool text_as_path_set = false;
    double text_path_threshold = 0.0;
    bool text_path_threshold_set = false;
    bool disable_tcc = false;
    bool raster_logging = false;
    bool geometry_logging = false;
    bool color_logging = false;
    bool width_logging = false;
    bool geometry_validate = false;
    double geometry_tolerance = 0.01;
    int shim_mode = OPENCGM_SHIM_AUTO;
    std::string shim_url;
    bool quantize_png = false;
    bool embed_aps_metadata_json = false;
    double viewbox_padding = 0.0;
    double nurbs_tolerance_svg_units = 0.5;
    int palette_override_mode = 0; // 0=none, 1=monochrome, 2=custom

    // Hotspot configuration
    opencgm::HotspotProfileConfig hotspot_config = opencgm::HotspotProfileConfig::fromProfile(opencgm::HotspotProfile::Generic);

    // Output target configuration (two-tier profile model)
    opencgm::SVGConverter::OutputProfile output_profile = opencgm::SVGConverter::OutputProfile::WebCGM21;
    bool output_profile_set = false;
    opencgm::OutputTargetConfig output_target_config = opencgm::OutputTargetConfig::forS1000DIETP();

    std::ofstream log_stream;

    FontMapData font_map;

    // XCF generation options
    bool generate_xcf = false;
    std::string xcf_dtd_version = "2.1";
    bool xcf_include_hotspots = true;
    bool xcf_include_metadata = true;
    int multi_picture_mode = 0;  // 0=first only, 1=separate files, 2=combined layers

    // XCF input/merger for companion file integration
    opencgm::XcfMerger xcf_merger;
    int companion_mode = OPENCGM_COMPANION_AUTO;
    std::string xcf_input_path;

    // SVGZ compression output
    bool compress_output = false;

    // Raster encoding settings
    int raster_encoding = 0;  // 0=Auto, 1=PNG, 2=JPEG
    int jpeg_quality = 85;    // 1-100
    bool jpeg_444_subsampling = true;

    // Attribute output format and legacy CLI compatibility value.
    std::string output_format = "combined";
    bool output_format_set = false;
    opencgm::svg::AttributeManager::OutputFormat attribute_output_format =
        opencgm::svg::AttributeManager::OutputFormat::Multi;

    std::unique_ptr<opencgm::ConversionDocumentModel> last_document;
    std::unique_ptr<opencgm::ConversionReport> last_report;
    std::string last_report_json;
    std::string last_report_text;
    std::string last_profile_detection_json;
    std::string last_validation_json;
    std::string last_builtin_profile_catalog_json;
};

// Thread-local last error
static thread_local std::string g_last_error;

static void set_error(const std::string& msg) {
    g_last_error = msg;
}

// Basic path validation for local desktop usage.
static bool validate_file_path(const char* path) {
    if (!path || path[0] == '\0') {
        return false;
    }
    // Reject control characters that can break logs or tooling.
    for (const char* p = path; *p != '\0'; ++p) {
        unsigned char ch = static_cast<unsigned char>(*p);
        if (ch < 0x20 && ch != '\t') {
            return false;
        }
    }
    return true;
}

static bool write_xcf_output(opencgm_ctx_t* ctx,
                             const opencgm::CGMFile* cgm_file,
                             const std::string& svg_filename,
                             const std::string& output_xcf_path) {
    opencgm::XcfOptions options;
    options.includeHotspots = ctx->xcf_include_hotspots;
    options.includeMetadata = ctx->xcf_include_metadata;
    options.dtdVersion = ctx->xcf_dtd_version;
    options.prettyPrint = ctx->pretty;

    if (ctx->xcf_dtd_version == "2.0") {
        options.dtdUrl = "http://www.w3.org/Graphics/WebCGM/DTD/2.0/webcgm20.dtd";
    } else if (ctx->xcf_dtd_version == "2.1") {
        options.dtdUrl = "http://www.w3.org/Graphics/WebCGM/DTD/2.1/webcgm21.dtd";
    }

    opencgm::XcfGenerator generator;
    const auto hotspots = generator.extractHotspots(cgm_file);
    return generator.writeToFile(
        cgm_file,
        svg_filename,
        output_xcf_path,
        hotspots,
        options);
}

static int load_font_map(opencgm_ctx_t* ctx);

struct GeometryValidationResult {
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

static opencgm::ConversionIssue make_runtime_issue(
    opencgm::ValidationSeverity severity,
    opencgm::PreservationDisposition disposition,
    const std::string& category,
    const std::string& message)
{
    opencgm::ConversionIssue issue;
    issue.severity = severity;
    issue.disposition = disposition;
    issue.category = category;
    issue.message = message;
    return issue;
}

static std::vector<opencgm::ConversionIssue> make_geometry_issues(const GeometryValidationResult& validation)
{
    std::vector<opencgm::ConversionIssue> issues;
    issues.reserve(validation.warnings.size() + validation.errors.size());

    for (const auto& warning : validation.warnings)
    {
        issues.push_back(make_runtime_issue(
            opencgm::ValidationSeverity::WARNING,
            opencgm::PreservationDisposition::Degraded,
            "geometry",
            warning));
    }

    for (const auto& error : validation.errors)
    {
        issues.push_back(make_runtime_issue(
            opencgm::ValidationSeverity::ERROR,
            opencgm::PreservationDisposition::Dropped,
            "geometry",
            error));
    }

    return issues;
}

static std::vector<opencgm::ConversionIssue> make_unknown_command_issues(
    const opencgm::CGMFile& cgm_file,
    bool enabled)
{
    std::vector<opencgm::ConversionIssue> issues;
    if (!enabled)
    {
        return issues;
    }

    for (const auto& command : cgm_file.commands())
    {
        if (dynamic_cast<const opencgm::UnknownCommand*>(command.get()) == nullptr)
        {
            continue;
        }

        std::ostringstream message;
        message << "Unsupported CGM command class "
                << static_cast<int>(command->elementClass())
                << ", element " << command->elementId()
                << " was not rendered";
        issues.push_back(make_runtime_issue(
            opencgm::ValidationSeverity::WARNING,
            opencgm::PreservationDisposition::Dropped,
            "unknown-command",
            message.str()));
    }

    return issues;
}

static std::string remove_xml_comments(std::string_view svg_content)
{
    std::string normalized;
    normalized.reserve(svg_content.size());

    size_t cursor = 0;
    while (cursor < svg_content.size())
    {
        const size_t comment_start = svg_content.find("<!--", cursor);
        if (comment_start == std::string_view::npos)
        {
            normalized.append(svg_content.substr(cursor));
            break;
        }

        normalized.append(svg_content.substr(cursor, comment_start - cursor));
        const size_t comment_end = svg_content.find("-->", comment_start + 4);
        if (comment_end == std::string_view::npos)
        {
            break;
        }

        cursor = comment_end + 3;
    }

    return normalized;
}

static std::string remove_empty_svg_groups(std::string svg_content)
{
    static const std::regex empty_group_regex(
        R"(<((?:[A-Za-z_][A-Za-z0-9_.-]*:)?g)\b[^>]*>\s*</\1>)",
        std::regex::icase);

    while (true)
    {
        const auto updated = std::regex_replace(svg_content, empty_group_regex, "");
        if (updated == svg_content)
        {
            return updated;
        }

        svg_content = updated;
    }
}

static bool is_xml_name_char(char ch)
{
    const auto uch = static_cast<unsigned char>(ch);
    return std::isalnum(uch) != 0 || ch == '_' || ch == '-' || ch == ':' || ch == '.';
}

static bool equals_ignore_case(std::string_view lhs, std::string_view rhs)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }

    for (size_t index = 0; index < lhs.size(); ++index)
    {
        if (std::tolower(static_cast<unsigned char>(lhs[index])) !=
            std::tolower(static_cast<unsigned char>(rhs[index])))
        {
            return false;
        }
    }

    return true;
}

static std::string_view xml_local_name(std::string_view qualified_name)
{
    const size_t separator = qualified_name.rfind(':');
    return separator == std::string_view::npos
        ? qualified_name
        : qualified_name.substr(separator + 1);
}

static size_t find_xml_tag_end(std::string_view xml_content, size_t start)
{
    char quote = '\0';
    for (size_t index = start; index < xml_content.size(); ++index)
    {
        const char current = xml_content[index];
        if (quote != '\0')
        {
            if (current == quote)
            {
                quote = '\0';
            }
            continue;
        }

        if (current == '"' || current == '\'')
        {
            quote = current;
            continue;
        }

        if (current == '>')
        {
            return index;
        }
    }

    return std::string_view::npos;
}

static bool is_self_closing_xml_tag(std::string_view xml_content, size_t tag_end)
{
    if (tag_end == 0 || tag_end == std::string_view::npos)
    {
        return false;
    }

    size_t cursor = tag_end;
    while (cursor > 0)
    {
        --cursor;
        const char current = xml_content[cursor];
        if (std::isspace(static_cast<unsigned char>(current)) != 0)
        {
            continue;
        }

        return current == '/';
    }

    return false;
}

struct XmlElementSpan
{
    size_t start = 0;
    size_t content_start = 0;
    size_t close_start = 0;
    size_t end = 0;
    bool self_closing = false;
    std::string name;
};

static size_t consume_xml_markup(std::string_view xml_content, size_t markup_start)
{
    if (markup_start >= xml_content.size() || xml_content[markup_start] != '<')
    {
        return std::string_view::npos;
    }

    if (xml_content.compare(markup_start, 4, "<!--") == 0)
    {
        const size_t end = xml_content.find("-->", markup_start + 4);
        return end == std::string_view::npos ? end : end + 3;
    }

    if (xml_content.compare(markup_start, 9, "<![CDATA[") == 0)
    {
        const size_t end = xml_content.find("]]>", markup_start + 9);
        return end == std::string_view::npos ? end : end + 3;
    }

    if (xml_content.compare(markup_start, 2, "<?") == 0)
    {
        const size_t end = xml_content.find("?>", markup_start + 2);
        return end == std::string_view::npos ? end : end + 2;
    }

    if (xml_content.compare(markup_start, 2, "<!") == 0)
    {
        const size_t end = find_xml_tag_end(xml_content, markup_start + 2);
        return end == std::string_view::npos ? end : end + 1;
    }

    return std::string_view::npos;
}

static bool try_parse_xml_element(std::string_view xml_content, size_t element_start, XmlElementSpan& span)
{
    if (element_start >= xml_content.size() || xml_content[element_start] != '<')
    {
        return false;
    }

    const size_t name_start = element_start + 1;
    if (name_start >= xml_content.size())
    {
        return false;
    }

    if (xml_content[name_start] == '/' || xml_content[name_start] == '!' || xml_content[name_start] == '?')
    {
        return false;
    }

    size_t name_end = name_start;
    while (name_end < xml_content.size() && is_xml_name_char(xml_content[name_end]))
    {
        ++name_end;
    }

    if (name_end == name_start)
    {
        return false;
    }

    const size_t tag_end = find_xml_tag_end(xml_content, name_end);
    if (tag_end == std::string_view::npos)
    {
        return false;
    }

    span = {};
    span.start = element_start;
    span.content_start = tag_end + 1;
    span.self_closing = is_self_closing_xml_tag(xml_content, tag_end);
    span.name = std::string(xml_content.substr(name_start, name_end - name_start));

    if (span.self_closing)
    {
        span.close_start = span.content_start;
        span.end = span.content_start;
        return true;
    }

    int depth = 1;
    size_t cursor = span.content_start;
    while (cursor < xml_content.size())
    {
        const size_t next_tag = xml_content.find('<', cursor);
        if (next_tag == std::string_view::npos)
        {
            return false;
        }

        const size_t markup_end = consume_xml_markup(xml_content, next_tag);
        if (markup_end != std::string_view::npos)
        {
            cursor = markup_end;
            continue;
        }

        if (next_tag + 1 < xml_content.size() && xml_content[next_tag + 1] == '/')
        {
            size_t close_name_start = next_tag + 2;
            while (close_name_start < xml_content.size() &&
                   std::isspace(static_cast<unsigned char>(xml_content[close_name_start])) != 0)
            {
                ++close_name_start;
            }

            size_t close_name_end = close_name_start;
            while (close_name_end < xml_content.size() && is_xml_name_char(xml_content[close_name_end]))
            {
                ++close_name_end;
            }

            const size_t close_tag_end = find_xml_tag_end(xml_content, close_name_end);
            if (close_tag_end == std::string_view::npos)
            {
                return false;
            }

            if (equals_ignore_case(
                    xml_content.substr(close_name_start, close_name_end - close_name_start),
                    span.name))
            {
                --depth;
                if (depth == 0)
                {
                    span.close_start = next_tag;
                    span.end = close_tag_end + 1;
                    return true;
                }
            }

            cursor = close_tag_end + 1;
            continue;
        }

        XmlElementSpan nested_span;
        if (!try_parse_xml_element(xml_content, next_tag, nested_span))
        {
            const size_t tag_end_fallback = find_xml_tag_end(xml_content, next_tag + 1);
            if (tag_end_fallback == std::string_view::npos)
            {
                return false;
            }

            cursor = tag_end_fallback + 1;
            continue;
        }

        if (equals_ignore_case(nested_span.name, span.name))
        {
            ++depth;
        }

        cursor = nested_span.content_start;
        if (nested_span.self_closing)
        {
            cursor = nested_span.end;
        }
    }

    return false;
}

static std::optional<std::string> extract_xml_attribute_value(
    std::string_view xml_fragment,
    const std::regex& attribute_pattern)
{
    std::match_results<std::string_view::const_iterator> match;
    if (!std::regex_search(xml_fragment.begin(), xml_fragment.end(), match, attribute_pattern))
    {
        return std::nullopt;
    }

    if (match[2].matched)
    {
        return std::string(match[2].str());
    }

    if (match[3].matched)
    {
        return std::string(match[3].str());
    }

    return std::nullopt;
}

static std::unordered_set<std::string> collect_used_svg_definition_ids(std::string_view svg_content)
{
    static const std::regex url_reference_pattern(R"(url\(#([^)]+)\))", std::regex::icase);
    static const std::regex href_double_quote_pattern(
        R"(\b(?:href|xlink:href)\s*=\s*\"#([^\"]+)\")",
        std::regex::icase);
    static const std::regex href_single_quote_pattern(
        R"(\b(?:href|xlink:href)\s*=\s*'#([^']+)')",
        std::regex::icase);

    std::unordered_set<std::string> used_ids;
    auto collect_matches = [&used_ids, svg_content](const std::regex& pattern) {
        std::match_results<std::string_view::const_iterator> match;
        auto search_start = svg_content.begin();
        while (std::regex_search(search_start, svg_content.end(), match, pattern))
        {
            if (match[1].matched)
            {
                used_ids.insert(match[1].str());
            }

            search_start = match[0].second;
        }
    };

    collect_matches(url_reference_pattern);
    collect_matches(href_double_quote_pattern);
    collect_matches(href_single_quote_pattern);
    return used_ids;
}

static std::string remove_unused_svg_defs(std::string svg_content)
{
    static const std::regex id_attribute_pattern(
        R"(\bid\s*=\s*(\"([^\"]*)\"|'([^']*)'))",
        std::regex::icase);

    const auto used_ids = collect_used_svg_definition_ids(svg_content);
    std::string normalized;
    normalized.reserve(svg_content.size());

    size_t cursor = 0;
    while (cursor < svg_content.size())
    {
        const size_t next_tag = svg_content.find('<', cursor);
        if (next_tag == std::string::npos)
        {
            normalized.append(svg_content.substr(cursor));
            break;
        }

        normalized.append(svg_content.substr(cursor, next_tag - cursor));

        XmlElementSpan defs_span;
        if (!try_parse_xml_element(svg_content, next_tag, defs_span) ||
            !equals_ignore_case(xml_local_name(defs_span.name), "defs"))
        {
            normalized.push_back('<');
            cursor = next_tag + 1;
            continue;
        }

        std::string kept_defs_content;
        kept_defs_content.reserve(defs_span.close_start - defs_span.content_start);

        size_t defs_cursor = defs_span.content_start;
        size_t kept_child_count = 0;
        while (defs_cursor < defs_span.close_start)
        {
            const size_t child_tag = svg_content.find('<', defs_cursor);
            if (child_tag == std::string::npos || child_tag >= defs_span.close_start)
            {
                kept_defs_content.append(
                    svg_content.substr(defs_cursor, defs_span.close_start - defs_cursor));
                break;
            }

            kept_defs_content.append(svg_content.substr(defs_cursor, child_tag - defs_cursor));

            const size_t markup_end = consume_xml_markup(svg_content, child_tag);
            if (markup_end != std::string::npos && markup_end <= defs_span.close_start)
            {
                kept_defs_content.append(svg_content.substr(child_tag, markup_end - child_tag));
                defs_cursor = markup_end;
                continue;
            }

            if (child_tag + 1 < defs_span.close_start && svg_content[child_tag + 1] == '/')
            {
                kept_defs_content.append(svg_content.substr(child_tag, defs_span.close_start - child_tag));
                defs_cursor = defs_span.close_start;
                break;
            }

            XmlElementSpan child_span;
            if (!try_parse_xml_element(svg_content, child_tag, child_span) || child_span.end > defs_span.close_start)
            {
                kept_defs_content.append(svg_content.substr(child_tag, defs_span.close_start - child_tag));
                defs_cursor = defs_span.close_start;
                break;
            }

            const std::string_view start_tag(
                svg_content.data() + child_span.start,
                child_span.content_start - child_span.start);
            const auto id = extract_xml_attribute_value(start_tag, id_attribute_pattern);
            const bool keep_child = !id.has_value() || used_ids.find(*id) != used_ids.end();
            if (keep_child)
            {
                kept_defs_content.append(svg_content.substr(child_span.start, child_span.end - child_span.start));
                ++kept_child_count;
            }

            defs_cursor = child_span.end;
        }

        if (kept_child_count > 0)
        {
            normalized.append(svg_content.substr(defs_span.start, defs_span.content_start - defs_span.start));
            normalized.append(kept_defs_content);
            normalized.append(svg_content.substr(defs_span.close_start, defs_span.end - defs_span.close_start));
        }

        cursor = defs_span.end;
    }

    return normalized;
}

static std::string trim_trailing_zeroes(std::string value)
{
    const auto decimal_pos = value.find('.');
    if (decimal_pos == std::string::npos)
    {
        return value;
    }

    while (!value.empty() && value.back() == '0')
    {
        value.pop_back();
    }

    if (!value.empty() && value.back() == '.')
    {
        value.pop_back();
    }

    if (value == "-0")
    {
        return "0";
    }

    return value;
}

static std::string format_path_number(double value, int decimals)
{
    if (decimals <= 0)
    {
        const auto rounded = std::round(value);
        if (std::abs(rounded) < 1e-9)
        {
            return "0";
        }

        std::ostringstream oss;
        oss << static_cast<long long>(rounded);
        return oss.str();
    }

    const auto scale = std::pow(10.0, decimals);
    const auto rounded = std::round(value * scale) / scale;
    if (std::abs(rounded) < 1e-9)
    {
        return "0";
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(decimals) << rounded;
    return trim_trailing_zeroes(oss.str());
}

static std::string optimize_path_data_value(std::string path_data, int decimals)
{
    static const std::regex number_regex(R"([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)");
    static const std::regex whitespace_regex(R"(\s+)");
    static const std::regex command_spacing_regex(R"(\s*([MLHVCSQTAZmlhvcsqtaz])\s*)");

    std::string optimized;
    optimized.reserve(path_data.size());

    std::smatch match;
    std::string::const_iterator search_start(path_data.cbegin());
    while (std::regex_search(search_start, path_data.cend(), match, number_regex))
    {
        optimized.append(search_start, match[0].first);

        try
        {
            const auto value = std::stod(match.str());
            optimized.append(format_path_number(value, decimals));
        }
        catch (const std::exception&)
        {
            optimized.append(match.str());
        }

        search_start = match[0].second;
    }

    optimized.append(search_start, path_data.cend());
    optimized = std::regex_replace(optimized, whitespace_regex, " ");
    optimized = std::regex_replace(optimized, command_spacing_regex, "$1");

    const auto first = optimized.find_first_not_of(' ');
    if (first == std::string::npos)
    {
        return std::string();
    }

    const auto last = optimized.find_last_not_of(' ');
    return optimized.substr(first, last - first + 1);
}

static std::string rewrite_path_data_attributes(std::string svg_content, const std::regex& pattern, int decimals)
{
    std::string rewritten;
    rewritten.reserve(svg_content.size());

    std::smatch match;
    std::string::const_iterator search_start(svg_content.cbegin());
    while (std::regex_search(search_start, svg_content.cend(), match, pattern))
    {
        rewritten.append(search_start, match[0].first);
        rewritten.append(match[1].str());
        rewritten.append(optimize_path_data_value(match[2].str(), decimals));
        rewritten.append(match[3].str());
        search_start = match[0].second;
    }

    rewritten.append(search_start, svg_content.cend());
    return rewritten;
}

static std::string optimize_svg_path_attributes(std::string svg_content, double precision)
{
    const int decimals = std::clamp(static_cast<int>(std::round(precision)), 0, 6);
    static const std::regex path_data_double_quote(R"((<path\b[^>]*?\bd\s*=\s*")([^"]*)("))", std::regex::icase);
    static const std::regex path_data_single_quote(R"((<path\b[^>]*?\bd\s*=\s*')([^']*)('))", std::regex::icase);

    svg_content = rewrite_path_data_attributes(std::move(svg_content), path_data_double_quote, decimals);
    return rewrite_path_data_attributes(std::move(svg_content), path_data_single_quote, decimals);
}

static std::string normalize_svg_output(
    std::string svg_content,
    bool minify_output,
    bool optimize_paths,
    double precision)
{
    if (svg_content.empty())
    {
        return svg_content;
    }

    if (optimize_paths)
    {
        svg_content = remove_xml_comments(svg_content);
        svg_content = remove_unused_svg_defs(std::move(svg_content));
        svg_content = remove_empty_svg_groups(std::move(svg_content));
        svg_content = optimize_svg_path_attributes(std::move(svg_content), precision);
    }

    if (!minify_output)
    {
        return svg_content;
    }

    static const std::regex inter_element_whitespace(R"(>\s+<)");
    if (!optimize_paths)
    {
        svg_content = remove_xml_comments(svg_content);
    }
    svg_content = std::regex_replace(svg_content, inter_element_whitespace, "><");

    const auto first = svg_content.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return std::string();
    }

    const auto last = svg_content.find_last_not_of(" \t\r\n");
    return svg_content.substr(first, last - first + 1);
}

static void store_conversion_report(
    opencgm_ctx_t* ctx,
    opencgm::ConversionDocumentModel document,
    const std::string& output_path,
    bool success,
    bool compressed_output,
    bool minified_output,
    bool optimized_paths,
    const std::optional<opencgm::GeometryMetrics>& geometry_metrics,
    std::vector<opencgm::ConversionIssue> runtime_issues)
{
    if (!ctx)
    {
        return;
    }

    auto report = std::make_unique<opencgm::ConversionReport>();
    report->document = document;
    report->outputPath = output_path;
    report->success = success;
    report->compressedOutput = compressed_output;
    report->minifiedOutput = minified_output;
    report->optimizedPaths = optimized_paths;
    report->geometryMetrics = geometry_metrics;
    report->runtimeIssues = std::move(runtime_issues);

    ctx->last_document = std::make_unique<opencgm::ConversionDocumentModel>(std::move(document));
    ctx->last_report = std::move(report);
    ctx->last_report_json = ctx->last_report->generateJsonReport();
    ctx->last_report_text = ctx->last_report->generateTextReport();
}

static std::string profile_type_to_contract_name(opencgm::ProfileType profile)
{
    switch (profile)
    {
    case opencgm::ProfileType::WEBCGM_1_0:
        return "webcgm-1.0";
    case opencgm::ProfileType::WEBCGM_2_0:
        return "webcgm-2.0";
    case opencgm::ProfileType::WEBCGM_2_1:
        return "webcgm-2.1";
    case opencgm::ProfileType::ISO_IEC_8632_COMPAT:
        return "iso-iec-8632";
    case opencgm::ProfileType::ATA_GREXCHANGE_2_6:
        return "ata-grexchange-2.6";
    case opencgm::ProfileType::ATA_GREXCHANGE_2_7:
        return "ata-grexchange-2.7";
    case opencgm::ProfileType::ATA_GREXCHANGE_2_8:
        return "ata-grexchange-2.8";
    case opencgm::ProfileType::ATA_GREXCHANGE_2_9:
        return "ata-grexchange-2.9";
    case opencgm::ProfileType::S1000D_ISSUE_6:
        return "s1000d-issue-6";
    case opencgm::ProfileType::PIP_CGGC:
        return "pip-cggc";
    case opencgm::ProfileType::CALS_MIL_PRF_28003:
        return "cals";
    case opencgm::ProfileType::UNKNOWN:
    default:
        return "unknown";
    }
}

static bool is_auto_profile_contract(std::string_view requested_profile)
{
    if (requested_profile.empty())
    {
        return true;
    }

    const std::string lowered = to_lower_copy(std::string(requested_profile));
    return lowered == "auto" || lowered == "auto-detect" || lowered == "autodetect";
}

static std::optional<opencgm::ProfileType> try_parse_validation_profile(std::string_view requested_profile)
{
    if (requested_profile.empty())
    {
        return std::nullopt;
    }

    const std::string lowered = to_lower_copy(std::string(requested_profile));

    if (lowered == "generic" || lowered == "compat" || lowered == "iso-iec-8632" || lowered == "iso8632")
    {
        return opencgm::ProfileType::ISO_IEC_8632_COMPAT;
    }

    if (lowered == "webcgm" || lowered == "strict" || lowered == "webcgm-2.1")
    {
        return opencgm::ProfileType::WEBCGM_2_1;
    }

    if (lowered == "s1000d" || lowered == "s1000d-issue-6" ||
        lowered == "s1000d-legacy" || lowered == "s1000dlegacy")
    {
        // v6 vs legacy is an output-side attribute choice; both validate
        // against the same S1000D input family rules.
        return opencgm::ProfileType::S1000D_ISSUE_6;
    }

    if (lowered == "ata" || lowered == "ata2200" || lowered == "ataispec2200" ||
        lowered == "ata-grexchange" || lowered == "ata-grexchange-2.9")
    {
        return opencgm::ProfileType::ATA_GREXCHANGE_2_9;
    }

    if (lowered == "pip" || lowered == "pip-cggc" ||
        lowered == "cgm-pip" || lowered == "cgmpip")
    {
        return opencgm::ProfileType::PIP_CGGC;
    }

    if (lowered == "cals" || lowered == "mil-prf-28003" || lowered == "mil-d-28003")
    {
        return opencgm::ProfileType::CALS_MIL_PRF_28003;
    }

    if (lowered == "cgmplus" || lowered == "cgm+")
    {
        // Retired preset value; its honest input family is WebCGM 2.1.
        return opencgm::ProfileType::WEBCGM_2_1;
    }

    return std::nullopt;
}

static std::string make_validation_message_code(const opencgm::ValidationMessage& message, size_t index)
{
    // Generate a deterministic synthetic code from the native rule/message tuple
    // until the native validators emit stable spec-facing codes directly.
    const std::string seed = !message.rule.empty()
        ? message.rule + "|" + message.message
        : message.message;

    uint32_t hash = 2166136261u;
    for (unsigned char ch : seed)
    {
        hash ^= ch;
        hash *= 16777619u;
    }

    std::ostringstream oss;
    oss << "NAT" << std::uppercase << std::hex << std::setw(6) << std::setfill('0')
        << ((hash + static_cast<uint32_t>(index)) & 0xFFFFFFu);
    return oss.str();
}

static std::string build_profile_detection_json(const opencgm::ProfileDetector::DetectionResult& detection)
{
    json payload;
    payload["profile"] = profile_type_to_contract_name(detection.profile);
    payload["reason"] = detection.reason;
    payload["confident"] = detection.confident;
    payload["metadata"] = {
        {"profileId", detection.metadata.profileId},
        {"profileEdition", detection.metadata.profileEdition},
        {"sourceApplication", detection.metadata.sourceApplication},
        {"cgmVersion", detection.metadata.cgmVersion},
        {"detectionConfidence", detection.metadata.detectionConfidence},
        {"hasApsStructures", detection.metadata.hasApsStructures},
        {"hasNurbsElements", detection.metadata.hasNurbsElements},
        {"hasParabolicArcs", detection.metadata.hasParabolicArcs},
        {"pictureCount", detection.metadata.pictureCount}
    };

    return payload.dump();
}

static std::string build_validation_json(
    std::string_view requested_profile,
    opencgm::ProfileType effective_profile,
    const std::vector<opencgm::ValidationMessage>& messages,
    const std::optional<opencgm::ProfileDetector::DetectionResult>& detection)
{
    const opencgm::ValidationReport report(messages);
    const bool was_auto_detected = detection.has_value();

    json payload;
    payload["requestedProfile"] = requested_profile.empty() ? "auto" : std::string(requested_profile);
    payload["effectiveProfile"] = profile_type_to_contract_name(effective_profile);
    payload["wasAutoDetected"] = was_auto_detected;
    payload["isUnknownVariant"] = detection.has_value() && detection->profile == opencgm::ProfileType::UNKNOWN;
    payload["summary"] = {
        {"errors", report.getErrorCount()},
        {"warnings", report.getWarningCount()},
        {"info", report.getInfoCount()},
        {"passed", report.passed()}
    };

    if (detection.has_value())
    {
        payload["detection"] = {
            {"profile", profile_type_to_contract_name(detection->profile)},
            {"reason", detection->reason},
            {"confident", detection->confident},
            {"metadata", {
                {"profileId", detection->metadata.profileId},
                {"profileEdition", detection->metadata.profileEdition},
                {"sourceApplication", detection->metadata.sourceApplication},
                {"cgmVersion", detection->metadata.cgmVersion},
                {"detectionConfidence", detection->metadata.detectionConfidence},
                {"hasApsStructures", detection->metadata.hasApsStructures},
                {"hasNurbsElements", detection->metadata.hasNurbsElements},
                {"hasParabolicArcs", detection->metadata.hasParabolicArcs},
                {"pictureCount", detection->metadata.pictureCount}
            }}
        };
    }

    payload["messages"] = json::array();
    for (size_t i = 0; i < messages.size(); ++i)
    {
        const auto& message = messages[i];
        json entry = {
            {"code", make_validation_message_code(message, i)},
            {"severity", message.getSeverityString()},
            {"message", message.message},
            {"element", message.elementName},
            {"elementId", message.elementId},
            {"pictureIndex", message.pictureIndex},
            {"byteOffset", message.byteOffset},
            {"rule", message.rule}
        };
        payload["messages"].push_back(std::move(entry));
    }

    return payload.dump();
}

static const char* embedded_builtin_profile_catalog_json()
{
    // Build-time embedded copy of engine/config/profile-presets.json (see
    // cmake/profile_presets_embedded.h.in). Used only when the on-disk
    // catalog is missing; guaranteed identical to the file at build time.
    return opencgm::generated::kBuiltInProfileCatalogJson;
}

static std::string load_builtin_profile_catalog_json()
{
    const std::vector<std::filesystem::path> candidate_paths = {
        std::filesystem::current_path() / "config" / "profile-presets.json",
        std::filesystem::current_path() / "engine" / "config" / "profile-presets.json"
    };

    for (const auto& candidate : candidate_paths)
    {
        std::error_code ec;
        if (!std::filesystem::exists(candidate, ec) || ec)
        {
            continue;
        }

        std::ifstream in(candidate, std::ios::in | std::ios::binary);
        if (!in.is_open())
        {
            continue;
        }

        std::ostringstream contents;
        contents << in.rdbuf();
        auto catalog = contents.str();
        if (!catalog.empty())
        {
            return catalog;
        }
    }

    return embedded_builtin_profile_catalog_json();
}

static GeometryValidationResult validate_geometry_metrics(const opencgm::GeometryMetrics &metrics,
                                                          double tolerance,
                                                          bool strict_profile,
                                                          bool adopt_view_on_load_request)
{
    GeometryValidationResult result;
    const double relTol = 1e-4;
    double tol = tolerance;
    if (!std::isfinite(tol) || tol < 0.0)
    {
        tol = 0.01;
    }

    auto tolFor = [&](double magnitude) {
        double rel = std::abs(magnitude) * relTol;
        double base = std::max(tol, rel);
        if (!(base > 0.0))
        {
            base = tol > 0.0 ? tol : 1e-6;
        }
        return base;
    };

    if (!std::isfinite(metrics.scale_factor) || metrics.scale_factor <= 0.0)
    {
        result.errors.emplace_back("Metric scale factor is invalid (<= 0 or non-finite)");
    }

    if (!metrics.flip_y_applied)
    {
        result.errors.emplace_back("Y-axis flip was not applied; SVG will render upside-down");
    }

    if (!std::isfinite(metrics.viewbox_width) || metrics.viewbox_width <= 0.0 ||
        !std::isfinite(metrics.viewbox_height) || metrics.viewbox_height <= 0.0)
    {
        result.errors.emplace_back("viewBox dimensions are invalid or zero");
    }

    if (metrics.compatibility_mode && strict_profile)
    {
        result.errors.emplace_back("Strict profile requested but compatibility heuristics are active");
    }

    if (!metrics.view_context_present && adopt_view_on_load_request && strict_profile)
    {
        // No view context to adopt; warn so callers know why nothing happened.
        result.warnings.emplace_back("Requested viewcontext adoption but no unique viewcontext was detected");
    }

    const bool adopting_view_context = metrics.view_context_adopted && metrics.view_context_present;

    if (strict_profile && metrics.auto_fit_applied)
    {
        result.errors.emplace_back("Auto-fit to geometry triggered under strict WebCGM profile");
    }

    double expectedWidth = metrics.picture_width * metrics.scale_factor;
    double expectedHeight = metrics.picture_height * metrics.scale_factor;
    double expectedX = 0.0;
    double expectedY = 0.0;

    if (adopting_view_context)
    {
        double vcWidth = (metrics.view_context_max_x - metrics.view_context_min_x) * metrics.scale_factor;
        double vcHeight = (metrics.view_context_max_y - metrics.view_context_min_y) * metrics.scale_factor;
        expectedWidth = std::max(vcWidth, 1e-6);
        expectedHeight = std::max(vcHeight, 1e-6);
        expectedX = (metrics.view_context_min_x - metrics.picture_min_x) * metrics.scale_factor;
        expectedY = (metrics.picture_max_y - metrics.view_context_max_y) * metrics.scale_factor;
    }

    if (metrics.viewbox_width > 0.0)
    {
        double diff = std::abs(metrics.viewbox_width - expectedWidth);
        if (diff > tolFor(expectedWidth))
        {
            std::ostringstream oss;
            oss << "viewBox width (" << metrics.viewbox_width << ") differs from expected "
                << expectedWidth << " by " << diff;
            result.errors.emplace_back(oss.str());
        }
    }

    if (metrics.viewbox_height > 0.0)
    {
        double diff = std::abs(metrics.viewbox_height - expectedHeight);
        if (diff > tolFor(expectedHeight))
        {
            std::ostringstream oss;
            oss << "viewBox height (" << metrics.viewbox_height << ") differs from expected "
                << expectedHeight << " by " << diff;
            result.errors.emplace_back(oss.str());
        }
    }

    {
        double diffX = std::abs(metrics.viewbox_x - expectedX);
        if (diffX > tolFor(expectedWidth))
        {
            std::ostringstream oss;
            oss << "viewBox x-offset (" << metrics.viewbox_x << ") differs from expected "
                << expectedX << " by " << diffX;
            if (adopting_view_context)
            {
                result.errors.emplace_back(oss.str());
            }
            else
            {
                result.errors.emplace_back(oss.str());
            }
        }
    }

    {
        double diffY = std::abs(metrics.viewbox_y - expectedY);
        if (diffY > tolFor(expectedHeight))
        {
            std::ostringstream oss;
            oss << "viewBox y-offset (" << metrics.viewbox_y << ") differs from expected "
                << expectedY << " by " << diffY;
            result.errors.emplace_back(oss.str());
        }
    }

    if (metrics.has_geometry)
    {
        double geomMinX = (metrics.geometry_min_x - metrics.picture_min_x) * metrics.scale_factor;
        double geomMaxX = (metrics.geometry_max_x - metrics.picture_min_x) * metrics.scale_factor;
        double geomTop = (metrics.picture_max_y - metrics.geometry_max_y) * metrics.scale_factor;
        double geomBottom = (metrics.picture_max_y - metrics.geometry_min_y) * metrics.scale_factor;

        double viewboxMinX = metrics.viewbox_x;
        double viewboxMaxX = metrics.viewbox_x + metrics.viewbox_width;
        double viewboxMinY = metrics.viewbox_y;
        double viewboxMaxY = metrics.viewbox_y + metrics.viewbox_height;

        double containmentTolX = tolFor(metrics.viewbox_width);
        double containmentTolY = tolFor(metrics.viewbox_height);

        if (geomMinX < viewboxMinX - containmentTolX)
        {
            std::ostringstream oss;
            oss << "Geometry extends " << (viewboxMinX - geomMinX) << " units left of viewBox";
            if (adopting_view_context)
            {
                result.warnings.emplace_back(oss.str());
            }
            else
            {
                result.errors.emplace_back(oss.str());
            }
        }
        if (geomMaxX > viewboxMaxX + containmentTolX)
        {
            std::ostringstream oss;
            oss << "Geometry extends " << (geomMaxX - viewboxMaxX) << " units right of viewBox";
            if (adopting_view_context)
            {
                result.warnings.emplace_back(oss.str());
            }
            else
            {
                result.errors.emplace_back(oss.str());
            }
        }
        if (geomTop < viewboxMinY - containmentTolY)
        {
            std::ostringstream oss;
            oss << "Geometry extends " << (viewboxMinY - geomTop) << " units above viewBox";
            if (adopting_view_context)
            {
                result.warnings.emplace_back(oss.str());
            }
            else
            {
                result.errors.emplace_back(oss.str());
            }
        }
        if (geomBottom > viewboxMaxY + containmentTolY)
        {
            std::ostringstream oss;
            oss << "Geometry extends " << (geomBottom - viewboxMaxY) << " units below viewBox";
            if (adopting_view_context)
            {
                result.warnings.emplace_back(oss.str());
            }
            else
            {
                result.errors.emplace_back(oss.str());
            }
        }
    }
    else
    {
        result.warnings.emplace_back("No drawable geometry detected; viewBox is based on VDC extent");
    }

    if (metrics.coverage_x < -relTol || metrics.coverage_y < -relTol)
    {
        result.warnings.emplace_back("Computed coverage ratio is negative; check picture extent inputs");
    }
    if (metrics.coverage_x > 1.0 + 0.05 || metrics.coverage_y > 1.0 + 0.05)
    {
        result.warnings.emplace_back("Coverage ratio exceeds 1.0; viewBox may include autofit padding");
    }

    return result;
}

static std::vector<std::string> as_string_array(const json &value)
{
    std::vector<std::string> result;
    if (!value.is_array())
    {
        return result;
    }
    for (const auto &item : value)
    {
        if (item.is_string())
        {
            result.push_back(item.get<std::string>());
        }
    }
    return result;
}

static int load_font_map(opencgm_ctx_t* ctx)
{
    if (!ctx)
    {
        set_error("Invalid context");
        return OPENCGM_ERR_INVALID_ARG;
    }

    ctx->font_map = FontMapData{};

    if (ctx->font_map_path.empty())
    {
        return OPENCGM_OK;
    }

    std::ifstream input(ctx->font_map_path);
    if (!input.is_open())
    {
        set_error("Failed to open font map: " + ctx->font_map_path);
        return OPENCGM_ERR_IO;
    }

    json doc;
    try
    {
        input >> doc;
    }
    catch (const std::exception &e)
    {
        set_error(std::string("Font map parse error: ") + e.what());
        return OPENCGM_ERR_PARSE;
    }

    FontMapData data;

    if (doc.contains("defaults") && doc["defaults"].is_object())
    {
        const json &defaults = doc["defaults"];
        if (defaults.contains("fallback_stack"))
        {
            data.fallback_stack = as_string_array(defaults["fallback_stack"]);
        }
        if (defaults.contains("text_as_path_threshold") && defaults["text_as_path_threshold"].is_number())
        {
            data.default_text_path_threshold = defaults["text_as_path_threshold"].get<double>();
        }
    }

    if (doc.contains("families") && doc["families"].is_object())
    {
        const json &families = doc["families"];
        for (auto it = families.begin(); it != families.end(); ++it)
        {
            if (!it.value().is_object())
            {
                continue;
            }
            std::string canonical = it.key();
            const json &entry = it.value();
            if (entry.contains("web") && entry["web"].is_string())
            {
                data.family_web[canonical] = entry["web"].get<std::string>();
            }
            if (entry.contains("embed") && entry["embed"].is_string())
            {
                std::filesystem::path embedPath = entry["embed"].get<std::string>();
                if (embedPath.is_relative())
                {
                    std::filesystem::path base = std::filesystem::path(ctx->font_map_path).parent_path();
                    embedPath = (base / embedPath).lexically_normal();
                }
                data.family_embed[canonical] = embedPath.string();
            }
        }
    }

    if (doc.contains("substitutions") && doc["substitutions"].is_array())
    {
        for (const auto &sub : doc["substitutions"])
        {
            if (!sub.is_object())
            {
                continue;
            }
            if (sub.contains("match") && sub.contains("use") &&
                sub["match"].is_string() && sub["use"].is_string())
            {
                data.substitutions.emplace_back(sub["match"].get<std::string>(), sub["use"].get<std::string>());
            }
        }
    }

    if (doc.contains("overrides") && doc["overrides"].is_object())
    {
        const json &overrides = doc["overrides"];
        for (auto it = overrides.begin(); it != overrides.end(); ++it)
        {
            if (!it.value().is_object())
            {
                continue;
            }
            std::string key = to_lower_copy(it.key());
            constexpr std::string_view prefix = "profile:";
            if (key.rfind(prefix.data(), 0) != 0)
            {
                continue;
            }
            std::string profileName = key.substr(prefix.size());
            FontProfileOverride overrideData;
            const json &overrideJson = it.value();
            if (overrideJson.contains("fallback_stack"))
            {
                overrideData.fallback_stack = as_string_array(overrideJson["fallback_stack"]);
            }
            if (overrideJson.contains("text_as_path_threshold") && overrideJson["text_as_path_threshold"].is_number())
            {
                overrideData.text_path_threshold = overrideJson["text_as_path_threshold"].get<double>();
            }
            data.profile_overrides[profileName] = std::move(overrideData);
        }
    }

    data.loaded = true;
    ctx->font_map = std::move(data);
    set_error("");
    return OPENCGM_OK;
}

struct ConverterSetup {
    std::string effective_profile;
    bool adopt_view_on_load = false;
};

static ConverterSetup configure_svg_converter(opencgm_ctx_t* ctx,
                                               opencgm::SVGConverter& converter) {
    if (ctx->xcf_merger.hasXcfData()) {
        converter.setXcfMerger(&ctx->xcf_merger);
    }

    converter.setTransparentCellColourEnabled(!ctx->disable_tcc);
    converter.setEmbedApsMetadataJson(ctx->embed_aps_metadata_json);
    converter.setRasterLoggingEnabled(ctx->raster_logging);
    converter.setGeometryLoggingEnabled(ctx->geometry_logging);
    converter.setColorLoggingEnabled(ctx->color_logging);
    converter.setWidthLoggingEnabled(ctx->width_logging);
    converter.setPngQuantizationEnabled(ctx->quantize_png);
    converter.setViewboxPaddingFraction(ctx->viewbox_padding);
    converter.setNurbsToleranceSvgUnits(ctx->nurbs_tolerance_svg_units);
    converter.setTargetDpi(ctx->dpi);
    converter.setRasterEncoding(
        static_cast<opencgm::SVGConverter::RasterEncoding>(ctx->raster_encoding));
    converter.setJpegQuality(ctx->jpeg_quality);
    converter.setJpeg444Subsampling(ctx->jpeg_444_subsampling);

    if (ctx->palette_override_mode != 0) {
        opencgm::SVGConverter::PaletteOverride palette_override;
        palette_override.mode =
            static_cast<opencgm::SVGConverter::PaletteOverride::Mode>(
                ctx->palette_override_mode);
        converter.setPaletteOverride(palette_override);
    }

    ConverterSetup setup;
    setup.effective_profile = "s1000d";
    setup.adopt_view_on_load = ctx->adopt_view_on_load;

    if (ctx->output_profile_set) {
        converter.setOutputProfile(ctx->output_profile);
        converter.setOutputTargetConfig(ctx->output_target_config);

        const std::string input_profile =
            ctx->profile_set ? to_lower_copy(ctx->profile) : "";
        if (input_profile == "compat") {
            converter.setSegmentPolicy(
                opencgm::SVGConverter::SegmentPolicy::AllowSegments);
            converter.setCompatibilityMode(true);
        } else {
            converter.setSegmentPolicy(
                opencgm::SVGConverter::SegmentPolicy::StrictWebCgm);
            converter.setCompatibilityMode(false);
        }

        switch (ctx->output_profile) {
            case opencgm::SVGConverter::OutputProfile::S1000DLegacy:
                setup.effective_profile = "s1000d-legacy";
                break;
            case opencgm::SVGConverter::OutputProfile::S1000D:
                setup.effective_profile = "s1000d";
                break;
            case opencgm::SVGConverter::OutputProfile::ATA2200:
                setup.effective_profile = "ata";
                break;
            case opencgm::SVGConverter::OutputProfile::StandardSVG:
                setup.effective_profile = "standard";
                break;
            case opencgm::SVGConverter::OutputProfile::Custom:
                setup.effective_profile = "custom";
                break;
            case opencgm::SVGConverter::OutputProfile::WebCGM21:
            default:
                setup.effective_profile = "webcgm";
                break;
        }

        if (!ctx->adopt_view_on_load_set) {
            setup.adopt_view_on_load =
                ctx->output_profile !=
                opencgm::SVGConverter::OutputProfile::WebCGM21;
        }
    } else {
        setup.effective_profile =
            to_lower_copy(ctx->profile_set ? ctx->profile : "s1000d");
        if (setup.effective_profile == "strict") {
            setup.effective_profile = "webcgm";
        }

        converter.setRequestedProfileLabel(setup.effective_profile);

        if (!ctx->adopt_view_on_load_set) {
            setup.adopt_view_on_load =
                setup.effective_profile == "compat";
        }

        if (setup.effective_profile == "webcgm") {
            converter.setOutputProfile(
                opencgm::SVGConverter::OutputProfile::WebCGM21);
            converter.setSegmentPolicy(
                opencgm::SVGConverter::SegmentPolicy::StrictWebCgm);
            converter.setCompatibilityMode(false);
            converter.setAttributeOutputFormat(
                opencgm::svg::AttributeManager::OutputFormat::WebCGM);
        } else if (setup.effective_profile == "compat") {
            converter.setOutputProfile(
                opencgm::SVGConverter::OutputProfile::S1000D);
            converter.setSegmentPolicy(
                opencgm::SVGConverter::SegmentPolicy::AllowSegments);
            converter.setCompatibilityMode(true);
            converter.setAttributeOutputFormat(
                opencgm::svg::AttributeManager::OutputFormat::WebCGM);
        } else if (setup.effective_profile == "s1000d") {
            converter.setOutputProfile(
                opencgm::SVGConverter::OutputProfile::S1000D);
            converter.setSegmentPolicy(
                opencgm::SVGConverter::SegmentPolicy::StrictWebCgm);
            converter.setCompatibilityMode(false);
            converter.setAttributeOutputFormat(
                opencgm::svg::AttributeManager::OutputFormat::S1000D_Issue6);
        } else if (
            setup.effective_profile == "s1000d-legacy" ||
            setup.effective_profile == "s1000dlegacy" ||
            setup.effective_profile == "s1000d-pre-v6" ||
            setup.effective_profile == "s1000dprev6") {
            converter.setOutputProfile(
                opencgm::SVGConverter::OutputProfile::S1000DLegacy);
            converter.setSegmentPolicy(
                opencgm::SVGConverter::SegmentPolicy::StrictWebCgm);
            converter.setCompatibilityMode(false);
            converter.setAttributeOutputFormat(
                opencgm::svg::AttributeManager::OutputFormat::S1000D_Legacy);
        } else if (setup.effective_profile == "ata") {
            converter.setOutputProfile(
                opencgm::SVGConverter::OutputProfile::ATA2200);
            converter.setSegmentPolicy(
                opencgm::SVGConverter::SegmentPolicy::StrictWebCgm);
            converter.setCompatibilityMode(false);
            converter.setAttributeOutputFormat(
                opencgm::svg::AttributeManager::OutputFormat::S1000D_Issue6);
        } else if (setup.effective_profile == "cgmplus") {
            converter.setOutputProfile(
                opencgm::SVGConverter::OutputProfile::WebCGM21);
            converter.setSegmentPolicy(
                opencgm::SVGConverter::SegmentPolicy::StrictWebCgm);
            converter.setCompatibilityMode(true);
            converter.setAttributeOutputFormat(
                opencgm::svg::AttributeManager::OutputFormat::S1000D_Issue6);
        } else if (setup.effective_profile == "pip") {
            converter.setOutputProfile(
                opencgm::SVGConverter::OutputProfile::WebCGM21);
            converter.setSegmentPolicy(
                opencgm::SVGConverter::SegmentPolicy::AllowSegments);
            converter.setCompatibilityMode(true);
            converter.setAttributeOutputFormat(
                opencgm::svg::AttributeManager::OutputFormat::WebCGM);
        } else if (
            setup.effective_profile == "cals" ||
            setup.effective_profile == "mil-prf-28003" ||
            setup.effective_profile == "mil-d-28003") {
            // CALS: MIL-D-28003A sources are CGM v1 binary (predate WebCGM
            // strictness entirely); MIL-PRF-28003B-era sources may carry
            // WebCGM-style APS. Lenient acceptance, WebCGM attribute dialect
            // for whatever APS metadata exists.
            converter.setOutputProfile(
                opencgm::SVGConverter::OutputProfile::WebCGM21);
            converter.setSegmentPolicy(
                opencgm::SVGConverter::SegmentPolicy::AllowSegments);
            converter.setCompatibilityMode(true);
            converter.setAttributeOutputFormat(
                opencgm::svg::AttributeManager::OutputFormat::WebCGM);
        } else {
            converter.setOutputProfile(
                opencgm::SVGConverter::OutputProfile::S1000D);
            converter.setSegmentPolicy(
                opencgm::SVGConverter::SegmentPolicy::AllowSegments);
            converter.setCompatibilityMode(true);
            converter.setAttributeOutputFormat(
                opencgm::svg::AttributeManager::OutputFormat::WebCGM);
        }

        // setOutputProfile resets its preset; caller overrides must win.
        converter.setOutputTargetConfig(ctx->output_target_config);
    }

    if (ctx->output_format_set) {
        converter.setAttributeOutputFormat(ctx->attribute_output_format);
    }
    converter.setAdoptViewOnLoad(setup.adopt_view_on_load);
    auto conversionPlan = converter.getConversionPlan();
    conversionPlan.hotspot = ctx->hotspot_config;
    converter.setConversionPlan(conversionPlan);

    opencgm::SVGConverter::TextRenderOptions text_options;
    if (!ctx->font_map.fallback_stack.empty()) {
        text_options.font_fallback_stack = ctx->font_map.fallback_stack;
    }

    if (auto it =
            ctx->font_map.profile_overrides.find(setup.effective_profile);
        it != ctx->font_map.profile_overrides.end()) {
        if (!it->second.fallback_stack.empty()) {
            text_options.font_fallback_stack = it->second.fallback_stack;
        }
        if (it->second.text_path_threshold) {
            text_options.text_as_path_threshold =
                *(it->second.text_path_threshold);
        }
    }

    if (ctx->font_map.default_text_path_threshold &&
        text_options.text_as_path_threshold == 0.0) {
        text_options.text_as_path_threshold =
            *(ctx->font_map.default_text_path_threshold);
    }
    if (ctx->text_path_threshold_set) {
        text_options.text_as_path_threshold = ctx->text_path_threshold;
    }

    text_options.text_as_path = ctx->text_as_path;
    text_options.font_overrides = ctx->font_map.family_web;
    text_options.font_file_paths = ctx->font_map.family_embed;
    text_options.font_substitutions = ctx->font_map.substitutions;
    converter.setTextRenderOptions(text_options);

    opencgm::SVGConverter::ViewerShimMode shim_mode =
        opencgm::SVGConverter::ViewerShimMode::Auto;
    if (ctx->shim_mode == OPENCGM_SHIM_ON) {
        shim_mode = opencgm::SVGConverter::ViewerShimMode::Always;
    } else if (ctx->shim_mode == OPENCGM_SHIM_OFF) {
        shim_mode = opencgm::SVGConverter::ViewerShimMode::Never;
    }
    converter.setViewerShimMode(shim_mode);
    converter.setViewerShimUrl(ctx->shim_url);

    return setup;
}

extern "C" {

OPENCGM_API opencgm_ctx_t* opencgm_create(void) {
    try {
        return new opencgm_ctx_t();
    } catch (...) {
        set_error("Failed to create context");
        return nullptr;
    }
}

OPENCGM_API void opencgm_destroy(opencgm_ctx_t* ctx) {
    delete ctx;
}

OPENCGM_API void opencgm_set_yflip_mode(opencgm_ctx_t* ctx, int mode) {
    (void)ctx;
    (void)mode;
}

OPENCGM_API void opencgm_set_fit_to_content(opencgm_ctx_t* ctx, int fit) {
    (void)ctx;
    (void)fit;
}

OPENCGM_API void opencgm_set_scale(opencgm_ctx_t* ctx, double scale) {
    (void)ctx;
    (void)scale;
}

OPENCGM_API void opencgm_set_dpi(opencgm_ctx_t* ctx, int dpi) {
    if (ctx) {
        // Validate DPI to prevent division by zero or invalid calculations
        if (dpi <= 0) {
            dpi = 96;  // Default to standard screen DPI
        }
        ctx->dpi = dpi;
    }
}

OPENCGM_API int opencgm_set_font_map(opencgm_ctx_t* ctx, const char* json_path) {
    if (!ctx || !json_path) {
        set_error("Invalid argument");
        return OPENCGM_ERR_INVALID_ARG;
    }
    if (!validate_file_path(json_path)) {
        set_error("Invalid file path");
        return OPENCGM_ERR_INVALID_ARG;
    }

    ctx->font_map_path = json_path;
    ctx->font_map.loaded = false;
    int status = load_font_map(ctx);
    if (status == OPENCGM_OK) {
        set_error("");
    }
    return status;
}

OPENCGM_API void opencgm_set_verbose(opencgm_ctx_t* ctx, int verbose) {
    if (ctx) {
        ctx->verbose = (verbose != 0);
    }
}

OPENCGM_API void opencgm_set_quiet(opencgm_ctx_t* ctx, int quiet) {
    if (ctx) {
        ctx->quiet = (quiet != 0);
    }
}

OPENCGM_API void opencgm_set_trace_unknown(opencgm_ctx_t* ctx, int trace) {
    if (ctx) {
        ctx->trace_unknown = (trace != 0);
    }
}

OPENCGM_API void opencgm_set_fail_on_warn(opencgm_ctx_t* ctx, int fail) {
    if (ctx) {
        ctx->fail_on_warn = (fail != 0);
    }
}

OPENCGM_API int opencgm_set_log_file(opencgm_ctx_t* ctx, const char* log_path) {
    if (!ctx || !log_path) {
        set_error("Invalid argument");
        return OPENCGM_ERR_INVALID_ARG;
    }
    if (!validate_file_path(log_path)) {
        set_error("Invalid file path");
        return OPENCGM_ERR_INVALID_ARG;
    }

    ctx->log_file_path = log_path;
    ctx->log_stream.open(log_path, std::ios::out | std::ios::trunc);
    if (!ctx->log_stream.is_open()) {
        set_error("Failed to open log file");
        return OPENCGM_ERR_IO;
    }

    return OPENCGM_OK;
}

OPENCGM_API void opencgm_set_style_mode(opencgm_ctx_t* ctx, int mode) {
    // Retained for ABI compatibility with older clients. CSS-class emission
    // was never implemented; profile-compatible presentation attributes are
    // emitted consistently instead.
    (void)ctx;
    (void)mode;
}

OPENCGM_API void opencgm_set_minify(opencgm_ctx_t* ctx, int minify) {
    if (ctx) {
        ctx->minify = (minify != 0);
    }
}

OPENCGM_API void opencgm_set_optimize_paths(opencgm_ctx_t* ctx, int optimize) {
    if (ctx) {
        ctx->optimize_paths = (optimize != 0);
    }
}

OPENCGM_API void opencgm_set_pretty_print(opencgm_ctx_t* ctx, int pretty) {
    if (ctx) {
        ctx->pretty = (pretty != 0);
    }
}

OPENCGM_API void opencgm_set_precision(opencgm_ctx_t* ctx, double precision) {
    if (ctx) {
        ctx->precision = precision;
    }
}

OPENCGM_API void opencgm_set_profile(opencgm_ctx_t* ctx, const char* profile) {
    if (!ctx || !profile) {
        return;
    }
    ctx->profile = to_lower_copy(profile);
    ctx->profile_set = true;
}

OPENCGM_API void opencgm_set_adopt_view_on_load(opencgm_ctx_t* ctx, int adopt) {
    if (!ctx) {
        return;
    }
    ctx->adopt_view_on_load = (adopt != 0);
    ctx->adopt_view_on_load_set = true;
}

OPENCGM_API void opencgm_set_text_as_path(opencgm_ctx_t* ctx, int enable) {
    if (!ctx) {
        return;
    }
    ctx->text_as_path = (enable != 0);
    ctx->text_as_path_set = true;
}

OPENCGM_API void opencgm_set_text_path_threshold(opencgm_ctx_t* ctx, double threshold) {
    if (!ctx) {
        return;
    }
    if (threshold < 0.0) {
        threshold = 0.0;
    }
    ctx->text_path_threshold = threshold;
    ctx->text_path_threshold_set = true;
}

OPENCGM_API void opencgm_set_tcc_enabled(opencgm_ctx_t* ctx, int enabled) {
    if (!ctx) {
        return;
    }
    ctx->disable_tcc = (enabled == 0);
}

OPENCGM_API void opencgm_set_raster_logging(opencgm_ctx_t* ctx, int enabled) {
    if (!ctx) {
        return;
    }
    ctx->raster_logging = (enabled != 0);
}

OPENCGM_API void opencgm_set_geometry_logging(opencgm_ctx_t* ctx, int enabled) {
    if (!ctx) {
        return;
    }
    ctx->geometry_logging = (enabled != 0);
}

OPENCGM_API void opencgm_set_color_logging(opencgm_ctx_t* ctx, int enabled) {
    if (!ctx) {
        return;
    }
    ctx->color_logging = (enabled != 0);
}

OPENCGM_API void opencgm_set_width_logging(opencgm_ctx_t* ctx, int enabled) {
    if (!ctx) {
        return;
    }
    ctx->width_logging = (enabled != 0);
}

OPENCGM_API void opencgm_set_nurbs_tolerance(opencgm_ctx_t* ctx, double svg_units) {
    if (!ctx) return;
    ctx->nurbs_tolerance_svg_units = svg_units;
}

OPENCGM_API void opencgm_set_viewbox_padding(opencgm_ctx_t* ctx, double fraction) {
    if (!ctx) {
        return;
    }
    ctx->viewbox_padding = fraction;
}

OPENCGM_API void opencgm_set_palette_override_mode(opencgm_ctx_t* ctx, int mode) {
    if (!ctx) {
        return;
    }
    ctx->palette_override_mode = mode;
}

OPENCGM_API void opencgm_set_quantize_png(opencgm_ctx_t* ctx, int enabled) {
    if (!ctx) {
        return;
    }
    ctx->quantize_png = (enabled != 0);
}

OPENCGM_API void opencgm_set_geometry_validation(opencgm_ctx_t* ctx, int enabled) {
    if (!ctx) {
        return;
    }
    ctx->geometry_validate = (enabled != 0);
}

OPENCGM_API void opencgm_set_geometry_tolerance(opencgm_ctx_t* ctx, double tolerance) {
    if (!ctx) {
        return;
    }
    if (!std::isfinite(tolerance) || tolerance < 0.0) {
        ctx->geometry_tolerance = 0.01;
        return;
    }
    ctx->geometry_tolerance = tolerance;
}

OPENCGM_API void opencgm_set_embed_shim(opencgm_ctx_t* ctx, int mode) {
    if (!ctx) {
        return;
    }
    switch (mode) {
        case OPENCGM_SHIM_ON:
        case OPENCGM_SHIM_OFF:
            ctx->shim_mode = mode;
            break;
        default:
            ctx->shim_mode = OPENCGM_SHIM_AUTO;
            break;
    }
}

OPENCGM_API void opencgm_set_shim_url(opencgm_ctx_t* ctx, const char* url) {
    if (!ctx) {
        return;
    }
    ctx->shim_url = url ? url : "";
}

OPENCGM_API void opencgm_set_hotspot_profile(opencgm_ctx_t* ctx, int profile) {
    if (!ctx) {
        return;
    }

    switch (profile) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
            ctx->hotspot_config = opencgm::HotspotProfileConfig::fromProfile(
                static_cast<opencgm::HotspotProfile>(profile));
            break;
        default:
            ctx->hotspot_config = opencgm::HotspotProfileConfig::fromProfile(
                opencgm::HotspotProfile::Generic);
            break;
    }
}

OPENCGM_API void opencgm_add_custom_attribute(opencgm_ctx_t* ctx, const char* key, const char* value, int scope, const char* selector) {
    if (!ctx || !key || !value || !selector) {
        return;
    }
    if (scope < static_cast<int>(opencgm::CustomAttributeScope::All) ||
        scope > static_cast<int>(opencgm::CustomAttributeScope::Layer)) {
        return;
    }

    opencgm::CustomAttributeRule rule;
    rule.key = key;
    rule.value = value;
    rule.scope = static_cast<opencgm::CustomAttributeScope>(scope);
    rule.selector = selector;

    ctx->hotspot_config.custom_attributes.push_back(rule);
}

// ============================================================================
// Output Target Configuration (Two-Tier Profile Model)
// ============================================================================

OPENCGM_API void opencgm_set_output_profile(opencgm_ctx_t* ctx, int profile) {
    if (!ctx) return;

    ctx->output_profile_set = true;

    switch (profile) {
        case OPENCGM_OUTPUT_WEBCGM21:
            ctx->output_profile = opencgm::SVGConverter::OutputProfile::WebCGM21;
            ctx->output_target_config = opencgm::OutputTargetConfig::forS1000DIETP();
            break;
        case OPENCGM_OUTPUT_S1000D:
            ctx->output_profile = opencgm::SVGConverter::OutputProfile::S1000D;
            ctx->output_target_config = opencgm::OutputTargetConfig::forS1000DIETP();
            break;
        case OPENCGM_OUTPUT_S1000D_LEGACY:
            ctx->output_profile = opencgm::SVGConverter::OutputProfile::S1000DLegacy;
            ctx->output_target_config = opencgm::OutputTargetConfig::forS1000DIETP();
            break;
        case OPENCGM_OUTPUT_ATA2200:
            ctx->output_profile = opencgm::SVGConverter::OutputProfile::ATA2200;
            ctx->output_target_config = opencgm::OutputTargetConfig::forATAIETM();
            break;
        case OPENCGM_OUTPUT_STANDARD_SVG:
            ctx->output_profile = opencgm::SVGConverter::OutputProfile::StandardSVG;
            ctx->output_target_config = opencgm::OutputTargetConfig::forStandardSVG();
            break;
        case OPENCGM_OUTPUT_CUSTOM:
            ctx->output_profile = opencgm::SVGConverter::OutputProfile::Custom;
            // Keep existing output_target_config (user will configure via sub-options)
            break;
        default:
            ctx->output_profile = opencgm::SVGConverter::OutputProfile::WebCGM21;
            ctx->output_target_config = opencgm::OutputTargetConfig::forS1000DIETP();
            break;
    }
}

OPENCGM_API int opencgm_get_output_profile(opencgm_ctx_t* ctx) {
    if (!ctx) return OPENCGM_OUTPUT_WEBCGM21;
    return static_cast<int>(ctx->output_profile);
}

OPENCGM_API void opencgm_set_hotspot_encoding(opencgm_ctx_t* ctx, int mode) {
    if (!ctx) return;
    switch (mode) {
        case OPENCGM_HOTSPOT_SVG_ANCHOR_TITLE:
            ctx->output_target_config.hotspot_encoding = opencgm::HotspotEncodingMode::SvgAnchorTitle;
            break;
        case OPENCGM_HOTSPOT_DATA_ATTRIBUTES:
            ctx->output_target_config.hotspot_encoding = opencgm::HotspotEncodingMode::DataAttributes;
            break;
        case OPENCGM_HOTSPOT_BOTH:
        default:
            ctx->output_target_config.hotspot_encoding = opencgm::HotspotEncodingMode::Both;
            break;
    }
}

OPENCGM_API void opencgm_set_region_handling(opencgm_ctx_t* ctx, int mode) {
    if (!ctx) return;
    switch (mode) {
        case OPENCGM_REGION_OVERLAY_ONLY:
            ctx->output_target_config.region_handling = opencgm::RegionHandlingMode::OverlayOnly;
            break;
        case OPENCGM_REGION_BBOX_ONLY:
            ctx->output_target_config.region_handling = opencgm::RegionHandlingMode::BboxOnly;
            break;
        case OPENCGM_REGION_BOTH:
        default:
            ctx->output_target_config.region_handling = opencgm::RegionHandlingMode::Both;
            break;
    }
}

OPENCGM_API void opencgm_set_multi_link_mode(opencgm_ctx_t* ctx, int mode) {
    if (!ctx) return;
    switch (mode) {
        case OPENCGM_MULTILINK_FIRST_ONLY:
            ctx->output_target_config.multi_link_mode = opencgm::MultiLinkMode::FirstLinkOnly;
            break;
        case OPENCGM_MULTILINK_JSON_DATA_ATTR:
            ctx->output_target_config.multi_link_mode = opencgm::MultiLinkMode::JsonDataAttribute;
            break;
        case OPENCGM_MULTILINK_JS_HANDLER:
            ctx->output_target_config.multi_link_mode = opencgm::MultiLinkMode::JsEventHandler;
            break;
        default:
            ctx->output_target_config.multi_link_mode = opencgm::MultiLinkMode::JsonDataAttribute;
            break;
    }
}

OPENCGM_API void opencgm_set_emit_data_aps_type(opencgm_ctx_t* ctx, int enabled) {
    if (!ctx) return;
    ctx->output_target_config.emit_data_aps_type = (enabled != 0);
}

OPENCGM_API void opencgm_set_emit_data_name(opencgm_ctx_t* ctx, int enabled) {
    if (!ctx) return;
    ctx->output_target_config.emit_data_name = (enabled != 0);
}

OPENCGM_API void opencgm_set_emit_data_content(opencgm_ctx_t* ctx, int enabled) {
    if (!ctx) return;
    ctx->output_target_config.emit_data_content = (enabled != 0);
}

OPENCGM_API void opencgm_set_emit_data_viewcontext(opencgm_ctx_t* ctx, int enabled) {
    if (!ctx) return;
    ctx->output_target_config.emit_data_viewcontext = (enabled != 0);
}

OPENCGM_API void opencgm_set_preserve_layer_hierarchy(opencgm_ctx_t* ctx, int enabled) {
    if (!ctx) return;
    ctx->output_target_config.preserve_layer_hierarchy = (enabled != 0);
}

OPENCGM_API void opencgm_set_preserve_aps_id(opencgm_ctx_t* ctx, int enabled) {
    if (!ctx) return;
    ctx->output_target_config.preserve_aps_id = (enabled != 0);
}

OPENCGM_API void opencgm_set_preserve_aps_link_title(opencgm_ctx_t* ctx, int enabled) {
    if (!ctx) return;
    ctx->output_target_config.preserve_aps_link_title = (enabled != 0);
}

OPENCGM_API void opencgm_set_preserve_aps_region(opencgm_ctx_t* ctx, int enabled) {
    if (!ctx) return;
    ctx->output_target_config.preserve_aps_region = (enabled != 0);
}

OPENCGM_API void opencgm_set_preserve_aps_screen_tip(opencgm_ctx_t* ctx, int enabled) {
    if (!ctx) return;
    ctx->output_target_config.preserve_aps_screen_tip = (enabled != 0);
}

OPENCGM_API void opencgm_set_webcgm_namespace(opencgm_ctx_t* ctx, int enabled) {
    if (!ctx) return;
    ctx->output_target_config.emit_webcgm_namespace = (enabled != 0);
}

OPENCGM_API void opencgm_set_validate_output_s1000d6(opencgm_ctx_t* ctx, int enabled) {
    // Deprecated no-op, retained for ABI stability. The flag was stored but
    // never consumed by any conversion or validation path; the corresponding
    // app setting has been removed. A real post-conversion SVG conformance
    // checker is tracked separately (see plan item P2.6).
    (void)ctx;
    (void)enabled;
}

OPENCGM_API void opencgm_set_embed_aps_metadata_json(opencgm_ctx_t* ctx, int enabled) {
    if (!ctx) return;
    ctx->embed_aps_metadata_json = (enabled != 0);
}

OPENCGM_API void opencgm_set_compress_output(opencgm_ctx_t* ctx, int enabled) {
    if (!ctx) return;
    ctx->compress_output = (enabled != 0);
}

OPENCGM_API void opencgm_set_raster_encoding(opencgm_ctx_t* ctx, int mode) {
    if (!ctx) return;
    // 0=Auto, 1=PNG, 2=JPEG
    ctx->raster_encoding = (mode >= 0 && mode <= 2) ? mode : 0;
}

OPENCGM_API int opencgm_get_raster_encoding(opencgm_ctx_t* ctx) {
    if (!ctx) return 0;
    return ctx->raster_encoding;
}

OPENCGM_API void opencgm_set_jpeg_quality(opencgm_ctx_t* ctx, int quality) {
    if (!ctx) return;
    ctx->jpeg_quality = (quality < 1) ? 1 : ((quality > 100) ? 100 : quality);
}

OPENCGM_API int opencgm_get_jpeg_quality(opencgm_ctx_t* ctx) {
    if (!ctx) return 85;
    return ctx->jpeg_quality;
}

OPENCGM_API void opencgm_set_jpeg_444_subsampling(opencgm_ctx_t* ctx, int enabled) {
    if (!ctx) return;
    ctx->jpeg_444_subsampling = (enabled != 0);
}

OPENCGM_API int opencgm_get_jpeg_444_subsampling(opencgm_ctx_t* ctx) {
    if (!ctx) return 1;
    return ctx->jpeg_444_subsampling ? 1 : 0;
}


OPENCGM_API int opencgm_convert_cgm_to_svg(opencgm_ctx_t* ctx,
                                            const char* input_path,
                                            const char* output_svg_path) {
    if (!ctx || !input_path || !output_svg_path) {
        set_error("Invalid argument");
        return OPENCGM_ERR_INVALID_ARG;
    }

    try {
        ctx->last_document.reset();
        ctx->last_report.reset();
        ctx->last_report_json.clear();
        ctx->last_report_text.clear();

        // Load CGM file
        auto cgm_file = std::make_unique<opencgm::BinaryCGMFile>(input_path);
        auto document = opencgm::ConversionDocumentBuilder::fromCgm(*cgm_file, input_path);

        if (ctx->verbose && !ctx->quiet) {
            if (ctx->log_stream.is_open()) {
                ctx->log_stream << "Loaded CGM: " << input_path << std::endl;
                ctx->log_stream << "Commands: " << cgm_file->commands().size() << std::endl;
            }
        }

        if (!ctx->font_map_path.empty() && !ctx->font_map.loaded) {
            int status = load_font_map(ctx);
            if (status != OPENCGM_OK) {
                std::vector<opencgm::ConversionIssue> issues;
                issues.push_back(make_runtime_issue(
                    opencgm::ValidationSeverity::ERROR,
                    opencgm::PreservationDisposition::Dropped,
                    "configuration",
                    std::string("Failed to load font map: ") + ctx->font_map_path));
                store_conversion_report(ctx, std::move(document), output_svg_path, false, ctx->compress_output, ctx->minify, ctx->optimize_paths, std::nullopt, std::move(issues));
                return status;
            }
        }

        // Create SVG converter
        opencgm::SVGConverter converter(cgm_file.get());
        const ConverterSetup converter_setup =
            configure_svg_converter(ctx, converter);

        const auto picture_ranges = cgm_file->getPictureRanges();
        if (picture_ranges.size() > 1 && ctx->multi_picture_mode == 1) {
            set_error("Separate-file mode must use opencgm_convert_picture_to_svg for each picture");
            return OPENCGM_ERR_INVALID_ARG;
        }
        if (picture_ranges.size() > 1 && ctx->multi_picture_mode == 2) {
            set_error("Combined layered multi-picture output is not implemented");
            return OPENCGM_ERR_INVALID_ARG;
        }

        // The default contract is first-picture-only. Passing -1 for files without
        // picture delimiters retains compatibility with malformed/simple inputs.
        const int picture_index = picture_ranges.empty() ? -1 : 0;
        std::string svg_content = converter.convert(picture_index);
        svg_content = normalize_svg_output(std::move(svg_content), ctx->minify, ctx->optimize_paths, ctx->precision);
        std::optional<opencgm::GeometryMetrics> geometryMetrics = converter.geometryMetrics();
        std::vector<opencgm::ConversionIssue> runtimeIssues;
        auto unknownIssues =
            make_unknown_command_issues(*cgm_file, ctx->trace_unknown);
        if (!unknownIssues.empty() && ctx->verbose && !ctx->quiet)
        {
            for (const auto& issue : unknownIssues)
            {
                std::fprintf(stderr, "[unknown] warning: %s\n", issue.message.c_str());
            }
        }
        runtimeIssues.insert(
            runtimeIssues.end(),
            std::make_move_iterator(unknownIssues.begin()),
            std::make_move_iterator(unknownIssues.end()));

        if (ctx->fail_on_warn && !runtimeIssues.empty())
        {
            set_error("Conversion rejected because unsupported CGM commands were encountered");
            store_conversion_report(
                ctx,
                std::move(document),
                output_svg_path,
                false,
                ctx->compress_output,
                ctx->minify,
                ctx->optimize_paths,
                geometryMetrics,
                std::move(runtimeIssues));
            return OPENCGM_ERR_GENERAL;
        }

        if (ctx->geometry_validate)
        {
            const auto &metrics = *geometryMetrics;
            GeometryValidationResult validation = validate_geometry_metrics(metrics,
                                                                            ctx->geometry_tolerance,
                                                                            converter_setup.effective_profile == "webcgm",
                                                                            converter_setup.adopt_view_on_load);
            auto geometryIssues = make_geometry_issues(validation);
            runtimeIssues.insert(runtimeIssues.end(),
                                 std::make_move_iterator(geometryIssues.begin()),
                                 std::make_move_iterator(geometryIssues.end()));

            if (!validation.warnings.empty() && !ctx->quiet)
            {
                for (const auto &warn : validation.warnings)
                {
                    std::fprintf(stderr, "[geometry] warning: %s\n", warn.c_str());
                }
            }

            bool fail = !validation.errors.empty();
            if (!fail && ctx->fail_on_warn && !validation.warnings.empty())
            {
                fail = true;
            }

            if (fail)
            {
                std::ostringstream oss;
                oss << "Geometry validation failed";
                if (!validation.errors.empty())
                {
                    for (const auto &err : validation.errors)
                    {
                        oss << "\n  - " << err;
                    }
                }
                else
                {
                    for (const auto &warn : validation.warnings)
                    {
                        oss << "\n  - " << warn;
                    }
                }
                set_error(oss.str());
                store_conversion_report(ctx, std::move(document), output_svg_path, false, ctx->compress_output, ctx->minify, ctx->optimize_paths, geometryMetrics, std::move(runtimeIssues));
                return OPENCGM_ERR_GENERAL;
            }
        }

        // Write to file (optionally compressed for SVGZ)
        if (ctx->compress_output) {
            // Compress SVG content using gzip for SVGZ output
            std::vector<uint8_t> compressed;
            if (!opencgm::gzipCompress(svg_content, compressed)) {
                set_error("Failed to compress SVG output");
                runtimeIssues.push_back(make_runtime_issue(
                    opencgm::ValidationSeverity::ERROR,
                    opencgm::PreservationDisposition::Dropped,
                    "io",
                    "Failed to compress SVG output"));
                store_conversion_report(ctx, std::move(document), output_svg_path, false, true, ctx->minify, ctx->optimize_paths, geometryMetrics, std::move(runtimeIssues));
                return OPENCGM_ERR_GENERAL;
            }

            std::ofstream out(output_svg_path, std::ios::out | std::ios::binary | std::ios::trunc);
            if (!out.is_open()) {
                set_error(std::string("Failed to open output file: ") + output_svg_path);
                runtimeIssues.push_back(make_runtime_issue(
                    opencgm::ValidationSeverity::ERROR,
                    opencgm::PreservationDisposition::Dropped,
                    "io",
                    std::string("Failed to open output file: ") + output_svg_path));
                store_conversion_report(ctx, std::move(document), output_svg_path, false, true, ctx->minify, ctx->optimize_paths, geometryMetrics, std::move(runtimeIssues));
                return OPENCGM_ERR_IO;
            }

            out.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
            out.close();

            if (ctx->verbose && !ctx->quiet) {
                if (ctx->log_stream.is_open()) {
                    ctx->log_stream << "Wrote SVGZ: " << output_svg_path
                                    << " (compressed from " << svg_content.size()
                                    << " to " << compressed.size() << " bytes)" << std::endl;
                }
            }
        } else {
            std::ofstream out(output_svg_path, std::ios::out | std::ios::trunc);
            if (!out.is_open()) {
                set_error(std::string("Failed to open output file: ") + output_svg_path);
                runtimeIssues.push_back(make_runtime_issue(
                    opencgm::ValidationSeverity::ERROR,
                    opencgm::PreservationDisposition::Dropped,
                    "io",
                    std::string("Failed to open output file: ") + output_svg_path));
                store_conversion_report(ctx, std::move(document), output_svg_path, false, false, ctx->minify, ctx->optimize_paths, geometryMetrics, std::move(runtimeIssues));
                return OPENCGM_ERR_IO;
            }

            out << svg_content;
            out.close();

            if (ctx->verbose && !ctx->quiet) {
                if (ctx->log_stream.is_open()) {
                    ctx->log_stream << "Wrote SVG: " << output_svg_path << std::endl;
                }
            }
        }

        if (ctx->generate_xcf) {
            std::filesystem::path xcf_path(output_svg_path);
            xcf_path.replace_extension(".xcf");
            const std::string svg_filename = std::filesystem::path(output_svg_path).filename().string();
            if (!write_xcf_output(ctx, cgm_file.get(), svg_filename, xcf_path.string())) {
                set_error(std::string("Failed to write XCF file: ") + xcf_path.string());
                runtimeIssues.push_back(make_runtime_issue(
                    opencgm::ValidationSeverity::ERROR,
                    opencgm::PreservationDisposition::Dropped,
                    "io",
                    std::string("Failed to write XCF file: ") + xcf_path.string()));
                store_conversion_report(ctx, std::move(document), output_svg_path, false, ctx->compress_output, ctx->minify, ctx->optimize_paths, geometryMetrics, std::move(runtimeIssues));
                return OPENCGM_ERR_IO;
            }
        }

        store_conversion_report(ctx, std::move(document), output_svg_path, true, ctx->compress_output, ctx->minify, ctx->optimize_paths, geometryMetrics, std::move(runtimeIssues));
        set_error(""); // Clear error
        return OPENCGM_OK;

    } catch (const std::exception& e) {
        set_error(std::string("Conversion failed: ") + e.what());
        return OPENCGM_ERR_PARSE;
    } catch (...) {
        set_error("Conversion failed: unknown error");
        return OPENCGM_ERR_GENERAL;
    }
}

OPENCGM_API int opencgm_detect_profile(opencgm_ctx_t* ctx,
                                        const char* input_path) {
    if (!ctx || !input_path) {
        set_error("Invalid argument");
        return OPENCGM_ERR_INVALID_ARG;
    }

    try {
        ctx->last_profile_detection_json.clear();

        auto cgm_file = std::make_unique<opencgm::BinaryCGMFile>(input_path);
        auto detection = opencgm::ProfileDetector::detectProfileWithMetadata(cgm_file.get());
        ctx->last_profile_detection_json = build_profile_detection_json(detection);

        set_error("");
        return OPENCGM_OK;
    } catch (const std::exception& e) {
        set_error(std::string("Profile detection failed: ") + e.what());
        return OPENCGM_ERR_PARSE;
    } catch (...) {
        set_error("Profile detection failed: unknown error");
        return OPENCGM_ERR_GENERAL;
    }
}

OPENCGM_API const char* opencgm_get_last_profile_detection_json(opencgm_ctx_t* ctx) {
    if (!ctx || ctx->last_profile_detection_json.empty()) {
        return "";
    }

    return ctx->last_profile_detection_json.c_str();
}

OPENCGM_API int opencgm_validate_profile(opencgm_ctx_t* ctx,
                                         const char* input_path,
                                         const char* requested_profile) {
    if (!ctx || !input_path || !requested_profile) {
        set_error("Invalid argument");
        return OPENCGM_ERR_INVALID_ARG;
    }

    try {
        ctx->last_validation_json.clear();

        auto cgm_file = std::make_unique<opencgm::BinaryCGMFile>(input_path);

        const std::string requested = requested_profile;
        const bool auto_detect = is_auto_profile_contract(requested);
        std::optional<opencgm::ProfileDetector::DetectionResult> detection;
        std::optional<opencgm::ProfileType> requested_type;

        if (auto_detect) {
            detection = opencgm::ProfileDetector::detectProfileWithMetadata(cgm_file.get());
            requested_type = detection->profile == opencgm::ProfileType::UNKNOWN
                ? opencgm::ProfileType::ISO_IEC_8632_COMPAT
                : detection->profile;
        } else {
            requested_type = try_parse_validation_profile(requested);
            if (!requested_type.has_value()) {
                set_error("Native validation does not support the requested profile");
                return OPENCGM_ERR_INVALID_ARG;
            }

            // GREXCHANGE editions span 2.4-2.12+. When the metafile declares
            // its ProfileEd, validate against the matching shipped rule set
            // (2.6-2.9) instead of assuming one frozen edition; editions
            // newer than 2.9 clamp to the 2.9 rules. Uses a local probe so
            // the payload's wasAutoDetected flag (keyed on `detection`)
            // stays false for pinned profiles.
            if (*requested_type == opencgm::ProfileType::ATA_GREXCHANGE_2_9) {
                const auto editionProbe =
                    opencgm::ProfileDetector::detectProfileWithMetadata(
                        cgm_file.get());
                const std::string& edition =
                    editionProbe.metadata.profileEdition;
                if (edition.rfind("2.6", 0) == 0) {
                    requested_type = opencgm::ProfileType::ATA_GREXCHANGE_2_6;
                } else if (edition.rfind("2.7", 0) == 0) {
                    requested_type = opencgm::ProfileType::ATA_GREXCHANGE_2_7;
                } else if (edition.rfind("2.8", 0) == 0) {
                    requested_type = opencgm::ProfileType::ATA_GREXCHANGE_2_8;
                }
                // else: 2.9, newer editions, or undeclared -> 2.9 rule set.
            }
        }

        auto validator = opencgm::ProfileDetector::createValidator(*requested_type);
        if (!validator) {
            set_error("Failed to create native profile validator");
            return OPENCGM_ERR_GENERAL;
        }

        auto messages = validator->validate(cgm_file.get());
        ctx->last_validation_json = build_validation_json(
            requested,
            *requested_type,
            messages,
            detection);

        set_error("");
        return OPENCGM_OK;
    } catch (const std::exception& e) {
        set_error(std::string("Profile validation failed: ") + e.what());
        return OPENCGM_ERR_PARSE;
    } catch (...) {
        set_error("Profile validation failed: unknown error");
        return OPENCGM_ERR_GENERAL;
    }
}

OPENCGM_API const char* opencgm_get_last_validation_json(opencgm_ctx_t* ctx) {
    if (!ctx || ctx->last_validation_json.empty()) {
        return "";
    }

    return ctx->last_validation_json.c_str();
}

OPENCGM_API const char* opencgm_get_builtin_profile_catalog_json(opencgm_ctx_t* ctx) {
    if (!ctx) {
        return "";
    }

    if (ctx->last_builtin_profile_catalog_json.empty()) {
        ctx->last_builtin_profile_catalog_json = load_builtin_profile_catalog_json();
    }

    return ctx->last_builtin_profile_catalog_json.c_str();
}

OPENCGM_API int opencgm_run_builtin_qa(opencgm_ctx_t* ctx, const char* svg_path) {
    if (!ctx || !svg_path) {
        set_error("Invalid argument");
        return -1;  // Error indicator
    }

    // Validate file path
    if (!validate_file_path(svg_path)) {
        set_error("Invalid file path");
        return -1;
    }

    int warnings = 0;

    try {
        // Rule 1: Check file exists and is readable
        std::ifstream file(svg_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            set_error("Cannot open SVG file for QA");
            return -1;
        }

        // Check file size to prevent memory exhaustion
        constexpr std::streamsize MAX_QA_FILE_SIZE = 100 * 1024 * 1024;  // 100MB
        std::streamsize file_size = file.tellg();
        if (file_size > MAX_QA_FILE_SIZE) {
            set_error("QA: File too large (max 100MB)");
            return -1;
        }
        file.seekg(0, std::ios::beg);

        std::string content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        file.close();

        // Rule 2: Check for valid SVG root element
        if (content.find("<svg") == std::string::npos) {
            set_error("QA: Missing <svg> root element");
            return -1;
        }

        // Rule 3: Check viewBox is defined
        if (content.find("viewBox") == std::string::npos) {
            warnings++;
            if (ctx->verbose) {
                std::cerr << "QA Warning: No viewBox attribute found\n";
            }
        }

        // Rule 4: Check for empty paths (d attribute with no content)
        size_t pos = 0;
        while ((pos = content.find("<path", pos)) != std::string::npos) {
            size_t end = content.find(">", pos);
            if (end != std::string::npos) {
                std::string pathTag = content.substr(pos, end - pos + 1);
                // Check for d="" or d='' (empty path data)
                if (pathTag.find("d=\"\"") != std::string::npos ||
                    pathTag.find("d=''") != std::string::npos) {
                    warnings++;
                    if (ctx->verbose) {
                        std::cerr << "QA Warning: Empty path data found\n";
                    }
                }
            }
            pos++;
        }

        // Rule 5: Check for extremely large dimensions
        auto extractNumber = [&content](const std::string& attr) -> double {
            size_t attrPos = content.find(attr + "=\"");
            if (attrPos == std::string::npos) {
                attrPos = content.find(attr + "='");
            }
            if (attrPos != std::string::npos) {
                size_t start = attrPos + attr.length() + 2;
                size_t end = content.find_first_of("\"'", start);
                if (end != std::string::npos) {
                    try {
                        return std::stod(content.substr(start, end - start));
                    } catch (...) {
                        return 0.0;
                    }
                }
            }
            return 0.0;
        };

        double width = extractNumber("width");
        double height = extractNumber("height");
        if (width > 100000 || height > 100000) {
            warnings++;
            if (ctx->verbose) {
                std::cerr << "QA Warning: Extremely large dimensions ("
                          << width << "x" << height << ")\n";
            }
        }

        // Rule 6: Check for degenerate transforms (zero scale)
        if (content.find("scale(0,") != std::string::npos ||
            content.find("scale(0)") != std::string::npos ||
            content.find("scale(0 ") != std::string::npos) {
            warnings++;
            if (ctx->verbose) {
                std::cerr << "QA Warning: Zero scale transform detected\n";
            }
        }

        // Rule 7: Check for NaN/Inf values
        if (content.find("NaN") != std::string::npos) {
            warnings++;
            if (ctx->verbose) {
                std::cerr << "QA Warning: NaN value detected in SVG\n";
            }
        }
        if (content.find("Infinity") != std::string::npos ||
            content.find("-Infinity") != std::string::npos) {
            warnings++;
            if (ctx->verbose) {
                std::cerr << "QA Warning: Infinity value detected in SVG\n";
            }
        }

        // Rule 8: Check for unclosed groups
        size_t groupOpens = 0, groupCloses = 0;
        pos = 0;
        while ((pos = content.find("<g", pos)) != std::string::npos) {
            // Make sure it's a group tag, not something like <gradient
            if (pos + 2 < content.length() &&
                (content[pos + 2] == ' ' || content[pos + 2] == '>' || content[pos + 2] == '/')) {
                groupOpens++;
            }
            pos++;
        }
        pos = 0;
        while ((pos = content.find("</g>", pos)) != std::string::npos) {
            groupCloses++;
            pos++;
        }
        if (groupOpens != groupCloses) {
            warnings++;
            if (ctx->verbose) {
                std::cerr << "QA Warning: Mismatched group tags (opens: "
                          << groupOpens << ", closes: " << groupCloses << ")\n";
            }
        }

        // Rule 9: Check for missing xmlns
        if (content.find("xmlns=\"http://www.w3.org/2000/svg\"") == std::string::npos &&
            content.find("xmlns='http://www.w3.org/2000/svg'") == std::string::npos) {
            warnings++;
            if (ctx->verbose) {
                std::cerr << "QA Warning: Missing SVG namespace declaration\n";
            }
        }

        // Rule 10: Check for negative dimensions
        if (width < 0 || height < 0) {
            warnings++;
            if (ctx->verbose) {
                std::cerr << "QA Warning: Negative dimensions detected\n";
            }
        }

        return warnings;
    } catch (const std::exception& e) {
        set_error(std::string("QA failed: ") + e.what());
        return -1;
    }
}

OPENCGM_API int opencgm_write_report(opencgm_ctx_t* ctx, const char* report_json_path) {
    if (!ctx || !report_json_path) {
        set_error("Invalid argument");
        return OPENCGM_ERR_INVALID_ARG;
    }

    if (!ctx->last_report) {
        set_error("No conversion report is available");
        return OPENCGM_ERR_GENERAL;
    }

    try {
        std::ofstream out(report_json_path);
        if (!out.is_open()) {
            set_error("Failed to open report file");
            return OPENCGM_ERR_IO;
        }

        out << (ctx->last_report_json.empty() ? ctx->last_report->generateJsonReport() : ctx->last_report_json);
        out.close();

        return OPENCGM_OK;
    } catch (...) {
        set_error("Failed to write report");
        return OPENCGM_ERR_IO;
    }
}

OPENCGM_API const char* opencgm_get_last_report_json(opencgm_ctx_t* ctx) {
    if (!ctx || ctx->last_report_json.empty()) {
        return "";
    }

    return ctx->last_report_json.c_str();
}

OPENCGM_API const char* opencgm_get_last_report_text(opencgm_ctx_t* ctx) {
    if (!ctx || ctx->last_report_text.empty()) {
        return "";
    }

    return ctx->last_report_text.c_str();
}

OPENCGM_API const char* opencgm_last_error(void) {
    // Return a static empty string for safety when no error is set
    // This ensures we never return a potentially dangling pointer
    static const char* const empty = "";
    if (g_last_error.empty()) {
        return empty;
    }
    return g_last_error.c_str();
}

OPENCGM_API int opencgm_trace_cgm(opencgm_ctx_t* ctx,
                                   const char* input_path,
                                   const char* output_json_path) {
    if (!ctx || !input_path || !output_json_path) {
        set_error("Invalid argument");
        return OPENCGM_ERR_INVALID_ARG;
    }
    if (!validate_file_path(output_json_path)) {
        set_error("Invalid output path");
        return OPENCGM_ERR_INVALID_ARG;
    }

    try {
        // Load CGM file
        auto cgm_file = std::make_unique<opencgm::BinaryCGMFile>(input_path);

        // Write commands as JSON trace
        std::ofstream out(output_json_path);
        if (!out.is_open()) {
            set_error("Failed to open trace file");
            return OPENCGM_ERR_IO;
        }

        out << "{\n";
        out << "  \"file\": " << json(std::string(input_path)).dump() << ",\n";
        out << "  \"commandCount\": " << cgm_file->commands().size() << ",\n";
        out << "  \"commands\": [\n";

        const auto& commands = cgm_file->commands();
        for (size_t i = 0; i < commands.size(); i++) {
            out << "    {\n";
            out << "      \"index\": " << i << ",\n";
            out << "      \"description\": " << json(commands[i]->toString()).dump() << "\n";
            out << "    }";
            if (i < commands.size() - 1) {
                out << ",";
            }
            out << "\n";
        }

        out << "  ]\n";
        out << "}\n";
        out.close();

        return OPENCGM_OK;
    } catch (const std::exception& e) {
        set_error(std::string("Trace failed: ") + e.what());
        return OPENCGM_ERR_PARSE;
    } catch (...) {
        set_error("Trace failed: unknown error");
        return OPENCGM_ERR_GENERAL;
    }
}

// Output format configuration
OPENCGM_API int opencgm_set_output_format(opencgm_ctx_t* ctx, const char* format) {
    if (!ctx || !format) {
        set_error("Invalid argument");
        return OPENCGM_ERR_INVALID_ARG;
    }

    std::string fmt = to_lower_copy(format);

    // Legacy compatibility: keep accepting legacy multipicture values.
    if (fmt == "separate" || fmt == "layered") {
        ctx->output_format = fmt;
        ctx->output_format_set = false;
        return OPENCGM_OK;
    }

    try {
        auto parsed = opencgm::svg::AttributeManager::parseOutputFormat(fmt);
        ctx->attribute_output_format = parsed;
        ctx->output_format = fmt;
        ctx->output_format_set = true;
        return OPENCGM_OK;
    } catch (const std::invalid_argument&) {
        set_error("Invalid output format. Use: legacy, s1000d6, combined, rws, boeing, r4i, or multi");
        return OPENCGM_ERR_INVALID_ARG;
    }
}

OPENCGM_API const char* opencgm_get_output_format(opencgm_ctx_t* ctx) {
    if (!ctx) {
        return "combined";  // Safe default
    }
    return ctx->output_format.c_str();
}

// Multi-picture support
OPENCGM_API int opencgm_get_picture_count(opencgm_ctx_t* ctx, const char* input_path) {
    if (!ctx || !input_path) {
        set_error("Invalid argument");
        return -1;
    }

    try {
        auto cgm_file = std::make_unique<opencgm::BinaryCGMFile>(input_path);
        // Count BEGIN PICTURE commands (class 0 = Delimiter, id 3 = BEGIN PICTURE)
        int count = 0;
        for (const auto& cmd : cgm_file->commands()) {
            if (cmd->elementClass() == opencgm::ClassCode::DelimiterElement && cmd->elementId() == 3) {
                count++;
            }
        }
        return count;
    } catch (...) {
        set_error("Failed to load CGM file");
        return -1;
    }
}

OPENCGM_API int opencgm_convert_picture_to_svg(opencgm_ctx_t* ctx,
                                                const char* input_path,
                                                const char* output_svg_path,
                                                int picture_index) {
    if (!ctx || !input_path || !output_svg_path) {
        set_error("Invalid argument");
        return OPENCGM_ERR_INVALID_ARG;
    }

    if (picture_index < 0) {
        set_error("Picture index must be non-negative");
        return OPENCGM_ERR_INVALID_ARG;
    }

    try {
        ctx->last_document.reset();
        ctx->last_report.reset();
        ctx->last_report_json.clear();
        ctx->last_report_text.clear();

        // Load CGM file
        auto cgm_file = std::make_unique<opencgm::BinaryCGMFile>(input_path);
        auto document = opencgm::ConversionDocumentBuilder::fromCgm(*cgm_file, input_path);

        // Validate picture index
        auto picture_ranges = cgm_file->getPictureRanges();
        if (picture_index >= static_cast<int>(picture_ranges.size())) {
            set_error("Picture index out of range");
            std::vector<opencgm::ConversionIssue> issues;
            issues.push_back(make_runtime_issue(
                opencgm::ValidationSeverity::ERROR,
                opencgm::PreservationDisposition::Dropped,
                "picture-selection",
                "Picture index out of range"));
            store_conversion_report(ctx, std::move(document), output_svg_path, false, ctx->compress_output, ctx->minify, ctx->optimize_paths, std::nullopt, std::move(issues));
            return OPENCGM_ERR_INVALID_ARG;
        }

        if (ctx->verbose && !ctx->quiet && ctx->log_stream.is_open()) {
            ctx->log_stream << "Loaded CGM: " << input_path << std::endl;
            ctx->log_stream << "Pictures: " << picture_ranges.size() << std::endl;
            ctx->log_stream << "Converting picture: " << picture_index << std::endl;
        }

        if (!ctx->font_map_path.empty() && !ctx->font_map.loaded) {
            int status = load_font_map(ctx);
            if (status != OPENCGM_OK) {
                std::vector<opencgm::ConversionIssue> issues;
                issues.push_back(make_runtime_issue(
                    opencgm::ValidationSeverity::ERROR,
                    opencgm::PreservationDisposition::Dropped,
                    "configuration",
                    std::string("Failed to load font map: ") + ctx->font_map_path));
                store_conversion_report(ctx, std::move(document), output_svg_path, false, ctx->compress_output, ctx->minify, ctx->optimize_paths, std::nullopt, std::move(issues));
                return status;
            }
        }

        // Create SVG converter with same configuration as main convert function
        opencgm::SVGConverter converter(cgm_file.get());
        configure_svg_converter(ctx, converter);

        // Convert specific picture to SVG
        std::string svg_content = converter.convert(picture_index);
        svg_content = normalize_svg_output(std::move(svg_content), ctx->minify, ctx->optimize_paths, ctx->precision);
        std::optional<opencgm::GeometryMetrics> geometryMetrics = converter.geometryMetrics();
        std::vector<opencgm::ConversionIssue> runtimeIssues;
        auto unknownIssues =
            make_unknown_command_issues(*cgm_file, ctx->trace_unknown);
        runtimeIssues.insert(
            runtimeIssues.end(),
            std::make_move_iterator(unknownIssues.begin()),
            std::make_move_iterator(unknownIssues.end()));

        if (ctx->fail_on_warn && !runtimeIssues.empty())
        {
            set_error("Conversion rejected because unsupported CGM commands were encountered");
            store_conversion_report(
                ctx,
                std::move(document),
                output_svg_path,
                false,
                ctx->compress_output,
                ctx->minify,
                ctx->optimize_paths,
                geometryMetrics,
                std::move(runtimeIssues));
            return OPENCGM_ERR_GENERAL;
        }

        // Write to file (optionally compressed for SVGZ).
        if (ctx->compress_output) {
            std::vector<uint8_t> compressed;
            if (!opencgm::gzipCompress(svg_content, compressed)) {
                set_error("Failed to compress SVG output");
                return OPENCGM_ERR_GENERAL;
            }

            std::ofstream out(output_svg_path, std::ios::out | std::ios::binary | std::ios::trunc);
            if (!out.is_open()) {
                set_error(std::string("Failed to open output file: ") + output_svg_path);
                return OPENCGM_ERR_IO;
            }
            out.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
            out.close();
        } else {
            std::ofstream out(output_svg_path, std::ios::out | std::ios::trunc);
            if (!out.is_open()) {
                set_error(std::string("Failed to open output file: ") + output_svg_path);
                return OPENCGM_ERR_IO;
            }
            out << svg_content;
            out.close();
        }

        if (ctx->verbose && !ctx->quiet && ctx->log_stream.is_open()) {
            ctx->log_stream << "Wrote SVG: " << output_svg_path << std::endl;
        }

        if (ctx->generate_xcf) {
            std::filesystem::path xcf_path(output_svg_path);
            xcf_path.replace_extension(".xcf");
            const std::string svg_filename = std::filesystem::path(output_svg_path).filename().string();
            if (!write_xcf_output(ctx, cgm_file.get(), svg_filename, xcf_path.string())) {
                set_error(std::string("Failed to write XCF file: ") + xcf_path.string());
                return OPENCGM_ERR_IO;
            }
        }

        runtimeIssues.push_back(make_runtime_issue(
            opencgm::ValidationSeverity::INFO,
            opencgm::PreservationDisposition::Preserved,
            "picture-selection",
            "Converted a single picture from a multi-picture CGM"));
        store_conversion_report(ctx, std::move(document), output_svg_path, true, ctx->compress_output, ctx->minify, ctx->optimize_paths, geometryMetrics, std::move(runtimeIssues));
        set_error("");
        return OPENCGM_OK;

    } catch (const std::exception& e) {
        set_error(std::string("Picture conversion failed: ") + e.what());
        return OPENCGM_ERR_PARSE;
    } catch (...) {
        set_error("Picture conversion failed: unknown error");
        return OPENCGM_ERR_GENERAL;
    }
}

OPENCGM_API void opencgm_set_multi_picture_mode(opencgm_ctx_t* ctx, int mode) {
    if (!ctx) {
        return;
    }
    ctx->multi_picture_mode = (mode >= 0 && mode <= 2) ? mode : 0;
}

// XCF Generation
OPENCGM_API void opencgm_set_generate_xcf(opencgm_ctx_t* ctx, int enabled) {
    if (!ctx) {
        return;
    }
    ctx->generate_xcf = (enabled != 0);
}

OPENCGM_API void opencgm_set_xcf_dtd_version(opencgm_ctx_t* ctx, const char* version) {
    if (!ctx || !version) {
        return;
    }
    ctx->xcf_dtd_version = version;
}

OPENCGM_API void opencgm_set_xcf_include_hotspots(opencgm_ctx_t* ctx, int include) {
    if (!ctx) {
        return;
    }
    ctx->xcf_include_hotspots = (include != 0);
}

OPENCGM_API void opencgm_set_xcf_include_metadata(opencgm_ctx_t* ctx, int include) {
    if (!ctx) {
        return;
    }
    ctx->xcf_include_metadata = (include != 0);
}

OPENCGM_API int opencgm_generate_xcf(opencgm_ctx_t* ctx,
                                      const char* input_cgm_path,
                                      const char* svg_filename,
                                      const char* output_xcf_path) {
    if (!ctx || !input_cgm_path || !svg_filename || !output_xcf_path) {
        set_error("Invalid argument");
        return OPENCGM_ERR_INVALID_ARG;
    }

    // Validate file paths
    if (!validate_file_path(input_cgm_path) || !validate_file_path(output_xcf_path)) {
        set_error("Invalid file path");
        return OPENCGM_ERR_INVALID_ARG;
    }

    try {
        // Load CGM file
        opencgm::BinaryCGMFile cgmFile(input_cgm_path);

        if (!write_xcf_output(ctx, &cgmFile, svg_filename, output_xcf_path)) {
            set_error("Failed to write XCF file");
            return OPENCGM_ERR_GENERAL;
        }

        return OPENCGM_OK;
    } catch (const std::exception& e) {
        set_error(std::string("XCF generation failed: ") + e.what());
        return OPENCGM_ERR_GENERAL;
    }
}

OPENCGM_API int opencgm_get_hotspot_count(opencgm_ctx_t* ctx, const char* input_path) {
    if (!ctx || !input_path) {
        set_error("Invalid argument");
        return -1;
    }

    try {
        auto cgm_file = std::make_unique<opencgm::BinaryCGMFile>(input_path);
        // Count BEGIN APS commands (class 8 = Segment Control, id 21 = BEGIN APS)
        int count = 0;
        for (const auto& cmd : cgm_file->commands()) {
            if (cmd->elementClass() == opencgm::ClassCode::SegmentControlandSegmentAttributeElements &&
                cmd->elementId() == 21) {
                count++;
            }
        }
        return count;
    } catch (...) {
        set_error("Failed to load CGM file");
        return -1;
    }
}

// ============================================================================
// XCF Input / Merging
// ============================================================================

OPENCGM_API void opencgm_set_companion_mode(opencgm_ctx_t* ctx, int mode) {
    if (!ctx) return;
    ctx->companion_mode = mode;
}

OPENCGM_API int opencgm_set_xcf_input(opencgm_ctx_t* ctx, const char* xcf_path) {
    if (!ctx || !xcf_path) {
        set_error("Invalid argument");
        return OPENCGM_ERR_INVALID_ARG;
    }

    try {
        // Parse XCF file
        opencgm::XcfParser parser;
        opencgm::XcfParseResult result = parser.parseFile(xcf_path);

        if (!result.success) {
            set_error("XCF parse error: " + result.errorMessage);
            return OPENCGM_ERR_PARSE;
        }

        // Load into merger
        ctx->xcf_merger.setXcfData(result.data);
        ctx->xcf_input_path = xcf_path;

        set_error("");
        return OPENCGM_OK;
    } catch (const std::exception& e) {
        set_error(std::string("Failed to load XCF: ") + e.what());
        return OPENCGM_ERR_PARSE;
    }
}

OPENCGM_API void opencgm_clear_xcf_input(opencgm_ctx_t* ctx) {
    if (!ctx) return;
    ctx->xcf_merger = opencgm::XcfMerger();  // Reset to empty merger
    ctx->xcf_input_path.clear();
}

OPENCGM_API int opencgm_has_xcf_input(opencgm_ctx_t* ctx) {
    if (!ctx) return 0;
    return ctx->xcf_merger.hasXcfData() ? 1 : 0;
}

// Static storage for returned path (thread-local for safety)
static thread_local std::string g_found_xcf_path;

OPENCGM_API const char* opencgm_find_companion_xcf(const char* cgm_path) {
    if (!cgm_path) {
        return "";
    }

    g_found_xcf_path = opencgm::XcfParser::findCompanionXcf(cgm_path);
    return g_found_xcf_path.c_str();
}

OPENCGM_API int opencgm_validate_xcf(opencgm_ctx_t* ctx, const char* xcf_path) {
    if (!ctx || !xcf_path) {
        set_error("Invalid argument");
        return OPENCGM_ERR_INVALID_ARG;
    }

    try {
        opencgm::XcfParser parser;
        opencgm::XcfParseResult result = parser.parseFile(xcf_path);

        if (!result.success) {
            set_error("XCF validation failed: " + result.errorMessage);
            return OPENCGM_ERR_PARSE;
        }

        set_error("");
        return OPENCGM_OK;
    } catch (const std::exception& e) {
        set_error(std::string("XCF validation failed: ") + e.what());
        return OPENCGM_ERR_PARSE;
    }
}

} // extern "C"
