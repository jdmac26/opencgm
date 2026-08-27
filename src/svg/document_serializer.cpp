#include "opencgm/svg/document_serializer.h"

#include "opencgm/svg/xml_utils.h"
#include "opencgm/utils/string_utils.h"

#include <iomanip>
#include <sstream>

namespace opencgm::svg
{
    namespace
    {
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
                                   << std::uppercase
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
    }

    std::string DocumentSerializer::profileLabel(
        OutputProfile profile,
        const std::string &requestedLabel)
    {
        if (!requestedLabel.empty())
        {
            return requestedLabel == "compat"
                       ? "auto"
                       : requestedLabel;
        }

        switch (profile)
        {
            case OutputProfile::S1000D:
                return "s1000d-issue-6";
            case OutputProfile::S1000DLegacy:
                return "s1000d-legacy";
            case OutputProfile::ATA2200:
                return "ata2200";
            case OutputProfile::StandardSVG:
                return "standard";
            case OutputProfile::Custom:
                return "custom";
            case OutputProfile::WebCGM21:
            default:
                return "webcgm";
        }
    }

    std::string DocumentSerializer::title(
        const std::string &pictureName,
        const std::string &sourceName)
    {
        std::string resolved = utils::trimString(pictureName);
        if (resolved.empty())
        {
            resolved = utils::trimString(sourceName);
        }
        return resolved.empty()
                   ? "CGM Picture"
                   : resolved;
    }

    std::string DocumentSerializer::description(
        const std::string &version,
        const std::string &profileLabel,
        bool compatibilityMode,
        bool allowSegments,
        const std::string &sourceName,
        const std::string &sourceHash)
    {
        std::ostringstream output;
        output << "Converted by OpenCGM " << version
               << " (profile=" << profileLabel;
        if (compatibilityMode)
        {
            output << ", compatibility-mode";
            if (allowSegments)
            {
                output << ": iso8632-segments";
            }
        }
        output << ')';
        if (!sourceName.empty())
        {
            output << "; source=" << sourceName;
        }
        if (!sourceHash.empty())
        {
            output << "; md5=" << sourceHash;
        }
        return output.str();
    }

    std::string DocumentSerializer::header(
        const DocumentHeader &document)
    {
        std::ostringstream output;
        output
            << "<?xml version=\"1.0\" encoding=\"UTF-8\" "
               "standalone=\"no\"?>\n"
            << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
               "xmlns:xlink=\"http://www.w3.org/1999/xlink\" ";
        if (document.emit_webcgm_namespace)
        {
            output
                << "xmlns:webcgm=\"http://www.w3.org/Graphics/WebCGM\" ";
        }
        output << "viewBox=\"" << document.viewbox_x << ' '
               << document.viewbox_y << ' '
               << document.viewbox_width << ' '
               << document.viewbox_height << "\" "
               << "preserveAspectRatio=\"xMidYMid meet\">\n";

        if (document.include_metadata)
        {
            output << "  <title>"
                   << escapeXmlText(document.title)
                   << "</title>\n";
            output << "  <desc>"
                   << escapeXmlText(document.description)
                   << "</desc>\n";
        }

        if (document.background_color)
        {
            output << "  <rect x=\"" << document.viewbox_x
                   << "\" y=\"" << document.viewbox_y
                   << "\" width=\"" << document.viewbox_width
                   << "\" height=\"" << document.viewbox_height
                   << "\" fill=\""
                   << escapeXmlAttribute(*document.background_color)
                   << "\" pointer-events=\"none\"/>\n";
        }
        return output.str();
    }

    std::string DocumentSerializer::apsMetadata(
        const std::vector<ApsMetadataEntry> &entries)
    {
        if (entries.empty())
        {
            return {};
        }

        std::ostringstream json;
        json << "{\"aps\":[";
        for (size_t index = 0; index < entries.size(); ++index)
        {
            if (index != 0)
            {
                json << ',';
            }
            const auto &entry = entries[index];
            json << "{\"id\":"
                 << jsonString(entry.resolved_identifier);
            if (!entry.identifier.empty())
            {
                json << ",\"identifier\":"
                     << jsonString(entry.identifier);
            }
            if (!entry.type.empty())
            {
                json << ",\"type\":" << jsonString(entry.type);
            }
            json << ",\"inherit\":"
                 << (entry.inherit ? "true" : "false");
            if (!entry.attributes.empty())
            {
                json << ",\"attributes\":{";
                bool first = true;
                for (const auto &attribute : entry.attributes)
                {
                    if (!first)
                    {
                        json << ',';
                    }
                    first = false;
                    json << jsonString(attribute.first)
                         << ':'
                         << jsonString(attribute.second);
                }
                json << '}';
            }
            json << '}';
        }
        json << "]}";

        std::ostringstream output;
        output << "  <metadata id=\"webcgm-aps\">\n"
               << "    " << escapeXmlText(json.str()) << '\n'
               << "  </metadata>\n";
        return output.str();
    }

    bool DocumentSerializer::shouldEmbedViewerShim(
        ViewerShimMode mode,
        const DocumentFeatures &features)
    {
        switch (mode)
        {
            case ViewerShimMode::Always:
                return true;
            case ViewerShimMode::Never:
                return false;
            case ViewerShimMode::Auto:
            default:
                return features.any();
        }
    }

    std::string DocumentSerializer::viewerShim(
        ViewerShimMode mode,
        const DocumentFeatures &features,
        const std::string &externalUrl,
        const std::string &inlineScript)
    {
        if (!shouldEmbedViewerShim(mode, features))
        {
            return {};
        }

        std::ostringstream output;
        if (!externalUrl.empty())
        {
            output
                << "  <script type=\"application/ecmascript\" "
                   "xlink:href=\""
                << escapeXmlAttribute(externalUrl)
                << "\"></script>\n";
        }
        else
        {
            std::string safeScript = inlineScript;
            size_t offset = 0;
            while ((offset = safeScript.find("]]>", offset)) !=
                   std::string::npos)
            {
                static constexpr const char *replacement =
                    "]]]]><![CDATA[>";
                safeScript.replace(offset, 3, replacement);
                offset += 15;
            }
            output
                << "  <script type=\"application/ecmascript\"><![CDATA[\n"
                << safeScript
                << "\n]]></script>\n";
        }
        return output.str();
    }

    std::string DocumentSerializer::close()
    {
        return "</svg>\n";
    }
}
