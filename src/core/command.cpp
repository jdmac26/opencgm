#include "opencgm/command.h"
#include "opencgm/cgm_file.h"
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace opencgm {

Command::Command(const CommandConstructorArguments& args)
    : elementClass_(args.elementClass)
    , elementId_(args.elementId)
    , container_(args.container) {
}

std::string Command::toString() const {
    // Return the class name (simplified version)
    return "Command";
}

void Command::verify(bool condition, const std::string& errorMessage) {
    if (!condition) {
        throw std::runtime_error(errorMessage);
    }
}

std::string Command::writeDouble(double value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4) << value;
    return oss.str();
}

std::string Command::writeReal(double value) const {
    return writeDouble(value);
}

std::string Command::writeVDC(double value) const {
    if (container_->vdcType() == VDCType::Real) {
        return writeDouble(value);
    } else {
        return writeInt(static_cast<int>(value));
    }
}

std::string Command::writePoint(double x, double y) const {
    std::ostringstream oss;
    std::string signCharY = "";

    double roundedY = std::round(y * 10000.0) / 10000.0;
    if (roundedY == 0.0 && x < 0) {
        signCharY = "-";
    }

    oss << "(" << writeDouble(x) << "," << signCharY << writeDouble(y) << ")";
    return oss.str();
}

std::string Command::writeBool(bool value) const {
    return value ? "on" : "off";
}

std::string Command::writeString(const std::string& value) const {
    std::string result;
    result.reserve(value.size() * 2 + 2);
    result.push_back('\'');

    for (char ch : value) {
        unsigned char uc = static_cast<unsigned char>(ch);

        if (ch == '\'') {
            result.push_back('\'');
            result.push_back('\'');
        } else if (uc == '\n' || uc == '\r' || uc == '\t') {
            result.push_back(' ');
        } else if (uc < 0x20 || uc == 0x7F) {
            // Skip other control characters per ISO/IEC 8632-4 string rules
            continue;
        } else {
            result.push_back(ch);
        }
    }

    result.push_back('\'');
    return result;
}

std::string Command::writeEnum(int value) const {
    return std::to_string(value);
}

std::string Command::writeInt(int value) const {
    return std::to_string(value);
}

std::string Command::writeIndex(int value) const {
    return std::to_string(value);
}

} // namespace opencgm
