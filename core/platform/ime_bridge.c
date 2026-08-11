#include "core/platform/ime_bridge.h"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <string.h>

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wingdi.h>
#include <imm.h>
#include <stdlib.h>

#define EUI_IME_FILTER_PROP L"EuiNeoImeFilter"

typedef struct EuiImeFilterState {
    WNDPROC previousProc;
    double cursorX;
    double cursorY;
    double cursorWidth;
    double cursorHeight;
    double fontHeight;
    BOOL hasCursorRect;
    BOOL applyingCursorRect;
} EuiImeFilterState;

static LONG eui_ime_round_long(double value) {
    return (LONG)(value >= 0.0 ? value + 0.5 : value - 0.5);
}

static void eui_ime_apply_font(HIMC context, double fontHeight) {
    if (context == 0 || fontHeight <= 0.0) {
        return;
    }

    if (fontHeight < 12.0) {
        fontHeight = 12.0;
    }
    LOGFONTW font;
    ZeroMemory(&font, sizeof(font));
    font.lfHeight = -eui_ime_round_long(fontHeight);
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfQuality = CLEARTYPE_QUALITY;
    wcscpy_s(font.lfFaceName, LF_FACESIZE, L"Microsoft YaHei UI");
    ImmSetCompositionFontW(context, &font);
}

static void eui_ime_apply_cursor_rect(HWND hwnd,
                                      double x,
                                      double y,
                                      double width,
                                      double height,
                                      double fontHeight) {
    if (hwnd == 0) {
        return;
    }

    HIMC context = ImmGetContext(hwnd);
    if (context == 0) {
        return;
    }

    eui_ime_apply_font(context, fontHeight);

    // Keep both IMM placement paths on the same line box.  Some IMEs derive
    // the candidate position from COMPOSITIONFORM even when CANDIDATEFORM is
    // supplied; placing the composition point at the line bottom makes them
    // add a second line of vertical offset.
    const LONG caretX = eui_ime_round_long(x);
    const LONG compositionY = eui_ime_round_long(y);

    COMPOSITIONFORM composition;
    ZeroMemory(&composition, sizeof(composition));
    composition.dwStyle = CFS_FORCE_POSITION;
    composition.ptCurrentPos.x = caretX;
    composition.ptCurrentPos.y = compositionY;
    composition.rcArea.left = eui_ime_round_long(x);
    composition.rcArea.top = eui_ime_round_long(y);
    composition.rcArea.right = eui_ime_round_long(x + width);
    composition.rcArea.bottom = eui_ime_round_long(y + height);
    ImmSetCompositionWindow(context, &composition);

    CANDIDATEFORM candidate;
    ZeroMemory(&candidate, sizeof(candidate));
    candidate.dwIndex = 0;
    // CFS_CANDIDATEPOS is only a preferred point and Microsoft Pinyin may
    // move it below the entire composition area.  Exclude the cursor
    // rectangle instead, so the IME chooses the adjacent free position.
    candidate.dwStyle = CFS_EXCLUDE;
    candidate.rcArea = composition.rcArea;
    ImmSetCandidateWindow(context, &candidate);

    ImmReleaseContext(hwnd, context);
}

static void eui_ime_reapply_cursor_rect(HWND hwnd, EuiImeFilterState* state) {
    if (state == 0 || !state->hasCursorRect || state->applyingCursorRect) {
        return;
    }
    state->applyingCursorRect = TRUE;
    eui_ime_apply_cursor_rect(hwnd,
                              state->cursorX,
                              state->cursorY,
                              state->cursorWidth,
                              state->cursorHeight,
                              state->fontHeight);
    if ((EuiImeFilterState*)GetPropW(hwnd, EUI_IME_FILTER_PROP) == state) {
        state->applyingCursorRect = FALSE;
    }
}

static LRESULT CALLBACK eui_ime_window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    EuiImeFilterState* state = (EuiImeFilterState*)GetPropW(hwnd, EUI_IME_FILTER_PROP);
    const BOOL placementChanged = message == WM_IME_COMPOSITION ||
                                  (message == WM_IME_NOTIFY &&
                                   (wParam == IMN_OPENCANDIDATE || wParam == IMN_CHANGECANDIDATE));
    if (message == WM_IME_COMPOSITION && (lParam & GCS_COMPSTR) != 0) {
        lParam &= ~GCS_COMPSTR;
        if (lParam == 0) {
            eui_ime_reapply_cursor_rect(hwnd, state);
            return 0;
        }
    }
    LRESULT result;
    if (state != 0 && state->previousProc != 0) {
        result = CallWindowProcW(state->previousProc, hwnd, message, wParam, lParam);
    } else {
        result = DefWindowProcW(hwnd, message, wParam, lParam);
    }
    if (placementChanged) {
        eui_ime_reapply_cursor_rect(
            hwnd,
            (EuiImeFilterState*)GetPropW(hwnd, EUI_IME_FILTER_PROP));
    }
    return result;
}

void eui_ime_install_message_filter(GLFWwindow* window) {
    if (window == 0) {
        return;
    }

    HWND hwnd = glfwGetWin32Window(window);
    if (hwnd == 0 || GetPropW(hwnd, EUI_IME_FILTER_PROP) != 0) {
        return;
    }

    EuiImeFilterState* state = (EuiImeFilterState*)calloc(1, sizeof(EuiImeFilterState));
    if (state == 0) {
        return;
    }
    state->previousProc = (WNDPROC)GetWindowLongPtrW(hwnd, GWLP_WNDPROC);
    if (state->previousProc == 0) {
        free(state);
        return;
    }
    SetPropW(hwnd, EUI_IME_FILTER_PROP, state);
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)eui_ime_window_proc);
}

void eui_ime_uninstall_message_filter(GLFWwindow* window) {
    if (window == 0) {
        return;
    }

    HWND hwnd = glfwGetWin32Window(window);
    if (hwnd == 0) {
        return;
    }

    EuiImeFilterState* state = (EuiImeFilterState*)GetPropW(hwnd, EUI_IME_FILTER_PROP);
    if (state == 0) {
        return;
    }
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)state->previousProc);
    RemovePropW(hwnd, EUI_IME_FILTER_PROP);
    free(state);
}

void eui_ime_set_cursor_rect(GLFWwindow* window, double x, double y, double width, double height) {
    eui_ime_set_cursor_rect_with_font(window, x, y, width, height, height);
}

void eui_ime_set_cursor_rect_with_font(GLFWwindow* window, double x, double y, double width, double height, double fontHeight) {
    if (window == 0) {
        return;
    }

    HWND hwnd = glfwGetWin32Window(window);
    if (hwnd == 0) {
        return;
    }

    EuiImeFilterState* state = (EuiImeFilterState*)GetPropW(hwnd, EUI_IME_FILTER_PROP);
    if (state != 0) {
        state->cursorX = x;
        state->cursorY = y;
        state->cursorWidth = width;
        state->cursorHeight = height;
        state->fontHeight = fontHeight;
        state->hasCursorRect = TRUE;
    }
    eui_ime_apply_cursor_rect(hwnd, x, y, width, height, fontHeight);
}

int eui_ime_is_composing(GLFWwindow* window) {
    if (window == 0) {
        return 0;
    }

    HWND hwnd = glfwGetWin32Window(window);
    if (hwnd == 0) {
        return 0;
    }

    HIMC context = ImmGetContext(hwnd);
    if (context == 0) {
        return 0;
    }

    const LONG length = ImmGetCompositionStringW(context, GCS_COMPSTR, 0, 0);
    ImmReleaseContext(hwnd, context);
    return length > 0 ? 1 : 0;
}

int eui_ime_get_composition_string_utf8(GLFWwindow* window, char* buffer, int bufferSize) {
    if (buffer != 0 && bufferSize > 0) {
        buffer[0] = '\0';
    }
    if (window == 0 || buffer == 0 || bufferSize <= 0) {
        return 0;
    }

    HWND hwnd = glfwGetWin32Window(window);
    if (hwnd == 0) {
        return 0;
    }

    HIMC context = ImmGetContext(hwnd);
    if (context == 0) {
        return 0;
    }

    const LONG byteLength = ImmGetCompositionStringW(context, GCS_COMPSTR, 0, 0);
    if (byteLength <= 0) {
        ImmReleaseContext(hwnd, context);
        return 0;
    }

    WCHAR* wide = (WCHAR*)malloc((size_t)byteLength + sizeof(WCHAR));
    if (wide == 0) {
        ImmReleaseContext(hwnd, context);
        return 0;
    }
    const LONG copied = ImmGetCompositionStringW(context, GCS_COMPSTR, wide, byteLength);
    ImmReleaseContext(hwnd, context);
    if (copied <= 0) {
        free(wide);
        return 0;
    }
    wide[copied / (LONG)sizeof(WCHAR)] = L'\0';

    const int needed = WideCharToMultiByte(CP_UTF8, 0, wide, -1, 0, 0, 0, 0);
    if (needed <= 0) {
        free(wide);
        return 0;
    }
    const int written = WideCharToMultiByte(CP_UTF8, 0, wide, -1, buffer, bufferSize, 0, 0);
    free(wide);
    if (written <= 0) {
        buffer[0] = '\0';
        return 0;
    }
    return written - 1;
}

void eui_ime_clear_composition(GLFWwindow* window) {
    if (window == 0) {
        return;
    }

    HWND hwnd = glfwGetWin32Window(window);
    if (hwnd == 0) {
        return;
    }

    HIMC context = ImmGetContext(hwnd);
    if (context == 0) {
        return;
    }

    ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
    ImmReleaseContext(hwnd, context);
}

#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

static char euiImeRectKey;
static Class euiImeSwizzledClass = Nil;
static IMP euiImeOriginalFirstRect = nil;

static int eui_ime_copy_nsstring_utf8(NSString* text, char* buffer, int bufferSize) {
    if (buffer != 0 && bufferSize > 0) {
        buffer[0] = '\0';
    }
    if (text == nil || buffer == 0 || bufferSize <= 0 || [text length] == 0) {
        return 0;
    }

    const char* utf8 = [text UTF8String];
    if (utf8 == 0 || utf8[0] == '\0') {
        return 0;
    }

    const NSUInteger length = [text lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    const int copyLength = (int)MIN((NSUInteger)(bufferSize - 1), length);
    memcpy(buffer, utf8, (size_t)copyLength);
    buffer[copyLength] = '\0';
    return copyLength;
}

static NSString* eui_ime_marked_text(NSView* view) {
    if (view == nil) {
        return nil;
    }

    Ivar markedTextIvar = class_getInstanceVariable([view class], "markedText");
    if (markedTextIvar != nil) {
        id markedText = object_getIvar(view, markedTextIvar);
        if ([markedText isKindOfClass:[NSAttributedString class]]) {
            return [(NSAttributedString*)markedText string];
        }
        if ([markedText isKindOfClass:[NSString class]]) {
            return (NSString*)markedText;
        }
    }

    if ([view respondsToSelector:@selector(markedRange)] &&
        [view respondsToSelector:@selector(attributedSubstringForProposedRange:actualRange:)]) {
        NSRange markedRange = [(id)view markedRange];
        if (markedRange.location != NSNotFound && markedRange.length > 0) {
            NSRange actualRange = NSMakeRange(NSNotFound, 0);
            NSAttributedString* attributedText = [(id)view attributedSubstringForProposedRange:markedRange
                                                                                   actualRange:&actualRange];
            return [attributedText string];
        }
    }

    return nil;
}

static void eui_ime_clear_marked_text(NSView* view) {
    if (view == nil) {
        return;
    }

    Ivar markedTextIvar = class_getInstanceVariable([view class], "markedText");
    if (markedTextIvar != nil) {
        id markedText = object_getIvar(view, markedTextIvar);
        if ([markedText isKindOfClass:[NSMutableAttributedString class]]) {
            [[(NSMutableAttributedString*)markedText mutableString] setString:@""];
            return;
        }
        if ([markedText isKindOfClass:[NSAttributedString class]] ||
            [markedText isKindOfClass:[NSString class]]) {
            NSMutableAttributedString* empty = [[NSMutableAttributedString alloc] init];
            object_setIvar(view, markedTextIvar, empty);
            [empty release];
            return;
        }
    }

    if ([view respondsToSelector:@selector(unmarkText)]) {
        [(id)view unmarkText];
    }
}

static NSRect eui_ime_first_rect_for_character_range(id self, SEL selector, NSRange range, NSRangePointer actualRange) {
    if (actualRange != nil) {
        *actualRange = range;
    }

    NSValue* rectValue = objc_getAssociatedObject(self, &euiImeRectKey);
    if (rectValue != nil) {
        NSRect viewRect = [rectValue rectValue];
        NSRect windowRect = [self convertRect:viewRect toView:nil];
        return [[self window] convertRectToScreen:windowRect];
    }

    if (euiImeOriginalFirstRect != nil) {
        typedef NSRect (*FirstRectFn)(id, SEL, NSRange, NSRangePointer);
        return ((FirstRectFn)euiImeOriginalFirstRect)(self, selector, range, actualRange);
    }

    return NSMakeRect(0.0, 0.0, 0.0, 0.0);
}

static void eui_ime_prepare_view(NSView* view) {
    if (view == nil) {
        return;
    }

    Class viewClass = [view class];
    if (viewClass == euiImeSwizzledClass) {
        return;
    }

    SEL selector = @selector(firstRectForCharacterRange:actualRange:);
    Method method = class_getInstanceMethod(viewClass, selector);
    if (method == nil) {
        return;
    }

    euiImeOriginalFirstRect = method_getImplementation(method);
    method_setImplementation(method, (IMP)eui_ime_first_rect_for_character_range);
    euiImeSwizzledClass = viewClass;
}

void eui_ime_set_cursor_rect(GLFWwindow* window, double x, double y, double width, double height) {
    if (window == 0) {
        return;
    }

    NSWindow* nsWindow = glfwGetCocoaWindow(window);
    if (nsWindow == nil) {
        return;
    }

    NSView* view = [nsWindow contentView];
    if (view == nil) {
        return;
    }

    eui_ime_prepare_view(view);

    const CGFloat scale = MAX((CGFloat)1.0, [nsWindow backingScaleFactor]);
    const NSRect bounds = [view bounds];
    const CGFloat rectX = (CGFloat)x / scale;
    const CGFloat rectWidth = MAX((CGFloat)1.0, (CGFloat)width / scale);
    const CGFloat rectHeight = MAX((CGFloat)1.0, (CGFloat)height / scale);
    const CGFloat rectY = bounds.size.height - ((CGFloat)y / scale) - rectHeight;
    const NSRect viewRect = NSMakeRect(rectX, rectY, rectWidth, rectHeight);
    objc_setAssociatedObject(view, &euiImeRectKey, [NSValue valueWithRect:viewRect], OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

void eui_ime_set_cursor_rect_with_font(GLFWwindow* window, double x, double y, double width, double height, double fontHeight) {
    (void)fontHeight;
    eui_ime_set_cursor_rect(window, x, y, width, height);
}

void eui_ime_install_message_filter(GLFWwindow* window) {
    if (window == 0) {
        return;
    }

    NSWindow* nsWindow = glfwGetCocoaWindow(window);
    if (nsWindow == nil) {
        return;
    }

    eui_ime_prepare_view([nsWindow contentView]);
}

void eui_ime_uninstall_message_filter(GLFWwindow* window) {
    (void)window;
}

int eui_ime_is_composing(GLFWwindow* window) {
    if (window == 0) {
        return 0;
    }

    NSWindow* nsWindow = glfwGetCocoaWindow(window);
    if (nsWindow == nil) {
        return 0;
    }

    NSView* view = [nsWindow contentView];
    if (view == nil) {
        return 0;
    }

    if ([view respondsToSelector:@selector(hasMarkedText)] && [(id)view hasMarkedText]) {
        return 1;
    }

    NSString* markedText = eui_ime_marked_text(view);
    return markedText != nil && [markedText length] > 0 ? 1 : 0;
}

int eui_ime_get_composition_string_utf8(GLFWwindow* window, char* buffer, int bufferSize) {
    if (buffer != 0 && bufferSize > 0) {
        buffer[0] = '\0';
    }
    if (window == 0 || buffer == 0 || bufferSize <= 0) {
        return 0;
    }

    NSWindow* nsWindow = glfwGetCocoaWindow(window);
    if (nsWindow == nil) {
        return 0;
    }

    NSView* view = [nsWindow contentView];
    eui_ime_prepare_view(view);
    return eui_ime_copy_nsstring_utf8(eui_ime_marked_text(view), buffer, bufferSize);
}

void eui_ime_clear_composition(GLFWwindow* window) {
    if (window == 0) {
        return;
    }

    NSWindow* nsWindow = glfwGetCocoaWindow(window);
    if (nsWindow == nil) {
        return;
    }

    eui_ime_clear_marked_text([nsWindow contentView]);
}

#else

void eui_ime_set_cursor_rect(GLFWwindow* window, double x, double y, double width, double height) {
    (void)window;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}

void eui_ime_set_cursor_rect_with_font(GLFWwindow* window, double x, double y, double width, double height, double fontHeight) {
    (void)fontHeight;
    eui_ime_set_cursor_rect(window, x, y, width, height);
}

void eui_ime_install_message_filter(GLFWwindow* window) {
    (void)window;
}

void eui_ime_uninstall_message_filter(GLFWwindow* window) {
    (void)window;
}

int eui_ime_is_composing(GLFWwindow* window) {
    (void)window;
    return 0;
}

int eui_ime_get_composition_string_utf8(GLFWwindow* window, char* buffer, int bufferSize) {
    (void)window;
    if (buffer != 0 && bufferSize > 0) {
        buffer[0] = '\0';
    }
    return 0;
}

void eui_ime_clear_composition(GLFWwindow* window) {
    (void)window;
}

#endif
