#include "opencgm/svg/output_compactor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <utility>
#include <vector>

// Plain string scanning throughout: primitives in large illustrations carry
// points attributes tens of kilobytes long, which std::regex (recursive in
// MSVC's implementation) handles slowly and can overflow the stack on.

namespace opencgm {
namespace svg {

namespace {

// Inherited presentation attributes the converter writes on primitives.
bool isHoistable(const std::string& name)
{
    static const char* const names[] = {
        "stroke", "stroke-width", "stroke-linecap", "stroke-linejoin", "stroke-miterlimit",
        "stroke-dasharray", "stroke-dashoffset", "stroke-opacity", "fill", "fill-rule", "fill-opacity",
    };
    return std::find(std::begin(names), std::end(names), name) != std::end(names);
}

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

struct Primitive
{
    std::string original;
    std::string indent;
    std::string tag;
    std::string style;  // hoistable attributes, in document order, as written
    std::string rest;   // every other attribute, as written
};

// A self-closing polyline/polygon/path/line/rect/circle/ellipse alone on its
// line, with at least one hoistable attribute and no id or class.
bool parsePrimitive(const std::string& line, Primitive& out)
{
    static const char* const tags[] = {"polyline", "polygon", "path", "line", "rect", "circle", "ellipse"};
    size_t pos = 0;
    while (pos < line.size() && isSpace(line[pos]))
        ++pos;
    size_t end = line.size();
    while (end > pos && isSpace(line[end - 1]))
        --end;
    if (pos >= end || line[pos] != '<' || end < pos + 3 || line.compare(end - 2, 2, "/>") != 0)
        return false;

    const size_t nameStart = pos + 1;
    size_t nameEnd = nameStart;
    while (nameEnd < end && !isSpace(line[nameEnd]) && line[nameEnd] != '/')
        ++nameEnd;
    const std::string tag = line.substr(nameStart, nameEnd - nameStart);
    if (std::find_if(std::begin(tags), std::end(tags), [&](const char* t) { return tag == t; }) == std::end(tags))
        return false;

    out.original = line;
    out.indent = line.substr(0, pos);
    out.tag = tag;
    out.style.clear();
    out.rest.clear();

    size_t i = nameEnd;
    const size_t attrsEnd = end - 2;
    while (true)
    {
        while (i < attrsEnd && isSpace(line[i]))
            ++i;
        if (i >= attrsEnd)
            break;
        const size_t attrStart = i;
        while (i < attrsEnd && line[i] != '=' && !isSpace(line[i]))
            ++i;
        const std::string name = line.substr(attrStart, i - attrStart);
        while (i < attrsEnd && isSpace(line[i]))
            ++i;
        if (i >= attrsEnd || line[i] != '=')
            return false;
        ++i;
        while (i < attrsEnd && isSpace(line[i]))
            ++i;
        if (i >= attrsEnd || (line[i] != '"' && line[i] != '\''))
            return false;
        const char quote = line[i];
        const size_t close = line.find(quote, i + 1);
        if (close == std::string::npos || close >= attrsEnd)
            return false;
        i = close + 1;
        if (name.empty() || name == "id" || name == "class")
            return false;  // addressed directly by viewers and scripts
        (isHoistable(name) ? out.style : out.rest) += " " + line.substr(attrStart, i - attrStart);
    }
    return !out.style.empty();
}

// Net change in <defs>/<clipPath> nesting on a line; self-closing tags count 0.
int protectedDelta(const std::string& line)
{
    int delta = 0;
    for (const char* name : {"defs", "clipPath"})
    {
        const std::string open = std::string("<") + name;
        const std::string close = std::string("</") + name + ">";
        for (size_t at = line.find(open); at != std::string::npos; at = line.find(open, at + 1))
        {
            const char next = at + open.size() < line.size() ? line[at + open.size()] : '\0';
            if (next != '>' && !isSpace(next))
                continue;  // e.g. <defsX>
            const size_t gt = line.find('>', at);
            if (gt != std::string::npos && line[gt - 1] != '/')
                ++delta;
        }
        for (size_t at = line.find(close); at != std::string::npos; at = line.find(close, at + 1))
            --delta;
    }
    return delta;
}

} // namespace

std::string OutputCompactor::hoistSharedStyles(const std::string& svg)
{
    std::istringstream in(svg);
    std::ostringstream out;
    std::string line;
    std::vector<Primitive> run;
    int protectedDepth = 0;
    bool first = true;

    auto emit = [&](const std::string& text) {
        if (!first)
            out << '\n';
        out << text;
        first = false;
    };
    auto flush = [&]() {
        if (run.size() >= 2)
        {
            emit(run.front().indent + "<g" + run.front().style + ">");
            for (const auto& p : run)
                emit(p.indent + " <" + p.tag + p.rest + " />");
            emit(run.front().indent + "</g>");
        }
        else
        {
            for (const auto& p : run)
                emit(p.original);
        }
        run.clear();
    };

    while (std::getline(in, line))
    {
        Primitive p;
        if (protectedDepth == 0 && parsePrimitive(line, p))
        {
            if (!run.empty() && (run.front().style != p.style || run.front().indent != p.indent))
                flush();
            run.push_back(std::move(p));
            continue;
        }
        flush();
        protectedDepth = std::max(0, protectedDepth + protectedDelta(line));
        emit(line);
    }
    flush();
    if (!svg.empty() && svg.back() == '\n')
        out << '\n';
    return out.str();
}

int OutputCompactor::decimalsForViewBox(const std::string& svg, int minimumDecimals)
{
    const int floor = std::max(minimumDecimals, 0);
    const size_t root = svg.find("<svg");
    const size_t rootEnd = root == std::string::npos ? root : svg.find('>', root);
    const size_t attr = root == std::string::npos ? root : svg.find("viewBox=\"", root);
    if (attr == std::string::npos || attr > rootEnd)
        return floor;
    std::istringstream values(svg.substr(attr + 9, svg.find('"', attr + 9) - (attr + 9)));
    double x = 0, y = 0, w = 0, h = 0;
    values >> x >> y >> w >> h;
    const double longest = std::max(std::fabs(w), std::fabs(h));
    if (!(longest > 0.0) || !std::isfinite(longest))
        return floor;
    const int needed = static_cast<int>(std::ceil(-std::log10(longest / 10000.0)));
    return std::clamp(std::max(needed, floor), 0, 6);
}

std::string OutputCompactor::roundPoints(const std::string& svg, int decimals)
{
    auto format = [decimals](double v, std::string& to) {
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, v);
        std::string s = buffer;
        if (s.find('.') != std::string::npos)
        {
            s.erase(s.find_last_not_of('0') + 1);
            if (s.back() == '.')
                s.pop_back();
        }
        to += (s == "-0") ? "0" : s;
    };

    std::string result;
    result.reserve(svg.size());
    size_t cursor = 0;
    while (true)
    {
        size_t tag = svg.find('<', cursor);
        if (tag == std::string::npos)
            break;
        const bool target = svg.compare(tag, 9, "<polyline") == 0 || svg.compare(tag, 8, "<polygon") == 0;
        const size_t tagEnd = svg.find('>', tag);
        if (!target || tagEnd == std::string::npos)
        {
            result.append(svg, cursor, (tagEnd == std::string::npos ? svg.size() : tagEnd + 1) - cursor);
            cursor = tagEnd == std::string::npos ? svg.size() : tagEnd + 1;
            continue;
        }
        const size_t attr = svg.find(" points=\"", tag);
        if (attr == std::string::npos || attr > tagEnd)
        {
            result.append(svg, cursor, tagEnd + 1 - cursor);
            cursor = tagEnd + 1;
            continue;
        }
        const size_t valueStart = attr + 9;
        const size_t valueEnd = svg.find('"', valueStart);
        result.append(svg, cursor, valueStart - cursor);
        const char* p = svg.c_str() + valueStart;
        const char* stop = svg.c_str() + valueEnd;
        while (p < stop)
        {
            // Copy separators ourselves: strtod would skip leading whitespace
            // and silently drop the space between two numbers.
            if (isSpace(*p) || *p == ',')
            {
                result += *p++;
                continue;
            }
            char* after = nullptr;
            const double v = std::strtod(p, &after);
            if (after == p || after > stop)
            {
                result += *p++;
                continue;
            }
            format(v, result);
            p = after;
        }
        cursor = valueEnd;
    }
    result.append(svg, cursor, std::string::npos);
    return result;
}

} // namespace svg
} // namespace opencgm
