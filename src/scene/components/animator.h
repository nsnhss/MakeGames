#pragma once
#include "scene/component.h"
#include "third_party/json.hpp"

namespace mg {
namespace scene {

// 轻量帧动画组件：驱动 Sprite 的 UV 偏移
struct Animator : IComponent {
    int frame_count = 1;      // 总帧数
    int columns = 1;          // spritesheet 列数
    float fps = 4.0f;         // 帧率（帧/秒）
    int current_frame = 0;    // 当前帧索引（运行时）
    float timer = 0.0f;       // 帧计时器
    bool playing = true;      // 是否播放中
};

inline void to_json(nlohmann::json& j, const Animator& a) {
    j["frame_count"] = a.frame_count;
    j["columns"] = a.columns;
    j["fps"] = a.fps;
    j["current_frame"] = a.current_frame;
    j["timer"] = a.timer;
    j["playing"] = a.playing;
}
inline void from_json(const nlohmann::json& j, Animator& a) {
    if (j.contains("frame_count")) a.frame_count = j["frame_count"].get<int>();
    if (j.contains("columns")) a.columns = j["columns"].get<int>();
    if (j.contains("fps")) a.fps = j["fps"].get<float>();
    if (j.contains("current_frame")) a.current_frame = j["current_frame"].get<int>();
    if (j.contains("timer")) a.timer = j["timer"].get<float>();
    if (j.contains("playing")) a.playing = j["playing"].get<bool>();
}

} // namespace scene
} // namespace mg

REGISTER_COMPONENT(mg::scene::Animator, "Animator")
