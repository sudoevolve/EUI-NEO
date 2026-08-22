#pragma once

#include <core/render/shadertoy.h>

#include "core/animation.h"
#include "core/layout.h"
#include "core/input/input_types.h"
#include "core/render/render_types.h"
#include "core/render/text_types.h"

namespace eui {

using Align = core::Align;
using AnimProperty = core::AnimProperty;
using Ease = core::Ease;
using Transition = core::Transition;
using Color = core::Color;
using Vec2 = core::Vec2;
using Vec3 = core::Vec3;
using Rect = core::Rect;
using SizeValue = core::SizeValue;
using Gradient = core::Gradient;
using GradientDirection = core::GradientDirection;
using Border = core::Border;
using Shadow = core::Shadow;
using Transform = core::Transform;
using TransformMatrix = core::TransformMatrix;
using HorizontalAlign = core::HorizontalAlign;
using VerticalAlign = core::VerticalAlign;
using TextStyle = core::TextStyle;
using CursorShape = core::CursorShape;
using InputKey = core::InputKey;
using KeyAction = core::KeyAction;
using KeyModifiers = core::KeyModifiers;
using KeyEvent = core::KeyEvent;
using TextInputEvent = core::TextInputEvent;
using PointerAction = core::PointerAction;
using PointerButton = core::PointerButton;
using PointerButtons = core::PointerButtons;
using PointerEvent = core::PointerEvent;
using ShaderToyChannel = core::render::ShaderToyChannel;
using ShaderToyChannelKind = core::render::ShaderToyChannelKind;
using ShaderToyError = core::render::ShaderToyError;
using ShaderToyErrorCode = core::render::ShaderToyErrorCode;
using ShaderToyGraph = core::render::ShaderToyGraph;
using ShaderToyPass = core::render::ShaderToyPass;
using ShaderToySourceKind = core::render::ShaderToySourceKind;
using ShaderToyUniform = core::render::ShaderToyUniform;
using ShaderToyUniformKind = core::render::ShaderToyUniformKind;
using core::render::loadShaderToyGraphJson;
using core::render::parseShaderToyGraphJson;

inline Color mixColor(const Color& from, const Color& to, float amount) {
    return core::mixColor(from, to, amount);
}

} // namespace eui
