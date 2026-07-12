#include "core/dsl.h"

#include <iostream>

namespace {

struct PageState {
    int selectedIndex = 0;
    bool autoPlay = true;
};

} // namespace

int main() {
    core::dsl::Ui ui;

    ui.begin("state.page");
    PageState* first = &ui.state<PageState>("page");
    first->selectedIndex = 3;
    first->autoPlay = false;
    ui.end();

    ui.begin("state.page");
    PageState* second = &ui.state<PageState>("page");
    ui.end();

    if (first != second) {
        std::cerr << "page state address changed across compose\n";
        return 1;
    }
    if (second->selectedIndex != 3 || second->autoPlay) {
        std::cerr << "page state values did not survive compose\n";
        return 1;
    }
    return 0;
}