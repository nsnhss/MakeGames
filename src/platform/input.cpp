#include "input.h"
#include <cstring>
#include <utility>

namespace mg {
namespace platform {

const Uint8* Input::keyboard_state_ = nullptr;
Uint8 Input::prev_keyboard_state_[SDL_NUM_SCANCODES] = {};
Uint32 Input::mouse_state_ = 0;
Uint32 Input::prev_mouse_state_ = 0;
std::pair<float, float> Input::mouse_pos_ = {0.0f, 0.0f};
std::pair<float, float> Input::prev_mouse_pos_ = {0.0f, 0.0f};
std::pair<float, float> Input::mouse_delta_ = {0.0f, 0.0f};
float Input::mouse_scroll_ = 0.0f;
int Input::key_count_ = 0;

void Input::Init() {
    keyboard_state_ = SDL_GetKeyboardState(&key_count_);
    std::memset(prev_keyboard_state_, 0, sizeof(prev_keyboard_state_));
    mouse_state_ = SDL_GetMouseState(nullptr, nullptr);
    prev_mouse_state_ = 0;
    mouse_scroll_ = 0.0f;
}

void Input::Shutdown() {
}

void Input::Update() {
    // 保存上一帧键盘状态
    if (key_count_ > 0) {
        int copy_size = key_count_ < SDL_NUM_SCANCODES ? key_count_ : SDL_NUM_SCANCODES;
        std::memcpy(prev_keyboard_state_, keyboard_state_, copy_size);
    }

    // 保存上一帧鼠标状态
    prev_mouse_state_ = mouse_state_;
    prev_mouse_pos_ = mouse_pos_;

    // 获取当前鼠标状态
    int mx, my;
    mouse_state_ = SDL_GetMouseState(&mx, &my);
    mouse_pos_ = {static_cast<float>(mx), static_cast<float>(my)};
    mouse_delta_ = {mouse_pos_.first - prev_mouse_pos_.first,
                    mouse_pos_.second - prev_mouse_pos_.second};

    // 滚动值每帧清零，由事件系统填充
    mouse_scroll_ = 0.0f;
}

bool Input::IsKeyPressed(KeyCode key) {
    SDL_Scancode scancode = SDL_GetScancodeFromKey(static_cast<SDL_Keycode>(key));
    if (scancode >= SDL_NUM_SCANCODES) return false;
    return keyboard_state_[scancode] && !prev_keyboard_state_[scancode];
}

bool Input::IsKeyDown(KeyCode key) {
    SDL_Scancode scancode = SDL_GetScancodeFromKey(static_cast<SDL_Keycode>(key));
    if (scancode >= SDL_NUM_SCANCODES) return false;
    return keyboard_state_[scancode];
}

bool Input::IsKeyUp(KeyCode key) {
    SDL_Scancode scancode = SDL_GetScancodeFromKey(static_cast<SDL_Keycode>(key));
    if (scancode >= SDL_NUM_SCANCODES) return false;
    return !keyboard_state_[scancode] && prev_keyboard_state_[scancode];
}

bool Input::IsMouseButtonPressed(MouseButton button) {
    Uint32 mask = SDL_BUTTON(static_cast<int>(button));
    return (mouse_state_ & mask) && !(prev_mouse_state_ & mask);
}

bool Input::IsMouseButtonDown(MouseButton button) {
    Uint32 mask = SDL_BUTTON(static_cast<int>(button));
    return mouse_state_ & mask;
}

bool Input::IsMouseButtonUp(MouseButton button) {
    Uint32 mask = SDL_BUTTON(static_cast<int>(button));
    return !(mouse_state_ & mask) && (prev_mouse_state_ & mask);
}

std::pair<float, float> Input::GetMousePosition() {
    return mouse_pos_;
}

std::pair<float, float> Input::GetMouseDelta() {
    return mouse_delta_;
}

float Input::GetMouseScroll() {
    return mouse_scroll_;
}

} // namespace platform
} // namespace mg
