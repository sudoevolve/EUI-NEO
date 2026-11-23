#include "components/dropdown.h"
#include "components/slider.h"
#include "components/menubar.h"
#include "components/datatable.h"
#include "components/input.h"
#include "core/dsl_runtime.h"

#if defined(EUI_RENDER_BACKEND_OPENGL)
#include <glad/glad.h>
#endif
#if defined(EUI_WINDOW_BACKEND_SDL2)
#define SDL_MAIN_HANDLED
#include <SDL.h>
#if defined(EUI_RENDER_BACKEND_VULKAN)
#include <SDL_vulkan.h>
#endif
#else
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

#include <iostream>

int main() {
    core::render::initializeRenderBackendLoader();
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
#else
    if (!glfwInit()) return 1;
#endif
    core::window::WindowCreateRequest request;
    request.visible = false; request.width = 800; request.height = 600;
    request.renderApi = core::render::windowRenderApi();
    auto window = core::window::createWindow(request);
    auto backend = core::render::createRenderBackend(window);
    if (!window || !backend || !backend->initialize()) return 1;
    int failures = 0;
    const auto require = [&](bool value, const char* message) {
        if (!value) { std::cerr << message << '\n'; ++failures; }
    };
    {
        core::render::ScopedRenderBackend scope(*backend);
        core::dsl::Runtime runtime;
        runtime.initialize(window);
        eui::Signal<bool> open{false};
        eui::Signal<int> choice{0};
        eui::Signal<float> value{10};
        int outsideClicks = 0;
        eui::Signal<std::string> text;
        core::dsl::Ui* current = nullptr;
        int w = 0, h = 0;
#if defined(EUI_WINDOW_BACKEND_SDL2)
#if defined(EUI_RENDER_BACKEND_VULKAN)
        SDL_Vulkan_GetDrawableSize(static_cast<SDL_Window*>(window), &w, &h);
#else
        SDL_GL_GetDrawableSize(static_cast<SDL_Window*>(window), &w, &h);
#endif
#else
        glfwGetFramebufferSize(static_cast<GLFWwindow*>(window), &w, &h);
#endif
        for (float scale : {1.f, 1.5f, 2.f}) {
            const auto compose = [&] {
                runtime.compose("popup", w / scale, h / scale, [&](auto& ui, const auto& screen) {
                    current = &ui;
                    ui.stack("root").size(screen.width, screen.height).content([&] {
                        ui.stack("wrap").position(20, 20).size(200, 28).zIndex(73).content([&] {
                            components::dropdown(ui, "choice").size(200, 28).items({"System", "HTTP"})
                                .theme(components::theme::light()).bind(choice).bindOpen(open).transition(core::Transition::make(0)).build();
                        }).build();
                        ui.rect("later").position(20, 65).size(200, 90).zIndex(73).color({1, 0, 0, 1}).build();
                        ui.rect("outside").position(250, 160).size(100, 28).onClick([&] { ++outsideClicks; }).build();
                        components::input(ui, "outside.input").position(250, 195).size(100, 28).bind(text).build();
                        components::slider(ui, "range").position(20, 220).size(200, 28).range(10, 110).step(5).bind(value).build();
                    }).build();
                });
            };
            const auto render = [&] {
                runtime.update(window, 0, 1, scale);
                backend->beginFrame({window, core::window::nativeWindowInfo(window), w, h, scale});
                runtime.render(w, h, scale, {0, 0, 0, 1});
#if defined(EUI_RENDER_BACKEND_OPENGL)
                unsigned char pixel[4]{};
                glReadPixels(static_cast<int>(210 * scale), h - static_cast<int>(75 * scale), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
                if (open.get()) require(pixel[0] > 230 && pixel[1] > 230 && pixel[2] > 230, "later field paints through open dropdown");
                else require(pixel[0] > 230 && pixel[1] < 30 && pixel[2] < 30, "closed dropdown leaves visible popup pixels");
#endif
                backend->present();
            };
            const auto click = [&](float x, float y) {
                core::queuePointerButton(window, x * scale, y * scale, core::PointerButton::Left, core::PointerAction::Press, {});
                runtime.update(window, 0, 1, scale);
                core::queuePointerButton(window, x * scale, y * scale, core::PointerButton::Left, core::PointerAction::Release, {});
                runtime.update(window, 0, 1, scale);
            };
            compose(); render();
            click(40, 30); require(open.get(), "field click did not open dropdown");
            compose(); render();
            const int before = outsideClicks;
            click(270, 175);
            require(!open.get() && outsideClicks == before + 1, "outside click must close dropdown and activate its target");
            compose(); render();
            click(40, 30); compose(); render();
            click(270, 205); compose(); render();
            require(!open.get() && current->isFocused("outside.input.hit"), "closing dropdown stole outside input focus");
            const auto textLength = text.get().size();
            core::queueTextInput(window, "x");
            runtime.update(window, 0, 1, scale); compose();
            require(text.get().size() == textLength + 1, "outside input cannot type after closing dropdown");
            click(40, 30); compose(); render();
            click(40, 112);
            require(choice.get() == 1 && !open.get(), "popup option outside field bounds did not receive click");
            compose(); render();
            click(94, 234);
            require(value.get() >= 45 && value.get() <= 50, "pointer range conversion/step failed");
            const float ticks = (value.get() - 10) / 5;
            require(std::fabs(ticks - std::round(ticks)) < .001f, "pointer value not snapped");
        }
        int menuActions = 0;
        const auto menu = [&] {
            runtime.compose("keyboard", 800, 600, [&](auto& ui, const auto&) {
                current = &ui;
                auto disabled = components::ContextMenuItem("Disabled"); disabled.enabled = false;
                components::menuBar(ui, "bar").screen(800, 600).menus({
                    {"File", {{"Open", [&] { ++menuActions; }}, disabled, components::ContextMenuItem::separatorItem(),
                        {"Submenu", std::vector<components::ContextMenuItem>{{"Nested", [&] { menuActions += 10; }}}}}},
                    {"Edit", {{"Copy", [&] { menuActions += 100; }}}}
                }).build();
            });
        };
        const auto key = [&](core::InputKey code) {
            core::queueKeyInput(window, {code, core::KeyAction::Press, {}});
            runtime.update(window, 0, 1, 1); menu();
        };
        menu(); key(core::InputKey::Tab); key(core::InputKey::Enter);
        require(current->isFocused("bar.popup.0.level.0.item.0"), "opening menu did not focus first action");
        key(core::InputKey::Down);
        require(menuActions == 0 && current->isFocused("bar.popup.0.level.0.item.3"), "menu arrows activate an action or fail to skip disabled items");
        key(core::InputKey::Right);
        require(current->isFocused("bar.popup.0.level.1.item.0"), "submenu keyboard focus did not enter child");
        key(core::InputKey::Left);
        require(current->isFocused("bar.popup.0.level.0.item.3"), "submenu keyboard focus did not return to parent");
        key(core::InputKey::Right); key(core::InputKey::Enter);
        require(menuActions == 10 && current->find("bar.popup.0")->accessibleHidden, "nested action did not close menu");

        auto rows = std::make_shared<components::DataTableRows>(components::DataTableRows{{"A"}, {"B"}, {"C"}});
        eui::Signal<std::vector<std::string>> selected;
        const auto table = [&] {
            runtime.compose("selection", 800, 600, [&](auto& ui, const auto&) {
                current = &ui;
                components::dataTable(ui, "table").size(500, 250).columns({"Name"}).data(rows)
                    .sortable(false).resizable(false).bindSelection(selected).build();
            });
        };
        const auto tableKey = [&](core::InputKey code) {
            core::queueKeyInput(window, {code, core::KeyAction::Press, {}});
            runtime.update(window, 0, 1, 1); table();
        };
        table(); tableKey(core::InputKey::Tab); tableKey(core::InputKey::Space);
        require(selected.get() == std::vector<std::string>{"0"}, "table keyboard initial selection");
        tableKey(core::InputKey::Down); tableKey(core::InputKey::Down);
        require(selected.get() == std::vector<std::string>{"2"}, "table keyboard repeated movement");
        require(current->isFocused("table.body.slot.2.bg"), "table focus did not move with selection");
        require(current->find("table.body.slot.2.bg.focusRing") != nullptr &&
                current->find("table.body.slot.1.bg.focusRing") == nullptr, "requested keyboard focus ring did not follow selection");
        tableKey(core::InputKey::Tab); tableKey(core::InputKey::Space);
        require(selected.get() == std::vector<std::string>{"0"}, "table keyboard selection did not follow Tab focus");
        runtime.shutdown();
    }
    backend.reset();
    core::releaseInputQueue(window);
    core::window::destroyWindow(window);
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_Quit();
#else
    glfwTerminate();
#endif
    return failures ? 1 : 0;
}
