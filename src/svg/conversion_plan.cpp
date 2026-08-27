#include "opencgm/svg/conversion_plan.h"

namespace opencgm
{
    HotspotProfileConfig HotspotProfileConfig::fromProfile(
        HotspotProfile profile)
    {
        HotspotProfileConfig config;
        if (profile == HotspotProfile::Strict)
        {
            config.use_data_apsid = false;
            config.emit_xlink_href = false;
            config.metadata_mode = MetadataMode::None;
        }
        return config;
    }

    OutputTargetConfig OutputTargetConfig::forS1000DIETP()
    {
        return {};
    }

    OutputTargetConfig OutputTargetConfig::forATAIETM()
    {
        return {};
    }

    OutputTargetConfig OutputTargetConfig::forStandardSVG()
    {
        OutputTargetConfig config;
        config.hotspot_encoding = HotspotEncodingMode::SvgAnchorTitle;
        config.region_handling = RegionHandlingMode::BboxOnly;
        config.multi_link_mode = MultiLinkMode::FirstLinkOnly;
        // Clean-output contract: no webcgm:*/data-* metadata leaks into
        // standard SVG. Screentips still surface as <title> (inline
        // metadata), links as <a>.
        config.emit_attribute_metadata = false;
        config.emit_data_aps_type = false;
        config.emit_data_name = false;
        config.emit_data_content = false;
        config.emit_data_viewcontext = false;
        config.emit_webcgm_namespace = false;
        return config;
    }

    ResolvedConversionPlan ResolvedConversionPlan::forProfile(
        OutputProfile profile)
    {
        ResolvedConversionPlan plan;
        plan.output_profile = profile;
        plan.adopt_view_on_load = profile != OutputProfile::WebCGM21;

        switch (profile)
        {
            case OutputProfile::S1000D:
                plan.output_target = OutputTargetConfig::forS1000DIETP();
                plan.attribute_format =
                    svg::AttributeManager::OutputFormat::S1000D_Issue6;
                break;
            case OutputProfile::S1000DLegacy:
                plan.output_target = OutputTargetConfig::forS1000DIETP();
                plan.attribute_format =
                    svg::AttributeManager::OutputFormat::S1000D_Legacy;
                break;
            case OutputProfile::ATA2200:
                plan.output_target = OutputTargetConfig::forATAIETM();
                plan.attribute_format =
                    svg::AttributeManager::OutputFormat::S1000D_Issue6;
                break;
            case OutputProfile::StandardSVG:
                plan.output_target = OutputTargetConfig::forStandardSVG();
                plan.attribute_format =
                    svg::AttributeManager::OutputFormat::WebCGM;
                break;
            case OutputProfile::Custom:
                break;
            case OutputProfile::WebCGM21:
            default:
                plan.output_target = OutputTargetConfig::forS1000DIETP();
                plan.attribute_format =
                    svg::AttributeManager::OutputFormat::WebCGM;
                plan.adopt_view_on_load = false;
                break;
        }
        return plan;
    }
}
