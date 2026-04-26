#pragma once

#include "app/app.h"

#include <glad/glad.h>

#include "core/dsl_runtime.h"

namespace app {

struct DslAppConfig {
    const char* title = "DSL App";
    const char* pageId = "app";
    core::Color clearColor = {0.16f, 0.18f, 0.20f, 1.0f};
    int windowWidth = 800;
    int windowHeight = 600;
};

const DslAppConfig& dslAppConfig();
void compose(core::dsl::Ui& ui, const core::dsl::Screen& screen);

namespace detail {

inline core::dsl::Runtime& dslRuntime() {
    static core::dsl::Runtime runtime;
    return runtime;
}

} // namespace detail

const char* windowTitle() {
    return dslAppConfig().title;
}

int initialWindowWidth() {
    return dslAppConfig().windowWidth;
}

int initialWindowHeight() {
    return dslAppConfig().windowHeight;
}

bool initialize(GLFWwindow*) {
    return detail::dslRuntime().initialize();
}

bool update(GLFWwindow* window, float deltaSeconds, int windowWidth, int windowHeight, float dpiScale, float pointerScale) {
    const DslAppConfig& config = dslAppConfig();
    const float logicalWidth = static_cast<float>(windowWidth) / dpiScale;
    const float logicalHeight = static_cast<float>(windowHeight) / dpiScale;

    const auto composeFrame = [&] {
        detail::dslRuntime().compose(config.pageId, logicalWidth, logicalHeight, [](core::dsl::Ui& ui, const core::dsl::Screen& screen) {
            compose(ui, screen);
        });
    };

    composeFrame();
    const bool changed = detail::dslRuntime().update(window, deltaSeconds, pointerScale, dpiScale);
    if (changed) {
        composeFrame();
    }

    return changed;
}

bool isAnimating() {
    return detail::dslRuntime().isAnimating();
}

void render(int windowWidth, int windowHeight, float dpiScale) {
    const core::Color clearColor = dslAppConfig().clearColor;
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT);
    detail::dslRuntime().render(windowWidth, windowHeight, dpiScale);
}

void shutdown() {
    detail::dslRuntime().shutdown();
}

} // namespace app
