#ifndef OPENCGM_SVG_APS_POLICY_H
#define OPENCGM_SVG_APS_POLICY_H

#include "conversion_plan.h"
#include "internal_types.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace opencgm::svg
{
    struct ApsNode
    {
        std::string identifier;
        std::string resolved_identifier;
        std::string type;
        bool inheritanceFlag = false;
        std::map<std::string, std::string> attributes;
        std::map<std::string, std::string> merged_attributes;
        std::vector<LinkuriEntry> linkuris;
        int nesting_level = 0;
        bool emitted_anchor_tag = false;
    };

    /**
     * Pure APS policy decisions extracted from SVGConverter.
     *
     * This class does not write SVG or mutate conversion state. It resolves
     * profile gates, APS classification, and safe custom attributes so those
     * rules can be tested independently from command processing.
     */
    class ApsPolicy
    {
    public:
        static bool isLink(
            const ApsNode &node,
            const std::map<std::string, std::string> &mergedAttributes);

        static AttributeManager::ApsEmissionGates emissionGates(
            const ResolvedConversionPlan &plan);

        static bool shouldEmitAnchor(const ResolvedConversionPlan &plan);
        static bool shouldEmitDataAttributes(
            const ResolvedConversionPlan &plan);
        static bool shouldEmitInlineMetadata(
            const ResolvedConversionPlan &plan);
        static bool shouldEmitRegionOverlay(
            const ResolvedConversionPlan &plan);

        static std::map<std::string, std::string> customAttributes(
            const ResolvedConversionPlan &plan,
            const ApsNode &node,
            const std::vector<ApsNode> &ancestors,
            const std::set<std::string> &reservedNames);
    };
}

#endif
