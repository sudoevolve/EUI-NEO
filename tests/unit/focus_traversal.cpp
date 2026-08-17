#include "core/dsl_focus.h"
#include "core/dsl_runtime.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string orderText(const std::vector<std::string>& ids) {
    std::string text;
    for (const std::string& id : ids) {
        if (!text.empty()) {
            text += " ";
        }
        text += id;
    }
    return text;
}

bool expectOrder(const char* label, const std::vector<std::string>& ids,
                 const std::vector<std::string>& expected) {
    if (ids == expected) {
        return true;
    }
    std::cerr << label << ": got [" << orderText(ids) << "] expected ["
              << orderText(expected) << "]\n";
    return false;
}

// ---------- Pure helper tests (Ui + layout, no Runtime) ----------

bool collectFlatOrder() {
    core::dsl::Ui ui;
    ui.begin("page");
    ui.rect("a").size(10.0f, 10.0f).focusable().build();
    ui.rect("b").size(10.0f, 10.0f).focusable().build();
    ui.rect("c").size(10.0f, 10.0f).focusable().build();
    ui.end();
    ui.layout(800.0f, 600.0f);
    return expectOrder("collectFlatOrder", core::dsl::collectOrderedFocusableIds(ui),
                       {"page.a", "page.b", "page.c"});
}

bool collectHonorsZOrder() {
    core::dsl::Ui ui;
    ui.begin("page");
    ui.rect("a").size(10.0f, 10.0f).focusable().zIndex(0).build();
    ui.rect("b").size(10.0f, 10.0f).focusable().zIndex(2).build();
    ui.rect("c").size(10.0f, 10.0f).focusable().zIndex(1).build();
    ui.end();
    ui.layout(800.0f, 600.0f);
    return expectOrder("collectHonorsZOrder", core::dsl::collectOrderedFocusableIds(ui),
                       {"page.a", "page.c", "page.b"});
}

bool collectSkipsDisabled() {
    core::dsl::Ui ui;
    ui.begin("page");
    ui.rect("a").size(10.0f, 10.0f).focusable().build();
    ui.rect("b").size(10.0f, 10.0f).focusable().disabled().build();
    ui.rect("c").size(10.0f, 10.0f).focusable().build();
    ui.end();
    ui.layout(800.0f, 600.0f);
    return expectOrder("collectSkipsDisabled", core::dsl::collectOrderedFocusableIds(ui),
                       {"page.a", "page.c"});
}

bool collectSkipsDisabledTree() {
    core::dsl::Ui ui;
    ui.begin("page");
    ui.stack("p").focusable().disabled().content([&] {
        ui.rect("q").size(10.0f, 10.0f).focusable().build();
    }).build();
    ui.rect("r").size(10.0f, 10.0f).focusable().build();
    ui.end();
    ui.layout(800.0f, 600.0f);
    return expectOrder("collectSkipsDisabledTree", core::dsl::collectOrderedFocusableIds(ui),
                       {"page.r"});
}

bool collectSkipsInteractiveNotFocusable() {
    core::dsl::Ui ui;
    ui.begin("page");
    ui.rect("k").size(10.0f, 10.0f).onClick([] {}).build();
    ui.end();
    ui.layout(800.0f, 600.0f);
    return expectOrder("collectSkipsInteractiveNotFocusable",
                       core::dsl::collectOrderedFocusableIds(ui), {});
}

bool collectPreOrder() {
    core::dsl::Ui ui;
    ui.begin("page");
    ui.stack("p").focusable().content([&] {
        ui.rect("q").size(10.0f, 10.0f).focusable().build();
        ui.rect("r").size(10.0f, 10.0f).focusable().build();
    }).build();
    ui.end();
    ui.layout(800.0f, 600.0f);
    return expectOrder("collectPreOrder", core::dsl::collectOrderedFocusableIds(ui),
                       {"page.p", "page.q", "page.r"});
}

bool nextTargetEmptyList() {
    const std::vector<std::string> ids;
    if (!core::dsl::nextFocusTarget(ids, {}, false).empty() ||
        !core::dsl::nextFocusTarget(ids, "page.a", true).empty()) {
        std::cerr << "nextTargetEmptyList: expected empty target\n";
        return false;
    }
    return true;
}

bool nextTargetNoCurrent() {
    const std::vector<std::string> ids = {"page.a", "page.b", "page.c"};
    if (core::dsl::nextFocusTarget(ids, {}, false) != "page.a" ||
        core::dsl::nextFocusTarget(ids, {}, true) != "page.c") {
        std::cerr << "nextTargetNoCurrent: expected first/last\n";
        return false;
    }
    return true;
}

bool nextTargetMid() {
    const std::vector<std::string> ids = {"page.a", "page.b", "page.c"};
    if (core::dsl::nextFocusTarget(ids, "page.a", false) != "page.b" ||
        core::dsl::nextFocusTarget(ids, "page.c", true) != "page.b") {
        std::cerr << "nextTargetMid: forward/backward from ends failed\n";
        return false;
    }
    return true;
}

bool nextTargetWrap() {
    const std::vector<std::string> ids = {"page.a", "page.b", "page.c"};
    if (core::dsl::nextFocusTarget(ids, "page.c", false) != "page.a" ||
        core::dsl::nextFocusTarget(ids, "page.a", true) != "page.c") {
        std::cerr << "nextTargetWrap: wrap around failed\n";
        return false;
    }
    return true;
}

bool nextTargetCurrentMissing() {
    const std::vector<std::string> ids = {"page.a", "page.b", "page.c"};
    if (core::dsl::nextFocusTarget(ids, "page.ghost", false) != "page.a" ||
        core::dsl::nextFocusTarget(ids, "page.ghost", true) != "page.c") {
        std::cerr << "nextTargetCurrentMissing: stale current fell back wrong\n";
        return false;
    }
    return true;
}

bool nextTargetSingle() {
    const std::vector<std::string> ids = {"page.only"};
    if (core::dsl::nextFocusTarget(ids, "page.only", false) != "page.only" ||
        core::dsl::nextFocusTarget(ids, "page.only", true) != "page.only") {
        std::cerr << "nextTargetSingle: single element did not stay put\n";
        return false;
    }
    return true;
}

// ---------- Runtime-level tests (compose + traverseFocus + focusedId) ----------

void composeThreeFocusables(core::dsl::Runtime& runtime, bool disableB = false) {
    runtime.compose("page", 800.0f, 600.0f, [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
        ui.rect("a").size(10.0f, 10.0f).focusable().build();
        ui.rect("b").size(10.0f, 10.0f).focusable().disabled(disableB).build();
        ui.rect("c").size(10.0f, 10.0f).focusable().build();
    });
}

bool runtimeFirstTabFromNone() {
    core::dsl::Runtime runtime;
    composeThreeFocusables(runtime);
    runtime.traverseFocus(false);
    const bool ok = runtime.focusedId() == "page.a";
    if (!ok) {
        std::cerr << "runtimeFirstTabFromNone: got " << runtime.focusedId() << "\n";
    }
    runtime.shutdown(false);
    return ok;
}

bool runtimeShiftTabFromNone() {
    core::dsl::Runtime runtime;
    composeThreeFocusables(runtime);
    runtime.traverseFocus(true);
    const bool ok = runtime.focusedId() == "page.c";
    if (!ok) {
        std::cerr << "runtimeShiftTabFromNone: got " << runtime.focusedId() << "\n";
    }
    runtime.shutdown(false);
    return ok;
}

bool runtimeCyclesForwardAndWraps() {
    core::dsl::Runtime runtime;
    composeThreeFocusables(runtime);
    runtime.traverseFocus(false);
    runtime.traverseFocus(false);
    runtime.traverseFocus(false);
    runtime.traverseFocus(false);
    const bool ok = runtime.focusedId() == "page.a";
    if (!ok) {
        std::cerr << "runtimeCyclesForwardAndWraps: got " << runtime.focusedId() << "\n";
    }
    runtime.shutdown(false);
    return ok;
}

bool runtimeShiftTabWraps() {
    core::dsl::Runtime runtime;
    composeThreeFocusables(runtime);
    runtime.traverseFocus(false); // -> a
    runtime.traverseFocus(true);  // wraps back to c
    const bool ok = runtime.focusedId() == "page.c";
    if (!ok) {
        std::cerr << "runtimeShiftTabWraps: got " << runtime.focusedId() << "\n";
    }
    runtime.shutdown(false);
    return ok;
}

bool runtimeSkipsDisabled() {
    core::dsl::Runtime runtime;
    composeThreeFocusables(runtime, /*disableB=*/true);
    runtime.traverseFocus(false); // a
    runtime.traverseFocus(false); // skips disabled b -> c
    const bool ok = runtime.focusedId() == "page.c";
    if (!ok) {
        std::cerr << "runtimeSkipsDisabled: got " << runtime.focusedId() << "\n";
    }
    runtime.shutdown(false);
    return ok;
}

bool runtimeFiresOnFocusChanged() {
    core::dsl::Runtime runtime;
    std::vector<std::pair<std::string, bool>> events;
    runtime.compose("page", 800.0f, 600.0f, [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
        ui.rect("a").size(10.0f, 10.0f)
            .onFocusChanged([&](bool focused) { events.emplace_back("page.a", focused); })
            .build();
        ui.rect("b").size(10.0f, 10.0f)
            .onFocusChanged([&](bool focused) { events.emplace_back("page.b", focused); })
            .build();
        ui.rect("c").size(10.0f, 10.0f).focusable().build();
    });

    runtime.traverseFocus(false); // a gains focus
    runtime.traverseFocus(false); // a loses, b gains

    const std::vector<std::pair<std::string, bool>> expected = {
        {"page.a", true},
        {"page.a", false},
        {"page.b", true},
    };
    const bool ok = events == expected;
    if (!ok) {
        std::cerr << "runtimeFiresOnFocusChanged: got " << events.size()
                  << " events, expected " << expected.size() << "\n";
    }
    runtime.shutdown(false);
    return ok;
}

bool runtimeClearsFocusWhenDisabled() {
    core::dsl::Runtime runtime;
    composeThreeFocusables(runtime);
    runtime.traverseFocus(false); // a
    if (runtime.focusedId() != "page.a") {
        runtime.shutdown(false);
        std::cerr << "runtimeClearsFocusWhenDisabled: setup failed\n";
        return false;
    }

    // Re-compose with "a" now disabled: compose clears focus on the stale id.
    runtime.compose("page", 800.0f, 600.0f, [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
        ui.rect("a").size(10.0f, 10.0f).focusable().disabled().build();
        ui.rect("b").size(10.0f, 10.0f).focusable().build();
        ui.rect("c").size(10.0f, 10.0f).focusable().build();
    });

    const bool cleared = runtime.focusedId().empty();
    runtime.traverseFocus(false); // -> b (a is disabled)
    const bool ok = cleared && runtime.focusedId() == "page.b";
    if (!ok) {
        std::cerr << "runtimeClearsFocusWhenDisabled: cleared=" << cleared
                  << " focused=" << runtime.focusedId() << "\n";
    }
    runtime.shutdown(false);
    return ok;
}

bool runtimeEmptyTreeDoesNothing() {
    core::dsl::Runtime runtime;
    runtime.compose("page", 800.0f, 600.0f,
                    [&](core::dsl::Ui&, const core::dsl::Screen&) {});
    runtime.traverseFocus(false);
    runtime.traverseFocus(true);
    const bool ok = runtime.focusedId().empty();
    if (!ok) {
        std::cerr << "runtimeEmptyTreeDoesNothing: got " << runtime.focusedId() << "\n";
    }
    runtime.shutdown(false);
    return ok;
}

} // namespace

int main() {
    bool ok = true;
    ok = collectFlatOrder() && ok;
    ok = collectHonorsZOrder() && ok;
    ok = collectSkipsDisabled() && ok;
    ok = collectSkipsDisabledTree() && ok;
    ok = collectSkipsInteractiveNotFocusable() && ok;
    ok = collectPreOrder() && ok;
    ok = nextTargetEmptyList() && ok;
    ok = nextTargetNoCurrent() && ok;
    ok = nextTargetMid() && ok;
    ok = nextTargetWrap() && ok;
    ok = nextTargetCurrentMissing() && ok;
    ok = nextTargetSingle() && ok;
    ok = runtimeFirstTabFromNone() && ok;
    ok = runtimeShiftTabFromNone() && ok;
    ok = runtimeCyclesForwardAndWraps() && ok;
    ok = runtimeShiftTabWraps() && ok;
    ok = runtimeSkipsDisabled() && ok;
    ok = runtimeFiresOnFocusChanged() && ok;
    ok = runtimeClearsFocusWhenDisabled() && ok;
    ok = runtimeEmptyTreeDoesNothing() && ok;
    return ok ? 0 : 1;
}
