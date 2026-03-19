inline void handle_sdl_key_event(const SDL_KeyboardEvent& key, RuntimeState& state) {
    if (key.type != SDL_KEYDOWN) {
        return;
    }

    switch (key.keysym.sym) {
        case SDLK_BACKSPACE:
            state.pending_backspace = true;
            break;
        case SDLK_DELETE:
            state.pending_delete = true;
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            state.pending_enter = true;
            break;
        case SDLK_ESCAPE:
            state.pending_escape = true;
            break;
        case SDLK_LEFT:
            state.pending_left = true;
            break;
        case SDLK_RIGHT:
            state.pending_right = true;
            break;
        case SDLK_UP:
            state.pending_up = true;
            break;
        case SDLK_DOWN:
            state.pending_down = true;
            break;
        case SDLK_HOME:
            state.pending_home = true;
            break;
        case SDLK_END:
            state.pending_end = true;
            break;
        default:
            break;
    }
}

inline void accumulate_sdl_event(SDL_Window* window, const SDL_Event& event, RuntimeState& state) {
    switch (event.type) {
        case SDL_QUIT:
            state.quit_requested = true;
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_CLOSE &&
                (window == nullptr || event.window.windowID == SDL_GetWindowID(window))) {
                state.quit_requested = true;
            }
            break;
        case SDL_TEXTINPUT:
            state.text_input.append(event.text.text);
            break;
        case SDL_MOUSEWHEEL:
            state.scroll_y_accum += static_cast<double>(event.wheel.y);
            break;
        case SDL_KEYDOWN:
            handle_sdl_key_event(event.key, state);
            break;
        default:
            break;
    }
}

inline void pump_sdl_events(bool blocking, double timeout_seconds, SDL_Window* window, RuntimeState& state) {
    SDL_Event event{};
    if (blocking) {
        const int timeout_ms = std::max(1, static_cast<int>(std::round(timeout_seconds * 1000.0)));
        if (SDL_WaitEventTimeout(&event, timeout_ms) == 1) {
            accumulate_sdl_event(window, event, state);
        }
    }

    while (SDL_PollEvent(&event) == 1) {
        accumulate_sdl_event(window, event, state);
    }
}

inline bool sdl_ctrl_down() {
    return (SDL_GetModState() & KMOD_CTRL) != 0;
}

inline bool sdl_shift_down() {
    return (SDL_GetModState() & KMOD_SHIFT) != 0;
}

inline float sdl_window_dpi_scale(SDL_Window* window) {
    if (window == nullptr) {
        return 1.0f;
    }
    const int display_index = SDL_GetWindowDisplayIndex(window);
    if (display_index < 0) {
        return 1.0f;
    }

    float ddpi = 0.0f;
    float hdpi = 0.0f;
    float vdpi = 0.0f;
    if (SDL_GetDisplayDPI(display_index, &ddpi, &hdpi, &vdpi) != 0) {
        return 1.0f;
    }

    const float dpi = ddpi > 1.0f ? ddpi : (hdpi > 1.0f ? hdpi : vdpi);
    if (dpi <= 1.0f) {
        return 1.0f;
    }
    return dpi / 96.0f;
}
