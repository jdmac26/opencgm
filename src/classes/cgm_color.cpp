#include "opencgm/cgm_color.h"
#include <sstream>

namespace opencgm {

bool CGMColor::operator==(const CGMColor& other) const {
    return colorIndex_ == other.colorIndex_ &&
           color_.toArgb() == other.color_.toArgb();
}

bool CGMColor::operator!=(const CGMColor& other) const {
    return !(*this == other);
}

std::string CGMColor::toString() const {
    std::ostringstream oss;
    if (colorIndex_ > -1) {
        oss << "ColorIndex " << colorIndex_;
    } else {
        oss << "Color RGB("
            << static_cast<int>(color_.r) << ", "
            << static_cast<int>(color_.g) << ", "
            << static_cast<int>(color_.b) << ")";
    }
    return oss.str();
}

} // namespace opencgm