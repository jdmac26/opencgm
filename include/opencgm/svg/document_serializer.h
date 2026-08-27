#ifndef OPENCGM_SVG_DOCUMENT_SERIALIZER_H
#define OPENCGM_SVG_DOCUMENT_SERIALIZER_H

#include "conversion_plan.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace opencgm::svg
{
    enum class ViewerShimMode
    {
        Auto,
        Always,
        Never
    };

    struct DocumentFeatures
    {
        bool layers = false;
        bool links = false;
        bool view_context = false;

        bool any() const
        {
            return layers || links || view_context;
        }
    };

    struct ApsMetadataEntry
    {
        std::string identifier;
        std::string resolved_identifier;
        std::string type;
        bool inherit = false;
        std::map<std::string, std::string> attributes;
    };

    struct DocumentHeader
    {
        double viewbox_x = 0.0;
        double viewbox_y = 0.0;
        double viewbox_width = 0.0;
        double viewbox_height = 0.0;
        bool emit_webcgm_namespace = false;
        bool include_metadata = true;
        std::string title;
        std::string description;
        std::optional<std::string> background_color;
    };

    /**
     * Stateless serialization for the SVG document shell.
     *
     * SVGConverter remains responsible for source-file hashing and feature
     * discovery. This class owns lexical output and the pure policy used by
     * the root element, document metadata, APS report, and viewer shim.
     */
    class DocumentSerializer
    {
    public:
        static std::string profileLabel(
            OutputProfile profile,
            const std::string &requestedLabel);

        static std::string title(
            const std::string &pictureName,
            const std::string &sourceName);

        static std::string description(
            const std::string &version,
            const std::string &profileLabel,
            bool compatibilityMode,
            bool allowSegments,
            const std::string &sourceName,
            const std::string &sourceHash);

        static std::string header(const DocumentHeader &document);

        static std::string apsMetadata(
            const std::vector<ApsMetadataEntry> &entries);

        static bool shouldEmbedViewerShim(
            ViewerShimMode mode,
            const DocumentFeatures &features);

        static std::string viewerShim(
            ViewerShimMode mode,
            const DocumentFeatures &features,
            const std::string &externalUrl,
            const std::string &inlineScript);

        static std::string close();
    };
}

#endif
