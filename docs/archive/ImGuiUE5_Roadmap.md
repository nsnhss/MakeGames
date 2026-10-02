# MakeGames UI 改造路线图：基于 ImGui 源码修改实现 UE5 级编辑器界面

> **决策背景**：自研 UI 框架（`mg::uifw`）经过多轮迭代仍无法达到生产级品质，包括但不限于：Vulkan 渲染管线复杂度高、Flexbox 布局引擎 edge case 过多、控件集不完整、文本输入/IME 支持薄弱、动画系统与布局耦合导致性能问题。与其在不完备的地基上继续修补，不如直接修改 ImGui 源码——它已有成熟的渲染、布局、事件、输入系统，我们只需在其上"换皮"和"加料"，以工程量的性价比而言这是唯一正确的选择。

---

## 一、清理清单（Phase 0）

### 1.1 彻底删除的目录

| 目录 | 说明 | 理由 |
|------|------|------|
| `src/ui_framework/` | 自研 UI 框架全部源码 | 废弃，不再维护 |
| `src/ui/` | 旧版主题文件（theme.h/cpp, icons.h） | 被 ui_framework 和 imgui 主题替代 |
| `imgui_backup/` | 之前替换 docking 分支时留下的备份 | 当前 `imgui/` 已是 docking 分支，备份无意义 |
| `test_ninja/` | 测试用的 ninja 构建文件 | 无用 |

### 1.2 彻底删除的文件

| 文件 | 理由 |
|------|------|
| `launcher/test_ui.cpp` | 自研 UI 框架的测试入口 |
| `launcher/ui/launcher_window.cpp.obj` | 编译产物 |
| `docs/UIFW_Design_v1.md` | 自研框架设计文档，完全废弃 |
| `test_compile.cpp` | 测试文件 |
| `capture_screen.py` / `capture_window.py` / `check_braces.py` | 临时脚本 |
| `build_err.txt` / `build_err_full.txt` | 旧编译错误日志 |
| `compile_err.txt` / `compile_err2.txt` / `compile_out.txt` | 旧编译日志 |
| `err.txt` | 空错误文件 |
| `gpp_log.txt` / `gpp_log2.txt` / `gpp_log3.txt` | 旧编译日志 |
| `ninja_err.txt` / `ninja_out.txt` / `ninja_out2.txt` | 旧 ninja 日志 |
| `font_atlas_debug.bmp` | 调试输出 |
| `imgui.ini` | 旧 ImGui 布局配置（后续会重新生成） |
| `vk_debug.log` | Vulkan 调试日志 |

### 1.3 需要修改的构建配置

- `CMakeLists.txt`：移除对 `src/ui/` 和 `src/ui_framework/` 的引用
- `src/CMakeLists.txt`：清理 ui_framework 子目录添加
- `launcher/CMakeLists.txt`：移除 `test_ui.cpp`

---

## 二、ImGui 源码修改策略（核心思路）

我们不重写 ImGui，我们**定制 ImGui**。修改范围集中在以下维度，避免触碰 ImGui 核心状态机和渲染管线。

### 2.1 修改维度一览

```
ImGui 源码修改范围（按侵入性从低到高排序）

Level 1: 纯扩展（不修改 imgui 现有文件，只新增）
  ├── mg_imgui_theme.h/cpp       # 主题系统：UE5 配色、字体加载、图标字体合并
  ├── mg_imgui_extensions.h/cpp  # 扩展控件：带图标的按钮、属性网格、资源卡片等
  ├── mg_imgui_dock_layout.h/cpp # DockSpace 默认布局管理（UE5 式面板分布）
  └── mg_imgui_utils.h           # 辅助函数：颜色转换、尺寸计算、绘制装饰

Level 2: 局部修改（修改 imgui 文件中的特定函数，不改数据结构）
  ├── imgui.cpp                  # 修改 ImGui::ShowDemoWindow 为 ShowMGDemoWindow（可选）
  ├── imgui_widgets.cpp          # 修改 Button/Slider/InputText 的默认圆角、内边距
  ├── imgui_draw.cpp             # 修改 RenderCheckMark / RenderArrow 为更精致的矢量图形
  └── imgui_tables.cpp           # 修改 Table 默认边框样式、header 高度

Level 3: 结构性修改（新增数据结构或修改现有结构）
  ├── imgui.h / imgui_internal.h # 新增 ImGuiStyle 字段（如 TabRoundingBottom、SeparatorTextAlign）
  ├── imgui.cpp                  # 新增 ImGuiConfigFlags 标志位（如 MgConfigFlags_EnableUEMode）
  └── imgui_widgets.cpp          # 修改 TabItem、CollapsingHeader、TreeNode 绘制逻辑
```

### 2.2 不改什么（红线）

以下 ImGui 核心模块**绝不修改**，只通过 API 调用或扩展包装：

- **状态机**：`g.CurrentWindow`, `g.NavId`, `g.FocusScopeStack` 等内部状态
- **输入系统**：`ImGuiIO`, `Key/Char/Mouse` 映射和队列
- **绘制队列**：`ImDrawList`, `ImDrawCmd`, `PrimReserve` 等底层图元系统
- **布局系统**：`ItemSize`, `ItemAdd`, `SameLine`, `BeginGroup` 等核心布局
- **Docking 核心**：`DockContext`, `DockNode`, `DockBuilder` 的分裂/合并算法
- **Viewports 核心**：多窗口管理、OS 窗口创建销毁

> **原则**：只改"长什么样"，不改"怎么工作"。

---

## 三、UE5 视觉规范（要落地的具体数值）

### 3.1 色彩体系

```cpp
// mg_imgui_theme.cpp - UE5 暗色主题预设
static const ImVec4 UE5_WindowBg         = ImVec4(0.059f, 0.059f, 0.059f, 1.00f);  // #0F0F0F
static const ImVec4 UE5_ChildBg          = ImVec4(0.082f, 0.082f, 0.082f, 1.00f);  // #151515
static const ImVec4 UE5_PanelBg          = ImVec4(0.102f, 0.102f, 0.102f, 1.00f);  // #1A1A1A
static const ImVec4 UE5_Header           = ImVec4(0.129f, 0.129f, 0.129f, 1.00f);  // #212121
static const ImVec4 UE5_HeaderActive     = ImVec4(0.169f, 0.169f, 0.169f, 1.00f);  // #2B2B2B
static const ImVec4 UE5_HeaderHovered    = ImVec4(0.149f, 0.149f, 0.149f, 1.00f);  // #262626
static const ImVec4 UE5_Border           = ImVec4(0.18f,  0.18f,  0.18f,  1.00f);  // #2E2E2E
static const ImVec4 UE5_BorderShadow     = ImVec4(0.00f,  0.00f,  0.00f,  0.00f);  // 无阴影
static const ImVec4 UE5_Text             = ImVec4(0.88f,  0.88f,  0.88f,  1.00f);  // #E0E0E0
static const ImVec4 UE5_TextDisabled     = ImVec4(0.38f,  0.38f,  0.38f,  1.00f);  // #606060
static const ImVec4 UE5_TextDim          = ImVec4(0.63f,  0.63f,  0.63f,  1.00f);  // #A0A0A0
static const ImVec4 UE5_Accent           = ImVec4(0.169f, 0.396f, 0.890f, 1.00f);  // #2B65E3 UE蓝
static const ImVec4 UE5_AccentHover      = ImVec4(0.220f, 0.470f, 0.950f, 1.00f);  // #3878F2
static const ImVec4 UE5_AccentActive     = ImVec4(0.125f, 0.345f, 0.820f, 1.00f);  // #2058D1
static const ImVec4 UE5_Button           = ImVec4(0.129f, 0.129f, 0.129f, 1.00f);  // #212121
static const ImVec4 UE5_ButtonHovered    = ImVec4(0.169f, 0.169f, 0.169f, 1.00f);  // #2B2B2B
static const ImVec4 UE5_ButtonActive     = ImVec4(0.200f, 0.200f, 0.200f, 1.00f);  // #333333
static const ImVec4 UE5_FrameBg          = ImVec4(0.102f, 0.102f, 0.102f, 1.00f);  // #1A1A1A
static const ImVec4 UE5_FrameBgHovered   = ImVec4(0.129f, 0.129f, 0.129f, 1.00f);  // #212121
static const ImVec4 UE5_FrameBgActive    = ImVec4(0.149f, 0.149f, 0.149f, 1.00f);  // #262626
static const ImVec4 UE5_TitleBg          = ImVec4(0.059f, 0.059f, 0.059f, 1.00f);  // #0F0F0F
static const ImVec4 UE5_TitleBgActive    = ImVec4(0.082f, 0.082f, 0.082f, 1.00f);  // #151515
static const ImVec4 UE5_CheckMark        = ImVec4(0.169f, 0.396f, 0.890f, 1.00f);  // Accent
static const ImVec4 UE5_SliderGrab       = ImVec4(0.38f,  0.38f,  0.38f,  1.00f);  // #606060
static const ImVec4 UE5_SliderGrabActive = ImVec4(0.50f,  0.50f,  0.50f,  1.00f);  // #808080
static const ImVec4 UE5_ScrollbarBg      = ImVec4(0.059f, 0.059f, 0.059f, 1.00f);  // #0F0F0F
static const ImVec4 UE5_ScrollbarGrab    = ImVec4(0.22f,  0.22f,  0.22f,  1.00f);  // #383838
static const ImVec4 UE5_ScrollbarGrabHovered = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);  // #4D4D4D
static const ImVec4 UE5_ScrollbarGrabActive  = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);  // #606060
static const ImVec4 UE5_Success          = ImVec4(0.20f,  0.80f,  0.20f,  1.00f);  // 绿
static const ImVec4 UE5_Warning          = ImVec4(0.95f,  0.65f,  0.10f,  1.00f);  // 橙
static const ImVec4 UE5_Error            = ImVec4(0.90f,  0.20f,  0.20f,  1.00f);  // 红
```

### 3.2 尺寸与圆角

| 元素 | 数值 | 说明 |
|------|------|------|
| `WindowRounding` | 0.0f | UE5 窗口无圆角，硬朗风格 |
| `ChildRounding` | 2.0f | 子面板微圆角 |
| `FrameRounding` | 3.0f | 输入框/按钮圆角 |
| `PopupRounding` | 4.0f | 弹窗圆角 |
| `ScrollbarRounding` | 4.0f | 滚动条圆角 |
| `GrabRounding` | 3.0f | 滑块抓手圆角 |
| `TabRounding` | 2.0f | Tab 顶部圆角 |
| `WindowBorderSize` | 1.0f | 窗口边框 |
| `FrameBorderSize` | 1.0f | 输入框边框 |
| `PopupBorderSize` | 1.0f | 弹窗边框 |
| `WindowPadding` | (8, 8) | 窗口内边距 |
| `FramePadding` | (6, 4) | 控件内边距 |
| `ItemSpacing` | (6, 4) | 控件间距 |
| `ItemInnerSpacing` | (6, 4) | 控件内部间距 |
| `CellPadding` | (4, 2) | 表格单元格内边距 |
| `ScrollbarSize` | 10.0f | 滚动条宽度 |

### 3.3 字体规范

| 字体 | 大小 | 用途 |
|------|------|------|
| 微软雅黑 Regular | 16px | 主 UI 文字、按钮、标签 |
| 微软雅黑 Regular | 14px | 次要文字、表格内容 |
| 微软雅黑 Regular | 12px | 极小文字、状态提示 |
| FontAwesome 6 Free Solid | 14px | 图标（文件夹、播放按钮、设置齿轮等） |
| JetBrains Mono / Consolas | 14px | 代码/控制台输出 |

---

## 四、扩展控件库（新增文件）

### 4.1 文件清单

| 文件 | 说明 |
|------|------|
| `src/editor/imgui/mg_imgui_theme.h/cpp` | 主题加载、切换、字体合并加载 |
| `src/editor/imgui/mg_imgui_extensions.h/cpp` | 扩展控件 |
| `src/editor/imgui/mg_imgui_dock_layout.h/cpp` | 默认 Dock 布局管理 |
| `src/editor/imgui/mg_imgui_utils.h` | 辅助绘制函数 |

### 4.2 扩展控件清单

```cpp
// mg_imgui_extensions.h

// ---- 属性编辑控件（Property Grid） ----
bool MG_PropertyLabel(const char* label);                          // 左侧标签，带tooltip提示
bool MG_PropertyInputText(const char* label, char* buf, size_t sz);
bool MG_PropertyInputInt(const char* label, int* v);
bool MG_PropertyInputFloat(const char* label, float* v, const char* fmt = "%.3f");
bool MG_PropertyInputVec2(const char* label, float v[2], const char* fmt = "%.3f");
bool MG_PropertyInputVec3(const char* label, float v[3], const char* fmt = "%.3f");
bool MG_PropertyInputVec4(const char* label, float v[4], const char* fmt = "%.3f");
bool MG_PropertyColorEdit3(const char* label, float col[3]);       // 带颜色预览
bool MG_PropertyColorEdit4(const char* label, float col[4]);
bool MG_PropertyCombo(const char* label, int* current_item, const char* items[], int items_count);
bool MG_PropertyCheckbox(const char* label, bool* v);
bool MG_PropertySliderFloat(const char* label, float* v, float min, float max);

// ---- 资源卡片控件（Content Browser 用） ----
bool MG_AssetCard(const char* name, ImTextureID icon, bool selected, float size = 64.0f);
void MG_AssetCardGridBegin(int columns);
void MG_AssetCardGridEnd();

// ---- 工具栏控件 ----
bool MG_ToolbarButton(const char* icon, const char* tooltip, bool active = false);
void MG_ToolbarSeparator();
void MG_ToolbarBegin();
void MG_ToolbarEnd();

// ---- 菜单栏增强 ----
bool MG_MenuItemIcon(const char* icon, const char* label, const char* shortcut = nullptr, bool selected = false, bool enabled = true);

// ---- 树形控件增强（Scene Outliner 用） ----
bool MG_TreeNodeEx(const char* label, const char* icon, ImGuiTreeNodeFlags flags = 0);

// ---- 视口面板 ----
void MG_ViewportBegin(const char* name, ImTextureID render_target);
void MG_ViewportEnd();
void MG_ViewportOverlayText(const char* text, ImVec2 pos, ImU32 color);

// ---- 状态栏 ----
void MG_StatusBarBegin();
void MG_StatusBarEnd();
void MG_StatusBarText(const char* text, ImU32 color = 0);

// ---- 分隔线增强 ----
void MG_SeparatorTextEx(const char* label, ImU32 color);           // 带文字的分隔线，UE5风格
```

---

## 五、ImGui 源码具体修改点

### 5.1 imgui.h / imgui_internal.h — 新增 Style 字段

```cpp
// 在 ImGuiStyle 结构体末尾追加（保持 ABI 兼容的位置）
struct ImGuiStyle {
    // ... 原有字段 ...
    
    // ---- MakeGames 新增字段 ----
    float       TabRoundingBottom;      // Tab 底部圆角（默认 = TabRounding，设为0可做出直角底部）
    ImVec2      TabTextPadding;         // Tab 文字两侧额外内边距
    float       SeparatorTextHeight;    // SeparatorText 的高度（用于居中计算）
    ImVec2      ToolbarButtonPadding;   // 工具栏按钮内边距
    float       PropertyLabelWidth;     // Property Grid 中标签列的默认宽度比例（0.0~1.0）
};
```

### 5.2 imgui_widgets.cpp — TabItem 绘制修改

目标：让 Tab 更像 UE5 的文档标签页（直角底部、悬停高亮、选中下划线/背景）。

修改函数：`ImGui::TabItemLabelAndCloseButton`, `ImGui::TabItemBackground`

```cpp
// 修改 TabItemBackground 中圆角计算逻辑：
// 当 style.TabRoundingBottom >= 0 时，底部使用指定的圆角值
// UE5 风格：顶部圆角 2px，底部圆角 0px
```

### 5.3 imgui_widgets.cpp — CollapsingHeader / TreeNode 绘制修改

目标：更像 UE5 的 Details Panel 中的可折叠分组。

- 箭头从 `>` 改为更精致的三角形（或保持默认但调整颜色）
- Header 背景高度增加
- 选中/悬停状态的背景色按 UE5 规范

### 5.4 imgui_draw.cpp — 矢量图标绘制

修改 `RenderCheckMark`, `RenderArrow`, `RenderBullet` 函数，使默认图标更精致：

- CheckMark：粗度适当、末端带圆角
- Arrow：更锐利的三角形或改为 chevron 风格

### 5.5 imgui_tables.cpp — 表格样式

- Header 行高度增加
- 列分隔线更细、颜色更淡
- 排序箭头更精致

---

## 六、编辑器 Dock 布局（UE5 默认布局）

```
┌─────────────────────────────────────────────────────────────────────┐
│  [MenuBar]  File  Edit  Window  Help                                │
├─────────────────────────────────────────────────────────────────────┤
│  [Toolbar]  [Play][Pause][Stop] | [Move][Rotate][Scale] | ...       │
├──────────┬──────────────────────────────────────────┬───────────────┤
│          │                                          │               │
│  Scene   │                                          │   Details     │
│  Outliner│           Viewport                       │   Panel       │
│  (20%)   │           (Central Node)                 │   (20%)       │
│          │                                          │               │
│          │                                          │               │
├──────────┤                                          ├───────────────┤
│ Content  │                                          │   World       │
│ Browser  │                                          │   Settings    │
│ (Bottom) │                                          │               │
│ 15% height                                          │               │
└──────────┴──────────────────────────────────────────┴───────────────┘
                         [StatusBar]
```

通过 `mg_imgui_dock_layout.cpp` 中的 `MG_LoadDefaultEditorLayout()` 用 `ImGui::DockBuilder` API 程序化构建。

---

## 七、实施阶段

### Phase 0：清理（当前）

- [ ] 删除所有废弃文件和目录（见清理清单）
- [ ] 更新 CMakeLists.txt 和相关构建配置
- [ ] 验证 `launcher` 和 `editor` 仍能编译运行（此时界面退化为默认 ImGui 样式）

### Phase 1：主题系统（预计 2-3 小时）

- [ ] 创建 `src/editor/imgui/mg_imgui_theme.cpp`
- [ ] 实现 `MG_ApplyTheme_UE5Dark()` 函数，完整设置所有 ImGuiStyle 字段
- [ ] 实现字体合并加载（微软雅黑 + FontAwesome）
- [ ] 在 `launcher/main.cpp` 和 `editor/main.cpp` 中调用主题初始化
- [ ] 验证：窗口、按钮、输入框、滑块、Tab 的配色和尺寸符合 UE5 规范

### Phase 2：扩展控件（预计 4-6 小时）

- [ ] 实现 Property Grid 系列控件
- [ ] 实现 Asset Card 控件
- [ ] 实现 Toolbar 控件
- [ ] 实现增强版 TreeNode（带图标）
- [ ] 实现 Viewport 面板包装
- [ ] 实现 StatusBar
- [ ] 在 Demo 窗口中展示所有扩展控件

### Phase 3：ImGui 源码微调（预计 3-4 小时）

- [ ] 修改 TabItem 底部圆角逻辑
- [ ] 修改 CollapsingHeader 高度和颜色
- [ ] 修改 CheckMark / Arrow 绘制
- [ ] 修改 Table Header 样式
- [ ] 每步修改后编译验证，确保无回归

### Phase 4：编辑器面板骨架（预计 4-6 小时）

- [ ] 实现 `MG_LoadDefaultEditorLayout()`
- [ ] 创建 `editor_app.cpp`：编辑器主类，管理 DockSpace
- [ ] Scene Outliner 面板（树形控件 + 搜索）
- [ ] Details Panel 面板（Property Grid 展示假数据）
- [ ] Content Browser 面板（Asset Card 网格 + 路径导航）
- [ ] Viewport 面板（占位渲染 + HUD）
- [ ] 菜单栏和工具栏

### Phase 5：Launcher 重皮肤（预计 2-3 小时）

- [ ] Launcher 界面应用 UE5 主题
- [ ] 项目卡片改用 AssetCard 风格
- [ ] 添加悬停动画和精致阴影

### Phase 6：打磨与细节（预计 3-4 小时）

- [ ] 调整所有控件的 hover/active 过渡（ImGui 默认是瞬间切换，可考虑在扩展层做颜色插值）
- [ ] 添加 subtle 的内发光/外发光效果（通过修改绘制顺序和颜色叠加）
- [ ] 验证中文显示、图标对齐、DPI 缩放
- [ ] 修复所有视觉不一致问题

---

## 八、文件变更总览

### 新增文件

```
src/editor/imgui/
  ├── mg_imgui_theme.h/.cpp
  ├── mg_imgui_extensions.h/.cpp
  ├── mg_imgui_dock_layout.h/.cpp
  └── mg_imgui_utils.h

src/editor/
  ├── editor_app.h/.cpp
  ├── panels/
  │   ├── outliner_panel.h/.cpp
  │   ├── details_panel.h/.cpp
  │   ├── content_browser_panel.h/.cpp
  │   └── viewport_panel.h/.cpp
  └── toolbar.h/.cpp
```

### 修改文件

```
imgui/imgui.h              # 新增 Style 字段（少量）
imgui/imgui_internal.h     # 同上
imgui/imgui_widgets.cpp    # TabItem, CollapsingHeader 绘制
imgui/imgui_draw.cpp       # RenderCheckMark, RenderArrow
imgui/imgui_tables.cpp     # Table Header 样式

launcher/main.cpp          # 应用新主题
editor/main.cpp            # 应用新主题 + 加载 Dock 布局
CMakeLists.txt             # 清理废弃引用
src/CMakeLists.txt         # 清理废弃引用
launcher/CMakeLists.txt    # 移除 test_ui.cpp
```

### 删除文件

```
src/ui_framework/          # 整个目录
src/ui/                    # 整个目录
imgui_backup/              # 整个目录
test_ninja/                # 整个目录
launcher/test_ui.cpp
launcher/ui/launcher_window.cpp.obj
docs/UIFW_Design_v1.md
根目录所有 .txt 日志/编译产物/测试文件
```

---

## 九、风险与应对

| 风险 | 概率 | 应对 |
|------|------|------|
| 修改 ImGui 源码后 upstream 升级困难 | 中 | 所有修改集中在特定函数，用 `// MG_MOD:` 注释标记，升级时可通过 diff 快速移植 |
| FontAwesome 图标字体版权/体积问题 | 低 | 使用 FontAwesome Free（SIL Open Font License），只加载所需 glyph 子集 |
| 中文字体文件过大导致启动慢 | 中 | 延迟加载：先显示英文字符，后台线程加载中文字体并合并 |
| Docking 布局 API 行为变化 | 低 | 只使用稳定的 `DockBuilder` API，避免依赖内部函数 |
| 视觉微调消耗时间超预期 | 高 | 设定明确的验收标准（色彩值、尺寸值），避免无休止的"再调一点" |

---

## 十、验收标准

1. **编译**：`launcher` 和 `editor` 零警告编译通过
2. **Launcher**：启动后呈现 UE5 风格暗色主题，项目卡片精致，无默认 ImGui 蓝/灰配色
3. **Editor**：
   - 全屏 DockSpace，默认布局符合 UE5 面板分布
   - 所有面板标题、按钮、输入框、滑块、Tab 的视觉风格统一
   - 属性面板能展示带图标的分组和各类输入控件
   - 资源浏览器显示带图标的资源卡片网格
   - 视口面板带坐标轴 HUD 和 FPS 显示
4. **中文**：所有界面文字正确显示中文，无乱码/问号
5. **图标**：按钮、菜单、树节点均有对应的 FontAwesome 图标
6. **对比度**：在普通 LCD 显示器上各层级背景可区分，文字可读

---

> **最后更新**：2026-08-14
> **状态**：全部完成。R1-R5 与 A2（ImGui Vulkan 后端提取）于 2026-06-10 完成；R6（3 处源码微调，均带 `// [MakeGames]` 标记）已提前实现。

---

## 十一、Editor 精致化（第二轮）

> **状态：R1-R5、A2 与 R6 均已完成（2026-08-14 核实）。**
>
> 骨架正确、组件库够用。当前问题不在颜色，在三个地方：**面板没 chrome**、**内容全是占位**、**零动画**。

### 问题清单（历史记录，已全部解决）

| 等级 | 问题 | 解决方式 |
|------|------|----------|
| 致命 | 面板无 chrome | `MG_BeginPanel` / `MG_EndPanel`，docked 时自动隐藏避免双标题 |
| 明显 | Viewport 是死的 | 极深色背景 + 20px 网格 + 红绿坐标轴 + 中心十字准线 |
| 明显 | Console 等于零 | 级别着色 + 清除/自动滚动图标按钮 + 过滤 Combo + IME 接入 |
| 一般 | 零动画 | `MG_GetAnimatedColor` 接入 AssetCard hover、ToolbarButton active |
| 一般 | 状态栏缺失 | `MG_StatusBarBegin/End/Text` 接入 editor 底部 |
| 一般 | 底部无 Tab | 由 DockSpace 默认布局自然解决 |

### 实施路线

```
P0    Editor IME 接入                      2 行 × N 处
R1    Panel Chrome 引擎                    ~100 行新 + 10 行改
R2    Viewport 网格 + 坐标轴               ~30 行
R3    Console 升级                         ~40 行
R4    动画 + 状态栏                        ~10 行
R5    ImDrawList 增强（选中线/引导线/Section/卡片hover） ~60 行，不改源码
R6    ImGui 源码微调（Tab线/Splitter/Scrollbar）       ✅ 已提前实现（3 处均带 `// [MakeGames]` 标记）
```

### R5 明细（不改 ImGui 源码）

- **5a 选中线**：Outliner / Content Browser 选中项左侧 2px accent 竖线
- **5b Section Header**：圆点 + 标题 + 延伸细线，替换裸 `Separator()`
- **5c 引导线**：Tree 父子层级间的半透明竖线
- **5d 卡片 hover**：accent 边框 + 轻微放大 + 平滑过渡

### R6 明细（3 处源码修改）

- **6a Tab 激活线**：活动 Tab 顶部 2px accent 横条（`TabItemBackground`）
- **6b Splitter 触感**：4px → 6px，hover 变亮（`DockNodeUpdateSplitter`）
- **6c Scrollbar 加宽**：grab hover 时绘制更宽（overlay 扩展，不改 `ScrollbarSize`）

### 不做列表

| 想法 | 理由 |
|------|------|
| Panel collapse 动画 | ImGui Begin/End 生命周期不支持中间帧 |
| 浮动窗口阴影 | 当前全部 docked，无浮动 panel 场景 |
| Viewport 3D 渲染 | 属性编辑器框架阶段不该碰管线 |
| 代码 minimap | 无代码编辑器场景 |
| 自定义字体渲染 | 当前微软雅黑已够好 |
