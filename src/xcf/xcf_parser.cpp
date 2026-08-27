#include "opencgm/xcf_parser.h"
#include "opencgm/security_limits.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace opencgm {

// ============================================================================
// XcfData implementation
// ============================================================================

const XcfExtendedBinding* XcfData::findBindingById(const std::string& apsId) const
{
    for (const auto& binding : bindings) {
        if (binding.mode == XcfBindingMode::ById && binding.targetRef == apsId) {
            return &binding;
        }
    }
    return nullptr;
}

const XcfExtendedBinding* XcfData::findBindingByName(const std::string& apsName) const
{
    for (const auto& binding : bindings) {
        if (binding.mode == XcfBindingMode::ByName && binding.targetRef == apsName) {
            return &binding;
        }
    }
    return nullptr;
}

const XcfHotspotRegion* XcfData::findHotspotById(const std::string& id) const
{
    for (const auto& hotspot : hotspots) {
        if (hotspot.id == id) {
            return &hotspot;
        }
    }
    return nullptr;
}

bool XcfData::isEmpty() const
{
    return hotspots.empty() && bindings.empty() &&
           metadata.title.empty() && metadata.description.empty();
}

// ============================================================================
// XcfParser implementation
// ============================================================================

XcfParseResult XcfParser::parseFile(const std::string& filePath)
{
    XcfParseResult result;

    // Read file content
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        result.errorMessage = "Failed to open file: " + filePath;
        return result;
    }

    file.seekg(0, std::ios::end);
    const std::streamoff fileSize = file.tellg();
    if (fileSize < 0) {
        result.errorMessage = "Failed to read file size: " + filePath;
        return result;
    }
    if (static_cast<size_t>(fileSize) > security::MAX_STRING_LENGTH) {
        result.errorMessage = "XCF file exceeds maximum supported size";
        return result;
    }
    file.seekg(0, std::ios::beg);

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    return parseString(buffer.str());
}

XcfParseResult XcfParser::parseString(const std::string& xmlContent)
{
    XcfParseResult result;

    // Initialize parsing state
    content_ = xmlContent;
    pos_ = 0;
    line_ = 1;
    column_ = 1;
    lastError_.clear();

    // Skip BOM if present
    if (content_.size() >= 3 &&
        static_cast<unsigned char>(content_[0]) == 0xEF &&
        static_cast<unsigned char>(content_[1]) == 0xBB &&
        static_cast<unsigned char>(content_[2]) == 0xBF) {
        pos_ = 3;
    }

    // Skip XML declaration and DOCTYPE
    skipWhitespace();
    while (skipPI() || skipDoctype() || skipComment()) {
        skipWhitespace();
    }

    // Parse root element
    XmlElement root;
    if (!parseElement(root)) {
        result.errorMessage = lastError_;
        result.errorLine = line_;
        result.errorColumn = column_;
        return result;
    }

    // Extract XCF data from parsed XML
    extractXcfData(root, result.data);
    result.success = true;

    return result;
}

bool XcfParser::isXcfFile(const std::string& filePath)
{
    std::filesystem::path path(filePath);
    std::string ext = path.extension().string();

    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (ext == ".xcf") {
        return true;
    }

    // Check for .xml files that might be XCF
    if (ext == ".xml") {
        std::ifstream file(filePath);
        if (file.is_open()) {
            std::string line;
            int lineCount = 0;
            while (std::getline(file, line) && lineCount < 10) {
                if (line.find("webcgm") != std::string::npos ||
                    line.find("WebCGM") != std::string::npos) {
                    return true;
                }
                lineCount++;
            }
        }
    }

    return false;
}

std::string XcfParser::findCompanionXcf(const std::string& cgmFilePath)
{
    std::filesystem::path cgmPath(cgmFilePath);
    std::filesystem::path dir = cgmPath.parent_path();
    std::string stem = cgmPath.stem().string();

    // Try different XCF naming conventions
    std::vector<std::string> candidates = {
        (dir / (stem + ".xcf")).string(),
        (dir / (stem + ".xml")).string(),
        (dir / (stem + "_xcf.xml")).string(),
        (dir / (stem + ".XCF")).string(),
        (dir / (stem + ".XML")).string()
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            // For .xml files, verify it's actually an XCF
            std::filesystem::path candidatePath(candidate);
            std::string ext = candidatePath.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            if (ext == ".xcf" || isXcfFile(candidate)) {
                return candidate;
            }
        }
    }

    return "";
}

// Core XML parsing

char XcfParser::peek() const
{
    if (pos_ >= content_.size()) {
        return '\0';
    }
    return content_[pos_];
}

char XcfParser::advance()
{
    if (pos_ >= content_.size()) {
        return '\0';
    }

    char c = content_[pos_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

void XcfParser::skipWhitespace()
{
    while (pos_ < content_.size() && std::isspace(static_cast<unsigned char>(content_[pos_]))) {
        advance();
    }
}

bool XcfParser::skipComment()
{
    if (match("<!--")) {
        // Advance past "<!--"
        for (int i = 0; i < 4; i++) advance();

        while (pos_ + 2 < content_.size()) {
            if (content_[pos_] == '-' && content_[pos_ + 1] == '-' && content_[pos_ + 2] == '>') {
                advance(); advance(); advance();
                return true;
            }
            advance();
        }
        setError("Unterminated comment");
        return false;
    }
    return false;
}

bool XcfParser::skipPI()
{
    if (match("<?")) {
        // Advance past "<?"
        advance(); advance();

        while (pos_ + 1 < content_.size()) {
            if (content_[pos_] == '?' && content_[pos_ + 1] == '>') {
                advance(); advance();
                return true;
            }
            advance();
        }
        setError("Unterminated processing instruction");
        return false;
    }
    return false;
}

bool XcfParser::skipDoctype()
{
    if (match("<!DOCTYPE") || match("<!doctype")) {
        // Skip until closing >
        int depth = 1;
        while (pos_ < content_.size() && depth > 0) {
            char c = advance();
            if (c == '<') depth++;
            else if (c == '>') depth--;
        }
        return true;
    }
    return false;
}

bool XcfParser::match(const std::string& str)
{
    if (pos_ + str.size() > content_.size()) {
        return false;
    }
    return content_.compare(pos_, str.size(), str) == 0;
}

bool XcfParser::matchChar(char c)
{
    if (peek() == c) {
        advance();
        return true;
    }
    return false;
}

bool XcfParser::parseXml(XmlElement& root)
{
    skipWhitespace();
    while (skipPI() || skipDoctype() || skipComment()) {
        skipWhitespace();
    }
    return parseElement(root);
}

bool XcfParser::parseElement(XmlElement& element)
{
    skipWhitespace();

    if (!matchChar('<')) {
        setError("Expected '<'");
        return false;
    }

    // Check for comment
    if (match("!--")) {
        // Skip back and handle comment
        pos_--;
        if (!skipComment()) {
            return false;
        }
        skipWhitespace();
        return parseElement(element);
    }

    // Parse element name
    element.name = parseElementName();
    if (element.name.empty()) {
        setError("Expected element name");
        return false;
    }

    // Parse attributes
    if (!parseAttributes(element.attributes)) {
        return false;
    }

    skipWhitespace();

    // Check for self-closing element
    if (match("/>")) {
        advance(); advance();
        return true;
    }

    if (!matchChar('>')) {
        setError("Expected '>' or '/>'");
        return false;
    }

    // Parse content (text and child elements)
    if (!parseContent(element)) {
        return false;
    }

    // Parse closing tag
    skipWhitespace();
    if (!match("</")) {
        setError("Expected closing tag '</" + element.name + ">'");
        return false;
    }
    advance(); advance();

    std::string closingName = parseElementName();
    if (closingName != element.name) {
        setError("Mismatched closing tag: expected '</" + element.name + ">', got '</" + closingName + ">'");
        return false;
    }

    skipWhitespace();
    if (!matchChar('>')) {
        setError("Expected '>' in closing tag");
        return false;
    }

    return true;
}

std::string XcfParser::parseElementName()
{
    std::string name;
    while (pos_ < content_.size()) {
        char c = peek();
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == ':' || c == '.') {
            name += advance();
        } else {
            break;
        }
    }
    return name;
}

bool XcfParser::parseAttributes(std::map<std::string, std::string>& attrs)
{
    while (true) {
        skipWhitespace();

        // Check for end of attributes
        if (peek() == '>' || peek() == '/' || peek() == '\0') {
            break;
        }

        // Parse attribute name
        std::string attrName = parseElementName();
        if (attrName.empty()) {
            break;
        }

        skipWhitespace();

        if (!matchChar('=')) {
            setError("Expected '=' after attribute name");
            return false;
        }

        skipWhitespace();

        std::string attrValue = parseAttributeValue();
        attrs[attrName] = decodeEntities(attrValue);
    }

    return true;
}

std::string XcfParser::parseAttributeValue()
{
    char quote = peek();
    if (quote != '"' && quote != '\'') {
        setError("Expected quote for attribute value");
        return "";
    }
    advance();

    std::string value;
    while (pos_ < content_.size() && peek() != quote) {
        value += advance();
    }

    if (!matchChar(quote)) {
        setError("Unterminated attribute value");
    }

    return value;
}

bool XcfParser::parseContent(XmlElement& element)
{
    std::string text;

    while (pos_ < content_.size()) {
        if (match("</")) {
            // Closing tag - we're done
            element.text = decodeEntities(text);
            return true;
        }

        if (match("<!--")) {
            // Comment - skip it
            if (!skipComment()) {
                return false;
            }
            continue;
        }

        if (match("<![CDATA[")) {
            // CDATA section
            advance(); advance(); advance(); // <![
            advance(); advance(); advance(); advance(); advance(); advance(); // CDATA[

            while (pos_ + 2 < content_.size()) {
                if (content_[pos_] == ']' && content_[pos_ + 1] == ']' && content_[pos_ + 2] == '>') {
                    advance(); advance(); advance();
                    break;
                }
                text += advance();
            }
            continue;
        }

        if (peek() == '<') {
            // Store any accumulated text
            if (!text.empty()) {
                element.text = decodeEntities(text);
                text.clear();
            }

            // Child element
            XmlElement child;
            if (!parseElement(child)) {
                return false;
            }
            element.children.push_back(std::move(child));
        } else {
            text += advance();
        }
    }

    setError("Unexpected end of document");
    return false;
}

std::string XcfParser::decodeEntities(const std::string& text) const
{
    std::string result;
    result.reserve(text.size());

    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '&') {
            if (text.compare(i, 4, "&lt;") == 0) {
                result += '<';
                i += 3;
            } else if (text.compare(i, 4, "&gt;") == 0) {
                result += '>';
                i += 3;
            } else if (text.compare(i, 5, "&amp;") == 0) {
                result += '&';
                i += 4;
            } else if (text.compare(i, 6, "&quot;") == 0) {
                result += '"';
                i += 5;
            } else if (text.compare(i, 6, "&apos;") == 0) {
                result += '\'';
                i += 5;
            } else if (text.compare(i, 2, "&#") == 0) {
                // Numeric entity
                size_t end = text.find(';', i);
                if (end != std::string::npos) {
                    std::string numStr = text.substr(i + 2, end - i - 2);
                    try {
                        unsigned long codePoint = 0;
                        if (!numStr.empty() && (numStr[0] == 'x' || numStr[0] == 'X')) {
                            codePoint = std::stoul(numStr.substr(1), nullptr, 16);
                        } else {
                            codePoint = std::stoul(numStr);
                        }

                        if (codePoint <= 0x7F) {
                            result += static_cast<char>(codePoint);
                        } else if (codePoint <= 0x7FF) {
                            result += static_cast<char>(0xC0 | (codePoint >> 6));
                            result += static_cast<char>(0x80 | (codePoint & 0x3F));
                        } else if (codePoint <= 0xFFFF) {
                            result += static_cast<char>(0xE0 | (codePoint >> 12));
                            result += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                            result += static_cast<char>(0x80 | (codePoint & 0x3F));
                        } else if (codePoint <= 0x10FFFF) {
                            result += static_cast<char>(0xF0 | (codePoint >> 18));
                            result += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
                            result += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                            result += static_cast<char>(0x80 | (codePoint & 0x3F));
                        } else {
                            result += '\xEF';
                            result += '\xBF';
                            result += '\xBD';
                        }
                    } catch (...) {
                        result += '\xEF';
                        result += '\xBF';
                        result += '\xBD';
                    }
                    i = end;
                } else {
                    result += text[i];
                }
            } else {
                result += text[i];
            }
        } else {
            result += text[i];
        }
    }

    return result;
}

void XcfParser::setError(const std::string& message)
{
    lastError_ = message + " at line " + std::to_string(line_) + ", column " + std::to_string(column_);
}

// XCF structure extraction

void XcfParser::extractXcfData(const XmlElement& root, XcfData& data)
{
    // Check for webcgm root element (case-insensitive)
    std::string rootName = root.name;
    std::transform(rootName.begin(), rootName.end(), rootName.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (rootName != "webcgm") {
        return;
    }

    // Extract root attributes
    auto versionIt = root.attributes.find("version");
    if (versionIt != root.attributes.end()) {
        data.version = versionIt->second;
    }

    auto sourceIt = root.attributes.find("source");
    if (sourceIt != root.attributes.end()) {
        data.sourceFile = sourceIt->second;
    }

    // Process child elements
    for (const auto& child : root.children) {
        std::string childName = child.name;
        std::transform(childName.begin(), childName.end(), childName.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (childName == "metadata") {
            extractMetadata(child, data.metadata);
        } else if (childName == "hotspots") {
            extractHotspots(child, data.hotspots);
        } else if (childName == "bindings" || childName == "bindbyid" || childName == "bindbyname") {
            extractBindings(child, data.bindings);
        } else if (childName == "grobject" || childName == "grnode" || childName == "layer" ||
                   childName == "para" || childName == "subpara") {
            // WebCGM 2.1 uses grobject/grnode elements directly for bindings
            XcfExtendedBinding binding;
            extractBinding(child, binding);
            if (!binding.targetRef.empty()) {
                data.bindings.push_back(std::move(binding));
            }
        }
    }
}

void XcfParser::extractMetadata(const XmlElement& element, XcfMetadata& metadata)
{
    for (const auto& child : element.children) {
        std::string childName = child.name;
        std::transform(childName.begin(), childName.end(), childName.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (childName == "title") {
            metadata.title = child.text;
        } else if (childName == "description") {
            metadata.description = child.text;
        } else if (childName == "cgm-version") {
            metadata.cgmVersion = child.text;
        } else if (childName == "color-model") {
            metadata.colorModel = child.text;
        } else if (childName == "generator") {
            metadata.generator = child.text;
        } else {
            // Store unknown elements as custom metadata
            if (!child.text.empty()) {
                metadata.customMetadata[child.name] = child.text;
            }
        }
    }
}

void XcfParser::extractHotspots(const XmlElement& element, std::vector<XcfHotspotRegion>& hotspots)
{
    for (const auto& child : element.children) {
        std::string childName = child.name;
        std::transform(childName.begin(), childName.end(), childName.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (childName == "area" || childName == "hotspot") {
            XcfHotspotRegion hotspot;
            extractHotspot(child, hotspot);
            if (!hotspot.id.empty()) {
                hotspots.push_back(std::move(hotspot));
            }
        }
    }
}

void XcfParser::extractHotspot(const XmlElement& element, XcfHotspotRegion& hotspot)
{
    // Extract standard attributes
    for (const auto& attr : element.attributes) {
        std::string attrName = attr.first;
        std::transform(attrName.begin(), attrName.end(), attrName.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (attrName == "id" || attrName == "apsid") {
            hotspot.id = attr.second;
        } else if (attrName == "name") {
            hotspot.name = attr.second;
        } else if (attrName == "shape") {
            hotspot.type = attr.second;
        } else if (attrName == "coords") {
            // Parse coordinate string
            std::istringstream iss(attr.second);
            std::string token;
            std::vector<double> values;
            while (std::getline(iss, token, ',')) {
                try {
                    values.push_back(std::stod(token));
                } catch (...) {
                    // Ignore parse errors
                }
            }
            // Convert to points
            for (size_t i = 0; i + 1 < values.size(); i += 2) {
                hotspot.coords.push_back(CGMPoint(values[i], values[i + 1]));
            }
        } else if (attrName == "href" || attrName == "linkuri") {
            hotspot.linkUri = attr.second;
        } else if (attrName == "alt" || attrName == "title" || attrName == "screentip") {
            if (hotspot.screenTip.empty()) {
                hotspot.screenTip = attr.second;
            }
        } else if (attrName == "data-viewcontext" || attrName == "viewcontext") {
            hotspot.viewContext = attr.second;
        } else if (attrName.substr(0, 5) == "data-") {
            // Store data-* attributes
            hotspot.attributes[attrName.substr(5)] = attr.second;
        }
    }
}

void XcfParser::extractBindings(const XmlElement& element, std::vector<XcfExtendedBinding>& bindings)
{
    // Determine binding mode from element name
    std::string elemName = element.name;
    std::transform(elemName.begin(), elemName.end(), elemName.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    XcfBindingMode defaultMode = XcfBindingMode::ById;
    if (elemName == "bindbyname") {
        defaultMode = XcfBindingMode::ByName;
    }

    for (const auto& child : element.children) {
        std::string childName = child.name;
        std::transform(childName.begin(), childName.end(), childName.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (childName == "bind" || childName == "grobject" || childName == "grnode" ||
            childName == "layer" || childName == "para" || childName == "subpara") {
            XcfExtendedBinding binding;
            binding.mode = defaultMode;
            extractBinding(child, binding);
            if (!binding.targetRef.empty()) {
                bindings.push_back(std::move(binding));
            }
        }
    }
}

void XcfParser::extractBinding(const XmlElement& element, XcfExtendedBinding& binding)
{
    // Extract binding target from attributes
    for (const auto& attr : element.attributes) {
        std::string attrName = attr.first;
        std::transform(attrName.begin(), attrName.end(), attrName.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (attrName == "ref" || attrName == "apsid" || attrName == "id") {
            binding.targetRef = attr.second;
            binding.mode = XcfBindingMode::ById;
        } else if (attrName == "name" || attrName == "apsname") {
            if (binding.targetRef.empty()) {
                binding.targetRef = attr.second;
                binding.mode = XcfBindingMode::ByName;
            }
            binding.name = attr.second;
        } else if (attrName == "linkuri" || attrName == "href") {
            binding.linkUri = attr.second;
        } else if (attrName == "screentip" || attrName == "title" || attrName == "alt") {
            binding.screenTip = attr.second;
        } else if (attrName == "content") {
            binding.content = attr.second;
        } else if (attrName == "visibility") {
            binding.visibility = attr.second;
        } else if (attrName == "type") {
            // Skip type attribute (used for binding type, not custom attr)
        } else if (attrName == "value") {
            // Handle legacy bind format: type + value
            auto typeIt = element.attributes.find("type");
            if (typeIt != element.attributes.end()) {
                std::string bindType = typeIt->second;
                std::transform(bindType.begin(), bindType.end(), bindType.begin(),
                               [](unsigned char c) { return std::tolower(c); });

                if (bindType == "linkuri") {
                    binding.linkUri = attr.second;
                } else if (bindType == "screentip") {
                    binding.screenTip = attr.second;
                } else if (bindType == "name") {
                    binding.name = attr.second;
                } else if (bindType == "content") {
                    binding.content = attr.second;
                } else if (bindType == "visibility") {
                    binding.visibility = attr.second;
                }
            }
        } else {
            // Store as custom attribute
            binding.customAttributes[attr.first] = attr.second;
        }
    }

    // Process child elements for nested content
    for (const auto& child : element.children) {
        std::string childName = child.name;
        std::transform(childName.begin(), childName.end(), childName.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (childName == "linkuri") {
            binding.linkUri = child.text;
            if (binding.linkUri.empty()) {
                auto hrefIt = child.attributes.find("href");
                if (hrefIt != child.attributes.end()) {
                    binding.linkUri = hrefIt->second;
                }
            }
        } else if (childName == "screentip") {
            binding.screenTip = child.text;
        } else if (childName == "name") {
            if (binding.name.empty()) {
                binding.name = child.text;
            }
        } else if (childName == "content") {
            binding.content = child.text;
        } else if (childName == "visibility") {
            binding.visibility = child.text;
        } else if (childName == "style" || childName == "styleproperties") {
            extractStyleProperties(child, binding.styleProperties);
        }
    }
}

void XcfParser::extractStyleProperties(const XmlElement& element, std::vector<XcfStyleProperty>& styles)
{
    // Handle CSS-like style string in text content
    if (!element.text.empty()) {
        std::istringstream iss(element.text);
        std::string property;
        while (std::getline(iss, property, ';')) {
            size_t colonPos = property.find(':');
            if (colonPos != std::string::npos) {
                XcfStyleProperty style;
                style.property = property.substr(0, colonPos);
                style.value = property.substr(colonPos + 1);

                // Trim whitespace
                auto trim = [](std::string& s) {
                    s.erase(0, s.find_first_not_of(" \t\n\r"));
                    s.erase(s.find_last_not_of(" \t\n\r") + 1);
                };
                trim(style.property);
                trim(style.value);

                if (!style.property.empty()) {
                    styles.push_back(std::move(style));
                }
            }
        }
    }

    // Handle child elements for individual properties
    for (const auto& child : element.children) {
        if (!child.name.empty() && !child.text.empty()) {
            XcfStyleProperty style;
            style.property = child.name;
            style.value = child.text;
            styles.push_back(std::move(style));
        }
    }
}

// ============================================================================
// XcfMerger implementation
// ============================================================================

void XcfMerger::setXcfData(const XcfData& xcfData)
{
    xcfData_ = xcfData;
    xcfLoaded_ = true;
    buildIndices();
}

bool XcfMerger::loadXcfFile(const std::string& xcfFilePath)
{
    XcfParser parser;
    XcfParseResult result = parser.parseFile(xcfFilePath);

    if (result.success) {
        setXcfData(result.data);
        return true;
    }

    return false;
}

void XcfMerger::buildIndices()
{
    bindingByIdIndex_.clear();
    bindingByNameIndex_.clear();
    hotspotByIdIndex_.clear();

    // Build binding indices
    for (size_t i = 0; i < xcfData_.bindings.size(); i++) {
        const auto& binding = xcfData_.bindings[i];
        if (binding.mode == XcfBindingMode::ById) {
            bindingByIdIndex_[binding.targetRef] = i;
        } else {
            bindingByNameIndex_[binding.targetRef] = i;
        }
    }

    // Build hotspot index
    for (size_t i = 0; i < xcfData_.hotspots.size(); i++) {
        hotspotByIdIndex_[xcfData_.hotspots[i].id] = i;
    }
}

std::map<std::string, std::string> XcfMerger::getMergedAttributes(
    const std::string& apsId,
    const std::string& apsName,
    const std::map<std::string, std::string>& cgmAttributes) const
{
    // Start with CGM attributes
    std::map<std::string, std::string> result = cgmAttributes;

    if (!xcfLoaded_) {
        return result;
    }

    // Try to find binding by ID first
    const XcfExtendedBinding* binding = nullptr;
    auto idIt = bindingByIdIndex_.find(apsId);
    if (idIt != bindingByIdIndex_.end()) {
        binding = &xcfData_.bindings[idIt->second];
    }

    // Fall back to name lookup
    if (!binding && !apsName.empty()) {
        auto nameIt = bindingByNameIndex_.find(apsName);
        if (nameIt != bindingByNameIndex_.end()) {
            binding = &xcfData_.bindings[nameIt->second];
        }
    }

    // Apply binding overrides
    if (binding) {
        if (!binding->linkUri.empty()) {
            result["linkuri"] = binding->linkUri;
        }
        if (!binding->screenTip.empty()) {
            result["screentip"] = binding->screenTip;
        }
        if (!binding->name.empty()) {
            result["name"] = binding->name;
        }
        if (!binding->content.empty()) {
            result["content"] = binding->content;
        }
        if (!binding->visibility.empty()) {
            result["visibility"] = binding->visibility;
        }

        // Apply custom attributes
        for (const auto& attr : binding->customAttributes) {
            result[attr.first] = attr.second;
        }
    }

    return result;
}

std::optional<XcfHotspotRegion> XcfMerger::getHotspotRegion(const std::string& apsId) const
{
    if (!xcfLoaded_) {
        return std::nullopt;
    }

    auto it = hotspotByIdIndex_.find(apsId);
    if (it != hotspotByIdIndex_.end()) {
        return xcfData_.hotspots[it->second];
    }

    return std::nullopt;
}

std::vector<XcfStyleProperty> XcfMerger::getStyleProperties(
    const std::string& apsId,
    const std::string& apsName) const
{
    if (!xcfLoaded_) {
        return {};
    }

    // Try ID lookup first
    auto idIt = bindingByIdIndex_.find(apsId);
    if (idIt != bindingByIdIndex_.end()) {
        return xcfData_.bindings[idIt->second].styleProperties;
    }

    // Fall back to name lookup
    if (!apsName.empty()) {
        auto nameIt = bindingByNameIndex_.find(apsName);
        if (nameIt != bindingByNameIndex_.end()) {
            return xcfData_.bindings[nameIt->second].styleProperties;
        }
    }

    return {};
}

} // namespace opencgm
