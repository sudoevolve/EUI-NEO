#pragma once

#include "components/gallery_components.h"

namespace app::gallery {

inline components::InputStyle contactInputStyle() {
    components::InputStyle style(galleryTheme());
    style.background = {1.0f, 1.0f, 1.0f, 0.48f};
    style.focused = {1.0f, 1.0f, 1.0f, 0.92f};
    style.border = {0.15f, 0.14f, 0.13f, 0.32f};
    style.focusBorder = kAccent;
    style.text = kInk;
    style.placeholder = {0.25f, 0.24f, 0.22f, 0.54f};
    style.cursor = kAccent;
    style.shadow = {};
    style.radius = 0.0f;
    return style;
}

inline void composeContactsPage(eui::Ui& ui, float width, float viewportHeight) {
    const float pad = pagePadding(width);
    const float contentWidth = std::min(1240.0f, std::max(280.0f, width - pad * 2.0f));
    const float originX = std::max(pad, (width - contentWidth) * 0.5f);
    const bool wide = contentWidth >= 900.0f;
    const float pageHeight = wide ? 1040.0f : 1500.0f;
    const float scroll = state.contactsScroll.get();

    ui.stack("contacts.document").position(originX, 0.0f).size(contentWidth, pageHeight).content([&] {
        sectionEyebrow(ui, "contacts.eyebrow", "VISIT / CONNECT", 0.0f, 48.0f, 240.0f);
        serifHeading(ui, "contacts.heading", "Come closer\nto the work.", 0.0f, 78.0f,
                     wide ? contentWidth * 0.46f : contentWidth, 152.0f, wide ? 54.0f : 45.0f);
        const float leftWidth = wide ? contentWidth * 0.38f : contentWidth;
        const float formX = wide ? contentWidth * 0.50f : 0.0f;
        const float formWidth = wide ? contentWidth * 0.50f : contentWidth;
        const float formY = wide ? 72.0f : 420.0f;
        sectionEyebrow(ui, "contacts.address.label", "ADDRESS", 0.0f, 278.0f, leftWidth);
        bodyText(ui, "contacts.address", "1124 Ninth Avenue SW\nCalgary, Alberta", 0.0f, 310.0f,
                 leftWidth, 72.0f, 16.0f, kInk);
        sectionEyebrow(ui, "contacts.hours.label", "OPENING HOURS", 0.0f, 405.0f, leftWidth);
        bodyText(ui, "contacts.hours", "Tuesday - Saturday  11:00 - 18:00\nSunday by appointment",
                 0.0f, 437.0f, leftWidth, 72.0f, 15.0f, kInk);
        bodyText(ui, "contacts.email", "hello@calgary.gallery\n+1 403 555 0198",
                 0.0f, 548.0f, leftWidth, 72.0f, 15.0f, kInk);
        sectionEyebrow(ui, "contacts.form.label", "SEND A NOTE", formX, formY, formWidth);
        components::input(ui, "contacts.name").position(formX, formY + 44.0f).size(formWidth, 52.0f)
            .placeholder("Your name").bind(state.name).fontSize(15.0f)
            .style(contactInputStyle()).transition(quickMotion()).build();
        components::input(ui, "contacts.email.input").position(formX, formY + 116.0f).size(formWidth, 52.0f)
            .placeholder("Email address").bind(state.email).fontSize(15.0f)
            .style(contactInputStyle()).transition(quickMotion()).build();
        components::input(ui, "contacts.message").position(formX, formY + 188.0f).size(formWidth, 152.0f)
            .placeholder("Tell us what you would like to know").bind(state.message).multiline()
            .fontSize(15.0f).style(contactInputStyle()).transition(quickMotion()).build();
        components::button(ui, "contacts.submit").position(formX, formY + 368.0f)
            .size(std::min(220.0f, formWidth), 48.0f).text("SEND MESSAGE").icon(0xF1D8)
            .fontSize(12.0f).iconSize(13.0f).colors(kInk, {0.15f, 0.14f, 0.12f, 1.0f}, kAccent)
            .textColor(kPaper).iconColor(kPaper).radius(0.0f).pressScale(0.97f)
            .transition(quickMotion()).onClick([] {
                state.messageSent = !state.name.get().empty() && !state.email.get().empty() && !state.message.get().empty();
            }).build();
        ui.text("contacts.success").position(formX + std::min(238.0f, formWidth), formY + 372.0f)
            .size(std::max(0.0f, formWidth - std::min(238.0f, formWidth)), 40.0f)
            .text(state.messageSent ? "MESSAGE RECEIVED" : "COMPLETE ALL FIELDS")
            .fontSize(11.0f).lineHeight(16.0f).fontWeight(760).color(kAccent)
            .opacity(state.messageSent ? 1.0f : 0.0f).translateY(state.messageSent ? 0.0f : 8.0f)
            .verticalAlign(eui::VerticalAlign::Center).transition(pageMotion())
            .animate(eui::AnimProperty::Opacity | eui::AnimProperty::Transform).build();
        if (!wide) {
            artCard(ui, "contacts.mobile.art", 5, 0.0f, 930.0f, contentWidth, 360.0f,
                    scroll, viewportHeight, false);
        }
        footer(ui, "contacts.footer", contentWidth, wide ? 930.0f : 1380.0f);
    }).build();
}

} // namespace app::gallery
