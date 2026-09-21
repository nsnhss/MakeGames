#pragma once
#include "app/layer.h"
#include "app/application.h"
#include "ui/launcher_window.h"

namespace mg {
namespace launcher {

// Launcher 业务 Layer：将 LauncherWindow 接入 Application 主循环
// 负责窗口初始化、每帧绘制、以及处理用户关闭请求（ShouldQuit → app_->Close）
class LauncherLayer : public Layer {
public:
    explicit LauncherLayer(Application* app);

    void OnAttach() override;
    void OnDetach() override;
    void OnRender() override;

private:
    Application* app_;
    LauncherWindow launcher_window_;
};

} // namespace launcher
} // namespace mg
