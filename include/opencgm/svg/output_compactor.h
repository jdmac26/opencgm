#ifndef OPENCGM_SVG_OUTPUT_COMPACTOR_H
#define OPENCGM_SVG_OUTPUT_COMPACTOR_H

#include <string>

namespace opencgm {
namespace svg {

// Size reductions applied when web optimisation is requested. They keep the
// rendering identical but change the document's shape, so they are opt-in:
// downstream tooling may expect every primitive to carry its own style.
class OutputCompactor
{
public:
    // Wraps each run of two or more consecutive sibling primitives that carry
    // identical presentation attributes (stroke*, fill*) in a <g> holding them
    // once. Those properties are all inherited, so rendering is unchanged.
    // Left alone: anything inside <defs> or <clipPath> (where <g> is not
    // allowed or not meaningful), and primitives with an id or class, which
    // viewers and scripts address directly. Expects the converter's layout of
    // one element per line.
    static std::string hoistSharedStyles(const std::string& svg);

    // The number of decimals coordinates need so that rounding stays within
    // 1/10000 of the picture's longest side, read from the root viewBox.
    // Never fewer than minimumDecimals; 0 when no viewBox is found.
    static int decimalsForViewBox(const std::string& svg, int minimumDecimals);

    // Rounds every number in polyline/polygon points to the given decimals.
    static std::string roundPoints(const std::string& svg, int decimals);
};

} // namespace svg
} // namespace opencgm

#endif // OPENCGM_SVG_OUTPUT_COMPACTOR_H
