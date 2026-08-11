#include "eui_neo.h"

#include <algorithm>
#include <cmath>
#include <string>

// Native EUI-NEO adaptation of the 21st.dev "Magic Wand" React component.
//
// Original semantics preserved:
//  - modifiedX = clientX * 1.3 - 0.15 * innerWidth
//  - modifiedY = clientY * 0.4 - 0.10 * innerHeight
//  - rotation = (clientX / innerWidth) * 20 - 10   (degrees)
//  - wand follows via springs: stiffness 200, damping 25, mass 1
//  - tile reveal follows the sprung wand position with eased opacity and image blur

namespace app {

namespace {

constexpr float kPi = 3.14159265358979323846f;

float degToRad(float degrees) {
    return degrees * kPi / 180.0f;
}

// framer-motion useSpring({ damping: 25, stiffness: 200 }) with mass = 1,
// restDelta = 0.01, restSpeed = 0.01, semi-implicit Euler integration.
struct Spring {
    float value = 0.0f;
    float velocity = 0.0f;
    float target = 0.0f;
    bool settled = true;

    void setTarget(float next) {
        target = next;
        settled = false;
    }

    void step(float dt) {
        if (settled) {
            return;
        }
        dt = std::clamp(dt, 0.0f, 1.0f / 30.0f);
        const float displacement = value - target;
        const float acceleration = -200.0f * displacement - 25.0f * velocity;
        velocity += acceleration * dt;
        value += velocity * dt;
        if (std::abs(value - target) < 0.01f && std::abs(velocity) < 0.01f) {
            value = target;
            velocity = 0.0f;
            settled = true;
        }
    }
};

struct WandState {
    Spring x;         // springs towards modifiedX
    Spring y;         // springs towards modifiedY
    Spring rotation;  // springs towards rotation degrees
    float modifiedX = 0.0f;
    bool awake() const {
        return !x.settled || !y.settled || !rotation.settled;
    }
};

const char* kTileImages[3] = {
    "https://images.pexels.com/photos/27073784/pexels-photo-27073784/free-photo-of-bumblebee-on-a-sunflower.jpeg?auto=compress&cs=tinysrgb&w=600",
    "https://images.pexels.com/photos/19087694/pexels-photo-19087694/free-photo-of-ice-cream-with-cookies-and-chocolate.jpeg?auto=compress&cs=tinysrgb&w=600",
    "https://images.pexels.com/photos/638341/pexels-photo-638341.jpeg?auto=compress&cs=tinysrgb&w=600",
};

const char* kCameraPaths =
    "<path d=\"M14.5 4h-5L7 7H4a2 2 0 0 0-2 2v9a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2V9a2 2 0 0 0-2-2h-3l-2.5-3z\"/>"
    "<circle cx=\"12\" cy=\"13\" r=\"3\"/>";

const char* kFileImagePaths =
    "<path d=\"M15 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V7Z\"/>"
    "<path d=\"M14 2v4a2 2 0 0 0 2 2h4\"/>"
    "<circle cx=\"10\" cy=\"12\" r=\"2\"/>"
    "<path d=\"m20 17-1.296-1.296a2.41 2.41 0 0 0-3.408 0L9 22\"/>";

const char* kImagePaths =
    "<rect width=\"18\" height=\"18\" x=\"3\" y=\"3\" rx=\"2\" ry=\"2\"/>"
    "<circle cx=\"9\" cy=\"9\" r=\"2\"/>"
    "<path d=\"m21 15-3.086-3.086a2 2 0 0 0-2.828 0L6 21\"/>";

std::string lucideSvg(const char* paths, float opacity) {
    std::string svg =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" "
        "stroke=\"#ffffff\" stroke-opacity=\"";
    svg += std::to_string(opacity);
    svg +=
        "\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">";
    svg += paths;
    svg += "</svg>";
    return svg;
}

// Wand body: linear-gradient(to right, rgb(26,24,28) 10%, rgb(42,40,44) 45% 55%, rgb(26,24,28) 90%)
// Silver tip (top 20%): same stops with rgb(212,221,236) / white / rgb(212,221,236)
const char* kWandSvg = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 100">
  <defs>
    <linearGradient id="wandBody" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0.1" stop-color="#1a181c"/>
      <stop offset="0.45" stop-color="#2a282c"/>
      <stop offset="0.55" stop-color="#2a282c"/>
      <stop offset="0.9" stop-color="#1a181c"/>
    </linearGradient>
    <linearGradient id="wandTip" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0.1" stop-color="#d4ddec"/>
      <stop offset="0.45" stop-color="#ffffff"/>
      <stop offset="0.55" stop-color="#ffffff"/>
      <stop offset="0.9" stop-color="#d4ddec"/>
    </linearGradient>
  </defs>
  <rect x="0" y="0" width="10" height="100" rx="3" fill="url(#wandBody)"/>
  <path d="M0,3 Q0,0 3,0 H7 Q10,0 10,3 V20 H0 Z" fill="url(#wandTip)"/>
</svg>)SVG";

} // namespace

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("Magic Wand")
        .pageId("magic_wand")
        .clearColor({2.0f / 255.0f, 6.0f / 255.0f, 23.0f / 255.0f, 1.0f})
        .windowSize(1280, 800)
        .fps(120.0);
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    WandState& state = ui.state<WandState>("wand.state");

    const float width = screen.width;
    const float height = screen.height;
    const float vmin = std::min(width, height) / 100.0f;

    const float tile = 38.0f * vmin;
    const float containerWidth = 94.0f * vmin;  // 3 * 38vmin - 2 * 10vmin overlap
    const float containerX = (width - containerWidth) * 0.5f;
    const float containerY = (height - tile) * 0.5f;
    const float revealX = 0.05f * width + state.x.value;

    const float tileRotations[3] = {3.0f, -2.0f, 5.0f};
    const float iconSizes[3] = {20.0f * vmin, 12.0f * vmin, 20.0f * vmin};
    const float iconOpacities[3] = {0.20f, 0.15f, 0.20f};
    const char* iconPaths[3] = {kCameraPaths, kFileImagePaths, kImagePaths};

    ui.stack("page")
        .size(width, height)
        .content([&] {
            // Background doubles as the global mouse tracker (window mousemove in the original).
            auto background = ui.rect("page.background");
            background
                .position(0.0f, 0.0f)
                .size(width, height)
                .color({2.0f / 255.0f, 6.0f / 255.0f, 23.0f / 255.0f, 1.0f})
                .onMove([&state, width, height](const eui::PointerEvent& event, const eui::Rect&) {
                    const float clientX = static_cast<float>(event.x);
                    const float clientY = static_cast<float>(event.y);
                    state.modifiedX = clientX * 1.3f - width * 0.15f;
                    state.x.setTarget(state.modifiedX);
                    state.y.setTarget(clientY * 0.4f - height * 0.1f);
                    state.rotation.setTarget((clientX / width) * 20.0f - 10.0f);
                })
                .cursor(eui::CursorShape::Arrow);
            if (state.awake()) {
                // Only keep the frame callback alive while springs are moving,
                // so the runtime can sleep when the scene is idle.
                background.onFrame([&state](float dt) {
                    state.x.step(dt);
                    state.y.step(dt);
                    state.rotation.step(dt);
                });
            }
            background.build();

            // Tile strip (flex row with -ml-[10vmin] overlaps, centered by grid place-items-center).
            ui.stack("tiles")
                .position(containerX, containerY)
                .size(containerWidth, tile)
                .content([&] {
                    for (int i = 0; i < 3; ++i) {
                        const float radians = degToRad(tileRotations[i]);
                        // getBoundingClientRect() of the rotated tile = axis-aligned bounding box.
                        const float aabbWidth = tile * (std::abs(std::cos(radians)) + std::abs(std::sin(radians)));
                        const float centerX = containerX + i * 28.0f * vmin + tile * 0.5f;
                        const float aabbLeft = centerX - aabbWidth * 0.5f;
                        const float rawReveal = std::clamp((revealX - aabbLeft) / aabbWidth, 0.0f, 1.0f);
                        const float reveal = rawReveal * rawReveal * (3.0f - 2.0f * rawReveal);
                        const std::string index = std::to_string(i);

                        ui.stack("tile." + index)
                            .position(i * 28.0f * vmin, 0.0f)
                            .size(tile, tile)
                            .rotate(radians)
                            .zIndex(3 - i)
                            .content([&] {
                                ui.rect("tile.bg." + index)
                                    .position(0.0f, 0.0f)
                                    .size(tile, tile)
                                    .radius(6.0f * vmin)
                                    .color({31.0f / 255.0f, 41.0f / 255.0f, 55.0f / 255.0f, 1.0f})
                                    .shadow(6.0f * vmin, 0.0f, 3.0f * vmin, {0.0f, 0.0f, 0.0f, 0.25f})
                                    .build();

                                // CSS paints the inset shadow above the background, below the content.
                                ui.rect("tile.inset." + index)
                                    .position(0.0f, 0.0f)
                                    .size(tile, tile)
                                    .radius(6.0f * vmin)
                                    .color({0.0f, 0.0f, 0.0f, 0.0f})
                                    .insetShadow(1.0f * vmin, 0.0f, 0.5f * vmin, {1.0f, 1.0f, 1.0f, 0.15f})
                                    .build();

                                ui.svg("tile.icon." + index)
                                    .position((tile - iconSizes[i]) * 0.5f, (tile - iconSizes[i]) * 0.5f)
                                    .size(iconSizes[i], iconSizes[i])
                                    .source(lucideSvg(iconPaths[i], iconOpacities[i]))
                                    .build();

                                ui.image("tile.image." + index)
                                    .position(0.0f, 0.0f)
                                    .size(tile, tile)
                                    .source(kTileImages[i])
                                    .cover()
                                    .radius(6.0f * vmin)
                                    .opacity(reveal)
                                    .blur((1.0f - reveal) * (1.0f - reveal) * 10.0f)
                                    .build();
                            })
                            .build();
                    }
                })
                .build();

            // Wand: absolute left-[5%] top-[20%] of the viewport, -translate-x-1/2,
            // then framer-motion translate/rotate on top (spring driven).
            const float wandWidth = 10.0f * vmin;
            const float wandHeight = 100.0f * vmin;
            const float wandX = 0.05f * width + state.x.value - wandWidth * 0.5f;
            const float wandY = 0.2f * height + state.y.value;

            ui.stack("wand")
                .position(wandX, wandY)
                .size(wandWidth, wandHeight)
                .rotate(degToRad(state.rotation.value))
                .zIndex(100)
                .content([&] {
                    // Shadow carrier: CSS box-shadow lives on the (opaque) wand body.
                    ui.rect("wand.shadow")
                        .position(0.0f, 0.0f)
                        .size(wandWidth, wandHeight)
                        .radius(3.0f * vmin)
                        .color({26.0f / 255.0f, 24.0f / 255.0f, 28.0f / 255.0f, 1.0f})
                        .shadow(4.0f * vmin, 0.0f, 1.0f * vmin, {0.0f, 0.0f, 0.0f, 0.8f})
                        .build();

                    ui.svg("wand.body")
                        .position(0.0f, 0.0f)
                        .size(wandWidth, wandHeight)
                        .source(kWandSvg)
                        .build();
                })
                .build();
        })
        .build();
}

} // namespace app
