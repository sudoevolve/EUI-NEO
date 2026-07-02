---
name: eui-neo-ui-replicator
description: Design or replicate high-quality desktop interfaces in the EUI-NEO C++ DSL. Use when Codex needs to recreate screenshots, mockups, web pages, product UI, or create an original competition-grade desktop UI/UX concept from user requirements and reference research, prioritizing Row/Column/Stack/Flow layout, built-in components from components/, theme tokens, short non-looping motion, Bing image placeholders when media is unavailable, and only falling back to Rect/Text/Image/SVG/Polygon primitives or new lightweight components when no built-in component can express the target.
---

# EUI-NEO UI Designer And Replicator

## Goal

Recreate a target interface or design an original desktop UI as idiomatic EUI-NEO code. Prefer stable layout and built-in components over pixel-by-pixel drawing. Use primitive DSL layers only for visual details, custom shapes, or new components that the framework does not already provide.

When the user provides only a product idea, vague requirements, or a desired style, act as a UI/UX designer first: extract requirements, gather or infer high-quality desktop UI references, analyze their reusable techniques, define a design direction, and then implement it in EUI-NEO.

## First Pass

Before writing code, inspect these local sources when available:

- `README.md` or `README.zh-CN.md` for app setup and public entry points.
- `docs/DSL.md` for DSL element capabilities.
- `docs/*.md` with searches for `Row`, `Column`, `Stack`, `Flow`, `SizeValue`, `ignoreLayout`, and component names for layout and component behavior.
- `examples/gallery.cpp` and `examples/pages/*.h` only as references for idiomatic page composition.
- `components/components.h` for the exported component list.
- `components/workshop/SKILL.md` when porting an effect-heavy or CSS-like custom component.

Create runnable user apps under `apps/`, not under `examples/`, unless the user explicitly asks to modify the built-in gallery or examples. Prefer a directory app such as `apps/my_app/app.cpp` when the UI may grow or needs assets; use `apps/my_app/assets/` for app-local media. A single-file `apps/my_app.cpp` is fine only for very small experiments.
Before creating code, choose and state the app target name. The target name comes from the flat file stem (`apps/my_app.cpp` -> `my_app`) or directory name (`apps/my_app/app.cpp` -> `my_app`). Build that exact target for verification.

Create a short implementation inventory:

```text
Mode: replicate | original design | hybrid
Design brief:
Reference set:
Reference techniques:
Target areas:
Layout containers:
Reusable built-in components:
Custom primitive pieces:
State and interactions:
Motion plan:
Assets or SVG needed:
Verification target:
```

## Requirement Discovery

If the target UI is not fully specified, collect enough context to design well:

- Product purpose and primary user.
- Core desktop workflows and the most frequent actions.
- Information hierarchy, data density, and navigation model.
- Required pages, panels, overlays, empty states, and error states.
- Brand mood, visual constraints, and accessibility needs.
- Media needs: photos, illustrations, logos, thumbnails, charts, maps, or avatars.

Ask concise follow-up questions only when missing information would materially change the interface. Otherwise make explicit assumptions and continue. For a competition-grade concept, favor a focused, memorable workflow over a generic dashboard with many unrelated cards.

## Reference Research

When designing an original UI or when the user asks for top-tier, modern, award-level, or current references, research current desktop UI/UX examples before designing if internet access is available. Collect a small reference set from credible product pages, design award galleries, platform design systems, or high-quality case studies. Cite sources in the work summary when external research is used.

Analyze references for transferable techniques rather than copying them:

- Layout rhythm: shell, sidebar, toolbar, canvas, inspector, split panes, density.
- Visual hierarchy: contrast, typography, accent use, spacing, grouping.
- Interaction model: command placement, progressive disclosure, shortcuts, drag/drop, hover affordances.
- Motion: page transitions, panel entrance, control feedback, selection changes.
- Data presentation: tables, charts, cards, timelines, previews, and comparisons.
- Desktop polish: resizable regions, persistent navigation, keyboard/focus states, status feedback.

Convert the analysis into a design direction:

```text
Design thesis:
Primary workflow:
Navigation model:
Surface system:
Component system:
Motion language:
Media strategy:
```

## Decision Order

Use this order for every target region:

1. Use layout containers for structure: `ui.row`, `ui.column`, `ui.stack`, `ui.flow`.
2. Use built-in components for familiar controls and data display.
3. Use themed wrappers for simple surfaces: `components::card`, `components::panel`, `components::text`, `components::image`.
4. Use primitives for missing visual details: `ui.rect`, `ui.text`, `ui.image`, `ui.svg`, `ui.polygon`.
5. Create a component only when the same custom piece repeats, has interaction, or would make the page hard to read.
6. Put generic reusable controls in `components/`; put creative, brand-like, or effect-heavy ports in `components/workshop/`; keep one-off page decorations as local helper functions.

Treat motion as part of the replica. Built-in components already include interaction motion, so keep and configure their `.transition(...)` rather than reimplementing it. Add short transitions to page switches and to custom components made from primitives. Do not use long-running, looping, pulsing, marquee, spinner-like, or decorative infinite animations.

Do not bypass the DSL runtime or hold backend primitives directly. Do not read GLFW/SDL state directly. Keep rendering, events, animation, hit testing, dirty rects, and state lifecycle inside the EUI runtime.

## Layout Rules

Use `Row` for horizontal groups, toolbars, two-column shells, button rows, and chart rows.

Use `Column` for forms, stacked sections, sidebars, property lists, and vertical content.

Use `Stack` for overlays, absolute local coordinates, backgrounds behind content, badges, decorative layers, and modal surfaces.

Use `Flow` for chips, tags, filter buttons, compact action groups, and any horizontal content that should wrap.

Use `components::scrollView` for scrollable measured content. Use low-level `components::scroll` only when manually binding a runtime scroll state is necessary.

Inside `scrollView`, make the scroll content root measure its real content height. Prefer `.height(SizeValue::wrapContent())` on the content root; do not lock it to the viewport height unless the content is intentionally non-scrollable.

Keep one logical wrapping grid in one `Flow`. Do not split one continuous card grid into multiple sibling flows, because each flow wraps independently and will restart from a new line.

Treat `Flow` as wrapping, not as a full responsive layout engine. Use explicit thresholds when structure changes: keep two-column `Row` layouts while both children fit, switch to single-column `Column` only when the width is truly too small.

Avoid putting content with variable visible height inside a fixed-height parent just to group it. If a table, picker, markdown block, wrapped flow, or stacked section can grow, let it participate directly in the parent column or reserve enough measured height; otherwise later content can visually overlap.

Prefer `.padding(...)` on containers over extra spacer containers. Prefer `.gap(...)` for repeated child spacing. Use `.fill()`, `SizeValue::wrapContent()`, `flexGrow`, `minWidth`, and `maxWidth` to handle responsive resizing instead of hard-coded screen branches everywhere.

Use `.ignoreLayout()` only for decorative or debug overlays that should not affect measurement. Use `.zIndex(...)` only for drawing and hit-test order; it does not remove an element from layout.

For responsive pages, branch at clear layout thresholds and keep both branches structurally simple:

```cpp
const bool twoColumns = width >= 820.0f;
if (twoColumns) {
    ui.row("page.grid").size(width, height).gap(18.0f).content([&] {
        ui.column("page.grid.left").size(leftWidth, leftHeight).gap(18.0f).content([&] {
            // sections
        }).build();
        ui.column("page.grid.right").size(rightWidth, rightHeight).gap(18.0f).content([&] {
            // sections
        }).build();
    }).build();
} else {
    ui.column("page.grid.single").size(width, singleHeight).gap(18.0f).content([&] {
        // same sections in single-column order
    }).build();
}
```

## Component Selection

Use these built-ins first:

- Commands: `components::button`
- Boolean choices: `components::checkbox`, `components::toggleSwitch`
- Exclusive choices: `components::radio`, `components::segmented`, `components::tabs`
- Numeric input: `components::slider`, `components::stepper`, `components::progress`
- Text entry: `components::input`
- Menus and pickers: `components::dropdown`, `components::datePicker`, `components::timePicker`, `components::colorPicker`, `components::contextMenu`
- Overlays and feedback: `components::dialog`, `components::sidebar`, `components::toast`, `components::tooltip`
- Data and media: `components::dataTable`, `components::carousel`, `components::lineChart`, `components::barChart`, `components::pieChart`, `components::markdown`
- Hit regions: `components::mouseArea`
- Surfaces and typography: `components::card`, `components::panel`, `components::text`, `components::image`

For controlled components, keep business state in the page or owning model. Pass current values into the component and update them through `onChange`, `onOpenChange`, `onDismiss`, or `Signal<T>::bind(...)`.

Place global overlays near the end of root composition so they sit above normal content. Pass `screen(width, height)` to modal, picker, toast, and context menu components.

## Primitive Translation

Translate visual details into DSL primitives:

- CSS block, panel, card, track, chip, or pill -> `ui.rect(...)`
- Text label, icon font glyph, heading, metric -> `ui.text(...)`
- Photos, PNG/JPG assets, SVG files -> `ui.image(...)`
- Exact inline SVG icon or logo -> `ui.svg(...).source(svg).tint(color).contain()`
- Triangles, pointers, sectors, custom polygonal marks -> `ui.polygon(...)`
- Pseudo-elements like `::before` and `::after` -> extra stable DSL layers
- Box shadow -> `.shadow(...)`
- Inset shadow -> `.insetShadow(...)`
- Multiple shadows -> multiple always-present rect layers with stable ids
- Border radius -> `.radius(...)`
- CSS transform -> `.translate(...)`, `.scale(...)`, `.rotate(...)`, `.rotateX(...)`, `.rotateY(...)`, `.perspective(...)`

If an exact SVG path matters, inline SVG through `ui.svg` instead of approximating with polygons. If the shape is simple and themable, prefer primitives so the result stays native and animatable.

Keep internal ids stable. Use caller ids for root components and `id + ".part"` for child layers. Avoid creating and removing layers for hover/pressed visual changes; keep layers present and animate color, opacity, or transform.

## Media And Placeholder Rules

Use real user-provided assets first. If the user references a product, person, venue, object, or brand and exact imagery is important, search for or request the correct asset instead of inventing it.

If a media slot needs an image and no asset or URL is available, use Bing daily image as the first placeholder:

```cpp
ui.image("hero.placeholder")
    .size(width, height)
    .bingDaily(0, "zh-CN")
    .cover()
    .radius(18.0f)
    .transition(pageTransition())
    .build();
```

Use `components::image(...)` when theme defaults are useful; use `ui.image(...)` when the placeholder needs exact primitive control. Do not leave major media frames blank unless the design intentionally uses an empty-state treatment. Keep placeholder ids clear, such as `.placeholder`, so they are easy to replace with final assets.

## Visual Fidelity

Extract the target interface into tokens before coding:

```text
Canvas/background:
Surfaces:
Primary/accent:
Text primary/muted:
Borders:
Shadow:
Radii:
Spacing scale:
Typography:
```

Prefer `components::theme::dark()`, `components::theme::light()`, `pageVisuals(tokens)`, `fieldVisuals(tokens)`, and helpers such as `withAlpha`, `withOpacity`, `buttonHover`, `buttonPressed`, `border`, and `shadow`. Override colors directly only when matching a specific source design.

Use Font Awesome icon codepoints through `.icon(...)` on `components::button` or `ui.text(...).icon(...)` when an icon glyph exists. Use inline SVG for exact brand or path icons.

Keep text boxes large enough for the intended strings. Set `fontSize`, `lineHeight`, `horizontalAlign`, and `verticalAlign` explicitly for polished compact controls.

## App File Placement

For user-facing generated apps, create or update one of these shapes:

```text
apps/
  my_app/
    app.cpp
    assets/        # optional app-local assets
```

or, only for tiny throwaway experiments:

```text
apps/
  my_app.cpp
```

Do not add new user apps to `examples/`. Use `examples/` only when the user explicitly asks for a built-in example, gallery page, release demo, or regression fixture.

## Page Skeleton

Write application files so the UI reads from top to bottom. Put `dslAppConfig()` near the top, then `compose()` as the root overview, then the page or region composition functions in visual order, and put small reusable helpers after the UI sections. Use short forward declarations when needed to keep this order. Comments should label meaningful sections such as root layout, sidebar, content host, Home, Settings, overlays, and custom primitive pieces; avoid line-by-line comments that repeat the code.

Use this shape for `apps/<target>/app.cpp` or `apps/<target>.cpp`:

```cpp
#include <algorithm>

#include "eui_neo.h"

namespace app {

namespace {

components::theme::ThemeColorTokens themeColors() {
    auto tokens = components::theme::dark();
    tokens.primary = {0.28f, 0.48f, 0.92f, 1.0f};
    return tokens;
}

eui::Transition pageTransition() {
    return eui::Transition::make(0.24f, eui::Ease::OutCubic);
}

} // namespace

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("Replicated UI")
        .pageId("replicated_ui")
        .windowSize(1280, 820);
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    const auto tokens = themeColors();

    ui.stack("root")
        .size(screen.width, screen.height)
        .content([&] {
            ui.rect("root.bg")
                .fill()
                .color(tokens.background)
                .build();

            ui.column("page")
                .size(screen.width, screen.height)
                .padding(32.0f)
                .gap(18.0f)
                .content([&] {
                    components::text(ui, "page.title", tokens)
                        .size(std::max(0.0f, screen.width - 64.0f), 42.0f)
                        .text("Replicated UI")
                        .fontSize(32.0f)
                        .lineHeight(38.0f)
                        .build();
                })
                .build();
        })
        .build();
}

} // namespace app
```

## Custom Component Pattern

When primitives become a repeated or interactive unit, wrap them in a small builder:

```cpp
class BadgeBuilder {
public:
    BadgeBuilder(eui::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    BadgeBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    BadgeBuilder& text(std::string value) { text_ = std::move(value); return *this; }
    BadgeBuilder& theme(const components::theme::ThemeColorTokens& tokens) {
        fill_ = components::theme::withAlpha(tokens.primary, 0.16f);
        textColor_ = tokens.primary;
        return *this;
    }

    void build() {
        const auto transition = eui::Transition::make(0.18f, eui::Ease::OutCubic);

        ui_.stack(id_)
            .size(width_, height_)
            .content([&] {
                ui_.rect(id_ + ".bg")
                    .fill()
                    .color(fill_)
                    .radius(height_ * 0.5f)
                    .transition(transition)
                    .build();

                ui_.text(id_ + ".text")
                    .fill()
                    .text(text_)
                    .fontSize(13.0f)
                    .lineHeight(18.0f)
                    .fontWeight(720)
                    .color(textColor_)
                    .horizontalAlign(eui::HorizontalAlign::Center)
                    .verticalAlign(eui::VerticalAlign::Center)
                    .transition(transition)
                    .build();
            })
            .build();
    }

private:
    eui::Ui& ui_;
    std::string id_;
    std::string text_ = "Badge";
    float width_ = 96.0f;
    float height_ = 30.0f;
    eui::Color fill_{0.16f, 0.28f, 0.50f, 1.0f};
    eui::Color textColor_{0.80f, 0.88f, 1.0f, 1.0f};
};
```

If the component belongs in `components/` or `components/workshop/`, follow existing builder shape: lowerCamelCase factory, `style(...)`, `theme(...)`, `transition(...)`, stable child ids, no business-state ownership unless it is purely internal visual interaction state.

## Interaction And Animation

Use DSL events and component callbacks for interaction. Use `components::mouseArea` for custom hit regions, drag, scroll, context menu, or pointer-local behavior.

For pure pointer-follow effects, prefer runtime bindings such as `.runtimePointerTransformFrom(...)` or `.runtimePointerTiltFrom(...)` when available. Use callback-driven `onMove` state only when the runtime cannot express the effect.

Use `.transition(...)` on components and primitives. Built-in components should normally receive the page transition so their existing hover, press, open, close, selection, and value-change motion stays consistent with the page. Primitive-built components must add explicit short transitions on the layers that change color, opacity, radius, shadow, or transform.

Animate page switching with `ui.loader(...)` plus short opacity, translate, or transform transitions on the page container or page sections. Use `keepAlive()` when preserving page-local state matters and `destroyOnHide()` when hidden page state should reset.

Keep normal UI transitions around `0.12f - 0.32f`. Allow up to about `0.45f` only for modal/sidebar entrance, page-level transitions, or expressive workshop effects. Do not add delayed, long-duration, constantly running, or looped animation for decoration. If a loading state is needed, use a finite state change or an existing component behavior rather than an infinite spinner.

Add `.animate(eui::AnimProperty::Frame)` only when layout frame changes should animate. For color, opacity, radius, shadow, and transform, use the relevant animation flags supported by the element.

Use `.transformedHitTest()` only when the hit area must follow visual transforms.

## Verification

After implementing a replica:

```powershell
git diff --check
cmake --build build --target <app_target> --parallel
```

Use `gallery` or other `examples/*` targets only when the task explicitly modified built-in examples:

```powershell
cmake --build build --target gallery --parallel
```

If Vulkan-specific rendering, shaders, or retained cache behavior is touched, also verify the Vulkan build directory or target used by the repository.

Before finishing, review the diff and confirm:

- The UI uses layout containers for structure.
- Existing components are used before primitives.
- Any custom component has a clear reusable reason.
- Page switches and primitive-built custom components have short transitions.
- No long-duration or looping decorative animation was added.
- Missing media uses real assets when available or Bing daily placeholders when not.
- Original designs include a brief, reference techniques, and a clear design direction.
- Business state is page-owned.
- Ids are stable and child ids are namespaced.
- Overlay components are composed above normal content.
- No unrelated framework behavior or dependency was changed.
