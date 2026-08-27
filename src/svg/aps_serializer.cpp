#include "opencgm/svg/aps_serializer.h"

#include "opencgm/svg/aps_attribute_interpreter.h"
#include "opencgm/svg/xml_utils.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <utility>

namespace opencgm::svg
{
    namespace
    {
        std::string indentation(int level)
        {
            return std::string(
                static_cast<size_t>(std::max(0, level)) * 2U,
                ' ');
        }

        std::string lowerCopy(const std::string &value)
        {
            std::string lower = value;
            std::transform(
                lower.begin(),
                lower.end(),
                lower.begin(),
                [](unsigned char ch)
                {
                    return static_cast<char>(std::tolower(ch));
                });
            return lower;
        }

        std::string jsonString(const std::string &value)
        {
            std::ostringstream output;
            output << '"';
            for (unsigned char ch : value)
            {
                switch (ch)
                {
                    case '"':
                        output << "\\\"";
                        break;
                    case '\\':
                        output << "\\\\";
                        break;
                    case '\b':
                        output << "\\b";
                        break;
                    case '\f':
                        output << "\\f";
                        break;
                    case '\n':
                        output << "\\n";
                        break;
                    case '\r':
                        output << "\\r";
                        break;
                    case '\t':
                        output << "\\t";
                        break;
                    default:
                        if (ch < 0x20)
                        {
                            output << "\\u"
                                   << std::hex
                                   << std::setw(4)
                                   << std::setfill('0')
                                   << static_cast<int>(ch)
                                   << std::dec;
                        }
                        else
                        {
                            output << static_cast<char>(ch);
                        }
                        break;
                }
            }
            output << '"';
            return output.str();
        }

        std::string linksJson(const std::vector<LinkuriEntry> &links)
        {
            std::ostringstream output;
            output << '[';
            for (size_t index = 0; index < links.size(); ++index)
            {
                if (index != 0)
                {
                    output << ',';
                }
                const auto &link = links[index];
                output << "{\"uri\":" << jsonString(link.uri);
                if (!link.title.empty())
                {
                    output << ",\"title\":" << jsonString(link.title);
                }
                if (!link.behavior.empty())
                {
                    output << ",\"behavior\":"
                           << jsonString(link.behavior);
                }
                output << '}';
            }
            output << ']';
            return output.str();
        }

        std::string linkMenuJson(const std::vector<LinkuriEntry> &links)
        {
            std::ostringstream output;
            output << '[';
            for (size_t index = 0; index < links.size(); ++index)
            {
                if (index != 0)
                {
                    output << ',';
                }
                const auto &link = links[index];
                output << "{\"uri\":" << jsonString(link.uri)
                       << ",\"title\":"
                       << jsonString(
                              link.title.empty()
                                  ? link.uri
                                  : link.title)
                       << '}';
            }
            output << ']';
            return output.str();
        }

        std::string inlineMetadata(
            const std::map<std::string, std::string> &attributes,
            const ResolvedConversionPlan &plan,
            int indentationLevel,
            bool includeScreenTip,
            bool &wroteMetadata)
        {
            wroteMetadata = false;
            if (!ApsPolicy::shouldEmitInlineMetadata(plan))
            {
                return {};
            }

            std::ostringstream output;
            const std::string indent = indentation(indentationLevel);
            if (includeScreenTip &&
                plan.output_target.preserve_aps_screen_tip)
            {
                if (auto it = attributes.find("screentip");
                    it != attributes.end() && !it->second.empty())
                {
                    output << indent << "<title>"
                           << escapeXmlText(it->second)
                           << "</title>\n";
                    wroteMetadata = true;
                }
            }

            if (auto it = attributes.find("desc");
                it != attributes.end() && !it->second.empty())
            {
                output << indent << "<desc>"
                       << escapeXmlText(it->second)
                       << "</desc>\n";
                wroteMetadata = true;
            }
            return output.str();
        }
    }

    ApsSerializationState ApsSerializer::analyze(
        const ApsNode &node,
        const std::map<std::string, std::string> &mergedAttributes,
        std::map<std::string, std::string> metadataAttributes)
    {
        ApsSerializationState state;
        state.aps_id = !node.identifier.empty()
                           ? node.identifier
                           : node.resolved_identifier;
        state.is_layer = lowerCopy(node.type) == "layer";
        state.is_link = ApsPolicy::isLink(node, mergedAttributes);

        if (!node.type.empty())
        {
            metadataAttributes.try_emplace("type", node.type);
        }
        if (!node.identifier.empty())
        {
            metadataAttributes.try_emplace("apsid", node.identifier);
        }

        auto nameIt = metadataAttributes.find("apsname");
        if (nameIt == metadataAttributes.end())
        {
            nameIt = metadataAttributes.find("name");
        }
        if (nameIt != metadataAttributes.end())
        {
            state.aps_name = nameIt->second;
        }

        bool hasVisibility = false;
        std::string visibility;
        if (auto it = metadataAttributes.find("visibility");
            it != metadataAttributes.end())
        {
            hasVisibility = true;
            visibility = it->second;
        }
        else if (auto mergedIt = mergedAttributes.find("visibility");
                 mergedIt != mergedAttributes.end())
        {
            hasVisibility = true;
            visibility = mergedIt->second;
        }
        state.effective_visible =
            ApsAttributeInterpreter::parseVisibility(visibility, true);
        state.emit_visibility_state = state.is_layer || hasVisibility;

        if (state.is_layer)
        {
            if (auto it = metadataAttributes.find("layername");
                it != metadataAttributes.end() && !it->second.empty())
            {
                state.layer_name = it->second;
            }
            else if (!state.aps_name.empty())
            {
                state.layer_name = state.aps_name;
            }
            else
            {
                state.layer_name = state.aps_id;
            }

            if (auto it = metadataAttributes.find("layerdesc");
                it != metadataAttributes.end() && !it->second.empty())
            {
                state.layer_title = it->second;
            }
        }

        state.serialization_attributes = metadataAttributes;
        state.serialization_attributes.erase("apsid");

        if (state.emit_visibility_state)
        {
            metadataAttributes["resolved-visibility"] =
                state.effective_visible ? "on" : "off";
        }
        if (state.is_layer && !state.layer_name.empty())
        {
            metadataAttributes["resolved-layer"] = state.layer_name;
        }
        state.metadata_attributes = std::move(metadataAttributes);
        return state;
    }

    std::string ApsSerializer::derivedGroupAttributes(
        const ApsSerializationState &state,
        const ResolvedConversionPlan &plan)
    {
        // Metadata emission is governed by emit_attribute_metadata plus the
        // preserve flags, not by the hotspot link-wrapping mode (see
        // ApsPolicy::emissionGates).
        const bool emitAttrs =
            plan.output_target.emit_attribute_metadata;
        std::ostringstream output;
        if (emitAttrs &&
            state.is_layer &&
            !state.layer_name.empty() &&
            plan.output_target.preserve_layer_hierarchy)
        {
            output << " data-layer=\""
                   << escapeXmlAttribute(state.layer_name)
                   << "\"";
        }
        if (emitAttrs &&
            state.is_layer &&
            !state.layer_title.empty() &&
            plan.output_target.preserve_layer_hierarchy)
        {
            output << " data-layer-title=\""
                   << escapeXmlAttribute(state.layer_title)
                   << "\"";
        }
        if (state.emit_visibility_state)
        {
            if (emitAttrs)
            {
                output << " data-aps-visible=\""
                       << (state.effective_visible ? "true" : "false")
                       << "\"";
            }
            if (!state.effective_visible)
            {
                output << " style=\"display:none\"";
            }
        }
        return output.str();
    }

    ApsOpenMarkup ApsSerializer::bodyOpen(
        const ApsNode &node,
        const std::map<std::string, std::string> &mergedAttributes,
        const ApsSerializationState &state,
        const ResolvedConversionPlan &plan,
        const std::string &regionPolygon,
        bool regionPolygonValid)
    {
        ApsOpenMarkup result;
        std::ostringstream output;
        const bool emitRegion =
            regionPolygonValid &&
            ApsPolicy::shouldEmitRegionOverlay(plan);
        // Owner marker rides with the region overlay itself (gated by
        // preserve_aps_region via emitRegion), independent of link wrapping.
        const bool emitOwner =
            plan.output_target.emit_attribute_metadata &&
            !state.aps_id.empty();

        const auto linkIt = mergedAttributes.find("linkuri");
        const bool emitAnchor =
            state.is_link &&
            linkIt != mergedAttributes.end() &&
            ApsPolicy::shouldEmitAnchor(plan);

        output << ">\n";
        if (emitAnchor)
        {
            output << indentation(node.nesting_level + 1);
            const bool multiple = node.linkuris.size() > 1;
            const MultiLinkMode mode =
                plan.output_target.multi_link_mode;

            if (multiple && mode == MultiLinkMode::JsEventHandler)
            {
                const std::string script =
                    "event.preventDefault();var links=" +
                    linkMenuJson(node.linkuris) +
                    ";if(typeof showCgmLinkMenu==='function')"
                    "showCgmLinkMenu(event,links);"
                    "else if(links.length>0)"
                    "window.location.href=links[0].uri;";
                output << "<a onclick=\""
                       << escapeXmlAttribute(script)
                       << "\" style=\"cursor:pointer\""
                       << " xlink:href=\""
                       << escapeXmlAttribute(linkIt->second)
                       << "\"";
            }
            else
            {
                output << "<a xlink:href=\""
                       << escapeXmlAttribute(linkIt->second)
                       << "\"";
            }

            if (multiple &&
                mode == MultiLinkMode::JsonDataAttribute &&
                plan.output_target.emit_data_content)
            {
                output << " data-linkuri=\""
                       << escapeXmlAttribute(linksJson(node.linkuris))
                       << "\" data-multilink=\"true\""
                       << " data-link-count=\""
                       << node.linkuris.size()
                       << "\"";
            }

            if (auto it = mergedAttributes.find("behavior");
                it != mergedAttributes.end())
            {
                const std::string lower = lowerCopy(it->second);
                std::string target;
                bool embed = false;
                if (lower == "_blank" ||
                    lower == "_self" ||
                    lower == "_parent" ||
                    lower == "_top")
                {
                    target = lower;
                }
                else if (lower == "_replace")
                {
                    target = "_self";
                }
                else if (lower == "embed")
                {
                    embed = true;
                }

                if (!target.empty())
                {
                    output << " target=\""
                           << escapeXmlAttribute(target)
                           << "\"";
                }
                if (ApsPolicy::shouldEmitDataAttributes(plan))
                {
                    output << " data-aps-behavior=\""
                           << escapeXmlAttribute(it->second)
                           << "\"";
                    if (embed)
                    {
                        output << " data-aps-embed=\"true\"";
                    }
                }
            }
            output << ">\n";

            bool wroteMetadata = false;
            output << inlineMetadata(
                mergedAttributes,
                plan,
                node.nesting_level + 2,
                true,
                wroteMetadata);
            if (emitRegion)
            {
                output << regionOverlay(
                    regionPolygon,
                    state.aps_id,
                    emitOwner,
                    node.nesting_level + 2);
            }
            if (wroteMetadata)
            {
                output << indentation(node.nesting_level + 1);
            }
            result.emitted_anchor = true;
        }
        else
        {
            bool wroteMetadata = false;
            output << inlineMetadata(
                mergedAttributes,
                plan,
                node.nesting_level,
                true,
                wroteMetadata);
            if (emitRegion)
            {
                output << regionOverlay(
                    regionPolygon,
                    state.aps_id,
                    emitOwner,
                    node.nesting_level);
            }
        }

        result.markup = output.str();
        return result;
    }

    std::string ApsSerializer::regionOverlay(
        const std::string &points,
        const std::string &owner,
        bool emitOwner,
        int indentationLevel)
    {
        std::ostringstream output;
        output << indentation(indentationLevel)
               << "<polygon class=\"aps-region-hit\" points=\""
               << escapeXmlAttribute(points)
               << "\" fill=\"#000000\" fill-opacity=\"0\""
               << " stroke=\"none\" pointer-events=\"fill\""
               << " data-aps-region-shape=\"true\"";
        if (emitOwner && !owner.empty())
        {
            output << " data-aps-region-owner=\""
                   << escapeXmlAttribute(owner)
                   << "\"";
        }
        output << " />\n";
        return output.str();
    }

    std::string ApsSerializer::close(
        int nestingLevel,
        bool emittedAnchor)
    {
        std::ostringstream output;
        output << indentation(nestingLevel);
        if (emittedAnchor)
        {
            output << "</a>\n"
                   << indentation(nestingLevel);
        }
        output << "</g>\n";
        return output.str();
    }
}
