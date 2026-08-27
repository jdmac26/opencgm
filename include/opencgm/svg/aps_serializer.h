#ifndef OPENCGM_SVG_APS_SERIALIZER_H
#define OPENCGM_SVG_APS_SERIALIZER_H

#include "aps_policy.h"

#include <map>
#include <string>
#include <vector>

namespace opencgm::svg
{
    struct ApsSerializationState
    {
        std::string aps_id;
        std::string aps_name;
        bool is_layer = false;
        bool is_link = false;
        bool emit_visibility_state = false;
        bool effective_visible = true;
        std::string layer_name;
        std::string layer_title;
        std::map<std::string, std::string> metadata_attributes;
        std::map<std::string, std::string> serialization_attributes;
    };

    struct ApsOpenMarkup
    {
        std::string markup;
        bool emitted_anchor = false;
    };

    /**
     * Stateless APS SVG serialization.
     *
     * Coordinate conversion, XCF merging, and AttributeManager transformation
     * stay with SVGConverter. This class owns the pure decisions and lexical
     * output for derived APS state, anchors, inline metadata, region overlays,
     * and matching close tags.
     */
    class ApsSerializer
    {
    public:
        static ApsSerializationState analyze(
            const ApsNode &node,
            const std::map<std::string, std::string> &mergedAttributes,
            std::map<std::string, std::string> metadataAttributes);

        static std::string derivedGroupAttributes(
            const ApsSerializationState &state,
            const ResolvedConversionPlan &plan);

        static ApsOpenMarkup bodyOpen(
            const ApsNode &node,
            const std::map<std::string, std::string> &mergedAttributes,
            const ApsSerializationState &state,
            const ResolvedConversionPlan &plan,
            const std::string &regionPolygon,
            bool regionPolygonValid);

        static std::string regionOverlay(
            const std::string &points,
            const std::string &owner,
            bool emitOwner,
            int indentationLevel);

        static std::string close(
            int nestingLevel,
            bool emittedAnchor);
    };
}

#endif
