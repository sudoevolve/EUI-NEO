#include "app/dsl_app.h"

#include "components/components.h"

#include <algorithm>
#include <functional>
#include <string>

namespace app {

namespace {

int selectedPage = 0;

constexpr float kSidebarWidth = 232.0f;
constexpr float kPanelRadius = 18.0f;

core::Color pageBackground() {
    return {0.08f, 0.09f, 0.11f, 1.0f};
}

core::Color surface() {
    return {0.12f, 0.14f, 0.18f, 1.0f};
}

core::Color surfaceSoft() {
    return {0.16f, 0.18f, 0.23f, 1.0f};
}

core::Color textPrimary() {
    return {0.94f, 0.97f, 1.0f, 1.0f};
}

core::Color textMuted() {
    return {0.62f, 0.70f, 0.82f, 1.0f};
}

void sidebarButton(core::dsl::Ui& ui, const std::string& id, const std::string& label, unsigned int icon, int page) {
    const bool active = selectedPage == page;
    components::button(ui, id)
        .size(184.0f, 48.0f)
        .icon(icon)
        .fontSize(18.0f)
        .iconSize(17.0f)
        .text(label)
        .colors(active ? core::Color{0.18f, 0.48f, 0.78f, 1.0f} : core::Color{0.13f, 0.15f, 0.19f, 1.0f},
                active ? core::Color{0.22f, 0.56f, 0.88f, 1.0f} : core::Color{0.18f, 0.22f, 0.28f, 1.0f},
                {0.08f, 0.30f, 0.62f, 1.0f})
        .radius(12.0f)
        .onClick([page] {
            selectedPage = page;
        })
        .build();
}

void header(core::dsl::Ui& ui, const std::string& title, const std::string& subtitle, float width) {
    ui.column("page.header")
        .size(width, 92.0f)
        .gap(6.0f)
        .content([&] {
            ui.text("page.title")
                .size(width, 42.0f)
                .text(title)
                .customFont("YouSheBiaoTiHei")
                .fontSize(36.0f)
                .lineHeight(42.0f)
                .color(textPrimary())
                .build();

            ui.text("page.subtitle")
                .size(width, 28.0f)
                .text(subtitle)
                .fontSize(20.0f)
                .lineHeight(28.0f)
                .color(textMuted())
                .build();
        });
}

void sampleLabel(core::dsl::Ui& ui, const std::string& id, const std::string& text) {
    ui.text(id)
        .size(180.0f, 28.0f)
        .text(text)
        .fontSize(18.0f)
        .lineHeight(24.0f)
        .color(textMuted())
        .horizontalAlign(core::HorizontalAlign::Center)
        .build();
}

void rectSample(core::dsl::Ui& ui, const std::string& id, const std::string& label, const std::function<void(core::dsl::RectBuilder&)>& style) {
    ui.column(id)
        .size(200.0f, 152.0f)
        .gap(10.0f)
        .alignItems(core::Align::CENTER)
        .content([&] {
            auto rect = ui.rect(id + ".shape")
                .size(160.0f, 96.0f)
                .color(surfaceSoft())
                .radius(16.0f);
            style(rect);
            rect.build();
            sampleLabel(ui, id + ".label", label);
        });
}

void rectGallery(core::dsl::Ui& ui, float width) {
    header(ui, "Primitive Gallery", "Rect color, gradient, border, radius, shadow and opacity.", width);

    ui.column("rect.gallery")
        .size(width, 360.0f)
        .gap(22.0f)
        .content([&] {
            ui.row("rect.row.a")
                .size(width, 152.0f)
                .gap(22.0f)
                .content([&] {
                    rectSample(ui, "sample.color", "Color", [](auto& rect) {
                        rect.color({0.18f, 0.48f, 0.78f, 1.0f});
                    });
                    rectSample(ui, "sample.gradient", "Gradient", [](auto& rect) {
                        rect.gradient({0.21f, 0.66f, 0.84f, 1.0f}, {0.52f, 0.28f, 0.88f, 1.0f});
                    });
                    rectSample(ui, "sample.border", "Border", [](auto& rect) {
                        rect.color({0.10f, 0.12f, 0.16f, 1.0f})
                            .border(3.0f, {0.52f, 0.78f, 1.0f, 1.0f});
                    });
                });

            ui.row("rect.row.b")
                .size(width, 152.0f)
                .gap(22.0f)
                .content([&] {
                    rectSample(ui, "sample.radius", "Radius", [](auto& rect) {
                        rect.color({0.22f, 0.42f, 0.32f, 1.0f}).radius(32.0f);
                    });
                    rectSample(ui, "sample.shadow", "Shadow", [](auto& rect) {
                        rect.color({0.20f, 0.24f, 0.30f, 1.0f})
                            .shadow(28.0f, 0.0f, 12.0f, {0.0f, 0.0f, 0.0f, 0.34f});
                    });
                    rectSample(ui, "sample.opacity", "Opacity", [](auto& rect) {
                        rect.gradient({0.88f, 0.38f, 0.34f, 1.0f}, {0.95f, 0.72f, 0.28f, 1.0f})
                            .opacity(0.62f);
                    });
                });
        });
}

void textGallery(core::dsl::Ui& ui, float width) {
    header(ui, "Text And Icons", "Custom font assets and Font Awesome icon text.", width);

    ui.column("text.gallery")
        .size(width, 360.0f)
        .gap(18.0f)
        .content([&] {
            ui.text("text.custom.title")
                .size(width, 54.0f)
                .text("YouSheBiaoTiHei Custom Font")
                .customFont("YouSheBiaoTiHei")
                .fontSize(40.0f)
                .lineHeight(48.0f)
                .color({0.90f, 0.95f, 1.0f, 1.0f})
                .build();

            ui.text("text.body")
                .size(width, 74.0f)
                .text("Text supports font family, font size, color, wrapping, max width and alignment.")
                .fontSize(22.0f)
                .lineHeight(30.0f)
                .maxWidth(width - 32.0f)
                .wrap(true)
                .color(textMuted())
                .build();

            ui.row("icon.row")
                .size(width, 96.0f)
                .gap(28.0f)
                .content([&] {
                    const unsigned int icons[] = {0xF015, 0xF53F, 0xF031, 0xF013};
                    const char* labels[] = {"Home", "Palette", "Font", "Settings"};
                    for (int i = 0; i < 4; ++i) {
                        ui.column(std::string("icon.card.") + std::to_string(i))
                            .size(120.0f, 96.0f)
                            .gap(8.0f)
                            .alignItems(core::Align::CENTER)
                            .content([&, i] {
                                ui.text(std::string("icon.") + std::to_string(i))
                                    .size(70.0f, 44.0f)
                                    .icon(icons[i])
                                    .fontSize(34.0f)
                                    .lineHeight(40.0f)
                                    .color({0.38f, 0.72f, 1.0f, 1.0f})
                                    .horizontalAlign(core::HorizontalAlign::Center)
                                    .build();
                                sampleLabel(ui, std::string("icon.label.") + std::to_string(i), labels[i]);
                            });
                    }
                });
        });
}

void transformGallery(core::dsl::Ui& ui, float width) {
    header(ui, "Transforms", "Translate, scale and rotate are stored on primitive nodes.", width);

    ui.row("transform.gallery")
        .size(width, 220.0f)
        .gap(30.0f)
        .content([&] {
            rectSample(ui, "transform.translate", "Translate", [](auto& rect) {
                rect.color({0.22f, 0.46f, 0.74f, 1.0f}).translate(18.0f, 10.0f);
            });
            rectSample(ui, "transform.scale", "Scale", [](auto& rect) {
                rect.color({0.54f, 0.34f, 0.78f, 1.0f}).scale(1.12f, 0.82f);
            });
            rectSample(ui, "transform.rotate", "Rotate", [](auto& rect) {
                rect.color({0.68f, 0.42f, 0.24f, 1.0f}).rotate(0.16f);
            });
        });
}

void composeSidebar(core::dsl::Ui& ui, float height) {
    ui.stack("sidebar")
        .size(kSidebarWidth, height)
        .content([&] {
            ui.rect("sidebar.bg")
                .size(kSidebarWidth, height)
                .color({0.07f, 0.08f, 0.10f, 1.0f})
                .border(1.0f, {0.20f, 0.24f, 0.30f, 1.0f})
                .build();

            ui.column("sidebar.content")
                .size(kSidebarWidth, std::max(0.0f, height - 32.0f))
                .margin(0.0f, 32.0f, 0.0f, 0.0f)
                .gap(14.0f)
                .alignItems(core::Align::CENTER)
                .content([&] {
                    ui.text("brand.icon")
                        .size(184.0f, 42.0f)
                        .icon(0xF135)
                        .fontSize(30.0f)
                        .lineHeight(38.0f)
                        .color({0.43f, 0.74f, 1.0f, 1.0f})
                        .horizontalAlign(core::HorizontalAlign::Center)
                        .build();

                    ui.text("brand.title")
                        .size(184.0f, 34.0f)
                        .text("Gallery")
                        .customFont("YouSheBiaoTiHei")
                        .fontSize(30.0f)
                        .lineHeight(34.0f)
                        .color(textPrimary())
                        .horizontalAlign(core::HorizontalAlign::Center)
                        .build();

                    sidebarButton(ui, "nav.rect", "Primitives", 0xF0C8, 0);
                    sidebarButton(ui, "nav.text", "Text", 0xF031, 1);
                    sidebarButton(ui, "nav.transform", "Transform", 0xF2F1, 2);
                });
        });
}

void composeContent(core::dsl::Ui& ui, float width, float height) {
    ui.stack("content.area")
        .size(width, height)
        .content([&] {
            ui.rect("content.bg")
                .size(width, height)
                .color(pageBackground())
                .build();

            ui.column("content.inner")
                .size(std::max(0.0f, width - 64.0f), std::max(0.0f, height - 64.0f))
                .gap(24.0f)
                .margin(32.0f)
                .content([&] {
                    if (selectedPage == 0) {
                        rectGallery(ui, width - 64.0f);
                    } else if (selectedPage == 1) {
                        textGallery(ui, width - 64.0f);
                    } else {
                        transformGallery(ui, width - 64.0f);
                    }
                });
        });
}

} // namespace

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = {
        "Primitive Gallery",
        "demo1",
        {0.08f, 0.09f, 0.11f, 1.0f},
        1440,
        900
    };
    return config;
}

void compose(core::dsl::Ui& ui, const core::dsl::Screen& screen) {
    const float contentWidth = std::max(0.0f, screen.width - kSidebarWidth);

    ui.stack("root")
        .size(screen.width, screen.height)
        .align(core::Align::START, core::Align::START)
        .content([&] {
            ui.row("shell")
                .size(screen.width, screen.height)
                .content([&] {
                    composeSidebar(ui, screen.height);
                    composeContent(ui, contentWidth, screen.height);
                });
        });
}

} // namespace app
