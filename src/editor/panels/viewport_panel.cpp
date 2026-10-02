// viewport_panel.cpp — 中央视口面板。
// 编辑模式：相机导航（滚轮缩放/中键平移）、点选、Gizmo 拖拽、网格、
//           Tile/Passage/Terrain 绘制（帧末 flush 进 Undo 栈）、实体放置、
//           底部浮动工具栏（Gizmo 模式 + ViewMode）。
// Play Mode：标题画面、暂停菜单（存档槽位/物品/状态）、对话框/选项、Toast、
//            Fade 遮罩、商店/战斗、HUD（覆盖层 UI 实现见 gameui/playmode_ui.*，与 game.exe 共用）。
#include "viewport_panel.h"
#include "tile_autotile.h"
#include "src/editor/editor_context.h"
#include "src/editor/imgui/mg_imgui_icons.h"
#include "src/editor/imgui/mg_imgui_extensions.h"
#include "src/editor/imgui/mg_imgui_panel.h"
#include "scene/scene.h"
#include "scene/constants.h"
#include "scene/world.h"
#include "scene/components/transform.h"
#include "scene/components/name.h"
#include "scene/components/sprite.h"
#include "scene/components/tilemap.h"
#include "scene/event_system.h"
#include "resource/actor_database.h"
#include "resource/item_database.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace mg {
namespace editor {

// ---------------------------------------------------------------------------
// 坐标换算：相机为 Y-up 世界坐标，ortho_size（zoom 参数）= 半高（世界单位），
// NDC Y 翻转（见 CameraSystem::ComputeViewProj）。img_min/img_max 为 letterbox
// 后的屏幕矩形，vp_w/vp_h 为离屏纹理尺寸（仅用于宽高比）。
// ---------------------------------------------------------------------------
static bool MouseToWorld(float mx, float my, ImVec2 img_min, ImVec2 img_max,
                         math::Vec2 cam_pos, float zoom, float vp_w, float vp_h,
                         float* out_wx, float* out_wy) {
    float img_w = img_max.x - img_min.x;
    float img_h = img_max.y - img_min.y;
    if (img_w <= 0.0f || img_h <= 0.0f || vp_h <= 0.0f || zoom <= 0.0f) return false;
    float u = (mx - img_min.x) / img_w;   // 0..1
    float v = (my - img_min.y) / img_h;   // 0..1（屏幕向下）
    float aspect = vp_w / vp_h;
    *out_wx = cam_pos.x + (u * 2.0f - 1.0f) * zoom * aspect;
    *out_wy = cam_pos.y - (v * 2.0f - 1.0f) * zoom;
    return true;
}

static ImVec2 WorldToScreen(float wx, float wy, ImVec2 img_min, ImVec2 img_max,
                            math::Vec2 cam_pos, float zoom, float vp_w, float vp_h) {
    float aspect = vp_h > 0.0f ? vp_w / vp_h : 1.0f;
    float half_w = zoom * aspect;
    float u = half_w > 0.0f ? ((wx - cam_pos.x) / half_w + 1.0f) * 0.5f : 0.5f;
    float v = zoom > 0.0f ? (-(wy - cam_pos.y) / zoom + 1.0f) * 0.5f : 0.5f;
    return ImVec2(img_min.x + u * (img_max.x - img_min.x),
                  img_min.y + v * (img_max.y - img_min.y));
}

static float PointToSegmentDist(ImVec2 p, ImVec2 a, ImVec2 b) {
    float abx = b.x - a.x, aby = b.y - a.y;
    float len2 = abx * abx + aby * aby;
    float t = len2 > 0.0f ? ((p.x - a.x) * abx + (p.y - a.y) * aby) / len2 : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    float dx = p.x - (a.x + abx * t);
    float dy = p.y - (a.y + aby * t);
    return std::sqrt(dx * dx + dy * dy);
}

// 世界坐标点选：返回半径内最近的非 Tilemap 实体（0 = 未命中）
static scene::EntityID PickEntityAt(scene::World* world, float wx, float wy, float radius) {
    if (!world) return 0;
    std::vector<scene::EntityID> entities;
    world->Query<scene::Transform>(entities);
    scene::EntityID best = 0;
    float best_d = radius;
    for (auto e : entities) {
        if (world->HasComponent<scene::Tilemap>(e)) continue; // 地图背景不参与点选
        auto* t = world->GetComponent<scene::Transform>(e);
        if (!t) continue;
        float dx = t->position.x - wx;
        float dy = t->position.y - wy;
        float d = std::sqrt(dx * dx + dy * dy);
        if (d < best_d) { best_d = d; best = e; }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Undo 命令：Gizmo 拖拽位移 / Tile / Passage / Terrain 批量绘制
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

// tileset_editor 用于 Execute/Undo 后触发 VB 重建（MarkTilemapDirty）
class TilePaintCommand : public ICommand {
public:
    TilePaintCommand(scene::World* world, scene::EntityID entity,
                     std::vector<TilePaintPending> changes, TilesetEditor* ts_editor)
        : world_(world), entity_(entity), changes_(std::move(changes)), ts_editor_(ts_editor) {}
    void Execute() override { Apply(false); }
    void Undo() override { Apply(true); }
    const char* GetName() const override { return "Paint Tiles"; }
private:
    void Apply(bool undo) {
        auto* tm = world_ ? world_->GetComponent<scene::Tilemap>(entity_) : nullptr;
        if (!tm) return;
        for (const auto& c : changes_)
            tm->SetTile(c.layer, c.x, c.y, undo ? c.old_tile : c.new_tile);
        if (ts_editor_) ts_editor_->MarkTilemapDirty();
    }
    scene::World* world_;
    scene::EntityID entity_;
    std::vector<TilePaintPending> changes_;
    TilesetEditor* ts_editor_;
};

class PassagePaintCommand : public ICommand {
public:
    PassagePaintCommand(scene::World* world, scene::EntityID entity,
                        std::vector<PassagePaintPending> changes)
        : world_(world), entity_(entity), changes_(std::move(changes)) {}
    void Execute() override { Apply(false); }
    void Undo() override { Apply(true); }
    const char* GetName() const override { return "Paint Passage"; }
private:
    void Apply(bool undo) {
        auto* tm = world_ ? world_->GetComponent<scene::Tilemap>(entity_) : nullptr;
        if (!tm) return;
        for (const auto& c : changes_)
            tm->SetPassage(c.layer, c.x, c.y, undo ? c.old_passage : c.new_passage);
    }
    scene::World* world_;
    scene::EntityID entity_;
    std::vector<PassagePaintPending> changes_;
};

class TerrainPaintCommand : public ICommand {
public:
    TerrainPaintCommand(scene::World* world, scene::EntityID entity,
                        std::vector<TerrainPaintPending> changes)
        : world_(world), entity_(entity), changes_(std::move(changes)) {}
    void Execute() override { Apply(false); }
    void Undo() override { Apply(true); }
    const char* GetName() const override { return "Paint Terrain"; }
private:
    void Apply(bool undo) {
        auto* tm = world_ ? world_->GetComponent<scene::Tilemap>(entity_) : nullptr;
        if (!tm) return;
        for (const auto& c : changes_) {
            if (c.layer < 0 || c.layer >= (int)tm->layers.size()) continue;
            tm->layers[c.layer].SetTerrain(c.x, c.y, undo ? c.old_terrain : c.new_terrain);
        }
    }
    scene::World* world_;
    scene::EntityID entity_;
    std::vector<TerrainPaintPending> changes_;
};

const char* ViewportPanel::GetIcon() const {
    return ICON_FA_IMAGE;
}

// ---------------------------------------------------------------------------
// 主绘制
// ---------------------------------------------------------------------------
void ViewportPanel::Draw() {
    if (!MG_BeginPanel("Viewport", GetIcon(), &is_open_)) { MG_EndPanel(); return; }

    auto& ctx = ctx_;
    using PlayState = EditorContext::PlayState;
    using GizmoMode = EditorContext::GizmoMode;
    const PlayState state = ctx.GetPlayState();

    auto* scene = ctx.GetActiveScene();
    auto* world = scene ? scene->GetWorld() : nullptr;

    // ---- 1. 视口图像（letterbox / pillarbox 居中，保持宽高比）----
    ImVec2 avail = ImGui::GetContentRegionAvail();
    uint32_t vp_w = ctx.GetViewportWidth();
    uint32_t vp_h = ctx.GetViewportHeight();
    ImTextureID tex = ctx.GetViewportTexture();
    if (!tex || vp_w == 0 || vp_h == 0 || avail.x < 8.0f || avail.y < 8.0f) {
        ImGui::TextUnformatted("Viewport initializing...");
        MG_EndPanel();
        return;
    }
    float aspect = (float)vp_w / (float)vp_h;
    float img_w = avail.x;
    float img_h = img_w / aspect;
    if (img_h > avail.y) { img_h = avail.y; img_w = img_h * aspect; }
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 img_min(origin.x + (avail.x - img_w) * 0.5f,
                   origin.y + (avail.y - img_h) * 0.5f);
    ImVec2 img_max(img_min.x + img_w, img_min.y + img_h);

    ImGui::SetCursorScreenPos(img_min);
    ImGui::Image(tex, ImVec2(img_w, img_h));
    const bool hovered = ImGui::IsItemHovered();

    // 渲染相机（Play Mode 由 CameraFollow 驱动，Edit 为编辑相机）
    math::Vec2 cam_pos = ctx.GetRenderCameraPosition();
    float zoom = ctx.GetRenderCameraZoom();   // = ortho_size（半高，世界单位）
    if (zoom <= 0.0f) zoom = 1.0f;
    float half_w = zoom * aspect;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 mouse = ImGui::GetMousePos();
    float mouse_wx = 0.0f, mouse_wy = 0.0f;
    const bool mouse_valid = MouseToWorld(mouse.x, mouse.y, img_min, img_max,
                                          cam_pos, zoom, (float)vp_w, (float)vp_h,
                                          &mouse_wx, &mouse_wy);
    // 屏幕像素 → 世界单位比例（Gizmo 拖拽 / 相机平移用）
    const float wpp_x = (2.0f * half_w) / img_w;
    const float wpp_y = (2.0f * zoom) / img_h;

    // 非编辑模式下 Gizmo 状态强制复位
    if (state != PlayState::Edit) gizmo_.mode = GizmoState::Idle;

    // =======================================================================
    // 编辑模式
    // =======================================================================
    if (state == PlayState::Edit && world) {
        const GizmoMode gizmo_mode = ctx.GetGizmoMode();
        const bool pick_allowed = (gizmo_mode != GizmoMode::Tile &&
                                   gizmo_mode != GizmoMode::Entity);

        // ---- 相机导航：滚轮缩放 + 中键平移 ----
        if (hovered) {
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f) {
                float nz = std::clamp(zoom * (1.0f - wheel * 0.1f), 16.0f, 4096.0f);
                ctx.SetCameraZoom(nz);
                zoom = nz;
                half_w = zoom * aspect;
            }
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                ImVec2 md = ImGui::GetIO().MouseDelta;
                cam_pos.x -= md.x * wpp_x;
                cam_pos.y += md.y * wpp_y;
                ctx.SetCameraPosition(cam_pos);
            }
        }

        // ---- 2. 点选实体（左键；Gizmo 悬停/拖拽中不触发）----
        if (pick_allowed && hovered && mouse_valid &&
            gizmo_.mode == GizmoState::Idle &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            scene::EntityID hit = PickEntityAt(world, mouse_wx, mouse_wy, scene::kPickRadius);
            if (hit != 0) ctx.SelectEntity(hit);
            else ctx.ClearSelection();
        }

        // ---- 3. 右键点选 + 上下文菜单 ----
        static math::Vec2 rc_world_pos{0.0f, 0.0f}; // 右键点击处世界坐标（Create 用）
        if (pick_allowed && hovered && mouse_valid &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            rc_target_entity_ = PickEntityAt(world, mouse_wx, mouse_wy, scene::kPickRadius);
            rc_on_entity_ = (rc_target_entity_ != 0);
            rc_world_pos = {mouse_wx, mouse_wy};
            if (rc_on_entity_) ctx.SelectEntity(rc_target_entity_);
            ImGui::OpenPopup("##VPContext");
        }
        if (ImGui::BeginPopup("##VPContext")) {
            if (rc_on_entity_ && world->IsValid(rc_target_entity_)) {
                if (ImGui::MenuItem(ICON_FA_PEN " Edit Events")) {
                    ctx.SelectEntity(rc_target_entity_);
                    ctx.OpenEventEditorPanel();
                }
                if (ImGui::MenuItem(ICON_FA_LIST " Inspect")) {
                    ctx.SelectEntity(rc_target_entity_);
                    ctx.OpenDetailsPanel();
                }
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_COPY " Duplicate")) {
                    auto dup = world->CloneEntity(rc_target_entity_);
                    if (auto* t = world->GetComponent<scene::Transform>(dup))
                        t->position.x += 16.0f;
                    ctx.ExecuteCommand(std::make_unique<CreateEntityCommand>(world, dup));
                    ctx.SelectEntity(dup);
                    ctx.MarkSceneDirty();
                }
                if (ImGui::MenuItem(ICON_FA_TRASH " Delete")) {
                    ctx.ExecuteCommand(
                        std::make_unique<DeleteEntityCommand>(world, rc_target_entity_));
                    ctx.ClearSelection();
                    rc_target_entity_ = 0;
                    rc_on_entity_ = false;
                }
            } else {
                if (ImGui::BeginMenu(ICON_FA_PLUS " Create")) {
                    auto create = [&](EditorContext::EntityTemplate type, const char* label) {
                        if (ImGui::MenuItem(label))
                            ctx.CreateEntityFromTemplate(type, rc_world_pos);
                    };
                    create(EditorContext::EntityTemplate::Empty,      "Entity");
                    create(EditorContext::EntityTemplate::NPC,        "NPC");
                    create(EditorContext::EntityTemplate::Monster,    "Monster");
                    create(EditorContext::EntityTemplate::Chest,      "Chest");
                    create(EditorContext::EntityTemplate::Transfer,   "Transfer");
                    create(EditorContext::EntityTemplate::Shop,       "Shop");
                    create(EditorContext::EntityTemplate::Decoration, "Decoration");
                    ImGui::EndMenu();
                }
            }
            ImGui::EndPopup();
        }

        // ---- Delete 键删除选中实体（与右键/Outliner 统一走 Undo 栈）----
        if (ImGui::IsWindowFocused() &&
            ImGui::IsKeyPressed(ImGuiKey_Delete, false) &&
            ctx.GetSelectionType() == EditorContext::SelectionType::Entity) {
            scene::EntityID sel = ctx.GetSelectedEntity();
            if (sel != 0 && world->IsValid(sel)) {
                ctx.ExecuteCommand(std::make_unique<DeleteEntityCommand>(world, sel));
                ctx.ClearSelection();
            }
        }

        // ---- 4. 网格（对齐到首个 Tilemap 原点，间距 = tile 尺寸）----
        {
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
                dl->PushClipRect(img_min, img_max, true);
                float left = cam_pos.x - half_w, right = cam_pos.x + half_w;
                float bottom = cam_pos.y - zoom, top = cam_pos.y + zoom;
                for (float gx = grid_ox + std::floor((left - grid_ox) / grid_step) * grid_step;
                     gx <= right; gx += grid_step) {
                    ImVec2 a = WorldToScreen(gx, bottom, img_min, img_max, cam_pos, zoom, (float)vp_w, (float)vp_h);
                    ImVec2 b = WorldToScreen(gx, top, img_min, img_max, cam_pos, zoom, (float)vp_w, (float)vp_h);
                    dl->AddLine(a, b, grid_col);
                }
                for (float gy = grid_oy + std::floor((bottom - grid_oy) / grid_step) * grid_step;
                     gy <= top; gy += grid_step) {
                    ImVec2 a = WorldToScreen(left, gy, img_min, img_max, cam_pos, zoom, (float)vp_w, (float)vp_h);
                    ImVec2 b = WorldToScreen(right, gy, img_min, img_max, cam_pos, zoom, (float)vp_w, (float)vp_h);
                    dl->AddLine(a, b, grid_col);
                }
                dl->PopClipRect();
            }
        }

        // ---- 5. 选中实体高亮 + Translate Gizmo（X/Y 轴拖拽）----
        {
            scene::EntityID sel = (ctx.GetSelectionType() == EditorContext::SelectionType::Entity)
                ? ctx.GetSelectedEntity() : 0;
            auto* t = (sel != 0 && world->IsValid(sel))
                ? world->GetComponent<scene::Transform>(sel) : nullptr;
            if (!t) {
                if (gizmo_.mode != GizmoState::Idle) gizmo_.mode = GizmoState::Idle;
            } else {
                ImVec2 c = WorldToScreen(t->position.x, t->position.y,
                                         img_min, img_max, cam_pos, zoom, (float)vp_w, (float)vp_h);
                // 高亮框（±16 世界单位，约一格）
                ImVec2 h0 = WorldToScreen(t->position.x - 16.0f, t->position.y - 16.0f,
                                          img_min, img_max, cam_pos, zoom, (float)vp_w, (float)vp_h);
                ImVec2 h1 = WorldToScreen(t->position.x + 16.0f, t->position.y + 16.0f,
                                          img_min, img_max, cam_pos, zoom, (float)vp_w, (float)vp_h);
                dl->AddRect(ImVec2(std::min(h0.x, h1.x), std::min(h0.y, h1.y)),
                            ImVec2(std::max(h0.x, h1.x), std::max(h0.y, h1.y)),
                            IM_COL32(255, 220, 80, 255));

                if (gizmo_mode == GizmoMode::Translate) {
                    constexpr float kAxisLen = 60.0f;
                    ImVec2 x_end(c.x + kAxisLen, c.y);
                    ImVec2 y_end(c.x, c.y - kAxisLen); // 屏幕向上 = 世界 Y+

                    // hover 检测（仅 Idle 且未在拖拽其他控件时）
                    if (gizmo_.mode == GizmoState::Idle && hovered) {
                        if (PointToSegmentDist(mouse, c, x_end) < 8.0f)
                            gizmo_.mode = GizmoState::HoverX;
                        else if (PointToSegmentDist(mouse, c, y_end) < 8.0f)
                            gizmo_.mode = GizmoState::HoverY;
                    } else if ((gizmo_.mode == GizmoState::HoverX ||
                                gizmo_.mode == GizmoState::HoverY) && !hovered) {
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
                        float wdx = md.x * wpp_x;
                        float wdy = -md.y * wpp_y; // 屏幕 Y 向下 = 世界 Y 向下偏移取反
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
                    dl->AddLine(c, x_end, x_col, 2.0f);
                    dl->AddLine(c, y_end, y_col, 2.0f);
                    dl->AddRectFilled(ImVec2(x_end.x - 4, x_end.y - 4),
                                      ImVec2(x_end.x + 4, x_end.y + 4), x_col);
                    dl->AddRectFilled(ImVec2(y_end.x - 4, y_end.y - 4),
                                      ImVec2(y_end.x + 4, y_end.y + 4), y_col);
                }
            }
        }

        // ---- 6. Entity 放置模式：左键在点击处创建实体（支持网格吸附）----
        if (gizmo_mode == GizmoMode::Entity && hovered && mouse_valid) {
            math::Vec2 place_pos(mouse_wx, mouse_wy);
            float tile = scene && scene->GetTileSize() > 0 ? (float)scene->GetTileSize() : 32.0f;
            if (ctx.IsSnapToGrid()) {
                place_pos.x = (std::floor(place_pos.x / tile) + 0.5f) * tile;
                place_pos.y = (std::floor(place_pos.y / tile) + 0.5f) * tile;
            }
            // 放置预览框
            ImVec2 g0 = WorldToScreen(place_pos.x - tile * 0.5f, place_pos.y - tile * 0.5f,
                                      img_min, img_max, cam_pos, zoom, (float)vp_w, (float)vp_h);
            ImVec2 g1 = WorldToScreen(place_pos.x + tile * 0.5f, place_pos.y + tile * 0.5f,
                                      img_min, img_max, cam_pos, zoom, (float)vp_w, (float)vp_h);
            dl->AddRect(ImVec2(std::min(g0.x, g1.x), std::min(g0.y, g1.y)),
                        ImVec2(std::max(g0.x, g1.x), std::max(g0.y, g1.y)),
                        IM_COL32(120, 200, 255, 220));
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                ctx.CreateEntityFromTemplate(EditorContext::EntityTemplate::Empty, place_pos);
            }
        }

        // ---- 7. Tile 绘制模式（左键绘制 / 右键或 Shift+左键擦除）----
        if (gizmo_mode == GizmoMode::Tile && hovered && mouse_valid) {
            // 目标 Tilemap 实体：优先 Tile Palette 选定，否则场景中第一个
            scene::EntityID tm_entity = ctx.GetTilemapEntity();
            if (tm_entity == 0 || !world->IsValid(tm_entity)) {
                std::vector<scene::EntityID> tm_entities;
                world->Query<scene::Tilemap>(tm_entities);
                tm_entity = tm_entities.empty() ? 0 : tm_entities[0];
            }
            auto* tm = tm_entity ? world->GetComponent<scene::Tilemap>(tm_entity) : nullptr;
            if (tm && !tm->layers.empty()) {
                float ox = 0.0f, oy = 0.0f;
                if (auto* tt = world->GetComponent<scene::Transform>(tm_entity)) {
                    ox = tt->position.x;
                    oy = tt->position.y;
                }
                const float tw = (float)tm->tile_width;
                const float th = (float)tm->tile_height;
                int tx = (int)std::floor((mouse_wx - ox) / tw);
                int ty = (int)std::floor((mouse_wy - oy) / th);
                int layer_idx = std::clamp(ctx.GetCurrentTileLayer(), 0, (int)tm->layers.size() - 1);
                auto& layer = tm->layers[layer_idx];

                if (tx >= 0 && tx < layer.width && ty >= 0 && ty < layer.height) {
                    // 悬停格高亮
                    ImVec2 c0 = WorldToScreen(ox + tx * tw, oy + ty * th,
                                              img_min, img_max, cam_pos, zoom, (float)vp_w, (float)vp_h);
                    ImVec2 c1 = WorldToScreen(ox + (tx + 1) * tw, oy + (ty + 1) * th,
                                              img_min, img_max, cam_pos, zoom, (float)vp_w, (float)vp_h);
                    dl->AddRect(ImVec2(std::min(c0.x, c1.x), std::min(c0.y, c1.y)),
                                ImVec2(std::max(c0.x, c1.x), std::max(c0.y, c1.y)),
                                IM_COL32(255, 255, 0, 200));

                    const bool shift = ImGui::GetIO().KeyShift;
                    const bool paint = ImGui::IsMouseDown(ImGuiMouseButton_Left) && !shift;
                    const bool erase = ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
                                       (ImGui::IsMouseDown(ImGuiMouseButton_Left) && shift);

                    if (paint || erase) {
                        auto brush = ctx.GetTileBrushMode();
                        if (brush == EditorContext::TileBrushMode::Tile) {
                            int new_tile = erase ? -1 : ctx.GetSelectedGlobalTileIndex();
                            if (erase || new_tile >= 0) {
                                int old_tile = layer.GetTile(tx, ty);
                                if (old_tile != new_tile) {
                                    // autotile：捕获十字 5 格 before/after，变化全部入 Undo
                                    const TilesetAsset* auto_ts = nullptr;
                                    if (!erase) {
                                        const auto* ts = ctx.GetSelectedTileset();
                                        if (ts && ts->auto_tile) auto_ts = ts;
                                    } else if (old_tile >= 0) {
                                        const auto* ts = ctx.GetTilesetByGlobalTile(old_tile);
                                        if (ts && ts->auto_tile) auto_ts = ts;
                                    }
                                    if (auto_ts) {
                                        const int dx5[5] = {0, 0, 0, -1, 1};
                                        const int dy5[5] = {0, -1, 1, 0, 0};
                                        int old_vals[5];
                                        for (int i = 0; i < 5; ++i)
                                            old_vals[i] = layer.GetTile(tx + dx5[i], ty + dy5[i]);
                                        layer.SetTile(tx, ty, new_tile);
                                        RefreshAutoTiles(layer, tx, ty, auto_ts->start_index);
                                        for (int i = 0; i < 5; ++i) {
                                            int nx = tx + dx5[i], ny = ty + dy5[i];
                                            int nv = layer.GetTile(nx, ny);
                                            if (nv != old_vals[i]) {
                                                pending_tile_changes_.push_back(
                                                    {layer_idx, nx, ny, old_vals[i], nv});
                                            }
                                        }
                                    } else {
                                        layer.SetTile(tx, ty, new_tile);
                                        pending_tile_changes_.push_back(
                                            {layer_idx, tx, ty, old_tile, new_tile});
                                    }
                                    if (!pending_tile_changes_.empty()) {
                                        pending_tile_world_ = world;
                                        pending_tilemap_entity_ = tm_entity;
                                    }
                                    ctx.MarkTilemapDirty();
                                    ctx.MarkSceneDirty();
                                }
                            }
                        } else if (brush == EditorContext::TileBrushMode::Passage) {
                            uint8_t nv = erase ? scene::Pass_None : ctx.GetPassageBrushMask();
                            uint8_t ov = layer.GetPassage(tx, ty);
                            if (ov != nv) {
                                layer.SetPassage(tx, ty, nv);
                                pending_passage_changes_.push_back({layer_idx, tx, ty, ov, nv});
                                pending_passage_world_ = world;
                                pending_passage_entity_ = tm_entity;
                                ctx.MarkSceneDirty();
                            }
                        } else { // Terrain
                            uint8_t nv = erase ? 0 : ctx.GetTerrainBrushTag();
                            uint8_t ov = layer.GetTerrain(tx, ty);
                            if (ov != nv) {
                                layer.SetTerrain(tx, ty, nv);
                                pending_terrain_changes_.push_back({layer_idx, tx, ty, ov, nv});
                                pending_terrain_world_ = world;
                                pending_terrain_entity_ = tm_entity;
                                ctx.MarkSceneDirty();
                            }
                        }
                    }
                }
            }
        }

        // ---- 8. 底部浮动工具栏（Gizmo 模式 + ViewMode）----
        {
            constexpr float kBtn = 24.0f;
            constexpr float kPad = 8.0f;
            constexpr float kGap = 4.0f;
            const struct { const char* label; const char* tip; GizmoMode mode; } kGizmoBtns[5] = {
                {"T",         "Translate",   GizmoMode::Translate},
                {"R",         "Rotate",      GizmoMode::Rotate},
                {"S",         "Scale",       GizmoMode::Scale},
                {ICON_FA_TH,   "Tile Paint",  GizmoMode::Tile},
                {ICON_FA_PLUS, "Place Entity", GizmoMode::Entity},
            };
            float bar_w = 5 * kBtn + 4 * kGap + 2 * kPad;
            ImVec2 bar_min(img_min.x + kPad, img_max.y - kBtn - 2 * kPad);
            dl->AddRectFilled(bar_min, ImVec2(bar_min.x + bar_w, bar_min.y + kBtn + kPad),
                              IM_COL32(20, 22, 28, 220), 6.0f);
            ImGui::SetCursorScreenPos(ImVec2(bar_min.x + kPad, bar_min.y + kPad * 0.5f));
            for (const auto& b : kGizmoBtns) {
                if (MG_ToolbarButton(b.label, b.tip, gizmo_mode == b.mode, ImVec2(kBtn, kBtn)))
                    ctx.SetGizmoMode(b.mode);
                ImGui::SameLine(0.0f, kGap);
            }

            // ViewMode（右下）：Lit / Unlit / Wireframe
            auto view_mode = ctx.GetViewMode();
            const struct { const char* label; const char* tip; EditorContext::ViewMode mode; } kViewBtns[3] = {
                {"L", "Lit",       EditorContext::ViewMode::Lit},
                {"U", "Unlit",     EditorContext::ViewMode::Unlit},
                {"W", "Wireframe", EditorContext::ViewMode::Wireframe},
            };
            float vw = 3 * kBtn + 2 * kGap + 2 * kPad;
            ImVec2 vm_min(img_max.x - vw - kPad, img_max.y - kBtn - 2 * kPad);
            dl->AddRectFilled(vm_min, ImVec2(vm_min.x + vw, vm_min.y + kBtn + kPad),
                              IM_COL32(20, 22, 28, 220), 6.0f);
            ImGui::SetCursorScreenPos(ImVec2(vm_min.x + kPad, vm_min.y + kPad * 0.5f));
            for (int i = 0; i < 3; ++i) {
                bool wireframe_off = (kViewBtns[i].mode == EditorContext::ViewMode::Wireframe &&
                                      !ctx.GetViewport().IsWireframeSupported());
                if (wireframe_off) ImGui::BeginDisabled();
                if (MG_ToolbarButton(kViewBtns[i].label, kViewBtns[i].tip,
                                     view_mode == kViewBtns[i].mode, ImVec2(kBtn, kBtn)))
                    ctx.SetViewMode(kViewBtns[i].mode);
                if (wireframe_off) ImGui::EndDisabled();
                if (i < 2) ImGui::SameLine(0.0f, kGap);
            }
        }
    }

    // =======================================================================
    // Play Mode（覆盖层 UI 已提取到 gameui/playmode_ui.*，与 game.exe 共用）
    // =======================================================================
    auto* event_system = ctx.GetEventSystem();
    const bool in_game = (state == PlayState::Playing &&
                          ctx.GetScreenMode() == EditorContext::ScreenMode::Game);

    // Escape 暂停切换已下沉 GameRuntime；此处仅维护子菜单状态复位
    if (state != PlayState::Paused) pause_sub_ = gameui::PauseSub::None;

    // 组装共享 UI 输入
    gameui::PlayUIContext pui;
    pui.es = event_system;
    pui.dbs = &ctx.GetDatabases();
    pui.play_mode = &ctx.GetPlayMode();
    pui.dl = dl;
    pui.view_min = img_min;
    pui.view_max = img_max;
    pui.fade_alpha = ctx.GetFadeAlpha();
    if (const auto* manifest = ctx.GetProjectManifest())
        if (!manifest->name.empty()) pui.game_title = manifest->name.c_str();
    pui.project_root = ctx.GetProjectPath();
    pui.on_exit = [&ctx]() { ctx.SetPlayState(PlayState::Edit); };
    pui.exit_label = "Exit to Editor";
    pui.cam_pos = cam_pos;
    pui.cam_zoom = zoom;

    // ---- Fade 过渡遮罩 ----
    if (state != PlayState::Edit) gameui::DrawFadeOverlay(pui);

    // ---- 标题画面 ----
    if (state == PlayState::Playing &&
        ctx.GetScreenMode() == EditorContext::ScreenMode::Title) {
        gameui::DrawTitleScreen(pui);
    }

    // ---- 暂停菜单 ----
    if (state == PlayState::Paused) gameui::DrawPauseMenu(pui, pause_sub_);

    // ---- Toast / 对话框 / 选项 / 商店 / 战斗 / HUD ----
    if (in_game) {
        gameui::DrawToasts(pui);
        gameui::DrawDialog(pui, mouse);
        gameui::DrawChoices(pui);
        gameui::DrawShopOverlay(pui);
        gameui::DrawBattleOverlay(pui);
        gameui::DrawHUDOverlays(pui);
    }

    // ---- 17. Undo flush：帧内收集的绘制变化推入命令栈 ----
    if (!pending_tile_changes_.empty()) {
        if (pending_tile_world_ && pending_tile_world_ == world) {
            ctx.ExecuteCommand(std::make_unique<TilePaintCommand>(
                pending_tile_world_, pending_tilemap_entity_,
                std::move(pending_tile_changes_), &ctx.GetTilesetEditor()));
        }
        pending_tile_changes_.clear();
        pending_tile_world_ = nullptr;
        pending_tilemap_entity_ = 0;
    }
    if (!pending_passage_changes_.empty()) {
        if (pending_passage_world_ && pending_passage_world_ == world) {
            ctx.ExecuteCommand(std::make_unique<PassagePaintCommand>(
                pending_passage_world_, pending_passage_entity_,
                std::move(pending_passage_changes_)));
        }
        pending_passage_changes_.clear();
        pending_passage_world_ = nullptr;
        pending_passage_entity_ = 0;
    }
    if (!pending_terrain_changes_.empty()) {
        if (pending_terrain_world_ && pending_terrain_world_ == world) {
            ctx.ExecuteCommand(std::make_unique<TerrainPaintCommand>(
                pending_terrain_world_, pending_terrain_entity_,
                std::move(pending_terrain_changes_)));
        }
        pending_terrain_changes_.clear();
        pending_terrain_world_ = nullptr;
        pending_terrain_entity_ = 0;
    }

    MG_EndPanel();
}

} // namespace editor
} // namespace mg
