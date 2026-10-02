#include <cstdio>
#include <string>
#include <SDL.h>

#ifdef _WIN32
#include <windows.h>
#include <processthreadsapi.h>
#endif

#include "platform/app_bootstrap.h"
#include "src/editor/editor_application.h"
#include "src/editor/editor_layer.h"
#include "src/editor/editor_context.h"
#include "core/debug_log.h"

int main(int argc, char** argv)
{
    // 先完成控制台附加/崩溃处理/DPI/工作目录，再输出日志（避免无控制台时写 stdout）
    mg::platform::BootstrapApp("crash.log", "editor/editor.exe");

    EDITOR_LOG("=== main() START ===");

    // Editor 接收命令行参数作为项目路径（由 Launcher 启动时传入）
    std::string project_path;
    if (argc > 1 && argv[1] != nullptr && argv[1][0] != '\0') {
        project_path = argv[1];
    }
    EDITOR_LOG("project_path=" << project_path);

    // Editor 应用入口：创建 Application（传入项目路径）→ 压入 EditorLayer → 进入主循环
#ifdef _WIN32
    EDITOR_LOG("Creating EditorApplication... pid=" << GetCurrentProcessId());
#else
    EDITOR_LOG("Creating EditorApplication...");
#endif
    mg::editor::EditorContext editor_ctx;
    mg::editor::EditorApplication app(project_path, editor_ctx);
    EDITOR_LOG("EditorApplication created, window=" << app.GetWindow());
    if (!app.GetWindow()) {
        EDITOR_LOG("SDL_CreateWindow failed: " << (SDL_GetError() ? SDL_GetError() : "unknown"));
        return 1;
    }
    EDITOR_LOG("Pushing EditorLayer...");
    app.PushLayer(std::make_unique<mg::editor::EditorLayer>(project_path, editor_ctx));
    EDITOR_LOG("EditorLayer pushed, calling Run()...");
    app.Run();
    EDITOR_LOG("Run() returned, main() END");
    return 0;
}
