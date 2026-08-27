#include "opencgm/cgm_point.h"
#include <sstream>
#include <iomanip>

namespace opencgm {

std::string CGMPoint::toString() const {
    std::ostringstream oss;
    oss << "Point(" << x_ << ", " << y_ << ")";
    return oss.str();
}

bool CGMPoint::operator==(const CGMPoint& other) const {
    return isSame(x_, other.x_) && isSame(y_, other.y_);
}

bool CGMPoint::operator!=(const CGMPoint& other) const {
    return !(*this == other);
}

bool CGMPoint::operator<(const CGMPoint& other) const {
    int xCompare = compareValues(x_, other.x_);
    if (xCompare == 0) {
        return compareValues(y_, other.y_) < 0;
    }
    return xCompare < 0;
}

bool CGMPoint::isSame(double x, double y) {
    return compareValues(x, y) == 0;
}

int CGMPoint::compareValues(double x, double y) {
    // Round to 4 decimal places
    double roundedX = std::round(x * 10000.0) / 10000.0;
    double roundedY = std::round(y * 10000.0) / 10000.0;

    if (roundedX == roundedY) {
        return 0;
    } else if (std::abs(roundedX - roundedY) < 0.0004) {
        return 0;
    } else if (roundedX < roundedY) {
        return -1;
    } else {
        return 1;
    }
}

} // namespace opencgm