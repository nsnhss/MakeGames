#pragma once

namespace mg {
namespace platform {

// Windows 应用启动引导（应在 main() 开头调用一次）：
//   1. 附加父进程控制台（若有），重定向 stdout/stderr
//   2. 安装未处理异常过滤器，崩溃时写 crash_log_name（含异常地址/相对偏移/48 帧堆栈 + addr2line 提示）
//   3. 将工作目录锁定到 exe 所在目录（避免由其他进程启动时相对路径错乱）
//   4. 启用 PerMonitorV2 DPI 感知（失败回退 SetProcessDPIAware）
// 非 Windows 平台为空操作。
//   crash_log_name：崩溃日志文件名（如 "crash.log" / "launcher_crash.log"）
//   exe_path_hint ：addr2line 提示中的 exe 相对路径（如 "editor/editor.exe"）
void BootstrapApp(const char* crash_log_name, const char* exe_path_hint);

} // namespace platform
} // namespace mg
