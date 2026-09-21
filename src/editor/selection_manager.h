#pragma once
#include <cstdint>
#include <string>
#include <functional>

namespace mg {
namespace editor {

// 选区管理：实体/资源选择的唯一事实源，变更时触发 OnSelectionChanged 回调。
// 面板通过它实现选择同步（Outliner / Viewport / Scene Explorer 联动）。
class SelectionManager {
public:
    enum class Type { None, Entity, Asset, Multi };

    void SelectEntity(uint32_t entity_id) {
        type_ = Type::Entity;
        selected_entity_ = entity_id;
        selected_asset_path_.clear();
        if (OnSelectionChanged) OnSelectionChanged();
    }

    void SelectAsset(const std::string& path) {
        type_ = Type::Asset;
        selected_asset_path_ = path;
        selected_entity_ = 0;
        if (OnSelectionChanged) OnSelectionChanged();
    }

    void Clear() {
        type_ = Type::None;
        selected_entity_ = 0;
        selected_asset_path_.clear();
        if (OnSelectionChanged) OnSelectionChanged();
    }

    Type GetType() const { return type_; }
    uint32_t GetSelectedEntity() const { return selected_entity_; }
    const std::string& GetSelectedAssetPath() const { return selected_asset_path_; }

    std::function<void()> OnSelectionChanged;

private:
    Type type_ = Type::None;
    uint32_t selected_entity_ = 0;
    std::string selected_asset_path_;
};

} // namespace editor
} // namespace mg
