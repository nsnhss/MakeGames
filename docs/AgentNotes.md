# Agent 操作备忘

## Windows 编译环境 PATH 问题（高频踩坑点）

本项目在 Windows 下使用 **CLion + MinGW + Ninja** 构建。  
**通过 Shell 直接调用 `g++.exe` 或 `ninja` 时，必须将 MinGW 的 `bin` 目录加入 `PATH`**，否则子进程（如 `cc1plus.exe`）会静默崩溃：

- 表现为返回码 `1`，但 **stdout / stderr 完全为空**，没有任何错误提示。
- 极易误以为是代码语法错误，浪费大量时间排查。

### 正确做法

在 PowerShell 中执行编译前，先设置 PATH：

```powershell
$env:PATH = "D:\JetBrains\CLion 2025.2.5\bin\mingw\bin;D:\JetBrains\CLion 2025.2.5\bin\ninja\win\x64;" + $env:PATH
cd D:\CLionProjects\MakeGames\cmake-build-release
ninja launcher
```

### 关键路径

| 工具 | 路径 |
|------|------|
| g++ | `D:\JetBrains\CLion 2025.2.5\bin\mingw\bin\g++.exe` |
| ninja | `D:\JetBrains\CLion 2025.2.5\bin\ninja\win\x64\ninja.exe` |
| 构建目录 | `D:\CLionProjects\MakeGames\cmake-build-release` |

> **注意**：即使 `g++.exe --version` 能正常输出版本号，也不代表它能正常编译。版本号查询不依赖 `cc1plus.exe`，而实际编译需要。

---

## 严禁用 `Start-Process` 启动 GUI 程序

**绝对禁止**通过 Shell 的 `Start-Process` 命令启动 `launcher.exe` 或 `editor.exe`：

- `Start-Process` 会改变程序的工作目录（通常是 Shell 当前目录，而非 exe 所在目录）。
- 这会导致所有相对路径（日志文件、项目配置、资源加载）错乱，产生无法复现的 bug。
- 可能同时启动多个进程实例，造成窗口重复、ImGui Viewport 状态冲突。

### 正确做法

- **由用户自己双击运行**，或从 IDE（CLion）启动。
- Agent **只负责编译**（`ninja launcher` / `ninja editor`），**绝不负责启动**。
- 如果需要分析运行日志，等用户手动运行并关闭程序后，Agent 再去读取生成的日志文件。
