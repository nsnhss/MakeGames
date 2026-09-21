#pragma once
#include "scene/entity.h"
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <string>

namespace mg {
namespace scene {

using TypeIndex = uint32_t;

// 空基类，仅用于 type erasure
struct IComponent {};

// 组件类型特征（主模板，可被特化覆盖名称）
template<typename T>
struct ComponentTraits {
    static const char* GetName() { return typeid(T).name(); }
};

// 组件类型注册表
class ComponentRegistry {
public:
    static ComponentRegistry& Get();

    // 注册组件类型，返回全局唯一的 TypeIndex
    // 如果 type 已注册，返回已有 index（name 不会被更新）
    TypeIndex Register(const std::type_info& type, const char* name);

    TypeIndex GetIndex(const std::type_info& type) const;
    const char* GetName(TypeIndex index) const;
    TypeIndex GetIndexByName(const std::string& name) const;

    size_t GetTypeCount() const;

private:
    std::unordered_map<std::type_index, TypeIndex> type_to_index_;
    std::unordered_map<TypeIndex, const char*> index_to_name_;
    std::unordered_map<std::string, TypeIndex> name_to_index_;
    TypeIndex next_index_ = 0;
};

// 获取组件类型对应的 TypeIndex（运行时自动注册）
template<typename T>
TypeIndex GetComponentTypeIndex() {
    static TypeIndex index = ComponentRegistry::Get().Register(typeid(T), ComponentTraits<T>::GetName());
    return index;
}

} // namespace scene
} // namespace mg

// 注册组件并指定友好名称的宏
// 用法：在组件定义的头文件末尾写 REGISTER_COMPONENT(mg::scene::Transform, "Transform")
// 必须在 #include "scene/component.h" 之后使用
#define REGISTER_COMPONENT(T, NameStr) \
    template<> inline const char* mg::scene::ComponentTraits<T>::GetName() { return NameStr; }
