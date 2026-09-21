#include "scene/world.h"
#include "scene/components/transform.h"
#include "scene/components/sprite.h"
#include "scene/components/name.h"
#include "scene/components/tag.h"
#include "scene/components/event_trigger.h"
#include "scene/components/movement.h"
#include "scene/components/camera.h"
#include "scene/components/camera_follow.h"
#include "scene/components/tilemap.h"
#include "scene/components/animator.h"

namespace mg {
namespace scene {

World::World() = default;
World::~World() = default;

// 从空闲列表复用 ID，或分配新 ID。Entity 0 保留为 kInvalidEntity。
EntityID World::CreateEntity() {
    EntityID id;
    if (!free_entities_.empty()) {
        id = free_entities_.back();
        free_entities_.pop_back();
    } else {
        id = next_entity_id_++;
        if (id >= alive_.size()) {
            alive_.resize(id + 1, false);
        }
    }
    alive_[id] = true;
    ++entity_count_;
    return id;
}

// 销毁实体：移除所有组件、标记死亡、回收 ID 到空闲列表
void World::DestroyEntity(EntityID entity) {
    if (!IsValid(entity)) return;

    // Remove all components
    for (auto& pair : storages_) {
        pair.second->Remove(entity);
    }

    alive_[entity] = false;
    --entity_count_;
    free_entities_.push_back(entity);
}

// 检查实体 ID 是否有效（非 0、在 alive_ 范围内、且处于存活状态）
bool World::IsValid(EntityID entity) const {
    return entity != kInvalidEntity && entity < alive_.size() && alive_[entity];
}

// 遍历所有存活实体并追加到 out（预分配空间优化）
void World::GetAllEntities(std::vector<EntityID>& out) const {
    out.reserve(out.size() + entity_count_);
    for (EntityID i = 1; i < alive_.size(); ++i) {
        if (alive_[i]) {
            out.push_back(i);
        }
    }
}

// 按 TypeIndex 获取组件存储（内部使用，用于 type erasure）
IComponentStorage* World::GetStorageByIndex(TypeIndex index) {
    auto it = storages_.find(index);
    return (it != storages_.end()) ? it->second.get() : nullptr;
}

// const 版本
const IComponentStorage* World::GetStorageByIndex(TypeIndex index) const {
    auto it = storages_.find(index);
    return (it != storages_.end()) ? it->second.get() : nullptr;
}

EntityID World::CloneEntity(EntityID src) {
    if (!IsValid(src)) return kInvalidEntity;
    EntityID dst = CreateEntity();
    if (auto* c = GetComponent<Transform>(src))       AddComponent<Transform>(dst)       = *c;
    if (auto* c = GetComponent<Sprite>(src))          AddComponent<Sprite>(dst)          = *c;
    if (auto* c = GetComponent<Name>(src))            AddComponent<Name>(dst)            = *c;
    if (auto* c = GetComponent<Tag>(src))             AddComponent<Tag>(dst)             = *c;
    if (auto* c = GetComponent<EventTrigger>(src))    AddComponent<EventTrigger>(dst)    = *c;
    if (auto* c = GetComponent<Movement>(src))        AddComponent<Movement>(dst)        = *c;
    if (auto* c = GetComponent<Camera>(src))          AddComponent<Camera>(dst)          = *c;
    if (auto* c = GetComponent<CameraFollowTarget>(src)) AddComponent<CameraFollowTarget>(dst) = *c;
    if (auto* c = GetComponent<Tilemap>(src))         AddComponent<Tilemap>(dst)         = *c;
    if (auto* c = GetComponent<Animator>(src))        AddComponent<Animator>(dst)        = *c;
    return dst;
}

nlohmann::json World::SnapshotEntity(EntityID src) const {
    nlohmann::json j;
    j["components"] = nlohmann::json::object();
    for (const auto& [type_index, storage] : storages_) {
        if (!storage->Has(src)) continue;
        const char* name = ComponentRegistry::Get().GetName(type_index);
        if (std::strcmp(name, "Unknown") == 0) continue;
        j["components"][name] = storage->GetComponentJson(src);
    }
    return j;
}

EntityID World::RestoreEntity(const nlohmann::json& snapshot) {
    EntityID entity = CreateEntity();
    ApplySnapshotComponents(entity, snapshot);
    return entity;
}

bool World::RestoreEntityInto(EntityID dst, const nlohmann::json& snapshot) {
    if (!IsValid(dst)) return false;
    // 清空现有组件（保留实体身份）
    for (auto& [type_index, storage] : storages_) {
        storage->Remove(dst);
    }
    ApplySnapshotComponents(dst, snapshot);
    return true;
}

void World::ApplySnapshotComponents(EntityID entity, const nlohmann::json& snapshot) {
    if (snapshot.contains("components") && snapshot["components"].is_object()) {
        for (auto& [name, comp_json] : snapshot["components"].items()) {
            TypeIndex index = ComponentRegistry::Get().GetIndexByName(name);
            if (index == static_cast<TypeIndex>(-1)) continue;
            IComponentStorage* storage = GetStorageByIndex(index);
            if (storage) storage->SetComponentFromJson(entity, comp_json);
        }
    }
    // Camera primary conflict resolution: ensure only one primary camera exists.
    if (auto* restored_cam = GetComponent<Camera>(entity)) {
        if (restored_cam->is_primary) {
            std::vector<EntityID> all;
            GetAllEntities(all);
            for (auto e : all) {
                if (e == entity) continue;
                if (auto* cam = GetComponent<Camera>(e)) {
                    cam->is_primary = false;
                }
            }
        }
    }
}

} // namespace scene
} // namespace mg
