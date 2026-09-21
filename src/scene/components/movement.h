#pragma once
#include "scene/component.h"
#include "core/math/vec2.h"
#include "third_party/json.hpp"

namespace mg {
namespace scene {

struct Movement : IComponent {
    math::Vec2 velocity;
    float speed = 200.0f;
    enum class Mode { TopDown, Platformer };
    Mode mode = Mode::TopDown;
};

inline void to_json(nlohmann::json& j, const Movement& m) {
    j["velocity"] = m.velocity;
    j["speed"] = m.speed;
    j["mode"] = static_cast<int>(m.mode);
}
inline void from_json(const nlohmann::json& j, Movement& m) {
    if (j.contains("velocity")) m.velocity = j["velocity"].get<math::Vec2>();
    if (j.contains("speed")) m.speed = j["speed"].get<float>();
    if (j.contains("mode")) m.mode = static_cast<Movement::Mode>(j["mode"].get<int>());
}

} // namespace scene
} // namespace mg

REGISTER_COMPONENT(mg::scene::Movement, "Movement")
