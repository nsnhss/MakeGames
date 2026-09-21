#include "platform/app_bootstrap.h"

#ifdef _WIN32

#include <cstdio>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace mg {
namespace platform {

namespace {

const char* g_exe_hint = "app.exe";

// 崩溃追踪：未处理异常过滤器，打印异常地址、模块基址、相对偏移、48 帧堆栈。
// 输出写入 crash log 文件（持久化），不写入 stderr：
// GUI 子进程可能没有控制台，fprintf(stderr) 可能让崩溃处理器自身崩溃。
void PrintCrashInfo(FILE* out, EXCEPTION_POINTERS* ex)
{
    HMODULE mod = GetModuleHandleA(NULL);
    uintptr_t rel = (uintptr_t)ex->ExceptionRecord->ExceptionAddress - (uintptr_t)mod;

    fprintf(out, "\n========== CRASH ==========\n");
    fprintf(out, "Exception: 0x%08X\n", ex->ExceptionRecord->ExceptionCode);
    fprintf(out, "Address:   %p\n", ex->ExceptionRecord->ExceptionAddress);
    fprintf(out, "Module:    %p\n", (void*)mod);
    fprintf(out, "Relative:  %p\n", (void*)rel);

    void* stack[48];
    USHORT n = CaptureStackBackTrace(0, 48, stack, NULL);
    fprintf(out, "\nStack trace (%d frames):\n", (int)n);
    for (USHORT i = 0; i < n; i++) {
        uintptr_t srel = (uintptr_t)stack[i] - (uintptr_t)mod;
        fprintf(out, "  #%02d  %p  (+%p)\n", i, stack[i], (void*)srel);
    }
    fprintf(out, "===========================\n");
    fprintf(out, "\n用 addr2line 定位（在 cmake-build-release 目录执行）:\n");
    fprintf(out, "  addr2line -e %s -f -C %p\n", g_exe_hint, (void*)rel);
    fprintf(out, "===========================\n\n");
    fflush(out);
}

const char* g_crash_log_name = "crash.log";

LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ex)
{
    FILE* f = fopen(g_crash_log_name, "w");
    if (f) {
        PrintCrashInfo(f, ex);
        fclose(f);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace

void BootstrapApp(const char* crash_log_name, const char* exe_path_hint)
{
    if (crash_log_name && crash_log_name[0] != '\0') g_crash_log_name = crash_log_name;
    if (exe_path_hint && exe_path_hint[0] != '\0') g_exe_hint = exe_path_hint;

    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
    SetUnhandledExceptionFilter(CrashHandler);

    // 无论从哪里启动，都把工作目录锁定到 exe 所在目录，
    // 避免由 launcher 等进程启动时工作目录不一致导致相对路径错误
    {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring dir(path);
        dir = dir.substr(0, dir.find_last_of(L'\\'));
        SetCurrentDirectoryW(dir.c_str());
    }

    // 动态加载 PerMonitorV2 DPI 感知（Win10 1703+），失败则回退到 SetProcessDPIAware
    typedef BOOL (WINAPI *SetProcessDpiAwarenessContextFunc)(void*);
    void* dpi_awareness_v2 = (void*)-4; // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (user32) {
        auto fn = (SetProcessDpiAwarenessContextFunc)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (fn) {
            fn(dpi_awareness_v2);
        } else {
            ::SetProcessDPIAware();
        }
    } else {
        ::SetProcessDPIAware();
    }
}

} // namespace platform
} // namespace mg

#else // 非 Windows：空操作

namespace mg {
namespace platform {

void BootstrapApp(const char*, const char*) {}

} // namespace platform
} // namespace mg

#endif
