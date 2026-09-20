#pragma once

// Included after the actual gallery source: exercise its real button callbacks.
namespace {
struct GallerySnapshot {
    modules::plot::Scene3D scene;
    std::string view, camera, interaction;
};
std::string cameraState(const modules::plot::Plot3D& plot) {
    modules::plot::ViewState3D view;
    view.camera = plot.view().camera;
    return modules::plot::saveView3D(view);
}
std::string interactionState(const modules::plot::Plot3D& plot) {
    std::ostringstream out;
    out << std::setprecision(17);
    if (auto selected = plot.selection())
        out << selected->sourceIndex << ' ' << selected->position.x << ' ' << selected->position.y << ' '
            << selected->position.z;
    out << '|';
    if (auto measured = plot.measurement())
        out << measured->from.x << ' ' << measured->from.y << ' ' << measured->from.z << ' ' << measured->to.x
            << ' ' << measured->to.y << ' ' << measured->to.z << ' ' << measured->distance;
    return out.str();
}
void verifyGalleryControls(double dpi) {
    using namespace modules::plot;
    const auto require = [](bool ok, const std::string& message) {
        if (!ok)
            throw std::runtime_error(message);
    };
    const auto check = [&](std::size_t source, const char* suffix, std::vector<std::size_t> affected,
                           bool changesCamera = false) {
        std::vector<GallerySnapshot> before;
        std::size_t picked = 0;
        for (auto& panel : app::panels) {
            auto& plot = *panel.plot;
            auto view = plot.view();
            view.camera.zoom(0.95);
            view.camera.orbit(0.01, 0.005);
            plot.setView(view);
            eui::Ui ui;
            ui.begin("selection");
            plot.compose(ui, "plot", 160, 120, dpi);
            ui.end();
            // Seed selection and a completed measurement on the original scene.
            SceneRenderer3D renderer(plot.scene());
            bool found = false;
            for (int y = 8; y < 120 && !found; y += 8)
                for (int x = 8; x < 160 && !found; x += 8)
                    if (renderer.pick(plot.view().camera, {double(x), double(y)}, 160, 120)) {
                        const auto* input = ui.find("plot.input");
                        core::PointerEvent event;
                        event.button = core::PointerButton::Left;
                        event.modifiers.control = true;
                        event.x = float(x * dpi);
                        event.y = float(y * dpi);
                        const core::Rect bounds{0, 0, float(160 * dpi), float(120 * dpi)};
                        for (int click = 0; click < 2; ++click) {
                            event.action = core::PointerAction::Press;
                            input->onPress(event, bounds);
                            event.action = core::PointerAction::Release;
                            input->onRelease(event, bounds);
                        }
                        require(plot.selection().has_value() && plot.measurement().has_value(),
                                "failed to seed gallery interaction");
                        found = true;
                        ++picked;
                    }
            before.push_back(
                {plot.scene(), saveView3D(plot.view()), cameraState(plot), interactionState(plot)});
        }
        require(picked >= 4, "insufficient selection coverage");
        eui::Ui controls;
        controls.begin("controls");
        app::controls(controls, source);
        controls.end();
        const std::string id = "control." + std::to_string(source) + "." + suffix + ".bg";
        const auto* button = controls.find(id);
        require(button && bool(button->onClick), "missing gallery button: " + id);
        button->onClick();
        for (std::size_t i = 0; i < app::panels.size(); ++i) {
            auto& plot = *app::panels[i].plot;
            eui::Ui ui;
            ui.begin("after");
            plot.compose(ui, "plot", 160, 120, dpi);
            ui.end();
            const auto context = id + " affected panel " + std::to_string(i);
            if (!(changesCamera && i == source))
                require(cameraState(plot) == before[i].camera, context + " camera");
            if (std::find(affected.begin(), affected.end(), i) != affected.end())
                continue;
            require(saveView3D(plot.view()) == before[i].view, context + " view");
            require(interactionState(plot) == before[i].interaction, context + " selection/measurement");
            require(plot.scene().volume == before[i].scene.volume, context + " volume");
            require(plot.scene().objects.size() == before[i].scene.objects.size(), context + " objects");
            for (std::size_t j = 0; j < plot.scene().objects.size(); ++j)
                require(plot.scene().objects[j].geometry == before[i].scene.objects[j].geometry,
                        context + " geometry");
        }
    };
#if EUI_GALLERY_PHASE == 3
    for (int repeat = 0; repeat < 2; ++repeat) {
        check(0, "gap", {0});
        check(1, "map", {1});
        check(2, "normals", {2});
        check(3, "light", {3});
        check(3, "cull", {3});
        check(4, "alpha", {4});
        check(5, "clip", {5});
        for (std::size_t i = 0; i < app::panels.size(); ++i)
            check(i, "projection", {i}, true);
    }
#else
    const auto saved = app::settings();
    check(3, "angle", {3});
    check(4, "level", {4});
    check(5, "quality", {5});
    require(app::quality[0] == 1 && app::quality[1] == 0, "quality was shared");
    check(5, "transfer", {5});
    require(app::transfer[0] == 1 && app::transfer[1] == 0, "transfer was shared");
    check(6, "quality", {6});
    check(6, "quality", {6});
    check(6, "transfer", {6});
    check(6, "transfer", {6});
    check(0, "move", {0, 3});
    check(1, "move", {1, 3});
    check(2, "move", {2, 3, 6, 7});
    check(3, "position", {2, 3, 6, 7});
    check(0, "link", {0, 1, 2, 3, 6, 7});
    check(7, "update", {7});
    check(7, "budget", {});
    const auto independent = app::settings();
    app::restoreSettings(saved);
    app::restoreSettings(independent);
    require(app::settings() == independent, "independent volume settings did not round trip");
    app::restoreSettings("0.3 0.3 0 2 1 0 0 0 0");
    require(app::quality[0] == 2 && app::quality[1] == 2 && app::transfer[0] == 1 && app::transfer[1] == 1,
            "legacy gallery state was not restored");
    app::restoreSettings(saved);
#endif
    std::cout << "gallery button isolation dpi " << dpi << " passed\n";
}
} // namespace
