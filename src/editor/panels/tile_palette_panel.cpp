#include "tile_palette_panel.h"
#include "src/editor/tileset_editor.h"
#include "src/editor/scene_session.h"
#include "src/editor/editor_project_manager.h"
#include "src/editor/file_dialog.h"
#include "src/editor/tileset_asset.h"
#include "scene/scene.h"
#include "scene/world.h"
#include "scene/components/tilemap.h"
#include "imgui.h"
#include "../imgui/mg_imgui_panel.h"
#include "../imgui/mg_imgui_icons.h"
#include <filesystem>
#include <vector>

namespace mg {
namespace editor {

const char* TilePalettePanel::GetIcon() const {
    return ICON_FA_TH;
}

static void ShowTilesetImageDialog(char* path_buf, size_t buf_size) {
    auto files = OpenFileDialogMulti("Images", "*.png;*.jpg;*.jpeg;*.bmp");
    if (files.empty()) return;
    std::strncpy(path_buf, files[0].c_str(), (int)(buf_size - 1));
    path_buf[buf_size - 1] = '\0';
}

void TilePalettePanel::Draw() {
    if (!MG_BeginPanel("Tile Palette", ICON_FA_TH, &is_open_,
                       ImGuiWindowFlags_NoDocking)) { MG_EndPanel(); return; }

    const auto& registry = tilesets_.GetRegistry();

    // Tileset 选择下拉框 + New Tileset 按钮
    if (!registry.empty()) {
        std::string current = tilesets_.GetSelectedTileset() ? tilesets_.GetSelectedTileset()->name : "";
        if (ImGui::BeginCombo("##TilesetSelect", current.empty() ? "Select Tileset..." : current.c_str())) {
            for (const auto& [name, ts] : registry) {
                bool selected = (name == current);
                if (ImGui::Selectable(name.c_str(), selected)) {
                    tilesets_.SelectTileset(name);
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    } else {
        ImGui::TextDisabled("No tilesets loaded.");
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_PLUS " New Tileset")) {
        ImGui::OpenPopup("NewTilesetPopup");
        // reset defaults
        std::strncpy(nt_name_, "NewTileset", sizeof(nt_name_));
        nt_name_[sizeof(nt_name_) - 1] = '\0';
        nt_path_[0] = '\0';
        nt_tile_w_ = 32;
        nt_tile_h_ = 32;
        nt_columns_ = 8;
        nt_rows_ = 4;
        nt_error_[0] = '\0';
    }

    // New Tileset Popup
    if (ImGui::BeginPopupModal("NewTilesetPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name", nt_name_, sizeof(nt_name_));
        ImGui::InputText("Image Path", nt_path_, sizeof(nt_path_));
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_FA_FOLDER_OPEN)) {
            ShowTilesetImageDialog(nt_path_, sizeof(nt_path_));
        }
        ImGui::InputInt("Tile W", &nt_tile_w_);
        ImGui::InputInt("Tile H", &nt_tile_h_);
        ImGui::InputInt("Columns", &nt_columns_);
        ImGui::InputInt("Rows", &nt_rows_);

        if (ImGui::Button("Create")) {
            // 复制图片到项目 assets/tilesets/
            std::string dst_dir = project_.GetProjectPath() + "/assets/tilesets";
            std::filesystem::create_directories(dst_dir);
            std::filesystem::path p(nt_path_);
            std::string dst = dst_dir + "/" + p.filename().string();
            try {
                std::filesystem::copy_file(nt_path_, dst, std::filesystem::copy_options::overwrite_existing);
            } catch (...) {
                std::strncpy(nt_error_, "Failed to copy tileset image.", sizeof(nt_error_));
                nt_error_[sizeof(nt_error_) - 1] = '\0';
            }
            std::string rel = "assets/tilesets/" + p.filename().string();
            if (tilesets_.LoadTileset(nt_name_, rel, nt_tile_w_, nt_tile_h_, nt_columns_, nt_rows_)) {
                tilesets_.SelectTileset(nt_name_);
                tilesets_.SaveToProject(project_.GetProjectPath());
                session_.MarkSceneDirty();
                nt_error_[0] = '\0';
                ImGui::CloseCurrentPopup();
            } else {
                std::strncpy(nt_error_, "Failed to load tileset image.", sizeof(nt_error_));
                nt_error_[sizeof(nt_error_) - 1] = '\0';
            }
        }
        if (nt_error_[0] != '\0') {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", nt_error_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    const TilesetAsset* ts = tilesets_.GetSelectedTileset();
    if (!ts || !ts->imgui_ds) {
        MG_EndPanel();
        return;
    }

    ImGui::Separator();

    // Auto-tile toggle
    bool auto_tile = ts->auto_tile;
    if (ImGui::Checkbox("Auto-tile [A]", &auto_tile)) {
        tilesets_.SetAutoTile(ts->name, auto_tile);
    }

    // ---- 瓦片动画编辑 ----
    if (ImGui::TreeNode("Tile Animations")) {
        auto* ts_mut = tilesets_.GetTileset(ts->name);
        int sel = tilesets_.GetSelectedTileIndex();   // tileset 局部索引
        if (ts_mut) {
            bool sel_has_anim = false;
            for (const auto& a : ts_mut->animations) if (a.tile_index == sel) sel_has_anim = true;
            if (sel >= 0 && !sel_has_anim) {
                if (ImGui::SmallButton("Add Animation (selected tile)")) {
                    auto& a = ts_mut->animations.emplace_back();
                    a.tile_index = sel;
                    a.frames.push_back(sel);
                }
            } else if (sel < 0) {
                ImGui::TextDisabled("Select a tile to add animation");
            }
            int remove_idx = -1;
            for (int i = 0; i < (int)ts_mut->animations.size(); ++i) {
                auto& a = ts_mut->animations[i];
                ImGui::PushID(i);
                ImGui::Text("Tile %d (%d frames)", a.tile_index, (int)a.frames.size());
                ImGui::SameLine();
                if (sel >= 0 && ImGui::SmallButton("+Sel Frame")) a.frames.push_back(sel);
                ImGui::SameLine();
                if (ImGui::SmallButton("-Frame") && !a.frames.empty()) a.frames.pop_back();
                ImGui::SameLine();
                ImGui::SetNextItemWidth(70.0f);
                if (ImGui::DragFloat("fps", &a.fps, 0.1f, 0.1f, 60.0f, "%.1f") && a.fps < 0.1f) a.fps = 0.1f;
                ImGui::SameLine();
                if (ImGui::SmallButton("Del")) remove_idx = i;
                ImGui::PopID();
            }
            if (remove_idx >= 0) ts_mut->animations.erase(ts_mut->animations.begin() + remove_idx);
        }
        ImGui::TreePop();
    }

    // Brush mode: tile paint vs passage/collision paint vs terrain tag paint
    int brush_mode = static_cast<int>(tilesets_.GetBrushMode());
    const char* brush_names[] = { "Tiles", "Passage", "Terrain" };
    if (ImGui::Combo("Brush", &brush_mode, brush_names, IM_ARRAYSIZE(brush_names))) {
        tilesets_.SetBrushMode(static_cast<TilesetEditor::Brush>(brush_mode));
    }
    if (tilesets_.GetBrushMode() == TilesetEditor::Brush::Passage) {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.4f, 1.0f), "LMB: paint mask  RMB/Shift+LMB: clear");
        uint8_t mask = tilesets_.GetPassageBrushMask();
        auto toggle = [&](const char* label, uint8_t bit) {
            bool blocked = (mask & bit) != 0;
            if (ImGui::Checkbox(label, &blocked)) {
                if (blocked) mask |= bit;
                else mask &= ~bit;
                tilesets_.SetPassageBrushMask(mask);
            }
        };
        toggle("Up",    mg::scene::Pass_Up);
        ImGui::SameLine();
        toggle("Left",  mg::scene::Pass_Left);
        ImGui::SameLine();
        toggle("Right", mg::scene::Pass_Right);
        ImGui::SameLine();
        toggle("Down",  mg::scene::Pass_Down);
    } else if (tilesets_.GetBrushMode() == TilesetEditor::Brush::Terrain) {
        int tag = tilesets_.GetTerrainBrushTag();
        if (ImGui::InputInt("Tag", &tag)) {
            if (tag < 0) tag = 0;
            if (tag > 255) tag = 255;
            tilesets_.SetTerrainBrushTag(static_cast<uint8_t>(tag));
        }
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "LMB: paint tag  RMB/Shift+LMB: clear");
    }
    ImGui::Separator();

    // 显示 tile 网格
    float tile_display_size = 32.0f;
    float avail_w = ImGui::GetContentRegionAvail().x;
    int cols = (int)(avail_w / (tile_display_size + 4.0f));
    if (cols < 1) cols = 1;

    int selected_idx = tilesets_.GetSelectedTileIndex();
    float tex_w = (float)ts->texture_ptr->GetWidth();
    float tex_h = (float)ts->texture_ptr->GetHeight();

    for (int i = 0; i < ts->tile_count; ++i) {
        int col = i % ts->columns;
        int row = i / ts->columns;
        float u0 = (float)(col * ts->tile_width) / tex_w;
        float v0 = (float)(row * ts->tile_height) / tex_h;
        float u1 = (float)((col + 1) * ts->tile_width) / tex_w;
        float v1 = (float)((row + 1) * ts->tile_height) / tex_h;

        if (i > 0 && i % cols != 0) ImGui::SameLine(0, 4);

        ImGui::PushID(i);
        if (selected_idx == i) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive));
        }
        if (ImGui::ImageButton("", (ImTextureID)ts->imgui_ds,
                               ImVec2(tile_display_size, tile_display_size),
                               ImVec2(u0, v0), ImVec2(u1, v1))) {
            tilesets_.SelectTile(i);
        }
        if (selected_idx == i) {
            ImGui::PopStyleColor();
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    ImGui::Text("Selected: %d", selected_idx);

    // ---- Layer Management ----
    auto* scene = session_.GetActiveScene();
    if (scene && scene->GetWorld()) {
        auto* world = scene->GetWorld();
        scene::Tilemap* tilemap = nullptr;
        uint32_t tme = tilesets_.GetTilemapEntity();
        if (tme != 0) tilemap = world->GetComponent<scene::Tilemap>(tme);
        if (!tilemap) {
            std::vector<scene::EntityID> entities;
            world->GetAllEntities(entities);
            for (auto e : entities) {
                tilemap = world->GetComponent<scene::Tilemap>(e);
                if (tilemap) {
                    tilesets_.SetTilemapEntity(e);
                    break;
                }
            }
        }

        if (tilemap) {
            ImGui::Separator();
            ImGui::Text("Layers");

            int current_layer = tilesets_.GetCurrentTileLayer();
            if (current_layer < 0) current_layer = 0;
            if (current_layer >= (int)tilemap->layers.size()) current_layer = (int)tilemap->layers.size() - 1;
            if (current_layer < 0) current_layer = 0;

            for (int i = 0; i < (int)tilemap->layers.size(); ++i) {
                auto& layer = tilemap->layers[i];
                ImGui::PushID(i);

                // Current-layer selector button
                bool is_current = (i == current_layer);
                ImGui::PushStyleColor(ImGuiCol_Text,
                    is_current ? ImVec4(0.2f, 0.6f, 1.0f, 1.0f) : ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
                if (ImGui::SmallButton(is_current ? ">" : " ")) {
                    tilesets_.SetCurrentTileLayer(i);
                }
                ImGui::PopStyleColor();
                ImGui::SameLine();

                // Visibility toggle
                bool visible = layer.visible;
                if (ImGui::Checkbox("##Visible", &visible)) {
                    layer.visible = visible;
                    tilesets_.MarkTilemapDirty();
                    session_.MarkSceneDirty();
                }
                ImGui::SameLine();

                // Layer name
                char name_buf[64];
                std::strncpy(name_buf, layer.name.c_str(), sizeof(name_buf) - 1);
                name_buf[sizeof(name_buf) - 1] = '\0';
                ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetContentRegionAvail().x - 140.0f));
                if (ImGui::InputText("##Name", name_buf, sizeof(name_buf))) {
                    layer.name = name_buf;
                    session_.MarkSceneDirty();
                }
                ImGui::SameLine();

                // Move up
                ImGui::BeginDisabled(i == 0);
                if (ImGui::SmallButton(ICON_FA_ARROW_UP)) {
                    std::swap(tilemap->layers[i], tilemap->layers[i - 1]);
                    if (current_layer == i) tilesets_.SetCurrentTileLayer(i - 1);
                    else if (current_layer == i - 1) tilesets_.SetCurrentTileLayer(i);
                    tilesets_.MarkTilemapDirty();
                    session_.MarkSceneDirty();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();

                // Move down
                ImGui::BeginDisabled(i == (int)tilemap->layers.size() - 1);
                if (ImGui::SmallButton(ICON_FA_ARROW_DOWN)) {
                    std::swap(tilemap->layers[i], tilemap->layers[i + 1]);
                    if (current_layer == i) tilesets_.SetCurrentTileLayer(i + 1);
                    else if (current_layer == i + 1) tilesets_.SetCurrentTileLayer(i);
                    tilesets_.MarkTilemapDirty();
                    session_.MarkSceneDirty();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();

                // Delete layer
                if (ImGui::SmallButton(ICON_FA_TRASH)) {
                    tilemap->layers.erase(tilemap->layers.begin() + i);
                    if (current_layer >= (int)tilemap->layers.size()) {
                        tilesets_.SetCurrentTileLayer(std::max(0, (int)tilemap->layers.size() - 1));
                    }
                    tilesets_.MarkTilemapDirty();
                    session_.MarkSceneDirty();
                    ImGui::PopID();
                    break;
                }

                ImGui::PopID();
            }

            if (ImGui::Button(ICON_FA_PLUS " Add Layer")) {
                int w = scene->GetWidth() > 0 ? scene->GetWidth() : 20;
                int h = scene->GetHeight() > 0 ? scene->GetHeight() : 15;
                if (!tilemap->layers.empty()) {
                    w = tilemap->layers[0].width;
                    h = tilemap->layers[0].height;
                }
                tilemap->layers.emplace_back("Layer " + std::to_string((int)tilemap->layers.size() + 1), w, h);
                tilesets_.SetCurrentTileLayer((int)tilemap->layers.size() - 1);
                tilesets_.MarkTilemapDirty();
                session_.MarkSceneDirty();
            }
        }
    }

    MG_EndPanel();
}

} // namespace editor
} // namespace mg
