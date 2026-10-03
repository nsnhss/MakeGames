# AGENTS.md — MakeGames 项目操作约定

类 RPG Maker 的 2D 游戏引擎：Launcher + Editor + game.exe 独立运行时。
C++20，CMake + Ninja + MinGW（GCC 13.1），SDL2 2.32.10（vendor 于 `SDL2-2.32.10/`），Vulkan SDK（`VULKAN_SDK`），ImGui docking，Lua 5.4，FreeType，stb。

## 构建 / 测试

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1 all    # launcher + editor + game
powershell -ExecutionPolicy Bypass -File build.ps1 test   # engine_tests + ctest
```

- 构建目录固定 `cmake-build-release/`（须先存在；不存在则 `cmake -S . -B cmake-build-release -G Ninja -DCMAKE_BUILD_TYPE=Release`）。
- 工具链回退链：CLion 捆绑 MinGW → 仓库 `mingw_link/`（不入库）→ 系统 PATH。
- 着色器改动后运行 `tools/compile_shaders.ps1`。
- **文档事实源**：`docs/Progress.md`（每阶段完成后必须更新）。

## 编译环境 PATH 坑（高频）

Shell 直接调用 `g++.exe` / `ninja` 前必须把 MinGW bin 加入 PATH，否则 `cc1plus.exe` 静默崩溃（返回码 1，无任何输出）：

```powershell
$env:PATH = "D:\JetBrains\CLion 2025.2.5\bin\mingw\bin;D:\JetBrains\CLion 2025.2.5\bin\ninja\win\x64;" + $env:PATH
```

即使 `g++ --version` 正常也不代表能编译（版本查询不依赖 cc1plus）。

## 单 TU 验证模式（大改之间的快速验证，不跑全量 build）

```powershell
& "D:\JETBRA~1\CLION2~1.5\bin\mingw\bin\g++.exe" -DSDL_MAIN_HANDLED `
  -I<repo>\SDL2-2.32.10\x86_64-w64-mingw32\include\SDL2 -IC:/VulkanSDK/1.4.341.1/Include `
  -I<repo>\imgui -I<repo> -I<repo>\src -I<repo>\third_party\freetype\include `
  -I<repo>\third_party\lua [-I<repo>\tests] `
  -O3 -DNDEBUG -std=gnu++20 -finput-charset=UTF-8 -fexec-charset=UTF-8 -Wall `
  -c <file> -o NUL   # stderr 有输出即警告/错误；警告必须清零
```

## 纪律

- **绝不启动 GUI**：Agent 只编译，不运行 launcher/editor/game；GUI 冒烟由用户手动执行。
  严禁 `Start-Process` 启动 exe（会错乱工作目录导致相对路径 bug，且可能多开）。
- **零警告**：改动的文件单 TU `-Wall` 必须干净。
- **PowerShell 写文件**：① 禁用数组拼接重写大文件（曾截断 viewport_panel.cpp）；② 同一文件多处修改必须合并进单次 StrReplaceFile 调用的 edit 数组（并行调用同一文件会 read-modify-write 互相覆盖）；③ 含中文的 .ps1 必须存为 **UTF-8 with BOM**（PS 5.1 无 BOM 按 ANSI 解析会炸）；④ **严禁用 `Get-Content`（无 -Encoding）读写源文件**——PS 5.1 默认按 GBK 解码 UTF-8 无 BOM 文件，写回即全文件中文乱码（曾毁 editor_application.cpp，靠 git checkout 恢复）；必须改行级内容时用 `[System.IO.File]::ReadAllLines/WriteAllLines(path, UTF8)` 显式指定编码，写回用 `UTF8Encoding($false)` 保持无 BOM。
- **git**：不主动 commit/push/reset/rebase，除非用户明确要求。
- 中文 UI 字符串直接写 UTF-8 字面量（构建带 `-fexec-charset=UTF-8`）。

## 结构速览

- `src/`：`engine_runtime`（零 ImGui：core/renderer/scene/resource/audio/script/gameui 之外的运行时）+ `engine_editor`（编辑器 ImGui 封装）+ `engine_gameui`（ImGui 本体 + Vulkan/SDL2 后端 + Play Mode 覆盖层 UI `gameui/playmode_ui.*`，editor/game 共用）+ `game_main.cpp`
- `editor/`、`launcher/`：各自 exe；`tests/`：engine_tests（自包含最小框架，宏见 `test_framework.h`）
- Play Mode 编排 `scene/game_runtime.*`（editor 与 game 共用）；存档格式见 `game_runtime.cpp` SaveToSlot
- `docs/`：`Progress.md`（事实源）、`GameDataModel.md`、`TilemapDesign.md`、`archive/`（历史快照，不再维护）

## 日志

- 需要运行日志时：等用户手动运行并关闭程序后，读取 `logs/engine.log`。
