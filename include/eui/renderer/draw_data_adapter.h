#pragma once

#include <cstddef>

#include "eui/renderer/contracts.h"

namespace eui::renderer {

template <typename CommandT>
inline DrawDataView make_draw_data_view(const CommandT* commands,
                                        std::size_t command_count,
                                        const void* text_data,
                                        std::size_t text_size) {
    return DrawDataView{
        commands,
        sizeof(CommandT),
        command_count,
        MemoryView{text_data, text_size},
    };
}

template <typename CommandT, typename TextCharT>
inline DrawDataView make_draw_data_view(const CommandT* commands,
                                        std::size_t command_count,
                                        const TextCharT* text_data,
                                        std::size_t text_size) {
    return make_draw_data_view(commands, command_count, static_cast<const void*>(text_data), text_size);
}

}  // namespace eui::renderer
