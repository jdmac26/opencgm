#include "opencgm/svg/attribute_manager.h"
#include "opencgm/svg/xml_utils.h"
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <set>

namespace opencgm {
namespace svg {

AttributeManager::AttributeManager(OutputFormat format)
    : format_(format)
{
}

void AttributeManager::setOutputFormat(OutputFormat format) {
    format_ = format;
}

AttributeManager::AttributeSet AttributeManager::transformAttributes(
    const std::string& apsId,
    const std::string& apsName,
    const std::map<std::string, std::string>& cgmAttributes)
{
    AttributeSet attrs;

    // Helper: classify a CGM attribute key against the active emission gates.
    // Returns true when the attribute may be emitted under the current profile.
    // Keys observed in the wild: "name", "inherit", "id", "type", "region", "regiontype",
    // "regionshape", "viewcontext", "linkuri", "linktarget", "linktitle", "title",
    // "screentip", "apsname", "apstype", "apsid", "behavior", "desc".
    auto gatedKey = [&](const std::string& key) -> bool {
        if (key == "region" || key == "regionshape" || key == "regiontype") return gates_.emit_region;
        if (key == "viewcontext") return gates_.emit_viewcontext;
        if (key == "linkuri" || key == "linktarget") return gates_.emit_link_uri;
        if (key == "linktitle" || key == "title") return gates_.emit_link_title;
        if (key == "screentip") return gates_.emit_screen_tip;
        if (key == "apstype" || key == "type") return gates_.emit_aps_type;
        if (key == "apsname" || key == "name" || key == "inherit") return gates_.emit_name;
        if (key == "apsid" || key == "id") return gates_.emit_id;
        return gates_.emit_auxiliary;
    };

    // Tier 0: WebCGM 2.1 namespace baseline. A de-facto convention across traditional IETP
    // viewers, which consume these via getAttributeNS(). Emitted for every WebCGM-family
    // format; opt out via gates_.emit_webcgm_namespace (CALS / StandardSVG).
    if (includeWebCgmNamespace()) {
        if (gates_.emit_name && !apsName.empty()) {
            attrs.webcgm["webcgm:name"] = apsName;
        }
        for (const auto& [key, value] : cgmAttributes) {
            if (!gatedKey(key)) continue;
            // Mirror the ten WebCGM 2.1 APS attribute types plus the type marker.
            // `name` is handled above from the apsName parameter; skip the duplicate.
            // `id` is conveyed by the SVG id="" attribute on the same <g>, not webcgm:id.
            // `inherit` has no documented consumer.
            if (key == "id" || key == "apsid"
                || key == "name" || key == "apsname"
                || key == "inherit") {
                continue;
            }
            // WebCGM 2.1 defines exactly ten APS attribute types
            // (region, viewcontext, linkuri, layername, layerdesc, screentip,
            //  name, content, visibility, interactivity). `linktitle` is an
            // S1000D / IETP extension, not WebCGM-conformant — it remains in
            // the Tier 1 (data-aps-linktitle) HTML5-host stream below, but is
            // not mis-attributed to the webcgm: namespace here.
            if (key == "type" || key == "apstype"
                || key == "region" || key == "regionshape" || key == "regiontype"
                || key == "viewcontext"
                || key == "linkuri" || key == "linktarget"
                || key == "screentip"
                || key == "content" || key == "visibility" || key == "interactivity"
                || key == "layername" || key == "layerdesc"
                || key == "behavior" || key == "desc") {
                // Normalize type/apstype to webcgm:type for the APS-type marker.
                std::string attrName = (key == "apstype") ? "webcgm:type" : "webcgm:" + key;
                attrs.webcgm[attrName] = value;
            }
        }
    }

    // Tier 1: Legacy WebCGM 2.1 HTML5 compatibility (data-aps-*). Note: data-aps-id,
    // data-aps-type, and data-aps-inherit are intentionally NOT emitted -- the bare
    // SVG `id` and `class="cgm-grobject"` on the same <g> carry the same information
    // (and `id` is what every SVG/JS consumer uses anyway). data-aps-inherit had no
    // documented consumer in or outside this codebase.
    if (includeLegacy()) {
        if (gates_.emit_name && !apsName.empty()) {
            // Skip data-aps-name when webcgm:name already carries this value
            // (Tier 0 ↔ Tier 1 dedupe — same value on the same element is byte waste).
            auto it = attrs.webcgm.find("webcgm:name");
            if (it == attrs.webcgm.end() || it->second != apsName) {
                attrs.legacy["data-aps-name"] = apsName;
            }
        }

        // Map remaining CGM attributes to data-aps-* format, gated per
        // attribute. Skip keys whose information is already conveyed by
        // structural attributes on the <g> element or by the explicit emit
        // above (apsid -> id, type -> class, apsname/name -> data-aps-name).
        for (const auto& [key, value] : cgmAttributes) {
            if (!gatedKey(key)) continue;
            if (key == "id" || key == "apsid"
                || key == "type" || key == "apstype"
                || key == "name" || key == "apsname"
                || key == "inherit") {
                continue;
            }
            // Skip data-aps-foo when webcgm:foo already carries the same value.
            // The dual-emission strategy (HTML5 host vs IETP-viewer parity) is
            // intentional for distinct keys, but emitting two attributes with
            // literally identical values on the same element is byte waste.
            // Tier 2 v6/legacy attributes (data-apsid/data-apsname/bare
            // apsid/name) are unaffected — those are separate identifiers.
            const std::string webcgmKey = "webcgm:" + key;
            auto webcgmIt = attrs.webcgm.find(webcgmKey);
            if (webcgmIt != attrs.webcgm.end() && webcgmIt->second == value) {
                continue;
            }
            attrs.legacy["data-aps-" + key] = value;
        }
    }

    // Tier 2 (Issue 6): S1000D Issue 6 modern overlay per Table 11 — data-apsid /
    // data-apsname + DM-reference translation. Region/viewcontext/screentip/etc.
    // are now carried by the Tier 0 (webcgm:*) and Tier 1 (data-aps-*) layers.
    if (includeS1000D()) {
        if (gates_.emit_id) {
            attrs.s1000d["data-apsid"] = apsId;
        }
        if (gates_.emit_name && !apsName.empty()) {
            attrs.s1000d["data-apsname"] = apsName;
        }

        // S1000D-specific: translate DM/internal references on linkuri.
        auto linkIt = cgmAttributes.find("linkuri");
        if (linkIt != cgmAttributes.end() && gatedKey("linkuri")) {
            const std::string& value = linkIt->second;
            if (value.find("DMC-") == 0 || value.find("DMRef-") == 0) {
                attrs.s1000d["data-dm-ref"] = value;
            } else if (value.find("#") == 0) {
                attrs.s1000d["data-internal-ref-id"] = value.substr(1);
            }
        }
    }

    // Tier 2 (pre-v6): S1000D Issues 2.3 - 5.0 never standardized SVG hotspot
    // markup, so each pre-v6 viewer keys on its own bare-attribute convention.
    // Emit the superset: apsid + name (historic OpenCGM output) plus apsname
    // (RWS LiveContent documents matching bare apsname against the DM
    // hotspot's applicationStructureName). Bytes are cheap; a missed viewer
    // convention breaks hotspots entirely.
    // Region/viewcontext/etc. still flow through Tier 0 + Tier 1 above.
    if (includeS1000DLegacy()) {
        if (gates_.emit_id && !apsId.empty()) {
            attrs.s1000d_legacy["apsid"] = apsId;
        }
        if (gates_.emit_name && !apsName.empty()) {
            attrs.s1000d_legacy["name"] = apsName;
            attrs.s1000d_legacy["apsname"] = apsName;
        }
    }

    // Vendor-specific formats
    if (format_ == OutputFormat::RWS || format_ == OutputFormat::Multi) {
        // RWS LiveContent Viewer format
        attrs.vendor["data-aps-id"] = apsId;  // RWS requires legacy id
        attrs.vendor["data-aps-name"] = apsName;

        // Map behavior to RWS action
        auto behaviorIt = cgmAttributes.find("behavior");
        if (behaviorIt != cgmAttributes.end()) {
            std::string behavior = behaviorIt->second;
            std::string action = "navigate";
            if (behavior == "NewWindow" || behavior == "Replace") {
                action = "navigate";
            } else if (behavior == "Embed") {
                action = "showDetails";
            }
            attrs.vendor["data-rws-action"] = action;
        }

        // Map linkuri to RWS target
        auto linkIt = cgmAttributes.find("linkuri");
        if (linkIt != cgmAttributes.end()) {
            attrs.vendor["data-rws-target"] = linkIt->second;
        }

        // RWS defaults
        attrs.vendor["data-rws-highlight"] = "true";
        attrs.vendor["data-rws-clickable"] = "true";
    }

    if (format_ == OutputFormat::Boeing || format_ == OutputFormat::Multi) {
        // Boeing Spectrum format
        attrs.vendor["data-hotspot-id"] = apsId;
        attrs.vendor["data-hotspot-name"] = apsName;

        // Map behavior to Spectrum action
        auto behaviorIt = cgmAttributes.find("behavior");
        if (behaviorIt != cgmAttributes.end()) {
            std::string behavior = behaviorIt->second;
            std::string action = "link";
            if (behavior == "NewWindow") {
                action = "link";
            } else if (behavior == "Embed") {
                action = "zoom";
            }
            attrs.vendor["data-spectrum-action"] = action;
        }

        // Map linkuri to Spectrum href with dm:// protocol
        auto linkIt = cgmAttributes.find("linkuri");
        if (linkIt != cgmAttributes.end()) {
            std::string href = linkIt->second;
            // Add dm:// protocol if it looks like a DM reference
            if (href.find("DMC-") == 0 || href.find("DMRef-") == 0) {
                href = "dm://" + href;
            }
            attrs.vendor["data-spectrum-href"] = href;
        }
    }

    if (format_ == OutputFormat::R4I || format_ == OutputFormat::Multi) {
        // Rolls-Royce R4I format (hybrid: legacy + R4I extensions)
        attrs.vendor["data-aps-id"] = apsId;
        attrs.vendor["data-aps-name"] = apsName;

        // R4I uses custom highlight color (gold by default)
        attrs.vendor["data-r4i-highlight-color"] = "#FFD700";

        // R4I detail level (default to 2)
        attrs.vendor["data-r4i-detail-level"] = "2";
    }

    return attrs;
}

void AttributeManager::addS1000DMetadata(
    AttributeSet& attrs,
    const HotspotMetadata& metadata)
{
    if (!includeS1000D()) {
        return;
    }

    // Add S1000D Issue 6 hotspot metadata attributes
    if (!metadata.hotspotType.empty()) {
        attrs.s1000d["data-hotspot-type"] = metadata.hotspotType;

        // Also map to vendor formats
        if (format_ == OutputFormat::RWS || format_ == OutputFormat::Multi) {
            // RWS doesn't have direct equivalent, but we can add a tooltip hint
            if (metadata.hotspotType == "callout") {
                attrs.vendor["data-rws-style"] = "callout";
            }
        }
        if (format_ == OutputFormat::Boeing || format_ == OutputFormat::Multi) {
            attrs.vendor["data-spectrum-type"] = metadata.hotspotType;
        }
    }

    if (!metadata.hotspotTitle.empty()) {
        attrs.s1000d["data-hotspot-title"] = metadata.hotspotTitle;

        // Map to vendor tooltip attributes
        if (format_ == OutputFormat::RWS || format_ == OutputFormat::Multi) {
            attrs.vendor["data-rws-tooltip"] = metadata.hotspotTitle;
        }
        if (format_ == OutputFormat::Boeing || format_ == OutputFormat::Multi) {
            attrs.vendor["data-spectrum-tooltip"] = metadata.hotspotTitle;
        }
    }

    if (!metadata.objectDescr.empty()) {
        attrs.s1000d["data-object-descr"] = metadata.objectDescr;
    }

    if (!metadata.visibility.empty()) {
        attrs.s1000d["data-visibility"] = metadata.visibility;
    }

    // Add internal references
    if (!metadata.internalRefIds.empty()) {
        std::ostringstream refs;
        for (size_t i = 0; i < metadata.internalRefIds.size(); ++i) {
            if (i > 0) refs << " ";
            refs << metadata.internalRefIds[i];
        }
        attrs.s1000d["data-internal-ref-ids"] = refs.str();
    }

    // Add data module references
    if (!metadata.dmRefs.empty()) {
        std::ostringstream refs;
        for (size_t i = 0; i < metadata.dmRefs.size(); ++i) {
            if (i > 0) refs << " ";
            refs << metadata.dmRefs[i];
        }
        attrs.s1000d["data-dm-refs"] = refs.str();
    }
}

void AttributeManager::addVendorAttributes(
    AttributeSet& attrs,
    const std::string& vendorName,
    const std::map<std::string, std::string>& vendorData)
{
    if (!includeVendor(vendorName)) {
        return;
    }

    // Add vendor-specific attributes with appropriate prefix
    std::string prefix;
    if (vendorName == "rws") {
        prefix = "data-rws-";
    } else if (vendorName == "boeing" || vendorName == "spectrum") {
        prefix = "data-spectrum-";
    } else if (vendorName == "r4i") {
        prefix = "data-r4i-";
    } else {
        // Generic vendor prefix
        prefix = "data-" + vendorName + "-";
    }

    for (const auto& [key, value] : vendorData) {
        attrs.vendor[prefix + key] = value;
    }
}

std::string AttributeManager::generateSvgAttributes(const AttributeSet& attrs) const {
    std::ostringstream output;
    auto emitAttributes = [&](const std::map<std::string, std::string>& map, std::set<std::string>& emitted) {
        for (const auto& [key, value] : map) {
            if (emitted.insert(key).second) {
                output << " " << key << "=\"" << escapeXmlAttribute(value) << "\"";
            }
        }
    };

    std::set<std::string> emittedKeys;

    // Tier 0 always first: WebCGM 2.1 namespace baseline (empty when opted out).
    emitAttributes(attrs.webcgm, emittedKeys);

    switch (format_) {
        case OutputFormat::WebCGM:
            // Tier 1 only (HTML5 data-aps-*).
            emitAttributes(attrs.legacy, emittedKeys);
            break;
        case OutputFormat::S1000D_Issue6:
            // Tier 1 (data-aps-*) + Tier 2 v6 overlay (data-apsid / data-apsname / data-dm-ref).
            emitAttributes(attrs.legacy, emittedKeys);
            emitAttributes(attrs.s1000d, emittedKeys);
            break;
        case OutputFormat::S1000D_Legacy:
            // Tier 1 (data-aps-*) + Tier 2 pre-v6 overlay (bare apsid / name).
            emitAttributes(attrs.legacy, emittedKeys);
            emitAttributes(attrs.s1000d_legacy, emittedKeys);
            break;
        case OutputFormat::RWS:
        case OutputFormat::Boeing:
        case OutputFormat::R4I:
            // Tier 1 + vendor overlay.
            emitAttributes(attrs.legacy, emittedKeys);
            emitAttributes(attrs.vendor, emittedKeys);
            break;
        case OutputFormat::Multi:
            // Diagnostic: emit everything.
            emitAttributes(attrs.legacy, emittedKeys);
            emitAttributes(attrs.s1000d, emittedKeys);
            emitAttributes(attrs.s1000d_legacy, emittedKeys);
            emitAttributes(attrs.vendor, emittedKeys);
            break;
    }

    return output.str();
}

AttributeManager::OutputFormat AttributeManager::parseOutputFormat(const std::string& formatStr) {
    std::string lower = formatStr;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower == "legacy" || lower == "webcgm" || lower == "webcgm21" || lower == "default") {
        return OutputFormat::WebCGM;
    } else if (lower == "s1000d6" || lower == "s1000d" || lower == "modern") {
        return OutputFormat::S1000D_Issue6;
    } else if (lower == "s1000d-legacy" || lower == "s1000dlegacy" ||
               lower == "s1000d-pre-v6" || lower == "s1000dprev6") {
        return OutputFormat::S1000D_Legacy;
    } else if (lower == "rws" || lower == "livecontent") {
        return OutputFormat::RWS;
    } else if (lower == "boeing" || lower == "spectrum") {
        return OutputFormat::Boeing;
    } else if (lower == "r4i" || lower == "rollsroyce") {
        return OutputFormat::R4I;
    } else if (lower == "multi" || lower == "all" || lower == "combined") {
        return OutputFormat::Multi;
    } else {
        throw std::invalid_argument("Invalid output format: " + formatStr);
    }
}

std::string AttributeManager::outputFormatToString(OutputFormat format) {
    switch (format) {
        case OutputFormat::WebCGM:
            return "webcgm";
        case OutputFormat::S1000D_Issue6:
            return "s1000d6";
        case OutputFormat::S1000D_Legacy:
            return "s1000d-legacy";
        case OutputFormat::RWS:
            return "rws";
        case OutputFormat::Boeing:
            return "boeing";
        case OutputFormat::R4I:
            return "r4i";
        case OutputFormat::Multi:
            return "multi";
        default:
            return "unknown";
    }
}

std::string AttributeManager::mapAttributeName(
    const std::string& cgmAttr,
    const std::string& targetFormat) const
{
    // Attribute name mapping table
    if (targetFormat == "legacy") {
        return "data-aps-" + cgmAttr;
    } else if (targetFormat == "s1000d") {
        if (cgmAttr == "linkuri") {
            return "data-dm-ref";  // Context-dependent, may be overridden
        }
        return "data-aps-" + cgmAttr;  // Most attributes keep same name
    } else if (targetFormat == "rws") {
        if (cgmAttr == "linkuri") return "data-rws-target";
        if (cgmAttr == "behavior") return "data-rws-action";
        return "data-rws-" + cgmAttr;
    } else if (targetFormat == "boeing") {
        if (cgmAttr == "linkuri") return "data-spectrum-href";
        if (cgmAttr == "behavior") return "data-spectrum-action";
        return "data-spectrum-" + cgmAttr;
    } else if (targetFormat == "r4i") {
        return "data-r4i-" + cgmAttr;
    }

    return "data-" + cgmAttr;
}

std::string AttributeManager::mapAttributeValue(
    const std::string& cgmAttr,
    const std::string& value,
    const std::string& targetFormat) const
{
    // Behavior attribute value mapping
    if (cgmAttr == "behavior") {
        if (targetFormat == "rws") {
            if (value == "NewWindow" || value == "Replace") return "navigate";
            if (value == "Embed") return "showDetails";
        } else if (targetFormat == "boeing") {
            if (value == "NewWindow") return "link";
            if (value == "Replace") return "link";
            if (value == "Embed") return "zoom";
        }
    }

    // Link URI transformation for Boeing (add dm:// protocol)
    if (cgmAttr == "linkuri" && targetFormat == "boeing") {
        if (value.find("DMC-") == 0 || value.find("DMRef-") == 0) {
            return "dm://" + value;
        }
    }

    // No transformation needed
    return value;
}

bool AttributeManager::includeVendor(const std::string& vendorName) const {
    if (format_ == OutputFormat::Multi) {
        return true;  // Multi includes all vendors
    }

    if (format_ == OutputFormat::RWS && vendorName == "rws") {
        return true;
    }
    if (format_ == OutputFormat::Boeing && (vendorName == "boeing" || vendorName == "spectrum")) {
        return true;
    }
    if (format_ == OutputFormat::R4I && vendorName == "r4i") {
        return true;
    }

    return false;
}

} // namespace svg
} // namespace opencgm
