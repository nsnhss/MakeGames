#pragma once
#include "scene/entity.h"
#include <vector>
#include <cstddef>
#include <cassert>

namespace mg {
namespace scene {

// SparseSet: O(1) 增删查 + 连续内存遍历
// sparse[entity] = dense_index + 1 (0 表示不存在)
// dense[dense_index] = component_data
// entities[dense_index] = entity_id
template<typename T>
class SparseSet {
public:
    bool Has(EntityID entity) const {
        if (entity >= sparse_.size()) return false;
        uint32_t dense_index = sparse_[entity];
        return dense_index != 0 && dense_index <= dense_.size()
            && entities_[dense_index - 1] == entity;
    }

    T& Get(EntityID entity) {
        assert(Has(entity) && "Entity does not have component");
        return dense_[sparse_[entity] - 1];
    }

    const T& Get(EntityID entity) const {
        assert(Has(entity) && "Entity does not have component");
        return dense_[sparse_[entity] - 1];
    }

    T* TryGet(EntityID entity) {
        if (!Has(entity)) return nullptr;
        return &dense_[sparse_[entity] - 1];
    }

    const T* TryGet(EntityID entity) const {
        if (!Has(entity)) return nullptr;
        return &dense_[sparse_[entity] - 1];
    }

    void Add(EntityID entity, T component) {
        assert(!Has(entity) && "Entity already has component");
        if (entity >= sparse_.size()) {
            sparse_.resize(entity + 1, 0);
        }
        sparse_[entity] = static_cast<uint32_t>(dense_.size()) + 1;
        dense_.push_back(std::move(component));
        entities_.push_back(entity);
    }

    void Remove(EntityID entity) {
        if (!Has(entity)) return;
        uint32_t dense_index = sparse_[entity] - 1;
        uint32_t last_index = static_cast<uint32_t>(dense_.size()) - 1;
        EntityID last_entity = entities_[last_index];

        // 用最后一个元素填补空缺，保持 dense 连续
        if (dense_index != last_index) {
            dense_[dense_index] = std::move(dense_[last_index]);
            entities_[dense_index] = last_entity;
            sparse_[last_entity] = dense_index + 1;
        }

        dense_.pop_back();
        entities_.pop_back();
        sparse_[entity] = 0;
    }

    void Clear() {
        sparse_.clear();
        dense_.clear();
        entities_.clear();
    }

    size_t Count() const { return dense_.size(); }

    // 遍历支持
    using iterator = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;

    iterator begin() { return dense_.begin(); }
    iterator end() { return dense_.end(); }
    const_iterator begin() const { return dense_.begin(); }
    const_iterator end() const { return dense_.end(); }

    // 通过 dense index 获取对应 entity
    EntityID GetEntity(size_t dense_index) const {
        assert(dense_index < entities_.size());
        return entities_[dense_index];
    }

    // 获取所有 entity ID（按 dense 顺序）
    const std::vector<EntityID>& GetEntities() const { return entities_; }

private:
    std::vector<uint32_t> sparse_;
    std::vector<T> dense_;
    std::vector<EntityID> entities_;
};

} // namespace scene
} // namespace mg
