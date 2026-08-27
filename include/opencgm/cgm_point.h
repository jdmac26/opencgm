#ifndef OPENCGM_POINT_H
#define OPENCGM_POINT_H

#include <string>
#include <cmath>

namespace opencgm {

/**
 * @brief Represents a point parameter type
 */
class CGMPoint {
public:
    CGMPoint() : x_(0.0), y_(0.0) {}
    CGMPoint(double x, double y) : x_(x), y_(y) {}

    double x() const { return x_; }
    double y() const { return y_; }

    void set_x(double x) { x_ = x; }
    void set_y(double y) { y_ = y; }

    std::string toString() const;

    bool operator==(const CGMPoint& other) const;
    bool operator!=(const CGMPoint& other) const;
    bool operator<(const CGMPoint& other) const;

    /**
     * @brief Check if two double values are approximately equal
     */
    static bool isSame(double x, double y);

    /**
     * @brief Compare two double values with tolerance
     * @return -1 if x < y, 0 if equal, 1 if x > y
     */
    static int compareValues(double x, double y);

private:
    double x_;
    double y_;
};

} // namespace opencgm

#endif // OPENCGM_POINT_H