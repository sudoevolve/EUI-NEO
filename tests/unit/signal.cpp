#include "components/components.h"
#include "eui/dsl.h"
#include "eui/platform.h"
#include "eui/signal.h"

#include <iostream>
#include <string>

int main() {
    eui::Signal<int> selected{1};
    if (selected.get() != 1) {
        std::cerr << "Signal did not keep initial value\n";
        return 1;
    }

    selected.set(2);
    if (selected.get() != 2) {
        std::cerr << "Signal set failed\n";
        return 1;
    }

    eui::Signal<std::string> text = eui::signal(std::string("hello"));
    text = std::string("world");
    if (text.get() != "world") {
        std::cerr << "Signal assignment failed\n";
        return 1;
    }

    eui::Ui ui;
    ui.begin("signal.compile");

    eui::Signal<float> sliderValue{0.4f};
    components::slider(ui, "slider")
        .bind(sliderValue)
        .build();

    eui::Signal<bool> checked{true};
    components::checkbox(ui, "checkbox")
        .bind(checked)
        .build();

    eui::Signal<bool> dialogOpen{false};
    components::dialog(ui, "dialog")
        .bindOpen(dialogOpen)
        .build();

    eui::Signal<std::string> inputValue{std::string("text")};
    components::input(ui, "input")
        .bind(inputValue)
        .build();

    ui.end();
    return 0;
}
