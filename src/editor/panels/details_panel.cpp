#include "details_panel.h"
#include "src/editor/selection_manager.h"
#include "src/editor/scene_session.h"
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

namespace mg {
namespace editor {

static void DrawTransformInspector(SceneSession& session_, scene::World* world, scene::EntityID entity) {
    auto* comp = world->GetComponent<scene::Transform>(entity);
    if (!comp) return;

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (MG_PropertyInputFloat2("Position", &comp->position.x, "%.2f")) session_.MarkSceneDirty();
        if (MG_PropertyInputFloat("Rotation", &comp->rotation, 1.0f, "%.2f")) session_.MarkSceneDirty();
        if (MG_PropertyInputFloat2("Scale", &comp->scale.x, "%.2f")) session_.MarkSceneDirty();
        if (MG_PropertyInputInt("Sorting Order", &comp->sorting_order)) session_.MarkSceneDirty();
    }
}

static void DrawNameInspector(SceneSession& session_, scene::World* world, scene::EntityID entity) {
    auto* comp = world->GetComponent<scene::Name>(entity);
    if (!comp) return;

    if (ImGui::CollapsingHeader("Name", ImGuiTreeNodeFlags_DefaultOpen)) {
        char buf[256];
        std::strncpy(buf, comp->value.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText("##Name", buf, sizeof(buf),
                ImGuiInputTextFlags_CallbackAlways,
                ImGui_ImplSDL2_InputTextCallback)) {
            comp->value = buf;
            session_.MarkSceneDirty();
        }
        ImGui_ImplSDL2_RenderTextInputIME(buf, ImGui::GetColorU32(ImGuiCol_CheckMark));
    }
}

static void DrawTagInspector(SceneSession& session_, scene::World* world, scene::EntityID entity) {
    auto* comp = world->GetComponent<scene::Tag>(entity);
    if (!comp) return;

    if (ImGui::CollapsingHeader("Tag", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (size_t i = 0; i < comp->tags.size(); ++i) {
            ImGui::BulletText("%s", comp->tags[i].c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton((std::string("x##") + std::to_string(i)).c_str())) {
                comp->tags.erase(comp->tags.begin() + i);
                --i;
                session_.MarkSceneDirty();
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
        }
    }
}

static void ShowSpriteTextureDialog(SceneSession& session_, scene::Sprite* comp) {
    auto files = mg::editor::OpenFileDialogMulti("Images", "*.png;*.jpg;*.jpeg;*.bmp");
    if (files.empty()) return;
    std::filesystem::path p(files[0]);
    std::string rel = "assets/" + p.filename().string();
    comp->texture_path = rel;
    session_.MarkSceneDirty();
}

static void DrawSpriteInspector(SceneSession& session_, scene::World* world, scene::EntityID entity) {
    auto* comp = world->GetComponent<scene::Sprite>(entity);
    if (!comp) return;

    if (ImGui::CollapsingHeader("Sprite", ImGuiTreeNodeFlags_DefaultOpen)) {
        char buf[256];
        std::strncpy(buf, comp->texture_path.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText("Texture", buf, sizeof(buf),
                ImGuiInputTextFlags_CallbackAlways,
                ImGui_ImplSDL2_InputTextCallback)) {
            comp->texture_path = buf;
            session_.MarkSceneDirty();
        }
        ImGui_ImplSDL2_RenderTextInputIME(buf, ImGui::GetColorU32(ImGuiCol_CheckMark));
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_FA_FOLDER_OPEN)) {
            ShowSpriteTextureDialog(session_, comp);
        }
        if (MG_PropertyInputFloat2("Pivot", &comp->pivot.x, "%.2f")) session_.MarkSceneDirty();
        if (MG_PropertyColorEdit4("Tint", &comp->tint.r)) session_.MarkSceneDirty();
        if (MG_PropertyCheckbox("Flip X", &comp->flip_x)) session_.MarkSceneDirty();
        if (MG_PropertyCheckbox("Flip Y", &comp->flip_y)) session_.MarkSceneDirty();
        if (MG_PropertyCheckbox("Y-Sort", &comp->y_sort)) session_.MarkSceneDirty();
    }
}

static void DrawCameraInspector(SceneSession& session_, scene::World* world, scene::EntityID entity) {
    auto* comp = world->GetComponent<scene::Camera>(entity);
    if (!comp) return;

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        int proj = (comp->projection == scene::Camera::Projection::Orthographic) ? 0 : 1;
        const char* items[] = { "Orthographic", "Perspective" };
        if (ImGui::Combo("Projection", &proj, items, 2)) {
            comp->projection = (proj == 1) ? scene::Camera::Projection::Perspective : scene::Camera::Projection::Orthographic;
            session_.MarkSceneDirty();
        }
        if (MG_PropertyInputFloat("Ortho Size", &comp->ortho_size, 0.1f, "%.2f")) session_.MarkSceneDirty();
        if (MG_PropertyCheckbox("Primary", &comp->is_primary)) session_.MarkSceneDirty();

        MG_SectionHeader("Follow");
        if (MG_PropertyInputFloat("Follow Speed", &comp->follow_speed, 0.1f, "%.2f")) session_.MarkSceneDirty();
        if (MG_PropertyInputFloat2("Dead Zone", &comp->dead_zone.x, "%.1f")) session_.MarkSceneDirty();
        if (MG_PropertyCheckbox("Smooth Follow", &comp->smooth_follow)) session_.MarkSceneDirty();
    }
}

static void DrawAnimatorInspector(SceneSession& session_, scene::World* world, scene::EntityID entity) {
    auto* comp = world->GetComponent<scene::Animator>(entity);
    if (!comp) return;

    if (ImGui::CollapsingHeader("Animator", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (MG_PropertyInputInt("Frame Count", &comp->frame_count)) session_.MarkSceneDirty();
        if (MG_PropertyInputInt("Columns", &comp->columns)) session_.MarkSceneDirty();
        if (MG_PropertyInputFloat("FPS", &comp->fps, 0.1f, "%.1f")) session_.MarkSceneDirty();
        if (MG_PropertyCheckbox("Playing", &comp->playing)) session_.MarkSceneDirty();
        ImGui::TextDisabled("Current Frame: %d", comp->current_frame);
    }
}

static void DrawCameraFollowTargetInspector(SceneSession& session_, scene::World* world, scene::EntityID entity) {
    auto* comp = world->GetComponent<scene::CameraFollowTarget>(entity);
    if (!comp) return;

    if (ImGui::CollapsingHeader("Camera Follow Target", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("This entity is a camera follow target.");
        ImGui::TextDisabled("A Camera component on another entity can follow it.");
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
        ImGui::TextDisabled("No object selected.");
        ImGui::TextDisabled("Select an entity from the Outliner or Viewport.");
        MG_EndPanel();
        return;
    }

    if (selection_.GetType() == SelectionManager::Type::Asset) {
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
            ImGui::TextDisabled("No active scene.");
            MG_EndPanel();
            return;
        }
        auto* world = scene->GetWorld();

        if (!world->IsValid(entity)) {
            ImGui::TextDisabled("Selected entity is no longer valid.");
            MG_EndPanel();
            return;
        }

        auto* name_comp = world->GetComponent<scene::Name>(entity);
        const char* display_name = name_comp ? name_comp->value.c_str() : "Unnamed";
        ImGui::Text(ICON_FA_CUBE " %s (#%u)", display_name, entity);
        MG_SectionHeader("Properties");

        // Component Inspectors
        DrawTransformInspector(session_, world, entity);
        DrawNameInspector(session_, world, entity);
        DrawTagInspector(session_, world, entity);
        DrawSpriteInspector(session_, world, entity);
        DrawCameraInspector(session_, world, entity);
        DrawCameraFollowTargetInspector(session_, world, entity);
        DrawAnimatorInspector(session_, world, entity);

        // Add Component
        MG_SectionHeader("Components");
        if (ImGui::Button(ICON_FA_PLUS " Add Component")) {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup")) {
            auto add_if_missing = [&](const char* label, auto add_fn) {
                if (ImGui::MenuItem(label)) {
                    add_fn();
                    session_.MarkSceneDirty();
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
            if (ImGui::Button("Remove Transform")) { world->RemoveComponent<scene::Transform>(entity); session_.MarkSceneDirty(); }
        }
        if (world->HasComponent<scene::Name>(entity)) {
            if (ImGui::Button("Remove Name")) { world->RemoveComponent<scene::Name>(entity); session_.MarkSceneDirty(); }
        }
        if (world->HasComponent<scene::Tag>(entity)) {
            if (ImGui::Button("Remove Tag")) { world->RemoveComponent<scene::Tag>(entity); session_.MarkSceneDirty(); }
        }
        if (world->HasComponent<scene::Sprite>(entity)) {
            if (ImGui::Button("Remove Sprite")) { world->RemoveComponent<scene::Sprite>(entity); session_.MarkSceneDirty(); }
        }
        if (world->HasComponent<scene::Camera>(entity)) {
            if (ImGui::Button("Remove Camera")) { world->RemoveComponent<scene::Camera>(entity); session_.MarkSceneDirty(); }
        }
        if (world->HasComponent<scene::CameraFollowTarget>(entity)) {
            if (ImGui::Button("Remove Camera Follow Target")) { world->RemoveComponent<scene::CameraFollowTarget>(entity); session_.MarkSceneDirty(); }
        }
        if (world->HasComponent<scene::EventTrigger>(entity)) {
            if (ImGui::Button("Remove Event Trigger")) { world->RemoveComponent<scene::EventTrigger>(entity); session_.MarkSceneDirty(); }
        }
        if (world->HasComponent<scene::Animator>(entity)) {
            if (ImGui::Button("Remove Animator")) { world->RemoveComponent<scene::Animator>(entity); session_.MarkSceneDirty(); }
        }
    }

    MG_EndPanel();
}

} // namespace editor
} // namespace mg
