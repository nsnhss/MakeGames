#include "editor_menu_bar.h"
#include "export_game.h"
#include "spritesheet_import.h"
#include "resource/spritesheet_grid.h"
#include "panel.h"
#include "panels/script_editor_panel.h"
#include "editor_context.h"
#include "file_dialog.h"
#include "imgui/mg_imgui_icons.h"
#include "imgui.h"
#include "scene/scene_serializer.h"
#include "scene/map_settings.h"
#include "scene/components/tilemap.h"
#include <filesystem>
#include <fstream>

namespace mg {
namespace editor {

static std::string GetAssetsDir(EditorContext& ctx) {
    std::string dir = ctx.GetProjectPath();
    if (!dir.empty()) dir += "/";
    dir += "assets";
    return dir;
}

static void ShowImportDialog(EditorContext& ctx) {
    auto files = mg::editor::OpenFileDialogMulti("All Files", "*.*");
    if (files.empty()) return;
    std::string assets_dir = GetAssetsDir(ctx);
    if (!std::filesystem::exists(assets_dir)) {
        std::filesystem::create_directories(assets_dir);
    }
    for (const auto& f : files) {
        std::filesystem::path src(f);
        std::filesystem::path dst = std::filesystem::path(assets_dir) / src.filename();
        try {
            std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing);
        } catch (...) {
            // ignore copy errors
        }
    }
}

// 绘制 Editor 顶部菜单栏：File（新建/打开/保存）、Edit（Undo/Redo）、Window（面板显隐/重置布局）、Help
void EditorMenuBar::Draw(EditorContext& ctx, PanelManager& panel_manager) {
    if (ImGui::BeginMenu(ICON_FA_FILE " File")) {
        if (ImGui::MenuItem(ICON_FA_FILE " New Project", "Ctrl+N")) {}
        if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open Project", "Ctrl+O")) {
            auto files = OpenFileDialogMulti("MakeGames Project", "project.json");
            if (!files.empty()) {
                std::string path = files[0];
                // 用户可能选中 project.json，也可能选中项目目录
                if (std::filesystem::path(path).filename() == "project.json") {
                    path = std::filesystem::path(path).parent_path().string();
                }
                std::replace(path.begin(), path.end(), '\\', '/');
                ctx.OpenProject(path);
            }
        }
        if (ImGui::BeginMenu(ICON_FA_FOLDER " Recent Projects")) {
            auto recent = ctx.GetRecentProjects();
            if (recent.empty()) {
                ImGui::MenuItem("No recent projects", nullptr, false, false);
            } else {
                for (const auto& path : recent) {
                    if (ImGui::MenuItem(path.c_str())) {
                        ctx.OpenProject(path);
                    }
                }
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_FILE_IMPORT " Import Assets")) {
            ShowImportDialog(ctx);
        }
        if (ImGui::MenuItem(ICON_FA_IMAGE " Import SpriteSheet...")) {
            show_spritesheet_import_ = true;
            spritesheet_msg_.clear();
        }
        if (ImGui::MenuItem(ICON_FA_BOOK " Database")) {
            if (auto* panel = panel_manager.FindPanel("Item Database")) {
                panel->SetOpen(true);
            }
        }
        if (ImGui::MenuItem(ICON_FA_CODE " Open Script...")) {
            auto files = OpenFileDialogMulti("Lua Scripts", "*.lua");
            if (!files.empty()) {
                if (auto* panel = panel_manager.FindPanel<ScriptEditorPanel>()) {
                    panel->SetOpen(true);
                    panel->OpenFile(files[0]);
                }
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_FILE " New Scene")) {
            show_new_scene_ = true;
            std::strncpy(new_scene_name_, "NewScene", sizeof(new_scene_name_));
        }
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_SAVE " Save", "Ctrl+S")) {
            ctx.SaveCurrentScene();
        }
        if (ImGui::MenuItem(ICON_FA_SAVE " Save All", "Ctrl+Shift+S")) {}
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_FILE_EXPORT " Export Game")) {
            std::string dir = OpenFolderDialog("Select Export Output Folder");
            if (!dir.empty()) {
                if (ExportGame(ctx, dir)) {
                    export_success_ = true;
                } else {
                    export_error_ = true;
                }
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_ARROW_LEFT " Exit to Launcher")) {}
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, ctx.CanUndo())) {
            ctx.Undo();
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, ctx.CanRedo())) {
            ctx.Redo();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Preferences")) {}
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window")) {
        panel_manager.DrawWindowMenu();
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout")) {
            panel_manager.ResetToDefaultLayout();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About")) {}
        if (ImGui::MenuItem("Documentation")) {}
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Scene")) {
        if (ImGui::MenuItem("Scene Properties")) {
            show_scene_properties_ = true;
        }
        ImGui::EndMenu();
    }
}

void EditorMenuBar::RenderPopups(EditorContext& ctx) {
    if (show_scene_properties_) {
        ImGui::OpenPopup("Scene Properties");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Scene Properties", &show_scene_properties_, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto* scene = ctx.GetActiveScene();
        if (scene) {
            char name_buf[64];
            std::strncpy(name_buf, scene->GetName().c_str(), sizeof(name_buf) - 1);
            name_buf[sizeof(name_buf) - 1] = '\0';
            if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
                scene->SetName(name_buf);
                ctx.MarkSceneDirty();
            }

            int w = scene->GetWidth();
            int h = scene->GetHeight();
            int old_w = w, old_h = h;
            if (ImGui::InputInt("Width", &w)) ctx.MarkSceneDirty();
            if (ImGui::InputInt("Height", &h)) ctx.MarkSceneDirty();
            if (w < 1) w = 1;
            if (h < 1) h = 1;
            if (w != old_w || h != old_h) {
                scene->SetSize(w, h);
                scene::ResizeAllTilemaps(scene->GetWorld(), w, h);
                ctx.MarkTilemapDirty();
            }
            int tile_size_now = scene->GetTileSize();
            if (tile_size_now < 1) tile_size_now = 1;
            ImGui::TextDisabled("Pixel Size: %d x %d px", w * tile_size_now, h * tile_size_now);

            int ts = scene->GetTileSize();
            if (ImGui::InputInt("Tile Size", &ts)) {
                if (ts < 1) ts = 1;
                scene->SetTileSize(ts);
                scene::SyncAllTilemapTileSize(scene->GetWorld(), ts);
                ctx.MarkTilemapDirty();
                ctx.MarkSceneDirty();
            }

            char bgm_buf[256];
            std::strncpy(bgm_buf, scene->GetBGMPath().c_str(), sizeof(bgm_buf) - 1);
            bgm_buf[sizeof(bgm_buf) - 1] = '\0';
            if (ImGui::InputText("BGM Path", bgm_buf, sizeof(bgm_buf))) {
                scene->SetBGMPath(bgm_buf);
                ctx.MarkSceneDirty();
            }

            auto bgc = scene->GetBackgroundColor();
            float col[4] = {bgc.r, bgc.g, bgc.b, bgc.a};
            if (ImGui::ColorEdit4("Background Color", col)) {
                scene->SetBackgroundColor(math::Color(col[0], col[1], col[2], col[3]));
                ctx.MarkSceneDirty();
            }
        }
        if (ImGui::Button("Close")) {
            show_scene_properties_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (show_new_scene_) {
        ImGui::OpenPopup("New Scene");
        new_scene_error_ = false;
    }
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("New Scene", &show_new_scene_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Scene Name", new_scene_name_, sizeof(new_scene_name_));
        ImGui::InputInt("Width", &new_scene_width_);
        ImGui::InputInt("Height", &new_scene_height_);
        ImGui::InputInt("Tile Size", &new_scene_tile_size_);
        if (new_scene_width_ < 1) new_scene_width_ = 1;
        if (new_scene_height_ < 1) new_scene_height_ = 1;
        if (new_scene_tile_size_ < 1) new_scene_tile_size_ = 1;
        if (new_scene_error_) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Invalid or duplicate scene name.");
        }
        if (ImGui::Button("Create")) {
            std::string name = new_scene_name_;
            // trim whitespace
            size_t a = name.find_first_not_of(" \t\r\n");
            size_t b = name.find_last_not_of(" \t\r\n");
            if (a == std::string::npos) name.clear();
            else name = name.substr(a, b - a + 1);

            bool ok = true;
            if (name.empty()) ok = false;
            else if (name.find_first_of("\\/:*?\"<>|") != std::string::npos) ok = false;
            else if (std::filesystem::exists(ctx.GetProjectPath() + "/maps/" + name + ".json")) ok = false;

            new_scene_error_ = !ok;
            if (ok) {
                if (ctx.CreateAndSaveNewScene(name, new_scene_width_, new_scene_height_, new_scene_tile_size_)) {
                    show_new_scene_ = false;
                    ImGui::CloseCurrentPopup();
                } else {
                    new_scene_error_ = true;
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            show_new_scene_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Export Game 结果弹窗
    if (export_success_) {
        ImGui::OpenPopup("Export Success");
        export_success_ = false;
    }
    if (export_error_) {
        ImGui::OpenPopup("Export Failed");
        export_error_ = false;
    }

    // ---- SpriteSheet 导入向导 ----
    if (show_spritesheet_import_) {
        ImGui::OpenPopup("Import SpriteSheet");
    }
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Import SpriteSheet", &show_spritesheet_import_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Image", spritesheet_path_, sizeof(spritesheet_path_));
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            auto files = OpenFileDialogMulti("Images", "*.png;*.jpg;*.jpeg;*.bmp");
            if (!files.empty()) {
                std::strncpy(spritesheet_path_, files[0].c_str(), sizeof(spritesheet_path_) - 1);
                spritesheet_path_[sizeof(spritesheet_path_) - 1] = '\0';
            }
        }
        ImGui::InputInt("Frame Width", &spritesheet_frame_w_);
        ImGui::InputInt("Frame Height", &spritesheet_frame_h_);
        ImGui::InputFloat("FPS", &spritesheet_fps_, 0.5f, 1.0f, "%.1f");

        // 图片尺寸与切分预览
        int img_w = 0, img_h = 0;
        if (spritesheet_path_[0] != '\0' && GetImageSize(spritesheet_path_, img_w, img_h)) {
            auto grid = ComputeSheetGrid(img_w, img_h, spritesheet_frame_w_, spritesheet_frame_h_);
            ImGui::Text("Image: %d x %d  ->  %d x %d = %d frames", img_w, img_h,
                        grid.columns, grid.rows, grid.frame_count);
        }

        if (!spritesheet_msg_.empty()) {
            bool is_error = spritesheet_msg_.rfind("Prefab created:", 0) != 0;
            ImGui::TextColored(is_error ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f) : ImVec4(0.4f, 0.9f, 0.4f, 1.0f),
                               "%s", spritesheet_msg_.c_str());
        }

        if (ImGui::Button("Import")) {
            std::string prefab_name, err;
            if (ImportSpriteSheetAsPrefab(ctx.GetProjectPath(), spritesheet_path_,
                                          spritesheet_frame_w_, spritesheet_frame_h_,
                                          spritesheet_fps_, prefab_name, err)) {
                spritesheet_msg_ = "Prefab created: " + prefab_name;
            } else {
                spritesheet_msg_ = err;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Close")) {
            show_spritesheet_import_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Export Success", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Game exported successfully.");
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Export Failed", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Failed to export game. Check output directory permissions.");
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

} // namespace editor
} // namespace mg
