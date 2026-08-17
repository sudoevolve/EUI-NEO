#pragma once

#include "core/dsl.h"

#include <algorithm>
#include <string>
#include <vector>

namespace core::dsl {

// Collect focusable element ids in z-order (back-to-front, stable by declaration
// order when zIndex ties). Mirrors the order used by hit-testing and
// Runtime::forEachElement. Skips elements inside disabled subtrees, matching
// Runtime::findElementDisabledState semantics without a second tree walk.
inline void collectFocusableIds(const Element& element,
                                bool ancestorDisabled,
                                std::vector<std::string>& out) {
    const bool disabledTree = ancestorDisabled || element.disabled;
    if (element.focusable && !disabledTree) {
        out.push_back(element.id);
    }
    for (const Element* child : element.orderedChildren) {
        collectFocusableIds(*child, disabledTree, out);
    }
}

inline std::vector<std::string> collectOrderedFocusableIds(const Ui& ui) {
    std::vector<std::string> ids;
    for (const Element* root : ui.orderedRoots()) {
        collectFocusableIds(*root, false, ids);
    }
    return ids;
}

// Next traversal target given the ordered focusable list, the current focus id
// (possibly stale/disabled) and the direction. Empty current -> first/last;
// current not in list (disabled or removed) -> first/last; wrap around at ends.
inline std::string nextFocusTarget(const std::vector<std::string>& orderedIds,
                                   const std::string& currentId,
                                   bool reverse) {
    if (orderedIds.empty()) {
        return {};
    }
    if (currentId.empty()) {
        return reverse ? orderedIds.back() : orderedIds.front();
    }
    const auto it = std::find(orderedIds.begin(), orderedIds.end(), currentId);
    if (it == orderedIds.end()) {
        return reverse ? orderedIds.back() : orderedIds.front();
    }
    const std::size_t index = static_cast<std::size_t>(it - orderedIds.begin());
    if (reverse) {
        return orderedIds[index == 0 ? orderedIds.size() - 1 : index - 1];
    }
    return orderedIds[(index + 1) % orderedIds.size()];
}

} // namespace core::dsl
