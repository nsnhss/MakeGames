// dear imgui: Platform Backend for SDL2
// This needs to be used along with a Renderer (e.g. DirectX11, OpenGL3, Vulkan..)
// (Info: SDL2 is a cross-platform general purpose library for handling windows, inputs, graphics context creation, etc.)
// (Prefer SDL 2.0.5+ for full feature support.)

// Implemented features:
//  [X] Platform: Clipboard support.
//  [X] Platform: Mouse support. Can discriminate Mouse/TouchScreen.
//  [X] Platform: Keyboard support. Since 1.87 we are using the io.AddKeyEvent() function. Pass ImGuiKey values to all key functions e.g. ImGui::IsKeyPressed(ImGuiKey_Space). [Legacy SDL_SCANCODE_* values are obsolete since 1.87 and not supported since 1.91.5]
//  [X] Platform: Gamepad support.
//  [X] Platform: Mouse cursor shape and visibility (ImGuiBackendFlags_HasMouseCursors). Disable with 'io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange'.
//  [X] Platform: Basic IME support. App needs to call 'SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");' before SDL_CreateWindow()!.
//  [X] Platform: Multi-viewport support (multiple windows). Enable with 'io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable'.
// Missing features or Issues:
//  [ ] Platform: Multi-viewport: Minimized windows seems to break mouse wheel events (at least under Windows).
//  [ ] Platform: Multi-viewport: Missing ImGuiBackendFlags_HasParentViewport support. The viewport->ParentViewportID field is ignored, and therefore io.ConfigViewportsNoDefaultParent has no effect either.

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// CHANGELOG
// (minor and older changes stripped away, please see git history for details)
//  2026-XX-XX: Platform: Added support for multiple windows via the ImGuiPlatformIO interface.
//  2026-04-16: Made ImGui_ImplSDL2_GetContentScaleForWindow(), ImGui_ImplSDL2_GetContentScaleForDisplay() helpers return a minimum of 1.0f, as some Linux setup seems to report <1.0f value and this breaks scaling border size. (#9369)
//  2026-03-09: [Docking] Fixed an issue dated 2025/04/09 (1.92 WIP) where a refactor+merge caused ImGuiBackendFlags_HasMouseHoveredViewport to never be set, causing foreign windows to be ignored when deciding of hovered viewport. (#9284)
//  2026-02-13: Inputs: systems other than X11 are back to starting mouse capture on mouse down (reverts 2025-02-26 change). Only X11 requires waiting for a drag by default (not ideal, but a better default for X11 users). Added ImGui_ImplSDL2_SetMouseCaptureMode() for X11 debugger users. (#3650, #6410, #9235)
//  2026-01-15: Changed GetClipboardText() handler to return nullptr on error aka clipboard contents is not text. Consistent with other backends. (#9168)
//  2025-09-24: Skip using the SDL_GetGlobalMouseState() state when one of our window is hovered, as the SDL_MOUSEMOTION data is reliable. Fix macOS notch mouse coordinates issue in fullscreen mode + better perf on X11. (#7919, #7786)
//  2025-09-18: Call platform_io.ClearPlatformHandlers() on shutdown.
//  2025-09-15: Content Scales are always reported as 1.0 on Wayland. (#8921)
//  2025-07-08: Made ImGui_ImplSDL2_GetContentScaleForWindow(), ImGui_ImplSDL2_GetContentScaleForDisplay() helpers return 1.0f on Emscripten and Android platforms, matching macOS logic. (#8742, #8733)
//  2025-06-11: Added ImGui_ImplSDL2_GetContentScaleForWindow(SDL_Window* window) and ImGui_ImplSDL2_GetContentScaleForDisplay(int display_index) helper to facilitate making DPI-aware apps.
//  2025-05-15: [Docking] Add Platform_GetWindowFramebufferScale() handler, to allow varying Retina display density on multiple monitors.
//  2025-04-09: [Docking] Revert update monitors and work areas information every frame. Only do it on Windows. (#8415, #8558)
//  2025-04-09: Don't attempt to call SDL_CaptureMouse() on drivers where we don't call SDL_GetGlobalMouseState(). (#8561)
//  2025-03-21: Fill gamepad inputs and set ImGuiBackendFlags_HasGamepad regardless of ImGuiConfigFlags_NavEnableGamepad being set.
//  2025-03-10: When dealing with OEM keys, use scancodes instead of translated keycodes to choose ImGuiKey values. (#7136, #7201, #7206, #7306, #7670, #7672, #8468)
//  2025-02-26: Only start SDL_CaptureMouse() when mouse is being dragged, to mitigate issues with e.g. Linux debuggers not claiming capture back. (#6410, #3650)
//  2025-02-25: [Docking] Revert to use SDL_GetDisplayBounds() for WorkPos/WorkRect if SDL_GetDisplayUsableBounds() failed.
//  2025-02-24: Avoid calling SDL_GetGlobalMouseState() when mouse is in relative mode.
//  2025-02-21: [Docking] Update monitors and work areas information every frame, as the later may change regardless of monitor changes. (#8415)
//  2025-02-18: Added ImGuiMouseCursor_Wait and ImGuiMouseCursor_Progress mouse cursor support.
//  2025-02-10: Using SDL_OpenURL() in platform_io.Platform_OpenInShellFn handler.
//  2025-01-20: Made ImGui_ImplSDL2_SetGamepadMode(ImGui_ImplSDL2_GamepadMode_Manual) accept an empty array.
//  2024-10-24: Emscripten: from SDL 2.30.9, SDL_EVENT_MOUSE_WHEEL event doesn't require dividing by 100.0f.
//  2024-09-09: use SDL_Vulkan_GetDrawableSize() when available. (#7967, #3190)
//  2024-08-22: moved some OS/backend related function pointers from ImGuiIO to ImGuiPlatformIO:
//               - io.GetClipboardTextFn    -> platform_io.Platform_GetClipboardTextFn
//               - io.SetClipboardTextFn    -> platform_io.Platform_SetClipboardTextFn
//               - io.PlatformOpenInShellFn -> platform_io.Platform_OpenInShellFn
//               - io.PlatformSetImeDataFn  -> platform_io.Platform_SetImeDataFn
//  2024-08-19: Storing SDL's Uint32 WindowID inside ImGuiViewport::PlatformHandle instead of SDL_Window*.
//  2024-08-19: ImGui_ImplSDL2_ProcessEvent() now ignores events intended for other SDL windows. (#7853)
//  2024-07-02: Emscripten: Added io.PlatformOpenInShellFn() handler for Emscripten versions.
//  2024-07-02: Update for io.SetPlatformImeDataFn() -> io.PlatformSetImeDataFn() renaming in main library.
//  2024-02-14: Inputs: Handle gamepad disconnection. Added ImGui_ImplSDL2_SetGamepadMode().
//  2023-10-05: Inputs: Added support for extra ImGuiKey values: F13 to F24 function keys, app back/forward keys.
//  2023-04-06: Inputs: Avoid calling SDL_StartTextInput()/SDL_StopTextInput() as they don't only pertain to IME. It's unclear exactly what their relation is to IME. (#6306)
//  2023-04-04: Inputs: Added support for io.AddMouseSourceEvent() to discriminate ImGuiMouseSource_Mouse/ImGuiMouseSource_TouchScreen. (#2702)
//  2023-02-23: Accept SDL_GetPerformanceCounter() not returning a monotonically increasing value. (#6189, #6114, #3644)
//  2023-02-07: Implement IME handler (io.SetPlatformImeDataFn will call SDL_SetTextInputRect()/SDL_StartTextInput()).
//  2023-02-07: *BREAKING CHANGE* Renamed this backend file from imgui_impl_sdl.cpp/.h to imgui_impl_sdl2.cpp/.h in prevision for the future release of SDL3.
//  2023-02-02: Avoid calling SDL_SetCursor() when cursor has not changed, as the function is surprisingly costly on Mac with latest SDL (may be fixed in next SDL version).
//  2023-02-02: Added support for SDL 2.0.18+ preciseX/preciseY mouse wheel data for smooth scrolling + Scaling X value on Emscripten (bug?). (#4019, #6096)
//  2023-02-02: Removed SDL_MOUSEWHEEL value clamping, as values seem correct in latest Emscripten. (#4019)
//  2023-02-01: Flipping SDL_MOUSEWHEEL 'wheel.x' value to match other backends and offer consistent horizontal scrolling direction. (#4019, #6096, #1463)
//  2022-10-11: Using 'nullptr' instead of 'NULL' as per our switch to C++11.
//  2022-09-26: Inputs: Disable SDL 2.0.22 new "auto capture" (SDL_HINT_MOUSE_AUTO_CAPTURE) which prevents drag and drop across windows for multi-viewport support + don't capture when drag and dropping. (#5710)
//  2022-09-26: Inputs: Renamed ImGuiKey_ModXXX introduced in 1.87 to ImGuiMod_XXX (old names still supported).
//  2022-03-22: Inputs: Fix mouse position issues when dragging outside of boundaries. SDL_CaptureMouse() erroneously still gives out LEAVE events when hovering OS decorations.
//  2022-03-22: Inputs: Added support for extra mouse buttons (SDL_BUTTON_X1/SDL_BUTTON_X2).
//  2022-02-04: Added SDL_Renderer* parameter to ImGui_ImplSDL2_InitForSDLRenderer(), so we can use SDL_GetRendererOutputSize() instead of SDL_GL_GetDrawableSize() when bound to a SDL_Renderer.
//  2022-01-26: Inputs: replaced short-lived io.AddKeyModsEvent() (added two weeks ago) with io.AddKeyEvent() using ImGuiKey_ModXXX flags. Sorry for the confusion.
//  2021-01-20: Inputs: calling new io.AddKeyAnalogEvent() for gamepad support, instead of writing directly to io.NavInputs[].
//  2022-01-17: Inputs: calling new io.AddMousePosEvent(), io.AddMouseButtonEvent(), io.AddMouseWheelEvent() API (1.87+).
//  2022-01-17: Inputs: always update key mods next and before key event (not in NewFrame) to fix input queue with very low framerates.
//  2022-01-12: Update mouse inputs using SDL_MOUSEMOTION/SDL_WINDOWEVENT_LEAVE + fallback to provide it when focused but not hovered/captured. More standard and will allow us to pass it to future input queue API.
//  2022-01-12: Maintain our own copy of MouseButtonsDown mask instead of using ImGui::IsAnyMouseDown() which will be obsoleted.
//  2022-01-10: Inputs: calling new io.AddKeyEvent(), io.AddKeyModsEvent() + io.SetKeyEventNativeData() API (1.87+). Support for full ImGuiKey range.
//  2021-08-17: Calling io.AddFocusEvent() on SDL_WINDOWEVENT_FOCUS_GAINED/SDL_WINDOWEVENT_FOCUS_LOST.
//  2021-07-29: Inputs: MousePos is correctly reported when the host platform window is hovered but not focused (using SDL_GetMouseFocus() + SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, requires SDL 2.0.5+)
//  2021-06:29: *BREAKING CHANGE* Removed 'SDL_Window* window' parameter to ImGui_ImplSDL2_NewFrame() which was unnecessary.
//  2021-06-29: Reorganized backend to pull data from a single structure to facilitate usage with multiple-contexts (all g_XXXX access changed to bd->XXXX).
//  2021-03-22: Rework global mouse pos availability check listing supported platforms explicitly, effectively fixing mouse access on Raspberry Pi. (#2837, #3950)
//  2020-05-25: Misc: Report a zero display-size when window is minimized, to be consistent with other backends.
//  2020-02-20: Inputs: Fixed mapping for ImGuiKey_KeyPadEnter (using SDL_SCANCODE_KP_ENTER instead of SDL_SCANCODE_RETURN2).
//  2019-12-17: Inputs: On Wayland, use SDL_GetMouseState (because there is no global mouse state).
//  2019-12-05: Inputs: Added support for ImGuiMouseCursor_NotAllowed mouse cursor.
//  2019-07-21: Inputs: Added mapping for ImGuiKey_KeyPadEnter.
//  2019-04-23: Inputs: Added support for SDL_GameController (if ImGuiConfigFlags_NavEnableGamepad is set by user application).
//  2019-03-12: Misc: Preserve DisplayFramebufferScale when main window is minimized.
//  2018-12-21: Inputs: Workaround for Android/iOS which don't seem to handle focus related calls.
//  2018-11-30: Misc: Setting up io.BackendPlatformName so it can be displayed in the About Window.
//  2018-11-14: Changed the signature of ImGui_ImplSDL2_ProcessEvent() to take a 'const SDL_Event*'.
//  2018-08-01: Inputs: Workaround for Emscripten which doesn't seem to handle focus related calls.
//  2018-06-29: Inputs: Added support for the ImGuiMouseCursor_Hand cursor.
//  2018-06-08: Misc: Extracted imgui_impl_sdl.cpp/.h away from the old combined SDL2+OpenGL/Vulkan examples.
//  2018-06-08: Misc: ImGui_ImplSDL2_InitForOpenGL() now takes a SDL_GLContext parameter.
//  2018-05-09: Misc: Fixed clipboard paste memory leak (we didn't call SDL_FreeMemory on the data returned by SDL_GetClipboardText).
//  2018-03-20: Misc: Setup io.BackendFlags ImGuiBackendFlags_HasMouseCursors flag + honor ImGuiConfigFlags_NoMouseCursorChange flag.
//  2018-02-16: Inputs: Added support for mouse cursors, honoring ImGui::GetMouseCursor() value.
//  2018-02-06: Misc: Removed call to ImGui::Shutdown() which is not available from 1.60 WIP, user needs to call CreateContext/DestroyContext themselves.
//  2018-02-06: Inputs: Added mapping for ImGuiKey_Space.
//  2018-02-05: Misc: Using SDL_GetPerformanceCounter() instead of SDL_GetTicks() to be able to handle very high framerate (1000+ FPS).
//  2018-02-05: Inputs: Keyboard mapping is using scancodes everywhere instead of a confusing mixture of keycodes and scancodes.
//  2018-01-20: Inputs: Added Horizontal Mouse Wheel support.
//  2018-01-19: Inputs: When available (SDL 2.0.4+) using SDL_CaptureMouse() to retrieve coordinates outside of client area when dragging. Otherwise (SDL 2.0.3 and before) testing for SDL_WINDOW_INPUT_FOCUS instead of SDL_WINDOW_MOUSE_FOCUS.
//  2018-01-18: Inputs: Added mapping for ImGuiKey_Insert.
//  2017-08-25: Inputs: MousePos set to -FLT_MAX,-FLT_MAX when mouse is unavailable/missing (instead of -1,-1).
//  2016-10-15: Misc: Added a void* user_data parameter to Clipboard function handlers.

#include "imgui.h"
#include "imgui_internal.h"
#ifndef IMGUI_DISABLE
#include "imgui_impl_sdl2.h"

// Clang warnings with -Weverything
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"                 // warning: use of old-style cast
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"  // warning: implicit conversion from 'xxx' to 'float' may lose precision
#endif

// SDL
// (the multi-viewports feature requires SDL features supported from SDL 2.0.4+. SDL 2.0.5+ is highly recommended)
#include <SDL.h>
#include <SDL_syswm.h>
#include <stdio.h>              // for snprintf()
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten/em_js.h>
#endif
#undef Status // X11 headers are leaking this.

#if SDL_VERSION_ATLEAST(2,0,4) && !defined(__EMSCRIPTEN__) && !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS) && !defined(__amigaos4__)
#define SDL_HAS_CAPTURE_AND_GLOBAL_MOUSE    1
#else
#define SDL_HAS_CAPTURE_AND_GLOBAL_MOUSE    0
#endif
#define SDL_HAS_WINDOW_ALPHA                SDL_VERSION_ATLEAST(2,0,5)
#define SDL_HAS_ALWAYS_ON_TOP               SDL_VERSION_ATLEAST(2,0,5)
#define SDL_HAS_USABLE_DISPLAY_BOUNDS       SDL_VERSION_ATLEAST(2,0,5)
#define SDL_HAS_PER_MONITOR_DPI             SDL_VERSION_ATLEAST(2,0,4)
#define SDL_HAS_VULKAN                      SDL_VERSION_ATLEAST(2,0,6)
#define SDL_HAS_DISPLAY_EVENT               SDL_VERSION_ATLEAST(2,0,9)
#define SDL_HAS_OPEN_URL                    SDL_VERSION_ATLEAST(2,0,14)
#define SDL_HAS_SHOW_WINDOW_ACTIVATION_HINT SDL_VERSION_ATLEAST(2,0,18)
#if SDL_HAS_VULKAN
#include <SDL_vulkan.h>
#else
static const Uint32 SDL_WINDOW_VULKAN = 0x10000000;
#endif

// 暴露给外部用的 composition string（拼音）
char g_SDLCompositionText[32] = {};
int  g_SDLCompositionCursor = 0;
int  g_SDLCompositionLength = 0;

// Inline composition 状态（由上层 launcher 消费）
bool g_SDLIMECommitted = false;
char g_SDLCommittedText[32] = {};
bool g_SDLIMEWasCancelled = false;
bool g_SDLIMEComposing = false;



// SDL Data
struct ImGui_ImplSDL2_Data
{
    SDL_Window*             Window;
    Uint32                  WindowID;       // Stored in ImGuiViewport::PlatformHandle. Use SDL_GetWindowFromID() to get SDL_Window* from Uint32 WindowID.
    SDL_Renderer*           Renderer;
    Uint64                  Time;
    char*                   ClipboardTextData;
    char                    BackendPlatformName[48];
    bool                    UseVulkan;
    bool                    WantUpdateMonitors;

    // Mouse handling
    Uint32                  MouseWindowID;
    int                     MouseButtonsDown;
    SDL_Cursor*             MouseCursors[ImGuiMouseCursor_COUNT];
    SDL_Cursor*             MouseLastCursor;
    int                     MouseLastLeaveFrame;
    bool                    MouseCanUseGlobalState;
    bool                    MouseCanReportHoveredViewport;  // This is hard to use/unreliable on SDL so we'll set ImGuiBackendFlags_HasMouseHoveredViewport dynamically based on state.
    ImGui_ImplSDL2_MouseCaptureMode MouseCaptureMode;

    // Gamepad handling
    ImVector<SDL_GameController*> Gamepads;
    ImGui_ImplSDL2_GamepadMode    GamepadMode;
    bool                          WantUpdateGamepadsList;

    // IME inline composition state
    // 存储在一个 composition 周期内的所有状态，以及供 WM_IME_STARTCOMPOSITION hook 使用的坐标。
    // 生命周期：WasComposing 在收到第一个 SDL_TEXTEDITING 时置 true，在 commit/cancel 时置 false。
    struct TextIMEState {
        bool   WasComposing = false;   // 当前是否处于 composition（拼音输入中）
        int    CompInsertPos = -1;     // 拼音在 buffer 中的插入位置（字节偏移）
        int    CompInsertLen = 0;      // 当前拼音在 buffer 中占据的字节长度
        bool   CompRectValid = false;  // CompRectMin/Max 是否有效（用于点击检测）
        ImVec2 CompRectMin;            // 拼音区域屏幕矩形左上角
        ImVec2 CompRectMax;            // 拼音区域屏幕矩形右下角
        int    CursorPos = 0;          // 上一帧光标位置（字节偏移），供非 composition 时计算 IME 位置
        ImVec2 SavedCursorScreenPos;   // 上一帧保存的光标屏幕坐标（供 hook 使用）
        float  SavedLineHeight = 0.0f; // 上一帧保存的行高（供 hook 使用）
        float  SavedWidgetRight = 0.0f;// 上一帧保存的 widget 右边界（供 hook 排除区域使用）
    } TextIME;

    ImGui_ImplSDL2_Data()   { memset((void*)this, 0, sizeof(*this)); }
};

// Backend data stored in io.BackendPlatformUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
// FIXME: multi-context support is not well tested and probably dysfunctional in this backend.
// FIXME: some shared resources (mouse cursor shape, gamepad) are mishandled when using multi-context.
static ImGui_ImplSDL2_Data* ImGui_ImplSDL2_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplSDL2_Data*)ImGui::GetIO().BackendPlatformUserData : nullptr;
}

// Windows 下 SDL_TEXTEDITING 经常拿不到拼音，用 IMM32 作为后备
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
#include <string>
static void ImGui_ImplSDL2_FetchIMMCompositionString()
{
    ImGui_ImplSDL2_Data* bd = ImGui_ImplSDL2_GetBackendData();
    if (!bd || !bd->Window) return;

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(bd->Window, &info)) return;

    HWND hwnd = info.info.win.window;
    if (!hwnd) return;

    HIMC himc = ImmGetContext(hwnd);
    if (!himc) return;

    LONG size = ImmGetCompositionStringW(himc, GCS_COMPSTR, NULL, 0);
    if (size > 0) {
        std::wstring wstr(size / sizeof(wchar_t), 0);
        ImmGetCompositionStringW(himc, GCS_COMPSTR, &wstr[0], size);
        int utf8_size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
        if (utf8_size > 0) {
            std::string result(utf8_size, 0);
            WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], utf8_size, NULL, NULL);
            if (!result.empty() && result.back() == '\0') result.pop_back();
            strncpy(g_SDLCompositionText, result.c_str(), sizeof(g_SDLCompositionText) - 1);
            g_SDLCompositionText[sizeof(g_SDLCompositionText) - 1] = '\0';
            g_SDLCompositionLength = (int)result.size();
        }
    } else {
        g_SDLCompositionText[0] = '\0';
        g_SDLCompositionLength = 0;
    }
    ImmReleaseContext(hwnd, himc);
}

// 主动取消 Windows IME 的当前 composition（上层检测到光标移出拼音区域时调用）
void ImGui_ImplSDL2_CancelIMEComposition()
{
    ImGui_ImplSDL2_Data* bd = ImGui_ImplSDL2_GetBackendData();
    if (!bd || !bd->Window) return;

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(bd->Window, &info)) return;

    HWND hwnd = info.info.win.window;
    if (!hwnd) return;

    HIMC himc = ImmGetContext(hwnd);
    if (!himc) return;

    ImmNotifyIME(himc, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
    ImmReleaseContext(hwnd, himc);
}

// 主动设置 SDL 输入矩形（修正第一帧候选框位置偏上）
// 注意：传入的 x,y 必须是屏幕坐标，函数内部会转换为窗口相对坐标
void ImGui_ImplSDL2_SetTextInputRect(float x, float y, float h)
{
    SDL_Rect r;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    r.x = (int)(x - viewport->Pos.x);
    r.y = (int)(y - viewport->Pos.y);
    r.w = 100;  // 与 Platform_SetImeDataFn 保持一致，避免 IME 因矩形过窄而计算异常
    r.h = (int)h;
    {
        FILE* fp = fopen("ime_trace.txt", "a");
        if (fp) {
            fprintf(fp, "[BackendDirect] x=%.2f y=%.2f h=%.2f viewport->Pos=(%.2f,%.2f) => r=(%d,%d,%d,%d)\n",
                    x, y, h, viewport->Pos.x, viewport->Pos.y,
                    r.x, r.y, r.w, r.h);
            fclose(fp);
        }
    }
    SDL_SetTextInputRect(&r);
}

// 绕过 SDL 的 CFS_POINT 行为，使用 Windows IMM CFS_EXCLUDE 避免候选框遮挡文本区域
// (x1,y1)-(x2,y2) 为屏幕坐标，表示需要排除的文本区域（通常是拼音所在行）
void ImGui_ImplSDL2_ExcludeTextArea(float x1, float y1, float x2, float y2)
{
    ImGui_ImplSDL2_Data* bd = ImGui_ImplSDL2_GetBackendData();
    if (!bd || !bd->Window) return;
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(bd->Window, &info)) return;
    HWND hwnd = info.info.win.window;
    if (!hwnd) return;
    HIMC himc = ImmGetContext(hwnd);
    if (!himc) return;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    int vx = (int)viewport->Pos.x;
    int vy = (int)viewport->Pos.y;

    // 设置候选窗口排除区域（不让候选框落在这个矩形内）
    CANDIDATEFORM caf;
    caf.dwIndex = 0;
    caf.dwStyle = CFS_EXCLUDE;
    caf.ptCurrentPos.x = (int)(x2 - vx);
    caf.ptCurrentPos.y = (int)(y2 - vy);
    caf.rcArea.left   = (int)(x1 - vx);
    caf.rcArea.top    = (int)(y1 - vy);
    caf.rcArea.right  = (int)(x2 - vx);
    caf.rcArea.bottom = (int)(y2 - vy);
    ImmSetCandidateWindow(himc, &caf);

    // 同时设置 composition window 的排除区域
    COMPOSITIONFORM cof;
    cof.dwStyle = CFS_EXCLUDE;
    cof.ptCurrentPos.x = (int)(x2 - vx);
    cof.ptCurrentPos.y = (int)(y2 - vy);
    cof.rcArea.left   = (int)(x1 - vx);
    cof.rcArea.top    = (int)(y1 - vy);
    cof.rcArea.right  = (int)(x2 - vx);
    cof.rcArea.bottom = (int)(y2 - vy);
    ImmSetCompositionWindow(himc, &cof);

    ImmReleaseContext(hwnd, himc);
}

// SDL_SetWindowsMessageHook 回调：拦截 WM_IME_STARTCOMPOSITION。
// 在 SDL 处理该消息之前立即重建 CANDIDATEFORM（CFS_EXCLUDE）和 COMPOSITIONFORM（CFS_POINT），
// 防止 IME 重置候选框位置导致首字符候选框错位。
// 坐标来自上一帧 RenderTextInputIME 保存的 TextIMEState。
static void SDLCALL ImGui_ImplSDL2_IMEStartCompositionHook(void* userdata, void* hWnd,
    unsigned int message, Uint64 wParam, Sint64 lParam)
{
    if (message == WM_IME_STARTCOMPOSITION) {
        ImVec2 pos;
        float line_height, widget_right;
        ImGui_ImplSDL2_GetTextIMESavedPos(&pos, &line_height, &widget_right);

        HIMC himc = ImmGetContext((HWND)hWnd);
        if (himc) {
            CANDIDATEFORM caf = {};
            caf.dwIndex = 0;
            caf.dwStyle = CFS_EXCLUDE;
            caf.rcArea.left   = (LONG)pos.x;
            caf.rcArea.top    = (LONG)pos.y;
            caf.rcArea.right  = (LONG)widget_right;
            caf.rcArea.bottom = (LONG)(pos.y + line_height);
            ImmSetCandidateWindow(himc, &caf);

            COMPOSITIONFORM cf = {};
            cf.dwStyle = CFS_POINT;
            cf.ptCurrentPos.x = (LONG)pos.x;
            cf.ptCurrentPos.y = (LONG)pos.y;
            ImmSetCompositionWindow(himc, &cf);

            ImmReleaseContext((HWND)hWnd, himc);
        }
    }
}
#endif

// ── IME Inline Composition Helpers ─────────────────────────

// 计算 buffer 中某字节偏移对应的屏幕坐标。
// 逐字符累加宽度（仅处理显式 '\n'，不处理 soft wrap），最后减去滚动偏移。
// 参数：
//   buf        — InputText buffer（UTF-8）
//   cursor_pos — 字节偏移（从 buf 开头算起）
//   text_origin— widget 内文本区域的左上角屏幕坐标
//   line_height— 行高（用于换行时累加 y）
//   scroll     — InputTextState 的滚动偏移（用于处理滚动后的可见区域）
// 返回：屏幕坐标（已减去 scroll 偏移）
static ImVec2 CalcCursorScreenPos(const char* buf, int cursor_pos,
    ImVec2 text_origin, float line_height, ImVec2 scroll)
{
    float x = text_origin.x;
    float y = text_origin.y;
    int buf_len = (int)strlen(buf);
    for (int i = 0; i < cursor_pos && i < buf_len; ) {
        if (buf[i] == '\n') {
            x = text_origin.x;
            y += line_height;
            i++;
        } else {
            unsigned int c;
            int char_len = ImTextCharFromUtf8(&c, buf + i, buf + i + (buf_len - i));
            if (char_len <= 0) char_len = 1;
            x += ImGui::CalcTextSize(buf + i, buf + i + char_len).x;
            i += char_len;
        }
    }
    return ImVec2(x - scroll.x, y - scroll.y);
}

// ImGuiInputTextFlags_CallbackAlways 回调。
// 管理 composition 生命周期：拼音插入、汉字替换、取消恢复、鼠标点击拼音区域归位。
// 与 SDL backend 全局状态（g_SDLIMEComposing / g_SDLIMECommitted / g_SDLCompositionText）配合工作。
// 每一步修改直接操作 ImGuiInputTextCallbackData，不需要上层干预。
int ImGui_ImplSDL2_InputTextCallback(ImGuiInputTextCallbackData* data)
{
    ImGui_ImplSDL2_Data* bd = ImGui_ImplSDL2_GetBackendData();
    if (!bd) return 0;
    ImGui_ImplSDL2_Data::TextIMEState& ime = bd->TextIME;

    if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways) {
        bool is_composing = g_SDLIMEComposing;

        // ── 1. 处理确认输入（SDL_TEXTINPUT）：用汉字替换拼音 ──
        if (ime.WasComposing && g_SDLIMECommitted) {
            if (ime.CompInsertLen > 0) {
                data->DeleteChars(ime.CompInsertPos, ime.CompInsertLen);
            }
            int commit_len = (int)strlen(g_SDLCommittedText);
            if (commit_len > 0) {
                data->InsertChars(ime.CompInsertPos, g_SDLCommittedText);
            }
            data->CursorPos = ime.CompInsertPos + commit_len;
            data->SelectionStart = data->SelectionEnd = data->CursorPos;

            ime.WasComposing = false;
            ime.CompInsertPos = -1;
            ime.CompInsertLen = 0;
            g_SDLIMECommitted = false;
            g_SDLCommittedText[0] = '\0';
            ime.CursorPos = data->CursorPos;
            return 0;
        }

        // ── 2. 检测鼠标点击拼音区域内部：恢复光标，不取消输入 ──
        if (ime.WasComposing && ImGui::IsMouseClicked(0) && ime.CompRectValid) {
            ImVec2 mouse_pos = ImGui::GetMousePos();
            if (mouse_pos.x >= ime.CompRectMin.x && mouse_pos.x <= ime.CompRectMax.x &&
                mouse_pos.y >= ime.CompRectMin.y && mouse_pos.y <= ime.CompRectMax.y) {
                data->CursorPos = ime.CompInsertPos + ime.CompInsertLen;
                data->SelectionStart = data->SelectionEnd = data->CursorPos;
                ime.CursorPos = data->CursorPos;
                return 0;
            }
        }

        // ── 3. 检测中断（光标移出拼音末尾、Escape、失焦点等）──
        if (ime.WasComposing) {
            bool cursor_moved = (data->CursorPos != ime.CompInsertPos + ime.CompInsertLen);
            if (!is_composing || cursor_moved) {
                // 取消输入：拼音保留为普通文本，去掉 IME 自动插入的分隔符
                for (int i = ime.CompInsertPos + ime.CompInsertLen; i > ime.CompInsertPos; ) {
                    int char_start = i - 1;
                    while (char_start > ime.CompInsertPos && ((unsigned char)data->Buf[char_start] & 0xC0) == 0x80)
                        char_start--;
                    unsigned int c;
                    int char_len = ImTextCharFromUtf8(&c, data->Buf + char_start, data->Buf + ime.CompInsertPos + ime.CompInsertLen);
                    if (char_len <= 0) char_len = 1;
                    bool is_pinyin_char = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
                    if (!is_pinyin_char) {
                        data->DeleteChars(char_start, char_len);
                    }
                    i = char_start;
                }
                ime.WasComposing = false;
                ime.CompInsertPos = -1;
                ime.CompInsertLen = 0;
                if (cursor_moved && is_composing) {
                    g_SDLIMEWasCancelled = true;
                    g_SDLCompositionText[0] = '\0';
                    g_SDLCompositionLength = 0;
                    g_SDLIMECommitted = false;
                    ImGui_ImplSDL2_CancelIMEComposition();
                }
                ime.CursorPos = data->CursorPos;
                return 0;
            }
        }

        // ── 4. 开始新的 composition ──
        if (is_composing && !ime.WasComposing) {
            ime.CompInsertPos = data->CursorPos;
            ime.CompInsertLen = 0;
            ime.WasComposing = true;
        }

        // ── 5. 更新 composition 拼音文本 ──
        if (ime.WasComposing) {
            if (ime.CompInsertLen > 0) {
                data->DeleteChars(ime.CompInsertPos, ime.CompInsertLen);
            }
            int comp_len = (int)strlen(g_SDLCompositionText);
            if (comp_len > 0) {
                data->InsertChars(ime.CompInsertPos, g_SDLCompositionText);
                ime.CompInsertLen = comp_len;
            } else {
                ime.CompInsertLen = 0;
            }
            data->CursorPos = ime.CompInsertPos + ime.CompInsertLen;
            data->SelectionStart = data->SelectionEnd = data->CursorPos;
        }

        ime.CursorPos = data->CursorPos;
    }
    return 0;
}

// 在 InputTextMultiline/InputText 之后调用，绘制拼音下划线并同步 IME 候选框位置。
// 必须在同一 widget 的渲染周期内调用（依赖 GetItemRectMin/GetItemID 等）。
// 非 active widget 不更新坐标，防止多 widget 竞争同一 IME 状态。
// 参数：
//   buf            — 与 InputText 相同的 UTF-8 buffer 指针（用于坐标计算）
//   underline_color— 下划线颜色（通常取主题 accent 色）
void ImGui_ImplSDL2_RenderTextInputIME(const char* buf, ImU32 underline_color)
{
    ImGui_ImplSDL2_Data* bd = ImGui_ImplSDL2_GetBackendData();
    if (!bd) return;
    ImGui_ImplSDL2_Data::TextIMEState& ime = bd->TextIME;

    // 非 active widget 不更新坐标（防止多 widget 竞争）
    if (!ImGui::IsItemActive())
        return;

    ime.CompRectValid = false;

    ImVec2 widget_min = ImGui::GetItemRectMin();
    ImVec2 widget_max = ImGui::GetItemRectMax();
    float line_height = ImGui::GetTextLineHeight();
    ImVec2 frame_pad  = ImGui::GetStyle().FramePadding;
    ImVec2 text_origin = ImVec2(widget_min.x + frame_pad.x, widget_min.y + frame_pad.y);

    ImVec2 scroll(0.0f, 0.0f);
    if (ImGuiInputTextState* state = ImGui::GetInputTextState(ImGui::GetItemID()))
        scroll = state->Scroll;

    if (ime.WasComposing && ime.CompInsertLen > 0) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 start_pos = CalcCursorScreenPos(buf, ime.CompInsertPos, text_origin, line_height, scroll);
        ImVec2 end_pos   = CalcCursorScreenPos(buf, ime.CompInsertPos + ime.CompInsertLen, text_origin, line_height, scroll);

        // 绘制下划线
        if (start_pos.y + line_height > widget_min.y && start_pos.y < widget_max.y) {
            float ul_y = start_pos.y + ImGui::GetFontSize() + 1.0f;
            dl->AddLine(ImVec2(start_pos.x, ul_y), ImVec2(end_pos.x, ul_y),
                underline_color, 2.0f);
        }

        // 记录 composition 区域矩形（供下一帧 CallbackAlways 做点击检测）
        float widget_right = widget_min.x + ImGui::GetContentRegionAvail().x;
        ime.CompRectMin = ImVec2(start_pos.x, start_pos.y);
        if (start_pos.y >= end_pos.y - line_height * 0.5f) {
            ime.CompRectMax = ImVec2(end_pos.x, end_pos.y + line_height);
        } else {
            ime.CompRectMax = ImVec2(widget_right, end_pos.y + line_height);
        }
        ime.CompRectValid = true;

        // 同步坐标到 hook
        ime.SavedCursorScreenPos = start_pos;
        ime.SavedLineHeight = line_height;
        ime.SavedWidgetRight = widget_right;

        // 主动设置 IME 候选框位置到拼音末尾
        ImGui_ImplSDL2_SetTextInputRect(end_pos.x, end_pos.y, line_height);

        // Windows: 使用 IMM CFS_EXCLUDE 避免候选框遮挡拼音区域
#ifdef _WIN32
        ImGui_ImplSDL2_ExcludeTextArea(start_pos.x, start_pos.y, end_pos.x, end_pos.y + line_height);
#endif

        // 立即调用 Platform_SetImeDataFn，绕过 NewFrame() 的一帧延迟
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        if (platform_io.Platform_SetImeDataFn) {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGuiPlatformImeData ime_data;
            ime_data.WantVisible = true;
            ime_data.InputPos = ImVec2(end_pos.x, end_pos.y);
            ime_data.InputLineHeight = line_height;
            platform_io.Platform_SetImeDataFn(ImGui::GetCurrentContext(), viewport, &ime_data);
        }
    } else {
        // 无 composition 时：也设 IME 位置到光标处，确保下次 composition 开始时位置正确
        int cursor_pos = ime.CursorPos;
        ImVec2 cursor_screen_pos = CalcCursorScreenPos(buf, cursor_pos, text_origin, line_height, scroll);
        ImGui_ImplSDL2_SetTextInputRect(cursor_screen_pos.x, cursor_screen_pos.y, line_height);

#ifdef _WIN32
        float widget_right = widget_min.x + ImGui::GetContentRegionAvail().x;
        ImGui_ImplSDL2_ExcludeTextArea(cursor_screen_pos.x, cursor_screen_pos.y,
            widget_right, cursor_screen_pos.y + line_height);
#endif

        // 同步坐标到 hook
        ime.SavedCursorScreenPos = cursor_screen_pos;
        ime.SavedLineHeight = line_height;
        ime.SavedWidgetRight = widget_min.x + ImGui::GetContentRegionAvail().x;

        // 同步 Platform_SetImeDataFn
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        if (platform_io.Platform_SetImeDataFn) {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGuiPlatformImeData ime_data;
            ime_data.WantVisible = true;
            ime_data.InputPos = ImVec2(cursor_screen_pos.x, cursor_screen_pos.y);
            ime_data.InputLineHeight = line_height;
            platform_io.Platform_SetImeDataFn(ImGui::GetCurrentContext(), viewport, &ime_data);
        }
    }
}

// 供 WM_IME_STARTCOMPOSITION hook 读取上一帧保存的坐标。
// 返回的数据是最近一次 RenderTextInputIME 调用时写入的。
// 若 backend 未初始化，各输出参数置零（调用方可安全使用）。
void ImGui_ImplSDL2_GetTextIMESavedPos(ImVec2* out_pos, float* out_line_height, float* out_widget_right)
{
    ImGui_ImplSDL2_Data* bd = ImGui_ImplSDL2_GetBackendData();
    if (!bd) {
        if (out_pos) *out_pos = ImVec2(0, 0);
        if (out_line_height) *out_line_height = 0.0f;
        if (out_widget_right) *out_widget_right = 0.0f;
        return;
    }
    if (out_pos) *out_pos = bd->TextIME.SavedCursorScreenPos;
    if (out_line_height) *out_line_height = bd->TextIME.SavedLineHeight;
    if (out_widget_right) *out_widget_right = bd->TextIME.SavedWidgetRight;
}

// Forward Declarations
static void ImGui_ImplSDL2_UpdateMonitors();
static void ImGui_ImplSDL2_InitMultiViewportSupport(SDL_Window* window, void* sdl_gl_context);
static void ImGui_ImplSDL2_ShutdownMultiViewportSupport();

// Functions
static const char* ImGui_ImplSDL2_GetClipboardText(ImGuiContext*)
{
    ImGui_ImplSDL2_Data* bd = ImGui_ImplSDL2_GetBackendData();
    if (bd->ClipboardTextData)
        SDL_free(bd->ClipboardTextData);
    if (SDL_HasClipboardText())
        bd->ClipboardTextData = SDL_GetClipboardText();
    else
        bd->ClipboardTextData = nullptr;
    return bd->ClipboardTextData;
}

static void ImGui_ImplSDL2_SetClipboardText(ImGuiContext*, const char* text)
{
    SDL_SetClipboardText(text);
}

// Note: native IME will only display if user calls SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1") _before_ SDL_CreateWindow().
// Note: native IME will only display if user calls SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1") _before_ SDL_CreateWindow().
static void ImGui_ImplSDL2_PlatformSetImeData(ImGuiContext*, ImGuiViewport* viewport, ImGuiPlatformImeData* data)
{
    if (data->WantVisible)
    {
        SDL_Rect r;
        r.x = (int)(data->InputPos.x - viewport->Pos.x);
        r.y = (int)(data->InputPos.y - viewport->Pos.y);
        r.w = 100;
        r.h = (int)data->InputLineHeight;
        {
            FILE* fp = fopen("ime_trace.txt", "a");
            if (fp) {
                fprintf(fp, "[BackendPlatform] InputPos=(%.2f,%.2f) viewport->Pos=(%.2f,%.2f) => r=(%d,%d,%d,%d)\n",
                        data->InputPos.x, data->InputPos.y, viewport->Pos.x, viewport->Pos.y,
                        r.x, r.y, r.w, r.h);
                fclose(fp);
            }
        }
        SDL_SetTextInputRect(&r);
    }
}

// Not static to allow third-party code to use that if they want to (but undocumented)
ImGuiKey ImGui_ImplSDL2_KeyEventToImGuiKey(SDL_Keycode keycode, SDL_Scancode scancode);
ImGuiKey ImGui_ImplSDL2_KeyEventToImGuiKey(SDL_Keycode keycode, SDL_Scancode scancode)
{
    switch (keycode)
    {
        case SDLK_TAB: return ImGuiKey_Tab;
        case SDLK_LEFT: return ImGuiKey_LeftArrow;
        case SDLK_RIGHT: return ImGuiKey_RightArrow;
        case SDLK_UP: return ImGuiKey_UpArrow;
        case SDLK_DOWN: return ImGuiKey_DownArrow;
        case SDLK_PAGEUP: return ImGuiKey_PageUp;
        case SDLK_PAGEDOWN: return ImGuiKey_PageDown;
        case SDLK_HOME: return ImGuiKey_Home;
        case SDLK_END: return ImGuiKey_End;
        case SDLK_INSERT: return ImGuiKey_Insert;
        case SDLK_DELETE: return ImGuiKey_Delete;
        case SDLK_BACKSPACE: return ImGuiKey_Backspace;
        case SDLK_SPACE: return ImGuiKey_Space;
        case SDLK_RETURN: return ImGuiKey_Enter;
        case SDLK_ESCAPE: return ImGuiKey_Escape;
        //case SDLK_QUOTE: return ImGuiKey_Apostrophe;
        case SDLK_COMMA: return ImGuiKey_Comma;
        //case SDLK_MINUS: return ImGuiKey_Minus;
        case SDLK_PERIOD: return ImGuiKey_Period;
        //case SDLK_SLASH: return ImGuiKey_Slash;
        case SDLK_SEMICOLON: return ImGuiKey_Semicolon;
        //case SDLK_EQUALS: return ImGuiKey_Equal;
        //case SDLK_LEFTBRACKET: return ImGuiKey_LeftBracket;
        //case SDLK_BACKSLASH: return ImGuiKey_Backslash;
        //case SDLK_RIGHTBRACKET: return ImGuiKey_RightBracket;
        //case SDLK_BACKQUOTE: return ImGuiKey_GraveAccent;
        case SDLK_CAPSLOCK: return ImGuiKey_CapsLock;
        case SDLK_SCROLLLOCK: return ImGuiKey_ScrollLock;
        case SDLK_NUMLOCKCLEAR: return ImGuiKey_NumLock;
        case SDLK_PRINTSCREEN: return ImGuiKey_PrintScreen;
        case SDLK_PAUSE: return ImGuiKey_Pause;
        case SDLK_KP_0: return ImGuiKey_Keypad0;
        case SDLK_KP_1: return ImGuiKey_Keypad1;
        case SDLK_KP_2: return ImGuiKey_Keypad2;
        case SDLK_KP_3: return ImGuiKey_Keypad3;
        case SDLK_KP_4: return ImGuiKey_Keypad4;
        case SDLK_KP_5: return ImGuiKey_Keypad5;
        case SDLK_KP_6: return ImGuiKey_Keypad6;
        case SDLK_KP_7: return ImGuiKey_Keypad7;
        case SDLK_KP_8: return ImGuiKey_Keypad8;
        case SDLK_KP_9: return ImGuiKey_Keypad9;
        case SDLK_KP_PERIOD: return ImGuiKey_KeypadDecimal;
        case SDLK_KP_DIVIDE: return ImGuiKey_KeypadDivide;
        case SDLK_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
        case SDLK_KP_MINUS: return ImGuiKey_KeypadSubtract;
        case SDLK_KP_PLUS: return ImGuiKey_KeypadAdd;
        case SDLK_KP_ENTER: return ImGuiKey_KeypadEnter;
        case SDLK_KP_EQUALS: return ImGuiKey_KeypadEqual;
        case SDLK_LCTRL: return ImGuiKey_LeftCtrl;
        case SDLK_LSHIFT: return ImGuiKey_LeftShift;
        case SDLK_LALT: return ImGuiKey_LeftAlt;
        case SDLK_LGUI: return ImGuiKey_LeftSuper;
        case SDLK_RCTRL: return ImGuiKey_RightCtrl;
        case SDLK_RSHIFT: return ImGuiKey_RightShift;
        case SDLK_RALT: return ImGuiKey_RightAlt;
        case SDLK_RGUI: return ImGuiKey_RightSuper;
        case SDLK_APPLICATION: return ImGuiKey_Menu;
        case SDLK_0: return ImGuiKey_0;
        case SDLK_1: return ImGuiKey_1;
        case SDLK_2: return ImGuiKey_2;
        case SDLK_3: return ImGuiKey_3;
        case SDLK_4: return ImGuiKey_4;
        case SDLK_5: return ImGuiKey_5;
        case SDLK_6: return ImGuiKey_6;
        case SDLK_7: return ImGuiKey_7;
        case SDLK_8: return ImGuiKey_8;
        case SDLK_9: return ImGuiKey_9;
        case SDLK_a: return ImGuiKey_A;
        case SDLK_b: return ImGuiKey_B;
        case SDLK_c: return ImGuiKey_C;
        case SDLK_d: return ImGuiKey_D;
        case SDLK_e: return ImGuiKey_E;
        case SDLK_f: return ImGuiKey_F;
        case SDLK_g: return ImGuiKey_G;
        case SDLK_h: return ImGuiKey_H;
        case SDLK_i: return ImGuiKey_I;
        case SDLK_j: return ImGuiKey_J;
        case SDLK_k: return ImGuiKey_K;
        case SDLK_l: return ImGuiKey_L;
        case SDLK_m: return ImGuiKey_M;
        case SDLK_n: return ImGuiKey_N;
        case SDLK_o: return ImGuiKey_O;
        case SDLK_p: return ImGuiKey_P;
        case SDLK_q: return ImGuiKey_Q;
        case SDLK_r: return ImGuiKey_R;
        case SDLK_s: return ImGuiKey_S;
        case SDLK_t: return ImGuiKey_T;
        case SDLK_u: return ImGuiKey_U;
        case SDLK_v: return ImGuiKey_V;
        case SDLK_w: return ImGuiKey_W;
        case SDLK_x: return ImGuiKey_X;
        case SDLK_y: return ImGuiKey_Y;
        case SDLK_z: return ImGuiKey_Z;
        case SDLK_F1: return ImGuiKey_F1;
        case SDLK_F2: return ImGuiKey_F2;
        case SDLK_F3: return ImGuiKey_F3;
        case SDLK_F4: return ImGuiKey_F4;
        case SDLK_F5: return ImGuiKey_F5;
        case SDLK_F6: return ImGuiKey_F6;
        case SDLK_F7: return ImGuiKey_F7;
        case SDLK_F8: return ImGuiKey_F8;
        case SDLK_F9: return ImGuiKey_F9;
        case SDLK_F10: return ImGuiKey_F10;
        case SDLK_F11: return ImGuiKey_F11;
        case SDLK_F12: return ImGuiKey_F12;
        case SDLK_F13: return ImGuiKey_F13;
        case SDLK_F14: return ImGuiKey_F14;
        case SDLK_F15: return ImGuiKey_F15;
        case SDLK_F16: return ImGuiKey_F16;
        case SDLK_F17: return ImGuiKey_F17;
        case SDLK_F18: return ImGuiKey_F18;
        case SDLK_F19: return ImGuiKey_F19;
        case SDLK_F20: return ImGuiKey_F20;
        case SDLK_F21: return ImGuiKey_F21;
        case SDLK_F22: return ImGuiKey_F22;
        case SDLK_F23: return ImGuiKey_F23;
        case SDLK_F24: return ImGuiKey_F24;
        case SDLK_AC_BACK: return ImGuiKey_AppBack;
        case SDLK_AC_FORWARD: return ImGuiKey_AppForward;
        default: break;
    }

    // Fallback to scancode
    switch (scancode)
    {
    case SDL_SCANCODE_GRAVE: return ImGuiKey_GraveAccent;
    case SDL_SCANCODE_MINUS: return ImGuiKey_Minus;
    case SDL_SCANCODE_EQUALS: return ImGuiKey_Equal;
    case SDL_SCANCODE_LEFTBRACKET: return ImGuiKey_LeftBracket;
    case SDL_SCANCODE_RIGHTBRACKET: return ImGuiKey_RightBracket;
    case SDL_SCANCODE_NONUSBACKSLASH: return ImGuiKey_Oem102;
    case SDL_SCANCODE_BACKSLASH: return ImGuiKey_Backslash;
    case SDL_SCANCODE_SEMICOLON: return ImGuiKey_Semicolon;
    case SDL_SCANCODE_APOSTROPHE: return ImGuiKey_Apostrophe;
    case SDL_SCANCODE_COMMA: return ImGuiKey_Comma;
    case SDL_SCANCODE_PERIOD: return ImGuiKey_Period;
    case SDL_SCANCODE_SLASH: return ImGuiKey_Slash;
    default: break;
    }
    return ImGuiKey_None;
}

static void ImGui_ImplSDL2_UpdateKeyModifiers(SDL_Keymod sdl_key_mods)
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiMod_Ctrl, (sdl_key_mods & KMOD_CTRL) != 0);
    io.AddKeyEvent(ImGuiMod_Shift, (sdl_key_mods & KMOD_SHIFT) != 0);
    io.AddKeyEvent(ImGuiMod_Alt, (sdl_key_mods & KMOD_ALT) != 0);
    io.AddKeyEvent(ImGuiMod_Super, (sdl_key_mods & KMOD_GUI) != 0);
}

static ImGuiViewport* ImGui_ImplSDL2_GetViewportForWindowID(Uint32 window_id)
{
    return ImGui::FindViewportByPlatformHandle((void*)(intptr_t)window_id);
}

// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
bool ImGui_ImplSDL2_ProcessEvent(const SDL_Event* event)
{
    ImGui_ImplSDL2_Data* bd = ImGui_ImplSDL2_GetBackendData();
    IM_ASSERT(bd != nullptr && "Context or backend not initialized! Did you call ImGui_ImplSDL2_Init()?");
    ImGuiIO& io = ImGui::GetIO();

    switch (event->type)
    {
        case SDL_MOUSEMOTION:
        {
            if (ImGui_ImplSDL2_GetViewportForWindowID(event->motion.windowID) == nullptr)
                return false;
            ImVec2 mouse_pos((float)event->motion.x, (float)event->motion.y);
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            {
                int window_x, window_y;
                SDL_GetWindowPosition(SDL_GetWindowFromID(event->motion.windowID), &window_x, &window_y);
                mouse_pos.x += window_x;
                mouse_pos.y += window_y;
            }
            io.AddMouseSourceEvent(event->motion.which == SDL_TOUCH_MOUSEID ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse);
            io.AddMousePosEvent(mouse_pos.x, mouse_pos.y);
            return true;
        }
        case SDL_MOUSEWHEEL:
        {
            if (ImGui_ImplSDL2_GetViewportForWindowID(event->wheel.windowID) == nullptr)
                return false;
            //IMGUI_DEBUG_LOG("wheel %.2f %.2f, precise %.2f %.2f\n", (float)event->wheel.x, (float)event->wheel.y, event->wheel.preciseX, event->wheel.preciseY);
#if SDL_VERSION_ATLEAST(2,0,18) // If this fails to compile on Emscripten: update to latest Emscripten!
            float wheel_x = -event->wheel.preciseX;
            float wheel_y = event->wheel.preciseY;
#else
            float wheel_x = -(float)event->wheel.x;
            float wheel_y = (float)event->wheel.y;
#endif
#if defined(__EMSCRIPTEN__) && !SDL_VERSION_ATLEAST(2,31,0)
            wheel_x /= 100.0f;
#endif
            io.AddMouseSourceEvent(event->wheel.which == SDL_TOUCH_MOUSEID ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse);
            io.AddMouseWheelEvent(wheel_x, wheel_y);
            return true;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        {
            if (ImGui_ImplSDL2_GetViewportForWindowID(event->button.windowID) == nullptr)
                return false;
            int mouse_button = -1;
            if (event->button.button == SDL_BUTTON_LEFT) { mouse_button = 0; }
            if (event->button.button == SDL_BUTTON_RIGHT) { mouse_button = 1; }
            if (event->button.button == SDL_BUTTON_MIDDLE) { mouse_button = 2; }
            if (event->button.button == SDL_BUTTON_X1) { mouse_button = 3; }
            if (event->button.button == SDL_BUTTON_X2) { mouse_button = 4; }
            if (mouse_button == -1)
                break;
            io.AddMouseSourceEvent(event->button.which == SDL_TOUCH_MOUSEID ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse);
            io.AddMouseButtonEvent(mouse_button, (event->type == SDL_MOUSEBUTTONDOWN));
            bd->MouseButtonsDown = (event->type == SDL_MOUSEBUTTONDOWN) ? (bd->MouseButtonsDown | (1 << mouse_button)) : (bd->MouseButtonsDown & ~(1 << mouse_button));
            return true;
        }
        case SDL_TEXTINPUT:
        {
            if (ImGui_ImplSDL2_GetViewportForWindowID(event->text.windowID) == nullptr)
                return false;
            if (g_SDLIMEWasCancelled) {
                // 输入已被上层取消，忽略此次确认（拼音保留为普通文本）
                g_SDLIMEWasCancelled = false;
                g_SDLIMEComposing = false;
            } else if (g_SDLIMEComposing) {
                // 有活跃的 composition，不直接输入；交由上层删除拼音并插入汉字
                g_SDLIMECommitted = true;
                g_SDLIMEComposing = false;
                strncpy(g_SDLCommittedText, event->text.text, sizeof(g_SDLCommittedText) - 1);
                g_SDLCommittedText[sizeof(g_SDLCommittedText) - 1] = '\0';
            } else {
                io.AddInputCharactersUTF8(event->text.text);
            }
            // 确认输入后清空 composition string
            g_SDLCompositionText[0] = '\0';
            g_SDLCompositionCursor = 0;
            g_SDLCompositionLength = 0;
            return true;
        }
        case SDL_TEXTEDITING:
        {
            if (ImGui_ImplSDL2_GetViewportForWindowID(event->edit.windowID) == nullptr)
                return false;
            // 将 composition string（拼音）暴露给外部使用
            extern char g_SDLCompositionText[32];
            extern int  g_SDLCompositionCursor;
            extern int  g_SDLCompositionLength;
            strncpy(g_SDLCompositionText, event->edit.text, sizeof(g_SDLCompositionText) - 1);
            g_SDLCompositionText[sizeof(g_SDLCompositionText) - 1] = '\0';
            g_SDLCompositionCursor = event->edit.start;
            g_SDLCompositionLength = event->edit.length;
            if (g_SDLCompositionText[0] != '\0')
                g_SDLIMEComposing = true;
            g_SDLIMECommitted = false;
            g_SDLIMEWasCancelled = false;
            // Windows 下 SDL_TEXTEDITING 经常拿不到拼音，走 IMM32 API 后备
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
            if (g_SDLCompositionText[0] == '\0')
                ImGui_ImplSDL2_FetchIMMCompositionString();
#endif
            return true;
        }
#if SDL_VERSION_ATLEAST(2,0,22)
        case SDL_TEXTEDITING_EXT:
        {
            if (ImGui_ImplSDL2_GetViewportForWindowID(event->editExt.windowID) == nullptr)
                return false;
            extern char g_SDLCompositionText[32];
            extern int  g_SDLCompositionCursor;
            extern int  g_SDLCompositionLength;
            strncpy(g_SDLCompositionText, event->editExt.text, sizeof(g_SDLCompositionText) - 1);
            g_SDLCompositionText[sizeof(g_SDLCompositionText) - 1] = '\0';
            g_SDLCompositionCursor = event->editExt.start;
            g_SDLCompositionLength = event->editExt.length;
            if (g_SDLCompositionText[0] != '\0')
                g_SDLIMEComposing = true;
            g_SDLIMECommitted = false;
            g_SDLIMEWasCancelled = false;
            SDL_free(event->editExt.text);
            // Windows 下如果 text 仍为空，走 IMM32 API 后备
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
            if (g_SDLCompositionText[0] == '\0')
                ImGui_ImplSDL2_FetchIMMCompositionString();
#endif
            return true;
        }
#endif
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        {
            ImGuiViewport* viewport = ImGui_ImplSDL2_GetViewportForWindowID(event->key.windowID);
            if (viewport == nullptr)
                return false;
            //IMGUI_DEBUG_LOG("SDL_KEY%s : key=0x%08X ('%s'), scancode=%d ('%s'), mod=%X, windowID=%d, viewport=%08X\n",
            //    (event->type == SDL_KEYDOWN) ? "DOWN" : "UP  ", event->key.keysym.sym, SDL_GetKeyName(event->key.keysym.sym), event->key.keysym.scancode, SDL_GetScancodeName(event->key.keysym.scancode), event->key.keysym.mod, event->key.windowID, viewport ? viewport->ID : 0);
            ImGui_ImplSDL2_UpdateKeyModifiers((SDL_Keymod)event->key.keysym.mod);
            ImGuiKey key = ImGui_ImplSDL2_KeyEventToImGuiKey(event->key.keysym.sym, event->key.keysym.scancode);
            io.AddKeyEvent(key, (event->type == SDL_KEYDOWN));
            io.SetKeyEventNativeData(key, (int)event->key.keysym.sym, (int)event->key.keysym.scancode, (int)event->key.keysym.scancode); // To support legacy indexing (<1.87 user code). Legacy backend uses SDLK_*** as indices to IsKeyXXX() functions.
            return true;
        }
#if SDL_HAS_DISPLAY_EVENT
        case SDL_DISPLAYEVENT:
        {
            // 2.0.26 has SDL_DISPLAYEVENT_CONNECTED/SDL_DISPLAYEVENT_DISCONNECTED/SDL_DISPLAYEVENT_ORIENTATION,
            // so change of DPI/Scaling are not reflected in this event. (SDL3 has it)
            bd->WantUpdateMonitors = true;
            return true;
        }
#endif
        case SDL_WINDOWEVENT:
        {
            ImGuiViewport* viewport = ImGui_ImplSDL2_GetViewportForWindowID(event->window.windowID);
            if (viewport == NULL)
                return false;

            // - When capturing mouse, SDL will send a bunch of conflicting LEAVE/ENTER event on every mouse move, but the final ENTER tends to be right.
            // - However we won't get a correct LEAVE event for a captured window.
            // - In some cases, when detaching a window from main viewport SDL may send SDL_WINDOWEVENT_ENTER one frame too late,
            //   causing SDL_WINDOWEVENT_LEAVE on previous frame to interrupt drag operation by clear mouse position. This is why
            //   we delay process the SDL_WINDOWEVENT_LEAVE events by one frame. See issue #5012 for details.
            Uint8 window_event = event->window.event;
            if (window_event == SDL_WINDOWEVENT_ENTER)
            {
                bd->MouseWindowID = event->window.windowID;
                bd->MouseLastLeaveFrame = 0;
            }
            if (window_event == SDL_WINDOWEVENT_LEAVE)
                bd->MouseLastLeaveFrame = ImGui::GetFrameCount() + 1;
            //if (window_event == SDL_WINDOWEVENT_FOCUS_GAINED) { IMGUI_DEBUG_LOG("SDL_WINDOWEVENT_FOCUS_GAINED: windowId %d, viewport: %08X\n", event->window.windowID, viewport ? viewport->ID : 0); }
            //if (window_event == SDL_WINDOWEVENT_FOCUS_LOST) { IMGUI_DEBUG_LOG("SDL_WINDOWEVENT_FOCUS_LOST: windowId %d, viewport: %08X\n", event->window.windowID, viewport ? viewport->ID : 0); }
            if (window_event == SDL_WINDOWEVENT_FOCUS_GAINED)
                io.AddFocusEvent(true);
            if (window_event == SDL_WINDOWEVENT_FOCUS_LOST)
                io.AddFocusEvent(false);
            if (window_event == SDL_WINDOWEVENT_CLOSE)
                viewport->PlatformRequestClose = true;
            if (window_event == SDL_WINDOWEVENT_MOVED)
                viewport->PlatformRequestMove = true;
            if (window_event == SDL_WINDOWEVENT_RESIZED)
                viewport->PlatformRequestResize = true;
            return true;
        }
        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
        {
            bd->WantUpdateGamepadsList = true;
            return true;
        }
        default:
            break;
    }
    return false;
}

#ifdef __EMSCRIPTEN__
EM_JS(void, ImGui_ImplSDL2_EmscriptenOpenURL, (char const* url), { url = url ? UTF8ToString(url) : null; if (url) window.open(url, '_blank'); });
#endif

static bool ImGui_ImplSDL2_Init(SDL_Window* window, SDL_Renderer* renderer, void* sdl_gl_context)
{
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendPlatformUserData == nullptr && "Already initialized a platform backend!");
    //SDL_SetHint(SDL_HINT_EVENT_LOGGING, "2");

    // Obtain compiled and runtime versions
    SDL_version ver_compiled;
    SDL_version ver_runtime;
    SDL_VERSION(&ver_compiled);
    SDL_GetVersion(&ver_runtime);

    // Setup backend capabilities flags
    ImGui_ImplSDL2_Data* bd = IM_NEW(ImGui_ImplSDL2_Data)();
    snprintf(bd->BackendPlatformName, sizeof(bd->BackendPlatformName), "imgui_impl_sdl2 (%u.%u.%u, %u.%u.%u)",
        ver_compiled.major, ver_compiled.minor, ver_compiled.patch, ver_runtime.major, ver_runtime.minor, ver_runtime.patch);
    io.BackendPlatformUserData = (void*)bd;
    io.BackendPlatformName = bd->BackendPlatformName;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;           // We can honor GetMouseCursor() values (optional)
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;            // We can honor io.WantSetMousePos requests (optional, rarely used)
    // (ImGuiBackendFlags_PlatformHasViewports may be set just below)

    bd->Window = window;
    bd->WindowID = SDL_GetWindowID(window);
    bd->Renderer = renderer;

    // Check and store if we are on a SDL backend that supports SDL_GetGlobalMouseState() and SDL_CaptureMouse()
    // ("wayland" and "rpi" don't support it, but we chose to use a white-list instead of a black-list)
    bd->MouseCanUseGlobalState = false;
    bd->MouseCaptureMode = ImGui_ImplSDL2_MouseCaptureMode_Disabled;
#if SDL_HAS_CAPTURE_AND_GLOBAL_MOUSE
    const char* sdl_backend = SDL_GetCurrentVideoDriver();
    const char* capture_and_global_state_whitelist[] = { "windows", "cocoa", "x11", "DIVE", "VMAN" };
    for (const char* item : capture_and_global_state_whitelist)
        if (strncmp(sdl_backend, item, strlen(item)) == 0)
        {
            bd->MouseCanUseGlobalState = true;
            bd->MouseCaptureMode = (strcmp(item, "x11") == 0) ? ImGui_ImplSDL2_MouseCaptureMode_EnabledAfterDrag : ImGui_ImplSDL2_MouseCaptureMode_Enabled;
        }
#endif
    if (bd->MouseCanUseGlobalState)
        io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;  // We can create multi-viewports on the Platform side (optional)

    // SDL on Linux/OSX doesn't report events for unfocused windows (see https://github.com/ocornut/imgui/issues/4960)
    // We will use 'MouseCanReportHoveredViewport' to set 'ImGuiBackendFlags_HasMouseHoveredViewport' dynamically each frame.
#ifndef __APPLE__
    bd->MouseCanReportHoveredViewport = bd->MouseCanUseGlobalState;
#else
    bd->MouseCanReportHoveredViewport = false;
#endif

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Platform_SetClipboardTextFn = ImGui_ImplSDL2_SetClipboardText;
    platform_io.Platform_GetClipboardTextFn = ImGui_ImplSDL2_GetClipboardText;
    platform_io.Platform_ClipboardUserData = nullptr;
    platform_io.Platform_SetImeDataFn = ImGui_ImplSDL2_PlatformSetImeData;
#ifdef __EMSCRIPTEN__
    platform_io.Platform_OpenInShellFn = [](ImGuiContext*, const char* url) { ImGui_ImplSDL2_EmscriptenOpenURL(url); return true; };
#elif SDL_HAS_OPEN_URL
    platform_io.Platform_OpenInShellFn = [](ImGuiContext*, const char* url) { return SDL_OpenURL(url) == 0; };
#endif

    // Update monitor a first time during init
    ImGui_ImplSDL2_UpdateMonitors();

    // Gamepad handling
    bd->GamepadMode = ImGui_ImplSDL2_GamepadMode_AutoFirst;
    bd->WantUpdateGamepadsList = true;

    // Load mouse cursors
    bd->MouseCursors[ImGuiMouseCursor_Arrow] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    bd->MouseCursors[ImGuiMouseCursor_TextInput] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
    bd->MouseCursors[ImGuiMouseCursor_ResizeAll] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
    bd->MouseCursors[ImGuiMouseCursor_ResizeNS] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
    bd->MouseCursors[ImGuiMouseCursor_ResizeEW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
    bd->MouseCursors[ImGuiMouseCursor_ResizeNESW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
    bd->MouseCursors[ImGuiMouseCursor_ResizeNWSE] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
    bd->MouseCursors[ImGuiMouseCursor_Hand] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
    bd->MouseCursors[ImGuiMouseCursor_Wait] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT);
    bd->MouseCursors[ImGuiMouseCursor_Progress] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAITARROW);
    bd->MouseCursors[ImGuiMouseCursor_NotAllowed] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO);

    // Set platform dependent data in viewport
    // Our mouse update function expect PlatformHandle to be filled for the main viewport
    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    main_viewport->PlatformHandle = (void*)(intptr_t)bd->WindowID;
    main_viewport->PlatformHandleRaw = nullptr;
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info))
    {
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
        main_viewport->PlatformHandleRaw = (void*)info.info.win.window;
#elif defined(__APPLE__) && defined(SDL_VIDEO_DRIVER_COCOA)
        main_viewport->PlatformHandleRaw = (void*)info.info.cocoa.window;
#endif
    }

    // From 2.0.5: Set SDL hint to receive mouse click events on window focus, otherwise SDL doesn't emit the event.
    // Without this, when clicking to gain focus, our widgets wouldn't activate even though they showed as hovered.
    // (This is unfortunately a global SDL setting, so enabling it might have a side-effect on your application.
    // It is unlikely to make a difference, but if your app absolutely needs to ignore the initial on-focus click:
    // you can ignore SDL_MOUSEBUTTONDOWN events coming right after a SDL_WINDOWEVENT_FOCUS_GAINED)
#ifdef SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
#endif

    // From 2.0.18: Enable native IME.
    // IMPORTANT: This is used at the time of SDL_CreateWindow() so this will only affects secondary windows, if any.
    // For the main window to be affected, your application needs to call this manually before calling SDL_CreateWindow().
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif
    // From 2.0.22: Enable extended text editing events for longer composition text
#ifdef SDL_HINT_IME_SUPPORT_EXTENDED_TEXT
    SDL_SetHint(SDL_HINT_IME_SUPPORT_EXTENDED_TEXT, "1");
#endif

    // From 2.0.22: Disable auto-capture, this is preventing drag and drop across multiple windows (see #5710)
#ifdef SDL_HINT_MOUSE_AUTO_CAPTURE
    SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE, "0");
#endif

    // We need SDL_CaptureMouse(), SDL_GetGlobalMouseState() from SDL 2.0.4+ to support multiple viewports.
    // We left the call to ImGui_ImplSDL2_InitMultiViewportSupport() outside of #ifdef to avoid unused-function warnings.
    if (io.BackendFlags & ImGuiBackendFlags_PlatformHasViewports)
        ImGui_ImplSDL2_InitMultiViewportSupport(window, sdl_gl_context);

#ifdef _WIN32
    SDL_SetWindowsMessageHook(ImGui_ImplSDL2_IMEStartCompositionHook, nullptr);
#endif

    return true;
}

bool ImGui_ImplSDL2_InitForOpenGL(SDL_Window* window, void* sdl_gl_context)
{
    return ImGui_ImplSDL2_Init(window, nullptr, sdl_gl_context);
}

bool ImGui_ImplSDL2_InitForVulkan(SDL_Window* window)
{
#if !SDL_HAS_VULKAN
    IM_ASSERT(0 && "Unsupported");
#endif
    if (!ImGui_ImplSDL2_Init(window, nullptr, nullptr))
        return false;
    ImGui_ImplSDL2_Data* bd = ImGui_ImplSDL2_GetBackendData();
    bd->UseVulkan = true;
    return true;
}

bool ImGui_ImplSDL2_InitForD3D(SDL_Window* window)
{
#if !defined(_WIN32)
    IM_ASSERT(0 && "Unsupported");
#endif
    return ImGui_ImplSDL2_Init(window, nullptr, nullptr);
}

bool ImGui_ImplSDL2_InitForMetal(SDL_Window* window)
{
    return ImGui_ImplSDL2_Init(window, nullptr, nullptr);
}

bool ImGui_ImplSDL2_InitForSDLRenderer(SDL_Window* window, SDL_Renderer* renderer)
{
    return ImGui_ImplSDL2_Init(window, renderer, nullptr);
}

bool ImGui_ImplSDL2_InitForOther(SDL_Window* window)
{
    return ImGui_ImplSDL2_Init(window, nullptr, nullptr);
}

static void ImGui_ImplSDL2_CloseGamepads();

void ImGui_ImplSDL2_Shutdown()
{
    ImGui_ImplSDL2_Data* bd = ImGui_ImplSDL2_GetBackendData();
    IM_ASSERT(bd != nullptr && "No platform backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

    ImGui_ImplSDL2_ShutdownMultiViewportSupport();
    if (bd->ClipboardTextData)
        SDL_free(bd->ClipboardTextData);
    for (ImGuiMouseCursor cursor_n = 0; cursor_n < ImGuiMouseCursor_COUNT; cursor_n++)
        SDL_FreeCursor(bd->MouseCursors[cursor_n]);
    ImGui_ImplSDL2_CloseGamepads();

    io.BackendPlatformName = nullptr;
    io.BackendPlatformUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasSetMousePos | ImGuiBackendFlags_HasGamepad | ImGuiBackendFlags_PlatformHasViewports | ImGuiBackendFlags_HasMouseHoveredViewport);
    platform_io.ClearPlatformHandlers();
    IM_DELETE(bd);
}

void ImGui_ImplSDL2_SetMouseCaptureMode(ImGui_ImplSDL2_MouseCaptureMode mode)
{
    ImGui_ImplSDL2_Data* bd = ImGui_ImplSDL2_GetBackendData();
    if (mode == ImGui_ImplSDL2_MouseCaptureMode_Disabled && bd->MouseCaptureMode != ImGui_ImplSDL2_MouseCaptureMode_Disabled)
        SDL_CaptureMouse(SDL_FALSE);
    bd->MouseCaptureMode = mode;
}

// This code is rather messy because some of the functions we need for full viewport support are not available in SDL < 2.0.4.
static void ImGui_ImplSDL2_UpdateMouseData()
{
    ImGui_ImplSDL2_Data* bd = ImGui_ImplSDL2_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();

    // We forward mouse input when hovered or captured (via SDL_MOUSEMOTION) or when focused (below)
#if SDL_HAS_CAPTURE_AND_GLOBAL_MOUSE
    // - SDL_CaptureMouse() let the OS know e.g. that our drags can extend outside of parent boundaries (we want updated position) and shouldn't trigger other operations outside.
    // - Debuggers under Linux tends to leave captured mouse on break, which may be very inconvenient, so to mitigate the issue on X11 we we wait until mouse has moved to begin capture.
    if (bd->MouseCaptureMode == ImGui_ImplSDL2_MouseCaptureMode_Enabled)
    {
        SDL_CaptureMouse((bd->MouseButtonsDown != 0) ? SDL_TRUE : SDL_FALSE);
    }
    else if (bd->MouseCaptureMode == ImGui_ImplSDL2_MouseCaptureMode_EnabledAfterDrag)
    {
        bool want_capture = false;
        for (int button_n = 0; button_n < ImGuiMouseButton_COUNT && !want_capture; button_n++)
            if (ImGui::IsMouseDragging(button_n, 1.0f))
                want_capture = true;
        SDL_CaptureMouse(want_capture ? SDL_TRUE : SDL_FALSE);
    }

    SDL_Window* focused_window = SDL_GetKeyboardFocus();
    const bool is_app_focused = (focused_window && (bd->Window == focused_window || ImGui_ImplSDL2_GetViewportForWindowID(SDL_GetWindowID(focused_window)) != NULL));
#else
    SDL_Window* focused_window = bd->Window;
    const bool is_app_focused = (SDL_GetWindowFlags(bd->Window) & SDL_WINDOW_INPUT_FOCUS) != 0; // SDL 2.0.3 and non-windowed systems: single-viewport only
#endif

    if (is_app_focused)
    {
        // (Optional) Set OS mouse position from Dear ImGui if requested (rarely used, only when io.ConfigNavMoveSetMousePos is enabled by user)
        if (io.WantSetMousePos)
        {
#if SDL_HAS_CAPTURE_AND_GLOBAL_MOUSE
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
                SDL_WarpMouseGlobal((int)io.MousePos.x, (int)io.MousePos.y);
            else
#endif
                SDL_WarpMouseInWindow(bd->Window, (int)io.MousePos.x, (int)io.MousePos.y);
        }

        // (Optional) Fallback to provide unclamped mouse position when focused but not hovered (SDL_MOUSEMOTION already provides this when hovered or captured)
        // Note that SDL_GetGlobalMouseState() is in theory slow on X11, but this only runs on rather specific cases. If a problem we may provide a way to opt-out this feature.
        SDL_Window* hovered_window = SDL_GetMouseFocus();
        const bool is_relative_mouse_mode = SDL_GetRelativeMouseMode() != 0;
        if (hovered_window == nullptr && bd->MouseCanUseGlobalState && bd->MouseButtonsDown == 0 && !is_relative_mouse_mode)
        {
            // Single-viewport mode: mouse position in client window coordinates (io.MousePos is (0,0) when the mouse is on the upper-left corner of the app window)
            // Multi-viewport mode: mouse position in OS absolute coordinates (io.MousePos is (0,0) when the mouse is on the upper-left of the primary monitor)
            int mouse_x, mouse_y;
            int window_x, window_y;
            SDL_GetGlobalMouseState(&mouse_x, &mouse_y);
            if (!(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
            {
                SDL_GetWindowPosition(focused_window, &window_x, &window_y);
                mouse_x -= window_x;
                mouse_y -= window_y;
            }
            io.AddMousePosEvent((float)mouse_x, (float)mouse_y);
        }
    }

    // (Optional) When using multiple viewports: call io.AddMouseViewportEvent() with the viewport the OS mouse cursor is hovering.
    // If ImGuiBackendFlags_HasMouseHoveredViewport is not set by the backend, Dear imGui will ignore this field and infer the information using its flawed heuristic.
    // - [!] SDL backend does NOT correctly ignore viewports with the _NoInputs flag.
    //       Some backend are not able to handle that correctly. If a backend report an hovered viewport that has the _NoInputs flag (e.g. when dragging a window
    //       for docking, the viewport has the _NoInputs flag in order to allow us to find the viewport under), then Dear ImGui is forced to ignore the value reported
    //       by the backend, and use its flawed heuristic to guess the viewport behind.
    // - [X] SDL backend correctly reports this regardless of another viewport behind focused and dragged from (we need this to find a useful drag and drop target).
    if (io.BackendFlags & ImGuiBackendFlags_HasMouseHoveredViewport)
    {
        ImGuiID mouse_viewport_id = 0;
        if (ImGuiViewport* mouse_viewport = ImGui_ImplSDL2_GetViewportForWindowID(bd->MouseWindowID))
            mouse_viewport_id = mouse_viewport->ID;
        io.AddMouseViewportEvent(mouse_viewport_id);
    }
}

static void ImGui_ImplSDL2_UpdateMouseCursor()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
        return;
    ImGui_ImplSDL2_Data* bd = ImGui_ImplSDL2_GetBackendData();

    ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
    if (io.MouseDrawCursor || imgui_cursor == ImGuiMouseCursor_None)
    {
        // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
        SDL_ShowCursor(SDL_FALSE);
    }
    else
    {
        // Show OS mouse cursor
        SDL_Cursor* expected_cursor = bd->MouseCursors[imgui_cursor] ? bd->MouseCursors[imgui_cursor] : bd->MouseCursors[ImGuiMouseCursor_Arrow];
        if (bd->MouseLastCursor != expected_cursor)
        {
            SDL_SetCursor(expected_cursor); // SDL function doesn't have an early out (see #6113)
            bd->MouseLastCursor = expected_cursor;
        }
        SDL_ShowCursor(SDL_TRUE);
    }
}

// - On Windows the process needs to be marked DPI-aware!! SDL2 doesn't do it by default. You can call ::SetProcessDPIAware() or call ImGui_ImplWin32_EnableDpiAwareness() from Win32 backend.
// - Apple platforms use FramebufferScale so we always return 1.0f.
// - Some accessibility applications are declaring virtual monitors with a DPI of 0.0f, see #7902. We preserve this value for caller to handle.
float ImGui_ImplSDL2_GetContentScaleForWindow(SDL_Window* window)
{
    return ImGui_ImplSDL2_GetContentScaleForDisplay(SDL_GetWindowDisplayIndex(window));
}

// SDL_GetDisplayDPI() seems rather unreliable on Linux.
float ImGui_ImplSDL2_GetContentScaleForDisplay(int display_index)
{
    const char* sdl_driver = SDL_GetCurrentVideoDriver();
    if (sdl_driver && strcmp(sdl_driver, "wayland") == 0)
        return 1.0f;
#if SDL_HAS_PER_MONITOR_DPI
#if !defined(__APPLE__) && !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
    float dpi = 0.0f;
    if (SDL_GetDisplayDPI(display_index, &dpi, nullptr, nullptr) == 0)
    {
        if (dpi < 96.0f)
            dpi = 96.0f;
        return dpi / 96.0f;
    }
#endif
#endif
    IM_UNUSED(display_index);
    return 1.0f;
}

static void ImGui_ImplSDL2_CloseGamepads()
{
    ImGui_ImplSDL2_Data* bd = ImGui_ImplSDL2_GetBackendData();
    if (bd->GamepadMode != ImGui_ImplSDL2_GamepadMode_Manual)
        for (SDL_GameController* gamepad : bd->Gamepads)
            SDL_GameControllerClose(gamepad);
    bd->Gamepads.resize(0);
}

void ImGui_ImplSDL2_SetGamepadMode(ImGui_ImplSDL2_GamepadMode mode, struct _SDL_GameController** manual_gamepads_array, int manual_gamepads_count)
{
    ImGui_ImplSDL2_Data* bd = ImGui_ImplSDL2_GetBackendData();
    ImGui_ImplSDL2_CloseGamepads();
    if (mode == ImGui_ImplSDL2_GamepadMode_Manual)
    {
        IM_ASSERT(manual_gamepads_array != nullptr || manual_gamepads_count <= 0);
        for (int n = 0; n < manual_gamepads_count; n++)
            bd->Gamepads.push_back(manual_gamepads_array[n]);
    }
    else
    {
        IM_ASSERT(manual_gamepads_array == nullptr && manual_gamepads_count <= 0);
        bd->WantUpdateGamepadsList = true;
    }
    bd->GamepadMode = mode;
}

static void ImGui_ImplSDL2_UpdateGamepadButton(ImGui_ImplSDL2_Data* bd, ImGuiIO& io, ImGuiKey key, SDL_GameControllerButton button_no)
{
    bool merged_value = false;
    for (SDL_GameController* gamepad : bd->Gamepads)
        merged_value |= SDL_GameControllerGetButton(gamepad, button_no) != 0;
    io.AddKeyEvent(key, merged_value);
}

static inline float Saturate(float v) { return v < 0.0f ? 0.0f : v  > 1.0f ? 1.0f : v; }
static void ImGui_ImplSDL2_UpdateGamepadAnalog(ImGui_ImplSDL2_Data* bd, ImGuiIO& io, ImGuiKey key, SDL_GameControllerAxis axis_no, float v0, float v1)
{
    float merged_value = 0.0f;
    for (SDL_GameController* gamepad : bd->Gamepads)
    {
        float vn = Saturate((float)(SDL_GameControllerGetAxis(gamepad, axis_no) - v0) / (float)(v1 - v0));
        if (merged_value < vn)
            merged_value = vn;
    }
    io.AddKeyAnalogEvent(key, merged_value > 0.1f, merged_value);
}

static void ImGui_ImplSDL2_UpdateGamepads()
{
    ImGui_ImplSDL2_Data* bd = ImGui_ImplSDL2_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();

    // Update list of controller(s) to use
    if (bd->WantUpdateGamepadsList && bd->GamepadMode != ImGui_ImplSDL2_GamepadMode_Manual)
    {
        ImGui_ImplSDL2_CloseGamepads();
        int joystick_count = SDL_NumJoysticks();
        for (int n = 0; n < joystick_count; n++)
            if (SDL_IsGameController(n))
                if (SDL_GameController* gamepad = SDL_GameControllerOpen(n))
                {
                    bd->Gamepads.push_back(gamepad);
                    if (bd->GamepadMode == ImGui_ImplSDL2_GamepadMode_AutoFirst)
                        break;
                }
        bd->WantUpdateGamepadsList = false;
    }

    io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
    if (bd->Gamepads.Size == 0)
        return;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

    // Update gamepad inputs
    const int thumb_dead_zone = 8000; // SDL_gamecontroller.h suggests using this value.
    ImGui_ImplSDL2_UpdateGamepadButton(bd, io, ImGuiKey_GamepadStart,       SDL_CONTROLLER_BUTTON_START);
    ImGui_ImplSDL2_UpdateGamepadButton(bd, io, ImGuiKey_GamepadBack,        SDL_CONTROLLER_BUTTON_BACK);
    ImGui_ImplSDL2_UpdateGamepadButton(bd, io, ImGuiKey_GamepadFaceLeft,    SDL_CONTROLLER_BUTTON_X);              // Xbox X, PS Square
    ImGui_ImplSDL2_UpdateGamepadButton(bd, io, ImGuiKey_GamepadFaceRight,   SDL_CONTROLLER_BUTTON_B);              // Xbox B, PS Circle
    ImGui_ImplSDL2_UpdateGamepadButton(bd, io, ImGuiKey_GamepadFaceUp,      SDL_CONTROLLER_BUTTON_Y);              // Xbox Y, PS Triangle
    ImGui_ImplSDL2_UpdateGamepadButton(bd, io, ImGuiKey_GamepadFaceDown,    SDL_CONTROLLER_BUTTON_A);              // Xbox A, PS Cross
    ImGui_ImplSDL2_UpdateGamepadButton(bd, io, ImGuiKey_GamepadDpadLeft,    SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    ImGui_ImplSDL2_UpdateGamepadButton(bd, io, ImGuiKey_GamepadDpadRight,   SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    ImGui_ImplSDL2_UpdateGamepadButton(bd, io, ImGuiKey_GamepadDpadUp,      SDL_CONTROLLER_BUTTON_DPAD_UP);
    ImGui_ImplSDL2_UpdateGamepadButton(bd, io, ImGuiKey_GamepadDpadDown,    SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    ImGui_ImplSDL2_UpdateGamepadButton(bd, io, ImGuiKey_GamepadL1,          SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    ImGui_ImplSDL2_UpdateGamepadButton(bd, io, ImGuiKey_GamepadR1,          SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    ImGui_ImplSDL2_UpdateGamepadAnalog(bd, io, ImGuiKey_GamepadL2,          SDL_CONTROLLER_AXIS_TRIGGERLEFT,  0.0f, 32767);
    ImGui_ImplSDL2_UpdateGamepadAnalog(bd, io, ImGuiKey_GamepadR2,          SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 0.0f, 32767);
    ImGui_ImplSDL2_UpdateGamepadButton(bd, io, ImGuiKey_GamepadL3,          SDL_CONTROLLER_BUTTON_LEFTSTICK);
    ImGui_ImplSDL2_UpdateGamepadButton(bd, io, ImGuiKey_GamepadR3,          SDL_CONTROLLER_BUTTON_RIGHTSTICK);
    ImGui_ImplSDL2_UpdateGamepadAnalog(bd, io, ImGuiKey_GamepadLStickLeft,  SDL_CONTROLLER_AXIS_LEFTX,  -thumb_dead_zone, -32768);
    ImGui_ImplSDL2_UpdateGamepadAnalog(bd, io, ImGuiKey_GamepadLStickRight, SDL_CONTROLLER_AXIS_LEFTX,  +thumb_dead_zone, +32767);
    ImGui_ImplSDL2_UpdateGamepadAnalog(bd, io, ImGuiKey_GamepadLStickUp,    SDL_CONTROLLER_AXIS_LEFTY,  -thumb_dead_zone, -32768);
    ImGui_ImplSDL2_UpdateGamepadAnalog(bd, io, ImGuiKey_GamepadLStickDown,  SDL_CONTROLLER_AXIS_LEFTY,  +thumb_dead_zone, +32767);
    ImGui_ImplSDL2_UpdateGamepadAnalog(bd, io, ImGuiKey_GamepadRStickLeft,  SDL_CONTROLLER_AXIS_RIGHTX, -thumb_dead_zone, -32768);
    ImGui_ImplSDL2_UpdateGamepadAnalog(bd, io, ImGuiKey_GamepadRStickRight, SDL_CONTROLLER_AXIS_RIGHTX, +thumb_dead_zone, +32767);
    ImGui_ImplSDL2_UpdateGamepadAnalog(bd, io, ImGuiKey_GamepadRStickUp,    SDL_CONTROLLER_AXIS_RIGHTY, -thumb_dead_zone, -32768);
    ImGui_ImplSDL2_UpdateGamepadAnalog(bd, io, ImGuiKey_GamepadRStickDown,  SDL_CONTROLLER_AXIS_RIGHTY, +thumb_dead_zone, +32767);
}

// FIXME: Note that doesn't update with DPI/Scaling change only as SDL2 doesn't have an event for it (SDL3 has).
static void ImGui_ImplSDL2_UpdateMonitors()
{
    ImGui_ImplSDL2_Data* bd = ImGui_ImplSDL2_GetBackendData();
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Monitors.resize(0);
    bd->WantUpdateMonitors = false;
    int display_count = SDL_GetNumVideoDisplays();
    for (int n = 0; n < display_count; n++)
    {
        // Warning: the validity of monitor DPI information on Windows depends on the application DPI awareness settings, which generally needs to be set in the manifest or at runtime.
        ImGuiPlatformMonitor monitor;
        SDL_Rect r;
        SDL_GetDisplayBounds(n, &r);
        monitor.MainPos = monitor.WorkPos = ImVec2((float)r.x, (float)r.y);
        monitor.MainSize = monitor.WorkSize = ImVec2((float)r.w, (float)r.h);
#if SDL_HAS_USABLE_DISPLAY_BOUNDS
        if (SDL_GetDisplayUsableBounds(n, &r) == 0 && r.w > 0 && r.h > 0)
        {
            monitor.WorkPos = ImVec2((float)r.x, (float)r.y);
            monitor.WorkSize = ImVec2((float)r.w, (float)r.h);
        }
#endif
        float dpi_scale = ImGui_ImplSDL2_GetContentScaleForDisplay(n);
        if (dpi_scale <= 0.0f)
            continue; // Some accessibility applications are declaring virtual monitors with a DPI of 0, see #7902.
        monitor.DpiScale = dpi_scale;
        monitor.PlatformHandle = (void*)(intptr_t)n;
        platform_io.Monitors.push_back(monitor);
    }
}

static void ImGui_ImplSDL2_GetWindowSizeAndFramebufferScale(SDL_Window* window, SDL_Renderer* renderer, ImVec2* out_size, ImVec2* out_framebuffer_scale)
{
    int w, h;
    int display_w, display_h;
    SDL_GetWindowSize(window, &w, &h);
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        w = h = 0;
    if (renderer != nullptr)
        SDL_GetRendererOutputSize(renderer, &display_w, &display_h);
#if SDL_HAS_VULKAN
    else if (SDL_GetWindowFlags(window) & SDL_WINDOW_VULKAN)
        SDL_Vulkan_GetDrawableSize(window, &display_w, &display_h);
#endif
    else
        SDL_GL_GetDrawableSize(window, &display_w, &display_h);
    if (out_size != nullptr)
        *out_size = ImVec2((float)w, (float)h);
    if (out_framebuffer_scale != nullptr)
        *out_framebuffer_scale = (w > 0 && h > 0) ? ImVec2((float)display_w / (float)w, (float)display_h / (float)h) : ImVec2(1.0f, 1.0f);
}


void ImGui_ImplSDL2_NewFrame()
{
    ImGui_ImplSDL2_Data* bd = ImGui_ImplSDL2_GetBackendData();
    IM_ASSERT(bd != nullptr && "Context or backend not initialized! Did you call ImGui_ImplSDL2_Init()?");
    ImGuiIO& io = ImGui::GetIO();

    // Setup main viewport size (every frame to accommodate for window resizing)
    ImGui_ImplSDL2_GetWindowSizeAndFramebufferScale(bd->Window, bd->Renderer, &io.DisplaySize, &io.DisplayFramebufferScale);

    // Update monitors
#ifdef WIN32
    bd->WantUpdateMonitors = true; // Keep polling under Windows to handle changes of work area when resizing task-bar (#8415)
#endif
    if (bd->WantUpdateMonitors)
        ImGui_ImplSDL2_UpdateMonitors();

    // Setup time step (we don't use SDL_GetTicks() because it is using millisecond resolution)
    // (Accept SDL_GetPerformanceCounter() not returning a monotonically increasing value. Happens in VMs and Emscripten, see #6189, #6114, #3644)
    static Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 current_time = SDL_GetPerformanceCounter();
    if (current_time <= bd->Time)
        current_time = bd->Time + 1;
    io.DeltaTime = bd->Time > 0 ? (float)((double)(current_time - bd->Time) / (double)frequency) : (float)(1.0f / 60.0f);
    bd->Time = current_time;

    if (bd->MouseLastLeaveFrame && bd->MouseLastLeaveFrame >= ImGui::GetFrameCount() && bd->MouseButtonsDown == 0)
    {
        bd->MouseWindowID = 0;
        bd->MouseLastLeaveFrame = 0;
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    }

    // Our io.AddMouseViewportEvent() calls will only be valid when not capturing.
    // Technically speaking testing for 'bd->MouseButtonsDown == 0' would be more rigorous, but testing for payload reduces noise and potential side-effects.
    if (bd->MouseCanReportHoveredViewport && ImGui::GetDragDropPayload() == nullptr)
        io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport;
    else
        io.BackendFlags &= ~ImGuiBackendFlags_HasMouseHoveredViewport;

    ImGui_ImplSDL2_UpdateMouseData();
    ImGui_ImplSDL2_UpdateMouseCursor();

    // Update game controllers (if enabled and available)
    ImGui_ImplSDL2_UpdateGamepads();
}

//--------------------------------------------------------------------------------------------------------
// MULTI-VIEWPORT / PLATFORM INTERFACE SUPPORT
// This is an _advanced_ and _optional_ feature, allowing the backend to create and handle multiple viewports simultaneously.
// If you are new to dear imgui or creating a new binding for dear imgui, it is recommended that you completely ignore this section first..
//--------------------------------------------------------------------------------------------------------

// Helper structure we store in the void* PlatformUserData field of each ImGuiViewport to easily retrieve our backend data.
struct ImGui_ImplSDL2_ViewportData
{
    SDL_Window*     Window;
    Uint32          WindowID;       // Stored in ImGuiViewport::PlatformHandle. Use SDL_GetWindowFromID() to get SDL_Window* from Uint32 WindowID.
    bool            WindowOwned;
    SDL_GLContext   GLContext;

    ImGui_ImplSDL2_ViewportData()   { Window = nullptr; WindowID = 0; WindowOwned = false; GLContext = nullptr; }
    ~ImGui_ImplSDL2_ViewportData()  { IM_ASSERT(Window == nullptr && GLContext == nullptr); }
};

static void ImGui_ImplSDL2_CreateWindow(ImGuiViewport* viewport)
{
    ImGui_ImplSDL2_Data* bd = ImGui_ImplSDL2_GetBackendData();
    ImGui_ImplSDL2_ViewportData* vd = IM_NEW(ImGui_ImplSDL2_ViewportData)();
    viewport->PlatformUserData = vd;

    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    ImGui_ImplSDL2_ViewportData* main_viewport_data = (ImGui_ImplSDL2_ViewportData*)main_viewport->PlatformUserData;

    // Share GL resources with main context
    bool use_opengl = (main_viewport_data->GLContext != nullptr);
    SDL_GLContext backup_context = nullptr;
    if (use_opengl)
    {
        backup_context = SDL_GL_GetCurrentContext();
        SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
        SDL_GL_MakeCurrent(main_viewport_data->Window, main_viewport_data->GLContext);
    }

    Uint32 sdl_flags = 0;
    sdl_flags |= use_opengl ? SDL_WINDOW_OPENGL : (bd->UseVulkan ? SDL_WINDOW_VULKAN : 0);
    sdl_flags |= SDL_GetWindowFlags(bd->Window) & SDL_WINDOW_ALLOW_HIGHDPI;
    sdl_flags |= SDL_WINDOW_HIDDEN;
    sdl_flags |= (viewport->Flags & ImGuiViewportFlags_NoDecoration) ? SDL_WINDOW_BORDERLESS : 0;
    sdl_flags |= (viewport->Flags & ImGuiViewportFlags_NoDecoration) ? 0 : SDL_WINDOW_RESIZABLE;
#if !defined(_WIN32)
    // See SDL hack in ImGui_ImplSDL2_ShowWindow().
    sdl_flags |= (viewport->Flags & ImGuiViewportFlags_NoTaskBarIcon) ? SDL_WINDOW_SKIP_TASKBAR : 0;
#endif
#if SDL_HAS_ALWAYS_ON_TOP
    sdl_flags |= (viewport->Flags & ImGuiViewportFlags_TopMost) ? SDL_WINDOW_ALWAYS_ON_TOP : 0;
#endif
    vd->Window = SDL_CreateWindow("No Title Yet", (int)viewport->Pos.x, (int)viewport->Pos.y, (int)viewport->Size.x, (int)viewport->Size.y, sdl_flags);
    vd->WindowOwned = true;
    if (use_opengl)
    {
        vd->GLContext = SDL_GL_CreateContext(vd->Window);
        SDL_GL_SetSwapInterval(0);
    }
    if (use_opengl && backup_context)
        SDL_GL_MakeCurrent(vd->Window, backup_context);

    viewport->PlatformHandle = (void*)(intptr_t)SDL_GetWindowID(vd->Window);
    viewport->PlatformHandleRaw = nullptr;
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(vd->Window, &info))
    {
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
        viewport->PlatformHandleRaw = info.info.win.window;
#elif defined(__APPLE__) && defined(SDL_VIDEO_DRIVER_COCOA)
        viewport->PlatformHandleRaw = (void*)info.info.cocoa.window;
#endif
    }
}

static void ImGui_ImplSDL2_DestroyWindow(ImGuiViewport* viewport)
{
    if (ImGui_ImplSDL2_ViewportData* vd = (ImGui_ImplSDL2_ViewportData*)viewport->PlatformUserData)
    {
        if (vd->GLContext && vd->WindowOwned)
            SDL_GL_DeleteContext(vd->GLContext);
        if (vd->Window && vd->WindowOwned)
            SDL_DestroyWindow(vd->Window);
        vd->GLContext = nullptr;
        vd->Window = nullptr;
        IM_DELETE(vd);
    }
    viewport->PlatformUserData = viewport->PlatformHandle = nullptr;
}

static void ImGui_ImplSDL2_ShowWindow(ImGuiViewport* viewport)
{
    ImGui_ImplSDL2_ViewportData* vd = (ImGui_ImplSDL2_ViewportData*)viewport->PlatformUserData;
#if defined(_WIN32) && !(defined(WINAPI_FAMILY) && ((defined(WINAPI_FAMILY_APP) && WINAPI_FAMILY == WINAPI_FAMILY_APP) || (defined(WINAPI_FAMILY_GAMES) && WINAPI_FAMILY == WINAPI_FAMILY_GAMES)))
    HWND hwnd = (HWND)viewport->PlatformHandleRaw;

    // SDL hack: Hide icon from task bar
    // Note: SDL 2.0.6+ has a SDL_WINDOW_SKIP_TASKBAR flag which is supported under Windows but the way it create the window breaks our seamless transition.
    if (viewport->Flags & ImGuiViewportFlags_NoTaskBarIcon)
    {
        LONG ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);
        ex_style &= ~WS_EX_APPWINDOW;
        ex_style |= WS_EX_TOOLWINDOW;
        ::SetWindowLong(hwnd, GWL_EXSTYLE, ex_style);
    }
#endif

#if SDL_HAS_SHOW_WINDOW_ACTIVATION_HINT
    SDL_SetHint(SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN, (viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing) ? "1" : "0");
#elif defined(_WIN32)
    // SDL hack: SDL always activate/focus windows :/
    if (viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing)
    {
        ::ShowWindow(hwnd, SW_SHOWNA);
        return;
    }
#endif
    SDL_ShowWindow(vd->Window);
}

static ImVec2 ImGui_ImplSDL2_GetWindowPos(ImGuiViewport* viewport)
{
    ImGui_ImplSDL2_ViewportData* vd = (ImGui_ImplSDL2_ViewportData*)viewport->PlatformUserData;
    int x = 0, y = 0;
    SDL_GetWindowPosition(vd->Window, &x, &y);
    return ImVec2((float)x, (float)y);
}

static void ImGui_ImplSDL2_SetWindowPos(ImGuiViewport* viewport, ImVec2 pos)
{
    ImGui_ImplSDL2_ViewportData* vd = (ImGui_ImplSDL2_ViewportData*)viewport->PlatformUserData;
    SDL_SetWindowPosition(vd->Window, (int)pos.x, (int)pos.y);
}

static ImVec2 ImGui_ImplSDL2_GetWindowSize(ImGuiViewport* viewport)
{
    ImGui_ImplSDL2_ViewportData* vd = (ImGui_ImplSDL2_ViewportData*)viewport->PlatformUserData;
    int w = 0, h = 0;
    SDL_GetWindowSize(vd->Window, &w, &h);
    return ImVec2((float)w, (float)h);
}

static void ImGui_ImplSDL2_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
    ImGui_ImplSDL2_ViewportData* vd = (ImGui_ImplSDL2_ViewportData*)viewport->PlatformUserData;
    SDL_SetWindowSize(vd->Window, (int)size.x, (int)size.y);
}

static ImVec2 ImGui_ImplSDL2_GetWindowFramebufferScale(ImGuiViewport* viewport)
{
    // FIXME: SDL_Renderer does not support multi-viewport.
    ImGui_ImplSDL2_ViewportData* vd = (ImGui_ImplSDL2_ViewportData*)viewport->PlatformUserData;
    ImVec2 framebuffer_scale;
    ImGui_ImplSDL2_GetWindowSizeAndFramebufferScale(vd->Window, nullptr, nullptr, &framebuffer_scale);
    return framebuffer_scale;
}

static void ImGui_ImplSDL2_SetWindowTitle(ImGuiViewport* viewport, const char* title)
{
    ImGui_ImplSDL2_ViewportData* vd = (ImGui_ImplSDL2_ViewportData*)viewport->PlatformUserData;
    SDL_SetWindowTitle(vd->Window, title);
}

#if SDL_HAS_WINDOW_ALPHA
static void ImGui_ImplSDL2_SetWindowAlpha(ImGuiViewport* viewport, float alpha)
{
    ImGui_ImplSDL2_ViewportData* vd = (ImGui_ImplSDL2_ViewportData*)viewport->PlatformUserData;
    SDL_SetWindowOpacity(vd->Window, alpha);
}
#endif

static void ImGui_ImplSDL2_SetWindowFocus(ImGuiViewport* viewport)
{
    ImGui_ImplSDL2_ViewportData* vd = (ImGui_ImplSDL2_ViewportData*)viewport->PlatformUserData;
    SDL_RaiseWindow(vd->Window);
}

static bool ImGui_ImplSDL2_GetWindowFocus(ImGuiViewport* viewport)
{
    ImGui_ImplSDL2_ViewportData* vd = (ImGui_ImplSDL2_ViewportData*)viewport->PlatformUserData;
    return (SDL_GetWindowFlags(vd->Window) & SDL_WINDOW_INPUT_FOCUS) != 0;
}

static bool ImGui_ImplSDL2_GetWindowMinimized(ImGuiViewport* viewport)
{
    ImGui_ImplSDL2_ViewportData* vd = (ImGui_ImplSDL2_ViewportData*)viewport->PlatformUserData;
    return (SDL_GetWindowFlags(vd->Window) & SDL_WINDOW_MINIMIZED) != 0;
}

static void ImGui_ImplSDL2_RenderWindow(ImGuiViewport* viewport, void*)
{
    ImGui_ImplSDL2_ViewportData* vd = (ImGui_ImplSDL2_ViewportData*)viewport->PlatformUserData;
    if (vd->GLContext)
        SDL_GL_MakeCurrent(vd->Window, vd->GLContext);
}

static void ImGui_ImplSDL2_SwapBuffers(ImGuiViewport* viewport, void*)
{
    ImGui_ImplSDL2_ViewportData* vd = (ImGui_ImplSDL2_ViewportData*)viewport->PlatformUserData;
    if (vd->GLContext)
    {
        SDL_GL_MakeCurrent(vd->Window, vd->GLContext);
        SDL_GL_SwapWindow(vd->Window);
    }
}

// Vulkan support (the Vulkan renderer needs to call a platform-side support function to create the surface)
// SDL is graceful enough to _not_ need <vulkan/vulkan.h> so we can safely include this.
#if SDL_HAS_VULKAN
#include <SDL_vulkan.h>
static int ImGui_ImplSDL2_CreateVkSurface(ImGuiViewport* viewport, ImU64 vk_instance, const void* vk_allocator, ImU64* out_vk_surface)
{
    ImGui_ImplSDL2_ViewportData* vd = (ImGui_ImplSDL2_ViewportData*)viewport->PlatformUserData;
    (void)vk_allocator;
    SDL_bool ret = SDL_Vulkan_CreateSurface(vd->Window, (VkInstance)vk_instance, (VkSurfaceKHR*)out_vk_surface);
    return ret ? 0 : 1; // ret ? VK_SUCCESS : VK_NOT_READY
}
#endif // SDL_HAS_VULKAN

static void ImGui_ImplSDL2_InitMultiViewportSupport(SDL_Window* window, void* sdl_gl_context)
{
    // Register platform interface (will be coupled with a renderer interface)
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Platform_CreateWindow = ImGui_ImplSDL2_CreateWindow;
    platform_io.Platform_DestroyWindow = ImGui_ImplSDL2_DestroyWindow;
    platform_io.Platform_ShowWindow = ImGui_ImplSDL2_ShowWindow;
    platform_io.Platform_SetWindowPos = ImGui_ImplSDL2_SetWindowPos;
    platform_io.Platform_GetWindowPos = ImGui_ImplSDL2_GetWindowPos;
    platform_io.Platform_SetWindowSize = ImGui_ImplSDL2_SetWindowSize;
    platform_io.Platform_GetWindowSize = ImGui_ImplSDL2_GetWindowSize;
    platform_io.Platform_GetWindowFramebufferScale = ImGui_ImplSDL2_GetWindowFramebufferScale;
    platform_io.Platform_SetWindowFocus = ImGui_ImplSDL2_SetWindowFocus;
    platform_io.Platform_GetWindowFocus = ImGui_ImplSDL2_GetWindowFocus;
    platform_io.Platform_GetWindowMinimized = ImGui_ImplSDL2_GetWindowMinimized;
    platform_io.Platform_SetWindowTitle = ImGui_ImplSDL2_SetWindowTitle;
    platform_io.Platform_RenderWindow = ImGui_ImplSDL2_RenderWindow;
    platform_io.Platform_SwapBuffers = ImGui_ImplSDL2_SwapBuffers;
#if SDL_HAS_WINDOW_ALPHA
    platform_io.Platform_SetWindowAlpha = ImGui_ImplSDL2_SetWindowAlpha;
#endif
#if SDL_HAS_VULKAN
    platform_io.Platform_CreateVkSurface = ImGui_ImplSDL2_CreateVkSurface;
#endif

    // Register main window handle (which is owned by the main application, not by us)
    // This is mostly for simplicity and consistency, so that our code (e.g. mouse handling etc.) can use same logic for main and secondary viewports.
    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    ImGui_ImplSDL2_ViewportData* vd = IM_NEW(ImGui_ImplSDL2_ViewportData)();
    vd->Window = window;
    vd->WindowID = SDL_GetWindowID(window);
    vd->WindowOwned = false;
    vd->GLContext = sdl_gl_context;
    main_viewport->PlatformUserData = vd;
    main_viewport->PlatformHandle = (void*)(intptr_t)vd->WindowID;
}

static void ImGui_ImplSDL2_ShutdownMultiViewportSupport()
{
    ImGui::DestroyPlatformWindows();
}

//-----------------------------------------------------------------------------

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // #ifndef IMGUI_DISABLE
