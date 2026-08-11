#include "eui_neo.h"

bool publicHeaderCompanionIsLinked() {
    const eui::Color color{0.0f, 1.0f, 0.0f, 1.0f};
    return color.g > 0.5f;
}
