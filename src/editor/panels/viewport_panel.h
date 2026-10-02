#pragma once
#include "src/editor/panel.h"
#include "src/scene/entity.h"
#include "core/math/vec2.h"
#include "gameui/playmode_ui.h"
#include <cstdint>

namespace mg {
namespace scene { class World; }
namespace editor {

class EditorContext;

struct TilePaintPending {
    int layer;
    int x;
    int y;
    int old_tile;
    int new_tile;
};

struct PassagePaintPending {
    int layer;
    int x;
    int y;
    uint8_t old_passage;
    uint8_t new_passage;
};

struct TerrainPaintPending {
    int layer;
    int x;
    int y;
    uint8_t old_terrain;
    uint8_t new_terrain;
};

struct GizmoState {
    enum Mode { Idle, HoverX, HoverY, DragX, DragY };
    Mode mode = Idle;
    mg::math::Vec2 drag_start_pos;
};

// 中央视口面板
class ViewportPanel : public IPanel {
public:
    explicit ViewportPanel(EditorContext& ctx) : ctx_(ctx) {}
    const char* GetName() const override { return "Viewport"; }
    const char* GetIcon() const override;
    void Draw() override;

private:
    EditorContext& ctx_;
    GizmoState gizmo_;
    scene::EntityID rc_target_entity_ = 0;
    bool rc_on_entity_ = false;

    // Tile paint undo 收集
    std::vector<TilePaintPending> pending_tile_changes_;
    scene::World* pending_tile_world_ = nullptr;
    scene::EntityID pending_tilemap_entity_ = 0;

    // Passage paint undo 收集
    std::vector<PassagePaintPending> pending_passage_changes_;
    scene::World* pending_passage_world_ = nullptr;
    scene::EntityID pending_passage_entity_ = 0;

    // Terrain paint undo 收集
    std::vector<TerrainPaintPending> pending_terrain_changes_;
    scene::World* pending_terrain_world_ = nullptr;
    scene::EntityID pending_terrain_entity_ = 0;

    // Play Mode — Pause 子菜单状态（枚举定义在 gameui/playmode_ui.h）
    gameui::PauseSub pause_sub_ = gameui::PauseSub::None;
};

} // namespace editor
} // namespace mg
