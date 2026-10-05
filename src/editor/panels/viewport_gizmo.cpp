// viewport_gizmo.cpp — 视口网格 / 选中高亮 + Translate Gizmo / Entity 放置模式
// （D4 自 viewport_panel.cpp 拆出）。
#include "viewport_panel.h"
#include "src/editor/editor_context.h"
#include "src/editor/command_history.h"
#include "scene/scene.h"
#include "scene/world.h"
#include "scene/components/transform.h"
#include "scene/components/tilemap.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace mg {
namespace editor {

// ---------------------------------------------------------------------------
// Undo 命令：Gizmo 拖拽位移
// ---------------------------------------------------------------------------
class SetTransformCommand : public ICommand {
public:
    SetTransformCommand(scene::World* world, scene::EntityID entity,
                        math::Vec2 old_pos, math::Vec2 new_pos)
        : world_(world), entity_(entity), old_pos_(old_pos), new_pos_(new_pos) {}
    void Execute() override {
        if (world_) if (auto* t = world_->GetComponent<scene::Transform>(entity_)) t->position = new_pos_;
    }
    void Undo() override {
        if (world_) if (auto* t = world_->GetComponent<scene::Transform>(entity_)) t->position = old_pos_;
    }
    const char* GetName() const override { return "Set Transform"; }
private:
    scene::World* world_;
    scene::EntityID entity_;
    math::Vec2 old_pos_;
    math::Vec2 new_pos_;
};

// 网格（对齐到首个 Tilemap 原点，间距 = tile 尺寸）
void ViewportPanel::DrawGrid(const ViewportFrameCtx& f) {
    auto& ctx = ctx_;
    auto* scene = ctx.GetActiveScene();
    auto* world = f.world;
    float grid_step = scene && scene->GetTileSize() > 0
        ? (float)scene->GetTileSize() : 32.0f;
    float grid_ox = 0.0f, grid_oy = 0.0f;
    std::vector<scene::EntityID> tm_entities;
    world->Query<scene::Tilemap>(tm_entities);
    if (!tm_entities.empty()) {
        if (auto* tt = world->GetComponent<scene::Transform>(tm_entities[0])) {
            grid_ox = tt->position.x;
            grid_oy = tt->position.y;
        }
        if (auto* tm = world->GetComponent<scene::Tilemap>(tm_entities[0]))
            grid_step = (float)tm->tile_width;
    }
    if (grid_step > 0.0f) {
        const ImU32 grid_col = IM_COL32(40, 40, 40, 255);
        f.dl->PushClipRect(f.img_min, f.img_max, true);
        float left = f.cam_pos.x - f.half_w, right = f.cam_pos.x + f.half_w;
        float bottom = f.cam_pos.y - f.zoom, top = f.cam_pos.y + f.zoom;
        for (float gx = grid_ox + std::floor((left - grid_ox) / grid_step) * grid_step;
             gx <= right; gx += grid_step) {
            ImVec2 a = WorldToScreen(gx, bottom, f.img_min, f.img_max, f.cam_pos, f.zoom, (float)f.vp_w, (float)f.vp_h);
            ImVec2 b = WorldToScreen(gx, top, f.img_min, f.img_max, f.cam_pos, f.zoom, (float)f.vp_w, (float)f.vp_h);
            f.dl->AddLine(a, b, grid_col);
        }
        for (float gy = grid_oy + std::floor((bottom - grid_oy) / grid_step) * grid_step;
             gy <= top; gy += grid_step) {
            ImVec2 a = WorldToScreen(left, gy, f.img_min, f.img_max, f.cam_pos, f.zoom, (float)f.vp_w, (float)f.vp_h);
            ImVec2 b = WorldToScreen(right, gy, f.img_min, f.img_max, f.cam_pos, f.zoom, (float)f.vp_w, (float)f.vp_h);
            f.dl->AddLine(a, b, grid_col);
        }
        f.dl->PopClipRect();
    }
}

// 选中实体高亮 + Translate Gizmo（X/Y 轴拖拽）
void ViewportPanel::DrawSelectionGizmo(ViewportFrameCtx& f) {
    auto& ctx = ctx_;
    auto* world = f.world;
    using GizmoMode = EditorContext::GizmoMode;
    const GizmoMode gizmo_mode = ctx.GetGizmoMode();

    scene::EntityID sel = (ctx.GetSelectionType() == EditorContext::SelectionType::Entity)
        ? ctx.GetSelectedEntity() : 0;
    auto* t = (sel != 0 && world->IsValid(sel))
        ? world->GetComponent<scene::Transform>(sel) : nullptr;
    if (!t) {
        if (gizmo_.mode != GizmoState::Idle) gizmo_.mode = GizmoState::Idle;
        return;
    }

    ImVec2 c = WorldToScreen(t->position.x, t->position.y,
                             f.img_min, f.img_max, f.cam_pos, f.zoom, (float)f.vp_w, (float)f.vp_h);
    // 高亮框（±16 世界单位，约一格）
    ImVec2 h0 = WorldToScreen(t->position.x - 16.0f, t->position.y - 16.0f,
                              f.img_min, f.img_max, f.cam_pos, f.zoom, (float)f.vp_w, (float)f.vp_h);
    ImVec2 h1 = WorldToScreen(t->position.x + 16.0f, t->position.y + 16.0f,
                              f.img_min, f.img_max, f.cam_pos, f.zoom, (float)f.vp_w, (float)f.vp_h);
    f.dl->AddRect(ImVec2(std::min(h0.x, h1.x), std::min(h0.y, h1.y)),
                  ImVec2(std::max(h0.x, h1.x), std::max(h0.y, h1.y)),
                  IM_COL32(255, 220, 80, 255));

    if (gizmo_mode != GizmoMode::Translate) return;

    constexpr float kAxisLen = 60.0f;
    ImVec2 x_end(c.x + kAxisLen, c.y);
    ImVec2 y_end(c.x, c.y - kAxisLen); // 屏幕向上 = 世界 Y+

    // hover 检测（仅 Idle 且未在拖拽其他控件时）
    if (gizmo_.mode == GizmoState::Idle && f.hovered) {
        if (PointToSegmentDist(f.mouse, c, x_end) < 8.0f)
            gizmo_.mode = GizmoState::HoverX;
        else if (PointToSegmentDist(f.mouse, c, y_end) < 8.0f)
            gizmo_.mode = GizmoState::HoverY;
    } else if ((gizmo_.mode == GizmoState::HoverX ||
                gizmo_.mode == GizmoState::HoverY) && !f.hovered) {
        gizmo_.mode = GizmoState::Idle;
    }

    // 开始拖拽
    if ((gizmo_.mode == GizmoState::HoverX || gizmo_.mode == GizmoState::HoverY) &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        gizmo_.mode = (gizmo_.mode == GizmoState::HoverX)
            ? GizmoState::DragX : GizmoState::DragY;
        gizmo_.drag_start_pos = t->position;
    }

    // 拖拽中：屏幕 delta → 世界 delta（单轴）
    if ((gizmo_.mode == GizmoState::DragX || gizmo_.mode == GizmoState::DragY) &&
        ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2 md = ImGui::GetIO().MouseDelta;
        float wdx = md.x * f.wpp_x;
        float wdy = -md.y * f.wpp_y; // 屏幕 Y 向下 = 世界 Y 向下偏移取反
        if (gizmo_.mode == GizmoState::DragX) t->position.x += wdx;
        else                                  t->position.y += wdy;
        ctx.MarkSceneDirty();
    }

    // 结束拖拽 → 推入 Undo 栈
    if ((gizmo_.mode == GizmoState::DragX || gizmo_.mode == GizmoState::DragY) &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (t->position.x != gizmo_.drag_start_pos.x ||
            t->position.y != gizmo_.drag_start_pos.y) {
            ctx.ExecuteCommand(std::make_unique<SetTransformCommand>(
                world, sel, gizmo_.drag_start_pos, t->position));
        }
        gizmo_.mode = GizmoState::Idle;
    }

    // 绘制轴（悬停/拖拽增亮）
    bool x_hot = (gizmo_.mode == GizmoState::HoverX || gizmo_.mode == GizmoState::DragX);
    bool y_hot = (gizmo_.mode == GizmoState::HoverY || gizmo_.mode == GizmoState::DragY);
    ImU32 x_col = x_hot ? IM_COL32(255, 120, 120, 255) : IM_COL32(220, 60, 60, 255);
    ImU32 y_col = y_hot ? IM_COL32(140, 255, 160, 255) : IM_COL32(60, 200, 90, 255);
    f.dl->AddLine(c, x_end, x_col, 2.0f);
    f.dl->AddLine(c, y_end, y_col, 2.0f);
    f.dl->AddRectFilled(ImVec2(x_end.x - 4, x_end.y - 4),
                        ImVec2(x_end.x + 4, x_end.y + 4), x_col);
    f.dl->AddRectFilled(ImVec2(y_end.x - 4, y_end.y - 4),
                        ImVec2(y_end.x + 4, y_end.y + 4), y_col);
}

// Entity 放置模式：左键在点击处创建实体（支持网格吸附）
void ViewportPanel::DrawEntityPlacement(const ViewportFrameCtx& f) {
    auto& ctx = ctx_;
    auto* scene = ctx.GetActiveScene();
    using GizmoMode = EditorContext::GizmoMode;
    if (ctx.GetGizmoMode() != GizmoMode::Entity || !f.hovered || !f.mouse_valid) return;

    math::Vec2 place_pos(f.mouse_wx, f.mouse_wy);
    float tile = scene && scene->GetTileSize() > 0 ? (float)scene->GetTileSize() : 32.0f;
    if (ctx.IsSnapToGrid()) {
        place_pos.x = (std::floor(place_pos.x / tile) + 0.5f) * tile;
        place_pos.y = (std::floor(place_pos.y / tile) + 0.5f) * tile;
    }
    // 放置预览框
    ImVec2 g0 = WorldToScreen(place_pos.x - tile * 0.5f, place_pos.y - tile * 0.5f,
                              f.img_min, f.img_max, f.cam_pos, f.zoom, (float)f.vp_w, (float)f.vp_h);
    ImVec2 g1 = WorldToScreen(place_pos.x + tile * 0.5f, place_pos.y + tile * 0.5f,
                              f.img_min, f.img_max, f.cam_pos, f.zoom, (float)f.vp_w, (float)f.vp_h);
    f.dl->AddRect(ImVec2(std::min(g0.x, g1.x), std::min(g0.y, g1.y)),
                  ImVec2(std::max(g0.x, g1.x), std::max(g0.y, g1.y)),
                  IM_COL32(120, 200, 255, 220));
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ctx.CreateEntityFromTemplate(EditorContext::EntityTemplate::Empty, place_pos);
    }
}

} // namespace editor
} // namespace mg
