#include "components/markdown.h"
#include "components/theme.h"
#include "core/layout.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <string>

namespace {

bool closeEnough(float left, float right) {
    return std::fabs(left - right) <= 0.01f;
}

bool expectClose(const std::string& label, float actual, float expected) {
    if (closeEnough(actual, expected)) {
        return true;
    }
    std::cerr << label << ": expected " << expected << ", got " << actual << "\n";
    return false;
}

std::unique_ptr<core::Node> fixedNode(float width, float height) {
    auto node = std::make_unique<core::Node>(core::LayoutType::Stack);
    node->setFixedSize(width, height);
    return node;
}

bool flexStaysInsideFixedRow() {
    core::Node row(core::LayoutType::Row);
    row.setFixedSize(600.0f, 62.0f);
    row.setSpacing(10.0f);

    row.addChild(fixedNode(72.0f, 54.0f));

    auto center = std::make_unique<core::Node>(core::LayoutType::Stack);
    center->setHeight(core::SizeValue::fixed(54.0f));
    center->setFlexGrow(1.0f);
    center->setMinWidth(92.0f);
    row.addChild(std::move(center));

    row.addChild(fixedNode(72.0f, 54.0f));

    row.measure(2000.0f, 62.0f);
    row.layout(0.0f, 0.0f);

    const auto& children = row.children();
    return expectClose("row width", row.measuredWidth(), 600.0f) &&
           expectClose("left x", children[0]->frame().x, 0.0f) &&
           expectClose("center x", children[1]->frame().x, 82.0f) &&
           expectClose("center width", children[1]->frame().width, 436.0f) &&
           expectClose("right x", children[2]->frame().x, 528.0f) &&
           expectClose("right width", children[2]->frame().width, 72.0f);
}

bool overlayDoesNotAffectColumnHeight() {
    core::Node column(core::LayoutType::Column);
    column.setWidth(core::SizeValue::fixed(200.0f));
    column.setHeight(core::SizeValue::wrapContent());
    column.setSpacing(8.0f);
    column.setPadding(core::EdgeInsets::all(10.0f));

    auto overlay = std::make_unique<core::Node>(core::LayoutType::Stack);
    overlay->setWidth(core::SizeValue::fill());
    overlay->setHeight(core::SizeValue::fill());
    overlay->setIgnoreLayout(true);
    column.addChild(std::move(overlay));

    column.addChild(fixedNode(120.0f, 30.0f));
    column.addChild(fixedNode(120.0f, 30.0f));

    column.measure(200.0f, 400.0f);
    column.layout(0.0f, 0.0f);

    const auto& children = column.children();
    return expectClose("column height", column.measuredHeight(), 88.0f) &&
           expectClose("overlay width", children[0]->frame().width, 180.0f) &&
           expectClose("overlay height", children[0]->frame().height, 68.0f) &&
           expectClose("first content y", children[1]->frame().y, 10.0f) &&
           expectClose("second content y", children[2]->frame().y, 48.0f);
}

bool themeMetricsDriveComponentVisuals() {
    components::theme::ThemeColorTokens tokens = components::theme::dark();
    tokens.metrics.typography.body = 19.0f;
    tokens.metrics.typography.display = 35.0f;
    tokens.metrics.spacing.control = 13.0f;
    tokens.metrics.spacing.section = 21.0f;
    tokens.metrics.radius.small = 7.0f;
    tokens.metrics.radius.section = 23.0f;
    tokens.metrics.control.field = 41.0f;

    const components::theme::PageVisualTokens page = components::theme::pageVisuals(tokens);
    const components::theme::FieldVisualTokens field = components::theme::fieldVisuals(tokens);
    const components::MarkdownStyle markdown(tokens);

    return expectClose("page title size", page.headerTitleSize, 35.0f) &&
           expectClose("page section gap", page.sectionGap, 21.0f) &&
           expectClose("page radius", page.sectionRounding, 23.0f) &&
           expectClose("page field height", page.fieldHeight, 41.0f) &&
           expectClose("field inset", field.horizontalInset, 13.0f) &&
           expectClose("field radius", field.rounding, 7.0f) &&
           expectClose("markdown body", markdown.bodySize, 19.0f) &&
           expectClose("markdown radius", markdown.radius, 7.0f);
}

} // namespace

int main() {
    bool ok = true;
    ok = flexStaysInsideFixedRow() && ok;
    ok = overlayDoesNotAffectColumnHeight() && ok;
    ok = themeMetricsDriveComponentVisuals() && ok;
    return ok ? 0 : 1;
}
