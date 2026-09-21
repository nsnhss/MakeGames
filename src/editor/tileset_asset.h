#pragma once
#include <string>
#include <vector>
#include "renderer/texture.h"
#include "scene/systems/tile_animation.h"
#include "imgui.h"

namespace mg {
namespace editor {

struct TilesetAsset {
    std::string name;
    std::string image_path;
    int tile_width = 32;
    int tile_height = 32;
    int columns = 0;
    int rows = 0;
    int tile_count = 0;
    float spacing = 0.0f;
    float margin = 0.0f;
    bool auto_tile = false;                    // 是否启用自动图块
    int start_index = 0;                       // 全局起始 tile 索引（多 tileset）
    std::vector<scene::TileAnimDef> animations; // 瓦片动画（局部 tile 索引）
    renderer::Texture* texture_ptr = nullptr;  // owned by ResourceManager
    ImTextureID imgui_ds = 0;

    void Destroy() {
        texture_ptr = nullptr;  // ResourceManager manages lifecycle
        imgui_ds = 0;
    }
};

} // namespace editor
} // namespace mg
