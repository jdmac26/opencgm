#ifndef OPENCGM_XCF_PARSER_H
#define OPENCGM_XCF_PARSER_H

#include "xcf_generator.h"  // Reuse XcfHotspotRegion, XcfBinding structures
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <functional>

namespace opencgm {

/**
 * @brief Metadata extracted from XCF file
 */
struct XcfMetadata {
    std::string title;
    std::string description;
    std::string cgmVersion;
    std::string colorModel;
    std::string generator;
    std::map<std::string, std::string> customMetadata;
};

/**
 * @brief Style property from XCF (WebCGM 2.1 style properties)
 */
struct XcfStyleProperty {
    std::string property;   // CSS-like property name (e.g., "stroke", "fill")
    std::string value;      // Property value
};

/**
 * @brief Binding mode for XCF elements
 */
enum class XcfBindingMode {
    ById,       // bindById - match by APS identifier
    ByName      // bindByName - match by APS name attribute
};

/**
 * @brief Extended binding with mode information
 */
struct XcfExtendedBinding {
    XcfBindingMode mode = XcfBindingMode::ById;
    std::string targetRef;              // Target APS identifier or name
    std::string linkUri;                // Link URI override
    std::string screenTip;              // Screentip override
    std::string name;                   // Name override
    std::string content;                // Content override
    std::string visibility;             // Visibility override ("on", "off", "inherit")
    std::vector<XcfStyleProperty> styleProperties;  // Style overrides
    std::map<std::string, std::string> customAttributes;  // Additional attributes
};

/**
 * @brief Parsed XCF file data structure
 */
struct XcfData {
    std::string version;                // XCF DTD version (e.g., "2.1")
    std::string sourceFile;             // Reference to source CGM/SVG file
    XcfMetadata metadata;               // Metadata section
    std::vector<XcfHotspotRegion> hotspots;     // Hotspot definitions
    std::vector<XcfExtendedBinding> bindings;   // Bindings to CGM APS elements

    /**
     * @brief Find binding by APS identifier
     * @param apsId The APS identifier to match
     * @return Pointer to binding if found, nullptr otherwise
     */
    const XcfExtendedBinding* findBindingById(const std::string& apsId) const;

    /**
     * @brief Find binding by APS name
     * @param apsName The APS name to match
     * @return Pointer to binding if found, nullptr otherwise
     */
    const XcfExtendedBinding* findBindingByName(const std::string& apsName) const;

    /**
     * @brief Find hotspot by ID
     * @param id The hotspot ID to match
     * @return Pointer to hotspot if found, nullptr otherwise
     */
    const XcfHotspotRegion* findHotspotById(const std::string& id) const;

    /**
     * @brief Check if XCF data is empty (no meaningful content)
     */
    bool isEmpty() const;
};

/**
 * @brief Parser result with error information
 */
struct XcfParseResult {
    bool success = false;
    std::string errorMessage;
    int errorLine = 0;
    int errorColumn = 0;
    XcfData data;
};

/**
 * @brief XML Companion File (XCF) parser for WebCGM 2.1
 *
 * Parses XCF files and extracts hotspot definitions, bindings,
 * and metadata for merging with CGM conversion output.
 *
 * Supports WebCGM 2.1 DTD structure including:
 * - bindById bindings (match by APS identifier)
 * - bindByName bindings (match by APS name attribute)
 * - Style properties
 * - Hotspot region definitions
 */
class XcfParser {
public:
    XcfParser() = default;
    ~XcfParser() = default;

    /**
     * @brief Parse XCF from file path
     *
     * @param filePath Path to XCF file
     * @return Parse result with data and error info
     */
    XcfParseResult parseFile(const std::string& filePath);

    /**
     * @brief Parse XCF from string content
     *
     * @param xmlContent XCF XML content as string
     * @return Parse result with data and error info
     */
    XcfParseResult parseString(const std::string& xmlContent);

    /**
     * @brief Check if a file is likely an XCF file (by extension or content)
     *
     * @param filePath Path to check
     * @return true if file appears to be XCF
     */
    static bool isXcfFile(const std::string& filePath);

    /**
     * @brief Auto-detect companion XCF file for a given CGM
     *
     * Searches for XCF files matching the CGM filename:
     * - Same name with .xcf extension
     * - Same name with .xml extension
     * - Same name with _xcf.xml suffix
     *
     * @param cgmFilePath Path to CGM file
     * @return Path to XCF file if found, empty string otherwise
     */
    static std::string findCompanionXcf(const std::string& cgmFilePath);

private:
    // Simple XML element structure for parsing
    struct XmlElement {
        std::string name;
        std::map<std::string, std::string> attributes;
        std::string text;
        std::vector<XmlElement> children;
    };

    // Parsing state
    std::string content_;
    size_t pos_ = 0;
    int line_ = 1;
    int column_ = 1;
    std::string lastError_;

    // Core parsing methods
    bool parseXml(XmlElement& root);
    bool parseElement(XmlElement& element);
    bool parseAttributes(std::map<std::string, std::string>& attrs);
    bool parseContent(XmlElement& element);
    std::string parseAttributeValue();
    std::string parseText();
    std::string parseElementName();

    // Character handling
    char peek() const;
    char advance();
    void skipWhitespace();
    bool skipComment();
    bool skipPI();  // Processing instruction
    bool skipDoctype();
    bool match(const std::string& str);
    bool matchChar(char c);

    // XML entity decoding
    std::string decodeEntities(const std::string& text) const;

    // XCF structure extraction
    void extractXcfData(const XmlElement& root, XcfData& data);
    void extractMetadata(const XmlElement& element, XcfMetadata& metadata);
    void extractHotspots(const XmlElement& element, std::vector<XcfHotspotRegion>& hotspots);
    void extractHotspot(const XmlElement& element, XcfHotspotRegion& hotspot);
    void extractBindings(const XmlElement& element, std::vector<XcfExtendedBinding>& bindings);
    void extractBinding(const XmlElement& element, XcfExtendedBinding& binding);
    void extractStyleProperties(const XmlElement& element, std::vector<XcfStyleProperty>& styles);

    // Error handling
    void setError(const std::string& message);
};

/**
 * @brief Merges XCF data with CGM APS attributes during conversion
 *
 * This class is used by the SVG converter to apply XCF overrides
 * to Application Structure attributes during rendering.
 */
class XcfMerger {
public:
    XcfMerger() = default;
    ~XcfMerger() = default;

    /**
     * @brief Set the XCF data to use for merging
     *
     * @param xcfData Parsed XCF data
     */
    void setXcfData(const XcfData& xcfData);

    /**
     * @brief Set XCF data from file path
     *
     * @param xcfFilePath Path to XCF file
     * @return true if file was parsed successfully
     */
    bool loadXcfFile(const std::string& xcfFilePath);

    /**
     * @brief Check if XCF data is loaded
     */
    bool hasXcfData() const { return xcfLoaded_; }

    /**
     * @brief Get merged attributes for an APS element
     *
     * Looks up binding by ID, then by name, and merges XCF overrides
     * with existing CGM attributes.
     *
     * @param apsId APS identifier from CGM
     * @param apsName APS name from CGM (may be empty)
     * @param cgmAttributes Current attributes from CGM
     * @return Merged attributes map
     */
    std::map<std::string, std::string> getMergedAttributes(
        const std::string& apsId,
        const std::string& apsName,
        const std::map<std::string, std::string>& cgmAttributes) const;

    /**
     * @brief Get hotspot region from XCF if defined
     *
     * @param apsId APS identifier
     * @return Hotspot region if found, std::nullopt otherwise
     */
    std::optional<XcfHotspotRegion> getHotspotRegion(const std::string& apsId) const;

    /**
     * @brief Get style properties from XCF binding
     *
     * @param apsId APS identifier
     * @param apsName APS name (fallback)
     * @return Style properties vector (empty if none)
     */
    std::vector<XcfStyleProperty> getStyleProperties(
        const std::string& apsId,
        const std::string& apsName) const;

    /**
     * @brief Get the loaded XCF metadata
     */
    const XcfMetadata& getMetadata() const { return xcfData_.metadata; }

private:
    XcfData xcfData_;
    bool xcfLoaded_ = false;

    // Index maps for fast lookup
    std::map<std::string, size_t> bindingByIdIndex_;
    std::map<std::string, size_t> bindingByNameIndex_;
    std::map<std::string, size_t> hotspotByIdIndex_;

    void buildIndices();
};

} // namespace opencgm

#endif // OPENCGM_XCF_PARSER_H
