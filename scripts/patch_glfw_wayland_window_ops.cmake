if(NOT DEFINED GLFW_WAYLAND_SOURCE OR NOT DEFINED GLFW_WAYLAND_OUTPUT)
    message(FATAL_ERROR "GLFW_WAYLAND_SOURCE and GLFW_WAYLAND_OUTPUT are required")
endif()
if(NOT EXISTS "${GLFW_WAYLAND_SOURCE}")
    message(FATAL_ERROR "GLFW Wayland source not found: ${GLFW_WAYLAND_SOURCE}")
endif()

set(expected_source_hash "62E33D84A5F1EBC7D06979194AEA50351A9A8D396B27F20E894FC693ED3A117F")
set(expected_output_hash "992EAB1778021B0C0675CC067A870D28F6C54EB66596C78FFC25A9BC45E1D51A")
get_filename_component(output_directory "${GLFW_WAYLAND_OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")
file(READ "${GLFW_WAYLAND_SOURCE}" source)
string(REPLACE "\r\n" "\n" source "${source}")
string(SHA256 source_hash "${source}")
string(TOUPPER "${source_hash}" source_hash)

if(source_hash STREQUAL expected_output_hash)
    configure_file("${GLFW_WAYLAND_SOURCE}" "${GLFW_WAYLAND_OUTPUT}" COPYONLY)
    return()
endif()
if(NOT source_hash STREQUAL expected_source_hash)
    message(FATAL_ERROR
        "Unsupported GLFW Wayland source hash ${source_hash}; expected bundled GLFW 3.4 "
        "${expected_source_hash} or the reviewed patched source. Review and update "
        "scripts/patch_glfw_wayland_window_ops.cmake for this GLFW revision.")
endif()

set(pointer_handler_before [=[static void pointerHandleButton(void* userData,]=])
set(pointer_handler_after [=[/* EUI-NEO: track the active press and expose compositor move/resize. */
static _GLFWwindow* euiWaylandPointerButtonWindow;
static uint32_t euiWaylandPointerButtonSerial;

static void pointerHandleButton(void* userData,]=])
set(pointer_focus_before [=[    _GLFWwindow* window = _glfw.wl.pointerFocus;
    if (!window)
        return;

    if (window->wl.hovered)]=])
set(pointer_focus_after [=[    _GLFWwindow* window = _glfw.wl.pointerFocus;

    if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_RELEASED)
    {
        euiWaylandPointerButtonWindow = NULL;
        euiWaylandPointerButtonSerial = 0;
    }

    if (!window)
        return;

    if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED)
    {
        euiWaylandPointerButtonWindow = window;
        euiWaylandPointerButtonSerial = serial;
    }

    if (window->wl.hovered)]=])
set(window_destroy_before [=[void _glfwDestroyWindowWayland(_GLFWwindow* window)
{
    if (window == _glfw.wl.pointerFocus)]=])
set(window_destroy_after [=[void _glfwDestroyWindowWayland(_GLFWwindow* window)
{
    if (euiWaylandPointerButtonWindow == window)
    {
        euiWaylandPointerButtonWindow = NULL;
        euiWaylandPointerButtonSerial = 0;
    }

    if (window == _glfw.wl.pointerFocus)]=])
set(native_api_before [=[//////////////////////////////////////////////////////////////////////////
//////                        GLFW native API                       //////
//////////////////////////////////////////////////////////////////////////]=])
set(native_api_after [=[static int euiWaylandWindowMoveResize(_GLFWwindow* window, int edge)
{
    if (_glfw.platform.platformID != GLFW_PLATFORM_WAYLAND ||
        !window || !window->wl.xdg.toplevel || !_glfw.wl.seat ||
        !_glfw.wl.display || euiWaylandPointerButtonWindow != window ||
        euiWaylandPointerButtonSerial == 0)
    {
        return GLFW_FALSE;
    }

    if (edge < 0)
    {
        xdg_toplevel_move(window->wl.xdg.toplevel,
                          _glfw.wl.seat,
                          euiWaylandPointerButtonSerial);
    }
    else
    {
        static const uint32_t resizeEdges[] =
        {
            XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT,
            XDG_TOPLEVEL_RESIZE_EDGE_TOP,
            XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT,
            XDG_TOPLEVEL_RESIZE_EDGE_RIGHT,
            XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT,
            XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM,
            XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT,
            XDG_TOPLEVEL_RESIZE_EDGE_LEFT
        };

        if (edge >= (int) (sizeof(resizeEdges) / sizeof(resizeEdges[0])))
            return GLFW_FALSE;

        xdg_toplevel_resize(window->wl.xdg.toplevel,
                            _glfw.wl.seat,
                            euiWaylandPointerButtonSerial,
                            resizeEdges[edge]);
    }

    if (wl_display_flush(_glfw.wl.display) < 0 && errno != EAGAIN)
        return GLFW_FALSE;

    return GLFW_TRUE;
}

GLFWAPI int eui_glfw_wayland_window_move(GLFWwindow* handle)
{
    _GLFW_REQUIRE_INIT_OR_RETURN(GLFW_FALSE);
    return euiWaylandWindowMoveResize((_GLFWwindow*) handle, -1);
}

GLFWAPI int eui_glfw_wayland_window_resize(GLFWwindow* handle, int edge)
{
    _GLFW_REQUIRE_INIT_OR_RETURN(GLFW_FALSE);
    if (edge < 0 || edge > 7)
        return GLFW_FALSE;

    return euiWaylandWindowMoveResize((_GLFWwindow*) handle, edge);
}

//////////////////////////////////////////////////////////////////////////
//////                        GLFW native API                       //////
//////////////////////////////////////////////////////////////////////////]=])

foreach(required_context
        "${pointer_handler_before}"
        "${pointer_focus_before}"
        "${window_destroy_before}"
        "${native_api_before}")
    string(FIND "${source}" "${required_context}" context_offset)
    if(context_offset EQUAL -1)
        message(FATAL_ERROR "GLFW Wayland source does not match the reviewed patch context")
    endif()
endforeach()

string(REPLACE "${pointer_handler_before}" "${pointer_handler_after}" source "${source}")
string(REPLACE "${pointer_focus_before}" "${pointer_focus_after}" source "${source}")
string(REPLACE "${window_destroy_before}" "${window_destroy_after}" source "${source}")
string(REPLACE "${native_api_before}" "${native_api_after}" source "${source}")
file(WRITE "${GLFW_WAYLAND_OUTPUT}" "${source}")
string(SHA256 output_hash "${source}")
string(TOUPPER "${output_hash}" output_hash)
if(NOT output_hash STREQUAL expected_output_hash)
    file(REMOVE "${GLFW_WAYLAND_OUTPUT}")
    message(FATAL_ERROR "Patched GLFW Wayland source hash mismatch: ${output_hash}")
endif()
