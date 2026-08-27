#include "opencgm/svg/aps_geometry.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace opencgm::svg
{
    namespace
    {
        std::optional<double> parseNumber(const std::string &value)
        {
            try
            {
                size_t consumed = 0;
                const double number = std::stod(value, &consumed);
                if (consumed != value.size() ||
                    !std::isfinite(number))
                {
                    return std::nullopt;
                }
                return number;
            }
            catch (const std::exception &)
            {
                return std::nullopt;
            }
        }

        std::string formatNumber(double value)
        {
            if (!std::isfinite(value))
            {
                return "0";
            }
            if (std::fabs(value) < 0.0000005)
            {
                value = 0.0;
            }

            std::ostringstream output;
            output.setf(std::ios::fixed);
            output << std::setprecision(6) << value;
            std::string result = output.str();

            const size_t decimal = result.find('.');
            if (decimal != std::string::npos)
            {
                while (!result.empty() && result.back() == '0')
                {
                    result.pop_back();
                }
                if (!result.empty() && result.back() == '.')
                {
                    result.pop_back();
                }
            }
            return result.empty() || result == "-0" ? "0" : result;
        }

        std::string formatPoints(
            const std::vector<CGMPoint> &points)
        {
            std::ostringstream output;
            for (size_t index = 0; index < points.size(); ++index)
            {
                if (index != 0)
                {
                    output << ' ';
                }
                output << points[index].x()
                       << ','
                       << points[index].y();
            }
            return output.str();
        }
    }

    std::optional<ApsRect> ApsGeometry::orderedRect(
        double x1,
        double y1,
        double x2,
        double y2)
    {
        if (!std::isfinite(x1) ||
            !std::isfinite(y1) ||
            !std::isfinite(x2) ||
            !std::isfinite(y2))
        {
            return std::nullopt;
        }
        return ApsRect{
            std::min(x1, x2),
            std::min(y1, y2),
            std::max(x1, x2),
            std::max(y1, y2)};
    }

    std::optional<ApsRect> ApsGeometry::parseRectTokens(
        const std::vector<std::string> &tokens)
    {
        if (tokens.size() < 4)
        {
            return std::nullopt;
        }

        const auto x1 = parseNumber(tokens[0]);
        const auto y1 = parseNumber(tokens[1]);
        const auto x2 = parseNumber(tokens[2]);
        const auto y2 = parseNumber(tokens[3]);
        if (!x1 || !y1 || !x2 || !y2)
        {
            return std::nullopt;
        }
        return orderedRect(*x1, *y1, *x2, *y2);
    }

    std::optional<ApsRect> ApsGeometry::clampRect(
        const ApsRect &rect,
        const ApsRect &bounds)
    {
        ApsRect clamped{
            std::clamp(rect.min_x, bounds.min_x, bounds.max_x),
            std::clamp(rect.min_y, bounds.min_y, bounds.max_y),
            std::clamp(rect.max_x, bounds.min_x, bounds.max_x),
            std::clamp(rect.max_y, bounds.min_y, bounds.max_y)};

        const double epsilon =
            std::numeric_limits<double>::epsilon() * 10.0;
        if ((clamped.max_x - clamped.min_x) <= epsilon ||
            (clamped.max_y - clamped.min_y) <= epsilon)
        {
            return std::nullopt;
        }
        return clamped;
    }

    std::optional<ApsRect> ApsGeometry::toNvdc(
        const ApsRect &rect,
        bool scaled,
        double metricScale,
        double pictureMinX,
        double pictureMinY,
        double pictureMaxY,
        bool yDown)
    {
        if (!scaled ||
            !std::isfinite(metricScale) ||
            metricScale <= 0.0)
        {
            return std::nullopt;
        }

        ApsRect result;
        result.min_x =
            (rect.min_x - pictureMinX) * metricScale;
        result.max_x =
            (rect.max_x - pictureMinX) * metricScale;

        if (yDown)
        {
            result.min_y =
                (pictureMaxY - rect.max_y) * metricScale;
            result.max_y =
                (pictureMaxY - rect.min_y) * metricScale;
        }
        else
        {
            result.min_y =
                (rect.min_y - pictureMinY) * metricScale;
            result.max_y =
                (rect.max_y - pictureMinY) * metricScale;
        }

        if (!std::isfinite(result.min_x) ||
            !std::isfinite(result.min_y) ||
            !std::isfinite(result.max_x) ||
            !std::isfinite(result.max_y))
        {
            return std::nullopt;
        }
        return result;
    }

    std::string ApsGeometry::formatRect(const ApsRect &rect)
    {
        return formatNumber(rect.min_x) + " " +
               formatNumber(rect.min_y) + " " +
               formatNumber(rect.max_x) + " " +
               formatNumber(rect.max_y);
    }

    std::optional<std::vector<CGMPoint>>
    ApsGeometry::parseRegionPoints(const std::string &value)
    {
        if (value.empty())
        {
            return std::nullopt;
        }

        std::string cleaned = value;
        std::replace(cleaned.begin(), cleaned.end(), ',', ' ');
        std::istringstream input(cleaned);
        std::vector<CGMPoint> points;

        while (true)
        {
            double x = 0.0;
            if (!(input >> x))
            {
                if (input.eof())
                {
                    break;
                }
                return std::nullopt;
            }

            double y = 0.0;
            if (!(input >> y) ||
                !std::isfinite(x) ||
                !std::isfinite(y))
            {
                return std::nullopt;
            }
            points.emplace_back(x, y);
        }
        return points.empty()
                   ? std::nullopt
                   : std::optional<std::vector<CGMPoint>>(
                         std::move(points));
    }

    ApsRegionGeometry ApsGeometry::normalizeRegion(
        const std::vector<CGMPoint> &points)
    {
        ApsRegionGeometry result;
        result.metadata_points = points;

        if (result.metadata_points.size() == 2)
        {
            const double minX = std::min(
                result.metadata_points[0].x(),
                result.metadata_points[1].x());
            const double maxX = std::max(
                result.metadata_points[0].x(),
                result.metadata_points[1].x());
            const double minY = std::min(
                result.metadata_points[0].y(),
                result.metadata_points[1].y());
            const double maxY = std::max(
                result.metadata_points[0].y(),
                result.metadata_points[1].y());
            result.metadata_points = {
                CGMPoint(minX, minY),
                CGMPoint(maxX, minY),
                CGMPoint(maxX, maxY),
                CGMPoint(minX, maxY)};
            result.expanded_rectangle = true;
        }

        result.polygon_points = result.metadata_points;
        if (result.polygon_points.size() >= 4 &&
            result.polygon_points.front().x() ==
                result.polygon_points.back().x() &&
            result.polygon_points.front().y() ==
                result.polygon_points.back().y())
        {
            result.polygon_points.pop_back();
        }

        result.metadata_value =
            formatPoints(result.metadata_points);
        result.polygon_value =
            formatPoints(result.polygon_points);
        return result;
    }
}
