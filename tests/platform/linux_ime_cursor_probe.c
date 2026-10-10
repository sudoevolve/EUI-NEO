// Run with an isolated X11 display and an XIM server (e.g. IBus --xim).
// Private GLFW access is confined to this upstream test, never application code.
#define GLFW_EXPOSE_NATIVE_X11
#include "../../3rd/glfw/src/internal.h"
#include <GLFW/glfw3native.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

static void committed(GLFWwindow* window, unsigned int codepoint) {
    (void)window;
    printf("commit U+%04X\n", codepoint); fflush(stdout);
}
static void move_caret(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode; (void)mods;
    if (key == GLFW_KEY_F2 && action == GLFW_PRESS)
        glfwSetX11InputMethodCursorPos(window, 300, 200);
}
int main(int argc, char** argv)
{
    setlocale(LC_ALL, "");
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(640, 480, "IME cursor probe", NULL, NULL);
    if (!window) return 2;
    glfwPollEvents();
    _GLFWwindow* native = (_GLFWwindow*) window;
    if (!native->x11.ic) { fprintf(stderr, "No XIM input context\n"); return 3; }
    if (!native->x11.imePosition) {
        fprintf(stderr, "XIM has no position style\n"); return 4;
    }
    const int coordinates[][2] = {{120, 360}, {300, 200}, {40, 420}};
    for (int i = 0; i < 3; ++i) {
        if (!glfwSetX11InputMethodCursorPos(window, coordinates[i][0], coordinates[i][1])) return 5;
    }
    if (argc > 1 && strcmp(argv[1], "--interactive") == 0) {
        glfwSetWindowPos(window, 100, 80);
        glfwFocusWindow(window);
        glfwSetCharCallback(window, committed);
        glfwSetKeyCallback(window, move_caret);
        glfwSetX11InputMethodCursorPos(window, 120, 360);
        puts("interactive ready"); fflush(stdout);
        for (int i = 0; i < 240; ++i) glfwWaitEventsTimeout(0.05);
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    puts("XIM position style and three caret updates accepted");
    return 0;
}
