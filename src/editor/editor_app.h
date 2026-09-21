#pragma once
#include "editor_layout.h"
#include "editor_menu_bar.h"
#include "editor_tool_bar.h"
#include "panel.h"

namespace mg {
namespace editor {

class EditorContext;

// Editor 核心应用逻辑：管理面板注册、主窗口 Host、MenuBar、ToolBar、DockSpace 布局
// 不负责 Vulkan/ImGui 初始化（由 EditorApplication 负责），只负责 Editor 业务 UI 的绘制
class EditorApp {
public:
    void Initialize(EditorContext& ctx);
    void Shutdown();
    void Draw();

    PanelManager& GetPanelManager() { return panel_manager_; }

private:
    void DrawMainHostWindow();

    EditorContext* ctx_ = nullptr;
    EditorLayout layout_;
    EditorMenuBar menu_bar_;
    EditorToolBar tool_bar_;
    PanelManager panel_manager_;
};

} // namespace editor
} // namespace mg
