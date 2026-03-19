inline constexpr bool has_glfw_window_backend() {
#ifdef EUI_ENABLE_GLFW_OPENGL_BACKEND
    return true;
#else
    return false;
#endif
}

inline constexpr bool has_sdl2_window_backend() {
#ifdef EUI_ENABLE_SDL2_OPENGL_BACKEND
    return true;
#else
    return false;
#endif
}

inline WindowBackend resolve_window_backend(WindowBackend requested) {
    if (requested != WindowBackend::Auto) {
        return requested;
    }
    if (has_glfw_window_backend()) {
        return WindowBackend::GLFW;
    }
    if (has_sdl2_window_backend()) {
        return WindowBackend::SDL2;
    }
    return WindowBackend::Auto;
}

template <typename BuildUiFn>
int run(BuildUiFn&& build_ui, const AppOptions& options = {}) {
    const WindowBackend backend = resolve_window_backend(options.window_backend);
    switch (backend) {
        case WindowBackend::GLFW:
#ifdef EUI_ENABLE_GLFW_OPENGL_BACKEND
            return run_with_glfw(std::forward<BuildUiFn>(build_ui), options);
#else
            break;
#endif
        case WindowBackend::SDL2:
#ifdef EUI_ENABLE_SDL2_OPENGL_BACKEND
            return run_with_sdl2(std::forward<BuildUiFn>(build_ui), options);
#else
            break;
#endif
        case WindowBackend::Auto:
        default:
            break;
    }

    std::cerr << "No compiled window backend matched the requested AppOptions::window_backend." << std::endl;
    return EXIT_FAILURE;
}
