// viewport_context_menu.cpp — 视口右键点选 + 上下文菜单 + Delete 键删除（D4 自 viewport_panel.cpp 拆出）。
#include "viewport_panel.h"
#include "src/editor/editor_context.h"
#include "src/editor/command_history.h"
#include "src/editor/imgui/mg_imgui_icons.h"
#include "scene/scene.h"
#include "scene/constants.h"
#include "scene/world.h"
#include "scene/components/transform.h"
#include "imgui.h"
#include <memory>

namespace mg {
namespace editor {

void ViewportPanel::DrawContextMenu(ViewportFrameCtx& f) {
    auto& ctx = ctx_;
    auto* world = f.world;
    using GizmoMode = EditorContext::GizmoMode;
    const GizmoMode gizmo_mode = ctx.GetGizmoMode();
    const bool pick_allowed = (gizmo_mode != GizmoMode::Tile &&
                               gizmo_mode != GizmoMode::Entity);

    // ---- 右键点选 + 上下文菜单 ----
    static math::Vec2 rc_world_pos{0.0f, 0.0f}; // 右键点击处世界坐标（Create 用）
    if (pick_allowed && f.hovered && f.mouse_valid &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        rc_target_entity_ = PickEntityAt(world, f.mouse_wx, f.mouse_wy, scene::kPickRadius);
        rc_on_entity_ = (rc_target_entity_ != 0);
        rc_world_pos = {f.mouse_wx, f.mouse_wy};
        if (rc_on_entity_) ctx.SelectEntity(rc_target_entity_);
        ImGui::OpenPopup("##VPContext");
    }
    if (ImGui::BeginPopup("##VPContext")) {
        if (rc_on_entity_ && world->IsValid(rc_target_entity_)) {
            if (ImGui::MenuItem(ICON_FA_PEN " Edit Events")) {
                ctx.SelectEntity(rc_target_entity_);
                ctx.OpenEventEditorPanel();
            }
            if (ImGui::MenuItem(ICON_FA_LIST " Inspect")) {
                ctx.SelectEntity(rc_target_entity_);
                ctx.OpenDetailsPanel();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_COPY " Duplicate")) {
                auto dup = world->CloneEntity(rc_target_entity_);
                if (auto* t = world->GetComponent<scene::Transform>(dup))
                    t->position.x += 16.0f;
                ctx.ExecuteCommand(std::make_unique<CreateEntityCommand>(world, dup));
                ctx.SelectEntity(dup);
                ctx.MarkSceneDirty();
            }
            if (ImGui::MenuItem(ICON_FA_TRASH " Delete")) {
                ctx.ExecuteCommand(
                    std::make_unique<DeleteEntityCommand>(world, rc_target_entity_));
                ctx.ClearSelection();
                rc_target_entity_ = 0;
                rc_on_entity_ = false;
            }
        } else {
            if (ImGui::BeginMenu(ICON_FA_PLUS " Create")) {
                auto create = [&](EditorContext::EntityTemplate type, const char* label) {
                    if (ImGui::MenuItem(label))
                        ctx.CreateEntityFromTemplate(type, rc_world_pos);
                };
                create(EditorContext::EntityTemplate::Empty,      "Entity");
                create(EditorContext::EntityTemplate::NPC,        "NPC");
                create(EditorContext::EntityTemplate::Monster,    "Monster");
                create(EditorContext::EntityTemplate::Chest,      "Chest");
                create(EditorContext::EntityTemplate::Transfer,   "Transfer");
                create(EditorContext::EntityTemplate::Shop,       "Shop");
                create(EditorContext::EntityTemplate::Decoration, "Decoration");
                ImGui::EndMenu();
            }
        }
        ImGui::EndPopup();
    }

    // ---- Delete 键删除选中实体（与右键/Outliner 统一走 Undo 栈）----
    if (ImGui::IsWindowFocused() &&
        ImGui::IsKeyPressed(ImGuiKey_Delete, false) &&
        ctx.GetSelectionType() == EditorContext::SelectionType::Entity) {
        scene::EntityID sel = ctx.GetSelectedEntity();
        if (sel != 0 && world->IsValid(sel)) {
            ctx.ExecuteCommand(std::make_unique<DeleteEntityCommand>(world, sel));
            ctx.ClearSelection();
        }
    }
}

} // namespace editor
} // namespace mg
