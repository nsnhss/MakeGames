# MakeGames 2D 框架规划

> ⚠️ **历史快照**：本文档为早期框架规划，部分设计与实现已漂移（Box2D 已否决、横板模式暂停且无实现、Tileset passage/Prefab 等描述与现状不符），**不再维护**。实际进度以 [`Progress.md`](../Progress.md) 为唯一事实源。

> 本文档是**架构规划**（项目目录结构、地图/Tileset 格式、编辑器工作流、运行时架构、渲染管线）。
> Component 与事件系统的**字段级详细定义**见 [`docs/GameDataModel.md`](../GameDataModel.md)。

> 参考对象：RPG Maker MV/MZ（数据驱动、JSON 明文）、Godot（场景树、文件即资源）、GameMaker（资源树、Room/Object 分离）、Unity 2D（Prefab/Scene、GUID 引用）

---

## 1. 核心设计哲学

| 原则 | 说明 |
|------|------|
| **文件即资源** | 像 Godot 一样，文件系统就是资源数据库。不建中央 AssetDatabase，直接用路径 + GUID 引用。 |
| **JSON 为主，二进制缓存为辅** | 所有源数据存为 `.json`（版本控制友好、可人工编辑），运行时自动编译为 `.bin`/`.cache` 加速加载。 |
| **ECS 原生** | 2D 场景底层走 ECS，编辑器提供可视化封装（Map = 场景，Entity = 对象，Component = 属性）。 |
| **数据与表现分离** | RPG Maker 的优点：角色数据、地图数据、事件逻辑全部纯数据，换皮不改代码。 |
| **渐进复杂度** | 新手可以零代码做简单 RPG/平台跳跃；进阶用户写 Lua/C++ 脚本扩展。 |
| **双模式原生支持** | 俯视角（Top-down）与横板（Side-scroller）作为引擎一等公民，项目创建时选定模板，底层系统与数据格式各自特化。 |

---

## 1.5 两种 2D 模式概述

引擎在创建项目时提供两种 **2D 项目模板**，底层共享 ECS + 渲染 + 资源系统，但地图格式、物理、相机、事件系统各自特化：

### A. 俯视角（Top-down）
类似 RPG Maker / 星露谷物语 / 塞尔达 2D。

| 特性 | 说明 |
|------|------|
| **视角** | 从正上方俯视，无重力概念 |
| **移动** | 四方向或八方向自由移动，按像素或按格（grid-based） |
| **地图** | Tilemap 多层（地面、建筑、装饰、屋顶），**Y-sort** 决定实体与图块的遮挡关系 |
| **碰撞** | 基于 Tile 的通行度（Passage：上/下/左/右 是否可通行）+ 可选的 BoxCollider |
| **相机** | 跟随玩家，可限制在地图边界内，支持视差背景（Parallax） |
| **典型游戏** | JRPG、ARPG、生存建造、恋爱模拟 |

### B. 横板（Side-scroller）
类似马里奥 / 空洞骑士 / 洛克人。

| 特性 | 说明 |
|------|------|
| **视角** | 侧面观察，有明确的地平面 |
| **移动** | 左右移动 + 跳跃/下落，受重力影响 |
| **地图** | Tilemap 以**碰撞层**为核心（地面、平台、斜坡、单向板），实体在 Tile 上行走/跳跃 |
| **碰撞** | Box2D / Tile-based 混合：角色用 RigidBody（动态），地形用静态碰撞体 |
| **相机** | 平滑跟随玩家，通常有死区（Dead Zone），支持纵向卷轴/房间切换（Room-based） |
| **典型游戏** | 平台跳跃、横板动作、横板射击、银河恶魔城 |

### 模式差异在数据层面的体现

| 差异点 | Top-down | Side-scroller |
|--------|----------|---------------|
| `project.json` 中 `mode` 字段 | `"2d_topdown"` | `"2d_sidescroller"` |
| 地图 JSON 中 `layers` | 多 Tile 层（地面/建筑/装饰） | 更强调 `collision` 层（地形碰撞数据） |
| 实体移动 Component | `TopDownMovement`（速度+方向） | `PlatformerMovement`（速度+重力+跳跃缓冲+土狼时间） |
| 摄像机 Component | `TopDownCamera`（边界限制） | `SideScrollerCamera`（死区、房间锁定） |
| 事件触发 | 靠近/按键/进入区域 | 同上，但多了"落地触发""边缘触发" |
| 预制实体 | 角色四方向行走图 | 角色带 idle/run/jump/fall 动画状态机 |

> 两种模式可在编辑器中**随时切换预览**（切换后地图数据和实体组件自动适配），但正式项目建议一开始就选定。

---

## 2. 项目目录结构

```
MyGame/
├── project.json              # 项目入口：名称、引擎版本、起始场景、全局设置
├── project.settings.json     # 编辑器用户设置（不提交 Git）
│
├── assets/                   # 原始资源（用户直接拖进来的文件）
│   ├── sprites/              # 角色、物品、特效图（.png/.jpg）
│   ├── tilesets/             # 图块集（.png + .tileset.json）
│   ├── backgrounds/          # 背景/视差图
│   ├── animations/           # 动画序列帧（引用 sprites/ 下的图）
│   ├── audio/
│   │   ├── bgm/              # 背景音乐
│   │   ├── bgs/              # 环境音效
│   │   ├── se/               # 音效
│   │   └── me/               # 音乐效果（升级音等）
│   ├── fonts/                # 自定义字体
│   └── scripts/              # Lua 脚本 / C++ DLL
│
├── data/                     # 游戏数据（纯 JSON，RPG Maker 风格）
│   ├── actors.json           # 角色定义（属性、成长曲线、初始装备）
│   ├── classes.json          # 职业/类别
│   ├── items.json            # 物品数据库
│   ├── skills.json           # 技能数据库
│   ├── enemies.json          # 敌人数据库
│   ├── troops.json           # 敌人编队
│   ├── states.json           # 状态/buff
│   ├── animations.json       # 动画特效定义（引用 assets/animations/）
│   ├── tilesets.json         # 图块集元数据索引
│   ├── system.json           # 全局系统设置（初始金钱、窗口肤色、 terminology）
│   └── common_events.json    # 公共事件（类似 RPG Maker 的 Common Event）
│
├── maps/                     # 地图/关卡（每个地图一个文件）
│   ├── Map001.json
│   ├── Map002.json
│   └── MapInfos.json         # 地图列表索引（名称、尺寸、父子关系）
│
├── scenes/                   # 场景模板（非地图类场景：主菜单、战斗场景等）
│   ├── main_menu.scene.json
│   └── battle.scene.json
│
├── prefabs/                  # 可复用实体模板（ECS 数据快照）
│   ├── player.prefab.json
│   └── slime_enemy.prefab.json
│
└── .cache/                   # 运行时编译缓存（自动产生，加入 .gitignore）
    ├── imported/             # 导入后的纹理、音频（压缩/切图后）
    └── compiled/             # JSON 编译后的二进制（.bjson / .bscene）
```

> **为什么分 `data/` 和 `assets/`？**
> - `assets/`：二进制文件（图、音、字），由外部工具（PS、Aseprite、FL Studio）生产。
> - `data/`：纯文本数据，可由编辑器直接修改，也可手写/脚本批量生成。
> - 这种分离是 RPG Maker 被广大非程序员接受的核心原因。

---

## 3. 文件格式设计

### 3.1 project.json（项目入口）

```json
{
    "name": "MyPlatformer",
    "engine_version": "0.1.0",
    "mode": "2d_sidescroller",
    "start_scene": "maps/Map001.json",
    "display": {
        "title": "我的第一个游戏",
        "width": 1280,
        "height": 720,
        "fullscreen": false,
        "vsync": true,
        "pixel_perfect": true
    },
    "tile_size": 32,
    "default_font": "assets/fonts/default.ttf"
}
```

### 3.2 Map 格式（地图/关卡）

参考 RPG Maker 的 `Mapxxx.json` + Godot 的 TileMap 层设计。两种模式共用同一套 Map Schema，但层的含义和默认组件不同：

#### 横板 Map 示例

```json
{
    "id": 1,
    "name": "1-1 草原",
    "width": 40,
    "height": 20,
    "tile_size": 32,
    "mode": "2d_sidescroller",
    "bgm": { "name": "Stage1", "volume": 80, "pitch": 100 },
    "background_color": "#87CEEB",
    "layers": [
        {
            "name": "背景装饰",
            "type": "tile",
            "tileset": "tilesets/Stage1_Decor.tileset.json",
            "data": [0, 0, 15, ...],
            "visible": true,
            "parallax_factor": 0.3
        },
        {
            "name": "地形",
            "type": "tile",
            "tileset": "tilesets/Stage1_Ground.tileset.json",
            "data": [0, 0, 1, 1, ...],
            "visible": true
        },
        {
            "name": "碰撞",
            "type": "collision",
            "data": [0, 0, 1, 1, ...],
            "collision_shapes": {
                "1": "solid",
                "2": "platform",      // 单向平台，可跳上去
                "3": "slope_left",    // 左斜坡
                "4": "slope_right"    // 右斜坡
            }
        },
        {
            "name": "实体层",
            "type": "entity",
            "entities": [
                {
                    "prefab": "prefabs/player.prefab.json",
                    "x": 2, "y": 15,
                    "overrides": {}
                },
                {
                    "prefab": "prefabs/slime.prefab.json",
                    "x": 10, "y": 15,
                    "patrol_range": [8, 18]
                }
            ]
        }
    ],
    "events": [
        {
            "id": 1,
            "name": "关卡终点",
            "x": 38, "y": 14,
            "width": 1, "height": 2,
            "pages": [
                {
                    "trigger": "player_enter",
                    "commands": [
                        { "cmd": "transfer_player", "params": ["maps/Map002.json", 2, 3] }
                    ]
                }
            ]
        }
    ],
    "camera_bounds": {
        "min_x": 0, "max_x": 1280,
        "min_y": 0, "max_y": 640
    },
    "rooms": [
        // 预留：银河恶魔城式房间分区
    ]
}
```

> 横板地图的 `collision` 层是核心：每个 tile 标记为 `solid` / `platform` / `slope` 等，运行时生成静态碰撞体。

---

#### 共用字段说明

| 字段 | Top-down | Side-scroller |
|------|----------|---------------|
| `layers` 中 `type=tile` | 地面、建筑、装饰、屋顶多层叠加 | 背景、地形、前景多层 |
| `layers` 中 `type=entity` | 实体按 **Y-sort** 与 Tile 层混合排序 | 实体按 `sorting_order` 排序，通常在地形之上 |
| `layers` 中 `type=collision` | 可选（通常用 Tileset 通行度） | **必需**，定义地形碰撞形状 |
| `parallax` / `parallax_factor` | 远景视差滚动 | 多层背景视差（近景快、远景慢） |
| `events` | 靠近触发、按键触发、进入区域 | 同上 + `player_enter` / `player_land` |
| `camera_bounds` | 限制相机不出地图边界 | 限制相机死区 + 房间边界 |

---

### 3.3 俯视角专用：Tileset 通行度

俯视角不依赖独立的 collision 层，而是在 Tileset 中定义每个 tile 的通行度：

```json
{
    "id": 0,
    "passage": [1, 1, 1, 1],
    "terrain_tag": 0
}
```

- `passage[0..3]` 分别表示：**上、右、下、左** 是否可通行（1=可，0=不可）。
- 运行时，角色移动时查询脚下 tile 的四方向通行度，实现格子移动或像素移动的阻挡。

---

### 3.4 横板专用：碰撞层形状标记

横板的 `collision` 层 `data` 中的每个值对应 `collision_shapes`：

| 形状 | 说明 |
|------|------|
| `solid` | 完全实心，不可穿透 |
| `platform` | 单向平台：从下方跳上去可站立，从上方落下可穿过（按方向键下可主动落下） |
| `slope_left` | 左高右低的斜坡 |
| `slope_right` | 右高左低的斜坡 |
| `ladder` | 梯子：可上下攀爬，无视重力 |
| `water` | 水域：进入后重力改变、跳跃力改变 |

### 3.5 Tileset 格式

```json
{
    "name": "Overworld",
    "image": "assets/tilesets/overworld.png",
    "tile_width": 32,
    "tile_height": 32,
    "columns": 8,
    "total_tiles": 64,
    "tiles": [
        {
            "id": 0,
            "passage": [1, 1, 1, 1],
            "terrain_tag": 0,
            "animation": null
        },
        {
            "id": 45,
            "passage": [0, 0, 0, 0],
            "terrain_tag": 1,
            "animation": {
                "frames": [45, 46, 47],
                "speed": 8
            }
        }
    ],
    "autotiles": [
        // 预留：自动图块（瓦片自动拼接规则）
    ]
}
```

### 3.6 Actor / Entity 数据格式（data/actors.json）

```json
[
    {
        "id": 1,
        "name": "勇者",
        "nickname": "传说中的少年",
        "class_id": 1,
        "initial_level": 1,
        "max_level": 99,
        "sprite": "assets/sprites/hero_walk.png",
        "face": "assets/faces/hero_face.png",
        "params": {
            "mhp": [450, 5000],
            "mmp": [50, 500],
            "atk": [15, 150],
            "def": [12, 120],
            "mat": [8, 80],
            "mdf": [8, 80],
            "agi": [10, 100],
            "luk": [10, 100]
        },
        "equips": [1, 0, 0, 0, 0]
    }
]
```

### 3.7 Prefab 格式（ECS 数据快照）

```json
{
    "name": "slime_enemy",
    "components": {
        "transform": {
            "position": [0, 0, 0],
            "rotation": 0,
            "scale": [1, 1]
        },
        "sprite": {
            "texture": "assets/sprites/slime.png",
            "pivot": [0.5, 1.0],
            "sorting_order": 0
        },
        "rigidbody": {
            "type": "dynamic",
            "gravity_scale": 1.0
        },
        "box_collider": {
            "size": [28, 20],
            "offset": [0, -6]
        },
        "enemy_ai": {
            "patrol_range": 3,
            "aggro_range": 5,
            "script": "scripts/enemies/slime_ai.lua"
        }
    }
}
```

> Prefab 本质上是 ECS Component 的 JSON 序列化。编辑器里拖拽 = 实例化 + 可选 override。

### 3.8 Script 格式

采用 **Lua** 作为首选脚本语言（轻量、易学、与 C++ 绑定成熟）。

```lua
-- scripts/enemies/slime_ai.lua
local SlimeAI = {}

function SlimeAI:on_init(entity)
    self.patrol_start = entity.transform.position.x
    self.direction = 1
end

function SlimeAI:on_update(entity, dt)
    local rb = entity.rigidbody
    local pos = entity.transform.position

    -- 简单巡逻
    if pos.x > self.patrol_start + 3 * 32 then
        self.direction = -1
        entity.sprite.flip_x = true
    elseif pos.x < self.patrol_start - 3 * 32 then
        self.direction = 1
        entity.sprite.flip_x = false
    end

    rb.velocity.x = self.direction * 50
end

return SlimeAI
```

> 预留 C++ DLL 热重载路径给需要极致性能或复杂系统的用户。

---

## 4. 资源存储与引用策略

### 4.1 GUID 系统（类似 Unity）

每个资源文件在首次被引擎识别时，生成一个 `.meta.json`：

```json
// assets/sprites/hero.png.meta.json
{
    "guid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
    "import_settings": {
        "filter": "nearest",
        "wrap": "clamp",
        "spritesheet": {
            "grid_width": 32,
            "grid_height": 32,
            "columns": 4,
            "rows": 4
        }
    }
}
```

- 引擎内部用 GUID 引用资源，搬家/改名不丢引用。
- `.meta.json` 必须提交 Git，`.cache/` 不提交。
- 如果 `.meta.json` 丢失，按文件路径重新生成 GUID（保持跨机器一致性的兜底）。

### 4.2 导入管线（Import Pipeline）

```
用户拖入 PNG  →  生成 .meta.json  →  引擎导入  →  写入 .cache/imported/
                                              ↓
                                    切图/压缩/生成 mipmap/
                                    生成对应的 Texture2D 资源
```

运行时只读 `.cache/` 里的编译后资源，不碰原始 `assets/`。

### 4.3 版本控制友好性

| 文件类型 | 是否提交 Git | 理由 |
|----------|-------------|------|
| `.json` 数据 | ✅ | 纯文本，diff 友好 |
| `.png/.ogg/.ttf` | ✅ | 原始资源，必须提交 |
| `.meta.json` | ✅ | GUID 和导入设置 |
| `.cache/` | ❌ | 自动生成，加入 `.gitignore` |
| `project.settings.json` | ❌ | 用户本地设置 |

---

## 5. 编辑器工作流（让别人怎么制作）

### 5.1 地图编辑器（Map Editor）

类似 RPG Maker 的地图编辑 + Godot 的 TileMap 层级：

1. **左侧**：Tileset 面板（显示当前图块集，支持多 tab 切换）。
2. **中央**：地图画布（网格显示，左键绘制，右键擦除，滚轮缩放）。
3. **右侧**：
   - 层管理器（增删改图层顺序、可见性、不透明度）。
   - 当前 Tile 属性（通行度、地形标记、动画帧）。
4. **底部**：事件编辑器（选中格子 → 编辑事件命令页）。

**快捷操作**：
- `B` 铅笔工具 / `E` 橡皮 / `F` 填充桶 / `R` 矩形填充
- 按住 `Shift` 画直线
- 右键快速取色（Eyedropper）

### 5.2 数据库编辑器（Database Editor）

RPG Maker 风格的大表格编辑器：

- **Actors**：角色列表，双击打开详情面板编辑属性曲线。
- **Items/Skills/Enemies**：标签页切换，表格展示基础属性。
- **Animations**：可视化动画编辑器（时间轴 + 关键帧 + 特效预览）。
- **System**：全局设置（术语、初始队伍、窗口皮肤、音乐）。

### 5.3 事件系统（可视化脚本）

为不会编程的用户提供 RPG Maker 风格的**命令式事件系统**：

```
▶ 显示文字: "村长", "欢迎来到村庄！"
▶ 显示选项: ["购买", "离开"] → 存入变量: choice_result
▶ 条件分支: 如果 choice_result == 0
    ▶ 打开商店: [生命药水, 铁剑]
▶ 结束分支
```

每个命令是一个预制逻辑块，用户只需填参数。进阶用户可插入**脚本调用**节点执行 Lua。

### 5.4 Prefab / 实体编辑器

- 在 Content Browser 右键 → `Create Prefab`
- 打开后进入小视口：中央显示该实体，右侧 Inspector 添加/删除 Component
- 组件列表：`Sprite`, `RigidBody`, `BoxCollider`, `LuaScript`, `AnimationPlayer`...

### 5.5 脚本编辑器

- **初学者**：事件系统够用，不写代码。
- **进阶**：内置 Lua 编辑器（基于 ImGui 的代码输入框 + 语法高亮，或外接 VS Code）。
- **专业**：C++ DLL 热重载，外接任意 IDE。

---

## 6. 运行时架构

### 6.1 加载流程

```
启动游戏
  → 读取 project.json
  → 初始化 Renderer / Window / Audio
  → 加载 system.json（全局设置）
  → 加载起始 Map/Scene
      → 解析 Map JSON
      → 按层顺序实例化 Tilemap（生成静态碰撞体）
      → 实例化 Entity 层（Prefab 展开为 ECS Entity）
      → 实例化 Events（注册触发器到区域系统）
  → 进入主循环
```

### 6.2 ECS System 清单（2D）

| System | 职责 |
|--------|------|
| `SpriteRenderSystem` | 收集所有 Sprite + Transform，按 sorting_order 排序，批量渲染 |
| `TilemapRenderSystem` | 渲染 Tilemap 层（静态批处理） |
| `AnimationSystem` | 更新 AnimationPlayer 组件的当前帧、触发事件 |
| `PhysicsSystem` (Box2D v3) | 更新 RigidBody + Collider，生成碰撞事件 |
| `ScriptSystem` | 每帧调用 Lua 脚本的 `on_update`，分发输入/碰撞事件 |
| `CameraSystem` | 根据目标 Entity 平滑跟随，限制边界 |
| `EventSystem` | 地图事件触发器（靠近/按键/进入区域） |
| `AudioSystem` | BGM/BGS/SE/ME 的播放、淡入淡出、3D 音效 |
| `UISystem` | 游戏内 UI（对话框、菜单、血条）的渲染和交互 |

### 6.3 渲染管线（2D）

```
[Render Target: Game Viewport]
  ├─ 背景层（纯色 / Parallax / 远景 Tilemap）
  ├─ 远景 Sprite 层（sorting_order < 0）
  ├─ 地图 Tilemap 层（按层顺序）
  ├─ 实体 Sprite 层（sorting_order = 0，Y-sort 可选）
  ├─ 特效层（粒子、动画）
  ├─ 近景 Sprite 层（sorting_order > 0）
  └─ UI 层（对话框、菜单、HUD）
```

- 2D 渲染用**正交相机**，支持像素完美（`pixel_perfect: true` 时禁止子像素渲染）。
- Tilemap 大场景用**视锥剔除**，只渲染屏幕内的 tile。

---

## 7. 关键决策记录

| # | 决策项 | 选择 | 理由 |
|---|--------|------|------|
| 1 | 地图格式 | **JSON + 自定义 Map 格式** | RPG Maker 验证过的人类可读格式，编辑器好做，版本控制友好 |
| 2 | 脚本语言 | **Lua 为主，C++ DLL 为辅** | Lua 轻量易学，非程序员能上手；C++ 给专业用户兜底 |
| 3 | 资源引用 | **GUID + 相对路径双保险** | Unity 的 GUID 方案成熟，防搬家断链 |
| 4 | 事件系统 | **命令式事件块 + Lua 脚本调用** | RPG Maker 的核心优点，零代码门槛；Lua 扩展无上限 |
| 5 | 图块碰撞 | **Tileset 层存 passage + 独立碰撞层** | RPG Maker 的四方向通行度简单直观；独立碰撞层给平台跳跃更灵活 |
| 6 | 场景切换 | **Map 为主场景单位，Scene 为 UI/特殊场景** | RPG Maker 的 Map 概念深入人心；Scene 处理主菜单、战斗等非地图画面 |
| 7 | 实体复用 | **Prefab（ECS 数据快照）** | Unity/Godot 的通用方案，与 ECS 架构天然契合 |

---

## 8. 开发阶段建议（2D 模块）

### Phase 1：2D 资源与数据框架
- [x] 定义核心 JSON Schema（project.json）
- [x] 实现 `DataManager`：加载/保存 JSON
- [x] 实现 `MetaFile` 系统：GUID 生成与解析
- [ ] 实现完整 `ImportPipeline`：图片导入 ✅ 部分完成（Content Browser Import），SpriteSheet 切分/压缩缓存延后

### Phase 2：2D 渲染基础
- [x] 2D 正交相机 + 视口系统
- [x] `Sprite` Component + `SpriteRenderSystem`
- [x] `Tilemap` Component + `TilemapRenderSystem`
- [x] 基础 Batch Rendering / 实例化（TD-N，性能优化延后）（2026-08-14 完成：instancing + 整帧单次上传 + sorting_order 排序 + 纹理图集跨纹理单 draw）

### Phase 3：地图编辑器（核心编辑能力）
- [x] 地图画布（网格 + 绘制工具）
- [x] Tileset 面板（加载 `.tileset.json` + 图片预览）
- [x] 层管理器（增删改、顺序、可见性）✅ Phase K 完成
- [x] 地图属性面板（尺寸、BGM、背景、传送区域）✅ 2026-08-14：Scene Properties 弹窗（尺寸/Tile Size/BGM/背景色）+ New Scene 对话框可指定宽高/tile 尺寸；修复 Tile Size 不同步 Tilemap 的 bug；传送区域仍由事件系统承担
- [x] 序列化保存为 `.json`

### Phase 4：实体与 Prefab
- [x] ECS 中支持 Prefab 实例化 + Override（2026-08-14 部分完成：手动活链接 PrefabRef + Apply/Revert/Unlink + 可 Undo；自动传播与组件级 override 明确不做）
- [x] 实体放置工具（在地图中点击放置基础实体：Transform+Name+Sprite）✅ Phase K1a 完成
- [x] Inspector 编辑 Component 属性
- [ ] Content Browser 右键 Create Prefab（延后）

### Phase 5：事件与脚本
- [x] 事件命令系统（22 个命令全部实现）
- [x] Lua 绑定（手动 C API 绑定，零新依赖）：Entity / Transform / Input / Audio 四组已接入（2026-08-14，详见 Progress.md；Sol2 明确不迁）
- [x] 事件编辑器（命令列表的可视化编辑）

### Phase 6：数据库编辑器
- [ ] Actors / Items / Skills / Enemies 表格编辑器
- [x] 属性曲线编辑（升级成长）✅ 2026-08-14 MVP：Actor 成长曲线（base→peak + exponent 早熟/线性/晚成）+ EvalStatCurve 纯函数 + 面板曲线预览，详见 Progress.md
- [x] Animation 编辑器（时间轴 + 关键帧）✅ 2026-08-14 MVP：TweenAnimation 组件 + Animation 面板（线性插值、clip 内嵌组件、4 轨道、编辑器预览），详见 Progress.md

---

## 9. 与现有引擎架构的对接（已落地）

| 层级 | 状态 | 说明 |
|------|------|------|
| **Core 层** | ✅ | `Vec2` / `Color` 已实现，`Mat4` 等 3D 数学预留 |
| **Platform 层** | ✅ | Window / Input / Event 已完成 |
| **Renderer 层** | ✅ | 2D Pipeline（正交投影、纹理采样、Alpha 混合）+ Viewport Offscreen RT 已实现 |
| **Scene 层** | ✅ | ECS World + 10 个 Component + 4 个 System 已落地 |
| **Editor 层** | ✅ | Docking + Outliner / Details / Viewport / Console / Content Browser / Event Editor / Tile Palette 已完成 |
| **Resource 层** | ✅ | `DataManager` + `MetaFile` + GUID + `ResourceManager`（纹理+Sampler DS 缓存）已实现 |
| **Audio 层** | ✅ | `AudioManager`（SDL2 原生）PlaySE / PlayBGM / StopBGM 已实现 |

## 10. 下一步行动

1. **A1**：ImGui R6 微调（Tab accent / Splitter / Scrollbar）
2. ~~**TD-N**：Sprite 批处理 / 实例化（性能优化）~~（2026-08-14 已完成，含纹理图集）
3. **K1b**：实体 Duplicate（需先实现通用组件克隆机制）
4. **Lua 绑定**：接入 Sol2 或手动 Lua C API，替换 `CallScript` 占位日志
4. **从 Map JSON Schema 开始**：先定义格式，再做 Map 加载器，最后做编辑器画布。
