#pragma once

namespace eui {

struct Color;
struct Rect;
struct InputState;
struct DrawCommand;
class Context;

namespace runtime {
struct FrameContext;
}

namespace app {
using FrameContext = eui::runtime::FrameContext;
}

}  // namespace eui
