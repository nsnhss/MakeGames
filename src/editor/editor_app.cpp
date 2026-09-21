#include "editor_app.h"
#include "editor_context.h"
#include "panels/viewport_panel.h"
#include "panels/outliner_panel.h"
#include "panels/details_panel.h"
#include "panels/console_panel.h"
#include "panels/event_editor_panel.h"
#include "panels/tile_palette_panel.h"
#include "panels/item_database_panel.h"
#include "panels/script_editor_panel.h"
#include "panels/animation_panel.h"
#include "imgui/mg_imgui_extensions.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "scene/scene_serializer.h"
#include <filesystem>

namespace mg {
namespace editor {

// 注册所有默认面板并初始化默认布局
void EditorApp::Initialize(EditorContext& ctx) {
    ctx_ = &ctx;
    // 注册所有面板（方案 B：除 Viewport 外按组件级注入，门面仅作分发源头）
    panel_manager_.RegisterPanel(std::make_unique<ViewportPanel>(ctx));
    panel_manager_.RegisterPanel(std::make_unique<OutlinerPanel>(
        ctx.GetSelection(), ctx.GetSceneSession(), ctx.GetProjectManager(),
        ctx.GetCommandHistory(), &panel_manager_,
        ctx.GetPrefabManager(), ctx.GetViewport()));
    panel_manager_.RegisterPanel(std::make_unique<DetailsPanel>(
        ctx.GetSelection(), ctx.GetSceneSession()));
    panel_manager_.RegisterPanel(std::make_unique<ConsolePanel>());
    panel_manager_.RegisterPanel(std::make_unique<EventEditorPanel>(
        ctx.GetSelection(), ctx.GetSceneSession(), ctx.GetProjectManager()));
    panel_manager_.RegisterPanel(std::make_unique<TilePalettePanel>(
        ctx.GetTilesetEditor(), ctx.GetSceneSession(), ctx.GetProjectManager()));
    panel_manager_.RegisterPanel(std::make_unique<ItemDatabasePanel>(
        ctx.GetDatabases(), [&ctx]() { return ctx.SaveProject(); }));
    panel_manager_.RegisterPanel(std::make_unique<ScriptEditorPanel>(
        ctx.GetSceneSession(), [&ctx]() { return ctx.GetEventSystem(); }));
    panel_manager_.RegisterPanel(std::make_unique<AnimationPanel>(
        ctx.GetSelection(), ctx.GetSceneSession()));
    layout_.Initialize();
}

void EditorApp::Shutdown() { /* reserved */ }

// 每帧绘制：HostWindow → MenuBar → ToolBar → DockSpace → 所有面板
void EditorApp::Draw() {
    DrawMainHostWindow();

    // 全局快捷键 Ctrl+O：打开场景文件
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O)) {
        auto& ctx = *ctx_;
        std::string scenes_dir = ctx.GetProjectPath() + "/scenes";
        if (std::filesystem::exists(scenes_dir)) {
            for (const auto& entry : std::filesystem::directory_iterator(scenes_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    auto scene = scene::SceneSerializer::LoadFromFile(entry.path().string());
                    if (scene) {
                        ctx.SetActiveScene(scene.release());
                        ctx.ClearSceneDirty();
                        ctx.SetScenePath(entry.path().string());
                    }
                    break;
                }
            }
        }
    }

    // MenuBar
    if (ImGui::BeginMenuBar()) {
        menu_bar_.Draw(*ctx_, panel_manager_);
        ImGui::EndMenuBar();
    }
    menu_bar_.RenderPopups(*ctx_);

    // ToolBar
    float toolbar_height = ImGui::GetFrameHeight() * 1.8f;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    ImGui::BeginChild("Toolbar", ImVec2(0, toolbar_height), false, ImGuiWindowFlags_NoScrollbar);
    tool_bar_.Draw(*ctx_);
    ImGui::EndChild();
    ImGui::PopStyleVar();

    // DockSpace
    ImGuiID dockspace_id = ImGui::GetID("EditorDockSpace");
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
        layout_.BuildDefaultLayout(dockspace_id, ImGui::GetContentRegionAvail());
    }
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::End();

    // 绘制所有已注册面板
    panel_manager_.DrawAllPanels();
}

// 创建全屏覆盖的主 Host 窗口，作为 DockSpace 的容器
// 无边框、无标题栏、无导航焦点，占据整个 Viewport 工作区
void EditorApp::DrawMainHostWindow() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGuiWindowFlags host_flags = ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoNavFocus
        | ImGuiWindowFlags_MenuBar;

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("EditorDockSpaceHost", nullptr, host_flags);

    ImGui::PopStyleVar(3);
}

} // namespace editor
} // namespace mg
