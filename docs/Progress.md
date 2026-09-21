# 开发进度与注意事项

> **重要变更（2026-05-22）**：自研 UI 框架（`mg::uifw` / `src/ui_framework/`）已彻底废弃并删除。
> 当前路线：直接修改 ImGui 源码实现 UE5 级编辑器界面。详见 `docs/ImGuiUE5_Roadmap.md`。

---

## 踩坑备忘（仍然有效）

### 1. MinGW UTF-8 编码
- Windows MinGW 编译器默认使用系统本地编码（GBK）处理源代码中的字符串字面量。
- **必须**在 `CMakeLists.txt` 中为 GCC/Clang 添加：
  ```cmake
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
      add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
  endif()
  ```
- 即使源文件保存为 UTF-8，没有这两个选项，运行时中文字符仍然是 GBK 编码。

### 2. Vulkan Push Constant 对齐
- GLSL `std430` 布局要求 `vec4` 在 16 字节边界对齐。
- 如果 push constant block 以 `vec2` 开头，后面紧跟 `vec4`，必须在 `vec2` 后补 8 字节 padding。
- C++ 端的 `sizeof(push)` 可以和 shader 实际使用的大小不同，但 **pipeline layout 的 `size` 必须是两者中较大者的倍数**（通常补齐到 16 的倍数）。
- 本项目中 C++ 传 80 字节（`float[20]`），shader 实际使用 68 字节，这是安全的。

### 3. 颜色对比度
- 深色主题下，相邻面板的背景色差异至少要在 RGB 各通道上有 20+ 的差值，否则在屏幕录制/截图压缩后几乎融为一体。
- UE5 风格使用明确的层级：`#0F0F0F` → `#151515` → `#1A1A1A` → `#212121`，差异足够清晰。

### 4. Windows 编译环境 PATH 问题
- 详见 `docs/AgentNotes.md`。高频踩坑点：Shell 直接调用 ninja/g++ 前必须设置 PATH，否则 `cc1plus.exe` 静默崩溃（返回码 1，无任何输出）。

### 5. ImGui 样式栈与动态主题切换（2026-05-27）
- **绝对不要**在 `PushStyleColor(...)` / `PopStyleColor()` 的保护区内调用主题切换函数（如 `MG_ApplyTheme_UE5Dark()`）。
- `PopStyleColor()` 恢复的是 `Push` 时备份的旧值，会把新主题当场回滚，导致下一帧重新 `Push` 时拿到的是旧颜色。
- **正确做法**：让 `BeginChild`/`Begin` 直接使用全局样式，不在 `ChildBg` / `WindowBg` 上套 `PushStyleColor/PopStyleColor`，主题切换自然生效。

### 6. 构造函数中调用虚函数导致初始化被跳过（2026-05-28）
- **现象**：`launcher.exe` / `editor.exe` 启动即崩溃 `0xC0000005`，`ImGui::GetCurrentContext()` 返回 `nullptr`，`swapchain_.GetImageCount()` 返回 `0`。
- **根因**：`Application` 基类构造函数末尾调用 `OnInit()`（虚函数）。C++ 规定：**基类构造期间派生类尚未构造完成，虚函数调用会被解析到基类版本**。因此 `LauncherApplication::OnInit()` / `EditorApplication::OnInit()` 从未执行。
- **修复**：将 `OnInit()` 调用从构造函数移到 `Application::Run()` 开头；析构函数中的 `OnShutdown()` 同理移除（`Run()` 末尾已调用）。
- **教训**：**永远不要在构造函数 / 析构函数中调用期望派生类重写的虚函数**。如需在构造后执行初始化，使用 `Run()` 等显式入口点，或工厂模式 / 两阶段构造。

### 7. 调试纪律：先上追踪代码，再分析（2026-05-28）
- **规则**：遇到崩溃或异常行为，如果在 **5 分钟内** 无法通过静态代码阅读定位根因，**立即停止分析，立刻插入追踪代码（`fprintf` / `TRACE` / `printf`）**。
- **原因**：推测和静态分析容易陷入"看起来合理"的陷阱，而实际运行时状态（指针值、布尔值、函数是否被调用）往往与假设完全不同。
- **执行标准**：
  1. 追踪代码必须覆盖"从程序入口到崩溃点"的**完整路径**，不能只在崩溃函数内部打 log。
  2. 必须打印**具体数值**（指针、计数、布尔），不能只打印"进入了某函数"。
  3. 追踪代码优先插入**自己的代码**；如需修改第三方库（如 ImGui），修改后必须立刻标记，修复后第一时间恢复。

---

## 已废弃内容（仅作存档，代码已删除）

- ~~自研 UI 框架（`src/ui_framework/`）~~ — 已删除
- ~~Flexbox 布局引擎~~ — 已删除，改用 ImGui 原生布局
- ~~FontAtlas 预渲染字符集限制~~ — 已删除，改用 ImGui 字体系统
- ~~Tween/Spring 动画系统~~ — 已删除
- ~~Vulkan 2D UI 渲染管线~~ — 已删除，改用 ImGui 渲染

---

## 当前进度（编辑器界面改造）

> 以下 Phase 为**编辑器 ImGui 界面改造**的进度记录，与 `EnginePlan.md` 中的引擎核心 Phase 相互独立。

### Phase 0：清理废弃代码 ✅ 已完成（2026-05-22）
- [x] 删除 `src/ui_framework/`（自研 UI 框架全部源码）
- [x] 删除 `src/ui/`（旧版主题文件）
- [x] 删除 `imgui_backup/`、`test_ninja/`、`launcher/test_ui.cpp`
- [x] 删除所有日志/编译产物/测试文件
- [x] 更新 `CMakeLists.txt` 移除废弃引用
- [x] 重写 `docs/UIPlan.md`、`docs/Progress.md`
- [x] 更新 `docs/EnginePlan.md` 架构描述
- [x] 撰写 `docs/ImGuiUE5_Roadmap.md` 详细路线图

### Phase 1：主题系统 ✅ 已完成（2026-05-27）
- [x] 创建 `src/editor/imgui/mg_imgui_theme.cpp`
- [x] 实现 `MG_ApplyTheme_UE5Dark()` / `MG_ApplyTheme_UE5Light()`
- [x] 实现字体合并加载（微软雅黑 + FontAwesome）
- [x] 修复主题切换后 Content 背景色被 `PopStyleColor` 回滚的 bug

### Phase 2：扩展控件库 ✅ 已完成（2026-05-27）
- [x] 创建 `src/editor/imgui/mg_imgui_extensions.h/cpp`
- [x] 实现 Property Grid / Asset Card / Toolbar / TreeNode / StatusBar
- [x] Details / Outliner / Content Browser / ToolBar 接入扩展控件

### Phase 3：ImGui 源码微调 ✅ 已完成（2026-05-28）
- [x] RenderArrow → chevron 风格
- [x] TabItem 底部直角
- [x] CollapsingHeader 高度微调
- [x] CheckMark / Arrow 精致化
- [x] Table Header 行高增加

### Phase 4：编辑器面板骨架 ✅ 已完成（2026-05-27）
- [x] DockSpace 默认布局
- [x] Scene Outliner / Details / Content Browser / Viewport / Console / MenuBar / ToolBar

### Phase 5：Launcher 重皮肤 ✅ 基本完成（2026-05-28）
- [x] Accent 颜色统一从主题读取
- [x] 卡片阴影、主题切换、新建项目向导、项目重新定位
- [x] DevLog 页面（只读/编辑双模式 + Inline IME Composition）
- [x] 崩溃追踪（未处理异常过滤器 + crash.log）
- [ ] 项目卡片完全改用 `MG_AssetCard` 风格（当前独立实现，风格已接近，可暂不迁移）

### Phase 6：打磨与细节 ✅ 已完成（2026-05-28）
- [x] 编辑器 / Launcher 默认最大化
- [x] Launcher 工作目录锁定
- [x] DPI 缩放适配（PerMonitorV2）
- [x] 卡片颜色过渡与光晕
- [x] **IME 候选框位置问题已根治**（`SDL_SetWindowsMessageHook` + `CFS_EXCLUDE` + backend 迁移）
- [x] hover / active 颜色过渡动画
- [x] subtle 内发光 / 外发光效果
- [x] Launcher → Editor 启动衔接

---

## IME Inline Composition 迁移记录（2026-05-28）

### 根因与修复

| 问题 | 根因 | 修复 |
|------|------|------|
| 候选框遮挡拼音（首字符） | `WM_IME_STARTCOMPOSITION` 重置 `CANDIDATEFORM` 为默认位置 | `SDL_SetWindowsMessageHook` 拦截，立即重建 `CFS_EXCLUDE` |
| 帧率 10 FPS | `TRACE_SDL2()` 每帧 10 次 `fprintf(stderr)`，CLion 终端 stderr 同步阻塞 | 删除宏及 10 处调用 |
| 双插（拼音+汉字同时出现） | `SDL_TEXTINPUT` 检查条件误用 `g_SDLCompositionText[0]`，commit 后仍走 `AddInputCharactersUTF8` | 引入持久 `g_SDLIMEComposing` 标志 |

### 架构迁移

原 ~150 行 launcher 专属 IME 代码（`LogEditCallback`、`CalcCursorScreenPos`、post-call 渲染）全部迁入 `imgui_impl_sdl2.cpp`：

```cpp
// 新增 API（imgui_impl_sdl2.h）
int  ImGui_ImplSDL2_InputTextCallback(ImGuiInputTextCallbackData* data);
void ImGui_ImplSDL2_RenderTextInputIME(const char* buf, ImU32 underline_color);
void ImGui_ImplSDL2_GetTextIMESavedPos(ImVec2* out_pos, float* out_line_height, float* out_widget_right);
```

launcher 调用简化为 2 行：
```cpp
ImGui::InputTextMultiline("##edit", buf, size, ImVec2(-1,-1),
    ImGuiInputTextFlags_CallbackAlways, ImGui_ImplSDL2_InputTextCallback);
ImGui_ImplSDL2_RenderTextInputIME(buf, underline_color);
```

- 状态管理：`TextIMEState` 嵌入 `ImGui_ImplSDL2_Data`
- 多 widget 竞争保护：`RenderTextInputIME` 中 `IsItemActive()` 非 active 不更新坐标
- Hook 注册：从 `launcher_application.cpp` 迁入 `ImGui_ImplSDL2_Init`，backend 自包含

---

## Editor 精致化（第二轮）✅ 已完成（2026-05-28）

> 第一轮（Phase 1-6）已完成主题、扩展控件、面板骨架。当前 editor "能用但不精致"——缺面板 chrome、viewport 占位、console 空白、零动画。
> 本轮目标：从"骨架"到"有质感的产品界面"。

### P0：Editor IME 接入 ✅
- [x] 所有 `InputTextMultiline` / `InputText` 接入 `ImGui_ImplSDL2_InputTextCallback` + `ImGui_ImplSDL2_RenderTextInputIME`
- [x] Console 搜索框、属性面板文本字段均获得 inline composition（代码编辑器暂缺）

### R1：Panel Chrome 引擎 ✅
- [x] 新增 `MG_BeginPanel` / `MG_EndPanel`（在 `ImGui::Begin` 后画自定义 header 条，不替换 docking 逻辑）
- [x] Header：左侧图标 + 标题，右侧关闭按钮（齿轮菜单预留）
- [x] 5 个面板全部接入，docked 时自动隐藏自定义 header 避免双标题

### R2：Viewport 诊断几何 ✅
- [x] 极深色背景 `#121212`
- [x] 20px 半透明网格线
- [x] 红/绿坐标轴（X/Y，长 60px）
- [x] 中心白色十字准线

### R3：Console 升级 ✅
- [x] 日志级别着色（0=Info 白 / 1=Warn 橙 / 2=Error 红）
- [x] Header 按钮行：清除（图标化）+ 自动滚动（图标化）+ 级别过滤 Combo + 搜索框
- [x] 命令输入框接入 IME

### R4：动画 + 状态栏 ✅
- [x] `MG_GetAnimatedColor` 接入 AssetCard hover、ToolbarButton active
- [x] `MG_StatusBarBegin/End/Text` 接入 editor 底部（DockSpace 之后、End 之前）

### R5：ImDrawList 级增强（不改 ImGui 源码） ✅
- [x] **5a** Outliner / Content Browser 选中项左侧 2px accent 竖线
- [x] **5b** Details Section Header（圆点 + 标题 + 延伸细线），替换裸 `Separator()`
- [x] **5c** Tree 缩进引导线（半透明竖线连接父子层级）—— ✅ 已实现（2026-09-07，见文末收尾批次）
- [x] **5d** Content Browser 卡片 hover accent 边框动画（`MG_GetAnimatedColor` 过渡）

### R6：ImGui 源码级微调（3 处 ~20 行）
- [x] **6a** Tab 顶部 2px accent 激活指示器（`imgui_widgets.cpp` `TabItemBackground`）——A1 审阅确认已提前实现
- [x] **6b** Dock Splitter 增大到 6px + hover 变亮（`imgui.cpp` `DockNodeUpdateSplitter`）——同上
- [x] **6c** Scrollbar grab hover 加宽（`imgui_draw.cpp` `ScrollBar`）——同上

### 明确不做（投入产出比差）
- Panel collapse 动画（ImGui 生命周期不支持中间帧）
- 浮动窗口阴影（当前全部 docked，无浮动 panel 场景）
- Viewport 3D 渲染（属性编辑器框架阶段不该碰）
- 代码 minimap（无代码编辑器场景）
- 自定义字体渲染（当前微软雅黑已够好）

---

## 引擎核心开发

### Phase 1：基础架构 ✅ 已完成
- [x] **Core 层**：`Log` 系统（5 级日志 + Sink 机制 + Ring Buffer 历史）
- [x] **Core 层**：`Timer` / `FixedTimeStep`（`double` 精度、FPS 滑动平均、最大 dt 限制）
- [x] **Math 层**：`Vec2`（2D 向量运算）、`Color`（RGBA + 预定义常量）
- [x] **Platform 层**：`Window`（SDL2 封装 + DPI PerMonitorV2）、`Input`（键盘/鼠标状态 + 按键枚举）
- [x] **App 层**：`Application` 基类（SDL 初始化、主循环 `Run`、事件分发、Layer 栈调度、最小化降频）
- [x] **App 层**：`Layer` 抽象 + `LayerStack`（普通 Layer + Overlay 优先级管理）
- [x] **子类实现**：`EditorApplication` / `LauncherApplication`（Vulkan + ImGui 初始化、帧渲染、Offscreen RT）
- [x] **修复**：构造函数中虚函数调用导致初始化跳过（`OnInit`/`OnShutdown` 移到 `Run()` 中显式调用）

### Phase 2：Vulkan 渲染器 ✅ 已完成
- [x] `RendererContext`（Instance、Device、Queue、DescriptorPool、Debug Report）
- [x] `SwapChain`（Surface、ImageViews、RenderPass、Framebuffers、Resize 重建、Acquire/Present）
- [x] `Shader`（SPIR-V → `VkShaderModule` + Pipeline Stage CreateInfo）
- [x] `Pipeline`（基于 Config 一键创建 Graphics Pipeline，含顶点布局、动态状态、PushConstant）
- [x] `Buffer`（Host-Visible 直接 Map + Device-Local Staging 上传）
- [x] `DescriptorSetLayout` / `DescriptorSet`（UBO/SSBO/Texture 绑定）
- [x] `Texture`（PNG 加载 / 内存 RGBA 创建、Image/View/Sampler、移动语义）
- [x] **基础 2D 渲染**：正交相机 UBO + 纯色/纹理 Quad（Sprite + Tilemap 共享 shader）
- [x] **ImGui 与引擎渲染共存**：Viewport Offscreen Render Target → ImGui::Image 显示
- [x] **技术债务 TD-1**：提取共享 Vulkan+ImGui 初始化到 `mg_imgui_backend.h/.cpp` ✅
- [x] **技术债务 TD-N**：Sprite 批处理/实例化（2026-08-14 完成：instance rendering + 整帧单次上传 + 分组偏移 draw，见 Phase Z4 与后续图集批次）
- [x] **技术债务 TD-P**：碰撞图块合并为更少形状（2026-08-14 完成：`src/scene/systems/collision_merge.h/.cpp` 纯逻辑 run 合并器，播放模式碰撞收集接入，面积等价性单测覆盖）
- [x] **技术债务 TD-P**：碰撞图块合并为更少形状（2026-08-14 完成：`src/scene/systems/collision_merge.h/.cpp` 纯逻辑 run 合并器，播放模式碰撞收集接入，面积等价性单测覆盖）

### Phase 2.5：ECS + 场景系统 ✅ 已完成（2026-05-28）
- [x] ECS Core：`EntityID`、`IComponent`、`ComponentRegistry`、`SparseSet<T>`、`World`
- [x] 组件查询：单组件 `Query<T>`、多组件交集 `Query<T, U>`
- [x] **10 个 ECS Component**：
  - `Transform`（位置、旋转、缩放、渲染排序层级）
  - `Sprite`（纹理路径、锚点、tint、翻转、动画帧索引）
  - `Animator`（spritesheet 帧动画：帧数、列数、FPS、播放状态）
  - `Camera`（正交投影参数、视口、背景色、跟随目标/速度/死区）
  - `CameraFollowTarget`（标记哪个 entity 被相机跟随）
  - `Name`（实体名称）
  - `Tag`（字符串标签数组）
  - `EventTrigger`（RPG Maker 风格事件页：触发类型、条件、动作序列、自身开关、run_once）
  - `Movement`（速度、移动速度、TopDown/Platformer 模式）
  - `Tilemap`（tileset 引用、tile 尺寸、多层 TileLayer，支持 visible/collision/edge_thickness/Resize）
- [x] `Scene` 类（World 容器 + 地图元数据：名称、尺寸、瓦片大小、模式）
- [x] `SceneSerializer`（JSON 序列化/反序列化，支持所有 9 个组件 + 全局变量/开关）

### Phase 2.6：事件系统 ✅ 已完成（2026-05-28）
- [x] `EventSystem`：触发器检测 + 动作解释器 + 执行上下文栈
- [x] 触发类型：`OnStart`、`OnInteract`、`OnEnter`、`OnTouch`、`OnVariableChange`
- [x] 条件评估：变量比较（int/float，6 种运算符）、开关布尔、自身开关
- [x] 全局状态：`int_variables_`、`float_variables_`、`bool_variables_`、`switches_`、`self_switches_`
- [x] **动作命令（22 个枚举值）**：
  - 已实现：`ShowMessage`、`ShowToast`、`ShowChoices`、`TransferPlayer`、`MoveEntity`、`SetVariable`、`SetSwitch`、`SetSelfSwitch`、`Wait`、`If`、`Else`、`EndIf`、`PlaySound`、`PlayBGM`、`StopBGM`、`Loop`、`BreakLoop`、`ShowUI`、`HideUI`、`PlayAnimation`、`AddItem`、`RemoveItem`、`CallScript`
  - 占位预留：无（22/22 全部有基础实现）
- [x] **两种消息类型**：
  - **Toast**（`ShowToast`）：实体头顶浮动，2秒自动渐隐，不阻塞移动/事件流
  - **Dialog**（`ShowMessage`）：屏幕底部居中黑框，Enter 确认，阻塞移动+交互
- [x] `dialog_active_` 状态管理（`IsDialogActive`）
- [x] 编辑器集成：Event Editor 面板可视化编辑 EventTrigger

### Phase 2.7：Play Mode 运行时 ✅ 已完成（2026-05-28）
- [x] Play Mode 状态机（Edit / Playing / Paused）+ 状态切换快照/恢复
- [x] `MovementSystem`（WASD / 方向键，Y-up 世界坐标）
- [x] `CameraFollowSystem`（平滑跟随、死区）
- [x] AABB Tile 碰撞（两轴分离，14px 半宽，`edge_thickness` 边缘厚度，四角拆成两条边）
- [x] 碰撞层不渲染（invisible property）
- [x] 交互键：`Enter` 触发最近 48px 内 EventTrigger（`OnInteract`）
- [x] 键位统一：Enter = 交互触发 + Dialog 确认；WASD/方向键 = 移动
- [x] Play Mode 面板管理（只保留 Viewport，隐藏其余面板）
- [x] Play Mode 禁用 ImGui 键盘导航（避免方向键干扰移动）
- [x] Viewport 保持宽高比居中（letterbox / pillarbox）

### Phase 2.8：Tilemap 编辑器 ✅ 已完成（2026-05-28）
- [x] `Tileset` 资源定义（名称、图像路径、瓦片尺寸、行列数、纹理、ImGui DS）
- [x] `TilePalettePanel`（tileset 选择下拉、tile 网格预览、选中高亮）
- [x] Toolbar Tile Mode 切换（GizmoMode::Tile）
- [x] Viewport 鼠标 paint（左键）/ erase（右键 / Shift+左键）
- [x] 鼠标悬停高亮框（世界坐标 → tile 坐标转换）
- [x] Tilemap Merged VB 单 draw call 渲染（每帧收集所有可见图层顶点 → upload → draw）
- [x] `tilemap_dirty_` 语义修正（仅守卫 VB upload，不跳过 draw；缓存 `tilemap_vertex_count_`）
- [x] 精灵渲染过滤（`RenderSceneEntities` 仅渲染有 `Sprite` component 的实体，排除 Tilemap）
- [x] `CreateTextureDescSetFromTexture` 纹理描述符缓存（避免每帧分配泄漏）
- [x] 视口诊断几何：极深色背景 `#121212`、20px 半透明网格、红绿坐标轴、中心白色十字准线

### Phase 3：场景与资源系统 ✅ 已完成
- [x] 渲染集成（Sprite → Vulkan 纹理渲染）
- [x] `ResourceManager` + 统一纹理加载（单例管理纹理+Sampler DS 缓存）
- [x] `Camera` ECS Component + 正交投影（CameraSystem 计算 VP 矩阵）
- [x] ~~Box2D v3 引入~~ — **已拒绝**（非 header-only，overkill；改用简单 AABB tile collision）
- [x] **技术债务 TD-1**：提取共享 Vulkan+ImGui 初始化（`launcher_application.cpp` 与 `editor_application.cpp` 大量重复代码）（2026-09-14 完成：5 个 `MG_VulkanImGui_*` 应用层助手入 `mg_imgui_backend`，见下）
- [x] **技术债务 TD-N**：Sprite 批处理/实例化（2026-08-14 完成：instance rendering + 整帧单次上传 + 分组偏移 draw，见 Phase Z4 与后续图集批次）
- [x] **技术债务 TD-P**：碰撞图块合并为更少形状（2026-08-14 完成：`src/scene/systems/collision_merge.h/.cpp` 纯逻辑 run 合并器，播放模式碰撞收集接入，面积等价性单测覆盖）

---

## Phase F：Demo → 可玩小游戏 ✅ 已完成（2026-06-10）

### F1：AudioManager（SDL2 音频，零新增依赖）
- [x] 新增 `src/audio/audio_manager.h/.cpp`：`PlaySE` / `PlayBGM` / `StopBGM`
- [x] BGM 循环通过 `SDL_QueueAudio` + 每帧补充缓冲实现
- [x] `EventSystem` 接入 `PlaySound`、`PlayBGM` 命令

### F2：Sprite Animation（Animator Component）
- [x] 新增 `Animator` ECS 组件（`frame_count` / `columns` / `fps` / `playing`）
- [x] 新增 `AnimatorSystem` 每帧更新当前帧，写入 `Sprite::anim_frame`
- [x] `SpriteRenderSystem` 读取 `anim_frame`，通过 push constant 传递 UV offset + scale
- [x] `sprite.vert` 使用 `world_pos.zw` 作为 frame UV offset，`frame_scale` 作为单帧尺寸比例
- [x] Details Panel 支持 Animator 增删改查，SceneSerializer 支持序列化

### F3：MoveEntity（实体路径移动）
- [x] `EventContext` 扩展 `move_pending` / `move_target` / `move_path` / `move_speed` / `move_index`
- [x] `MoveEntity` 命令实现：按 `Name` 查找目标 → 解析路径字符串 → 逐帧移动 → 阻塞事件流
- [x] Event Editor 面板独立 MoveEntity 参数编辑（Target / Path / Speed）

---

## Phase G：Event System 收尾 + Editor 补齐 ✅ 已完成（2026-06-10）

### G1：EventEditor 补齐
- [x] SetSwitch UI（Name + Checkbox）
- [x] If Combo 条件类型（switch / choice / variable）

### G2：Loop / BreakLoop
- [x] Loop 无限/固定次数循环，页末自动跳回
- [x] BreakLoop 跳出到页末

### G3：ShowUI / HideUI
- [x] HUDOverlay 状态管理
- [x] Viewport Play Mode HUD 渲染（status 左上 / inventory 左下 / title 顶部居中）

---

## Phase H：资产管线 ✅ 已完成（2026-06-10）

### H1：Content Browser 导入
- [x] `Import` 按钮（顶部 + 右键菜单）
- [x] Windows 原生文件对话框（`GetOpenFileNameW`，支持多选）
- [x] 过滤修复（All/Textures/Meshes/Scripts 实际生效）
- [x] 搜索框大小写不敏感过滤

### H2：Sprite 纹理浏览
- [x] Details Panel Sprite Inspector 加 `📂` 浏览按钮
- [x] 选中后自动填充 `assets/filename.ext`

### H3：Tileset 导入对话框
- [x] Tile Palette `[+ New Tileset]` 按钮
- [x] 弹出窗口：Name / Image Path / Tile W/H / Columns / Rows
- [x] Create 失败时红色错误提示，不关闭 Popup

---

## Phase K：实体工作流 + 图层管理 ✅ 已完成（2026-06-10）

### K1a：Entity 放置模式
- [x] `GizmoMode` 新增 `Entity` 值
- [x] Toolbar 添加 "Place Entity" 按钮（与 Tile Paint 同组）
- [x] Viewport 底部 floating toolbar 同步加入 Entity 按钮
- [x] Entity 模式下左键点击世界坐标 → 创建 `Transform + Name + Sprite` 实体
- [x] 点选逻辑排除 Entity/Tile 模式，避免冲突

### K2：Tilemap 图层管理 UI
- [x] Tile Palette 底部新增 "Layers" 面板
- [x] 图层列表：当前层指示器（`>`）、可见性 Checkbox、名称编辑
- [x] 图层顺序：上移 / 下移按钮
- [x] 图层删除：垃圾桶按钮
- [x] 新增层：`[+ Add Layer]` 按钮，尺寸继承自现有层或场景默认 20×15
- [x] 可见性切换联动 `MarkTilemapDirty()` → 下帧 `RebuildTilemapVB()` 自动重建 VB

### K1b：Duplicate 实体
- [x] 已于 Phase Q1/V2 完成：`World::CloneEntity` 组件快照克隆 + `CreateEntityCommand` 包装入 Undo 栈，三入口统一

---

## Phase L：库存 HUD + A1 审阅 ✅ 已完成（2026-06-10）

### L1：Play Mode 库存 HUD
- [x] `EventSystem::GetInventoryItems()` — 遍历 `int_variables_`，筛选 `item_*` 前缀且 count>0
- [x] Viewport Play Mode 中 "inventory" HUD 显示实际物品列表（`item_name xN`）
- [x] 空库存时只显示标题，无物品行

### L2：A1 ImGui R6 微调（审阅发现已提前实现）
- [x] Tab 顶部 accent 指示器 — 代码中已存在（`imgui_widgets.cpp:10904`，`// [MakeGames]` 标记）
- [x] Dock Splitter hover 加宽 + accent 色 — 代码中已存在（`imgui_widgets.cpp:1871`）
- [x] Scrollbar grab hover 加宽 — 代码中已存在（`imgui_widgets.cpp:11132`）
- **结论**：A1 实际已全部完成，文档滞后。默认 splitter 厚度仍为原始 DockNode 间隙（~4px），hover 时加宽至 ~6px。

---

## Phase M：TransferPlayer 跨场景切换 ✅ 已完成（2026-06-10）

### 核心机制
- [x] `EventSystem::GlobalStateSnapshot` — 封装 int/float/bool/switch 全局状态
- [x] `TakeGlobalStateSnapshot()` / `RestoreGlobalState()` — 跨场景状态持久化
- [x] `pending_scene_transition_` 信号 — `action.target` 非空时设信号，`Update()` 冻结
- [x] `ConsumePendingSceneTransition()` / `GetPendingSpawnX/Y()` — EditorLayer 读取目标场景与出生坐标

### EditorLayer 切换流程
- [x] `RunPlayModeSystems` 末尾检测场景切换信号
- [x] 保存全局状态 → `ctx.LoadScene(target)` → 新场景查找/创建 Player 实体
- [x] 重建 `EventSystem` → 恢复全局状态 → `TriggerOnStart()`
- [x] `TakeSnapshot()` 新场景（Stop 时回退到当前场景）

### 字段映射
- `action.target` → 目标场景路径（空 = 同场景内坐标传送）
- `action.int_value` → 出生 tile X
- `action.float_value` → 出生 tile Y

---

## Phase N：物品数据库 + 场景属性编辑 ✅ 已完成（2026-06-10）

### N1：物品数据库
- [x] 新增 `src/resource/item_database.h/.cpp`：`ItemDef` {id, name, desc, price} + `FromJson`
- [x] `EditorContext::OpenProject()` 自动加载 `<project>/data/items.json`
- [x] 库存 HUD 集成 `ItemDatabase::Find(id)` — 显示 `"生命药水 x3"` 而非 `"potion x3"`
- [x] 未找到的物品 ID 回退到原始 ID 显示

### N2：场景属性编辑
- [x] `Scene` 新增 `bgm_path_` + `background_color_` 字段 + getter/setter
- [x] `SceneSerializer` 序列化/反序列化 `bgm` 和 `background_color`（RGBA 数组）
- [x] Editor MenuBar 新增 `Scene → Scene Properties` 菜单
- [x] 弹出对话框：Name / Width / Height / Tile Size / BGM Path / Background Color
- [x] 编辑后自动 `MarkSceneDirty()`

---

## Phase O：Tile Paint Undo/Redo + TransferPlayer 路径修复 ✅ 已完成（2026-06-10）

### O1：TransferPlayer 路径修复
- [x] `EditorLayer::RunPlayModeSystems` 中 `full_path = ctx.GetProjectPath() + "/" + target_scene`

### O2：Tile Paint Undo/Redo
- [x] `TilePaintCommand`（World* + EntityID + vector<Change>）
- [x] ViewportPanel 帧内收集 pending_tile_changes_，帧末 flush 推入 Undo 栈
- [x] `LoadScene` / `SetPlayState` 切换时清空 undo_stack_ / redo_stack_

---

## Phase P：物品数据库编辑器 ✅ 已完成（2026-06-10）

### P1：ItemDatabase 补全
- [x] `ItemDatabase::ToJson()`（按 id 排序输出，diff 稳定）
- [x] `EditorContext::GetItemDatabase()` 非 const 重载
- [x] `SaveProject()` 末尾追加 `data/items.json` 保存

### P2：ItemDatabasePanel
- [x] 新面板 `src/editor/panels/item_database_panel.cpp/.h`
- [x] ImGui Table 4 列：ID（只读）/ Name / Description / Price
- [x] `[+ Add Item]`、`[Save]`、右键 Delete
- [x] Actors/Skills/Enemies 标签页骨架（"Coming Soon"）
- [x] `InputTextString()` 回调 resize 支持 `std::string`

---

## Phase Q：Entity Duplicate + 场景淡入淡出 ✅ 已完成（2026-06-10）

### Q1：Entity Duplicate
- [x] `World::CloneEntity(EntityID src)` — 硬编码 10 组件逐项 `GetComponent → AddComponent = *c`
- [x] Outliner 两处右键 "Duplicate" 接入：空白处右键（复制选中实体）+ 实体右键（复制该实体）
- [x] 新实体 `position.x += 16.0f` 防重叠

### Q2：场景淡入淡出
- [x] `EditorContext::FadeState {None, Out, In}` + `fade_alpha_`
- [x] TransferPlayer 触发 → FadeOut（alpha 0→1）→ 执行切换 → FadeIn（1→0）
- [x] ViewportPanel 全屏黑色 overlay（`ImDrawList::AddRectFilled`）
- [x] Fade 期间 `return` 跳过 `RunPlayModeSystems`，冻结逻辑更新

---

## Phase R：存档系统 + Content Browser 文件操作 ✅ 已完成（2026-06-10）

### R1：存档系统
- [x] `EventSystem::SaveState()` / `LoadState()` — 序列化 5 个全局变量 map 到 JSON
- [x] `GlobalStateSnapshot` 补齐 `self_switches`
- [x] Play Mode Escape 键弹出暂停菜单（Resume / Save / Load）
- [x] 存档格式：`saves/save_001.json` {scene, player_x, player_y, globals}
- [x] 读档：`LoadScene` → 查找/创建 Player → 恢复位置 → 重建 EventSystem → `LoadState` → `TriggerOnStart`
- [x] `SetPlayState` Pause/Resume 不清空 undo 栈（仅 Edit↔Playing 切换时清）

### R2：Content Browser 文件操作
- [x] New Folder（自动数字后缀防重名）
- [x] Delete（基于 `GetSelectedAssetPath()`）
- [x] Rename（右键菜单内联 `InputText` + OK/Cancel）

---

## Phase S：Entity Editing Suite ✅ 已完成（2026-06-10）

### S1：Transform Undo/Redo
- [x] `SetTransformCommand`（World* + EntityID + old/new Vec2）
- [x] Gizmo Drag `IsMouseReleased` 处推入 Command

### S2：Delete Undo/Redo
- [x] `DeleteEntityCommand`（Execute=DestroyEntity，Undo=CreateEntity）
- [x] 三处删除入口统一：Viewport Delete 键 / Viewport 右键 / Outliner 右键
- [x] ~~LIMITATION：Undo 后仅恢复空壳实体，组件不恢复~~（Phase V 已修复）

### S3：Create Undo/Redo
- [x] `CreateEntityCommand`（Execute 为空—实体已创建，Undo=DestroyEntity）
- [x] 三处创建入口统一：Viewport 右键 / Entity Place / Outliner 右键
- [x] ~~LIMITATION：Redo 不恢复组件~~（Phase V 已修复）

### S4：Grid Snapping + Toggle
- [x] `EditorContext::snap_to_grid_` + Toolbar `[Grid]` toggle
- [x] Entity Place 模式：`(floor(pos/tile) + 0.5) * tile` 吸附到网格中心

### S5：Content Browser 文件夹导航
- [x] 扫描逻辑支持 `current_path_`（`scan_dir = project_path + "/" + current_path_`）
- [x] 目录以文件夹图标显示，双击进入
- [x] `< Back` 按钮返回上级
- [x] New Folder / Delete / Rename 均在当前目录生效

---

## Phase T：Actor/Skill/Enemy/Shop 数据库与商店系统 ✅ 已完成（2026-06-10）

### T1/T2/T3：Actor / Skill / Enemy 数据库
- [x] `ActorDatabase`、`SkillDatabase`、`EnemyDatabase` struct + `FromJson` / `ToJson` / `Find`
- [x] `EditorContext` 加载/保存 `data/actors.json`、`skills.json`、`enemies.json`
- [x] `DatabasePanel` 扩展四标签页：Items / Actors / Skills / Enemies
- [x] 表格支持 Add / Save / Delete，ID 只读

### T4：商店系统
- [x] `ActionCmd::ShopProcessing` 命令（`target` 存 `shop_id`）
- [x] `EventSystem` 商店状态：`shop_pending_` / `pending_shop_id_`，阻塞事件流
- [x] `EditorContext` 通过 `SetShopDatabase` 向 `EventSystem` 注入 `ShopDatabase`
- [x] `ViewportPanel` Play Mode 商店 UI：商品列表 / 价格 / 金币 / Buy / Close
- [x] `BuyItem` 扣减 `gold` 变量并调用现有 `item_*` 库存机制
- [x] `EventEditorPanel` 支持选择 `Shop Processing` 并编辑 `Shop ID`
- [x] `SceneSerializer` 序列化/反序列化 `ShopProcessing`
- [x] 商店打开期间阻塞玩家移动（WASD）和交互键（Enter）

---

## Phase U：标题画面 / 多存档 / Dialog 角色名 / Pause 子菜单 / Shop 编辑器 ✅ 已完成（2026-06-10）

### U1：多存档槽位
- [x] `EditorContext::SaveSlotInfo` + `ReadSaveSlotInfo(slot)`
- [x] 存档路径参数化：`saves/save_%03d.json`
- [x] `RequestSaveToSlot(slot)` / `RequestLoadFromSlot(slot)`
- [x] Pause 菜单槽位选择 UI（3 槽位，显示场景名 / Empty）

### U2：标题画面
- [x] `EditorContext::ScreenMode {Title, Game}`
- [x] `GetStartScenePath()`：从 `project_schema.start_scene` 读取，回退到当前场景 / 第一个地图
- [x] `EnsurePlayer()`：无玩家时自动创建带 Movement / Sprite / CameraFollowTarget 的玩家实体
- [x] `ViewportPanel` 标题画面：Start New Game / Continue / Exit to Editor
- [x] Title 状态下跳过 `RunPlayModeSystems`

### U3：Dialog 角色名
- [x] `ViewportPanel` Dialog 用 `action.target` 作为 Speaker ID 查询 `ActorDatabase`
- [x] 找不到时回退显示原始 `action.target`
- [x] 黄色角色名 + 分隔线

### U4：Pause 子菜单
- [x] `PauseSub {None, SaveSlots, LoadSlots, Items, Status}`
- [x] 主菜单：Resume / Save / Load / Items / Status / Exit
- [x] `Items` 子菜单：复用 `GetInventoryItems()`，显示物品中文名
- [x] `Status` 子菜单：占位（HP/MP/Gold）

### U5：Shop 编辑器
- [x] `DatabasePanel` 增加 `Shops` 标签页
- [x] 左栏 Shop ID 列表 + Add Shop / Delete Shop
- [x] 右栏商品编辑：Item ID 下拉、Price、Add / Delete

---

## Phase V：组件快照 + S2/S3 修复 + Prefab 系统 ✅ 已完成（2026-06-10）

### V1：Component Snapshot
- [x] 10 组件添加 `to_json` / `from_json`：`Transform`、`Sprite`、`Name`、`Movement`、`Tag`、`CameraFollowTarget`、`Camera`、`Animator`、`EventTrigger`、`Tilemap`
- [x] `Vec2` / `Color` 添加 `to_json` / `from_json`
- [x] `IComponentStorage` 增加 `GetComponentJson` / `SetComponentFromJson`
- [x] `ComponentStorage<T>` 实现快照接口
- [x] `ComponentRegistry` 增加 `GetIndexByName`
- [x] `World::SnapshotEntity(src)` / `World::RestoreEntity(snapshot)`

### V2：修复 S2/S3 LIMITATION
- [x] `DeleteEntityCommand`：Execute 前 `SnapshotEntity`，Undo 时 `RestoreEntity`
- [x] `CreateEntityCommand`：Undo 前 `SnapshotEntity`，Redo 时 `RestoreEntity`
- [x] 命令内 `entity_` 动态追踪新 ID，避免 ID 竞争
- [x] Undo/Redo 后自动选中恢复/重建的实体（Outliner 同步高亮）
- [x] `Duplicate` 操作推入 Undo 栈（`CreateEntityCommand` 包装）

### V3：Prefab 系统
- [x] Content Browser 识别 `.prefab` 文件，图标 `ICON_FA_COPY`
- [x] Content Browser 右键 `Create Prefab from Selected Entity`（仅选中实体时启用）
- [x] 双击 `.prefab` 实例化，位置设为当前 `camera_pos`
- [x] Prefab 文件内部格式 = `SnapshotEntity` JSON

---

## Phase W：回合制战斗 MVP ✅ 已完成（2026-06-10）

### W1：BattleProcessing 命令
- [x] `ActionCmd::BattleProcessing` 枚举
- [x] `EventSystem` 战斗状态机：`BattleState {None, Intro, PlayerTurn, EnemyTurn, Victory, Defeat}`
- [x] 事件流阻塞：战斗中 `ExecuteContext` 返回，自动推进条件暂停
- [x] `SceneSerializer` 序列化/反序列化 `BattleProcessing`
- [x] `EventEditorPanel` 支持编辑 `Enemy ID` / `Count` / `Player Actor ID`

### W2：战斗 UI
- [x] `ViewportPanel` 战斗界面：敌人/玩家 HP 条、MP 条、战斗日志
- [x] `Attack` 按钮（仅 `PlayerTurn` 可用），`Skill`/`Item` 灰置
- [x] `Victory` / `Defeat` 大字显示
- [x] 战斗期间 `RunPlayModeSystems` 和 `HandleInteractKey` 阻塞

### W3：Enemy / Actor 数据接入
- [x] `action.target` = enemy_id，`action.int_value` = 数量（MVP 忽略，1v1）
- [x] `action.param` = player_actor_id，空时回退 `ActorDatabase` 中 id="Player"
- [x] `EventSystem` 注入 `ActorDatabase` / `EnemyDatabase`
- [x] 伤害公式：`damage = max(1, atk - def)`
- [x] 战斗结束写入 `int_variables_["battle_result"]`（1=胜利 / 2=失败），供后续 `If` 分支使用

---

## Phase X：回合制战斗系统扩展 ✅ 已完成（2026-06-10）

### X1：技能系统
- [x] `EventSystem` 注入 `SkillDatabase`
- [x] `PlayerUseSkill(skill_id)`：消耗 MP，伤害 `atk + skill.damage - def`
- [x] `ViewportPanel` 点击 `Skill` 弹出可选技能列表，按当前 MP 灰置不足项

### X2：物品系统
- [x] `PlayerUseItem()`：消耗任一 `item_*` 前缀库存物品，恢复 30 HP（不超过上限）
- [x] `ViewportPanel` `Item` 按钮可用

### X3：多敌人与目标选择
- [x] `battle_enemy_` 单实例重构为 `std::vector<BattleCombatant> enemies_`
- [x] `StartBattle` 根据 `Count` 参数生成多个同名敌人
- [x] `ViewportPanel` 顶部敌人列表，点击名称切换 `battle_target_`
- [x] 攻击/技能自动跳过已击败目标；全部击败后进入 `Victory`

### X4：逃跑
- [x] `PlayerTryEscape()`：50% 成功率，成功时 `battle_result = 3` 并结束战斗
- [x] `ViewportPanel` 新增 `Escape` 按钮

### X5：胜利奖励
- [x] 胜利时遍历敌人，从 `EnemyDatabase` 累加 `gold` / `exp`
- [x] 写入 `int_variables_["gold"]` / `int_variables_["exp"]`（累加，不覆盖）

---

## Phase Y：Lua 脚本系统 ✅ 已完成（2026-06-10）

### Y1：Lua 集成
- [x] `third_party/lua/` 本地源码编译为 `lua_static`
- [x] `ScriptEngine` 单例：`luaL_newstate` / `luaL_openlibs` / `lua_close`
- [x] CMake 优先本地 Lua，保留 FetchContent fallback

### Y2：`CallScript` 接通
- [x] `EventSystem` 注入 `ScriptEngine`
- [x] `CallScript` 执行前 `SetContext(world_, this)` 再 `DoFile(action.text)`
- [x] `luaL_dofile` / `luaL_dostring` 错误捕获，不崩溃

### Y3：脚本 API 绑定
- [x] `get_int` / `set_int` / `get_switch` / `set_switch`
- [x] `find_entity` / `get_entity_pos` / `set_entity_pos`

### Y4：脚本编辑器面板
- [x] `ScriptEditorPanel`：多行编辑 + IME + Run/Save
- [x] `OpenFile` 自动 `SetOpen(true)` 并聚焦
- [x] Content Browser 双击 `.lua` 打开脚本编辑器

### 补充修复
- [x] `ScriptEngine` 析构函数自动 `lua_close` 防泄漏
- [x] 脚本编辑器 `Run` 前自动 `SetContext` 到当前场景

---

## Phase Z：收尾打磨 ✅ 已完成（2026-06-10）

### Z1：自动图块
- [x] `TilesetAsset` 新增 `auto_tile` 标记
- [x] `TilePalettePanel` 增加 `Auto-tile [A]` 切换
- [x] `ViewportPanel` paint/erase 时按 4-bit 邻接查表更新中心及四邻居 tile
- [x] 支持 Undo/Redo 记录 autotile 引起的全部 tile 变化

### Z2：地图尺寸调整
- [x] `Scene Properties` 改 Width/Height 后同步裁剪/扩展所有 `Tilemap` layers
- [x] 复用 `Tilemap::ResizeLayers` 保留原有内容

### Z3：Camera is_primary 冲突
- [x] `World::RestoreEntity` 恢复 `Camera` 时，若 `is_primary` 为 true，遍历并关闭其他主相机

### Z4：Sprite 批处理
- [x] `sprite.vert/frag` 改为 instance rendering，去掉 push constant
- [x] `SpriteRenderSystem` 按 texture 分组，同组 sprite 合并为 instance data 单次 `vkCmdDrawInstanced`
- [x] `EditorApplication` 增加 host-visible `sprite_instance_vb_`（4096 instances）

---

## Demo：魔塔村物语 ✅ 已生成（2026-06-10）

- [x] `tools/generate_demo_project.py` 一键生成完整项目
- [x] 项目目录：`demo_project/`
- [x] 素材：纯色块 tileset + 5 个纯色块 sprite
- [x] 数据：items / actors / skills / enemies / shops / tilesets JSON
- [x] 地图：Map001（新手村）+ Map002（野外）+ MapInfos
- [x] 事件：村长对话、宝箱、商店、传送门、战斗触发、脚本调用
- [x] 修复 `SceneSerializer` 缺失 `Tilemap` / `Movement` 序列化
- [x] 修复相机未指定 `follow_target` 时不跟随 Player

### 使用方式
1. 启动 `launcher.exe`
2. 选择 / 打开项目路径：`D:\CLionProjects\MakeGames\demo_project`
3. 进入 editor 即可看到魔塔村场景
4. Play Mode 可测试行走、对话、战斗、跨场景传送、存档读档

---

## Phase Q：编辑器 UX 重构 + 创建体系 ✅ 已完成（2026-06-16）

基于 `docs/QualityOverhaul.md` 阶段二、三实施。

### Q1：Content Browser 删除
- [x] 删除 `src/editor/panels/content_browser_panel.h/.cpp`
- [x] `File → Import Assets` 接管导入

### Q2：弹窗体系
- [x] `Details` 改名为 `Scene Explorer`，双击 Outliner 实体弹出
- [x] `Event Editor` 右键实体 `Edit Events` 弹出
- [x] `Item Database` 改为 `File → Database` 弹出
- [x] `Tile Palette` 改为工具栏 `Tile` 按钮触发的浮窗，Play Mode 自动隐藏
- [x] `Script Editor` 保持弹窗，`File → Open Script...` 打开
- [x] 引入 `IPanel::IsPopup()`，`PanelManager` 对弹窗默认关闭、不参与 Dock、不参与 Reset Layout

### Q3：工具栏与菜单
- [x] 工具栏精简为 `Play/Pause/Stop | Tile/Entity/Create▼ | Undo/Redo`
- [x] Window 菜单只列出常驻面板（Viewport / Scene Outliner / Console / Reset Layout）
- [x] Console 保留为底部可隐藏常驻面板

### Q4：实体创建体系
- [x] 统一入口：Toolbar [Create▼]、Viewport 右键、Outliner 右键
- [x] 7 种类型：`Entity / NPC / Monster / Chest / Transfer / Shop / Decoration`
- [x] 创建位置 = 当前 Camera 世界中心
- [x] 创建后自动选中并标记场景脏

---

## Phase R：多地图管理 + 布局持久化 + 打磨 ✅ 已完成（2026-06-16）

基于 `docs/NextPhasePlan_v23.md` 实施。

### R1：多地图管理
- [x] Outliner 顶部场景下拉框，列出 `MapInfos.json` 所有场景
- [x] 切换场景时自动保存当前场景
- [x] 未保存场景名旁显示 `*`
- [x] `File → New Scene` / Outliner `+ New Scene` 创建新地图
- [x] `CreateAndSaveNewScene` 含失败回滚
- [x] 场景 Rename / Delete，同步 `MapInfos.json` 和 `project.json`

### R2：TransferPlayer 改进
- [x] `target` 字段存场景文件名
- [x] `EditorLayer` 执行时查 `MapInfos` 拼路径
- [x] `Event Editor` 中 `[...]` 按钮选择目标场景

### R3：布局持久化
- [x] `imgui.ini` 保存到项目目录
- [x] `editor_layout.json` 跳过弹窗面板

### R4：打磨
- [x] `Scene Explorer` / `Event Editor` 弹窗标题显示实体名
- [x] `File → Recent Projects` 最近项目列表

---

## 阶段五：导出系统 ✅ 已完成（2026-06-24）

基于 [`docs/QualityOverhaul.md`](QualityOverhaul.md) 阶段五实施。

### 5.1 Runtime 剥离
- [x] `engine_runtime` 静态库（零 ImGui 依赖）与 `engine_editor` 分离
- [x] 独立 `game.exe` 运行时入口（`src/game_main.cpp`，527 行）
- [x] MinGW 运行时静态链接 + SDL2.dll / 着色器自动拷贝

### 5.2 一键导出
- [x] `File → Export Game`（`src/editor/export_game.cpp`，275 行）
- [x] 导出流程：校验 project.json → 拷贝项目数据 → 收集依赖资源 → 附带 game.exe 与着色器
- [x] 导出结果成功/失败弹窗反馈

### 5.3 演示项目
- [x] `tools/generate_demo_project.py` 生成「魔塔村物语」demo（输出到 `demo_project/`，该目录为生成产物，不入库）

---

## 已暂停 / 远期规划（明确无排期，避免误读为进行中）

| 规划项 | 出处 | 状态 |
|--------|------|------|
| 横板模式（PlatformerMovement、重力、斜坡/梯子/单向板） | 2DFrameworkPlan §1.5 | **暂停**：当前实现全部为俯视角，横板零实现 |
| 3D 支持（Jolt 物理、Phase 5） | EnginePlan | **远期**：无排期 |
| Server 服务器框架 | EnginePlan | **远期**：仅架构图占位 |
| Box2D 集成 | 2DFrameworkPlan | **已否决**：俯视角 RPG 无需物理引擎 |

---

## 下一阶段方向（候选，未定优先级）

## Prefab 系统 MVP ✅ 已完成（2026-08-14）

- [x] `src/editor/prefab_manager.h/.cpp`：SavePrefab / ListPrefabs / Instantiate（实例即拷贝，无活链接）
- [x] 文件格式：`{project}/prefabs/<name>.prefab.json`，内容 `{ "name", "entity": <World::SnapshotEntity JSON> }`
- [x] Outliner：实体右键 `Save as Prefab`（命名弹窗）+ 底部 Prefabs 区块点击实例化到视口相机中心（走 Undo 命令）
- [x] 测试：`world_prefab_json_roundtrip`（快照→JSON→恢复等价性）

### Prefab 手动活链接 ✅（2026-08-14 追加）

- [x] `PrefabRef` 组件标记实例→prefab 关联（模板文件剥离自引用，Instantiate 自动附加；场景序列化/克隆自动携带）
- [x] `World::RestoreEntityInto`：原地还原快照（清空组件重建，实体 ID 不变），抽共用 `ApplySnapshotComponents`
- [x] PrefabManager：`ApplyToPrefab`（存回文件）/ `RevertToPrefab`（原地还原，保留位置）/ `BuildRevertSnapshot`（undo 用）
- [x] `UpdateEntityCommand`（before/after 快照）使 Revert 可 Undo
- [x] Outliner：实例名后缀 `[P]`，右键菜单 Apply to Prefab / Revert to Prefab / Unlink Prefab
- [x] 测试：`world_restore_entity_into`（ID 不变/值还原/多余组件移除/无效实体），52 tests / 311 checks 全绿
- [ ] 明确不做：自动传播、组件级 override 脏掩码、嵌套 prefab

---

## Sprite 批处理修正 + 渲染排序 ✅ 已完成（2026-08-14）

- [x] **正确性修复**：原每纹理组重复 `Upload()` 覆盖同一 host-visible instance buffer，多纹理同帧时所有组的 draw 读到最后一组数据 → 改为整帧单次上传 + `vkCmdDraw(..., firstInstance)` 偏移分组
- [x] **排序接入**：`Transform::sorting_order` 进入渲染路径，排序键 `(sorting_order, texture_path, entity_id)` 稳定排序
- [x] 抽纯逻辑 `src/scene/systems/sprite_batch_builder.h/.cpp`（Vulkan 无关）：收集→排序→分组→填充 instance，供单测
- [x] 测试：新增 `tests/test_sprite_batch.cpp` 4 个用例（分组偏移 / sorting_order 优先 / Animator UV / 过滤与空 World），22 tests / 154 checks 全绿
- [ ] 遗留：跨纹理合并单 draw（纹理图集，候选下一轮）；~~Sprite::pivot/flip_x/flip_y 未接入渲染~~（2026-09-07 已接入）

## 纹理图集（跨纹理合并单 draw）✅ 已完成（2026-08-14）

- [x] `src/resource/atlas_packer.h/.cpp`：纯逻辑 shelf 打包器，固定 2048²，2px 边缘克隆 padding 防 bleeding，放不下返回 false（fallback per-texture）
- [x] `ResourceManager`：`GetSpriteAtlasRect()`（懒打包）/ `GetSpriteAtlasDS()`（dirty 重建 + DS 重写，CLAMP_TO_EDGE）；`Texture::LoadPixelsFromFile`（CPU 像素加载）+ sampler address mode 参数化
- [x] `SpriteInstance` 12→16 floats（追加 vec4 atlas rect，identity = (0,0,1,1)）；sprite.vert loc7 + UV 组合公式（与 Animator 帧 UV 天然兼容）；spv 已重编
- [x] 新建 `sprite_vertex_layout.h` 共享 bindings/attrs，**消除 editor/game 两处逐字节重复**；wireframe 变体自动同步；tilemap dummy instance 补 identity atlas rect
- [x] 排序键 `(sorting_order, !is_atlas, texture, id)`：图集 sprite 合并单一 `@atlas` 组（单 DS 单 draw），未命中保持 per-texture fallback
- [x] 测试：新增 `test_atlas_packer.cpp` 4 用例 + builder 图集分组 2 用例，28 tests / 206 checks 全绿
- [ ] 遗留：动态增长/多图集、mipmap、bindless、tileset 进图集均不做

## Y-sort 渲染排序 ✅ 已完成（2026-08-14）

- [x] `Sprite` 组件加 `y_sort` bool（默认 true，序列化，Details 面板 `Y-Sort` 勾选）
- [x] 排序键升级为 `(sorting_order, y_key, !is_atlas, texture_path, entity_id)`：`y_key = y_sort ? position.y : -FLT_MAX`（关闭者沉到层底）；同层 Y 大者后画=靠前，实现俯视图遮挡
- [x] 图集组内 Y 排序不打散单 draw；跨纹理 fallback 场景组会随 Y 交错拆分（正确性优先）
- [x] 测试：builder 新增 `sprite_batch_y_sort` / `sprite_batch_sorting_order_beats_y`，30 tests / 214 checks 全绿

## 音频增强：软件混音器 + BGM 淡入淡出 + BGS/ME ✅ 已完成（2026-08-14）

- [x] **AudioManager 重构为软件混音器**（裸 SDL，不引 SDL_mixer）：新增 `src/audio/audio_mixer.h/.cpp` 纯逻辑混音核心（voice 列表 → int16 混音块），Update() 混音后一次性 QueueAudio
- [x] **修复既有缺陷**：SE/BGM 不再共享队列互相清掉；`volume` 参数生效；WAV 加载经 `SDL_AudioStream` 归一到 48k/S16/stereo
- [x] 通道语义：SE（多实例叠加）/ BGM（循环唯一，支持 fade in/out）/ BGS（循环唯一，与 BGM 并行）/ ME（一次性，期间 BGM 挂起、结束自动恢复）
- [x] 事件命令：新增 `StopBGM/PlayBGS/StopBGS/PlayME`；`PlayBGM`/`PlayBGS` 的 `float_value` = 淡入秒数；序列化 + 事件编辑器 UI（fade 输入框）+ 导出资源收集同步
- [x] 测试：`tests/test_audio_mixer.cpp` 7 用例（音量缩放/叠加截断/fade 包络/fade out 移除/loop 回绕/挂起静音/FadeStep 边界），37 tests / 234 checks 全绿

## Lua 绑定扩展：Audio / Input / Entity 补充 ✅ 已完成（2026-08-14）

- [x] `script_engine.cpp` 沿用手工 C API 模式新增 12 个绑定（零新依赖，不迁 Sol2）：
  - Audio：`play_se/play_bgm(v,fade)/stop_bgm(fade)/play_bgs/stop_bgs/play_me`（对接混音器）
  - Input：`is_key_down(name)`（SDL scancode 名，无效键名告警返回 false）、`get_mouse_pos()`
  - Entity：`move_entity/get_entity_scale/set_entity_scale/get_self_switch/set_self_switch`
- [x] 测试：`tests/test_lua_bindings.cpp` 6 用例 headless（变量/开关 roundtrip、实体 Transform 读写、self switch、Audio 注册空调用、语法错误后状态可用），51 tests / 302 checks 全绿
- [x] ~~遗留：OGG/MP3 解码（仅 WAV）~~（2026-09-08 已完成：stb_vorbis + dr_mp3，`DecodeAudioFile` 统一入口）

---

## 关键帧补间动画（TweenAnimation + Animation 面板）✅ 已完成（2026-08-14）

- [x] `src/scene/components/tween_animation.h`：`TweenAnimation` 组件（4 条内嵌轨道 position/rotation/scale/tint + duration/loop/playing；time/base_* 运行时不序列化）；组件快照体系自动获得场景序列化/Prefab/Undo 能力
- [x] `src/scene/systems/tween_animation_system.h/.cpp`：`SampleFloatTrack/SampleVec2Track/SampleColorTrack` 纯函数（keys 按 t 升序、两端钳制、线性插值）+ `Update(dt, world)` / `UpdateOne(dt, world, entity)`（首次捕获 base_position/base_scale；position 轨道为相对 base 偏移；loop 回绕 / 非 loop clamp 到末帧停播）
- [x] 系统接入：`game_main.cpp` 主循环 + `editor_layer.cpp` 播放模式 `Update`；非播放模式对**选中实体**调 `UpdateOne` 实现编辑器内预览
- [x] `src/editor/panels/animation_panel.h/.cpp`：`Animation` 面板（Add TweenAnimation 按钮、duration/loop/▶预览/time slider、4 轨道关键帧列表：+ Key 在当前时间捕获实体现值并保持有序插入、选中编辑 t/数值/删除，所有编辑 MarkSceneDirty）
- [x] 测试：`tests/test_tween_animation.cpp` 11 用例（空轨道 fallback / 单 key / 两端钳制 / 中点插值 / 系统推进 / base 只捕获一次 / loop 回绕 / 非 loop 停播 / rotation-scale-tint 写入 / JSON roundtrip），63 tests / 364 checks 全绿
- [ ] 遗留：独立 .anim 资源文件、多 clip、事件轨道（明确不做）；~~缓动曲线~~（2026-09-07 已完成）

## 地图设置（Map Settings）✅ 已完成（2026-08-14）

- [x] 新建 `src/scene/map_settings.h/.cpp` 纯逻辑：`ResizeAllTilemaps(world, w, h)`（复用 `TileLayer::Resize` 保留重叠区 tiles/passages/terrain）+ `SyncAllTilemapTileSize(world, size)`，均 clamp ≥1 并返回受影响数量
- [x] **Bug 修复**：Scene Properties 的 "Tile Size" 原先只写 `scene.tile_size_`，而渲染/碰撞/事件全用 `Tilemap.tile_width/tile_height` → 改了不生效；现同步所有 Tilemap 组件并触发 `MarkTilemapDirty`
- [x] `SceneSession::CreateDefaultScene(w, h, tile_size)` / `CreateAndSaveNewScene(name, w, h, tile_size)` 参数化（默认参数 20/15/32 保持既有调用零改动）；新场景同时写 scene 尺寸与 Tilemap tile 尺寸
- [x] UI：菜单栏 New Scene 与 Outliner NewScene 弹窗均加 Width/Height/Tile Size 输入；Scene Properties 加像素尺寸只读提示、宽高 clamp ≥1
- [x] 测试：`tests/test_map_settings.cpp` 5 用例（Resize 扩大保留/缩小裁剪/多实体批量/clamp+空指针/tile 尺寸同步），68 tests / 397 checks 全绿
- [x] 踩坑记录：测试中对同一组件类型连续 `AddComponent` 后，先前取的引用因 dense vector 重分配而**悬垂**（SIGSEGV）——引用必须在所有 AddComponent 完成后通过 `GetComponent` 取

## Actor 属性成长曲线 ✅ 已完成（2026-08-14）—— 2DFrameworkPlan Phase 6 全部收尾

- [x] `ActorDef` 扩展：`max_level`(99) + 4 个 `*_peak` 满级值 + `growth_exponent`(1.0)；内存不变量：peak 总是有效值（加载缺省回退 = base，即平坦曲线）
- [x] 纯函数 `EvalStatCurve(base, peak, exponent, level, max_level)`：`base + (peak-base) * pow(t, exponent)`，t=(level-1)/(max-1) clamp；exponent 1=线性、<1 早熟、>1 晚成；exponent 钳制 [0.05,10]
- [x] 序列化向后兼容：旧格式 actors.json 无新字段 → peak=base、max_level=99、exponent=1
- [x] Item Database 面板 Actors 标签页：行选中 + 「Growth Curves」区（Max Level、早熟/平均/晚成预设 + 自定义 exponent、4 参数 Lv1/Max 输入、`ImGui::PlotLines` 曲线预览可切换参数）
- [x] 测试：`tests/test_actor_curves.cpp` 6 用例（线性中点/早熟晚成序关系/level 钳制/max_level=1/递减曲线/exponent=0 钳制/roundtrip/旧格式兼容），74 tests / 432 checks 全绿
- [ ] 遗留：per-param 独立 exponent、逐等级数值表、职业（class）系统；~~运行时战斗集成~~（2026-09-07 已完成）

## 下一阶段候选（旧清单，均已完成或仍待定）

- ~~编辑器质量整改~~（2026-08-14 **已全部完成**：评估见 [`docs/Evaluation_2026-08.md`](Evaluation_2026-08.md)；6 阶段方案 A 文档 / B 死代码清理 / C 工程化 / D 功能补全 / E 测试网（17 tests 全绿） / F `EditorContext` 拆分为 8 个组件 + 聚合门面，cpp 由 1194 行减至 223 行）
- ~~`ViewMode`（Lit/Unlit/Wireframe）仅为标志位、无真实渲染分支~~（2026-08-14 已实现：Lit=纹理×tint、Unlit=纯纹理原色（sprite.frag push constant）、Wireframe=线框管线变体（fillModeNonSolid，设备不支持时按钮禁用）；视口工具栏入口已恢复；着色器编译脚本 `tools/compile_shaders.ps1`）
- ~~DetailsPanel / ConsolePanel 占位实现待接真实数据~~（2026-08-14 已接入真实 ECS 数据与引擎日志）
- ~~`EditorContext` 去单例化~~（2026-08-14 已完成：`Get()` 单例删除，77 处调用改为引用注入；Context 由 `editor/main.cpp` 栈持有，沿 EditorApplication → EditorLayer → EditorApp → 8 面板构造链下传；CommandHistory/SceneSession 组件经 `Bind()` 注入协作者；4 个无订阅者的死回调已清理）
- ~~`EditorContext` 面板从门面迁移到直接依赖子组件~~（2026-08-14 已完成：console 无依赖；details=Selection+SceneSession；outliner=+ProjectManager+CommandHistory+PanelManager；event_editor=Selection+SceneSession+ProjectManager；tile_palette=TilesetEditor+SceneSession+ProjectManager；script_editor=SceneSession+EventSystem getter；item_database=GameDatabases+Save 回调；仅 viewport 与编排层（menu_bar/tool_bar/editor_layer/editor_application）保留门面引用）

---

## 遗留功能收尾批次 ✅ 已完成（2026-09-07）

对 Progress.md 全部显式「遗留」项的清尾（明确标注「不做」的除外：OGG/MP3、图集动态增长/mipmap/bindless、Prefab 自动传播/嵌套、.anim 资源/多 clip/事件轨道、per-param exponent/职业系统、横板/3D/Server）。

### Sprite pivot / flip_x / flip_y 接入渲染 ✅

- [x] `SpriteInstance` 16 → 20 floats：追加 `pivot_x/pivot_y/flip_x/flip_y`；`sprite.vert` 新增 location 8 `iPivotFlip`：位置 `(aPos - (pivot - 0.5) * 32) * iScale + iWorldPos`（quad ±16px，pivot 0.5 时零偏移，向后兼容），UV `mix(aUV, 1-aUV, flip)`
- [x] `sprite_vertex_layout.h` stride 64→80 + location 8 属性；editor/game 两处 dummy instance 补默认值（pivot 0.5,0.5 / flip 0,0）
- [x] 序列化与 Details 面板 UI 此前已存在，本次仅接入渲染路径；`tools/compile_shaders.ps1` 重编译 spv
- [x] 测试：`test_sprite_batch.cpp` +2 用例（字段填充 / 默认值兼容）

### TweenAnimation 缓动曲线 ✅

- [x] `Easing` 枚举（Linear/InQuad/OutQuad/InOutQuad/InCubic/OutCubic/InOutCubic）+ 组件级 `easing` 字段（序列化 `"easing"` int，缺省 Linear 向后兼容）
- [x] `ApplyEasing()` 纯函数；`SampleTrackImpl` 对插值系数应用缓动（端点钳制不受影响）；`Sample*Track` 加默认参数保持旧签名
- [x] Animation 面板加 Easing Combo
- [x] 测试：`test_tween_animation.cpp` +4 用例（端点不变式 / 中点序关系 / 轨道采样 / JSON roundtrip + 旧格式缺省）

### 存档保存/恢复 BGM 状态 ✅

- [x] `AudioManager` 记录 `current_bgm_path_`（PlayBGM 记录 / StopBGM 清空）+ `GetCurrentBGMPath()`
- [x] Play Mode 存档 JSON 增加 `"bgm"` 字段；读档时字段存在则恢复播放（空串 → StopBGM），字段缺失不动（旧存档兼容）

### Actor 成长曲线接入战斗 ✅

- [x] 纯函数 `EvalActorBattleStats(def, level)` → `{hp, mp, atk, def}`（actor_database，可单测）
- [x] `EventSystem::StartBattle`：取变量 `"level"`（缺省 1）按成长曲线求战斗数值；peak=base（旧格式回退）时行为与旧版完全一致
- [x] 等级由游戏作者通过事件/Lua `set_int("level", n)` 控制（不做自动升级规则）
- [x] 测试：`test_actor_curves.cpp` +2 用例（按等级取值 / 平坦曲线兼容）

### 瓦片动画（TilemapDesign.md 第五章 MVP）✅

- [x] 数据：`tilesets.json` 每 tileset 增加 `"animations": [{tile_index, frames[], fps}]`；`TilesetAsset::animations`（`scene::TileAnimDef`）；TilesetEditor Load/Save 读写
- [x] 纯逻辑 `src/scene/systems/tile_animation.h/.cpp`：`ResolveAnimatedTile(anims, global_tile, start_index, time_sec)` 帧替换 + `AnimFrameHash` 帧变化检测
- [x] 运行时（editor + game）：动画时钟按 dt 推进，帧哈希变化才标脏重建 VB（MVP：CPU 替换，避免每帧重建）
- [x] 编辑器 UI：Tile Palette 面板「Tile Animations」区（选中 tile 添加动画 / +Sel Frame / -Frame / fps / Del）
- [x] 测试：`tests/test_tile_animation.cpp` 5 用例（直通 / 帧回绕 / 无效定义边界 / 多动画+偏移 / 帧哈希）

### R5-5c Outliner 缩进引导线 ✅

- [x] Outliner 场景树 Root → 子项画半透明竖线（`ImGuiCol_TextDisabled`，ImDrawList，Root 底部到最后子项行中）

**测试**：87 tests / 499 checks 全绿；`launcher / editor / game / engine_tests` 全量编译通过。

---

## 第二批：音频多格式 + SpriteSheet 导入 + 工程化 + 大文件拆分 ✅ 已完成（2026-09-08）

### OGG / MP3 解码 ✅

- [x] `third_party/stb_vorbis.h` + `dr_mp3.h` 单文件库 vendor 入库；`src/audio/audio_decoder.h/.cpp`（`DecodeAudioFile` 按扩展名分发，统一产出 48k/S16/stereo PCM）+ `audio_codec_impl.c`（实现 TU，声明侧需 `STB_VORBIS_HEADER_ONLY` 防 multiple definition）
- [x] `AudioManager::LoadSound` 多格式：`.wav` 走 SDL_LoadWAV+AudioStream 归一，`.ogg/.mp3` 走 DecodeAudioFile

### SpriteSheet 导入向导 ✅

- [x] 纯逻辑 `scene/systems/spritesheet_grid.h/.cpp`（行列网格切分、越界钳制，可单测）；editor 侧 `spritesheet_import.h/.cpp`（拷图入 assets/ + 生成 Sprite+Animator prefab）
- [x] 菜单入口 `File → Import Sprite Sheet`；测试 `test_spritesheet_grid.cpp`

### 工程化 ✅

- [x] `build.ps1` 工具链回退链：CLion 捆绑 → 仓库内置 `mingw_link`（GCC 13.1）→ 系统 PATH ninja
- [x] `.github/workflows/ci.yml`（windows-latest + Vulkan SDK 安装 + 仓库 mingw_link + compile_shaders + ctest），已用本地 `cmake-build-ci` 从零验证全流程后删除

### 大文件拆分 ✅

- [x] `event_system.cpp` 938 → 707 行：商店 → `event_system_shop.cpp`；战斗（StartBattle/EndBattle/Player* + UpdateBattle）→ `event_system_battle.cpp`
- [x] `viewport_panel.cpp` 1375 → 约 700 行：autotile 映射 → `panels/tile_autotile.h/.cpp`（`kAutoTileBlockSize` / `ComputeAutoTileIndex` / `RefreshAutoTiles`）；商店 + 战斗覆盖层 → `panels/viewport_panel_playmode.cpp`（`DrawShopOverlay` / `DrawBattleOverlay`，头文件补声明）
- [x] **事故记录**：拆分过程中 PowerShell 数组拼接写回导致 `viewport_panel.cpp` 被截断，中段（点选/右键菜单/网格/Gizmo/实体放置/Tile 绘制/浮动工具栏/标题/暂停菜单/Toast/对话）按 Progress.md 行为规格 + 头文件契约重建；教训——重写大文件禁用 PS `AddRange` 拼接，先备份再改
- [x] 重建连带修正：Gizmo 拖拽/相机平移改用 `ImGui::GetIO().MouseDelta`（该 ImGui 版本无 `GetMouseDelta`）；`TilePaintCommand` 持有 `TilesetEditor*` 使 Undo/Redo 后触发 VB 重建

**测试**：全量编译（launcher / editor / game / engine_tests）+ ctest 全绿。

---

## TD-1：Launcher/Editor 共享 Vulkan+ImGui 初始化 ✅ 已完成（2026-09-14）

- [x] `mg_imgui_backend.h/.cpp`（engine_editor，launcher/editor 均已链接，零 CMake 改动）新增 5 个应用层助手：`MG_VulkanImGui_Init`（renderer→swapchain→ImGui 一条龙，`enable_imgui_viewports` 区分 editor）/ `MG_VulkanImGui_BeginFrame`（NeedsRebuild→Resize + NewFrame，返回 resized 供 editor 重建 Viewport RT）/ `MG_VulkanImGui_RenderFrame`（swapchain pass 提交 ImGui 帧，clear 预乘 alpha）/ `MG_VulkanImGui_ProcessEvent`（SDL 事件转发 + resize→RequestRebuild）/ `MG_VulkanImGui_Shutdown`（ImGui→swapchain→renderer 拆解尾部）
- [x] `launcher_application.cpp` 129 → 75 行；`editor_application.cpp` 去掉 config 填充 / HandleResize / render pass 样板；两边 `HandleResize` 删除；editor 自有逻辑（Viewport 离屏 pass、Ctrl+S、窗口标题、imgui.ini、PlatformWindows）原样保留
- [x] **教训**：并行多个 StrReplaceFile 编辑同一文件存在 read-modify-write 竞态会互相覆盖——同文件多处修改必须合并进单次调用的 edit 数组
- [x] 测试：全量编译 + ctest 全绿

---

## 编译警告清零 ✅（2026-09-14，全量验证待统一执行）

- [x] `audio_manager.cpp` 真 bug：`SDL_NewAudioStream` 参数顺序错误（SDL 签名为 `(src_format, Uint8 src_channels, int src_rate, dst_format, Uint8 dst_channels, int dst_rate)`），原调用把 freq 塞进 channels 位、48000 截断为 Uint8=128（-Woverflow 根源）——已按正确顺序修复
- [x] `tile_palette_panel.cpp`：GCC 13.1 vector 拷贝内联 `-Warray-bounds` 误报，`push_back` 改 `emplace_back(std::move)` 消除

---

## 事件触发器补齐：OnExit / OnUpdate / OnInput / OnCollision ✅（2026-09-14，全量验证待统一执行）

枚举、序列化映射、编辑器下拉框此前已就位，本批次补齐派发逻辑（`event_system.h/.cpp`，editor_layer 零改动）：

- [x] `EventSystem::Update` 在执行上下文前新增三个每帧轮询：`PollOnUpdateTriggers`（OnUpdate 每帧评估条件）/ `PollOnInputTriggers`（OnInput 复用 `condition.name` 存 SDL 按键名，`SDL_GetScancodeFromName` + 上帧状态做按下沿检测，持续按住不重复触发）/ `PollAreaTriggers`（玩家 AABB±14px vs 事件点 ±16px 重叠差分：新进入 → OnEnter+OnCollision，持续 → OnTouch，离开 → OnExit；同时补上此前空挂的 OnEnter/OnTouch 接口）
- [x] 新公开接口 `TriggerOnExit(entity)` / `TriggerOnCollision(self, other)`
- [x] 行为修正（顺带）：`run_once` 的 `has_run` 标记从仅 OnStart 推广到所有触发类型（此前 OnInteract 等设 run_once 无效）；`EvaluatePagesForTrigger` 增加同实体同页活跃上下文去重，防止 OnUpdate/OnTouch 每帧堆叠上下文
- [x] 事件编辑器 UI：Trigger=On Input 时显示按键名输入框（写 `condition.name`）+ 提示；区域类触发显示「±16px 区域」说明
- [x] 测试：`tests/test_event_triggers.cpp` 6 用例（OnUpdate 每帧/条件门控/run_once 全类型生效/区域进入-持续-离开全流程/无玩家不崩溃/无效按键名忽略）
- [x] 局限记录：区域触发仅检测玩家（第一个 Movement 实体）vs 事件，OnTouch 的「任意 Entity」语义未做；OnCollision 的 Tag 过滤无 Tag 组件支撑，未做

---

> **最后更新**：2026-09-14（事件触发器补齐 OnExit/OnUpdate/OnInput/OnCollision + 编译警告清零；单 TU 验证干净，全量 build+ctest 待统一执行）
>
> ~~最后更新：2026-09-14~~（TD-1：共享 Vulkan+ImGui 初始化提取到 engine_editor；编译+ctest 全绿）
>
> ~~最后更新：2026-09-08~~（第二批：OGG/MP3 解码 + SpriteSheet 导入向导 + CI/build.ps1 工具链回退 + event_system 与 viewport_panel 拆分；编译+ctest 全绿）
>
> ~~最后更新：2026-09-07~~（遗留功能收尾：Sprite pivot/flip 渲染接入 + 缓动曲线 + BGM 存档 + 成长曲线战斗集成 + 瓦片动画 + Outliner 引导线；87 tests / 499 checks 全绿）
>
> ~~最后更新：2026-08-14~~（编辑器质量整改完成；去单例化 + 组件级注入 + ViewMode + Prefab MVP/手动活链接 + Sprite 批处理/图集/Y-sort + 音频混音器/BGS/ME + 碰撞合并 TD-P + Lua 绑定扩展 + 关键帧补间动画 + 地图设置 + Actor 成长曲线完成）
