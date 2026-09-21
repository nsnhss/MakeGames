#include "scene/component.h"
#include <cstdio>

namespace mg {
namespace scene {

// Meyer's Singleton：全局唯一的组件注册表
ComponentRegistry& ComponentRegistry::Get() {
    static ComponentRegistry instance;
    return instance;
}

// 注册组件类型：若已注册则返回已有 index，否则分配新 index
TypeIndex ComponentRegistry::Register(const std::type_info& type, const char* name) {
    auto it = type_to_index_.find(type);
    if (it != type_to_index_.end()) {
        return it->second;
    }
    TypeIndex index = next_index_++;
    type_to_index_[type] = index;
    index_to_name_[index] = name;
    name_to_index_[name] = index;
    return index;
}

// 按 type_info 查询 TypeIndex，未注册返回 -1
TypeIndex ComponentRegistry::GetIndex(const std::type_info& type) const {
    auto it = type_to_index_.find(type);
    return (it != type_to_index_.end()) ? it->second : static_cast<TypeIndex>(-1);
}

// 按 TypeIndex 查询友好名称，未注册返回 "Unknown"
const char* ComponentRegistry::GetName(TypeIndex index) const {
    auto it = index_to_name_.find(index);
    return (it != index_to_name_.end()) ? it->second : "Unknown";
}

// 按友好名称查询 TypeIndex，未注册返回 -1
TypeIndex ComponentRegistry::GetIndexByName(const std::string& name) const {
    auto it = name_to_index_.find(name);
    return (it != name_to_index_.end()) ? it->second : static_cast<TypeIndex>(-1);
}

// 当前已注册的组件类型总数
size_t ComponentRegistry::GetTypeCount() const {
    return type_to_index_.size();
}

} // namespace scene
} // namespace mg
