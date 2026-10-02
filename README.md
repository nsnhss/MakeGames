# MakeGames

类 RPG Maker 的 2D 游戏引擎 + 可视化编辑器：Launcher（项目管理器）+ Editor（场景/瓦片/事件编辑 + Play Mode）+ game.exe 独立运行时。

## 技术栈

- C++20，CMake + Ninja + MinGW（GCC 13+）
- Vulkan 渲染（自研渲染器，Sprite 批处理 + 纹理图集）
- SDL2 2.32.10（窗口/输入/音频，已 vendor 在 `SDL2-2.32.10/`）
- ImGui docking 分支（编辑器 UI，含源码级主题定制）
- Lua 5.4 脚本绑定 / FreeType 字体 / stb_image / stb_vorbis / dr_mp3

## 功能概览

- **编辑器**：Outliner / Details / Content Browser / Viewport（Gizmo、瓦片绘制 + autotile、实体点选）/ 事件编辑器（9 种触发器 + 动作流）/ Tile Palette / Animation 面板；Play Mode 一键试玩（与运行时同源 `GameRuntime`）
- **运行时**：ECS 场景、正交相机、AABB 瓦片碰撞、对话/选项/商店/回合制战斗、存档（3 槽位）、BGM/BGS/ME/SE 音频、Lua 绑定、标题/暂停/HUD 全套游戏内 UI
- **数据驱动**：Actors / Items / Skills / Enemies / Shops JSON 数据库 + Actor 成长曲线
- **导出发布**：编辑器内一键导出独立游戏包（可选 `.pak` 资源打包）；`tools/package_release.ps1` 组装引擎发布包（含 `demo_project/` 示例项目）

## 构建

前置：CMake、Ninja、MinGW GCC 13+（可用 CLion 捆绑工具链）、Vulkan SDK（需 `VULKAN_SDK` 环境变量）。

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1 all    # 全量构建
powershell -ExecutionPolicy Bypass -File build.ps1 test   # 构建 + ctest
```

> `build.ps1` 工具链回退链：CLion 捆绑 MinGW → 仓库 `mingw_link/`（本地可选，不入库）→ 系统 PATH。

## 文档

详见 `docs/`：

- `Progress.md` — 进度事实源（唯一权威）
- `GameDataModel.md` — 组件/事件/数据库字段级数据模型
- `TilemapDesign.md` — Tilemap 系统设计
- `archive/` — 历史规划快照（不再维护，仅供溯源）

Agent 操作约定见根目录 `AGENTS.md`。
