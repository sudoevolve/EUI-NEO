#include "components/components.h"
#include "eui/dsl_app.h"
#include "eui/detail/window_size.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace {
int failures = 0;
void require(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; ++failures; }
}
bool near(float a, float b) { return std::fabs(a - b) < .01f; }

void formAndPopup() {
    core::dsl::Ui ui;
    eui::Signal<int> selected{0};
    eui::Signal<bool> open{false};
    const auto compose = [&] {
        ui.begin("settings");
        components::form(ui, "form").position(20, 30).width(600).padding(12).controlHeight(28).gap(10)
            .row("proxy", "Proxy type", [&](auto& u, float w, float h) {
                components::dropdown(u, "proxy").size(w, h).items({"System", "HTTP", "SOCKS5"}).bind(selected).bindOpen(open).build();
            })
            .row("address", "Address", [&](auto& u, float w, float h) {
                components::input(u, "address").size(w, h).build();
            }).build();
        ui.end(); ui.layout(800, 600);
    };
    compose();
    auto* label = ui.find("form.proxy.label");
    auto* field = ui.find("proxy");
    require(label && field && near(label->frame.y, field->frame.y) && near(label->frame.height, field->frame.height), "form row label and control are not aligned");
    require(label && label->verticalAlign == core::VerticalAlign::Center, "form label must center in the row");
    require(near(ui.find("form")->frame.height, 90), "closed dropdown must not reserve popup height");
    require(ui.find("form")->orderedChildren.front() == ui.find("form.proxy"), "closed popup changes row order");
    ui.find("proxy.field")->onClick(); compose();
    require(open.get() && ui.find("proxy.popup")->opacity == 1, "controlled dropdown did not open");
    require(ui.find("form")->orderedChildren.back() == ui.find("form.proxy"), "open popup must paint over later form rows");
    require(near(ui.find("proxy")->frame.height, 28) && near(ui.find("address")->frame.y - ui.find("proxy")->frame.y, 38), "popup changes field layout");
    ui.find("proxy.item.2")->onClick(); compose();
    require(selected.get() == 2 && !open.get(), "dropdown selection must update and close");
    require(ui.find("form")->orderedChildren.front() == ui.find("form.proxy"), "closed popup retains elevated order");
    ui.begin("isolation");
    ui.stack("dialog").zIndex(1000).stackingContext().content([&] {
        ui.rect("child.popup").zIndex(5000).build();
    }).build();
    ui.rect("other.dialog").zIndex(2000).build();
    ui.end(); ui.layout(800, 600);
    require(ui.find("dialog")->subtreeMaxZIndex == 1000 && ui.orderedRoots().back() == ui.find("other.dialog"), "dialog internal layers escape stacking context");
}

void sliderRange() {
    core::dsl::Ui ui;
    eui::Signal<float> value{56};
    const auto compose = [&](bool reverse = false) {
        ui.begin("range");
        components::slider(ui, "limit").size(200, 28).bind(value).range(reverse ? 110.f : 10.f, reverse ? 10.f : 110.f).step(5).build();
        ui.end(); ui.layout(400, 100);
    };
    compose();
    require(near(ui.find("limit")->sliderValue, .45f), "range/step order depends on bind order");
    ui.find("limit")->onSliderValueChanged(.687f);
    require(value.get() == 80, "pointer value not converted to business range/step");
    compose(true);
    ui.find("limit.hit")->onKeyEvent({core::InputKey::Right, core::KeyAction::Press, {}});
    require(value.get() == 85, "keyboard ignores business step");
    compose();
    ui.find("limit.hit")->onKeyEvent({core::InputKey::End, core::KeyAction::Press, {}});
    require(value.get() == 110, "End does not reach maximum");
    const auto move = [&](float minimum, float maximum, float step, float start, core::InputKey key) {
        value.set(start);
        ui.begin("ticks");
        components::slider(ui, "ticks").range(minimum, maximum).step(step).bind(value).build();
        ui.end(); ui.layout(400, 100);
        ui.find("ticks.hit")->onKeyEvent({key, core::KeyAction::Press, {}});
        return value.get();
    };
    require(near(move(10, 110, 30, 110, core::InputKey::Left), 100), "Left from unmatched maximum skips previous tick");
    require(near(move(10, 110, 30, 100, core::InputKey::Right), 110), "Right must reach unmatched maximum");
    require(near(move(10, 110, 30, 110, core::InputKey::PageDown), 10), "PageDown must clamp to minimum");
    require(near(move(-25, 300, 30, 300, core::InputKey::PageDown), 5), "PageDown must count ten legal ticks from unmatched maximum");
    require(near(move(10, 110, 200, 110, core::InputKey::Down), 10), "step larger than range must reach minimum");
    require(near(move(10, 110, 200, 10, core::InputKey::Up), 110), "step larger than range must reach maximum");
    require(near(move(0, 1.2f, .4f, 1.2f, core::InputKey::Left), .8f), "fractional aligned maximum must not stall");
    require(near(move(10, 110, 0, 110, core::InputKey::PageDown), 100), "continuous PageDown must retain ten percent behavior");
    ui.begin("fixed");
    components::slider(ui, "fixed").range(42, 42).value(100).onChange([&](float v) { value.set(v); }).build();
    ui.end(); ui.layout(400, 100);
    ui.find("fixed")->onSliderValueChanged(.5f);
    require(value.get() == 42 && ui.find("fixed")->sliderValue == 0, "equal endpoints must produce a finite fixed slider");
    require(ui.find("fixed")->disabled, "fixed slider must not accept pointer dragging");
}

void componentLayout() {
    core::dsl::Ui ui;
    ui.begin("layout");
    components::button(ui, "button").position(10, 20).size(100, 28).zIndex(7).build();
    components::input(ui, "input").position(10, 20).size(100, 28).zIndex(7).build();
    components::checkbox(ui, "checkbox").position(10, 20).size(100, 28).zIndex(7).build();
    components::radio(ui, "radio").position(10, 20).size(100, 28).zIndex(7).build();
    components::toggleSwitch(ui, "switch").position(10, 20).size(100, 28).zIndex(7).build();
    components::slider(ui, "slider").position(10, 20).size(100, 28).zIndex(7).build();
    components::stepper(ui, "stepper").position(10, 20).size(100, 28).zIndex(7).build();
    components::dropdown(ui, "dropdown").position(10, 20).size(100, 28).zIndex(7).build();
    components::dataTable(ui, "table").position(10, 20).size(100, 28).zIndex(7).build();
    ui.end(); ui.layout(800, 600);
    for (const auto* id : {"button", "input", "checkbox", "radio", "switch", "slider", "stepper", "dropdown", "table"}) {
        const auto* node = ui.find(id);
        require(node && near(node->frame.x, 10) && near(node->frame.y, 20) && node->zIndex == 7, "component root position/zIndex not applied");
    }
    ui.begin("flow");
    ui.row("row").size(400, 28).gap(10).content([&] {
        components::button(ui, "a").size(100, 28).build();
        components::input(ui, "b").size(100, 28).flexGrow().margin(5, 0).build();
    }).build();
    ui.end(); ui.layout(800, 600);
    require(near(ui.find("b")->frame.x, 115) && near(ui.find("b")->frame.width, 280), "component margin/flexGrow do not participate in Row layout");

    ui.begin("layout-contract");
    const auto check = [&](auto builder, const char* id) {
        builder.position(10, 20).zIndex(7).flexGrow(2).flexShrink(1).ignoreLayout().build();
        const auto* node = ui.find(id);
        require(node && node->hasX && node->hasY && node->x == 10 && node->y == 20 && node->zIndex == 7 &&
                node->flexGrow == 2 && node->flexShrink == 1 && node->ignoreLayout, "component layout contract incomplete");
    };
    check(components::card(ui, "card"), "card");
    check(components::carousel(ui, "carousel"), "carousel");
    check(components::markdown(ui, "markdown"), "markdown");
    check(components::markdown(ui, "markdown.viewport").viewport(0, 100), "markdown.viewport");
    check(components::mouseArea(ui, "mouse"), "mouse");
    check(components::navbar(ui, "navbar"), "navbar");
    check(components::progress(ui, "progress"), "progress");
    check(components::scroll(ui, "scroll"), "scroll");
    check(components::scrollView(ui, "scrollview"), "scrollview");
    check(components::segmented(ui, "segmented"), "segmented");
    check(components::sidebar(ui, "sidebar"), "sidebar");
    check(components::tabs(ui, "tabs"), "tabs");
    check(components::virtualList(ui, "list"), "list");
    check(components::virtualMasonry(ui, "masonry"), "masonry");
    check(components::barChart(ui, "bars"), "bars");
    check(components::lineChart(ui, "line"), "line");
    check(components::pieChart(ui, "pie"), "pie");
    ui.end();
}

void sizing() {
    using app::detail::nativeWindowSize;
    require(nativeWindowSize(800, 600, 1.5f, 1, 1) == std::make_pair(1200, 900), "Windows logical sizing");
    require(nativeWindowSize(800, 600, 2, 2, 1.25f) == std::make_pair(1000, 750), "Retina logical sizing");
    require(nativeWindowSize(800, 600, 1, 1, 1) == std::make_pair(800, 600), "normal DPI sizing");
    require(nativeWindowSize(0, -1, 0, 0, std::numeric_limits<float>::infinity()) == std::make_pair(1, 1), "invalid sizing inputs");
    app::DslAppConfig config;
    config.logicalWindowSize(800, 600).uiScale(std::numeric_limits<float>::infinity());
    require(config.logicalWindowSizeValue && config.uiScaleValue == 1, "logical config/invalid UI scale");
    config.windowSize(640, 480);
    require(!config.logicalWindowSizeValue, "native size must reset unit mode");
}

void menu() {
    core::dsl::Ui ui;
    int called = 0;
    const auto compose = [&] {
        ui.begin("menu");
        auto disabled = components::ContextMenuItem("Unavailable", [&] { called += 100; }); disabled.enabled = false;
        components::menuBar(ui, "bar").screen(800, 600).menus({
            {"File", {{"Open", [&] { ++called; }}, components::ContextMenuItem::separatorItem(), disabled}},
            {"Edit", {{"Copy", [&] { called += 10; }}}}
        }).build();
        ui.end(); ui.layout(800, 600);
    };
    compose(); ui.find("bar.header.0")->onClick(); compose();
    auto* item = ui.find("bar.popup.0.level.0.item.0");
    require(item && !item->activateOnFocus, "menu navigation would activate actions");
    require(ui.find("bar.popup.0.level.0.item.2")->disabled, "disabled menu action is interactive");
    require(!ui.find("bar.popup.0.dismiss.top"), "outside dismiss covers menu bar");
    item->onKeyEvent({core::InputKey::Right, core::KeyAction::Press, {}}); compose();
    ui.find("bar.popup.1.level.0.item.0")->onClick(); compose();
    require(called == 10 && ui.find("bar.popup.1")->accessibleHidden, "menu switching/action/dismiss failed");
}
}

int main() { formAndPopup(); sliderRange(); componentLayout(); sizing(); menu(); return failures ? 1 : 0; }
