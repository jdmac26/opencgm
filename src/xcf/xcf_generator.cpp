#include "opencgm/xcf_generator.h"
#include "opencgm/commands/delimiter_commands.h"
#include "opencgm/commands/application_structure_commands.h"
#include "opencgm/commands/metafile_descriptor_commands.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>

namespace opencgm {

std::string XcfGenerator::generate(
    const CGMFile* cgmFile,
    const std::string& svgFilename,
    const std::vector<XcfHotspotRegion>& hotspots,
    const XcfOptions& options)
{
    std::ostringstream out;

    // Emit XML header and DTD
    emitXmlHeader(out, options);

    // Emit XCF root element
    emitXcfOpen(out, svgFilename, options);

    // Emit metadata section if enabled
    if (options.includeMetadata && cgmFile != nullptr) {
        emitMetadata(out, cgmFile, options);
    }

    // Emit hotspot definitions if enabled
    if (options.includeHotspots && !hotspots.empty()) {
        emitHotspots(out, hotspots, options);
    }

    // Close root element
    emitXcfClose(out);

    return out.str();
}

bool XcfGenerator::writeToFile(
    const CGMFile* cgmFile,
    const std::string& svgFilename,
    const std::string& xcfFilename,
    const std::vector<XcfHotspotRegion>& hotspots,
    const XcfOptions& options)
{
    std::string content = generate(cgmFile, svgFilename, hotspots, options);

    std::ofstream file(xcfFilename);
    if (!file.is_open()) {
        return false;
    }

    file << content;
    file.close();

    return file.good();
}

std::vector<XcfHotspotRegion> XcfGenerator::extractHotspots(const CGMFile* cgmFile)
{
    std::vector<XcfHotspotRegion> hotspots;

    if (cgmFile == nullptr) {
        return hotspots;
    }

    XcfHotspotRegion currentHotspot;
    bool inAps = false;

    for (const auto& cmd : cgmFile->commands()) {
        if (cmd->elementClass() == ClassCode::DelimiterElement) {
            switch (cmd->elementId()) {
                case 21: {  // BEGIN APPLICATION STRUCTURE
                    auto* aps = dynamic_cast<BeginApplicationStructure*>(cmd.get());
                    if (aps) {
                        // Only extract hotspots from grobject types (WebCGM graphics objects)
                        const std::string& type = aps->type();
                        if (type == "grobject" || type == "grnode" || type == "para" ||
                            type == "subpara" || type == "layer") {
                            inAps = true;
                            currentHotspot = XcfHotspotRegion();
                            currentHotspot.id = aps->identifier();
                            currentHotspot.attributes["aps-type"] = type;
                        }
                    }
                    break;
                }
                case 23:  // END APPLICATION STRUCTURE
                    if (inAps) {
                        // Only add hotspots that have at least an ID
                        if (!currentHotspot.id.empty()) {
                            hotspots.push_back(currentHotspot);
                        }
                        inAps = false;
                    }
                    break;
            }
        } else if (inAps && cmd->elementClass() == ClassCode::ApplicationStructureDescriptorElements) {
            if (cmd->elementId() == 1) {  // APPLICATION STRUCTURE ATTRIBUTE
                auto* attr = dynamic_cast<ApplicationStructureAttribute*>(cmd.get());
                if (attr) {
                    const std::string& attrType = attr->attributeType();

                    // Get the data - use structured text if available, otherwise use data()
                    std::string data;
                    if (attr->structuredText().has_value()) {
                        data = attr->structuredText().value();
                    } else {
                        data = attr->data();
                    }

                    // Process known WebCGM attribute types
                    if (attrType == "linkuri") {
                        currentHotspot.linkUri = data;
                    } else if (attrType == "screentip") {
                        currentHotspot.screenTip = data;
                    } else if (attrType == "name") {
                        currentHotspot.name = data;
                    } else if (attrType == "region") {
                        parseRegion(data, currentHotspot);
                    } else if (attrType == "viewcontext") {
                        currentHotspot.viewContext = data;
                    } else if (attrType == "content") {
                        // Content is used for embedded text/descriptions
                        if (currentHotspot.screenTip.empty()) {
                            currentHotspot.screenTip = data;
                        }
                    } else if (attrType == "layername") {
                        currentHotspot.attributes["layer"] = data;
                    } else if (attrType == "layerdesc") {
                        currentHotspot.attributes["layer-description"] = data;
                    } else {
                        // Store unrecognized attributes as data attributes
                        currentHotspot.attributes[attrType] = data;
                    }
                }
            }
        }
    }

    return hotspots;
}

void XcfGenerator::parseRegion(const std::string& data, XcfHotspotRegion& hotspot)
{
    // Parse SDR region data format
    // Region types per WebCGM spec:
    // 1 = rectangle (x1, y1, x2, y2)
    // 2 = polygon (x1, y1, x2, y2, ...)
    // 3 = ellipse (cx, cy, rx, ry)
    // 4 = elliptical arc (cx, cy, rx, ry, startAngle, endAngle)

    std::istringstream iss(data);
    int regionType = 0;
    iss >> regionType;

    switch (regionType) {
        case 1: {  // Rectangle
            hotspot.type = "rect";
            double x1, y1, x2, y2;
            if (iss >> x1 >> y1 >> x2 >> y2) {
                hotspot.coords.clear();
                hotspot.coords.push_back(CGMPoint(x1, y1));
                hotspot.coords.push_back(CGMPoint(x2, y2));
            }
            break;
        }
        case 2: {  // Polygon
            hotspot.type = "poly";
            hotspot.coords.clear();
            double x, y;
            while (iss >> x >> y) {
                hotspot.coords.push_back(CGMPoint(x, y));
            }
            break;
        }
        case 3: {  // Ellipse
            hotspot.type = "circle";  // Use circle for HTML compatibility
            double cx, cy, rx, ry;
            if (iss >> cx >> cy >> rx >> ry) {
                hotspot.coords.clear();
                hotspot.coords.push_back(CGMPoint(cx, cy));
                // Store radii as second point
                hotspot.coords.push_back(CGMPoint(rx, ry));
            }
            break;
        }
        case 4: {  // Elliptical arc
            hotspot.type = "poly";  // Approximate arc as polygon
            double cx, cy, rx, ry, startAngle, endAngle;
            if (iss >> cx >> cy >> rx >> ry >> startAngle >> endAngle) {
                hotspot.coords.clear();
                // Approximate arc with polygon points (every 10 degrees)
                const double pi = 3.14159265358979323846;
                double startRad = startAngle * pi / 180.0;
                double endRad = endAngle * pi / 180.0;
                double step = pi / 18.0;  // 10 degree steps
                for (double angle = startRad; angle <= endRad; angle += step) {
                    double x = cx + rx * std::cos(angle);
                    double y = cy + ry * std::sin(angle);
                    hotspot.coords.push_back(CGMPoint(x, y));
                }
                // Add final point
                double x = cx + rx * std::cos(endRad);
                double y = cy + ry * std::sin(endRad);
                hotspot.coords.push_back(CGMPoint(x, y));
            }
            break;
        }
        default:
            // Unknown region type - leave as empty
            break;
    }
}

void XcfGenerator::emitXmlHeader(std::ostringstream& out, const XcfOptions& options)
{
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << newline(options.prettyPrint);

    // WebCGM 2.1 DTD declaration
    out << "<!DOCTYPE webcgm PUBLIC \"-//W3C//DTD WebCGM " << options.dtdVersion
        << "//EN\" \"" << options.dtdUrl << "\">" << newline(options.prettyPrint);
}

void XcfGenerator::emitXcfOpen(std::ostringstream& out, const std::string& svgFilename, const XcfOptions& options)
{
    out << "<webcgm" << newline(options.prettyPrint);
    out << indent(1, options.prettyPrint) << "xmlns=\"http://www.w3.org/Graphics/WebCGM/2.1\"" << newline(options.prettyPrint);
    out << indent(1, options.prettyPrint) << "version=\"" << options.dtdVersion << "\"" << newline(options.prettyPrint);
    out << indent(1, options.prettyPrint) << "source=\"" << escapeXml(svgFilename) << "\">" << newline(options.prettyPrint);
}

void XcfGenerator::emitMetadata(std::ostringstream& out, const CGMFile* cgmFile, const XcfOptions& options)
{
    out << indent(1, options.prettyPrint) << "<metadata>" << newline(options.prettyPrint);

    if (cgmFile != nullptr) {
        // Extract metafile name
        if (!cgmFile->name().empty()) {
            out << indent(2, options.prettyPrint) << "<title>" << escapeXml(cgmFile->name())
                << "</title>" << newline(options.prettyPrint);
        }

        // Search for metafile description in commands
        for (const auto& cmd : cgmFile->commands()) {
            if (cmd->elementClass() == ClassCode::MetafileDescriptorElements) {
                if (cmd->elementId() == 2) {  // METAFILE DESCRIPTION
                    auto* desc = dynamic_cast<MetafileDescription*>(cmd.get());
                    if (desc && !desc->description().empty()) {
                        out << indent(2, options.prettyPrint) << "<description>"
                            << escapeXml(desc->description())
                            << "</description>" << newline(options.prettyPrint);
                    }
                } else if (cmd->elementId() == 1) {  // METAFILE VERSION
                    auto* ver = dynamic_cast<MetafileVersion*>(cmd.get());
                    if (ver) {
                        out << indent(2, options.prettyPrint) << "<cgm-version>"
                            << ver->version()
                            << "</cgm-version>" << newline(options.prettyPrint);
                    }
                }
            }
        }

        // Emit color model if set
        std::string colorModelStr;
        switch (cgmFile->colorModel()) {
            case ColorModel::RGB: colorModelStr = "RGB"; break;
            case ColorModel::CIELAB: colorModelStr = "CIELAB"; break;
            case ColorModel::CIELUV: colorModelStr = "CIELUV"; break;
            case ColorModel::CMYK: colorModelStr = "CMYK"; break;
            case ColorModel::RGB_RELATED: colorModelStr = "RGB-related"; break;
        }
        if (!colorModelStr.empty()) {
            out << indent(2, options.prettyPrint) << "<color-model>" << colorModelStr
                << "</color-model>" << newline(options.prettyPrint);
        }
    }

    // Emit generator info
    out << indent(2, options.prettyPrint) << "<generator>OpenCGM CGM Engine</generator>"
        << newline(options.prettyPrint);

    out << indent(1, options.prettyPrint) << "</metadata>" << newline(options.prettyPrint);
}

void XcfGenerator::emitHotspots(std::ostringstream& out, const std::vector<XcfHotspotRegion>& hotspots, const XcfOptions& options)
{
    out << indent(1, options.prettyPrint) << "<hotspots>" << newline(options.prettyPrint);

    for (const auto& hotspot : hotspots) {
        emitHotspot(out, hotspot, options.prettyPrint ? 2 : 0);
        if (options.prettyPrint) {
            out << newline(true);
        }
    }

    out << indent(1, options.prettyPrint) << "</hotspots>" << newline(options.prettyPrint);
}

void XcfGenerator::emitHotspot(std::ostringstream& out, const XcfHotspotRegion& hotspot, int indentLevel)
{
    bool prettyPrint = (indentLevel > 0);

    out << indent(indentLevel, prettyPrint) << "<area";
    out << " id=\"" << escapeXml(hotspot.id) << "\"";

    if (!hotspot.name.empty() && hotspot.name != hotspot.id) {
        out << " name=\"" << escapeXml(hotspot.name) << "\"";
    }

    // Emit shape and coordinates
    if (!hotspot.type.empty() && !hotspot.coords.empty()) {
        out << " shape=\"" << escapeXml(hotspot.type) << "\"";
        out << " coords=\"" << formatCoords(hotspot.coords) << "\"";
    }

    // Emit link if present
    if (!hotspot.linkUri.empty()) {
        out << " href=\"" << escapeXml(hotspot.linkUri) << "\"";
    }

    // Emit screentip/alt text
    if (!hotspot.screenTip.empty()) {
        out << " alt=\"" << escapeXml(hotspot.screenTip) << "\"";
        out << " title=\"" << escapeXml(hotspot.screenTip) << "\"";
    }

    // Emit view context
    if (!hotspot.viewContext.empty()) {
        out << " data-viewcontext=\"" << escapeXml(hotspot.viewContext) << "\"";
    }

    // Emit additional attributes
    for (const auto& attr : hotspot.attributes) {
        out << " data-" << escapeXml(attr.first) << "=\"" << escapeXml(attr.second) << "\"";
    }

    out << " />";
}

void XcfGenerator::emitBindings(std::ostringstream& out, const std::vector<XcfBinding>& bindings, const XcfOptions& options)
{
    if (bindings.empty()) {
        return;
    }

    out << indent(1, options.prettyPrint) << "<bindings>" << newline(options.prettyPrint);

    for (const auto& binding : bindings) {
        out << indent(2, options.prettyPrint) << "<bind";
        out << " ref=\"" << escapeXml(binding.cgmElementId) << "\"";
        out << " type=\"" << escapeXml(binding.bindingType) << "\"";
        out << " value=\"" << escapeXml(binding.bindingValue) << "\"";
        out << " />" << newline(options.prettyPrint);
    }

    out << indent(1, options.prettyPrint) << "</bindings>" << newline(options.prettyPrint);
}

void XcfGenerator::emitXcfClose(std::ostringstream& out)
{
    out << "</webcgm>";
}

std::string XcfGenerator::escapeXml(const std::string& text) const
{
    std::string result;
    result.reserve(text.size() + text.size() / 10); // Reserve ~10% more for escape sequences

    for (char c : text) {
        switch (c) {
            case '&':  result += "&amp;"; break;
            case '<':  result += "&lt;"; break;
            case '>':  result += "&gt;"; break;
            case '"':  result += "&quot;"; break;
            case '\'': result += "&apos;"; break;
            default:   result += c; break;
        }
    }

    return result;
}

std::string XcfGenerator::formatCoords(const std::vector<CGMPoint>& coords) const
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    bool first = true;
    for (const auto& pt : coords) {
        if (!first) {
            oss << ",";
        }
        oss << pt.x() << "," << pt.y();
        first = false;
    }

    return oss.str();
}

} // namespace opencgm
