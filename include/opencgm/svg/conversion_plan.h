#ifndef OPENCGM_SVG_CONVERSION_PLAN_H
#define OPENCGM_SVG_CONVERSION_PLAN_H

#include "attribute_manager.h"

#include <string>
#include <vector>

namespace opencgm
{
    enum class HotspotProfile { Generic, RWS, PTC, R4i, Strict };
    enum class MetadataMode { None, Inline, External };
    enum class CustomAttributeScope { All, ApsId, ApsName, Layer };

    struct CustomAttributeRule
    {
        std::string key;
        std::string value;
        CustomAttributeScope scope;
        std::string selector;
    };

    struct HotspotProfileConfig
    {
        bool use_apsname = true;
        bool use_data_apsid = true;
        bool emit_xlink_href = true;
        MetadataMode metadata_mode = MetadataMode::Inline;
        std::vector<CustomAttributeRule> custom_attributes;

        static HotspotProfileConfig fromProfile(HotspotProfile profile);
    };

    enum class HotspotEncodingMode { SvgAnchorTitle, DataAttributes, Both };
    enum class RegionHandlingMode { OverlayOnly, BboxOnly, Both };
    enum class MultiLinkMode
    {
        FirstLinkOnly,
        JsonDataAttribute,
        JsEventHandler
    };

    struct OutputTargetConfig
    {
        // hotspot_encoding selects link wrapping (<a>/<title> vs data
        // attributes vs both). It does NOT control metadata survival:
        // attribute metadata is governed by emit_attribute_metadata plus the
        // per-key preserve/emit flags below, so SvgAnchorTitle cannot
        // silently discard metadata the caller asked to preserve.
        HotspotEncodingMode hotspot_encoding = HotspotEncodingMode::Both;
        RegionHandlingMode region_handling = RegionHandlingMode::Both;
        MultiLinkMode multi_link_mode = MultiLinkMode::JsonDataAttribute;
        // Master switch for APS metadata attributes (webcgm:* / data-*).
        // False only for deliberately clean output targets (StandardSVG),
        // which still emit <a>/<title>/geometry.
        bool emit_attribute_metadata = true;
        bool emit_data_aps_type = true;
        bool emit_data_name = true;
        bool emit_data_content = true;
        bool emit_data_viewcontext = true;
        bool emit_viewbox = true;
        bool preserve_layer_hierarchy = true;
        bool preserve_aps_id = true;
        bool preserve_aps_link_title = true;
        bool preserve_aps_region = true;
        bool preserve_aps_screen_tip = true;
        bool emit_webcgm_namespace = true;

        static OutputTargetConfig forS1000DIETP();
        static OutputTargetConfig forATAIETM();
        static OutputTargetConfig forStandardSVG();
    };

    enum class OutputProfile
    {
        WebCGM21,
        S1000D,
        S1000DLegacy,
        ATA2200,
        StandardSVG,
        Custom
    };

    /**
     * Fully resolved output policy consumed by SVGConverter.
     *
     * Legacy individual setters mutate a pending plan. Conversion snapshots the
     * complete plan in one operation, preventing profile/target/format drift.
     */
    struct ResolvedConversionPlan
    {
        OutputProfile output_profile = OutputProfile::WebCGM21;
        OutputTargetConfig output_target =
            OutputTargetConfig::forS1000DIETP();
        svg::AttributeManager::OutputFormat attribute_format =
            svg::AttributeManager::OutputFormat::WebCGM;
        HotspotProfileConfig hotspot =
            HotspotProfileConfig::fromProfile(HotspotProfile::Generic);
        bool adopt_view_on_load = false;

        static ResolvedConversionPlan forProfile(OutputProfile profile);
    };
}

#endif
