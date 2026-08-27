#ifndef OPENCGM_VERSION_H
#define OPENCGM_VERSION_H

#include "opencgm/version_config.h"

namespace opencgm {

inline constexpr int kVersionMajor = OPENCGM_VERSION_MAJOR;
inline constexpr int kVersionMinor = OPENCGM_VERSION_MINOR;
inline constexpr int kVersionPatch = OPENCGM_VERSION_PATCH;

// Semantic version for the native engine and converter metadata.
inline constexpr const char* kEngineVersion = OPENCGM_VERSION_SEMVER;
inline constexpr const char* kEngineVersionString = OPENCGM_ENGINE_VERSION_STRING;
inline constexpr const char* kEngineVersionDisplay = OPENCGM_ENGINE_VERSION_DISPLAY;

// Backward-compatible alias used in SVG metadata.
inline constexpr const char* kSvgqaVersion = kEngineVersion;

} // namespace opencgm

#endif // OPENCGM_VERSION_H
