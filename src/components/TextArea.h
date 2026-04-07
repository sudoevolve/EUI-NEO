#pragma once

#include "InputBox.h"

namespace EUINEO {

class TextAreaNode : public InputBoxNode {
public:
    using Builder = InputBoxNode::Builder;

    explicit TextAreaNode(const std::string& key) : InputBoxNode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "TextAreaNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

protected:
    void resetDefaults() override {
        InputBoxNode::resetDefaults();
        primitive_.width = 300.0f;
        primitive_.height = 120.0f;
    }
};

} // namespace EUINEO
