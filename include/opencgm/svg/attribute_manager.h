#ifndef OPENCGM_SVG_ATTRIBUTE_MANAGER_H
#define OPENCGM_SVG_ATTRIBUTE_MANAGER_H

#include <string>
#include <map>
#include <vector>

namespace opencgm {
namespace svg {

/**
 * Manages multi-format attribute transformation for SVG output.
 *
 * Supports:
 * - Legacy WebCGM 2.1 (data-aps-* attributes)
 * - S1000D Issue 6 modern format (data-apsid, data-hotspot-* attributes)
 * - Vendor-specific formats (RWS, Boeing Spectrum, R4I)
 *
 * See: docs/HOTSPOT_ATTRIBUTE_MULTI_FORMAT_SUPPORT.md
 */
class AttributeManager {
public:
    /**
     * Output format enumeration
     */
    enum class OutputFormat {
        WebCGM,           ///< WebCGM 2.1 (data-aps-*)
        S1000D_Issue6,    ///< S1000D Issue 6 modern (data-apsid, data-hotspot-*)
        S1000D_Legacy,    ///< S1000D Issues 2.3 - 5.0 (bare apsid, name SVG attrs)
        RWS,              ///< RWS LiveContent Viewer
        Boeing,           ///< Boeing Spectrum
        R4I,              ///< Rolls-Royce R4I
        Multi             ///< Diagnostic: emit all supported formats
    };

    /**
     * Attribute set containing multiple format variants
     *
     * Tier 0 = webcgm (SDI de-facto namespace, baseline for the WebCGM family)
     * Tier 1 = legacy (HTML5 data-aps-* compatibility layer)
     * Tier 2 = s1000d / s1000d_legacy / vendor (standards-specific overlay)
     */
    struct AttributeSet {
        std::map<std::string, std::string> webcgm;         ///< WebCGM 2.1 namespace baseline (webcgm:*)
        std::map<std::string, std::string> legacy;         ///< WebCGM 2.1 HTML5 compatibility (data-aps-*)
        std::map<std::string, std::string> s1000d;         ///< S1000D Issue 6 modern overlay (data-apsid, data-apsname)
        std::map<std::string, std::string> s1000d_legacy;  ///< S1000D pre-v6 overlay (bare apsid, name)
        std::map<std::string, std::string> vendor;         ///< Vendor-specific overlay (data-rws-*, data-spectrum-*, data-r4i-*)
    };

    /**
     * Per-profile gates controlling which APS attributes transformAttributes emits.
     * Default-constructed values are "emit everything" so existing call sites behave
     * as before. CALS uses an all-false instance to strip APS metadata entirely.
     */
    struct ApsEmissionGates {
        bool emit_id = true;
        bool emit_name = true;
        bool emit_aps_type = true;
        bool emit_link_uri = true;
        bool emit_region = true;
        bool emit_viewcontext = true;
        bool emit_link_title = true;
        bool emit_screen_tip = true;
        bool emit_auxiliary = true;  ///< visibility, layer, behavior, desc, and vendor extensions
        bool emit_webcgm_namespace = true;  ///< Tier 0: SDI de-facto webcgm:* attrs on every APS <g>
    };

    /**
     * S1000D Issue 6 hotspot metadata from XML companion file
     */
    struct HotspotMetadata {
        std::string hotspotType;      ///< "callout", "detail", "zoomarea", etc.
        std::string hotspotTitle;     ///< Tooltip text
        std::string objectDescr;      ///< Extended description
        std::string visibility;       ///< "visible" or "hidden"
        std::vector<std::string> internalRefIds;  ///< Internal reference IDs
        std::vector<std::string> dmRefs;          ///< Data module references
    };

    /**
     * Constructor
     * @param format Output format to generate
     */
    explicit AttributeManager(OutputFormat format = OutputFormat::WebCGM);

    /**
     * Set output format
     * @param format New output format
     */
    void setOutputFormat(OutputFormat format);

    /**
     * Get current output format
     * @return Current format
     */
    OutputFormat getOutputFormat() const { return format_; }

    /**
     * Set the per-profile emission gates consulted by transformAttributes().
     */
    void setApsEmissionGates(const ApsEmissionGates& gates) { gates_ = gates; }

    /**
     * Get the currently active emission gates.
     */
    const ApsEmissionGates& getApsEmissionGates() const { return gates_; }

    /**
     * Transform CGM APS attributes to SVG data attributes
     *
     * @param apsId CGM APS identifier
     * @param apsName CGM APS name
     * @param cgmAttributes Map of CGM APS attributes (key=name, value=content)
     * @return AttributeSet with transformed attributes for all enabled formats
     */
    AttributeSet transformAttributes(
        const std::string& apsId,
        const std::string& apsName,
        const std::map<std::string, std::string>& cgmAttributes
    );

    /**
     * Add S1000D Issue 6 metadata to attribute set
     *
     * @param attrs AttributeSet to modify
     * @param metadata S1000D hotspot metadata
     */
    void addS1000DMetadata(
        AttributeSet& attrs,
        const HotspotMetadata& metadata
    );

    /**
     * Add vendor-specific attributes to attribute set
     *
     * @param attrs AttributeSet to modify
     * @param vendorName Vendor identifier ("rws", "boeing", "r4i")
     * @param vendorData Vendor-specific attribute data
     */
    void addVendorAttributes(
        AttributeSet& attrs,
        const std::string& vendorName,
        const std::map<std::string, std::string>& vendorData
    );

    /**
     * Generate final SVG attribute string from AttributeSet
     *
     * @param attrs AttributeSet to serialize
     * @return SVG attribute string (e.g., " data-aps-id=\"hot001\" data-apsid=\"hot001\"")
     */
    std::string generateSvgAttributes(const AttributeSet& attrs) const;

    /**
     * Parse output format from string
     *
     * @param formatStr Format string ("legacy", "s1000d6", "combined", "rws", "boeing", "r4i", "multi")
     * @return Corresponding OutputFormat enum value
     * @throws std::invalid_argument if format string is invalid
     */
    static OutputFormat parseOutputFormat(const std::string& formatStr);

    /**
     * Get output format as string
     *
     * @param format OutputFormat enum value
     * @return Format string
     */
    static std::string outputFormatToString(OutputFormat format);

private:
    OutputFormat format_;
    ApsEmissionGates gates_{};

    /**
     * Map CGM APS attribute name to target format attribute name
     *
     * @param cgmAttr CGM attribute name (e.g., "linkuri")
     * @param targetFormat Target format ("legacy", "s1000d", "rws", etc.)
     * @return Mapped attribute name (e.g., "data-aps-linkuri", "data-dm-ref", etc.)
     */
    std::string mapAttributeName(
        const std::string& cgmAttr,
        const std::string& targetFormat
    ) const;

    /**
     * Transform CGM APS attribute value to target format value
     * Context-aware transformation (e.g., linkuri → dm:// protocol for Boeing)
     *
     * @param cgmAttr CGM attribute name
     * @param value CGM attribute value
     * @param targetFormat Target format
     * @return Transformed value
     */
    std::string mapAttributeValue(
        const std::string& cgmAttr,
        const std::string& value,
        const std::string& targetFormat
    ) const;

    /**
     * Check if output format includes the legacy HTML5 data-aps-* compatibility
     * layer (Tier 1). True for every WebCGM-family format so the data-aps-* and
     * webcgm:* tiers always coexist for HTML5-host and IETP-viewer parity.
     */
    bool includeLegacy() const {
        return format_ == OutputFormat::WebCGM ||
               format_ == OutputFormat::S1000D_Issue6 ||
               format_ == OutputFormat::S1000D_Legacy ||
               format_ == OutputFormat::Multi;
    }

    /**
     * Check if output format includes S1000D Issue 6 attributes
     */
    bool includeS1000D() const {
        return format_ == OutputFormat::S1000D_Issue6 ||
               format_ == OutputFormat::Multi;
    }

    /**
     * Check if output format includes pre-v6 S1000D bare attributes (Issues 2.3 - 5.0)
     */
    bool includeS1000DLegacy() const {
        return format_ == OutputFormat::S1000D_Legacy ||
               format_ == OutputFormat::Multi;
    }

    /**
     * Check if output format includes the WebCGM 2.1 namespace baseline (Tier 0).
     * True for every format in the WebCGM family, gated off only when the profile
     * explicitly opts out via gates_.emit_webcgm_namespace (CALS, StandardSVG).
     */
    bool includeWebCgmNamespace() const {
        if (!gates_.emit_webcgm_namespace) return false;
        return format_ == OutputFormat::WebCGM ||
               format_ == OutputFormat::S1000D_Issue6 ||
               format_ == OutputFormat::S1000D_Legacy ||
               format_ == OutputFormat::RWS ||
               format_ == OutputFormat::Boeing ||
               format_ == OutputFormat::R4I ||
               format_ == OutputFormat::Multi;
    }

    /**
     * Check if output format includes vendor-specific attributes
     */
    bool includeVendor(const std::string& vendorName) const;

};

} // namespace svg
} // namespace opencgm

#endif // OPENCGM_SVG_ATTRIBUTE_MANAGER_H
