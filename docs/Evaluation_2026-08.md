# MakeGames 项目评估报告（2026-08-14）

> 本次评估覆盖全部计划文档（docs/ 9 份）与全部自有代码（src/ + editor/ + launcher/，143 文件约 16,900 行）。
> 构建验证：三个可执行目标（game / editor / launcher）全部编译通过。
> 本文档配套的整改方案分 6 个阶段（A 文档 → B 清理 → C 工程化 → D 功能补全 → E 测试 → F EditorContext 拆分），执行进度回填到 [`Progress.md`](Progress.md)。

---

## 一、项目概况

| 项 | 数据 |
|---|---|
| 定位 | 类 RPG Maker 的 2D 游戏引擎 + 编辑器 + 启动器（C++20 / SDL2 / Vulkan / ImGui / Lua） |
| 自有代码 | 143 个文件，约 16,900 行 |
| 构建产物 | `game.exe`（独立运行时）、`editor.exe`、`launcher.exe` + `engine_runtime` / `engine_editor` 静态库 |
| 构建状态 | ✅ 全部目标编译通过 |
| 测试 | ❌ 无任何自动化测试 |

核心闭环已落地：**Launcher 选项目 → spawn Editor → 编辑/播放 → Export Game 导出独立 game.exe 包**。

## 二、文档体系评估

**单体质量高，体系同步差。**

- ✅ TilemapDesign、NextPhasePlan_v24 达到"按文档可直接施工"的水平；ImGuiUE5_Roadmap 有"不做列表"、NextPhasePlan 有"延后表"，单点范围控制良好。
- 🔴 **高：进度事实源失准**。Progress.md 曾滞后约 2 个月（阶段五导出系统已完成却无记录）——**本次已回填**。
- 🟡 **中：多套 Phase 编号体系并存**（EnginePlan 0-5 / 2DFramework 1-6 / ImGui 0-6 / 字母 F-Z 且 Q、R 复用），跨文档引用易错位——建议今后用"日期+主题"命名阶段。
- 🟡 **中：设计与实现漂移**。2DFrameworkPlan 的 Box2D（已否决）、Tileset-passage、Prefab 等描述与实现相反——**本次已标记为历史快照**。
- 🟡 **范围蔓延隐忧**：横板模式零实现、3D/Server 长期占位——**本次已在 Progress.md 明确标记为暂停/远期**。

## 三、代码评估

### 亮点

1. **运行时/编辑器分层清晰且被严格执行**：`engine_runtime` 零 ImGui 依赖——本项目最重要的架构决策。
2. 自研 ECS 结构规整（sparse_set / world / 组件小文件化）；资源数据库按域拆分（Item/Skill/Enemy/Actor/Shop）。
3. Launcher 完成度较高（项目模板/导入/失效检测/主题/IME 中文输入均已落地）；Editor 功能密集（瓦片/事件/数据库/播放/导出闭环都在）。
4. 发布工程细节到位（UTF-8、MinGW 静态链接、DLL/着色器自动拷贝）；Windows 崩溃处理、DPI、工作目录等细节成熟。

### 主要问题（按严重度）

| # | 级别 | 问题 | 整改阶段 |
|---|---|---|---|
| 1 | 🔴 高 | `EditorContext` 上帝单例（.cpp 1109 行 + .h 373 行，40+ 方法揉合 8 类职责） | Phase F |
| 2 | 🔴 高 | 零测试：ECS、Vulkan 封装、序列化均无保护 | Phase E |
| 3 | 🟡 中 | 编辑器占代码 53%，头部 5 文件（659–1359 行）集中复杂度 | Phase F（部分） |
| 4 | 🟡 中 | 死代码：`platform/window.cpp` 未纳入构建、`trace_macros.h` 零引用、`EDITOR_LOG` 整体禁用、根 `MakeGames` demo 目标残留、`ProcessExitLogger` | Phase B/D |
| 5 | 🟡 中 | 占位与外观不符：DetailsPanel 假数据、ConsolePanel 假日志、Undo/Redo 未接线、Launcher 搜索框未接过滤 | Phase D |
| 6 | 🟡 中 | `test_proj/` 失效残留（非法 JSON、schema 不符） | Phase B |
| 7 | 🟢 低 | `g_pending_editor_path` extern 全局、裸 `catch(...)`、`file(GLOB)`、硬编码 VulkanSDK 路径、INTERFACE 链接滥用、main.cpp 崩溃处理重复、demo 脚本 mode 命名不一致 | Phase C |

## 四、评分（10 分制）

| 维度 | 分数 | 说明 |
|---|---|---|
| 架构设计 | 8 | 分层决策优秀并被坚持，扣在 EditorContext 单体化 |
| 功能完成度 | 7.5 | 核心闭环可用；横板/撤销/部分面板是缺口 |
| 代码质量 | 7 | 大部分文件小而整洁，头部大文件与死代码拖分 |
| 文档体系 | 6 → 8（整改后） | 单体高质量；状态同步机制缺失已修复 |
| 工程化 | 5 | 能构建能发布，但零测试、零 CI、路径硬编码 |

## 五、核心结论

完成度超出预期、文档驱动规范，但**事实源管理**曾失控。最大风险不是代码质量，而是：① 进度记录滞后导致决策基于错误状态（已修复）；② EditorContext 持续膨胀；③ 无测试使重构风险高。

整改路线：A 文档 → B 清理 → C 工程化 → D 功能补全 → E 测试安全网 → F EditorContext 拆分（8 小步，每步构建+ctest 验证）。
