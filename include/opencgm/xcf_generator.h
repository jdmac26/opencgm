#ifndef OPENCGM_XCF_GENERATOR_H
#define OPENCGM_XCF_GENERATOR_H

#include "cgm_file.h"
#include "cgm_point.h"
#include <string>
#include <vector>
#include <map>
#include <sstream>

namespace opencgm {

/**
 * @brief XCF (XML Companion File) generation options
 */
struct XcfOptions {
    bool includeHotspots = true;       // Include hotspot definitions
    bool includeMetadata = true;       // Include metafile metadata
    std::string dtdVersion = "2.1";    // WebCGM 2.1 DTD version
    std::string dtdUrl = "http://www.w3.org/Graphics/WebCGM/DTD/2.1/webcgm21.dtd";
    bool prettyPrint = true;           // Format with indentation
};

/**
 * @brief Hotspot region for XCF output
 */
struct XcfHotspotRegion {
    std::string id;                    // Hotspot identifier
    std::string name;                  // Hotspot name
    std::string type;                  // "rect", "poly", "circle"
    std::vector<CGMPoint> coords;      // Region coordinates
    std::string linkUri;               // Link target URI
    std::string screenTip;             // Tooltip text
    std::string viewContext;           // View context reference
    std::map<std::string, std::string> attributes; // Additional attributes
};

/**
 * @brief Binding entry for XCF (associates CGM elements with external data)
 */
struct XcfBinding {
    std::string cgmElementId;          // CGM element reference
    std::string bindingType;           // Type of binding (e.g., "linkuri", "screentip")
    std::string bindingValue;          // Binding value
};

/**
 * @brief XML Companion File (XCF) generator for WebCGM/S1000D
 *
 * Generates XCF files alongside SVG output to preserve hotspot
 * and metadata information from CGM Application Structures.
 *
 * XCF format follows WebCGM 2.1 DTD specification.
 */
class XcfGenerator {
public:
    /**
     * @brief Generate XCF content from CGM file data
     *
     * @param cgmFile The source CGM file
     * @param svgFilename The corresponding SVG filename (for reference)
     * @param hotspots Vector of hotspot regions extracted from CGM
     * @param options XCF generation options
     * @return XCF XML content as string
     */
    std::string generate(
        const CGMFile* cgmFile,
        const std::string& svgFilename,
        const std::vector<XcfHotspotRegion>& hotspots,
        const XcfOptions& options = XcfOptions());

    /**
     * @brief Generate XCF file and write to disk
     *
     * @param cgmFile The source CGM file
     * @param svgFilename The corresponding SVG filename
     * @param xcfFilename Output XCF filename
     * @param hotspots Vector of hotspot regions
     * @param options XCF generation options
     * @return true if file was written successfully
     */
    bool writeToFile(
        const CGMFile* cgmFile,
        const std::string& svgFilename,
        const std::string& xcfFilename,
        const std::vector<XcfHotspotRegion>& hotspots,
        const XcfOptions& options = XcfOptions());

    /**
     * @brief Extract hotspot regions from CGM Application Structures
     *
     * @param cgmFile The CGM file to analyze
     * @return Vector of hotspot regions found in the file
     */
    std::vector<XcfHotspotRegion> extractHotspots(const CGMFile* cgmFile);

private:
    /**
     * @brief Emit XML header and DTD declaration
     */
    void emitXmlHeader(std::ostringstream& out, const XcfOptions& options);

    /**
     * @brief Emit XCF root element opening tag
     */
    void emitXcfOpen(std::ostringstream& out, const std::string& svgFilename, const XcfOptions& options);

    /**
     * @brief Emit metadata section
     */
    void emitMetadata(std::ostringstream& out, const CGMFile* cgmFile, const XcfOptions& options);

    /**
     * @brief Emit hotspot definitions
     */
    void emitHotspots(std::ostringstream& out, const std::vector<XcfHotspotRegion>& hotspots, const XcfOptions& options);

    /**
     * @brief Emit a single hotspot element
     */
    void emitHotspot(std::ostringstream& out, const XcfHotspotRegion& hotspot, int indent);

    /**
     * @brief Emit binding section for external references
     */
    void emitBindings(std::ostringstream& out, const std::vector<XcfBinding>& bindings, const XcfOptions& options);

    /**
     * @brief Emit XCF root element closing tag
     */
    void emitXcfClose(std::ostringstream& out);

    /**
     * @brief Escape XML special characters
     */
    std::string escapeXml(const std::string& text) const;

    /**
     * @brief Format coordinates for XCF output
     */
    std::string formatCoords(const std::vector<CGMPoint>& coords) const;

    /**
     * @brief Parse region data from SDR format into hotspot structure
     */
    void parseRegion(const std::string& data, XcfHotspotRegion& hotspot);

    /**
     * @brief Generate indentation string
     */
    std::string indent(int level, bool prettyPrint) const {
        return prettyPrint ? std::string(level * 2, ' ') : "";
    }

    /**
     * @brief Generate newline if pretty printing
     */
    std::string newline(bool prettyPrint) const {
        return prettyPrint ? "\n" : "";
    }
};

} // namespace opencgm

#endif // OPENCGM_XCF_GENERATOR_H
