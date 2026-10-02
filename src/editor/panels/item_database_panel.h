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
    GameDatabases& databases_;
    std::function<bool()> on_save_;
    std::string selected_shop_id_;
    std::string selected_actor_id_;   // Actors 标签页成长曲线编辑目标
    int curve_preview_param_ = 0;     // 0=HP 1=MP 2=ATK 3=DEF
};

} // namespace editor
} // namespace mg
