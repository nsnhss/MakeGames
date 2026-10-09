#pragma once
#include "src/editor/panel.h"
#include "resource/game_databases.h"
#include <functional>
#include <string>

namespace mg {
namespace editor {

class ItemDatabasePanel : public IPanel {
public:
    ItemDatabasePanel(GameDatabases& databases, std::function<bool()> on_save)
        : databases_(databases), on_save_(std::move(on_save)) {}
    const char* GetName() const override { return "Item Database"; }
    const char* GetIcon() const override;
    bool IsPopup() const override { return true; }
    void Draw() override;

private:
    // D6-2：删除确认（防误删）——请求时延迟到帧尾模态框执行
    void RequestDelete(const std::string& label, std::function<void()> action);
    void DrawDeleteConfirm();

    GameDatabases& databases_;
    std::function<bool()> on_save_;
    std::string selected_shop_id_;
    std::string selected_actor_id_;   // Actors 标签页成长曲线编辑目标
    int curve_preview_param_ = 0;     // 0=HP 1=MP 2=ATK 3=DEF
    std::string pending_delete_label_;
    std::function<void()> pending_delete_action_;
    bool db_dirty_ = false;           // 有未保存修改（删除后提示，Save 后清除）
};

} // namespace editor
} // namespace mg
