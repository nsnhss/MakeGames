#pragma once
#include "scene/entity.h"
#include "scene/component.h"
#include "scene/sparse_set.h"
#include "third_party/json.hpp"
#include <unordered_map>
#include <vector>
#include <memory>
#include <type_traits>

namespace mg {
namespace scene {

// Type-erased component storage interface
class IComponentStorage {
public:
    virtual ~IComponentStorage() = default;
    virtual void Remove(EntityID entity) = 0;
    virtual bool Has(EntityID entity) const = 0;
    virtual size_t Count() const = 0;
    virtual void Clear() = 0;
    virtual TypeIndex GetTypeIndex() const = 0;
    virtual nlohmann::json GetComponentJson(EntityID entity) const = 0;
    virtual void SetComponentFromJson(EntityID entity, const nlohmann::json& j) = 0;
};

// Typed component storage wrapping SparseSet
template<typename T>
class ComponentStorage : public IComponentStorage {
public:
    void Add(EntityID entity, T component) {
        set_.Add(entity, std::move(component));
    }

    T* Get(EntityID entity) {
        return set_.TryGet(entity);
    }

    const T* Get(EntityID entity) const {
        return set_.TryGet(entity);
    }

    void Remove(EntityID entity) override {
        set_.Remove(entity);
    }

    bool Has(EntityID entity) const override {
        return set_.Has(entity);
    }

    size_t Count() const override {
        return set_.Count();
    }

    void Clear() override {
        set_.Clear();
    }

    TypeIndex GetTypeIndex() const override {
        return GetComponentTypeIndex<T>();
    }

    nlohmann::json GetComponentJson(EntityID entity) const override {
        const T* c = Get(entity);
        return c ? nlohmann::json(*c) : nlohmann::json{};
    }

    void SetComponentFromJson(EntityID entity, const nlohmann::json& j) override {
        T c;
        if (j.is_object()) c = j.get<T>();
        Add(entity, std::move(c));
    }

    SparseSet<T>& GetSet() { return set_; }
    const SparseSet<T>& GetSet() const { return set_; }

private:
    SparseSet<T> set_;
};

// ECS World：Entity-Component-System 的核心容器。
// - Entity 是轻量 ID（uint32_t），无数据
// - Component 是纯数据结构，通过 ComponentStorage<T> 存储在 SparseSet 中
// - World 负责 Entity 分配/回收、Component 存储、查询（Query）
// 线程安全：当前单线程使用，ComponentRegistry::Register() 无锁（CodeIssues #10 讨论过）
class World {
public:
    World();
    ~World();

    // Entity 生命周期
    EntityID CreateEntity();          // 分配新 Entity ID（优先复用已销毁的 ID）
    void DestroyEntity(EntityID entity); // 销毁 Entity，回收其所有 Component
    bool IsValid(EntityID entity) const; // 检查 Entity 是否存活
    EntityID CloneEntity(EntityID src); // 克隆实体及其所有组件（纯数据拷贝）

    // 快照：将实体所有组件序列化为 JSON（不含 EntityID，可用于 Prefab / Undo）
    nlohmann::json SnapshotEntity(EntityID src) const;
    // 从快照恢复实体，返回新 EntityID
    EntityID RestoreEntity(const nlohmann::json& snapshot);
    // 原地还原：清空 dst 现有组件后按快照重建（实体 ID 不变），dst 无效返回 false
    bool RestoreEntityInto(EntityID dst, const nlohmann::json& snapshot);

    // Component 操作
    template<typename T, typename... Args>
    T& AddComponent(EntityID entity, Args&&... args) {
        static_assert(std::is_base_of<IComponent, T>::value, "T must derive from IComponent");
        auto* storage = GetOrCreateStorage<T>();
        storage->Add(entity, T{std::forward<Args>(args)...});
        return *storage->Get(entity);
    }

    template<typename T>
    T* GetComponent(EntityID entity) {
        auto* storage = GetStorage<T>();
        return storage ? storage->Get(entity) : nullptr;
    }

    template<typename T>
    const T* GetComponent(EntityID entity) const {
        auto* storage = GetStorage<T>();
        return storage ? storage->Get(entity) : nullptr;
    }

    template<typename T>
    void RemoveComponent(EntityID entity) {
        auto* storage = GetStorage<T>();
        if (storage) storage->Remove(entity);
    }

    template<typename T>
    bool HasComponent(EntityID entity) const {
        auto* storage = GetStorage<T>();
        return storage && storage->Has(entity);
    }

    // 查询所有拥有组件 T 的 Entity（单组件查询，性能最优）
    template<typename T>
    void Query(std::vector<EntityID>& out) const {
        static_assert(std::is_base_of<IComponent, T>::value,
                      "T must be derived from IComponent");
        auto* storage = GetStorage<T>();
        if (!storage) return;
        const auto& entities = storage->GetSet().GetEntities();
        out.insert(out.end(), entities.begin(), entities.end());
    }

    // 查询所有同时拥有 T1, T2, ... 的 Entity（多组件交集查询）
    // 实现：先查 T1 得候选集，再逐个检查是否包含其余组件
    template<typename T1, typename T2, typename... Rest>
    void Query(std::vector<EntityID>& out) const {
        static_assert(std::is_base_of<IComponent, T1>::value &&
                      (std::is_base_of<IComponent, T2>::value && ... && std::is_base_of<IComponent, Rest>::value),
                      "All template parameters must be derived from IComponent");
        std::vector<EntityID> temp;
        Query<T1>(temp);
        for (EntityID e : temp) {
            if (HasAllComponents<T2, Rest...>(e)) {
                out.push_back(e);
            }
        }
    }

    void GetAllEntities(std::vector<EntityID>& out) const;

    // Get storage for a specific component type (for editor inspection)
    IComponentStorage* GetStorageByIndex(TypeIndex index);
    const IComponentStorage* GetStorageByIndex(TypeIndex index) const;

    size_t GetEntityCount() const { return entity_count_; }
    size_t GetComponentTypeCount() const { return storages_.size(); }

private:
    template<typename T>
    ComponentStorage<T>* GetStorage() {
        auto it = storages_.find(GetComponentTypeIndex<T>());
        if (it == storages_.end()) return nullptr;
        return static_cast<ComponentStorage<T>*>(it->second.get());
    }

    template<typename T>
    const ComponentStorage<T>* GetStorage() const {
        auto it = storages_.find(GetComponentTypeIndex<T>());
        if (it == storages_.end()) return nullptr;
        return static_cast<const ComponentStorage<T>*>(it->second.get());
    }

    template<typename T>
    ComponentStorage<T>* GetOrCreateStorage() {
        TypeIndex index = GetComponentTypeIndex<T>();
        auto it = storages_.find(index);
        if (it != storages_.end()) {
            return static_cast<ComponentStorage<T>*>(it->second.get());
        }
        auto storage = std::make_unique<ComponentStorage<T>>();
        auto* ptr = storage.get();
        storages_[index] = std::move(storage);
        return ptr;
    }

    template<typename T>
    bool HasAllComponents(EntityID entity) const {
        static_assert(std::is_base_of<IComponent, T>::value,
                      "T must be derived from IComponent");
        return HasComponent<T>(entity);
    }

    template<typename T1, typename T2, typename... Rest>
    bool HasAllComponents(EntityID entity) const {
        static_assert(std::is_base_of<IComponent, T1>::value,
                      "All template parameters must be derived from IComponent");
        return HasComponent<T1>(entity) && HasAllComponents<T2, Rest...>(entity);
    }

    // 应用快照组件 + Camera primary 冲突解决（RestoreEntity / RestoreEntityInto 共用）
    void ApplySnapshotComponents(EntityID entity, const nlohmann::json& snapshot);

    uint32_t next_entity_id_ = 1;
    std::vector<EntityID> free_entities_;
    std::vector<bool> alive_;
    size_t entity_count_ = 0;

    std::unordered_map<TypeIndex, std::unique_ptr<IComponentStorage>> storages_;
};

} // namespace scene
} // namespace mg
