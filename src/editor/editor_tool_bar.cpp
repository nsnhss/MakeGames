#include "editor_tool_bar.h"
#include "editor_context.h"
#include "panel.h"
#include "imgui.h"
#include "imgui/mg_imgui_icons.h"
#include "imgui/mg_imgui_extensions.h"

namespace mg {
namespace editor {

// 绘制 Editor 工具栏：水平排列的功能按钮组，所有状态通过 EditorContext 读写
void EditorToolBar::Draw(EditorContext& ctx) {

    // ---- 播放控制 ----
    {
        auto play_state = ctx.GetPlayState();
        bool is_playing = play_state == EditorContext::PlayState::Playing;
        bool is_paused  = play_state == EditorContext::PlayState::Paused;

        if (MG_ToolbarButton(ICON_FA_PLAY, "Play", is_playing)) {
            ctx.SetPlayState(EditorContext::PlayState::Playing);
        }
        ImGui::SameLine();
        if (MG_ToolbarButton(ICON_FA_PAUSE, "Pause", is_paused)) {
            ctx.SetPlayState(is_paused ? EditorContext::PlayState::Playing : EditorContext::PlayState::Paused);
        }
        ImGui::SameLine();
        if (MG_ToolbarButton(ICON_FA_STOP, "Stop", false)) {
            ctx.SetPlayState(EditorContext::PlayState::Edit);
        }
    }

    MG_ToolbarSeparator();

    // ---- Create ----
    {
        if (MG_ToolbarButton(ICON_FA_PLUS " Create", "Create Entity", false)) {
            ImGui::OpenPopup("CreateEntityPopup");
        }
        if (ImGui::BeginPopup("CreateEntityPopup")) {
            auto create = [&](EditorContext::EntityTemplate type, const char* label) {
                if (ImGui::MenuItem(label)) {
                    ctx.CreateEntityFromTemplateAtCameraCenter(type);
                }
            };
            create(EditorContext::EntityTemplate::Empty,       "Entity");
            create(EditorContext::EntityTemplate::NPC,          "NPC");
            create(EditorContext::EntityTemplate::Monster,      "Monster");
            create(EditorContext::EntityTemplate::Chest,        "Chest");
            create(EditorContext::EntityTemplate::Transfer,     "Transfer");
            create(EditorContext::EntityTemplate::Shop,         "Shop");
            create(EditorContext::EntityTemplate::Decoration,   "Decoration");
            ImGui::EndPopup();
        }
    }

    MG_ToolbarSeparator();

    // ---- Undo / Redo ----
    {
        ImGui::BeginDisabled(!ctx.CanUndo());
        if (MG_ToolbarButton(ICON_FA_ARROW_LEFT, "Undo", false)) {
            ctx.Undo();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        ImGui::BeginDisabled(!ctx.CanRedo());
        if (MG_ToolbarButton(ICON_FA_ARROW_RIGHT, "Redo", false)) {
            ctx.Redo();
        }
        ImGui::EndDisabled();
    }

    MG_ToolbarSeparator();

    // ---- Tile Paint + Entity Place ----
    {
        auto gizmo = ctx.GetGizmoMode();
        bool tile_active = gizmo == EditorContext::GizmoMode::Tile;
        if (MG_ToolbarButton(ICON_FA_TH, "Tile Paint", tile_active)) {
            if (tile_active) {
                // 已在 Tile 模式：再次点击关闭 Palette（仍保持 Tile 模式，避免意外切换工具）
                if (auto* pm = ctx.GetPanelManager()) {
                    if (auto* panel = pm->FindPanel("Tile Palette")) panel->SetOpen(false);
                }
            } else {
                ctx.SetGizmoMode(EditorContext::GizmoMode::Tile);
                ctx.OpenTilePalettePanel();
            }
        }
        ImGui::SameLine();
        bool ent_active = gizmo == EditorContext::GizmoMode::Entity;
        if (MG_ToolbarButton(ICON_FA_PLUS, "Place Entity", ent_active)) {
            ctx.SetGizmoMode(EditorContext::GizmoMode::Entity);
        }
    }
}

} // namespace editor
} // namespace mg
