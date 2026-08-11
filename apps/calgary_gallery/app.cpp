#include "eui_neo.h"

#include "pages/about_page.h"
#include "pages/artwork_detail.h"
#include "pages/contacts_page.h"
#include "pages/exhibitions_page.h"
#include "pages/news_page.h"

#include <algorithm>
#include <string>

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("Calgary Art Gallery")
        .pageId("calgary_gallery")
        .clearColor(gallery::kPaper)
        .windowSize(1440, 920)
        .fps(90.0);
    return config;
}

namespace {

void pageTicker(eui::Ui& ui) {
    if (gallery::state.pageReveal >= 1.0f) return;
    ui.stack("page.reveal.ticker").size(1.0f, 1.0f).ignoreLayout()
        .onFrame([](float deltaSeconds) {
            gallery::state.pageReveal = std::min(1.0f, gallery::state.pageReveal + deltaSeconds / 0.42f);
        }).build();
}

void logoMark(eui::Ui& ui, float x, float y) {
    ui.stack("header.logo").position(x, y).size(34.0f, 34.0f).content([&] {
        for (int index = 0; index < 4; ++index) {
            ui.rect("header.logo.ray." + std::to_string(index)).position(3.0f, 16.0f)
                .size(28.0f, 1.2f).color(gallery::kInk)
                .rotate(static_cast<float>(index) * 0.785398f).build();
        }
        ui.rect("header.logo.center").position(14.0f, 14.0f).size(6.0f, 6.0f)
            .color(gallery::kPaper).border(1.3f, gallery::kInk).radius(3.0f).build();
    }).build();
}

void navigationItem(eui::Ui& ui, gallery::Page page, float x, float y, float width, float height) {
    const int index = static_cast<int>(page);
    const std::string id = "header.nav." + std::to_string(index);
    const bool active = gallery::state.page == page;
    ui.stack(id).position(x, y).size(width, height).content([&] {
        ui.text(id + ".base").size(width, height).text(gallery::pageLabel(page))
            .fontSize(14.0f).lineHeight(18.0f).fontWeight(active ? 760 : 650)
            .color(gallery::kInk).opacity(active ? 0.0f : 1.0f)
            .horizontalAlign(eui::HorizontalAlign::Center).verticalAlign(eui::VerticalAlign::Center)
            .transition(gallery::quickMotion()).build();
        ui.text(id + ".accent").size(width, height).text(gallery::pageLabel(page))
            .fontSize(14.0f).lineHeight(18.0f).fontWeight(760).color(gallery::kAccent)
            .opacity(active ? 1.0f : 0.0f).hoverOpacityFrom(id + ".hit", active ? 1.0f : 0.0f, 1.0f)
            .horizontalAlign(eui::HorizontalAlign::Center).verticalAlign(eui::VerticalAlign::Center)
            .transition(gallery::quickMotion()).build();
        components::mouseArea(ui, id + ".hit").size(width, height)
            .onTap([page] { gallery::switchPage(page); }).build();
    }).build();
}

void composeHeader(eui::Ui& ui, float width, float headerHeight, bool compact) {
    const float pad = gallery::pagePadding(width);
    const float innerWidth = std::max(0.0f, width - pad * 2.0f);
    const float navY = compact ? 65.0f : 22.0f;
    const float navWidth = compact ? innerWidth : std::min(510.0f, innerWidth * 0.52f);
    const float navX = compact ? pad : width - pad - navWidth;
    const float itemWidth = navWidth / 4.0f;
    const float itemHeight = compact ? 40.0f : 48.0f;
    ui.stack("header").size(width, headerHeight).zIndex(100).content([&] {
        ui.rect("header.background").size(width, headerHeight)
            .color({0.985f, 0.982f, 0.972f, 0.975f}).build();
        logoMark(ui, pad, compact ? 18.0f : 28.0f);
        if (!compact) {
            ui.text("header.wordmark").position(pad + 48.0f, 24.0f).size(180.0f, 46.0f)
                .text("CALGARY").fontSize(12.0f).lineHeight(17.0f).fontWeight(780)
                .color(gallery::kInk).verticalAlign(eui::VerticalAlign::Center).build();
        }
        for (int index = 0; index < 4; ++index) {
            navigationItem(ui, static_cast<gallery::Page>(index),
                           navX + itemWidth * static_cast<float>(index), navY, itemWidth, itemHeight);
        }
        ui.rect("header.active.line")
            .position(navX + itemWidth * static_cast<float>(static_cast<int>(gallery::state.page)) + itemWidth * 0.22f,
                      navY + itemHeight - 3.0f)
            .size(itemWidth * 0.56f, 2.0f).color(gallery::kAccent)
            .transition(gallery::pageMotion()).animate(eui::AnimProperty::Frame | eui::AnimProperty::Color).build();
        ui.rect("header.rule").position(pad, headerHeight - 1.0f).size(innerWidth, 1.0f)
            .color(gallery::kRule).build();
    }).build();
}

void composeActivePage(eui::Ui& ui, float width, float height, float headerHeight) {
    const float pageHeight = std::max(0.0f, height - headerHeight);
    const float reveal = gallery::easeOutCubic(gallery::state.pageReveal);
    ui.stack("page.host").position(0.0f, headerHeight).size(width, pageHeight).clip().content([&] {
        ui.loader("page.current").active(true).destroyOnHide().content([&] {
            if (gallery::state.page == gallery::Page::Exhibitions) {
                ui.stack("page.exhibitions.motion").size(width, pageHeight).opacity(reveal)
                    .translateY((1.0f - reveal) * 34.0f).content([&] {
                        pageTicker(ui);
                        gallery::composeExhibitionsPage(ui, width, pageHeight);
                    }).build();
                return;
            }
            components::scrollView(ui, "page.scroll").size(width, pageHeight)
                .bind(gallery::activeScrollSignal()).step(58.0f).scrollbarWidth(3.0f).scrollbarGap(8.0f)
                .contentKey("gallery.page." + std::to_string(static_cast<int>(gallery::state.page)))
                .content([&](eui::Ui& contentUi, float contentWidth, float viewportHeight) {
                    contentUi.stack("page.motion").width(contentWidth)
                        .height(eui::SizeValue::wrapContent()).opacity(reveal)
                        .translateY((1.0f - reveal) * 34.0f).content([&] {
                            pageTicker(contentUi);
                            switch (gallery::state.page) {
                                case gallery::Page::News:
                                    gallery::composeNewsPage(contentUi, contentWidth, viewportHeight);
                                    break;
                                case gallery::Page::About:
                                    gallery::composeAboutPage(contentUi, contentWidth, viewportHeight);
                                    break;
                                case gallery::Page::Contacts:
                                    gallery::composeContactsPage(contentUi, contentWidth, viewportHeight);
                                    break;
                                case gallery::Page::Exhibitions:
                                    break;
                            }
                        }).build();
                }).build();
        }).build();
    }).build();
}

} // namespace

void compose(eui::Ui& ui, const eui::Screen& screen) {
    const bool compactHeader = screen.width < 850.0f;
    const float headerHeight = compactHeader ? 118.0f : 92.0f;
    ui.stack("root").size(screen.width, screen.height).content([&] {
        ui.rect("root.background").size(screen.width, screen.height).color(gallery::kPaper).build();
        composeActivePage(ui, screen.width, screen.height, headerHeight);
        composeHeader(ui, screen.width, headerHeight, compactHeader);
    }).build();
    gallery::composeArtworkDetail(ui, screen);
}

} // namespace app
