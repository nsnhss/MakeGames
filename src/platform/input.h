#pragma once
#include <SDL.h>
#include <utility>

namespace mg {
namespace platform {

enum class KeyCode {
    Unknown = SDLK_UNKNOWN,
    A = SDLK_a, B = SDLK_b, C = SDLK_c, D = SDLK_d, E = SDLK_e, F = SDLK_f,
    G = SDLK_g, H = SDLK_h, I = SDLK_i, J = SDLK_j, K = SDLK_k, L = SDLK_l,
    M = SDLK_m, N = SDLK_n, O = SDLK_o, P = SDLK_p, Q = SDLK_q, R = SDLK_r,
    S = SDLK_s, T = SDLK_t, U = SDLK_u, V = SDLK_v, W = SDLK_w, X = SDLK_x,
    Y = SDLK_y, Z = SDLK_z,
    Num0 = SDLK_0, Num1 = SDLK_1, Num2 = SDLK_2, Num3 = SDLK_3, Num4 = SDLK_4,
    Num5 = SDLK_5, Num6 = SDLK_6, Num7 = SDLK_7, Num8 = SDLK_8, Num9 = SDLK_9,
    Escape = SDLK_ESCAPE, Space = SDLK_SPACE, Enter = SDLK_RETURN,
    Tab = SDLK_TAB, Backspace = SDLK_BACKSPACE, Delete = SDLK_DELETE,
    Left = SDLK_LEFT, Right = SDLK_RIGHT, Up = SDLK_UP, Down = SDLK_DOWN,
    LShift = SDLK_LSHIFT, RShift = SDLK_RSHIFT,
    LCtrl = SDLK_LCTRL, RCtrl = SDLK_RCTRL,
    LAlt = SDLK_LALT, RAlt = SDLK_RALT,
    F1 = SDLK_F1, F2 = SDLK_F2, F3 = SDLK_F3, F4 = SDLK_F4, F5 = SDLK_F5,
    F6 = SDLK_F6, F7 = SDLK_F7, F8 = SDLK_F8, F9 = SDLK_F9, F10 = SDLK_F10,
    F11 = SDLK_F11, F12 = SDLK_F12
};

enum class MouseButton {
    Left = SDL_BUTTON_LEFT,
    Right = SDL_BUTTON_RIGHT,
    Middle = SDL_BUTTON_MIDDLE
};

class Input {
public:
    static void Init();
    static void Shutdown();
    static void Update();

    static bool IsKeyPressed(KeyCode key);
    static bool IsKeyDown(KeyCode key);
    static bool IsKeyUp(KeyCode key);

    static bool IsMouseButtonPressed(MouseButton button);
    static bool IsMouseButtonDown(MouseButton button);
    static bool IsMouseButtonUp(MouseButton button);

    static std::pair<float, float> GetMousePosition();
    static std::pair<float, float> GetMouseDelta();
    static float GetMouseScroll();

private:
    static const Uint8* keyboard_state_;
    static Uint8 prev_keyboard_state_[SDL_NUM_SCANCODES];
    static Uint32 mouse_state_;
    static Uint32 prev_mouse_state_;
    static std::pair<float, float> mouse_pos_;
    static std::pair<float, float> prev_mouse_pos_;
    static std::pair<float, float> mouse_delta_;
    static float mouse_scroll_;
    static int key_count_;
};

} // namespace platform
} // namespace mg
