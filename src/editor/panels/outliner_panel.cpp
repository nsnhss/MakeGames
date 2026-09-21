#include "outliner_panel.h"
#include "src/editor/selection_manager.h"
#include "src/editor/scene_session.h"
#include "src/editor/editor_project_manager.h"
#include "src/editor/command_history.h"
#include "src/editor/prefab_manager.h"
#include "src/editor/viewport_state.h"
#include "src/editor/panel.h"
#include "resource/schema/project_schema.h"
#include "scene/scene.h"
#include "scene/world.h"
#include "scene/components/name.h"
#include "scene/components/tag.h"
#include "scene/components/transform.h"
#include "scene/components/prefab_ref.h"
#include "src/editor/imgui/mg_imgui_icons.h"
#include "src/editor/imgui/mg_imgui_panel.h"
#include "src/editor/imgui/mg_imgui_extensions.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include <cstring>

namespace mg {
namespace editor {

const char* OutlinerPanel::GetIcon() const {
    return ICON_FA_LIST;
}

void OutlinerPanel::Draw() {
    if (!MG_BeginPanel("Scene Outliner", ICON_FA_LIST, &is_open_)) { MG_EndPanel(); return; }


    // 场景下拉框
    {
        std::string current_label = "<No Scene>";
        auto* scene = session_.GetActiveScene();
        if (scene) {
            current_label = scene->GetName();
            if (session_.IsSceneDirty()) current_label += "*";
        }

        ImGui::SetNextItemWidth(-FLT_MIN);
        bool open_new_scene_popup = false;
        if (ImGui::BeginCombo("##SceneSelector", current_label.c_str())) {
            auto mi = project_.LoadMapInfos();
            for (const auto& e : mi.entries) {
                std::string display = e.name;
                if (display.size() > 5 && display.substr(display.size() - 5) == ".json") {
                    display = display.substr(0, display.size() - 5);
                }
                bool is_selected = (scene && scene->GetName() == display);
                if (ImGui::Selectable(display.c_str(), is_selected)) {
                    session_.SwitchScene(e.name);
                }
                if (is_selected) ImGui::SetItemDefaultFocus();

                // 记录右键点击目标，Combo 关闭后再打开上下文菜单
                if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                    scene_context_target_ = e.name;
                    open_scene_context_ = true;
                }
            }
            ImGui::Separator();
            if (ImGui::Selectable(ICON_FA_PLUS " New Scene", false)) {
                open_new_scene_popup = true;
            }
            ImGui::EndCombo();
        }

        // 所有弹窗共用中心位置
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();

        // 场景上下文菜单（在 Combo 外部打开，避免嵌套弹窗）
        if (open_scene_context_) {
            ImGui::OpenPopup("SceneContextMenu");
            open_scene_context_ = false;
        }
        if (ImGui::BeginPopup("SceneContextMenu")) {
            std::string display = scene_context_target_;
            if (display.size() > 5 && display.substr(display.size() - 5) == ".json") {
                display = display.substr(0, display.size() - 5);
            }
            ImGui::Text("%s", display.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_PEN " Rename")) {
                scene_rename_old_ = scene_context_target_;
                scene_rename_new_[0] = '\0';
                if (scene_rename_new_.size() > 1) {
                    std::strncpy(scene_rename_new_.data(), display.c_str(), scene_rename_new_.size() - 1);
                    scene_rename_new_[scene_rename_new_.size() - 1] = '\0';
                }
                show_scene_rename_ = true;
            }
            if (ImGui::MenuItem(ICON_FA_TRASH " Delete")) {
                scene_delete_target_ = scene_context_target_;
                show_scene_delete_confirm_ = true;
            }
            ImGui::EndPopup();
        }

        // Outliner 内新建场景弹窗
        if (open_new_scene_popup) {
            ImGui::OpenPopup("NewSceneFromOutliner");
        }
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("NewSceneFromOutliner", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("Name", new_scene_from_outliner_, sizeof(new_scene_from_outliner_));
            ImGui::InputInt("Width", &new_scene_width_);
            ImGui::InputInt("Height", &new_scene_height_);
            ImGui::InputInt("Tile Size", &new_scene_tile_size_);
            if (new_scene_width_ < 1) new_scene_width_ = 1;
            if (new_scene_height_ < 1) new_scene_height_ = 1;
            if (new_scene_tile_size_ < 1) new_scene_tile_size_ = 1;
            if (ImGui::Button("Create")) {
                session_.CreateAndSaveNewScene(new_scene_from_outliner_,
                    new_scene_width_, new_scene_height_, new_scene_tile_size_);
                new_scene_from_outliner_[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                new_scene_from_outliner_[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // 重命名弹窗
        if (show_scene_rename_) {
            ImGui::OpenPopup("Rename Scene");
        }
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Rename Scene", &show_scene_rename_, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("New Name", scene_rename_new_.data(), scene_rename_new_.size());
            if (ImGui::Button("OK")) {
                session_.RenameScene(scene_rename_old_, scene_rename_new_.data());
                show_scene_rename_ = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                show_scene_rename_ = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // 删除确认弹窗
        if (show_scene_delete_confirm_) {
            ImGui::OpenPopup("Delete Scene?");
        }
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Delete Scene?", &show_scene_delete_confirm_, ImGuiWindowFlags_AlwaysAutoResize)) {
            std::string display = scene_delete_target_;
            if (display.size() > 5 && display.substr(display.size() - 5) == ".json") {
                display = display.substr(0, display.size() - 5);
            }
            ImGui::Text("Are you sure you want to delete '%s'?", display.c_str());
            if (ImGui::Button("Delete")) {
                session_.DeleteScene(scene_delete_target_);
                show_scene_delete_confirm_ = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                show_scene_delete_confirm_ = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

    }

    ImGui::Separator();

    // 搜索框
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##OutlinerSearch", ICON_FA_MAGNIFYING_GLASS " Search...",
            search_buffer_, sizeof(search_buffer_),
            ImGuiInputTextFlags_CallbackAlways,
            ImGui_ImplSDL2_InputTextCallback)) {
        // 过滤逻辑预留
    }
    ImGui_ImplSDL2_RenderTextInputIME(search_buffer_, ImGui::GetColorU32(ImGuiCol_TextDisabled));
    ImGui::Separator();

    auto* scene = session_.GetActiveScene();

    // 右键菜单
    if (ImGui::BeginPopupContextWindow()) {
        if (ImGui::BeginMenu(ICON_FA_PLUS " Create")) {
            auto create = [&](SceneSession::EntityTemplate type, const char* label) {
                if (ImGui::MenuItem(label)) {
                    session_.CreateEntityFromTemplateAtCameraCenter(type);
                }
            };
            create(SceneSession::EntityTemplate::Empty,     "Entity");
            create(SceneSession::EntityTemplate::NPC,        "NPC");
            create(SceneSession::EntityTemplate::Monster,    "Monster");
            create(SceneSession::EntityTemplate::Chest,      "Chest");
            create(SceneSession::EntityTemplate::Transfer,   "Transfer");
            create(SceneSession::EntityTemplate::Shop,       "Shop");
            create(SceneSession::EntityTemplate::Decoration, "Decoration");
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_COPY " Duplicate") && selection_.GetType() == SelectionManager::Type::Entity) {
            if (scene && scene->GetWorld()) {
                auto* world = scene->GetWorld();
                auto dup = world->CloneEntity(selection_.GetSelectedEntity());
                if (auto* t = world->GetComponent<scene::Transform>(dup)) t->position.x += 16.0f;
                commands_.Execute(std::make_unique<CreateEntityCommand>(world, dup));
                selection_.SelectEntity(dup);
                session_.MarkSceneDirty();
            }
        }
        if (ImGui::MenuItem(ICON_FA_TRASH " Delete") && selection_.GetType() == SelectionManager::Type::Entity) {
            if (scene && scene->GetWorld()) {
                auto* world = scene->GetWorld();
                commands_.Execute(std::make_unique<DeleteEntityCommand>(world, selection_.GetSelectedEntity()));
                selection_.Clear();
            }
        }
        if (ImGui::MenuItem(ICON_FA_PEN " Rename")) {
            // 预留
        }
        ImGui::EndPopup();
    }

    // 场景树
    if (scene && scene->GetWorld()) {
        auto* world = scene->GetWorld();
        std::vector<scene::EntityID> entities;
        world->GetAllEntities(entities);

        if (MG_TreeNodeEx("Root", ICON_FA_FOLDER, ImGuiTreeNodeFlags_DefaultOpen)) {
            // R5-5c：缩进引导线（半透明竖线连接 Root 与子层级）
            const float guide_x = ImGui::GetItemRectMin().x + ImGui::GetStyle().IndentSpacing * 0.5f;
            const float guide_y0 = ImGui::GetItemRectMax().y;
            float guide_y1 = guide_y0;
            for (scene::EntityID entity : entities) {
                auto* name_comp = world->GetComponent<scene::Name>(entity);
                auto* tag_comp = world->GetComponent<scene::Tag>(entity);

                const char* icon = ICON_FA_CUBE;
                if (tag_comp) {
                    if (tag_comp->Has("camera")) icon = ICON_FA_CAMERA;
                    else if (tag_comp->Has("light")) icon = ICON_FA_LIGHTBULB;
                    else if (tag_comp->Has("player")) icon = ICON_FA_CUBE;
                }

                std::string label;
                if (name_comp && !name_comp->value.empty()) {
                    label = name_comp->value;
                } else {
                    label = "Entity #" + std::to_string(entity);
                }
                // Prefab 实例标记
                if (world->GetComponent<scene::PrefabRef>(entity)) label += " [P]";
                label += " ##" + std::to_string(entity);

                bool selected = (selection_.GetType() == SelectionManager::Type::Entity
                                 && selection_.GetSelectedEntity() == entity);

                MG_TreeNodeExLeaf(label.c_str(), icon, selected);
                guide_y1 = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
                if (ImGui::IsItemClicked()) {
                    selection_.SelectEntity(entity);
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    selection_.SelectEntity(entity);
                    if (auto* p = panel_manager_->FindPanel("Scene Explorer")) p->SetOpen(true);
                }

                // 右键菜单（单个实体）
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::IsWindowAppearing()) {
                        selection_.SelectEntity(entity);
                    }
                    if (ImGui::MenuItem(ICON_FA_TRASH " Delete")) {
                        commands_.Execute(std::make_unique<DeleteEntityCommand>(world, entity));
                        if (selection_.GetType() == SelectionManager::Type::Entity
                            && selection_.GetSelectedEntity() == entity) {
                            selection_.Clear();
                        }
                        session_.MarkSceneDirty();
                    }
                    if (ImGui::MenuItem(ICON_FA_COPY " Duplicate")) {
                        auto dup = world->CloneEntity(entity);
                        if (auto* t = world->GetComponent<scene::Transform>(dup)) t->position.x += 16.0f;
                        commands_.Execute(std::make_unique<CreateEntityCommand>(world, dup));
                        selection_.SelectEntity(dup);
                        session_.MarkSceneDirty();
                    }
                    if (ImGui::MenuItem(ICON_FA_LIGHTBULB " Edit Events")) {
                        if (auto* p = panel_manager_->FindPanel("Event Editor")) p->SetOpen(true);
                    }
                    if (ImGui::MenuItem(ICON_FA_SAVE " Save as Prefab")) {
                        prefab_source_ = entity;
                        prefab_name_[0] = '\0';
                        show_prefab_save_ = true;
                    }
                    // Prefab 活链接操作（仅实例）
                    if (auto* prefab_ref = world->GetComponent<scene::PrefabRef>(entity)) {
                        ImGui::Separator();
                        if (ImGui::MenuItem(ICON_FA_UPLOAD " Apply to Prefab")) {
                            prefabs_.ApplyToPrefab(project_.GetProjectPath(), world, entity);
                        }
                        if (ImGui::MenuItem(ICON_FA_ROTATE_LEFT " Revert to Prefab")) {
                            nlohmann::json after;
                            if (prefabs_.BuildRevertSnapshot(world, project_.GetProjectPath(), entity, after)) {
                                commands_.Execute(std::make_unique<UpdateEntityCommand>(world, entity, std::move(after)));
                                selection_.SelectEntity(entity);
                                session_.MarkSceneDirty();
                            }
                        }
                        if (ImGui::MenuItem(ICON_FA_UNLINK " Unlink Prefab")) {
                            world->RemoveComponent<scene::PrefabRef>(entity);
                            session_.MarkSceneDirty();
                        }
                    }
                    ImGui::EndPopup();
                }
            }
            // 画缩进引导线（Root 底部 → 最后一个子项行中）
            if (guide_y1 > guide_y0) {
                ImGui::GetWindowDrawList()->AddLine(
                    ImVec2(guide_x, guide_y0), ImVec2(guide_x, guide_y1),
                    ImGui::GetColorU32(ImGuiCol_TextDisabled), 1.0f);
            }
            ImGui::TreePop();
        }
    } else {
        ImGui::TextDisabled("No active scene.");
    }

    // Save as Prefab 弹窗（上下文菜单外打开，避免嵌套弹窗）
    if (show_prefab_save_) {
        ImGui::OpenPopup("Save as Prefab");
    }
    ImVec2 prefab_center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(prefab_center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Save as Prefab", &show_prefab_save_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Prefab Name", prefab_name_, sizeof(prefab_name_));
        if (ImGui::Button("Save")) {
            if (scene && scene->GetWorld() && prefab_name_[0] != '\0') {
                prefabs_.SavePrefab(project_.GetProjectPath(), scene->GetWorld(), prefab_source_, prefab_name_);
            }
            show_prefab_save_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            show_prefab_save_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Prefabs 列表：点击实例化到视口相机中心
    ImGui::Separator();
    if (ImGui::CollapsingHeader(ICON_FA_CUBE " Prefabs")) {
        auto prefab_list = prefabs_.ListPrefabs(project_.GetProjectPath());
        if (prefab_list.empty()) {
            ImGui::TextDisabled("No prefabs. Right-click an entity → Save as Prefab.");
        }
        for (const auto& pname : prefab_list) {
            if (ImGui::Selectable(pname.c_str()) && scene && scene->GetWorld()) {
                auto* world = scene->GetWorld();
                auto inst = prefabs_.Instantiate(world, project_.GetProjectPath(), pname,
                                                 viewport_.GetCameraWorldCenter(world));
                if (inst != 0) {
                    commands_.Execute(std::make_unique<CreateEntityCommand>(world, inst));
                    selection_.SelectEntity(inst);
                    session_.MarkSceneDirty();
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Click to instantiate at camera center");
            }
        }
    }

    MG_EndPanel();
}

} // namespace editor
} // namespace mg
