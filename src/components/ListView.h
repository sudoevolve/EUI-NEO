#pragma once

#include "../ui/UIContext.h"
#include <algorithm>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

namespace EUINEO {

class ListView {
public:
    /**
     * @brief Render a virtual list view
     * 
     * @tparam Fn Callback type, should be either `void(int index)` or `void(int index, float itemY)`
     * @param ui The UI context
     * @param id Unique identifier for the scroll area
     * @param x X position of the list view
     * @param y Y position of the list view
     * @param width Width of the list view
     * @param height Height of the list view
     * @param itemCount Total number of items in the list
     * @param itemHeight Fixed height of each item
     * @param composeItem Callback to render the item at the given index
     * @param scrollStep How much to scroll per mouse wheel tick
     * @return float The current scroll offset Y
     */
    template <typename Fn>
    static float Compose(UIContext& ui, const std::string& id, float x, float y, float width, float height,
                         int itemCount, float itemHeight, Fn&& composeItem, float scrollStep = 48.0f) {
        const float contentHeight = std::max(0.0f, static_cast<float>(itemCount) * itemHeight);
        
        const float scrollOffsetY = ui.pushScrollArea(id, x, y, width, height, contentHeight, scrollStep, RenderLayer::Chrome);
        
        if (itemCount > 0 && itemHeight > 0.0f) {
            // Calculate visible range based on current scroll offset
            int startIndex = static_cast<int>(scrollOffsetY / itemHeight);
            int endIndex = static_cast<int>((scrollOffsetY + height) / itemHeight) + 1;
            
            // Buffer 1 item above and below to prevent pop-in during fast scrolling
            startIndex = std::max(0, startIndex - 1);
            endIndex = std::min(itemCount, endIndex + 1);
            
            for (int i = startIndex; i < endIndex; ++i) {
                const float itemY = y + i * itemHeight;
                
                // Provide itemY to the callback if requested, otherwise just index
                if constexpr (std::is_invocable_v<Fn, int, float>) {
                    std::forward<Fn>(composeItem)(i, itemY);
                } else if constexpr (std::is_invocable_v<Fn, int>) {
                    std::forward<Fn>(composeItem)(i);
                } else {
                    static_assert(sizeof(Fn) == 0, "Invalid signature for composeItem. Expected (int index) or (int index, float y)");
                }
            }
        }
        
        ui.popScrollArea();
        
        return scrollOffsetY;
    }
};

} // namespace EUINEO
