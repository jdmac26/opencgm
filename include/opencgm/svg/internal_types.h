#ifndef OPENCGM_SVG_INTERNAL_TYPES_H
#define OPENCGM_SVG_INTERNAL_TYPES_H

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct stbtt_fontinfo;

namespace opencgm {

// Internal types for SVG conversion - these were previously defined in svg_converter.cpp

struct GlyphOutlineVertex {
    uint8_t type = 0;
    float x = 0.0f;
    float y = 0.0f;
    float cx = 0.0f;
    float cy = 0.0f;
    float cx1 = 0.0f;
    float cy1 = 0.0f;
};

struct GlyphOutline {
    int advanceWidth = 0;
    int leftBearing = 0;
    bool hasContours = false;
    std::vector<GlyphOutlineVertex> vertices;
};

struct LoadedFont {
    std::vector<unsigned char> data;
    std::unique_ptr<stbtt_fontinfo> info;
    bool valid = false;
    std::unordered_map<int, GlyphOutline> outline_cache;
};

struct LinkuriFields {
    std::string uri;
    std::string behavior;
    std::string target;
    std::string content;
    std::string highlight;
};

/// Represents a single link entry for multi-link hotspots
struct LinkuriEntry {
    std::string uri;       ///< The link URI
    std::string title;     ///< Link title/screentip (from 2nd linkuri parameter)
    std::string behavior;  ///< Link behavior (from 3rd linkuri parameter)
};

struct SymbolLibraryDescriptor {
    std::string raw;
    std::string label;
    std::string uri;
    std::string fragment;
};

} // namespace opencgm

#endif // OPENCGM_SVG_INTERNAL_TYPES_H
