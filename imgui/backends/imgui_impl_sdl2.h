// dear imgui: Platform Backend for SDL2
// This needs to be used along with a Renderer (e.g. DirectX11, OpenGL3, Vulkan..)
// (Info: SDL2 is a cross-platform general purpose library for handling windows, inputs, graphics context creation, etc.)

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

#pragma once
#include "imgui.h"      // IMGUI_IMPL_API
#ifndef IMGUI_DISABLE

struct SDL_Window;
struct SDL_Renderer;
struct _SDL_GameController;
typedef union SDL_Event SDL_Event;

// Follow "Getting Started" link and check examples/ folder to learn about using backends!
IMGUI_IMPL_API bool     ImGui_ImplSDL2_InitForOpenGL(SDL_Window* window, void* sdl_gl_context);
IMGUI_IMPL_API bool     ImGui_ImplSDL2_InitForVulkan(SDL_Window* window);
IMGUI_IMPL_API bool     ImGui_ImplSDL2_InitForD3D(SDL_Window* window);
IMGUI_IMPL_API bool     ImGui_ImplSDL2_InitForMetal(SDL_Window* window);
IMGUI_IMPL_API bool     ImGui_ImplSDL2_InitForSDLRenderer(SDL_Window* window, SDL_Renderer* renderer);
IMGUI_IMPL_API bool     ImGui_ImplSDL2_InitForOther(SDL_Window* window);
IMGUI_IMPL_API void     ImGui_ImplSDL2_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplSDL2_NewFrame();
IMGUI_IMPL_API bool     ImGui_ImplSDL2_ProcessEvent(const SDL_Event* event);

// DPI-related helpers (optional)
IMGUI_IMPL_API float    ImGui_ImplSDL2_GetContentScaleForWindow(SDL_Window* window);
IMGUI_IMPL_API float    ImGui_ImplSDL2_GetContentScaleForDisplay(int display_index);

// Gamepad selection automatically starts in AutoFirst mode, picking first available SDL_Gamepad. You may override this.
// When using manual mode, caller is responsible for opening/closing gamepad.
enum ImGui_ImplSDL2_GamepadMode { ImGui_ImplSDL2_GamepadMode_AutoFirst, ImGui_ImplSDL2_GamepadMode_AutoAll, ImGui_ImplSDL2_GamepadMode_Manual };
IMGUI_IMPL_API void     ImGui_ImplSDL2_SetGamepadMode(ImGui_ImplSDL2_GamepadMode mode, struct _SDL_GameController** manual_gamepads_array = nullptr, int manual_gamepads_count = -1);

// (Advanced, for X11 users) Override Mouse Capture mode. Mouse capture allows receiving updated mouse position after clicking inside our window and dragging outside it.
// Having this 'Enabled' is in theory always better. But, on X11 if you crash/break to debugger while capture is active you may temporarily lose access to your mouse.
// The best solution is to setup your debugger to automatically release capture, e.g. 'setxkbmap -option grab:break_actions && xdotool key XF86Ungrab' or via a GDB script. See #3650.
// But you may independently decide on X11, when a debugger is attached, to set this value to ImGui_ImplSDL2_MouseCaptureMode_Disabled.
enum ImGui_ImplSDL2_MouseCaptureMode { ImGui_ImplSDL2_MouseCaptureMode_Enabled, ImGui_ImplSDL2_MouseCaptureMode_EnabledAfterDrag, ImGui_ImplSDL2_MouseCaptureMode_Disabled };
IMGUI_IMPL_API void     ImGui_ImplSDL2_SetMouseCaptureMode(ImGui_ImplSDL2_MouseCaptureMode mode);

// ── IME Inline Composition Support (Windows + SDL2) ────────────────────
//
// 这套 API 为 ImGui::InputText / InputTextMultiline 提供中文/日文/韩文等
// 需要 inline composition 的输入法支持。内部通过 SDL_TEXTEDITING / SDL_TEXTINPUT
// 事件驱动，在 Windows 上配合 IMM CFS_EXCLUDE 避免候选框遮挡拼音文本。
//
// 典型用法（2 行）：
//   ImGui::InputTextMultiline("##edit", buf, size, ImVec2(-1,-1),
//       ImGuiInputTextFlags_CallbackAlways, ImGui_ImplSDL2_InputTextCallback);
//   ImGui_ImplSDL2_RenderTextInputIME(buf, ImGui::GetColorU32(accent_color));
//
// 原理：
//   1. InputTextCallback 在 ImGuiInputTextFlags_CallbackAlways 时介入，
//      管理拼音插入、汉字替换、取消恢复、鼠标点击拼音区域归位。
//   2. RenderTextInputIME 在 InputTextMultiline 之后调用，利用 GetItemRectMin
//      等 ImGui API 计算拼音下划线屏幕坐标，并同步设置 IME 候选框位置。
//   3. 内部通过 SDL_SetWindowsMessageHook 拦截 WM_IME_STARTCOMPOSITION，
//      在 IME 重置 CANDIDATEFORM 之前立即重建排除区域，根治首字符候选框错位。
//
// 多 widget 安全：RenderTextInputIME 只在 ImGui::IsItemActive() 为 true 时更新
// 坐标，防止同一 frame 内多个 InputText 竞争状态。

// 用作 ImGui::InputText/InputTextMultiline 的 CallbackAlways 回调。
// 自动处理：拼音插入、汉字替换、取消恢复、鼠标点击拼音区域归位。
IMGUI_IMPL_API int  ImGui_ImplSDL2_InputTextCallback(ImGuiInputTextCallbackData* data);

// 在 InputTextMultiline/InputText 之后调用，绘制拼音下划线并设置 IME 候选框位置。
// buf: 与传给 InputText 相同的 buffer 指针（用于计算坐标）。
// underline_color: 下划线颜色（如 ImGui::GetColorU32(ImGuiCol_CheckMark)）。
IMGUI_IMPL_API void ImGui_ImplSDL2_RenderTextInputIME(const char* buf, ImU32 underline_color);

// 供 WM_IME_STARTCOMPOSITION hook 读取上一帧保存的光标坐标。
// 返回的数据是最近一次 RenderTextInputIME 调用时保存的屏幕坐标和行高。
// 若 backend 未初始化，各输出参数置零。
IMGUI_IMPL_API void ImGui_ImplSDL2_GetTextIMESavedPos(ImVec2* out_pos, float* out_line_height, float* out_widget_right);

#endif // #ifndef IMGUI_DISABLE
