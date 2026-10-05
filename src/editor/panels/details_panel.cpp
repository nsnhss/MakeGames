#include "details_panel.h"
#include "src/editor/selection_manager.h"
#include "src/editor/scene_session.h"
#include "src/editor/command_history.h"
#include "src/editor/file_dialog.h"
#include "scene/scene.h"
#include "scene/world.h"
#include "scene/component.h"
#include "scene/components/transform.h"
#include "scene/components/name.h"
#include "scene/components/tag.h"
#include "scene/components/sprite.h"
#include "scene/components/camera.h"
#include "scene/components/camera_follow.h"
#include "scene/components/event_trigger.h"
#include "scene/components/animator.h"
#include "src/editor/imgui/mg_imgui_icons.h"
#include "src/editor/imgui/mg_imgui_extensions.h"
#include "src/editor/imgui/mg_imgui_panel.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include <cstring>

// Details Undo（D4b）：
// - 属性编辑：无挂起编辑时捕获帧前快照；连续帧变化合并为一段，首个无变化帧
//   结算为单条 UpdateEntityCommand（before=段首快照，after=当前状态）。
// - Add/Remove Component：立即型 before/after 快照各包一条 UpdateEntityCommand。
// - 结算仅在挂起编辑仍属于当前活动场景时入栈；场景切换/实体删除则丢弃
//   （旧场景切换前已保存，删除实体由 DeleteEntityCommand 自身快照覆盖）。

namespace mg {
namespace editor {

static bool DrawTransformInspector(SceneSession& session_, scene::World* world, scene::EntityID entity) {
    auto* comp = world->GetComponent<scene::Transform>(entity);
    if (!comp) return false;

    bool changed = false;
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (MG_PropertyInputFloat2("Position", &comp->position.x, "%.2f")) { session_.MarkSceneDirty(); changed = true; }
        if (MG_PropertyInputFloat("Rotation", &comp->rotation, 1.0f, "%.2f")) { session_.MarkSceneDirty(); changed = true; }
        if (MG_PropertyInputFloat2("Scale", &comp->scale.x, "%.2f")) { session_.MarkSceneDirty(); changed = true; }
        if (MG_PropertyInputInt("Sorting Order", &comp->sorting_order)) { session_.MarkSceneDirty(); changed = true; }
    }
    return changed;
}

static bool DrawNameInspector(SceneSession& session_, scene::World* world, scene::EntityID entity) {
    auto* comp = world->GetComponent<scene::Name>(entity);
    if (!comp) return false;

    bool changed = false;
    if (ImGui::CollapsingHeader("Name", ImGuiTreeNodeFlags_DefaultOpen)) {
        char buf[256];
        std::strncpy(buf, comp->value.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText("##Name", buf, sizeof(buf),
                ImGuiInputTextFlags_CallbackAlways,
                ImGui_ImplSDL2_InputTextCallback)) {
            comp->value = buf;
            session_.MarkSceneDirty();
            changed = true;
        }
        ImGui_ImplSDL2_RenderTextInputIME(buf, ImGui::GetColorU32(ImGuiCol_CheckMark));
    }
    return changed;
}

static bool DrawTagInspector(SceneSession& session_, scene::World* world, scene::EntityID entity) {
    auto* comp = world->GetComponent<scene::Tag>(entity);
    if (!comp) return false;

    bool changed = false;
    if (ImGui::CollapsingHeader("Tag", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (size_t i = 0; i < comp->tags.size(); ++i) {
            ImGui::BulletText("%s", comp->tags[i].c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton((std::string("x##") + std::to_string(i)).c_str())) {
                comp->tags.erase(comp->tags.begin() + i);
                --i;
                session_.MarkSceneDirty();
                changed = true;
            }
        }
        static char new_tag[64] = {};
        ImGui::InputText("##NewTag", new_tag, sizeof(new_tag),
            ImGuiInputTextFlags_CallbackAlways,
            ImGui_ImplSDL2_InputTextCallback);
        ImGui_ImplSDL2_RenderTextInputIME(new_tag, ImGui::GetColorU32(ImGuiCol_CheckMark));
        ImGui::SameLine();
        if (ImGui::Button("Add Tag") && new_tag[0] != '\0') {
            comp->Add(new_tag);
            new_tag[0] = '\0';
            session_.MarkSceneDirty();
            changed = true;
        }
    }
    return changed;
}

static bool ShowSpriteTextureDialog(SceneSession& session_, scene::Sprite* comp) {
    auto files = mg::editor::OpenFileDialogMulti("Images", "*.png;*.jpg;*.jpeg;*.bmp");
    if (files.empty()) return false;
    std::filesystem::path p(files[0]);
    std::string rel = "assets/" + p.filename().string();
    comp->texture_path = rel;
    session_.MarkSceneDirty();
    return true;
}

static bool DrawSpriteInspector(SceneSession& session_, scene::World* world, scene::EntityID entity) {
    auto* comp = world->GetComponent<scene::Sprite>(entity);
    if (!comp) return false;

    bool changed = false;
    if (ImGui::CollapsingHeader("Sprite", ImGuiTreeNodeFlags_DefaultOpen)) {
        char buf[256];
        std::strncpy(buf, comp->texture_path.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText("Texture", buf, sizeof(buf),
                ImGuiInputTextFlags_CallbackAlways,
                ImGui_ImplSDL2_InputTextCallback)) {
            comp->texture_path = buf;
            session_.MarkSceneDirty();
            changed = true;
        }
        ImGui_ImplSDL2_RenderTextInputIME(buf, ImGui::GetColorU32(ImGuiCol_CheckMark));
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_FA_FOLDER_OPEN)) {
            if (ShowSpriteTextureDialog(session_, comp)) changed = true;
        }
        if (MG_PropertyInputFloat2("Pivot", &comp->pivot.x, "%.2f")) { session_.MarkSceneDirty(); changed = true; }
        if (MG_PropertyColorEdit4("Tint", &comp->tint.r)) { session_.MarkSceneDirty(); changed = true; }
        if (MG_PropertyCheckbox("Flip X", &comp->flip_x)) { session_.MarkSceneDirty(); changed = true; }
        if (MG_PropertyCheckbox("Flip Y", &comp->flip_y)) { session_.MarkSceneDirty(); changed = true; }
        if (MG_PropertyCheckbox("Y-Sort", &comp->y_sort)) { session_.MarkSceneDirty(); changed = true; }
    }
    return changed;
}

static bool DrawCameraInspector(SceneSession& session_, scene::World* world, scene::EntityID entity) {
    auto* comp = world->GetComponent<scene::Camera>(entity);
    if (!comp) return false;

    bool changed = false;
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        int proj = (comp->projection == scene::Camera::Projection::Orthographic) ? 0 : 1;
        const char* items[] = { "Orthographic", "Perspective" };
        if (ImGui::Combo("Projection", &proj, items, 2)) {
            comp->projection = (proj == 1) ? scene::Camera::Projection::Perspective : scene::Camera::Projection::Orthographic;
            session_.MarkSceneDirty();
            changed = true;
        }
        if (MG_PropertyInputFloat("Ortho Size", &comp->ortho_size, 0.1f, "%.2f")) { session_.MarkSceneDirty(); changed = true; }
        if (MG_PropertyCheckbox("Primary", &comp->is_primary)) { session_.MarkSceneDirty(); changed = true; }

        MG_SectionHeader("Follow");
        if (MG_PropertyInputFloat("Follow Speed", &comp->follow_speed, 0.1f, "%.2f")) { session_.MarkSceneDirty(); changed = true; }
        if (MG_PropertyInputFloat2("Dead Zone", &comp->dead_zone.x, "%.1f")) { session_.MarkSceneDirty(); changed = true; }
        if (MG_PropertyCheckbox("Smooth Follow", &comp->smooth_follow)) { session_.MarkSceneDirty(); changed = true; }
    }
    return changed;
}

static bool DrawAnimatorInspector(SceneSession& session_, scene::World* world, scene::EntityID entity) {
    auto* comp = world->GetComponent<scene::Animator>(entity);
    if (!comp) return false;

    bool changed = false;
    if (ImGui::CollapsingHeader("Animator", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (MG_PropertyInputInt("Frame Count", &comp->frame_count)) { session_.MarkSceneDirty(); changed = true; }
        if (MG_PropertyInputInt("Columns", &comp->columns)) { session_.MarkSceneDirty(); changed = true; }
        if (MG_PropertyInputFloat("FPS", &comp->fps, 0.1f, "%.1f")) { session_.MarkSceneDirty(); changed = true; }
        if (MG_PropertyCheckbox("Playing", &comp->playing)) { session_.MarkSceneDirty(); changed = true; }
        ImGui::TextDisabled("Current Frame: %d", comp->current_frame);
    }
    return changed;
}

static void DrawCameraFollowTargetInspector(scene::World* world, scene::EntityID entity) {
    auto* comp = world->GetComponent<scene::CameraFollowTarget>(entity);
    if (!comp) return;

    if (ImGui::CollapsingHeader("Camera Follow Target", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("This entity is a camera follow target.");
        ImGui::TextDisabled("A Camera component on another entity can follow it.");
    }
}

void DetailsPanel::FlushPendingEdit() {
    if (!edit_pending_) return;
    // 仅当挂起编辑仍属于当前活动场景时才入栈；场景切换/实体删除则丢弃
    if (edit_world_ && edit_world_ == session_.GetWorld() &&
        edit_world_->IsValid(edit_entity_)) {
        if (auto* history = session_.GetCommandHistory()) {
            history->Execute(std::make_unique<UpdateEntityCommand>(
                edit_world_, edit_entity_, std::move(edit_before_),
                edit_world_->SnapshotEntity(edit_entity_)));
        }
    }
    edit_pending_ = false;
    edit_world_ = nullptr;
    edit_entity_ = 0;
    edit_before_.clear();
}

void DetailsPanel::PushSnapshotCommand(scene::World* world, scene::EntityID entity,
                                       nlohmann::json before) {
    if (auto* history = session_.GetCommandHistory()) {
        history->Execute(std::make_unique<UpdateEntityCommand>(
            world, entity, std::move(before), world->SnapshotEntity(entity)));
    }
}

void DetailsPanel::Draw() {

    // 动态标题：选中实体时显示实体名
    std::string window_title = "Scene Explorer";
    if (selection_.GetType() == SelectionManager::Type::Entity) {
        auto* scene = session_.GetActiveScene();
        if (scene && scene->GetWorld()) {
            auto* name_comp = scene->GetWorld()->GetComponent<scene::Name>(selection_.GetSelectedEntity());
            if (name_comp && !name_comp->value.empty()) {
                window_title += ": " + name_comp->value;
            }
        }
    }

    if (!MG_BeginPanel(window_title.c_str(), ICON_FA_SLIDERS_H, &is_open_,
                       ImGuiWindowFlags_NoDocking)) { MG_EndPanel(); return; }

    if (selection_.GetType() == SelectionManager::Type::None) {
        FlushPendingEdit();
        ImGui::TextDisabled("No object selected.");
        ImGui::TextDisabled("Select an entity from the Outliner or Viewport.");
        MG_EndPanel();
        return;
    }

    if (selection_.GetType() == SelectionManager::Type::Asset) {
        FlushPendingEdit();
        ImGui::Text(ICON_FA_FILE " %s", selection_.GetSelectedAssetPath().c_str());
        MG_SectionHeader("Asset");
        ImGui::TextDisabled("Asset inspector not yet implemented.");
        MG_EndPanel();
        return;
    }

    // Entity 属性编辑
    if (selection_.GetType() == SelectionManager::Type::Entity) {
        auto entity = selection_.GetSelectedEntity();
        auto* scene = session_.GetActiveScene();
        if (!scene || !scene->GetWorld()) {
            FlushPendingEdit();
            ImGui::TextDisabled("No active scene.");
            MG_EndPanel();
            return;
        }
        auto* world = scene->GetWorld();

        if (!world->IsValid(entity)) {
            FlushPendingEdit();
            ImGui::TextDisabled("Selected entity is no longer valid.");
            MG_EndPanel();
            return;
        }

        // 选中实体/场景变化 → 结算上一段挂起编辑（针对旧实体）
        if (edit_pending_ && (edit_world_ != world || edit_entity_ != entity))
            FlushPendingEdit();

        auto* name_comp = world->GetComponent<scene::Name>(entity);
        const char* display_name = name_comp ? name_comp->value.c_str() : "Unnamed";
        ImGui::Text(ICON_FA_CUBE " %s (#%u)", display_name, entity);
        MG_SectionHeader("Properties");

        // Details Undo：无挂起编辑时捕获帧前快照（连续编辑段的 before）
        nlohmann::json frame_before;
        if (!edit_pending_) frame_before = world->SnapshotEntity(entity);

        // Component Inspectors
        bool changed = false;
        changed |= DrawTransformInspector(session_, world, entity);
        changed |= DrawNameInspector(session_, world, entity);
        changed |= DrawTagInspector(session_, world, entity);
        changed |= DrawSpriteInspector(session_, world, entity);
        changed |= DrawCameraInspector(session_, world, entity);
        DrawCameraFollowTargetInspector(world, entity);
        changed |= DrawAnimatorInspector(session_, world, entity);

        if (changed) {
            if (!edit_pending_) {
                edit_pending_ = true;
                edit_world_ = world;
                edit_entity_ = entity;
                edit_before_ = std::move(frame_before);
            }
        } else {
            // 本帧无变化 → 结算上一段连续编辑
            FlushPendingEdit();
        }

        // 立即型组件变更：before/after 快照包一条 UpdateEntityCommand
        auto component_edit_with_undo = [&](auto edit_fn) {
            FlushPendingEdit();
            auto before = world->SnapshotEntity(entity);
            edit_fn();
            session_.MarkSceneDirty();
            PushSnapshotCommand(world, entity, std::move(before));
        };

        // Add Component
        MG_SectionHeader("Components");
        if (ImGui::Button(ICON_FA_PLUS " Add Component")) {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup")) {
            auto add_if_missing = [&](const char* label, auto add_fn) {
                if (ImGui::MenuItem(label)) {
                    component_edit_with_undo(add_fn);
                    ImGui::CloseCurrentPopup();
                }
            };

            if (!world->HasComponent<scene::Transform>(entity))
                add_if_missing("Transform", [&]() { world->AddComponent<scene::Transform>(entity); });
            if (!world->HasComponent<scene::Name>(entity))
                add_if_missing("Name", [&]() { world->AddComponent<scene::Name>(entity); });
            if (!world->HasComponent<scene::Tag>(entity))
                add_if_missing("Tag", [&]() { world->AddComponent<scene::Tag>(entity); });
            if (!world->HasComponent<scene::Sprite>(entity))
                add_if_missing("Sprite", [&]() { world->AddComponent<scene::Sprite>(entity); });
            if (!world->HasComponent<scene::Camera>(entity))
                add_if_missing("Camera", [&]() { world->AddComponent<scene::Camera>(entity); });
            if (!world->HasComponent<scene::CameraFollowTarget>(entity))
                add_if_missing("Camera Follow Target", [&]() { world->AddComponent<scene::CameraFollowTarget>(entity); });
            if (!world->HasComponent<scene::EventTrigger>(entity))
                add_if_missing("Event Trigger", [&]() { world->AddComponent<scene::EventTrigger>(entity); });
            if (!world->HasComponent<scene::Animator>(entity))
                add_if_missing("Animator", [&]() { world->AddComponent<scene::Animator>(entity); });

            ImGui::EndPopup();
        }

        // Remove Component buttons
        ImGui::Spacing();
        if (world->HasComponent<scene::Transform>(entity)) {
            if (ImGui::Button("Remove Transform")) component_edit_with_undo([&]() { world->RemoveComponent<scene::Transform>(entity); });
        }
        if (world->HasComponent<scene::Name>(entity)) {
            if (ImGui::Button("Remove Name")) component_edit_with_undo([&]() { world->RemoveComponent<scene::Name>(entity); });
        }
        if (world->HasComponent<scene::Tag>(entity)) {
            if (ImGui::Button("Remove Tag")) component_edit_with_undo([&]() { world->RemoveComponent<scene::Tag>(entity); });
        }
        if (world->HasComponent<scene::Sprite>(entity)) {
            if (ImGui::Button("Remove Sprite")) component_edit_with_undo([&]() { world->RemoveComponent<scene::Sprite>(entity); });
        }
        if (world->HasComponent<scene::Camera>(entity)) {
            if (ImGui::Button("Remove Camera")) component_edit_with_undo([&]() { world->RemoveComponent<scene::Camera>(entity); });
        }
        if (world->HasComponent<scene::CameraFollowTarget>(entity)) {
            if (ImGui::Button("Remove Camera Follow Target")) component_edit_with_undo([&]() { world->RemoveComponent<scene::CameraFollowTarget>(entity); });
        }
        if (world->HasComponent<scene::EventTrigger>(entity)) {
            if (ImGui::Button("Remove Event Trigger")) component_edit_with_undo([&]() { world->RemoveComponent<scene::EventTrigger>(entity); });
        }
        if (world->HasComponent<scene::Animator>(entity)) {
            if (ImGui::Button("Remove Animator")) component_edit_with_undo([&]() { world->RemoveComponent<scene::Animator>(entity); });
        }
    }

    MG_EndPanel();
}

} // namespace editor
} // namespace mg
