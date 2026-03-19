inline void text_input_callback(GLFWwindow* window, unsigned int codepoint) {
    RuntimeState* state = static_cast<RuntimeState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr) {
        return;
    }
    if (codepoint < 32u && codepoint != '\t') {
        return;
    }
    if (codepoint <= 0x7Fu) {
        state->text_input.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FFu) {
        state->text_input.push_back(static_cast<char>(0xC0u | ((codepoint >> 6u) & 0x1Fu)));
        state->text_input.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
    } else if (codepoint <= 0xFFFFu) {
        state->text_input.push_back(static_cast<char>(0xE0u | ((codepoint >> 12u) & 0x0Fu)));
        state->text_input.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu)));
        state->text_input.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
    } else if (codepoint <= 0x10FFFFu) {
        state->text_input.push_back(static_cast<char>(0xF0u | ((codepoint >> 18u) & 0x07u)));
        state->text_input.push_back(static_cast<char>(0x80u | ((codepoint >> 12u) & 0x3Fu)));
        state->text_input.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu)));
        state->text_input.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
    }
}

inline void key_input_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) {
        return;
    }
    RuntimeState* state = static_cast<RuntimeState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr) {
        return;
    }

    switch (key) {
        case GLFW_KEY_BACKSPACE:
            state->pending_backspace = true;
            break;
        case GLFW_KEY_DELETE:
            state->pending_delete = true;
            break;
        case GLFW_KEY_ENTER:
        case GLFW_KEY_KP_ENTER:
            state->pending_enter = true;
            break;
        case GLFW_KEY_ESCAPE:
            state->pending_escape = true;
            break;
        case GLFW_KEY_LEFT:
            state->pending_left = true;
            break;
        case GLFW_KEY_RIGHT:
            state->pending_right = true;
            break;
        case GLFW_KEY_UP:
            state->pending_up = true;
            break;
        case GLFW_KEY_DOWN:
            state->pending_down = true;
            break;
        case GLFW_KEY_HOME:
            state->pending_home = true;
            break;
        case GLFW_KEY_END:
            state->pending_end = true;
            break;
        default:
            break;
    }
}

inline void scroll_callback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    RuntimeState* state = static_cast<RuntimeState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr) {
        return;
    }
    state->scroll_y_accum += yoffset;
}
