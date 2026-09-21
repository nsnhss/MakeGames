#include "editor/tileset_editor.h"
#include "resource/resource_manager.h"
#include "resource/data_manager.h"
#include "backends/imgui_impl_vulkan.h"
#include "third_party/json.hpp"
#include <algorithm>
#include <vector>

namespace mg {
namespace editor {

bool TilesetEditor::LoadTileset(const std::string& name, const std::string& image_path,
                                int tile_w, int tile_h, int columns, int rows,
                                int start_index) {
    auto it = registry_.find(name);
    if (it != registry_.end()) {
        if (it->second.imgui_ds) {
            ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)it->second.imgui_ds);
        }
        registry_.erase(it);
    }
    TilesetAsset ts;
    ts.name = name;
    ts.image_path = image_path;
    ts.tile_width = tile_w;
    ts.tile_height = tile_h;
    ts.columns = columns;
    ts.rows = rows;
    ts.tile_count = columns * rows;
    if (start_index < 0) {
        int next = 0;
        for (const auto& [n, t] : registry_) {
            next = std::max(next, t.start_index + t.tile_count);
        }
        ts.start_index = next;
    } else {
        ts.start_index = start_index;
    }
    ts.texture_ptr = ResourceManager::Get().LoadTexture(image_path);
    if (!ts.texture_ptr->IsValid()) {
        return false;
    }
    ts.imgui_ds = (ImTextureID)ImGui_ImplVulkan_AddTexture(
        ts.texture_ptr->GetSampler(), ts.texture_ptr->GetView(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    registry_[name] = std::move(ts);
    return true;
}

void TilesetEditor::UnloadAllTilesets() {
    for (auto& [name, ts] : registry_) {
        if (ts.imgui_ds) {
            ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)ts.imgui_ds);
        }
    }
    registry_.clear();
    selected_tileset_name_.clear();
    selected_tile_index_ = -1;
}

const TilesetAsset* TilesetEditor::GetTileset(const std::string& name) const {
    auto it = registry_.find(name);
    return (it != registry_.end()) ? &it->second : nullptr;
}

TilesetAsset* TilesetEditor::GetTileset(const std::string& name) {
    auto it = registry_.find(name);
    return (it != registry_.end()) ? &it->second : nullptr;
}

void TilesetEditor::SetAutoTile(const std::string& name, bool value) {
    auto it = registry_.find(name);
    if (it != registry_.end()) {
        it->second.auto_tile = value;
    }
}

void TilesetEditor::SelectTileset(const std::string& name) {
    selected_tileset_name_ = name;
    selected_tile_index_ = -1;
}

const TilesetAsset* TilesetEditor::GetSelectedTileset() const {
    return GetTileset(selected_tileset_name_);
}

int TilesetEditor::GetSelectedGlobalTileIndex() const {
    if (selected_tile_index_ < 0) return -1;
    const auto* ts = GetSelectedTileset();
    if (!ts) return -1;
    return ts->start_index + selected_tile_index_;
}

const TilesetAsset* TilesetEditor::GetTilesetByGlobalTile(int global_tile) const {
    if (global_tile < 0) return nullptr;
    for (const auto& [name, ts] : registry_) {
        if (global_tile >= ts.start_index && global_tile < ts.start_index + ts.tile_count)
            return &ts;
    }
    return nullptr;
}

namespace {

nlohmann::json TilesetToJson(const TilesetAsset& ts) {
    nlohmann::json j;
    j["name"] = ts.name;
    j["image_path"] = ts.image_path;
    j["tile_width"] = ts.tile_width;
    j["tile_height"] = ts.tile_height;
    j["columns"] = ts.columns;
    j["rows"] = ts.rows;
    j["auto_tile"] = ts.auto_tile;
    j["start_index"] = ts.start_index;
    if (!ts.animations.empty()) {
        nlohmann::json anims = nlohmann::json::array();
        for (const auto& a : ts.animations) {
            anims.push_back({{"tile_index", a.tile_index}, {"frames", a.frames}, {"fps", a.fps}});
        }
        j["animations"] = anims;
    }
    return j;
}

} // namespace

bool TilesetEditor::LoadFromProject(const std::string& project_path, renderer::RendererContext* ctx) {
    (void)ctx;
    std::string path = project_path + "/data/tilesets.json";
    auto raw = DataManager::Get().LoadJsonRaw(path);
    if (!raw || !raw->is_array()) return false;
    bool any = false;
    for (auto& item : *raw) {
        std::string name = item.value("name", "");
        if (name.empty()) continue;
        std::string img_path = item.value("image_path", "");
        int tw = item.value("tile_width", 32);
        int th = item.value("tile_height", 32);
        int cols = item.value("columns", 0);
        int rows = item.value("rows", 0);
        bool at = item.value("auto_tile", false);
        int si = item.value("start_index", -1);
        LoadTileset(name, img_path, tw, th, cols, rows, si);
        if (at) { auto it = registry_.find(name); if (it != registry_.end()) it->second.auto_tile = true; }
        if (item.contains("animations") && item["animations"].is_array()) {
            auto it = registry_.find(name);
            if (it != registry_.end()) {
                for (const auto& aj : item["animations"]) {
                    scene::TileAnimDef a;
                    a.tile_index = aj.value("tile_index", 0);
                    a.fps = aj.value("fps", 4.0f);
                    if (aj.contains("frames")) a.frames = aj["frames"].get<std::vector<int>>();
                    it->second.animations.push_back(std::move(a));
                }
            }
        }
        any = true;
    }
    return any;
}

void TilesetEditor::SaveToProject(const std::string& project_path) {
    if (project_path.empty()) return;
    nlohmann::json arr = nlohmann::json::array();
    for (auto& [name, ts] : registry_) {
        arr.push_back(TilesetToJson(ts));
    }
    DataManager::Get().Save(project_path + "/data/tilesets.json", arr);
}

bool TilesetEditor::LoadDemoTileset(renderer::RendererContext* ctx) {
    const int TW = 32, TH = 32, COLS = 2;
    const int IMG_W = COLS * TW, IMG_H = TH;
    std::vector<uint8_t> pixels(IMG_W * IMG_H * 4, 255);

    // Tile 0: green grass (#228B22)
    for (int y = 0; y < TH; ++y) {
        for (int x = 0; x < TW; ++x) {
            int i = (y * IMG_W + x) * 4;
            pixels[i+0] = 34;  pixels[i+1] = 139; pixels[i+2] = 34;
        }
    }
    // Tile 1: gray stone
    for (int y = 0; y < TH; ++y) {
        for (int x = TW; x < IMG_W; ++x) {
            int i = (y * IMG_W + x) * 4;
            pixels[i+0] = 128; pixels[i+1] = 128; pixels[i+2] = 128;
        }
    }

    TilesetAsset ts;
    ts.name = "Demo";
    ts.image_path = "Demo";
    ts.tile_width = TW;
    ts.tile_height = TH;
    ts.columns = COLS;
    ts.rows = 1;
    ts.tile_count = COLS;
    ts.texture_ptr = ResourceManager::Get().CreateTextureFromPixels(
        "DemoTileset", pixels.data(), IMG_W, IMG_H);
    ts.imgui_ds = (ImTextureID)ImGui_ImplVulkan_AddTexture(
        ts.texture_ptr->GetSampler(), ts.texture_ptr->GetView(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    registry_["Demo"] = std::move(ts);
    SelectTileset("Demo");
    SelectTile(0);
    return true;
}

} // namespace editor
} // namespace mg
