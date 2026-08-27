#ifndef OPENCGM_SVG_APS_GEOMETRY_H
#define OPENCGM_SVG_APS_GEOMETRY_H

#include "opencgm/cgm_point.h"

#include <optional>
#include <string>
#include <vector>

namespace opencgm::svg
{
    struct ApsRect
    {
        double min_x = 0.0;
        double min_y = 0.0;
        double max_x = 0.0;
        double max_y = 0.0;
    };

    struct ApsRegionGeometry
    {
        std::vector<CGMPoint> metadata_points;
        std::vector<CGMPoint> polygon_points;
        std::string metadata_value;
        std::string polygon_value;
        bool expanded_rectangle = false;

        bool validPolygon() const
        {
            return polygon_points.size() >= 3;
        }
    };

    class ApsGeometry
    {
    public:
        static std::optional<ApsRect> orderedRect(
            double x1,
            double y1,
            double x2,
            double y2);

        static std::optional<ApsRect> parseRectTokens(
            const std::vector<std::string> &tokens);

        static std::optional<ApsRect> clampRect(
            const ApsRect &rect,
            const ApsRect &bounds);

        static std::optional<ApsRect> toNvdc(
            const ApsRect &rect,
            bool scaled,
            double metricScale,
            double pictureMinX,
            double pictureMinY,
            double pictureMaxY,
            bool yDown);

        static std::string formatRect(const ApsRect &rect);

        static std::optional<std::vector<CGMPoint>> parseRegionPoints(
            const std::string &value);

        static ApsRegionGeometry normalizeRegion(
            const std::vector<CGMPoint> &points);
    };
}

#endif
