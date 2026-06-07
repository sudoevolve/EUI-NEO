# Workshop Component Porting Skill

Use this skill when porting a highly customized web/CSS component into `components/workshop/`.

## Goal

Workshop components are creative, brand-like, or effect-heavy components. They should feel like QML-style reusable components from the caller side, but internally they must still be plain EUI DSL composition.

## Placement

- Put workshop components in `components/workshop/`.
- Use namespace `components::workshop`.
- Export public workshop headers from `components/components.h`.
- Keep foundational containers and generic controls in `components/`, not in `components/workshop/`.

## Porting Workflow

1. Identify the web component states:
   - normal
   - hover
   - pressed / active
   - disabled, if present
   - focus, only when keyboard input matters

2. Translate CSS layers into DSL layers:
   - `background`, `border-radius`, `box-shadow` -> `ui.rect(...)`
   - text content -> `ui.text(...)`
   - icons -> `ui.text(...).icon(...)` or existing icon helpers
   - SVG icons -> use `ui.svg(...)` with inline SVG markup when exact path fidelity matters
   - pseudo-elements such as `::before` / `::after` -> extra rect/text layers with stable ids
   - CSS transforms -> `.translate(...)`, `.scale(...)`, `.rotate(...)`, `.rotateX(...)`, `.rotateY(...)`, `.perspective(...)`, `visualStateFrom(...)`

3. Translate CSS shadows carefully:
   - one outer `box-shadow` -> `.shadow(...)`
   - one inset `box-shadow` -> `.insetShadow(...)`
   - multiple shadows -> multiple transparent rect layers
   - when hiding an animated shadow, keep the RGB and set alpha to `0.0f`; do not switch to transparent black unless the target shadow is actually black

4. Keep ids stable:
   - component root id is provided by the caller
   - internal ids use `id_ + ".part"`
   - avoid creating/removing layers only for pressed/hover if their shadow/color animates; prefer always-present transparent layers

5. Theme the component:
   - provide `.style(...)` for complete override
   - provide `.theme(tokens)` for light/dark defaults
   - derive colors from the surface the component is meant to sit on
   - avoid drawing an extra background panel unless the source design explicitly has one

6. Keep caller usage short:

```cpp
components::workshop::myComponent(ui, "demo.my_component")
    .theme(themeColors())
    .size(240.0f, 72.0f)
    .transition(pageTransition())
    .build();
```

7. Preserve exact SVG shapes when needed:
   - If the source component is fundamentally an SVG path, do not approximate it with rect/polygon unless the user explicitly wants a DSL-only drawing.
   - Inline the SVG string in the component header when portability matters.
   - Render inline SVG through `ui.svg(...).source(svg).tint(color).contain()`.
   - Keep the SVG fill white and use image tint for theme colors.
   - Avoid adding repo asset files for one-off workshop components unless the user asks for external assets.

## Builder Shape

Use a small builder class:

```cpp
namespace components::workshop {

struct MyComponentStyle {
    core::Color surface;
    core::Color text;
};

class MyComponentBuilder {
public:
    MyComponentBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    MyComponentBuilder& size(float width, float height);
    MyComponentBuilder& theme(const theme::ThemeColorTokens& tokens);
    MyComponentBuilder& style(const MyComponentStyle& style);
    MyComponentBuilder& transition(const core::Transition& transition);
    void build();

private:
    core::dsl::Ui& ui_;
    std::string id_;
};

inline MyComponentBuilder myComponent(core::dsl::Ui& ui, const std::string& id) {
    return MyComponentBuilder(ui, id);
}

} // namespace components::workshop
```

## Interaction Rules

- Use DSL events (`onClick`, `onPress`, `onRelease`, `onMove`, `onHoverChanged`) or `components::mouseArea`.
- For pure visual pointer-follow transforms, prefer Runtime binding (`.runtimePointerTransformFrom(...)` or `.runtimePointerTiltFrom(...)`) over `mouseArea.onMove(...)` state maps so hover movement does not recompose the component.
- Use `components::mouseArea(...).onMove(...)` only when the component needs callback-driven business state or visual state that Runtime cannot derive yet.
- Map pointer-local coordinates into normalized `[-0.5, 0.5]` values before applying `.rotateX(...)`, `.rotateY(...)`, or light offsets.
- Do not read GLFW/SDL state directly.
- Business state belongs to the page; workshop components may keep small visual-only state when the DSL has no built-in state channel for that effect.
- Keep visual state maps keyed by stable component id.

## Verification

After adding or changing a workshop component:

```sh
git diff --check
cmake --build build-vk --target gallery --parallel
```

If shader behavior changes, also update both:

- `core/render/vulkan/shaders/*.frag` or `*.vert`
- generated `core/render/vulkan/*_shaders.h`
