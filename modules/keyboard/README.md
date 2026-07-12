# Keyboard Module

Responsibility:

- Provide a simple API to show, hide, and configure an on-screen keyboard.
- Offer light and night keyboard styles.
- Render keyboard layouts as EUI DSL/components rather than native widgets.
- Emit key/text actions back to the owning input field or application callback.

Basic API:

```cpp
std::string name;
std::string note;

modules::keyboard::KeyboardPanelController keyboard({
    {"name.input", name},
    {"note.input", note, true},
});

components::input(ui, "name.input")
    .value(name)
    .onChange([&](const std::string& value) { name = value; })
    .build();

keyboard.compose(ui, "keyboard", screen.width, screen.height);
```

The panel watches the bound input ids, opens when one is focused, writes text
back to the bound value, and owns the default bottom-center position, drag
handle, edge resize behavior, theme application, and layout switching.
