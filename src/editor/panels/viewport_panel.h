#pragma once
#include "src/editor/panel.h"
#include "src/scene/entity.h"
#include "core/math/vec2.h"
#include "gameui/playmode_ui.h"
#include "imgui.h"
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

// 帧内共享状态：Draw() 组装后传给各编辑模式子块（实现在 viewport_*.cpp）。
// 坐标系约定：相机 Y-up，zoom = ortho_size（半高，世界单位）。
struct ViewportFrameCtx {
    scene::World* world = nullptr;
    ImDrawList* dl = nullptr;
    ImVec2 img_min{}, img_max{};      // letterbox 后的屏幕矩形
    ImVec2 mouse{};
    math::Vec2 cam_pos{};             // 相机导航（中键平移）可修改
    float zoom = 1.0f;                // 滚轮缩放可修改
    float aspect = 1.0f;
    float half_w = 1.0f;              // = zoom * aspect
    float wpp_x = 0.0f, wpp_y = 0.0f; // 屏幕像素 → 世界单位比例
    float mouse_wx = 0.0f, mouse_wy = 0.0f;
    uint32_t vp_w = 0, vp_h = 0;      // 离屏纹理尺寸（仅用于宽高比）
    bool hovered = false;
    bool mouse_valid = false;
};

// 坐标换算/点选助手（定义在 viewport_panel.cpp，供 viewport_*.cpp 共用）
bool MouseToWorld(float mx, float my, ImVec2 img_min, ImVec2 img_max,
                  math::Vec2 cam_pos, float zoom, float vp_w, float vp_h,
                  float* out_wx, float* out_wy);
ImVec2 WorldToScreen(float wx, float wy, ImVec2 img_min, ImVec2 img_max,
                     math::Vec2 cam_pos, float zoom, float vp_w, float vp_h);
float PointToSegmentDist(ImVec2 p, ImVec2 a, ImVec2 b);
// 世界坐标点选：返回半径内最近的非 Tilemap 实体（0 = 未命中）
scene::EntityID PickEntityAt(scene::World* world, float wx, float wy, float radius);

// 中央视口面板
class ViewportPanel : public IPanel {
public:
    explicit ViewportPanel(EditorContext& ctx) : ctx_(ctx) {}
    const char* GetName() const override { return "Viewport"; }
    const char* GetIcon() const override;
    void Draw() override;

private:
    // 编辑模式子块（D4 拆分：实现在 viewport_context_menu/gizmo/tile_editing.cpp）
    void DrawContextMenu(ViewportFrameCtx& f);      // 右键点选 + 上下文菜单 + Delete 键
    void DrawGrid(const ViewportFrameCtx& f);       // 网格（对齐首个 Tilemap 原点）
    void DrawSelectionGizmo(ViewportFrameCtx& f);   // 选中高亮 + Translate Gizmo
    void DrawEntityPlacement(const ViewportFrameCtx& f); // Entity 放置模式
    void DrawTileEditing(ViewportFrameCtx& f);      // Tile/Passage/Terrain 绘制
    void FlushPaintCommands(scene::World* world);   // 帧末：绘制变化推入 Undo 栈

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
