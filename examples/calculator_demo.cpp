#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_GLFW
#include "EUI.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {

using eui::Rect;
using eui::quick::UI;

struct CalculatorState {
    std::string history{"0"};
    std::string display{"0"};
    double accumulator{0.0};
    bool has_accumulator{false};
    char pending_op{0};
    bool reset_input{false};
    bool history_locked{false};
    bool dark_mode{false};
};

const char* calc_op_text(char op) {
    switch (op) {
        case '+':
            return "+";
        case '-':
            return "-";
        case '*':
            return "*";
        case '/':
            return "/";
        default:
            return "";
    }
}

std::string calc_format(double value) {
    if (!std::isfinite(value)) {
        return "Error";
    }
    if (std::fabs(value) < 1e-12) {
        value = 0.0;
    }
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.12g", value);
    return std::string(buffer);
}

double calc_parse(const std::string& text) {
    if (text == "Error") {
        return 0.0;
    }
    try {
        const double parsed = std::stod(text);
        if (std::isfinite(parsed)) {
            return parsed;
        }
    } catch (...) {
    }
    return 0.0;
}

bool calc_apply(double& lhs, double rhs, char op) {
    switch (op) {
        case '+':
            lhs += rhs;
            return true;
        case '-':
            lhs -= rhs;
            return true;
        case '*':
            lhs *= rhs;
            return true;
        case '/':
            if (std::fabs(rhs) < 1e-12) {
                return false;
            }
            lhs /= rhs;
            return true;
        default:
            return true;
    }
}

void calc_clear(CalculatorState& state) {
    state.history = "0";
    state.display = "0";
    state.accumulator = 0.0;
    state.has_accumulator = false;
    state.pending_op = 0;
    state.reset_input = false;
    state.history_locked = false;
}

void calc_set_error(CalculatorState& state) {
    state.history = "Error";
    state.display = "Error";
    state.accumulator = 0.0;
    state.has_accumulator = false;
    state.pending_op = 0;
    state.reset_input = true;
    state.history_locked = false;
}

void calc_sync_history(CalculatorState& state) {
    if (state.display == "Error") {
        state.history = "Error";
        return;
    }
    if (state.pending_op != 0 && state.has_accumulator) {
        const std::string lhs = calc_format(state.accumulator);
        const std::string op = calc_op_text(state.pending_op);
        if (state.reset_input) {
            state.history = lhs + " " + op;
        } else {
            state.history = lhs + " " + op + " " + state.display;
        }
        return;
    }
    if (!state.history_locked) {
        state.history = state.display;
    }
}

void calc_input_digit(CalculatorState& state, char ch) {
    if (state.display == "Error") {
        calc_clear(state);
    }
    if (state.history_locked) {
        state.history_locked = false;
    }
    if (state.reset_input) {
        state.display = (ch == '.') ? "0." : std::string(1, ch);
        state.reset_input = false;
        if (state.pending_op == 0) {
            state.has_accumulator = false;
        }
        calc_sync_history(state);
        return;
    }
    if (ch == '.') {
        if (state.display.find('.') == std::string::npos) {
            state.display.push_back('.');
        }
        calc_sync_history(state);
        return;
    }
    if (state.display == "0") {
        state.display = std::string(1, ch);
    } else if (state.display.size() < 18u) {
        state.display.push_back(ch);
    }
    calc_sync_history(state);
}

void calc_input_operator(CalculatorState& state, char op) {
    if (state.display == "Error") {
        calc_clear(state);
    }
    state.history_locked = false;
    const double current = calc_parse(state.display);
    if (!state.has_accumulator) {
        state.accumulator = current;
        state.has_accumulator = true;
    } else if (state.pending_op != 0 && !state.reset_input) {
        if (!calc_apply(state.accumulator, current, state.pending_op)) {
            calc_set_error(state);
            return;
        }
        state.display = calc_format(state.accumulator);
    }
    state.pending_op = op;
    state.reset_input = true;
    calc_sync_history(state);
}

void calc_eval_equal(CalculatorState& state) {
    if (state.pending_op == 0) {
        return;
    }
    const double lhs_before = state.accumulator;
    const char op_before = state.pending_op;
    double rhs = calc_parse(state.display);
    if (state.reset_input && state.has_accumulator) {
        rhs = state.accumulator;
    }
    if (!state.has_accumulator) {
        state.accumulator = rhs;
        state.has_accumulator = true;
    }
    if (!calc_apply(state.accumulator, rhs, state.pending_op)) {
        calc_set_error(state);
        return;
    }
    state.display = calc_format(state.accumulator);
    state.history = calc_format(lhs_before) + " " + calc_op_text(op_before) + " " + calc_format(rhs) + " =";
    state.pending_op = 0;
    state.has_accumulator = false;
    state.reset_input = true;
    state.history_locked = true;
}

void calc_clear_entry(CalculatorState& state) {
    if (state.display == "Error") {
        calc_clear(state);
        return;
    }
    state.display = "0";
    state.reset_input = false;
    state.history_locked = false;
    calc_sync_history(state);
}

void calc_input_percent(CalculatorState& state) {
    if (state.display == "Error") {
        calc_clear(state);
    }
    state.display = calc_format(calc_parse(state.display) * 0.01);
    state.reset_input = false;
    state.history_locked = false;
    calc_sync_history(state);
}

void calc_input_inverse(CalculatorState& state) {
    if (state.display == "Error") {
        calc_clear(state);
    }
    const double value = calc_parse(state.display);
    if (std::fabs(value) < 1e-12) {
        calc_set_error(state);
        return;
    }
    state.display = calc_format(1.0 / value);
    state.reset_input = false;
    state.history_locked = false;
    calc_sync_history(state);
}

void calc_input_square(CalculatorState& state) {
    if (state.display == "Error") {
        calc_clear(state);
    }
    const double value = calc_parse(state.display);
    state.display = calc_format(value * value);
    state.reset_input = false;
    state.history_locked = false;
    calc_sync_history(state);
}

void calc_input_sqrt(CalculatorState& state) {
    if (state.display == "Error") {
        calc_clear(state);
    }
    const double value = calc_parse(state.display);
    if (value < 0.0) {
        calc_set_error(state);
        return;
    }
    state.display = calc_format(std::sqrt(value));
    state.reset_input = false;
    state.history_locked = false;
    calc_sync_history(state);
}

void calc_backspace(CalculatorState& state) {
    if (state.display == "Error") {
        calc_clear(state);
        return;
    }
    if (state.reset_input) {
        state.display = "0";
        state.reset_input = false;
        state.history_locked = false;
        calc_sync_history(state);
        return;
    }
    if (state.display.size() <= 1u) {
        state.display = "0";
        state.history_locked = false;
        calc_sync_history(state);
        return;
    }
    state.display.pop_back();
    if (state.display.empty() || state.display == "-") {
        state.display = "0";
    }
    state.history_locked = false;
    calc_sync_history(state);
}

void draw_calculator(UI& ui, CalculatorState& state, const Rect& rect, float scale) {
    const auto dp = [scale](float value) { return value * scale; };
    const float key_h = dp(62.0f);
    const float key_gap = dp(8.0f);
    const float key_text_scale = 1.16f;

    ui.card("CALCULATOR")
        .in(rect)
        .begin([&](auto&) {
            ui.readonly("", state.history).height(dp(34.0f)).align_right().value_scale(0.95f).draw();
            ui.readonly("", state.display).height(dp(88.0f)).align_right().value_scale(1.35f).muted(false).draw();
            ui.spacer(dp(8.0f));

            const auto key_button = [&](const char* label,
                                        eui::ButtonStyle style = eui::ButtonStyle::Secondary) {
                return ui.button(label).style(style).height(key_h).text_scale(key_text_scale).draw();
            };

            const auto draw_row = [&](auto&& fn) {
                ui.row()
                    .items({ui.flex(1.0f), ui.flex(1.0f), ui.flex(1.0f), ui.flex(1.0f)})
                    .gap(key_gap)
                    .align_center()
                    .begin(fn);
            };

            draw_row([&] {
                if (key_button("%")) {
                    calc_input_percent(state);
                }
                if (key_button("CE")) {
                    calc_clear_entry(state);
                }
                if (key_button("C")) {
                    calc_clear(state);
                }
                if (key_button(u8"\uF060")) {
                    calc_backspace(state);
                }
            });

            draw_row([&] {
                if (key_button("1/x")) {
                    calc_input_inverse(state);
                }
                if (key_button(u8"x\u00B2")) {
                    calc_input_square(state);
                }
                if (key_button(u8"\u221Ax")) {
                    calc_input_sqrt(state);
                }
                if (key_button(u8"\u00F7")) {
                    calc_input_operator(state, '/');
                }
            });

            draw_row([&] {
                if (key_button("7")) {
                    calc_input_digit(state, '7');
                }
                if (key_button("8")) {
                    calc_input_digit(state, '8');
                }
                if (key_button("9")) {
                    calc_input_digit(state, '9');
                }
                if (key_button(u8"\u00D7")) {
                    calc_input_operator(state, '*');
                }
            });

            draw_row([&] {
                if (key_button("4")) {
                    calc_input_digit(state, '4');
                }
                if (key_button("5")) {
                    calc_input_digit(state, '5');
                }
                if (key_button("6")) {
                    calc_input_digit(state, '6');
                }
                if (key_button("-")) {
                    calc_input_operator(state, '-');
                }
            });

            draw_row([&] {
                if (key_button("1")) {
                    calc_input_digit(state, '1');
                }
                if (key_button("2")) {
                    calc_input_digit(state, '2');
                }
                if (key_button("3")) {
                    calc_input_digit(state, '3');
                }
                if (key_button("+")) {
                    calc_input_operator(state, '+');
                }
            });

            draw_row([&] {
                if (key_button(state.dark_mode ? u8"\uF185" : u8"\uF186")) {
                    state.dark_mode = !state.dark_mode;
                }
                if (key_button("0")) {
                    calc_input_digit(state, '0');
                }
                if (key_button(".")) {
                    calc_input_digit(state, '.');
                }
                if (key_button("=", eui::ButtonStyle::Primary)) {
                    calc_eval_equal(state);
                }
            });
        });
}

}  // namespace

int main() {
    CalculatorState state{};
    eui::app::AppOptions options{};
    options.title = "EUI Calculator Demo";
    options.width = 430;
    options.height = 620;
    options.vsync = true;
    options.continuous_render = false;
    options.max_fps = 240.0;
    options.text_font_weight = 900;
    options.icon_font_family = "Font Awesome 7 Free Solid";
    options.icon_font_file = "include/Font Awesome 7 Free-Solid-900.otf";

    return eui::app::run(
        [&](eui::app::FrameContext frame) {
            auto& ctx = frame.context();
            const auto metrics = frame.window_metrics();
            const float scale = std::max(1.0f, metrics.dpi_scale);
            const auto dp = [scale](float value) { return value * scale; };

            ctx.set_theme_mode(state.dark_mode ? eui::ThemeMode::Dark : eui::ThemeMode::Light);
            ctx.set_primary_color(eui::rgba(0.30f, 0.74f, 0.77f, 1.0f));
            ctx.set_corner_radius(14.0f * scale);

            UI ui(ctx);
            const float margin = dp(18.0f);
            const Rect panel_rect{
                margin,
                dp(14.0f),
                std::max(dp(240.0f), static_cast<float>(metrics.framebuffer_w) - margin * 2.0f),
                std::max(dp(540.0f), static_cast<float>(metrics.framebuffer_h) - dp(28.0f)),
            };

            ui.panel()
                .in(panel_rect)
                .padding(0.0f)
                .begin([&](auto& root) {
                    draw_calculator(ui, state, root.content(), scale);
                });
        },
        options);
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main();
}
#endif
