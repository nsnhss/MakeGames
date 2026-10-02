# MakeGames 引擎架构规划

> ⚠️ **历史快照**：本文档为早期架构规划，Phase 勾选状态与部分设计（如 Box2D、双模式）已与实际实现漂移，**不再维护**。实际进度以 [`Progress.md`](../Progress.md) 为唯一事实源；横板/3D/Server 已标记为暂停/远期（见 Progress.md「已暂停 / 远期规划」）。

## 1. 项目现状

- **已有基础**：SDL2 (2.32.10)、Vulkan SDK (1.4.341.1)、ImGui（含 SDL2/Vulkan backends）
- **构建系统**：CMake + C++20 + MinGW (Windows)
- **编译器**：CLion 2025.2.5 捆绑 MinGW，GCC 13.1.0，CMake 4.2.1
  - 满足 Box2D v3（C17 + C11 atomics）和 Jolt（C++17）需求，**无需升级**
- **当前代码**：`main.cpp` 保留为过渡 Demo；`launcher/main.cpp` 是 Launcher 入口；Vulkan 逻辑已提取到 `RendererContext`
- **目标**：在此基础上构建一个模块化、可扩展的 **2D/3D 游戏开发平台**
  - 包含：Launcher（启动器）、Editor（编辑器）、Engine（引擎运行时）
  - 预留：Server（服务器框架）

---

## 2. 引擎架构总览

采用**分层架构**，从上到下依赖关系逐渐收敛：

```
┌─────────────────────────────────────────────┐
│  Launcher（项目启动器：项目管理、设置、日志）    │
├─────────────────────────────────────────────┤
│  Editor（基于定制 ImGui 的编辑器面板）         │
├─────────────────────────────────────────────┤
│  Scene（ECS / 组件系统）                       │
├─────────────────────────────────────────────┤
│  Renderer（Vulkan 渲染抽象）                   │
│  Resource（资源管理）                          │
├─────────────────────────────────────────────┤
│  Platform（SDL2 抽象：窗口/输入/事件）          │
├─────────────────────────────────────────────┤
│  Core（基础类型/数学库/日志/内存）              │
├─────────────────────────────────────────────┤
│  Server Stub（服务器接口预留）                  │
└─────────────────────────────────────────────┘
```

**程序入口拆分**：
- `launcher.exe`：轻量启动器，负责项目管理和设置
- `editor.exe`：完整引擎编辑器，由 Launcher 启动
- 共享代码编译为 `engine_core` 静态库

**原则**：上层可调用下层，下层不感知上层；跨模块通信通过 Event 系统解耦。

---

## 3. 目录结构规划

```
MakeGames/
├── cmake/                       # CMake 模块/FindPackage 脚本
├── docs/                        # 设计文档
│   └── EnginePlan.md            # 本文档
├── third_party/                 # 第三方库
│   ├── json.hpp                 # nlohmann/json v3.11.3（单头文件）
│   └── ...                      # SDL2、ImGui（目前仍在根目录，后续迁移）
├── assets/                      # 引擎默认资源（shader、字体、图标）
│
├── launcher/                    # 【新增】项目启动器（独立 exe）
│   ├── main.cpp                 # Launcher 入口
│   ├── core/
│   │   ├── theme.h/.cpp         # 主题系统（白天/黑夜）
│   │   └── project_manager.h/.cpp # 项目管理（JSON 读写）
│   └── ui/
│       ├── launcher_window.h/.cpp   # 主窗口布局
│       ├── project_panel.h/.cpp     # 项目列表卡片
│       ├── settings_panel.h/.cpp    # 设置面板
│       └── log_panel.h/.cpp         # 开发日志面板
│
├── editor/                      # 【新增】编辑器（独立 exe）
│   ├── main.cpp                 # Editor 入口
│   └── ...                      # 编辑器面板（Phase 4 实现）
│
├── src/                         # 引擎核心（共享静态库）
│   ├── core/                    # 核心层
│   │   ├── log.h/.cpp           # 日志系统
│   │   ├── timer.h/.cpp         # 计时器/时间步管理
│   │   └── math/                # 数学库 (Vec2, Color)
│   │       ├── vec2.h
│   │       └── color.h
│   │
│   ├── platform/                # 平台抽象层 (基于 SDL2)
│   │   ├── window.h/.cpp        # 窗口创建/销毁/属性
│   │   ├── input.h/.cpp         # 键盘/鼠标输入封装
│   │   └── event.h/.cpp         # 事件系统
│   │
│   ├── renderer/                # Vulkan 渲染层
│   │   ├── renderer_context.h/.cpp  # Instance / Device / Queue / DescriptorPool
│   │   ├── swapchain.h/.cpp     # SwapChain / Surface / 呈现控制
│   │   ├── pipeline.h/.cpp      # Pipeline / PipelineLayout / RenderPass 封装
│   │   ├── shader.h/.cpp        # ShaderModule 加载
│   │   ├── buffer.h/.cpp        # VertexBuffer / UniformBuffer
│   │   ├── texture.h/.cpp       # Image / ImageView / Sampler
│   │   └── descriptor.h/.cpp    # DescriptorSet / DescriptorPool / Layout
│   │
│   ├── resource/                # 资源管理
│   │   ├── resource_manager.h/.cpp  # 纹理+Sampler DS 单例管理
│   │   ├── data_manager.h/.cpp  # JSON 文件 I/O + 缓存
│   │   ├── meta_file.h          # .meta.json GUID 系统
│   │   ├── guid.h               # UUID v4 生成
│   │   └── schema/              # JSON Schema 定义
│   │       └── project_schema.h
│   │
│   ├── scene/                   # 场景系统（ECS）
│   │   ├── entity.h             # Entity (ID)
│   │   ├── component.h/.cpp     # Component 基类/注册表
│   │   ├── world.h/.cpp         # ECS World
│   │   ├── scene.h/.cpp         # Scene 容器
│   │   ├── scene_serializer.h/.cpp  # JSON 序列化/反序列化
│   │   ├── event_system.h/.cpp  # RPG Maker 风格事件系统
│   │   ├── action_commands.h    # 22 个内置动作命令枚举
│   │   ├── components/          # 组件定义（header-only）
│   │   │   ├── transform.h
│   │   │   ├── sprite.h
│   │   │   ├── animator.h
│   │   │   ├── camera.h
│   │   │   ├── camera_follow.h
│   │   │   ├── name.h
│   │   │   ├── tag.h
│   │   │   ├── event_trigger.h
│   │   │   ├── movement.h
│   │   │   └── tilemap.h
│   │   └── systems/             # ECS System
│   │       ├── sprite_render_system.h/.cpp
│   │       ├── tilemap_render_system.h/.cpp
│   │       ├── camera_system.h/.cpp
│   │       └── animator_system.h/.cpp
│   │
│   ├── audio/                   # 【新增】音频系统
│   │   └── audio_manager.h/.cpp # SDL2 音频：PlaySE / PlayBGM
│   │
│   ├── editor/                  # 编辑器层 (基于定制 ImGui)
│   │   ├── editor_app.h/.cpp    # 编辑器主应用类（面板注册+布局）
│   │   ├── editor_application.h/.cpp  # Vulkan/ImGui 初始化+帧渲染
│   │   ├── editor_context.h/.cpp      # 编辑器全局状态（场景/相机/Tileset）
│   │   ├── editor_layer.h/.cpp        # Play Mode 系统调度
│   │   ├── editor_layout.h/.cpp
│   │   ├── editor_menu_bar.h/.cpp
│   │   ├── editor_tool_bar.h/.cpp
│   │   ├── tileset_asset.h      # Tileset 定义
│   │   ├── file_dialog.h/.cpp   # Windows 原生文件对话框
│   │   ├── panel.h/.cpp         # 面板基类
│   │   ├── panels/              # 各编辑器面板
│   │   │   ├── outliner_panel.h/.cpp
│   │   │   ├── details_panel.h/.cpp
│   │   │   ├── content_browser_panel.h/.cpp
│   │   │   ├── viewport_panel.h/.cpp
│   │   │   ├── console_panel.h/.cpp
│   │   │   ├── event_editor_panel.h/.cpp
│   │   │   └── tile_palette_panel.h/.cpp
│   │   └── imgui/               # 【ImGui 定制层】
│   │       ├── mg_imgui_theme.h/.cpp
│   │       ├── mg_imgui_extensions.h/.cpp
│   │       ├── mg_imgui_panel.h/.cpp
│   │       └── mg_imgui_icons.h
│   │
│   └── app/                     # 应用层
│       ├── application.h/.cpp   # Engine 主循环、模块初始化、Layer 栈
│       └── layer.h/.cpp         # Layer 抽象 (Update/Render/OnEvent)
│
├── CMakeLists.txt               # 根 CMake，定义 launcher + editor + engine_core
└── main.cpp                     # 【保留过渡】原 Demo 入口，逐步废弃
```

---

## 4. 关键模块设计

### 4.1 Launcher（新增）

**定位**：轻量项目启动器，用户打开的第一个界面。

**界面布局**：
```
┌─────────────────────────────────────────┐
│  MakeGames Launcher        [设置] [?]   │
├─────────────────────────────────────────┤
│                                         │
│  ┌──────────────┐  ┌──────────────┐     │
│  │  项目 A      │  │  项目 B      │     │
│  │  最后打开... │  │  最后打开... │     │
│  │  [打开]      │  │  [打开]      │     │
│  └──────────────┘  └──────────────┘     │
│                                         │
│  [+ 创建新项目]    [打开已有项目]        │
│                                         │
├─────────────────────────────────────────┤
│  开发日志                               │
│  2026-05-11: 完成了 Launcher 骨架...    │
│  2026-05-10: 确定了架构选型...          │
│                                         │
└─────────────────────────────────────────┘
```

**核心功能**：
- **项目列表**：读取 `projects/` 目录或用户配置，展示最近项目卡片
- **创建项目**：弹出对话框，输入项目名称，在磁盘创建项目文件夹 + `project.json`
- **加载项目**：文件对话框选择已有项目，验证 `project.json` 后启动 `editor.exe`
- **主题切换**：全局设置白天/黑夜两套 ImGui 风格
- **开发日志**：文本输入框记录每日完成内容，持久化到本地文件

**项目文件格式（JSON）**：
```json
{
    "name": "MyPlatformer",
    "engine_version": "0.1.0",
    "mode": "2d_sidescroller",
    "start_scene": "maps/Map001.json",
    "last_opened": "2026-05-11T23:54:43",
    "created_at": "2026-05-01T12:00:00"
}
```
> `mode` 字段区分项目模板：`2d_topdown`（俯视角，类似 RPG Maker）、`2d_sidescroller`（横板，类似平台跳跃）、预留 `3d`。

### 4.2 Server 预留

> **状态**：仅设计阶段，源码树中尚无 `src/server/` 目录。接口设计已冻结，待 Phase 5+ 实现。

**接口设计**：
```cpp
class IServerClient {
public:
    virtual bool Login(const std::string& username, const std::string& token) = 0;
    virtual bool PublishProject(const std::string& project_id) = 0;
    virtual std::vector<NewsItem> FetchEngineUpdates() = 0;
};
```

**初期实现**：`LocalServerClient` 返回本地占位数据，不做真实网络请求。后续替换为 `HttpServerClient`。

### 4.3 Core 层

- **Math**：手写轻量数学库，当前实现 `Vec2`、`Color`。`Mat4`、`AABB` 等已预留接口，待 3D Phase 实现。
- **Log**：分级日志，输出到控制台 + 文件 + ImGui Console 面板
- **Timer**：DeltaTime, FPS, FixedTimestep

### 4.4 Platform 层

对 SDL2 做 thin wrapper：
- `Window`：封装 `SDL_Window`，提供尺寸/标题/全屏/VSync 控制
- `Input`：`IsKeyPressed(KeyCode)`, `GetMousePosition()`
- `Event`：引擎内部事件结构，回调分发

### 4.5 Renderer 层

当前 `main.cpp` 的 Vulkan 代码拆解为面向对象封装：
- `RendererContext`：集中管理 VkInstance / VkDevice / VkQueue / VkDescriptorPool
- `SwapChain`：Surface / SwapChain / 呈现控制
- `Pipeline` / `Shader` / `Buffer` / `Texture` 等资源类
- ImGui 与引擎渲染共存，共享 Vulkan Context

### 4.6 Scene 层（ECS）

纯 ECS 架构：
- **Entity**：轻量 ID（`uint32_t`）
- **Component**：纯数据结构（header-only，`REGISTER_COMPONENT` 宏自动注册）
- **System**：逻辑处理器，`Update` / `Render` 阶段分批执行
- **Scene**：管理 Entity/Component 生命周期
- **EventSystem**：RPG Maker 风格事件系统，支持 22 个内置命令（对话/分支/移动/UI/音频等）

**当前组件（10 个）**：
`Transform`, `Sprite`, `Animator`, `Camera`, `CameraFollowTarget`, `Name`, `Tag`, `EventTrigger`, `Movement`, `Tilemap`

**当前系统（4 个）**：
`SpriteRenderSystem`, `TilemapRenderSystem`, `CameraSystem`, `AnimatorSystem`

**DLL 热重载预留**：
- System 接口按 DLL 边界设计，主程序与逻辑 DLL 通过纯虚基类通信
- Component 数据由主程序管理，System 只读取/写入

### 4.7 Resource 层

- **DataManager**：JSON 文件读写 + 内存缓存（`data_manager.h/.cpp`）
- **GUID 系统**：`guid.h` 生成 UUID v4，`meta_file.h` 管理 `.meta.json`
- **ResourceManager**：纹理 + Sampler 的 DescriptorSet 缓存单例（非引用计数）
- 统一 `assets/` 根目录，编辑器通过 Windows 原生对话框导入外部文件

### 4.8 Editor 层

基于 ImGui 构建游戏内编辑器（Play-in-Editor）：
- **Outliner Panel**：Entity 层级树（双击定位、右键菜单）
- **Details Panel**：Component 属性编辑（含 Sprite Texture 浏览、Animator Inspector）
- **Viewport Panel**：游戏画面预览 + PlayMode HUD 渲染（Dialog/Choices/Toast/Status/Inventory/Title）
- **Console Panel**：Log 输出过滤/搜索
- **Content Browser Panel**：文件系统浏览 + 导入 + 搜索/过滤
- **Event Editor Panel**：RPG Maker 风格事件页编辑（22 个命令可视化编辑）
- **Tile Palette Panel**：Tileset 创建/切换、瓦片 brush 选择
- **Menu Bar**：Play / Pause / Stop

**运行模式**：
- `EditMode`：可修改场景，不运行游戏逻辑 System
- `PlayMode`：克隆场景运行游戏逻辑（`EventSystem` + `AnimatorSystem` + `AudioManager`）

---

## 5. 新增需求记录

以下为用户明确提出的新增需求：

| # | 需求 | 说明 | 阶段 |
|---|------|------|------|
| 1 | GitHub 发布能力 | 项目结构清晰、跨平台构建、可迁移 | 全程 |
| 2 | 服务器框架预留 | 未来支持登录、内容发布、多人协作 | Phase 1 留接口 |
| 3 | Launcher 主界面 | 创建项目、加载项目、项目列表 | Phase 0 |
| 4 | 主题切换 | 全局白天/黑夜两套 ImGui 风格 | Phase 0 |
| 5 | 开发日志（个人） | 用户自己记录每日完成内容 | Phase 0 |
| 6 | 引擎更新日志 | 显示引擎版本更新信息（本地/服务器） | Phase 0 |
| 7 | 制作日志（公开） | 游戏制作人撰写的项目日志，可分享 | Phase 3+ |
| 8 | 先做界面后补功能 | Phase 0 以界面骨架为主，逻辑逐步填充 | Phase 0 |

---

## 6. 决策记录

以下为已确认的关键架构决策：

| # | 决策项 | 选择 | 理由 |
|---|--------|------|------|
| 1 | 场景系统 | **B. 纯 ECS** | 性能最好，数据布局友好，与 C++ 热重载配合 |
| 2 | 数学库 | **A. 自研轻量 Math** | 编译快、可控、零依赖，方便 SIMD 优化 |
| 3 | 2D/3D 优先 | **C. 架构预留 3D，先落地 2D** | 接口按 3D 设计，先实现正交相机和 2D Sprite |
| 4 | 脚本系统 | **Lua 为主，C++ DLL 为辅** | Lua 轻量易学，非程序员能上手做 2D 游戏逻辑；C++ DLL 给专业用户兜底，System 仍按 DLL 边界设计 |
| 5 | 物理系统 | **已拒绝 Box2D v3** | 2D 项目当前仅需简单 tile collision，不引入物理引擎。若后续需要 AABB 碰撞，手写实现；3D 阶段直接引入 Jolt |
| 6 | Launcher/Editor | **B. 两个独立 exe** | 类似 Unity Hub，Launcher 轻量可独立更新 |
| 7 | 项目元数据 | **A. JSON** | 主流选择，nlohmann/json 单头文件零编译依赖 |
| 8 | 开发顺序 | **B. 先做 Launcher 界面** | 用户要求尽快看到界面成果，先做 Phase 0 |

---

## 7. 开发阶段建议

### Phase 0：Launcher 骨架（已完成，待 UI 微调）
- [x] 下载并集成 nlohmann/json
- [x] 从 `main.cpp` 提取最小共享层（`Platform::Window` + `RendererContext`）
- [x] 重构 `CMakeLists.txt`：定义 `engine_core` 静态库 + `launcher` 可执行目标
- [x] 实现 `Theme` 系统（白天/黑夜两套 ImGui 风格）
- [x] 实现 Launcher 主界面（Qt 风格左侧 Places 导航 + 右侧内容区）
- [x] 实现 `ProjectManager`（JSON 读写，假数据填充项目列表）
- [x] 实现开发日志面板（文本输入 + 历史记录持久化）
- [x] 中文字体支持（系统字体 `msyh.ttc`）
- [x] 静态链接 MinGW 运行时（发布无需额外 DLL）
- **里程碑**：Launcher 可独立运行，显示主界面，能切换主题，项目列表和日志面板可见
- **待优化**：Launcher 布局在高 DPI 下仍显拥挤，需继续调整

### Phase 1：基础重构 ✅ 已完成
- [x] 完善 Core 层（Log、Math、Timer）
- [x] 完善 Platform 层（Window、Input、Event）
- [x] 完善 Application + Layer 系统
- [x] Launcher → Editor 启动衔接（命令行传递项目路径）
- [x] 主循环跑通
- **里程碑**：代码结构化，编译通过，窗口正常显示

### Phase 2：Vulkan 渲染抽象 ✅ 已完成
- [x] 封装 `RendererContext`（Instance / Device / Queue / DescriptorPool）
- [x] 封装 `SwapChain`、`CommandBuffer`
- [x] 封装 `Pipeline`、`Shader`、`Buffer`、`Texture`
- [x] 基础 2D 渲染：纯色/纹理 Quad
- [x] ImGui 与引擎渲染共存（Viewport Offscreen RT → `ImGui::Image`）
- [ ] **技术债务 TD-1：提取共享 Vulkan+ImGui 初始化** —— `launcher_application.cpp` 和 `editor_application.cpp` 的 `OnInit()` 存在 ~48 行重复代码
- **里程碑**：屏幕上同时显示引擎渲染的 Quad 和 ImGui 窗口

### Phase 3：场景与资源系统 ✅ 已完成
- [x] ECS 核心（Entity 分配、Component 存储、System 调度）
- [x] `Transform`、`Sprite`、`Camera` Component + `Animator` Component
- [x] `ResourceManager` + 纹理加载
- [x] `SpriteRenderSystem` / `TilemapRenderSystem` / `CameraSystem`
- [x] ~~引入 Box2D v3~~ — **已拒绝**（改用简单 AABB tile collision）
- **里程碑**：可创建多个带图片的 Entity，相机控制视角，有简单物理和 Tilemap 碰撞

### Phase 4：编辑器与视口 ✅ 已完成
- [x] Renderer 支持 Render to Texture
- [x] Editor Viewport 面板
- [x] Scene Panel（Outliner）+ Inspector（Details）
- [x] Console 面板接入 Log
- [x] Play/Stop 模式切换
- [x] Tile Palette + Tilemap 编辑器
- [x] Content Browser + 资产导入
- [x] Event Editor 面板
- **里程碑**：纯 GUI 操作搭建简单 2D 场景并运行

**Viewport 渲染集成方案**：
Editor Viewport 面板目前为纯 ImGui 装饰几何（网格/坐标轴）。当引擎 2D 渲染管线就绪后，集成路径如下：
1. **创建额外 Render Target**：在 `RendererContext` 中分配独立的 `VkImage` + `VkFramebuffer`（尺寸跟随 Viewport 面板内容区），作为游戏画面的离屏渲染目标。
2. **ECS RenderSystem 写入该 Target**：`SpriteRenderSystem` / `TilemapRenderSystem` 使用独立的 RenderPass（与 SwapChain 的 present pass 分离），将场景内容渲染到上述 framebuffer。
3. **ImGui::Image 显示**：每帧将 Render Target 的 `VkImageView` 包装为 `ImTextureID`（通过 `ImGui_ImplVulkan_AddTexture`），在 `ViewportPanel::Draw()` 中调用 `ImGui::Image()` 绘制到面板内容区。
4. **RenderPass 协调**：SwapChain 的 main pass 先执行引擎 render（写入 offscreen target），再执行 ImGui render（读取该 target 作为 texture 并绘制 UI），最后 present。两者共享同一个 `VkCommandBuffer`，通过 `VkSubpassDependency` 或 pipeline barrier 保证图像布局从 `COLOR_ATTACHMENT_OPTIMAL` 过渡到 `SHADER_READ_ONLY_OPTIMAL`。
5. **尺寸同步**：监听 Viewport 面板 `ImGui::GetContentRegionAvail()` 变化，动态重建 framebuffer（延迟一帧以避免每帧重建）。

### Phase 5：3D 与 Jolt（后期扩展）
- [ ] 3D Mesh / Model 加载 (obj/gltf)
- [ ] 透视相机、深度缓冲
- [ ] 基础光照
- [ ] 引入 Jolt Physics（3D 物理）
- [ ] Post-processing / Render Graph
- **里程碑**：能加载并显示 3D 模型，有基础光照和 3D 物理

---

## 8. 技术债务记录

> 以下问题不影响当前功能正确性，但会在长期维护中积累成本，按计划逐步清偿。

| ID | 来源 | 问题 | 影响 | 建议清偿阶段 |
|----|------|------|------|-------------|
| TD-1 | CodeIssues #19 | `launcher_application.cpp` 与 `editor_application.cpp` 的 Vulkan+ImGui 初始化代码高度重复 | 修改初始化逻辑（如新增 ImGui 配置、换 Vulkan 扩展）时需改两处，易遗漏 | ✅ 已提取到 `mg_imgui_backend.h/.cpp` |
| TD-2 | CodeIssues #18 | `engine_core` 链接依赖用 `INTERFACE` 传递，但静态库本身不需要被链接 | CMake 惯例修正，功能无影响 | 随时（已改） |
| TD-3 | CodeIssues #15 | `platform/window.cpp` 从 `engine_core` 移除编译后，文件仍保留在源码树中 | 未来应决定是彻底删除还是重新集成到 `Application` 中 | Phase 1 结束 |
| TD-N | Phase F-G 实现 | Sprite 每实体 1 draw call，无批处理/实例化 | 实体数 >100 时 GPU 瓶颈明显 | Phase 4+ |
