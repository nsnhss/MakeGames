#include <cstdio>
#include <filesystem>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "platform/app_bootstrap.h"
#include "launcher_application.h"
#include "launcher_layer.h"
#include "pending_editor.h"

int main(int, char**)
{
    mg::platform::BootstrapApp("launcher_crash.log", "launcher/launcher.exe");

    // Launcher 应用入口：创建 Application → 压入 LauncherLayer → 进入主循环
    // LauncherLayer 负责所有 UI 渲染，Application 负责事件轮询和帧驱动
    mg::launcher::LauncherApplication app;
    app.PushLayer(std::make_unique<mg::launcher::LauncherLayer>(&app));
    app.Run();

#ifdef _WIN32
    // 延迟启动：Launcher 窗口关闭、Vulkan 销毁完毕后，才启动 Editor
    const std::string& pending_editor = mg::launcher::GetPendingEditorPath();
    if (!pending_editor.empty()) {
        namespace fs = std::filesystem;
        wchar_t self[MAX_PATH]; GetModuleFileNameW(nullptr, self, MAX_PATH);
        fs::path dir = fs::path(self).parent_path();
        fs::path exe = dir / "editor" / "editor.exe";
        if (!fs::exists(exe)) exe = dir / "editor.exe";
        if (!fs::exists(exe)) exe = dir / ".." / "editor" / "editor.exe";
        if (fs::exists(exe)) {
            std::string sargs = "\"" + pending_editor + "\"";
            std::wstring wexe = exe.wstring();
            int len = MultiByteToWideChar(CP_UTF8, 0, sargs.c_str(), -1, nullptr, 0);
            std::wstring wargs(len - 1, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, sargs.c_str(), -1, &wargs[0], len);
            STARTUPINFOW si = {}; si.cb = sizeof(si);
            si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_SHOW;
            PROCESS_INFORMATION pi = {};
            std::wstring cmd = wexe + L" " + wargs;
            if (CreateProcessW(wexe.c_str(), &cmd[0], nullptr, nullptr, FALSE,
                               DETACHED_PROCESS, nullptr, nullptr, &si, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            } else {
                fprintf(stderr, "Failed to launch editor.exe (error %lu): %s\n",
                        GetLastError(), exe.string().c_str());
            }
        } else {
            fprintf(stderr, "editor.exe not found near %s\n", dir.string().c_str());
        }
    }
#endif

    return 0;
}
