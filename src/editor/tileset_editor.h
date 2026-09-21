#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include "editor/tileset_asset.h"
#include "scene/components/tilemap.h" // mg::scene::Pass_All
#include "imgui.h" // ImTextureID

namespace mg {
namespace renderer { class RendererContext; }
namespace editor {

// Tileset 编辑状态：tileset 注册表（含 GPU 纹理/ImGui 描述集）、
// 当前选中 tileset/tile/图层、瓦片画笔模式（Tile/Passage/Terrain）、
// 当前编辑的 Tilemap 实体与脏标记，以及 tilesets.json 持久化。
class TilesetEditor {
public:
    enum class Brush { Tile, Passage, Terrain };

    bool LoadTileset(const std::string& name, const std::string& image_path,
                     int tile_w, int tile_h, int columns, int rows,
                     int start_index = -1);
    void UnloadAllTilesets();
    const TilesetAsset* GetTileset(const std::string& name) const;
    TilesetAsset* GetTileset(const std::string& name);
    const std::unordered_map<std::string, TilesetAsset>& GetRegistry() const { return registry_; }
    void SetAutoTile(const std::string& name, bool value);
    void SelectTileset(const std::string& name);
    const TilesetAsset* GetSelectedTileset() const;
    void SelectTile(int index) { selected_tile_index_ = index; }
    int GetSelectedTileIndex() const { return selected_tile_index_; }
    int GetSelectedGlobalTileIndex() const; // selected_tileset.start_index + local index, -1 if none

    // 按全局 tile 索引查找所属 tileset（多 tileset）
    const TilesetAsset* GetTilesetByGlobalTile(int global_tile) const;
    void SetCurrentTileLayer(int layer) { current_tile_layer_ = layer; }
    int GetCurrentTileLayer() const { return current_tile_layer_; }

    void SetBrushMode(Brush mode) { brush_mode_ = mode; }
    Brush GetBrushMode() const { return brush_mode_; }
    void SetTerrainBrushTag(uint8_t tag) { terrain_brush_tag_ = tag; }
    uint8_t GetTerrainBrushTag() const { return terrain_brush_tag_; }
    void SetPassageBrushMask(uint8_t mask) { passage_brush_mask_ = mask; }
    uint8_t GetPassageBrushMask() const { return passage_brush_mask_; }

    void SetTilemapEntity(uint32_t entity_id) { tilemap_entity_ = entity_id; }
    uint32_t GetTilemapEntity() const { return tilemap_entity_; }

    // tilesets.json 持久化（project_path 为项目根目录）
    bool LoadFromProject(const std::string& project_path, renderer::RendererContext* ctx);
    void SaveToProject(const std::string& project_path);
    bool LoadDemoTileset(renderer::RendererContext* ctx); // fallback: 无 tilesets.json 时生成纯色 Demo tileset

    void MarkTilemapDirty() { tilemap_dirty_ = true; }
    void ClearTilemapDirty() { tilemap_dirty_ = false; }
    bool IsTilemapDirty() const { return tilemap_dirty_; }

private:
    std::unordered_map<std::string, TilesetAsset> registry_;
    std::string selected_tileset_name_;
    int selected_tile_index_ = -1;
    int current_tile_layer_ = 0;
    Brush brush_mode_ = Brush::Tile;
    uint8_t terrain_brush_tag_ = 1;
    uint8_t passage_brush_mask_ = mg::scene::Pass_All;
    uint32_t tilemap_entity_ = 0;
    bool tilemap_dirty_ = true; // 首次渲染需生成 VB
};

} // namespace editor
} // namespace mg
