#pragma once
#include "scene/component.h"
#include "third_party/json.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace mg {
namespace scene {

// 4 方向阻挡掩码，保存在 TileLayer.passages 中。
// 与 TilemapDesign.md v2 保持一致：bit 置 1 表示该方向不可通行。
//   bit0 (1): 上
//   bit1 (2): 左
//   bit2 (4): 右
//   bit3 (8): 下
// 0       = 全方向可通行
// 0x0F(15)= 四方向全阻挡（墙）
enum Passage : uint8_t {
    Pass_None  = 0,
    Pass_Up    = 1 << 0,
    Pass_Left  = 1 << 1,
    Pass_Right = 1 << 2,
    Pass_Down  = 1 << 3,
    Pass_All   = Pass_Up | Pass_Left | Pass_Right | Pass_Down,
};

struct TileLayer {
    std::string name;
    int width = 0;
    int height = 0;
    std::vector<int> tiles;      // -1 = empty, >=0 = tile index
    std::vector<uint8_t> passages; // per-tile Passage bitmask
    std::vector<uint8_t> terrain;  // per-tile terrain tag (0 = none)
    bool visible = true;
    float edge_thickness = 1.0f;  // map border collision thickness (pixels), 0=open boundary

    TileLayer() = default;
    TileLayer(const std::string& n, int w, int h)
        : name(n), width(w), height(h), tiles(w * h, -1) {
        ResizePassagesAndTerrain();
    }

    int GetTile(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return -1;
        return tiles[y * width + x];
    }
    void SetTile(int x, int y, int tile_index) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        tiles[y * width + x] = tile_index;
    }

    // 确保 passages / terrain 数组与 tiles 同尺寸；默认全通(0)、地形 0。
    void ResizePassagesAndTerrain() {
        const size_t n = tiles.size();
        passages.assign(n, Pass_None);
        terrain.assign(n, 0);
    }

    uint8_t GetPassage(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return Pass_None;
        return passages[y * width + x];
    }
    void SetPassage(int x, int y, uint8_t flags) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        passages[y * width + x] = flags;
    }

    uint8_t GetTerrain(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return 0;
        return terrain[y * width + x];
    }
    void SetTerrain(int x, int y, uint8_t tag) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        terrain[y * width + x] = tag;
    }

    // 从 (x,y) 朝方向 (dx,dy) 移动一格是否可通过。
    // dx/dy 取 -1/0/1 且只有一个非零。bits 置 1 表示该方向被阻挡。
    // 约定：y+ 为“上”，y- 为“下”。
    bool CanPass(int x, int y, int dx, int dy) const {
        if (dx == 0 && dy == 0) return true;
        uint8_t from = GetPassage(x, y);
        uint8_t to   = GetPassage(x + dx, y + dy);
        uint8_t exit_dir = Pass_None;
        uint8_t enter_dir = Pass_None;
        if (dx > 0) { exit_dir = Pass_Right; enter_dir = Pass_Left; }
        else if (dx < 0) { exit_dir = Pass_Left; enter_dir = Pass_Right; }
        else if (dy > 0) { exit_dir = Pass_Up;    enter_dir = Pass_Down; }
        else if (dy < 0) { exit_dir = Pass_Down;  enter_dir = Pass_Up; }
        return (from & exit_dir) == 0 && (to & enter_dir) == 0;
    }

    // 改变层尺寸时同时保留原数据
    void Resize(int w, int h) {
        std::vector<int> new_tiles(w * h, -1);
        std::vector<uint8_t> new_passages(w * h, Pass_None);
        std::vector<uint8_t> new_terrain(w * h, 0);
        int copy_w = (w < width) ? w : width;
        int copy_h = (h < height) ? h : height;
        for (int y = 0; y < copy_h; ++y) {
            for (int x = 0; x < copy_w; ++x) {
                size_t src = y * width + x;
                size_t dst = y * w + x;
                new_tiles[dst] = tiles[src];
                new_passages[dst] = passages[src];
                new_terrain[dst] = terrain[src];
            }
        }
        width = w;
        height = h;
        tiles = std::move(new_tiles);
        passages = std::move(new_passages);
        terrain = std::move(new_terrain);
    }
};

struct Tilemap : IComponent {
    std::string tileset_name;                // 兼容旧版：主 tileset
    std::vector<std::string> tileset_names;  // 多 tileset 引用（优先级高于 tileset_name）
    int tile_width = 32;
    int tile_height = 32;
    std::vector<TileLayer> layers;

    std::vector<std::string> GetTilesetNames() const {
        if (!tileset_names.empty()) return tileset_names;
        if (!tileset_name.empty()) return { tileset_name };
        return {};
    }

    void ResizeLayers(int w, int h) {
        for (auto& layer : layers) {
            layer.Resize(w, h);
        }
    }

    int GetTile(int layer, int x, int y) const {
        if (layer < 0 || layer >= (int)layers.size()) return -1;
        return layers[layer].GetTile(x, y);
    }
    void SetTile(int layer, int x, int y, int tile_index) {
        if (layer < 0 || layer >= (int)layers.size()) return;
        layers[layer].SetTile(x, y, tile_index);
    }

    uint8_t GetPassage(int layer, int x, int y) const {
        if (layer < 0 || layer >= (int)layers.size()) return Pass_All;
        return layers[layer].GetPassage(x, y);
    }
    void SetPassage(int layer, int x, int y, uint8_t flags) {
        if (layer < 0 || layer >= (int)layers.size()) return;
        layers[layer].SetPassage(x, y, flags);
    }
};

inline void to_json(nlohmann::json& j, const TileLayer& l) {
    j["name"] = l.name;
    j["width"] = l.width;
    j["height"] = l.height;
    j["tiles"] = l.tiles;
    j["passages"] = l.passages;
    j["terrain"] = l.terrain;
    j["visible"] = l.visible;
    j["edge_thickness"] = l.edge_thickness;
}
inline void from_json(const nlohmann::json& j, TileLayer& l) {
    if (j.contains("name")) l.name = j["name"].get<std::string>();
    if (j.contains("width")) l.width = j["width"].get<int>();
    if (j.contains("height")) l.height = j["height"].get<int>();
    if (j.contains("tiles")) l.tiles = j["tiles"].get<std::vector<int>>();
    l.ResizePassagesAndTerrain();
    if (j.contains("passages")) l.passages = j["passages"].get<std::vector<uint8_t>>();
    if (j.contains("terrain")) l.terrain = j["terrain"].get<std::vector<uint8_t>>();

    // 兼容旧版：collision=true 的层整体不可通行，并保持隐藏
    if (j.contains("collision") && j.value("collision", false)) {
        l.visible = false;
        for (size_t i = 0; i < l.tiles.size(); ++i) {
            if (l.tiles[i] >= 0) l.passages[i] = Pass_All;
        }
    } else if (j.contains("visible")) {
        l.visible = j["visible"].get<bool>();
    }

    if (j.contains("edge_thickness")) l.edge_thickness = j["edge_thickness"].get<float>();
}

inline void to_json(nlohmann::json& j, const Tilemap& t) {
    j["tileset_name"] = t.tileset_name;
    j["tileset_names"] = t.tileset_names;
    j["tile_width"] = t.tile_width;
    j["tile_height"] = t.tile_height;
    j["layers"] = t.layers;
}
inline void from_json(const nlohmann::json& j, Tilemap& t) {
    if (j.contains("tileset_name")) t.tileset_name = j["tileset_name"].get<std::string>();
    if (j.contains("tileset_names")) t.tileset_names = j["tileset_names"].get<std::vector<std::string>>();
    if (j.contains("tile_width")) t.tile_width = j["tile_width"].get<int>();
    if (j.contains("tile_height")) t.tile_height = j["tile_height"].get<int>();
    if (j.contains("layers")) t.layers = j["layers"].get<std::vector<TileLayer>>();
}

} // namespace scene
} // namespace mg

REGISTER_COMPONENT(mg::scene::Tilemap, "Tilemap")
