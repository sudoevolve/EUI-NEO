#include "core/dsl_runtime.h"
#include "components/components.h"
#include <iostream>
#if defined(EUI_WINDOW_BACKEND_SDL2)
#define SDL_MAIN_HANDLED
#include <SDL.h>
#else
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

int main() {
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) return 20;
#else
    if (!glfwInit()) return 20;
#endif
    struct WindowLifetime {
        core::window::Handle handle = nullptr;
        ~WindowLifetime() {
            if (handle) core::window::destroyWindow(handle);
#if defined(EUI_WINDOW_BACKEND_SDL2)
            SDL_Quit();
#else
            glfwTerminate();
#endif
        }
    } lifetime;
    core::window::WindowCreateRequest request;
    request.width = 640; request.height = 480; request.title = "Accessibility Probe";
#if defined(EUI_RENDER_BACKEND_VULKAN)
    request.renderApi = core::window::RenderApi::Vulkan;
#endif
    lifetime.handle = core::window::createWindow(request);
    if (!lifetime.handle) return 21;
    core::dsl::Runtime runtime;
    int clicks = 0;
    auto window = lifetime.handle;
    bool modal = false, checked = false;
    float value = 0.5f;
    std::string focus;
    std::vector<core::AccessibleNode> semantics;
    core::dsl::Ui* currentUi = nullptr;
    const auto compose = [&] {
        runtime.compose("keys", 640, 480, [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
            currentUi = &ui;
            ui.stack("root").size(640, 480).content([&] {
                components::button(ui, "a").text("Open").onClick([&] { ++clicks; }).build();
                ui.stack("disabled").y(45).size(200, 40).disabled().content([&] {
                    components::button(ui, "skip").onClick([&] { clicks += 100; }).build();
                }).build();
                ui.stack("box").y(90).size(200, 40).content([&] {
                    components::checkbox(ui, "check").text("Enabled").checked(checked).onChange([&](bool x) { checked = x; }).build();
                }).build();
                ui.stack("slide").y(140).size(250, 40).content([&] {
                    components::slider(ui, "s").value(value).onChange([&](float x) { value = x; }).build();
                }).build();
                if (modal) components::dialog(ui, "dialog").open(true).screen(640, 480)
                    .onOpenChange([&](bool x) { modal = x; }).build();
            }).build();
            for (const auto& id : {"a.bg", "check.hit", "s.hit"}) if (ui.isFocused(id)) focus = id;
            semantics = ui.accessibilityTree();
        });
    };
    const auto key = [&](core::InputKey key, bool shift = false) {
        core::KeyModifiers modifiers; modifiers.shift = shift;
        core::queueKeyInput(window, {key, core::KeyAction::Press, modifiers});
        runtime.update(window, 0.016f, 1, 1);
        focus.clear();
        compose();
    };
    compose();
    key(core::InputKey::Tab);
    if (focus != "a.bg") { std::cerr << "first focus=" << focus << "\n"; for (const auto& node : semantics) std::cerr << node.id << " focused=" << node.focused << " disabled=" << node.disabled << "\n"; return 1; }
    key(core::InputKey::Enter);
    if (clicks != 1) return 2;
    key(core::InputKey::Tab);
    if (focus != "check.hit") return 3;
    key(core::InputKey::Space);
    if (!checked) return 4;
    key(core::InputKey::Tab);
    key(core::InputKey::Right);
    if (value < 0.509f) return 5;
    key(core::InputKey::Tab);
    if (focus != "a.bg") return 6;
    const auto* ring = currentUi->find("a.bg.focusRing");
    if (!ring || ring->frame.width <= 0 || ring->frame.height <= 0) return 11;
    key(core::InputKey::Tab, true);
    if (focus != "s.hit") return 7;
    modal = true;
    compose();
    key(core::InputKey::Tab);
    if (!focus.empty()) return 8;
    key(core::InputKey::Escape);
    compose();
    if (modal || focus != "s.hit") return 9;
    bool found = false;
    for (const auto& node : semantics) if (node.role == core::AccessibleRole::CheckBox) found = node.name == "Enabled" && node.checked;
    int selectedTab = 0, choice = 0;
    bool dropdownOpen = false;
    const auto groups = [&] {
        runtime.compose("groups", 640, 480, [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
            currentUi = &ui;
            components::tabs(ui, "tabs").items({"One", "Two", "Three"}).selected(selectedTab)
                .onChange([&](int x) { selectedTab = x; }).build();
            ui.stack("below").y(80).size(200, 200).content([&] {
                components::dropdown(ui, "choices").items({"A", "B", "C"}).selected(choice)
                    .open(dropdownOpen).onChange([&](int x) { choice = x; })
                    .onOpenChange([&](bool x) { dropdownOpen = x; }).build();
            }).build();
        });
    };
    const auto groupKey = [&](core::InputKey key) {
        core::queueKeyInput(window, {key, core::KeyAction::Press, {}});
        runtime.update(window, 0.016f, 1, 1);
        groups();
    };
    groups();
    groupKey(core::InputKey::Tab);
    groupKey(core::InputKey::Right);
    if (selectedTab != 1 || !currentUi->isFocused("tabs.hit.1")) return 12;
    groupKey(core::InputKey::Tab);
    groupKey(core::InputKey::Tab);
    groupKey(core::InputKey::End);
    if (choice != 2) return 13;
    groupKey(core::InputKey::Enter);
    if (!dropdownOpen) return 14;
    groupKey(core::InputKey::Tab);
    if (!currentUi->isFocused("choices.item.0")) return 16;
    groupKey(core::InputKey::Escape);
    if (dropdownOpen || !currentUi->isFocused("choices.field")) return 15;
    bool pickerOpen = true;
    int month = 1;
    const auto date = [&] {
        runtime.compose("date", 640, 480, [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
            currentUi = &ui;
            components::datePicker(ui, "date").screen(640, 480).open(pickerOpen).date(2026, month, 15)
                .onOpenChange([&](bool value) { pickerOpen = value; })
                .onChange([&](int, int value, int) { month = value; }).build();
        });
    };
    const auto dateKey = [&](core::InputKey key, bool shift = false) {
        core::KeyModifiers modifiers; modifiers.shift = shift;
        core::queueKeyInput(window, {key, core::KeyAction::Press, modifiers});
        runtime.update(window, .016f, 1, 1); date();
    };
    date();
    dateKey(core::InputKey::Tab);
    if (!currentUi->isFocused("date.column.0.hit")) return 17;
    dateKey(core::InputKey::Right);
    dateKey(core::InputKey::Tab, true);
    dateKey(core::InputKey::Enter);
    if (pickerOpen || month != 2) return 18;
    runtime.shutdown();
    core::releaseInputQueue(window);
    if (!found) std::cerr << "checkbox semantics missing\n";
    return found ? 0 : 10;
}
