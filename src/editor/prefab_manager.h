#pragma once
// PrefabManager：Prefab（实体 ECS 数据快照）的保存 / 列举 / 实例化 / 手动活链接
// 文件格式：{project}/prefabs/<name>.prefab.json，内容 { "name": ..., "entity": <World::SnapshotEntity JSON> }
// 手动链接：实例携带 PrefabRef 组件标记来源；Apply/Revert 由编辑器显式触发，无自动传播。

#include <string>
#include <vector>
#include "scene/entity.h"
#include "core/math/vec2.h"
#include "third_party/json.hpp"

namespace mg {
namespace scene { class World; }
namespace editor {

class PrefabManager {
public:
    // 保存实体为 prefab；name 规范化（空格→下划线），返回成功/失败
    bool SavePrefab(const std::string& project_path, scene::World* world,
                    scene::EntityID entity, const std::string& name);

    // 扫描 prefabs/*.prefab.json，返回 prefab 名列表（去扩展名）
    std::vector<std::string> ListPrefabs(const std::string& project_path) const;

    // 实例化：读 JSON → World::RestoreEntity → Transform.position 设为 position → 附加 PrefabRef
    // 失败返回 0
    scene::EntityID Instantiate(scene::World* world, const std::string& project_path,
                                const std::string& name, const math::Vec2& position);

    // 把实体当前状态存回其 PrefabRef 指向的 prefab 文件；无 PrefabRef 返回 false
    bool ApplyToPrefab(const std::string& project_path, scene::World* world,
                       scene::EntityID entity);

    // 从 prefab 文件原地还原本实体（ID 不变，保留 Transform.position，PrefabRef 保留）
    bool RevertToPrefab(scene::World* world, const std::string& project_path,
                        scene::EntityID entity);

    // 构建 Revert 的目标快照（不应用）：prefab 内容 + 保留当前 position + 保留 PrefabRef。
    // 供 UpdateEntityCommand 走 undo 栈使用。
    bool BuildRevertSnapshot(scene::World* world, const std::string& project_path,
                             scene::EntityID entity, nlohmann::json& out_snapshot);
};

} // namespace editor
} // namespace mg
