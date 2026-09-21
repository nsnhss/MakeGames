#include "launcher_layer.h"

namespace mg {
namespace launcher {

LauncherLayer::LauncherLayer(Application* app)
    : Layer("LauncherLayer"), app_(app)
{
}

// 初始化 Launcher UI 状态
void LauncherLayer::OnAttach()
{
    launcher_window_.Initialize();
}

// 退出前兜底保存 Launcher 状态（项目列表等）
void LauncherLayer::OnDetach()
{
    launcher_window_.Shutdown();
}

// 每帧绘制 Launcher UI，检测用户是否点击了退出按钮
void LauncherLayer::OnRender()
{
    launcher_window_.Draw();
    if (launcher_window_.ShouldQuit()) {
        app_->Close();
    }
}

} // namespace launcher
} // namespace mg
