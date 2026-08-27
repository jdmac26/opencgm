#include "opencgm/svg/aps_policy.h"

#include <algorithm>
#include <cctype>
#include <regex>

namespace opencgm::svg
{
    namespace
    {
        std::string lowerCopy(const std::string &value)
        {
            std::string lower = value;
            std::transform(
                lower.begin(), lower.end(), lower.begin(),
                [](unsigned char ch)
                {
                    return static_cast<char>(std::tolower(ch));
                });
            return lower;
        }

        bool isSafeCustomAttributeName(const std::string &name)
        {
            if (name.empty())
            {
                return false;
            }
            const auto isStart = [](unsigned char ch)
            {
                return std::isalpha(ch) != 0 || ch == '_';
            };
            const auto isCharacter = [&](unsigned char ch)
            {
                return isStart(ch) || std::isdigit(ch) != 0 ||
                       ch == '.' || ch == '-';
            };
            if (!isStart(static_cast<unsigned char>(name.front())) ||
                !std::all_of(
                    name.begin() + 1, name.end(),
                    [&](char ch)
                    {
                        return isCharacter(static_cast<unsigned char>(ch));
                    }))
            {
                return false;
            }

            const std::string lower = lowerCopy(name);
            return lower.rfind("xml", 0) != 0 &&
                   lower.rfind("on", 0) != 0 &&
                   lower != "id" &&
                   lower != "class" &&
                   lower != "style" &&
                   lower != "href";
        }

        std::string attributeValue(
            const ApsNode &node,
            std::initializer_list<const char *> names)
        {
            for (const char *name : names)
            {
                if (auto it = node.merged_attributes.find(name);
                    it != node.merged_attributes.end())
                {
                    return it->second;
                }
                if (auto it = node.attributes.find(name);
                    it != node.attributes.end())
                {
                    return it->second;
                }
            }
            return {};
        }

        bool matchesRule(
            const CustomAttributeRule &rule,
            const ApsNode &node,
            const std::vector<ApsNode> &ancestors)
        {
            const auto matches = [&](const std::string &value, bool useRegex)
            {
                if (rule.selector == "*")
                {
                    return true;
                }
                if (!useRegex)
                {
                    return value == rule.selector;
                }
                try
                {
                    return std::regex_match(value, std::regex(rule.selector));
                }
                catch (const std::regex_error &)
                {
                    return false;
                }
            };

            switch (rule.scope)
            {
                case CustomAttributeScope::All:
                    return true;
                case CustomAttributeScope::ApsId:
                    return matches(node.identifier, false) ||
                           matches(node.resolved_identifier, false);
                case CustomAttributeScope::ApsName:
                    return matches(
                        attributeValue(node, {"apsname", "name"}), true);
                case CustomAttributeScope::Layer:
                    for (const ApsNode &candidate : ancestors)
                    {
                        if (lowerCopy(candidate.type) != "layer")
                        {
                            continue;
                        }
                        if (matches(
                                attributeValue(
                                    candidate,
                                    {"layername", "apsname", "name"}),
                                false) ||
                            matches(candidate.identifier, false) ||
                            matches(candidate.resolved_identifier, false))
                        {
                            return true;
                        }
                    }
                    return false;
                default:
                    return false;
            }
        }
    }

    bool ApsPolicy::isLink(
        const ApsNode &node,
        const std::map<std::string, std::string> &mergedAttributes)
    {
        const std::string type = lowerCopy(node.type);
        return type == "linkuri" ||
               type == "hotspot" ||
               mergedAttributes.find("linkuri") != mergedAttributes.end();
    }

    AttributeManager::ApsEmissionGates ApsPolicy::emissionGates(
        const ResolvedConversionPlan &plan)
    {
        // hotspot_encoding controls link wrapping (<a>/<title>) only; the
        // emit_attribute_metadata master switch plus the per-key
        // preserve/emit flags decide whether APS metadata survives.
        // SvgAnchorTitle must not silently discard metadata the caller asked
        // to preserve.
        const auto &target = plan.output_target;
        const bool emitAttrs = target.emit_attribute_metadata;
        AttributeManager::ApsEmissionGates gates;
        gates.emit_id =
            emitAttrs &&
            target.preserve_aps_id &&
            plan.hotspot.use_data_apsid;
        gates.emit_name =
            emitAttrs &&
            target.emit_data_name &&
            plan.hotspot.use_apsname;
        gates.emit_aps_type =
            emitAttrs && target.emit_data_aps_type;
        gates.emit_link_uri =
            emitAttrs && target.emit_data_content;
        gates.emit_region =
            emitAttrs && target.preserve_aps_region;
        gates.emit_viewcontext =
            emitAttrs && target.emit_data_viewcontext;
        gates.emit_link_title =
            emitAttrs && target.preserve_aps_link_title;
        gates.emit_screen_tip =
            emitAttrs && target.preserve_aps_screen_tip;
        gates.emit_auxiliary = emitAttrs;
        gates.emit_webcgm_namespace = target.emit_webcgm_namespace;
        return gates;
    }

    bool ApsPolicy::shouldEmitAnchor(const ResolvedConversionPlan &plan)
    {
        return plan.hotspot.emit_xlink_href &&
               plan.output_target.hotspot_encoding !=
                   HotspotEncodingMode::DataAttributes;
    }

    bool ApsPolicy::shouldEmitDataAttributes(
        const ResolvedConversionPlan &plan)
    {
        return plan.output_target.hotspot_encoding !=
               HotspotEncodingMode::SvgAnchorTitle;
    }

    bool ApsPolicy::shouldEmitInlineMetadata(
        const ResolvedConversionPlan &plan)
    {
        return plan.hotspot.metadata_mode == MetadataMode::Inline;
    }

    bool ApsPolicy::shouldEmitRegionOverlay(
        const ResolvedConversionPlan &plan)
    {
        return plan.output_target.preserve_aps_region &&
               plan.output_target.region_handling !=
                   RegionHandlingMode::BboxOnly;
    }

    std::map<std::string, std::string> ApsPolicy::customAttributes(
        const ResolvedConversionPlan &plan,
        const ApsNode &node,
        const std::vector<ApsNode> &ancestors,
        const std::set<std::string> &reservedNames)
    {
        std::map<std::string, std::string> result;
        std::set<std::string> names = reservedNames;
        for (const CustomAttributeRule &rule :
             plan.hotspot.custom_attributes)
        {
            if (!isSafeCustomAttributeName(rule.key) ||
                names.find(rule.key) != names.end() ||
                !matchesRule(rule, node, ancestors))
            {
                continue;
            }
            result.emplace(rule.key, rule.value);
            names.insert(rule.key);
        }
        return result;
    }
}
