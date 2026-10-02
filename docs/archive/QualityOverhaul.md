# 产品质量整改方案

> 2026-06-15。5 阶段，本期做 1-3，4-5 延后。

> **阶段一已完成并验证通过**（2026-06-16）：白屏、闪烁、Camera 跟随/边界、地面铺满均修复。

> **阶段二、三已完成并验证通过**（2026-06-16）：编辑器 UX 重构 + 实体创建体系。

---

## 阶段一：修根基（已完成 ✅）

### 1.1 白屏
**根因**：`LoadTexture` 相对路径，CWD 不是项目根 → 找不到 → 白色默认纹理。

- `resource_manager.h`：加 `SetProjectPath(path)`
- `resource_manager.cpp`：`LoadFromFile` 前拼 `project_path_ + "/" + path`
- `editor_context.cpp`：`OpenProject` 末尾调用 `SetProjectPath`

### 1.2 闪烁
**根因**：tilemap 用 sprite instanced 管线，只绑 binding 0，binding 1 读上一帧 sprite 残留数据 → 顶点偏移。

- `tilemap_render_system.h`：Config 加 `dummy_instance_vb`
- `tilemap_render_system.cpp`：Render 开头绑 dummy 到 binding 1
- `editor_application.cpp`：OnInit 创建 10-float-zero Buffer 赋给 tilemap_cfg_

### 1.3 Camera 跟随
**根因**：当前隐式 fallback 查找，非显式绑定。

- `editor_context.cpp`：CreateDefaultScene 中 player 移到 camera 前创建，`cam.follow_target = player`

### 1.4 Camera 边界
**根因**：Camera 可移出地图 → 见灰色空位。

- `editor_layer.cpp:375`：跟随后查 Tilemap 尺寸 → `std::clamp` 相机到边界内

### 1.5 地面铺满
**根因**：Ground 层全 -1（空）→ 透出灰色。

- `editor_context.cpp`：Ground 层创建后 `fill(tiles, 0)`

---

## 阶段二：编辑器 UX 重构（已完成 ✅）

### 2.1 删除 Content Browser
**根因**：独立存在但无联动，用户不知道用途。

- ✅ 删除 `src/editor/panels/content_browser_panel.h/.cpp`
- ✅ 取消注册
- ✅ `File → Import Assets` 接管导入功能
- ⏸️ Prefab 实例化 / Create Prefab 延后实现

### 2.2 Details 改弹窗
**根因**：常驻面板占用空间，编辑频率低。

- ✅ 改名为 **Scene Explorer**
- ✅ 双击 Outliner 实体弹出
- ✅ `ImGuiWindowFlags_NoDocking`，不参与 Dock 布局

### 2.3 Event Editor 改弹窗
**根因**：常驻面板，与实体无关联感。

- ✅ 右键 Outliner 实体 → `Edit Events` 弹出
- ✅ `ImGuiWindowFlags_NoDocking`

### 2.4 Database 改菜单弹窗
**根因**：低频操作占用常驻面板。

- ✅ `File → Database` 打开 `Item Database` 弹窗
- ✅ `ImGuiWindowFlags_NoDocking`

### 2.5 Tile Palette 改浮窗
**根因**：画 tile 时常驻但占面板空间。

- ✅ 工具栏 `Tile Paint` 按钮触发
- ✅ 再点一次按钮关闭
- ✅ Play Mode 自动隐藏，Stop 后恢复之前状态
- ✅ `ImGuiWindowFlags_NoDocking`

### 2.6 工具栏精简
**根因**：按钮多且部分无用（T/R/S 已弃用）。

- ✅ 精简为：`[Play] [Pause] [Stop] | [Tile] [Entity] [Create▼] | [Undo] [Redo]`
- ✅ 去掉 T/R/S、Grid、旧视图模式按钮

### 2.7 Window 菜单简化
**根因**：列出 8 个面板，实际只需 2 个常驻。

- ✅ 弹窗面板不在 Window 菜单列出
- ✅ `Reset Layout` 只恢复常驻面板
- **实际保留**：Viewport / Scene Outliner / Console / Reset Layout
  - Console 按使用习惯保留为底部可隐藏面板，不是弹窗

---

## 阶段三：创建体系（已完成 ✅）

### 3.1 统一 Create 入口
**根因**：创建实体入口分散（右键空白、Entity Place、Outliner 右键），各创建不同内容。

- ✅ Toolbar `[Create▼]` 下拉
- ✅ Viewport 右键 `Create` 子菜单
- ✅ Outliner 右键空白处 `Create` 子菜单
- ✅ 三种入口统一调用 `CreateEntityFromTemplateAtCameraCenter`

### 3.2 7 种类型初始化
**根因**：只有 "Create Empty Entity" 和 Entity Place（空壳），不实用。

| 类型 | 初始化组件 |
|------|-----------|
| Entity | Transform + Name("Entity") + Sprite |
| NPC | Transform + Name + Sprite + EventTrigger(OnInteract,空页) + Movement(speed=0) |
| Monster | Transform + Name + Sprite + EventTrigger(OnTouch, BattleProcessing空) + Movement(speed=0) |
| Chest | Transform + Name + Sprite + EventTrigger(OnInteract, ShowToast+AddItem空, run_once=true) |
| Transfer | Transform + Name + Sprite + EventTrigger(OnTouch, TransferPlayer空) |
| Shop | Transform + Name + Sprite + EventTrigger(OnInteract, ShopProcessing空) + Movement(speed=0) |
| Decoration | Transform + Name("Decoration") + Sprite |

### 3.3 创建位置
**根因**：目前创建在屏幕点击处/屏幕中心，不合理。

- ✅ `GetCameraWorldCenter()` 取主相机世界中心
- ✅ 所有 Create 菜单在该位置生成实体

### 3.4 前置确认
**根因**：EventTrigger / 各种 Action 是否已实现未知。

- ✅ `BattleProcessing`、`TransferPlayer`、`ShopProcessing`、`AddItem`、`ShowToast` 在 `event_system.cpp` 均已实现

---

## 阶段四：多地图管理 + 布局持久化 + 打磨（已完成 ✅）

### 4.1 多地图管理
- ✅ Outliner 顶部场景下拉框，列出 `MapInfos.json` 所有场景
- ✅ 切换场景时自动保存当前场景
- ✅ 未保存场景名旁显示 `*`
- ✅ `File → New Scene` / Outliner `+ New Scene` 创建新地图
- ✅ `CreateAndSaveNewScene` 保存失败时回滚，不丢失原场景
- ✅ 场景右键 Rename / Delete，同步更新 `MapInfos.json` 和 `project.json` 的 `start_scene`
- ✅ 删除当前场景后自动切换到剩余场景

### 4.2 TransferPlayer 场景选择
- ✅ `TransferPlayer` 的 `target` 字段存场景文件名（如 `Village.json`）
- ✅ `EditorLayer` 执行时从 `MapInfos.json` 查文件名拼完整路径
- ✅ `Event Editor` 中 `Target` 字段旁边加 `[...]` 按钮选择目标场景

### 4.3 布局持久化补全
- ✅ `imgui.ini` 保存到项目目录
- ✅ `data/editor_layout.json` 跳过弹窗面板持久化

### 4.4 打磨
- ✅ `tilemap_render_system.cpp` 无残留 push constant（已确认）
- ✅ `Scene Explorer` / `Event Editor` 弹窗标题显示实体名
- ✅ `File → Recent Projects` 最近打开项目列表（存 `%APPDATA%/MakeGames/recent_projects.json`）

---

## 阶段五：导出系统（下期）

### 5.1 编辑器构建
- 用阶段三的 Create 体系手动作"新手村"

### 5.2 导出验证
- Export → 运行 game.exe

### 5.3 随引擎发布
- `example_project/` 放入发布包，Launcher 自动检测
