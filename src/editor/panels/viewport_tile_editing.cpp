// viewport_tile_editing.cpp — 视口 Tile/Passage/Terrain 绘制 + 帧末 Undo flush
// （D4 自 viewport_panel.cpp 拆出）。
// 绘制期间把每格 before/after 收集到 pending_*，帧末合并为单条命令推入 Undo 栈。
#include "viewport_panel.h"
#include "tile_autotile.h"
#include "src/editor/editor_context.h"
#include "src/editor/tileset_editor.h"
#include "scene/scene.h"
#include "scene/world.h"
#include "scene/components/transform.h"
#include "scene/components/tilemap.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace mg {
namespace editor {

// ---------------------------------------------------------------------------
// Undo 命令：Tile / Passage / Terrain 批量绘制
// ---------------------------------------------------------------------------
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

// Tile 绘制模式（左键绘制 / 右键或 Shift+左键擦除）
void ViewportPanel::DrawTileEditing(ViewportFrameCtx& f) {
    auto& ctx = ctx_;
    auto* world = f.world;
    using GizmoMode = EditorContext::GizmoMode;
    if (ctx.GetGizmoMode() != GizmoMode::Tile || !f.hovered || !f.mouse_valid) return;

    // 目标 Tilemap 实体：优先 Tile Palette 选定，否则场景中第一个
    scene::EntityID tm_entity = ctx.GetTilemapEntity();
    if (tm_entity == 0 || !world->IsValid(tm_entity)) {
        std::vector<scene::EntityID> tm_entities;
        world->Query<scene::Tilemap>(tm_entities);
        tm_entity = tm_entities.empty() ? 0 : tm_entities[0];
    }
    auto* tm = tm_entity ? world->GetComponent<scene::Tilemap>(tm_entity) : nullptr;
    if (!tm || tm->layers.empty()) return;

    float ox = 0.0f, oy = 0.0f;
    if (auto* tt = world->GetComponent<scene::Transform>(tm_entity)) {
        ox = tt->position.x;
        oy = tt->position.y;
    }
    const float tw = (float)tm->tile_width;
    const float th = (float)tm->tile_height;
    int tx = (int)std::floor((f.mouse_wx - ox) / tw);
    int ty = (int)std::floor((f.mouse_wy - oy) / th);
    int layer_idx = std::clamp(ctx.GetCurrentTileLayer(), 0, (int)tm->layers.size() - 1);
    auto& layer = tm->layers[layer_idx];

    if (tx < 0 || tx >= layer.width || ty < 0 || ty >= layer.height) return;

    // 悬停格高亮
    ImVec2 c0 = WorldToScreen(ox + tx * tw, oy + ty * th,
                              f.img_min, f.img_max, f.cam_pos, f.zoom, (float)f.vp_w, (float)f.vp_h);
    ImVec2 c1 = WorldToScreen(ox + (tx + 1) * tw, oy + (ty + 1) * th,
                              f.img_min, f.img_max, f.cam_pos, f.zoom, (float)f.vp_w, (float)f.vp_h);
    f.dl->AddRect(ImVec2(std::min(c0.x, c1.x), std::min(c0.y, c1.y)),
                  ImVec2(std::max(c0.x, c1.x), std::max(c0.y, c1.y)),
                  IM_COL32(255, 255, 0, 200));

    const bool shift = ImGui::GetIO().KeyShift;
    const bool paint = ImGui::IsMouseDown(ImGuiMouseButton_Left) && !shift;
    const bool erase = ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
                       (ImGui::IsMouseDown(ImGuiMouseButton_Left) && shift);

    if (!paint && !erase) return;

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

// Undo flush：帧内收集的绘制变化推入命令栈
void ViewportPanel::FlushPaintCommands(scene::World* world) {
    auto& ctx = ctx_;
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
}

} // namespace editor
} // namespace mg
