#pragma once
#include "scene/component.h"
#include "third_party/json.hpp"
#include <string>

namespace mg {
namespace scene {

// Prefab 活链接标记：实例 → prefab 名。
// 手动链接语义：无自动传播；Apply/Revert 由编辑器右键菜单显式触发。
// prefab 模板文件本身不存 PrefabRef（SavePrefab 剥离，Instantiate 附加）。
struct PrefabRef : IComponent {
    std::string prefab;
};

inline void to_json(nlohmann::json& j, const PrefabRef& p) {
    j["prefab"] = p.prefab;
}
inline void from_json(const nlohmann::json& j, PrefabRef& p) {
    if (j.contains("prefab")) p.prefab = j["prefab"].get<std::string>();
}

} // namespace scene
} // namespace mg

REGISTER_COMPONENT(mg::scene::PrefabRef, "PrefabRef")
