# MakeGames 游戏数据模型设计

> 本文档是**数据规范**（Component 字段定义、事件命令规格、全局状态格式）。
> 架构规划、项目目录结构、地图格式、Tileset 设计见 [`docs/2DFrameworkPlan.md`](2DFrameworkPlan.md)。

> 目标：定义 2D 游戏的核心类型、属性、事件系统，兼顾**零代码可视化制作**与**脚本扩展自由度**。

---

## 1. 核心设计原则

| 原则 | 说明 |
|------|------|
| **Component 即能力** | 一个 Entity 有什么行为，取决于它挂载了哪些 Component，不预设"类型"限制 |
| **Prefab 即模板** | 常用组合（如"玩家"）保存为 Prefab，实例化时可 Override 任意属性 |
| **事件是数据** | 事件触发条件、动作序列全部存为 JSON，编辑器可视化编辑 |
| **脚本兜底** | 当事件系统不够用时，用 Lua/C++ 脚本直接操作 Component |

---

## 2. ECS Component 设计

### 2.1 基础组件（所有 Entity 都可能需要）

```cpp
struct Transform {
    Vec2 position = {0, 0};      // 世界坐标（像素）
    float rotation = 0.0f;       // 角度（度）
    Vec2 scale = {1, 1};         // 缩放
    int   sorting_order = 0;     // 渲染排序层级
};

struct Name {
    std::string value = "Entity"; // 编辑器显示名
};

struct Tag {
    std::vector<std::string> tags; // 标签：["player", "damageable"]
};
```

### 2.2 渲染组件

```cpp
struct Sprite {
    std::string texture_path;    // 纹理路径（引用 assets/）
    Vec2 pivot = {0.5f, 0.5f};   // 锚点（0~1）
    Color tint = Color::White(); // 着色
    bool flip_x = false;
    bool flip_y = false;
    int anim_frame = 0;          // 当前动画帧（AnimatorSystem 写入）
};

struct Animator {
    int frame_count = 1;         // 总帧数
    int columns = 1;             // 每行帧数（spritesheet 列数）
    float fps = 4.0f;            // 播放速率
    int current_frame = 0;       // 当前帧索引（运行时）
    float timer = 0.0f;          // 帧切换计时器（运行时）
    bool playing = true;
};

struct TileLayer {
    std::string name;
    int width = 0;
    int height = 0;
    std::vector<int> tiles;      // -1 = empty, >=0 = tile index
    bool visible = true;
    bool collision = false;      // editor-only collision overlay
    float edge_thickness = 1.0f; // map border collision thickness
};

struct Tilemap {
    std::string tileset_name;    // 引用 TilesetAsset 名称
    int tile_width = 32;
    int tile_height = 32;
    std::vector<TileLayer> layers; // 多层 Tilemap
};
```

### 2.3 物理组件（架构预留，尚未实现）

```cpp
// 以下组件为 2D 物理/碰撞系统预留，当前引擎未实现：
// struct RigidBody { enum Type { Static, Kinematic, Dynamic }; ... };
// struct BoxCollider { Vec2 size, offset; bool is_trigger; };
// struct CircleCollider { float radius; Vec2 offset; bool is_trigger; };
```

### 2.4 游戏逻辑组件

```cpp
struct Movement {
    // 当前 Movement 组件仅作为标记/数据占位
    float move_speed = 100.0f;   // 像素/秒
};

struct CameraFollowTarget {
    // 标记此 Entity 为相机跟随目标
    // 跟随参数（smooth_follow/follow_speed/dead_zone）已迁移到 Camera 组件
};

struct Camera {
    enum class Projection { Orthographic, Perspective };
    Projection projection = Projection::Orthographic;
    float ortho_size = 5.0f;     // 半高（世界单位）
    float near_plane = -100.0f;
    float far_plane = 100.0f;
    Color clear_color = Color(0.07f, 0.07f, 0.08f);
    float viewport_x = 0, viewport_y = 0, viewport_w = 1, viewport_h = 1;
    bool is_primary = false;
    // 跟随参数
    EntityID follow_target = kInvalidEntity;
    bool smooth_follow = true;
    float follow_speed = 5.0f;
    Vec2 dead_zone = {0, 0};
};
```

### 2.5 脚本组件

```cpp
struct LuaScript {
    std::string script_path;     // Lua 脚本路径
    // 公开到 Inspector 的变量（编辑器可配置）
    std::vector<ScriptVariable> exposed_vars;
};
```

---

## 3. 预设实体类型（Prefab 分类）

> Prefab 不是"类型"，而是**常用 Component 组合的模板**。用户可以任意增删 Component。

| Prefab | 默认 Component 组合 | 说明 |
|--------|---------------------|------|
| **Player** | Transform + Sprite + RigidBody(Dynamic) + BoxCollider + Movement + CameraTarget + LuaScript | 玩家角色 |
| **NPC** | Transform + Sprite + RigidBody(Kinematic) + BoxCollider + LuaScript | 非玩家角色 |
| **Enemy** | Transform + Sprite + RigidBody(Dynamic) + BoxCollider + Actor + LuaScript | 敌人 |
| **Item** | Transform + Sprite + BoxCollider(IsTrigger) + LuaScript | 可拾取物品 |
| **TriggerZone** | Transform + BoxCollider(IsTrigger) + EventTrigger | 触发区域 |
| **TilemapLayer** | Transform + Tilemap | 地图层 |
| **ParallaxBg** | Transform + Sprite + LuaScript | 视差背景 |

---

## 4. 场景 / 地图结构

### 4.1 Map（关卡）

```json
{
    "id": 1,
    "name": "1-1 草原",
    "mode": "2d_sidescroller",
    "width": 40, "height": 20,
    "tile_size": 32,
    "layers": [
        { "type": "tilemap", "name": "地形", "tileset": "grass", "data": [...] },
        { "type": "tilemap", "name": "装饰", "tileset": "decor", "data": [...] },
        { "type": "entity", "name": "实体层", "entities": [...] }
    ],
    "events": [...],
    "camera_bounds": { "min_x": 0, "max_x": 1280, "min_y": 0, "max_y": 640 }
}
```

### 4.2 Scene（非地图场景）

```json
{
    "type": "scene",
    "name": "main_menu",
    "entities": [
        { "prefab": "ui_background", "x": 0, "y": 0 },
        { "prefab": "ui_button", "x": 400, "y": 300, "overrides": { "text": "开始游戏" } }
    ]
}
```

---

## 5. 事件系统（重点）

### 5.1 事件触发方式

| 触发方式 | 说明 | 参数 |
|----------|------|------|
| `on_enter` | 玩家进入触发区域 | 区域范围 (x, y, w, h) |
| `on_exit` | 玩家离开触发区域 | 区域范围 |
| `on_interact` | 玩家按下交互键（默认 Space/E）且面向此对象 | 交互距离 |
| `on_touch` | 任意 Entity（不仅是玩家）触碰此触发器 | 目标 Tag 过滤 |
| `on_start` | 地图加载完成后自动执行一次 | 延迟时间（秒） |
| `on_update` | 每帧检测条件 | 条件表达式 |
| `on_variable_change` | 全局变量变化时 | 变量名、目标值 |
| `on_input` | 按下特定按键 | 按键名 |
| `on_collision` | 物理碰撞发生时 | 目标 Tag |

### 5.2 事件页（Event Page）

RPG Maker 风格：一个事件可以有多个**页**，每页有独立的触发条件和动作列表。只有第一个满足条件的页会被激活。

```json
{
    "id": 1,
    "name": "宝箱",
    "x": 10, "y": 5,
    "pages": [
        {
            "condition": { "variable": "chest_1_opened", "equals": false },
            "trigger": "on_interact",
            "actions": [
                { "cmd": "show_message", "text": "获得 50 金币！" },
                { "cmd": "add_item", "item_id": 1, "count": 50 },
                { "cmd": "set_variable", "name": "chest_1_opened", "value": true },
                { "cmd": "play_animation", "anim_id": "sparkle", "x": 10, "y": 5 },
                { "cmd": "set_self_switch", "switch": "A", "value": true }
            ]
        },
        {
            "condition": { "variable": "chest_1_opened", "equals": true },
            "trigger": "on_interact",
            "actions": [
                { "cmd": "show_message", "text": "宝箱是空的。" }
            ]
        }
    ]
}
```

### 5.3 核心动作命令（内置）

| 命令 | 功能 | 参数示例 | 状态 |
|------|------|----------|------|
| `show_message` | 显示对话窗口 | `text`, `name`, `face` | ✅ |
| `show_toast` | 实体头顶浮动提示 | `text` | ✅ |
| `show_choices` | 显示选项 | `choices`, `variable` | ✅ |
| `transfer_player` | 传送玩家 | `map`, `x`, `y` | ✅ |
| `move_entity` | 移动实体 | `target`, `path`, `speed` | ✅ |
| `play_animation` | 播放动画（重置 Animator） | `text`（目标实体 Name） | ✅ |
| `play_sound` | 播放音效 | `text`（SE 路径） | ✅ |
| `play_bgm` | 切换 BGM | `text`（BGM 路径） | ✅ |
| `stop_bgm` | 停止 BGM | — | ✅ |
| `set_variable` | 设置全局变量 | `target`, `int_value` / `float_value`, `op` | ✅ |
| `set_switch` | 设置全局开关 | `target`, `bool_value` | ✅ |
| `set_self_switch` | 设置事件自身开关 | `target`, `bool_value` | ✅ |
| `add_item` | 添加物品 | `target`（Item ID）, `int_value`（count） | ✅ |
| `remove_item` | 移除物品 | `target`（Item ID）, `int_value`（count） | ✅ |
| `show_ui` / `hide_ui` | 显示/隐藏 HUD 面板 | `text`（ui_name） | ✅ |
| `wait` | 等待 N 秒 | `float_value` | ✅ |
| `if` / `else` / `end_if` | 条件分支 | `target`, `int_value`/`bool_value`，类型 Combo | ✅ |
| `loop` / `break_loop` | 循环 | `int_value`（次数，0=无限） | ✅ |
| `call_script` | 调用 Lua 脚本 | `text`（脚本路径） | ✅（Lua 已接入，经 `ScriptEngine::DoFile` 执行，API 绑定见 Progress.md Phase Y3） |

### 5.4 事件触发流程

```
玩家进入地图
  → 加载所有 Event 实体（每个 Event 是一个带 EventTrigger Component 的 Entity）
  → 每帧检查激活事件页的触发条件
  → 条件满足 → 将该事件页的动作序列加入"事件执行队列"
  → 队列按顺序执行（默认阻塞：一个动作完成后再执行下一个）
  → `wait` / 对话窗口 等会暂停队列，用户交互后恢复
```

---

## 6. 全局状态（变量与开关）

```json
// 运行时全局状态（自动持久化到存档）
{
    "variables": {
        "player_gold": 100,
        "chapter": 1,
        "chest_1_opened": false
    },
    "switches": {
        "main_quest_started": true,
        "boss_defeated": false
    },
    "self_switches": {
        "Map001_1_A": true   // 地图ID_事件ID_开关名
    }
}
```

---

## 7. 制作工作流（如何方便别人制作）

### 7.1 零代码制作（事件系统）

面向不会编程的用户：
1. 在地图上**右键 → 创建事件**
2. 选择触发方式（如 `on_interact`）
3. 从命令列表拖拽动作（显示文字、添加物品、播放动画…）
4. 填参数
5. 添加第二页实现"打开后变空"的状态切换

### 7.2 轻度脚本扩展（Lua）

当事件系统不够用时的过渡方案：

```lua
-- 挂在 Enemy Prefab 上的脚本
local EnemyAI = {}

function EnemyAI:on_init(entity)
    self.start_x = entity.transform.position.x
    self.dir = 1
end

function EnemyAI:on_update(entity, dt)
    local rb = entity.rigidbody
    local pos = entity.transform.position
    
    -- 巡逻逻辑：超出范围后转向
    if pos.x > self.start_x + 100 then self.dir = -1 end
    if pos.x < self.start_x - 100 then self.dir = 1 end
    
    rb.velocity.x = self.dir * 50
    entity.sprite.flip_x = (self.dir < 0)
end

function EnemyAI:on_trigger_enter(entity, other)
    if other:has_tag("player") then
        -- 受伤
        other.actor.hp = other.actor.hp - 10
    end
end

return EnemyAI
```

### 7.3 深度扩展（C++ DLL）

面向需要极致性能或复杂系统的开发者：
- 编写自定义 System（如大型粒子系统、网络同步）
- 编译为 DLL，主程序通过接口加载
- 与 ECS 直接交互， bypass Lua 虚拟机

---

## 8. 自由度保证

| 层级 | 自由度 | 适用人群 |
|------|--------|----------|
| **纯事件** | 中等（预设命令组合） | 完全不会编程的创作者 |
| **Lua 脚本** | 高（访问全部 Component API） | 会基础编程的开发者 |
| **C++ DLL** | 完全（直接操作系统内存和渲染） | 专业开发者 |

**关键设计**：所有内置命令最终都调用同一套 Component API。事件系统能做的，Lua 也能做；Lua 能做的，C++ 也能做。不存在"事件系统能做到但脚本做不到"的断层。

---

> **最后更新**：2026-06-10
> 状态：ECS Component（10 个）、事件系统（22/22 命令）、渲染管线均已实现。物理组件（RigidBody/Collider）为架构预留，尚未落地。
