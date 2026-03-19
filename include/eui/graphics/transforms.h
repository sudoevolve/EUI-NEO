#pragma once

namespace eui::graphics {

struct Transform2D {
    float translation_x{0.0f};
    float translation_y{0.0f};
    float scale_x{1.0f};
    float scale_y{1.0f};
    float rotation_deg{0.0f};
    float origin_x{0.0f};
    float origin_y{0.0f};
};

struct Transform3D {
    float translation_x{0.0f};
    float translation_y{0.0f};
    float translation_z{0.0f};
    float scale_x{1.0f};
    float scale_y{1.0f};
    float scale_z{1.0f};
    float rotation_x_deg{0.0f};
    float rotation_y_deg{0.0f};
    float rotation_z_deg{0.0f};
    float origin_x{0.0f};
    float origin_y{0.0f};
    float origin_z{0.0f};
    float perspective{0.0f};
};

}  // namespace eui::graphics
