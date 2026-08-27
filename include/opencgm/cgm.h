#ifndef OPENCGM_H
#define OPENCGM_H

/**
 * @file cgm.h
 * @brief Main header file for CGM Engine C++ library
 *
 * Include this file to access all CGM library functionality.
 */

// Core classes
#include "opencgm/cgm_point.h"
#include "opencgm/cgm_color.h"
#include "opencgm/enums.h"
#include "opencgm/command.h"
#include "opencgm/cgm_file.h"

// Interfaces
#include "opencgm/interfaces.h"

// Binary I/O
#include "opencgm/binary_reader.h"
#include "opencgm/binary_writer.h"

// Clear text I/O
#include "opencgm/clear_text_reader.h"
#include "opencgm/text_cgm_file.h"

// Factory
#include "opencgm/command_factory.h"
#include "opencgm/version.h"

/**
 * @namespace opencgm
 * @brief Main namespace for CGM library
 */
namespace opencgm {

/**
 * @brief Library version
 */
struct Version {
    static constexpr int MAJOR = kVersionMajor;
    static constexpr int MINOR = kVersionMinor;
    static constexpr int PATCH = kVersionPatch;

    static const char* string() {
        return kEngineVersion;
    }
};

} // namespace opencgm

// Backward compatibility alias
namespace cgm = opencgm;

#endif // OPENCGM_H
