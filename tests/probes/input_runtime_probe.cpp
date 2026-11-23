#include "core/dsl_runtime.h"
#include "core/input/input_state.h"
#include "core/window/window_backend.h"
#include "components/input.h"

#if defined(EUI_WINDOW_BACKEND_SDL2)
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#else
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#endif

#include <iostream>
#include <vector>

namespace {

core::window::RenderApi configuredRenderApi() {
#if defined(EUI_RENDER_BACKEND_VULKAN)
    return core::window::RenderApi::Vulkan;
#else
    return core::window::RenderApi::OpenGL;
#endif
}

bool initializeWindowBackend() {
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_SetMainReady();
    return SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0;
#else
    return glfwInit() == GLFW_TRUE;
#endif
}

void terminateWindowBackend() {
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_Quit();
#else
    glfwTerminate();
#endif
}

} // namespace

int main() {
    if (!initializeWindowBackend()) {
        return 1;
    }

    core::window::WindowCreateRequest request;
    request.width = 200;
    request.height = 160;
    request.title = "Input Runtime Probe";
    request.resizable = false;
    request.renderApi = configuredRenderApi();
    core::window::Handle window = core::window::createWindow(request);
    if (window == nullptr) {
        terminateWindowBackend();
        return 2;
    }

    int presses = 0;
    int releases = 0;
    int middleDrags = 0;
    int rightDrags = 0;
    int contextMenus = 0;
    int focusedKeys = 0;
    int defaultFocusedKeys = 0;
    int applicationKeys = 0;
    int clicks = 0;
    std::vector<core::GestureEvent> gestures;

    core::dsl::Runtime runtime;
    runtime.setKeyEventHandler([&](const core::KeyEvent&) { ++applicationKeys; });
    runtime.initialize(window);
    runtime.compose("input", 200.0f, 160.0f,
        [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
            ui.rect("target")
                .position(10.0f, 10.0f)
                .size(120.0f, 100.0f)
                .acceptedButtons(core::PointerButton::Left |
                                 core::PointerButton::Middle |
                                 core::PointerButton::Right)
                .dragThreshold(4.0f)
                .onPress([&](const core::PointerEvent&, const core::Rect&) {
                    ++presses;
                })
                .onClick([&] { ++clicks; })
                .onRelease([&](const core::PointerEvent&, const core::Rect&) {
                    ++releases;
                })
                .onDrag([&](const core::dsl::DragEvent& event) {
                    if (event.button == core::PointerButton::Middle) {
                        ++middleDrags;
                    } else if (event.button == core::PointerButton::Right) {
                        ++rightDrags;
                    }
                })
                .onContextMenu([&](const core::PointerEvent&, const core::Rect&) {
                    ++contextMenus;
                })
                .onGesture([&](const core::GestureEvent& event) {
                    gestures.push_back(event);
                })
                .onKeyEvent([&](const core::KeyEvent& event) {
                    ++focusedKeys;
                    return event.key == core::InputKey::F1;
                })
                .build();
            ui.rect("default-focus")
                .position(140.0f, 10.0f)
                .size(50.0f, 100.0f)
                .onKeyEvent([&](const core::KeyEvent&) {
                    ++defaultFocusedKeys;
                    return true;
                })
                .build();
        });

    const core::KeyModifiers modifiers{};
    core::queuePointerButton(window, 20.0, 20.0, core::PointerButton::Middle,
                             core::PointerAction::Press, modifiers);
    core::queuePointerMotion(window, 30.0, 20.0,
                             core::PointerButton::Middle, modifiers);
    core::queuePointerButton(window, 30.0, 20.0, core::PointerButton::Middle,
                             core::PointerAction::Release, modifiers);
    runtime.update(window, 1.0f / 60.0f, 1.0f, 1.0f);

    core::queuePointerButton(window, 20.0, 20.0, core::PointerButton::Right,
                             core::PointerAction::Press, modifiers);
    core::queuePointerButton(window, 20.0, 20.0, core::PointerButton::Right,
                             core::PointerAction::Release, modifiers);
    runtime.update(window, 1.0f / 60.0f, 1.0f, 1.0f);

    core::queuePointerButton(window, 20.0, 20.0, core::PointerButton::Right,
                             core::PointerAction::Press, modifiers);
    core::queuePointerMotion(window, 32.0, 20.0,
                             core::PointerButton::Right, modifiers);
    core::queuePointerButton(window, 32.0, 20.0, core::PointerButton::Right,
                             core::PointerAction::Release, modifiers);
    runtime.update(window, 1.0f / 60.0f, 1.0f, 1.0f);

    core::queuePointerButton(window, 20.0, 20.0, core::PointerButton::Left,
                             core::PointerAction::Press, modifiers);
    core::queuePointerButton(window, 20.0, 20.0, core::PointerButton::Left,
                             core::PointerAction::Release, modifiers);
    runtime.update(window, 1.0f / 60.0f, 1.0f, 1.0f);

    core::queuePointerButton(window, 150.0, 20.0, core::PointerButton::Right,
                             core::PointerAction::Press, modifiers);
    core::queuePointerButton(window, 150.0, 20.0, core::PointerButton::Right,
                             core::PointerAction::Release, modifiers);
    runtime.update(window, 1.0f / 60.0f, 1.0f, 1.0f);

    core::GestureEvent gesture;
    gesture.phase = core::GesturePhase::Begin;
    gesture.x = 20.0;
    gesture.y = 20.0;
    gesture.touchCount = 2;
    core::queueGestureInput(window, gesture);
    gesture.phase = core::GesturePhase::Update;
    gesture.x = 150.0;
    gesture.scale = 1.5;
    gesture.rotation = 12.0;
    gesture.deltaX = 3.0;
    core::queueGestureInput(window, gesture);
    gesture.phase = core::GesturePhase::End;
    core::queueGestureInput(window, gesture);
    runtime.update(window, 1.0f / 60.0f, 1.0f, 1.0f);

    gesture.phase = core::GesturePhase::Begin;
    gesture.x = 30.0;
    core::queueGestureInput(window, gesture);
    runtime.update(window, 1.0f / 60.0f, 1.0f, 1.0f);
    runtime.update(window, 1.0f / 60.0f, 1.0f, 1.0f, false);

    core::queuePointerButton(window, 20.0, 20.0, core::PointerButton::Left,
                             core::PointerAction::Press, modifiers, true);
    runtime.update(window, 1.0f / 60.0f, 1.0f, 1.0f);
    core::queueTouchScrollInput(window, 0.0, -20.0);
    core::queuePointerMotion(window, 20.0, 40.0, core::PointerButton::Left, modifiers, true);
    runtime.update(window, 1.0f / 60.0f, 1.0f, 1.0f);
    core::queuePointerButton(window, 20.0, 40.0, core::PointerButton::Left,
                             core::PointerAction::Release, modifiers, true);
    runtime.update(window, 1.0f / 60.0f, 1.0f, 1.0f);

    core::queuePointerButton(window, 20.0, 20.0, core::PointerButton::Left,
                             core::PointerAction::Press, modifiers, true);
    core::queuePointerButton(window, 20.0, 20.0, core::PointerButton::Left,
                             core::PointerAction::Release, modifiers, true);
    runtime.update(window, 1.0f / 60.0f, 1.0f, 1.0f);

    core::queueKeyInput(window, {core::InputKey::F1, core::KeyAction::Press, {}});
    core::queueKeyInput(window, {core::InputKey::F2, core::KeyAction::Press, {}});
    runtime.update(window, 1.0f / 60.0f, 1.0f, 1.0f);

    bool passed = presses == 6 && releases == 6 &&
        middleDrags > 0 && rightDrags > 0 && contextMenus == 1 &&
        focusedKeys == 2 && defaultFocusedKeys == 0 && applicationKeys == 1 && clicks == 5 &&
        gestures.size() == 5 &&
        gestures[0].phase == core::GesturePhase::Begin &&
        gestures[1].phase == core::GesturePhase::Update &&
        gestures[1].x == 150.0 &&
        gestures[1].scale == 1.5 && gestures[1].rotation == 12.0 &&
        gestures[1].deltaX == 3.0 &&
        gestures[2].phase == core::GesturePhase::End &&
        gestures[3].phase == core::GesturePhase::Begin &&
        gestures[4].phase == core::GesturePhase::Cancel &&
        gestures[4].x == 30.0;
    if (!passed) {
        std::cerr << "Input Runtime dispatch failed: presses=" << presses
                  << " releases=" << releases
                  << " middleDrags=" << middleDrags
                  << " rightDrags=" << rightDrags
                  << " contextMenus=" << contextMenus
                  << " clicks=" << clicks
                  << " focusedKeys=" << focusedKeys
                  << " defaultFocusedKeys=" << defaultFocusedKeys
                  << " applicationKeys=" << applicationKeys << "\n";
    }

    std::vector<char> touchOrder;
    runtime.compose("touch-order", 200.f, 160.f, [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
        ui.rect("touch-target").size(120.f, 100.f)
            .onPress([&](const core::PointerEvent&, const core::Rect&) { touchOrder.push_back('P'); })
            .onRelease([&](const core::PointerEvent& event, const core::Rect&) {
                if (event.action == core::PointerAction::Cancel) touchOrder.push_back('C');
            })
            .onClick([&] { touchOrder.push_back('X'); })
            .onGesture([&](const core::GestureEvent& event) {
                if (event.phase == core::GesturePhase::Begin) touchOrder.push_back('G');
            }).build();
    });
    core::queuePointerButton(window, 20.0, 20.0, core::PointerButton::Left,
                             core::PointerAction::Press, modifiers, true);
    core::cancelTouchPointerInput(window);
    gesture = {};
    gesture.phase = core::GesturePhase::Begin;
    gesture.x = 20.0;
    gesture.y = 20.0;
    core::queueGestureInput(window, gesture);
    gesture.phase = core::GesturePhase::End;
    core::queueGestureInput(window, gesture);
    core::queuePointerButton(window, 20.0, 20.0, core::PointerButton::Left,
                             core::PointerAction::Press, modifiers, true);
    core::queuePointerButton(window, 20.0, 20.0, core::PointerButton::Left,
                             core::PointerAction::Release, modifiers, true);
    runtime.update(window, 1.f / 60.f, 1.f, 1.f);
    passed = passed && touchOrder == std::vector<char>{'P', 'C', 'G', 'P', 'X'};
    if (touchOrder != std::vector<char>{'P', 'C', 'G', 'P', 'X'}) {
        std::cerr << "Touch and gesture events must retain their cross-stream order\n";
    }

    int backgroundGestures = 0;
    const auto composeGestureBlock = [&](bool disabled) {
        runtime.compose("gesture.block", 200.f, 160.f, [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
            ui.rect("background").size(100.f, 100.f)
                .onGesture([&](const core::GestureEvent&) { ++backgroundGestures; }).build();
            ui.rect("overlay").size(100.f, 100.f).blockPointer().disabled(disabled).build();
        });
        gesture = {};
        gesture.x = 20;
        gesture.y = 20;
        gesture.phase = core::GesturePhase::Begin;
        core::queueGestureInput(window, gesture);
        gesture.phase = core::GesturePhase::End;
        core::queueGestureInput(window, gesture);
        runtime.update(window, 1.f / 60.f, 1.f, 1.f);
    };
    composeGestureBlock(false);
    passed = passed && backgroundGestures == 0;
    composeGestureBlock(true);
    passed = passed && backgroundGestures == 2;
    if (backgroundGestures != 2) std::cerr << "Gesture overlay blocking or disabled hit testing failed\n";

    // 经由 Runtime 的命中、捕获和焦点分发验证输入框滚动条，而非直接调用回调。
    std::string document;
    for (int i = 0; i < 2000; ++i) document += "row " + std::to_string(i) + "\n";
    core::dsl::Ui* editorUi = nullptr;
    using InputState = components::input_detail::InputModel::InputState;
    InputState* editorState = nullptr;
    const auto composeEditor = [&] {
        runtime.compose("editor", 200.f, 160.f, [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
            editorUi = &ui;
            components::input(ui, "field").position(10.f, 10.f).size(180.f, 140.f)
                .inset(10.f).fontSize(16.f).multiline().scrollbar().value(document)
                .onChange([&](const std::string& value) { document = value; }).build();
            editorState = &ui.state<InputState>("field");
        });
    };
    const auto updateEditor = [&] {
        runtime.update(window, 1.f / 60.f, 1.f, 1.f);
        composeEditor();
    };
    const auto click = [&](double x, double y) {
        core::queuePointerButton(window, x, y, core::PointerButton::Left, core::PointerAction::Press, modifiers);
        core::queuePointerButton(window, x, y, core::PointerButton::Left, core::PointerAction::Release, modifiers);
        updateEditor();
    };
    composeEditor();
    runtime.update(window, 1.f / 60.f, 1.f, 1.f);
    click(30, 30);
    const auto* field = editorUi->find("field.hit");
    const auto focusBorder = components::InputStyle{}.focusBorder;
    passed = passed && editorUi->isFocused("field.hit") && !editorUi->find("field.hit.focusRing") && field &&
        field->border.color.r == focusBorder.r && field->border.color.g == focusBorder.g &&
        field->border.color.b == focusBorder.b && field->border.color.a == focusBorder.a;
    core::KeyModifiers selectModifiers;
    selectModifiers.control = true;
    selectModifiers.super = true;
    core::queueKeyInput(window, {core::InputKey::A, core::KeyAction::Press, selectModifiers});
    updateEditor();
    const int selectionStart = editorState->selectionStart;
    const int selectionEnd = editorState->selectionEnd;
    auto* track = editorUi->find("field.scrollbar.track");
    if (!track) {
        std::cerr << "Overflow editor has no scrollbar\n";
        passed = false;
    } else {
        const auto trackFrame = track->frame;
        click(trackFrame.x + trackFrame.width * 0.5f, trackFrame.y + 1.f);
        passed = passed && editorState->verticalScroll == 0.f && editorUi->isFocused("field.hit");
        const auto thumbFrame = editorUi->find("field.scrollbar.thumb")->frame;
        const double x = thumbFrame.x + thumbFrame.width * 0.5f;
        const double y = thumbFrame.y + thumbFrame.height * 0.5f;
        const double travel = trackFrame.height - thumbFrame.height;
        core::queuePointerButton(window, x, y, core::PointerButton::Left, core::PointerAction::Press, modifiers);
        updateEditor();
        core::queuePointerMotion(window, x, y + travel * 0.25, core::PointerButton::Left, modifiers);
        updateEditor();
        const float quarterOffset = editorState->verticalScroll;
        core::queuePointerMotion(window, x, y + travel * 0.5, core::PointerButton::Left, modifiers);
        updateEditor();
        const float halfOffset = editorState->verticalScroll;
        core::queuePointerButton(window, x, y + travel * 0.5, core::PointerButton::Left, core::PointerAction::Release, modifiers);
        updateEditor();
        passed = passed && quarterOffset > 0.f && std::fabs(halfOffset - quarterOffset * 2.f) < 1.f &&
            std::fabs(editorState->verticalScroll - halfOffset) < 1.f &&
            editorState->selectionStart == selectionStart && editorState->selectionEnd == selectionEnd &&
            selectionStart != selectionEnd && editorUi->isFocused("field.hit");
        // 滑块上的滚轮也应滚动文本；之后输入必须仍送到原输入框。
        core::queueScrollInput(window, 0, -1);
        updateEditor();
        passed = passed && editorState->verticalScroll > halfOffset;
        core::queueTextInput(window, "X");
        updateEditor();
        passed = passed && document == "X" && editorState->followCaret &&
            editorState->verticalScroll == 0.f && !editorUi->find("field.scrollbar.thumb");
        if (!passed) std::cerr << "Input scrollbar Runtime drag/focus/selection/wheel/type regression failed\n";
    }

    document = "prefix suffix";
    composeEditor();
    using Model = components::input_detail::InputModel;
    Model::moveCursorTo(*editorState, 7, false);
    core::queueTextEditing(window, "中文测试");
    updateEditor();
    passed = passed && editorState->preedit && editorState->preedit->text == "prefix 中文测试suffix" &&
        document == "prefix suffix" && !editorUi->find("field.composition") &&
        editorUi->find("field.composition.underline.0");
    if (editorState->preedit) {
        std::string rendered;
        for (size_t i = 0; i < editorState->preedit->cachedLines.size(); ++i) {
            const auto* line = editorUi->find("field.text." + std::to_string(i));
            if (line) rendered += line->text;
        }
        passed = passed && rendered == "prefix 中文测试suffix";
    }
    core::queueTextEditing(window, "");
    updateEditor();
    passed = passed && !editorState->preedit && document == "prefix suffix";
    editorState->selectionStart = 7;
    editorState->selectionEnd = static_cast<int>(document.size());
    editorState->cursor = editorState->selectionEnd;
    core::queueTextEditing(window, "替换");
    updateEditor();
    passed = passed && editorState->preedit && editorState->preedit->text == "prefix 替换";
    core::queueTextEditing(window, "");
    core::queueTextInput(window, "替换");
    updateEditor();
    passed = passed && !editorState->preedit && document == "prefix 替换";
    core::queueKeyInput(window, {core::InputKey::Z, core::KeyAction::Press, selectModifiers});
    updateEditor();
    passed = passed && document == "prefix suffix";
    if (!passed) std::cerr << "Input Runtime preedit inline layout, cancel, replacement or undo failed\n";

    runtime.shutdown(false);
    core::releaseInputQueue(window);
    core::window::destroyWindow(window);
    terminateWindowBackend();
    return passed ? 0 : 3;
}
