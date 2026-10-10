// Regression: a slider must not inflate its subtree's max z-index.
//
// Hit testing and painting traverse ordered children sorted by
// subtreeMaxZIndex (core/dsl.h rebuildOrderedElements). The slider's internal
// ".hit" area used to carry zIndex(10), which raised any page containing a
// slider above host-level overlays composed later at the default z=0 (scrim,
// dropdown menus): the overlays lost hit priority and clicks fell through to
// the page. The hit area keeps its input priority via composition order
// (last child of the slider stack), so no zIndex is needed.
//
// Assertions:
//  1. A page containing a slider has subtreeMaxZIndex == 0.
//  2. An overlay composed after the page stays after it in the root's ordered
//     children (stable order for equal keys), so reversed hit-testing reaches
//     the overlay first.
//  3. The slider's ".hit" remains the last ordered child of the slider stack,
//     i.e. it is still hit-tested before track/fill/knob.

#include "components/slider.h"

#include "core/dsl.h"

#include <iostream>
#include <vector>

namespace {

bool run() {
    core::dsl::Ui ui;
    ui.begin("slider_z_order");
    ui.stack("root")
        .size(400.0f, 300.0f)
        .content([&] {
            ui.stack("page")
                .size(400.0f, 200.0f)
                .content([&] {
                    components::slider(ui, "page.slider")
                        .size(120.0f, 20.0f)
                        .value(0.5f)
                        .build();
                })
                .build();
            ui.rect("overlay")
                .position(0.0f, 0.0f)
                .size(400.0f, 300.0f)
                .onClick([] {})
                .build();
        })
        .build();
    ui.layout(400.0f, 300.0f);

    if (ui.roots().size() != 1) {
        std::cerr << "expected exactly one root\n";
        return false;
    }
    const core::dsl::Element& root = *ui.roots().front();
    const core::dsl::Element* page = ui.find("page");
    const core::dsl::Element* overlay = ui.find("overlay");
    const core::dsl::Element* slider = ui.find("page.slider");
    if (page == nullptr || overlay == nullptr || slider == nullptr) {
        std::cerr << "missing page/overlay/slider elements\n";
        return false;
    }

    if (page->subtreeMaxZIndex != 0) {
        std::cerr << "slider inflates page subtreeMaxZIndex to "
                  << page->subtreeMaxZIndex << "\n";
        return false;
    }

    std::size_t pageIndex = root.orderedChildren.size();
    std::size_t overlayIndex = root.orderedChildren.size();
    for (std::size_t index = 0; index < root.orderedChildren.size(); ++index) {
        const core::dsl::Element* child = root.orderedChildren[index];
        if (child == page) {
            pageIndex = index;
        } else if (child == overlay) {
            overlayIndex = index;
        }
    }
    if (pageIndex >= root.orderedChildren.size() ||
        overlayIndex >= root.orderedChildren.size()) {
        std::cerr << "page/overlay missing from root ordered children\n";
        return false;
    }
    if (overlayIndex < pageIndex) {
        std::cerr << "overlay sorted before the page it overlays\n";
        return false;
    }

    const std::vector<const core::dsl::Element*>& sliderChildren =
        slider->orderedChildren;
    const core::dsl::Element* sliderHit = ui.find("page.slider.hit");
    if (sliderChildren.empty() || sliderHit == nullptr ||
        sliderChildren.back() != sliderHit) {
        std::cerr << "slider hit area is not the topmost ordered child\n";
        return false;
    }
    return true;
}

}  // namespace

int main() {
    const bool passed = run();
    if (!passed) {
        std::cerr << "slider subtree z-order regression failed\n";
    }
    return passed ? 0 : 1;
}
