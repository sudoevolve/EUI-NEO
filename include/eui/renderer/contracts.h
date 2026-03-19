#pragma once

#include <cstddef>

#include "eui/runtime/contracts.h"

namespace eui::renderer {

struct MemoryView {
    const void* data{nullptr};
    std::size_t size{0};
};

struct DrawDataView {
    const void* commands{nullptr};
    std::size_t command_stride{0};
    std::size_t command_count{0};
    MemoryView text_arena{};
};

struct ClearState {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{1.0f};
    bool clear_color{true};
};

class RendererBackend {
public:
    virtual ~RendererBackend() = default;

    virtual void begin_frame(const eui::runtime::WindowMetrics& metrics, const ClearState& clear_state) = 0;
    virtual void render(const DrawDataView& draw_data, const eui::runtime::WindowMetrics& metrics) = 0;
    virtual void end_frame() = 0;
    virtual void release_resources() = 0;
};

}  // namespace eui::renderer
