# Tilemap 系统 — 完整实施方案

> 基于当前代码的实际状态（2026-06-22）。
> 每个特性包含：数据结构变更、算法、代码位置、JSON 序列化影响。

---

## 一、当前代码基线

### 1.1 数据结构

```cpp
// src/scene/components/tilemap.h
struct TileLayer {
    std::string name;
    int width, height;
    std::vector<int> tiles;         // -1 = 空, >=0 = tile index
    std::vector<uint8_t> passages;  // per-tile 4 方向阻挡掩码
    std::vector<uint8_t> terrain;   // per-tile 地形标记 (0 = 无)
    bool visible;                   // 渲染开关
    float edge_thickness;           // 保留字段，当前未使用
};

struct Tilemap : IComponent {
    std::string tileset_name;       // 引用 TilesetAsset::name
    int tile_width, tile_height;
    std::vector<TileLayer> layers;
};

// src/editor/tileset_asset.h
struct TilesetAsset {
    std::string name, image_path;
    int tile_width, tile_height, columns, rows, tile_count;
    float spacing, margin;
    bool auto_tile;                 // 已有字段但无逻辑
    Texture* texture_ptr;
    ImTextureID imgui_ds;
};
```

### 1.2 渲染管线

```
TilemapRenderSystem::Render()
  ├── 绑定 pipeline（共用 Sprite 管线）
  ├── 绑定 UBO（set=0）
  ├── 绑定 Merged VB（set=0 binding=0）+ Dummy instance VB（binding=1）
  ├── 绑定 Tileset 纹理 DS（set=1）
  └── vkCmdDraw(vertex_count, 1)  ← 单次 draw call，所有层合并到一个 VB
```

VB 合并在 `EditorLayer` / `EditorApplication` 中每帧生成：
```
遍历 Tilemap.layers[] → 对每个 visible 层的每个非 -1 tile：
  计算世界坐标 → 计算 UV（tile_index → columns/rows → u,v） → push 6 顶点（2 三角形）
```

### 1.3 碰撞逻辑

在 `EditorLayer::RunPlayModeSystems()` 中内联：
```
遍历所有 Tilemap → 收集 collision=true 的层的所有非 -1 tile → 生成 Rect 列表
Player 移动时 → X/Y 轴分离 AABB 检测 → 遇 Rect 则阻止
```

---

## 二、P0：每 Tile 独立碰撞

### 2.1 数据结构变更

`TileLayer` 增加 per-tile 通行数据：

```cpp
// src/scene/components/tilemap.h
struct TileLayer {
    // ... 现有字段保持不变 ...

    // 新增：每 tile 阻挡掩码。用 uint8_t 位掩码，bit 置 1 表示该方向不可通行。
    // 位定义：
    //   bit0 (1):  上方向不可通行
    //   bit1 (2):  左方向不可通行
    //   bit2 (4):  右方向不可通行
    //   bit3 (8):  下方向不可通行
    // 特殊值：
    //   0x0F (15): 四方向全阻挡（墙）
    //   0x00 (0):  全通（地面）
    // 旧版 collision=true 的层在加载时会自动转换为非空 tile 的 passages=0x0F 并隐藏。
    std::vector<uint8_t> passages;
};
```

**为什么用方向位而不是简单 bool**：RPG 中"柜台"场景——玩家可以从下方穿过柜台上半部分但不能从上方走下来。四方向位可以精确描述。如果只需要二元碰撞，把所有位设 1 即可。

### 2.2 JSON 序列化

```json
// 旧格式（兼容）
{"name":"Ground","width":20,"height":15,"tiles":[...],"visible":true,"collision":false}

// 新格式
{"name":"Ground","width":20,"height":15,"tiles":[...],"visible":true,
 "collision":false,"passages":[0,0,15,0,0,15,...]}  // 300 个 uint8
```

`to_json` / `from_json` 逻辑：

```cpp
// 序列化：如果 passages_ 非空才写，节省旧数据空间
if (!l.passages.empty()) {
    // passages 存为 base64 字符串或 int 数组
    j["passages"] = l.passages;  // nlohmann::json 原生支持 vector<uint8_t> → 数字数组
}

// 反序列化：如果没有 passages 字段，passages_ 保持空（回退到旧行为）
if (j.contains("passages")) {
    l.passages = j["passages"].get<std::vector<uint8_t>>();
}
```

### 2.3 碰撞检测算法

**当前算法**（`editor_layer.cpp:289-343`）：
```
对每层每个 tile 生成一个 AABB Rect → player 做 AABB 检测
```
问题：遍历全部 tile 生成 Rect 是 O(W×H)，20×15=300 还好，100×100=10000 就慢了。

**新算法**：不用预生成 Rect 列表，直接在移动时查 tile：

```cpp
// 替换 editor_layer.cpp 中 collision_rects 收集 + AABB 检测的部分

/// 检查世界坐标 (wx, wy) 处是否有碰撞
static bool IsBlockedAt(const Tilemap& tm, float wx, float wy,
                        float from_dx, float from_dy,  // 移动方向（用于方向位判断）
                        const std::vector<EntityID>& all_entities, World* world) {

    // 1. 边界检查——超出地图边界不可通行
    if (wx < 0 || wy < 0) return true;
    int tx = (int)(wx / tm.tile_width);
    int ty = (int)(wy / tm.tile_height);

    // 2. 对每个 Tilemap 实体检查
    for (auto e : all_entities) {
        auto* map = world->GetComponent<Tilemap>(e);
        if (!map) continue;
        auto* t = world->GetComponent<Transform>(e);
        float ox = t ? t->position.x : 0.0f;
        float oy = t ? t->position.y : 0.0f;

        int local_tx = tx - (int)(ox / map->tile_width);
        int local_ty = ty - (int)(oy / map->tile_height);

        for (auto& layer : map->layers) {
            if (layer.GetTile(local_tx, local_ty) < 0) continue;  // 空格子 — 可通

            if (!layer.passages.empty()) {
                // 新行为：逐 tile 方向位
                uint8_t pass = layer.passages[local_ty * layer.width + local_tx];
                if (pass == 0) continue;  // 全通

                // 判断从哪个方向进入这个 tile
                // from_dx > 0 → 从左边来（向右移动）→ 检查 bit2（右方向阻挡）
                if (from_dx > 0 && (pass & 4)) return true;
                if (from_dx < 0 && (pass & 2)) return true;
                if (from_dy > 0 && (pass & 1)) return true;
                if (from_dy < 0 && (pass & 8)) return true;

                // 特殊：如果非零但无方向匹配（如恰好站在格子上），检查四方向是否全挡
                if (from_dx == 0 && from_dy == 0 && pass == 0x0F) return true;
            } else if (layer.collision) {
                // 旧行为：整层碰撞
                return true;
            }
        }
    }
    return false;
}
```

**移动检测改造**（替换当前 editor_layer.cpp:388-398）：

```cpp
// X 轴检测
float new_x = t->position.x + dx;
// 检查玩家矩形四个角是否都在可通过区域
float half = 14.0f;
bool blocked_x = IsBlockedAt(tm, new_x - half, t->position.y - half, dx, 0, ...)
              || IsBlockedAt(tm, new_x + half, t->position.y - half, dx, 0, ...)
              || IsBlockedAt(tm, new_x - half, t->position.y + half, dx, 0, ...)
              || IsBlockedAt(tm, new_x + half, t->position.y + half, dx, 0, ...);
if (!blocked_x) t->position.x = new_x;

// Y 轴同理
```

### 2.4 编辑器 UI

`TilePalettePanel` 绘制时增加碰撞编辑模式：
- 工具栏加 `[Passage]` 按钮，切换通行编辑模式
- 在 Viewport 中右键已放置的 tile → 弹出通行方向选择器（上下左右四个箭头按钮）
- 或在 TilePalette 中加"碰撞画笔"：选中一个通行模式（全挡/全通/定向），像画 tile 一样画碰撞

---

## 三、P0：Autotile 自动拼接

### 3.1 算法原理（Bitmask / Wang Tile）

RPG Maker 使用的标准 autotile 算法：

每个 autotile 在 tileset 中占据一个 **2×3 或 3×4 的子网格**（不同规格），但在我们的简化实现中，使用 **半 tile 拼接**：

```
一个 autotile 区域在 tileset 中排布如下（48 个半 tile 排列）：

  0  1  2  3     ← 四个 1/4 角
  4  5  6  7     ← 四个边
  ...

简化版（16 tiles 变体，RPG Maker MV 标准）：

  tiles[0..15] 在 tileset 中连续排列：
  索引对应 bitmask(0..15)，每个 4 位表示四邻接情况

  bit0 (1): 上邻接同类
  bit1 (2): 右邻接同类
  bit2 (4): 下邻接同类
  bit3 (8): 左邻接同类

  bitmask → tile 映射表（RPG Maker MV 标准）：
    0  (0000): 孤立 — tile 15 (四面无同类)
    1  (0001): 上    — tile 12
    2  (0010): 右    — tile 3
    3  (0011): 上右  — tile 9
    4  (0100): 下    — tile 6
    5  (0101): 上下  — tile 10 (竖管)
    6  (0110): 右下  — tile 5
    7  (0111): 上右下 — tile 7
    8  (1000): 左    — tile 0
    9  (1001): 上左  — tile 1
    10 (1010): 左右  — tile 2 (横管)
    11 (1011): 上左右 — tile 4
    12 (1100): 下左  — tile 13
    13 (1101): 上下左 — tile 8
    14 (1110): 右左下 — tile 11
    15 (1111): 全方向 — tile 14 (中心 tile)
```

### 3.2 TilesetAsset 扩展

```cpp
// src/editor/tileset_asset.h
struct TilesetAsset {
    // ... 现有字段 ...

    // autotile 配置（仅当 auto_tile = true 时生效）
    struct AutotileGroup {
        int base_tile_index;      // 起始 tile 索引（在 tileset 中的位置）
        std::string type;         // "water", "grass_edge", "path" 等（供 UI 显示）
    };
    std::vector<AutotileGroup> autotile_groups;
    // 只有 auto_tile = true 时才填充
};
```

`data/tilesets.json` 扩展：

```json
{
    "name": "Overworld",
    "auto_tile": true,
    "autotile_groups": [
        { "base_tile_index": 48,  "type": "water" },
        { "base_tile_index": 64,  "type": "grass_edge" }
    ]
}
```

### 3.3 渲染时自动计算

在 VB 合并阶段，对标记为 autotile 的 tile 动态计算实际显示的 tile index：

```cpp
// 在 tilemap VB 合并循环中（EditorLayer / GameApplication）

int ResolveAutotileIndex(const Tilemap& tm, const TileLayer& layer,
                          int tx, int ty, int base_index) {
    // 计算四邻接 bitmask
    int mask = 0;
    auto is_same = [&](int nx, int ny) {
        // 检查相邻 tile 是否属于同一个 autotile 组
        int neighbor = layer.GetTile(nx, ny);
        return neighbor >= base_index && neighbor < base_index + 16;
    };

    if (ty > 0 && is_same(tx, ty - 1))            mask |= 1;  // 上
    if (tx < layer.width - 1 && is_same(tx + 1, ty)) mask |= 2;  // 右
    if (ty < layer.height - 1 && is_same(tx, ty + 1)) mask |= 4;  // 下
    if (tx > 0 && is_same(tx - 1, ty))            mask |= 8;  // 左

    // bitmask → tile offset（RPG Maker MV 标准映射表）
    static const int OFFSET[16] = {
        15, 12, 3, 9, 6, 10, 5, 7,
        0,  1,  2, 4, 13, 8, 11, 14
    };
    return base_index + OFFSET[mask];
}

// 在 VB 生成时：
int display_tile = tile_index;
if (tileset.auto_tile) {
    for (auto& group : tileset.autotile_groups) {
        if (tile_index >= group.base_tile_index &&
            tile_index < group.base_tile_index + 16) {
            display_tile = ResolveAutotileIndex(tm, layer, tx, ty,
                                                 group.base_tile_index);
            break;
        }
    }
}
```

**注意**：这个计算在 VB 生成时做（CPU 端），不在 shader 里做。因为相邻 tile 信息只有 CPU 知道，shader 没有全局 tile 数据的访问。

### 3.4 编辑器如何画 autotile

- 用户在 TilePalette 中选中 autotile 组中的任意一个 tile
- 在 Viewport 中绘制时，实际写入的是 `base_tile_index`（不是偏移后的）
- VB 生成时通过 `ResolveAutotileIndex()` 计算实际显示 tile
- 当用户修改地图时，相邻 tile 的 autotile 也会自动更新（因为 neighbor 变了）

**刷新相邻 tile 的逻辑**：在 `SetTile()` 后触发：

```cpp
void Tilemap::SetTile(int layer, int x, int y, int tile_index) {
    layers[layer].SetTile(x, y, tile_index);
    // 如果是 autotile，标记周围 tile 为脏（下次 VB 重新生成）
    MarkNeighborsDirty(layer, x, y);
}
```

---

## 四、P1：地形标记

### 4.1 数据

```cpp
struct TileLayer {
    // ... 现有字段 ...
    std::vector<uint8_t> passages;   // 碰撞（P0）
    std::vector<uint8_t> terrain;    // 地形标记 0-255
    // 0 = 普通
    // 1 = 草地（遇敌）
    // 2 = 毒沼（扣血）
    // 3 = 冰面（滑行）
    // 4-7 = 自定义
};
```

编辑器在 TilePalette 中增加 `[Region]` 画笔模式，选 0-7 中的一个刷到地图上。

### 4.2 运行时使用

```cpp
// MovementSystem / EventSystem 读取
uint8_t region = GetTileRegion(world, player_x, player_y);
switch (region) {
    case 1: // 草地 — 触发随机遇敌
        if (rand() % 100 < encounter_rate) StartBattle();
        break;
    case 2: // 毒沼 — 每步扣血
        player_hp -= 1;
        break;
    case 3: // 冰面 — 滑行（松开方向键后继续滑动到障碍物）
        ApplyIceSliding(player, dx, dy);
        break;
}
```

---

## 五、瓦片动画

### 5.1 TilesetAsset 扩展

```cpp
struct TilesetAsset {
    struct TileAnim {
        int tile_index;              // 哪个 tile 有动画
        std::vector<int> frames;     // 动画帧序列（tile index 数组）
        float fps;
    };
    std::vector<TileAnim> animations;
};
```

### 5.2 渲染时替换

在 VB 生成时：

```cpp
// animation_timer 由 TilemapRenderSystem 维护（或全局时间）
float anim_time = timer.GetTotalTime();

int ResolveAnimatedTile(int tile_index, const TilesetAsset& ts, float time) {
    for (auto& anim : ts.animations) {
        if (tile_index == anim.tile_index) {
            int frame = (int)(time * anim.fps) % anim.frames.size();
            return anim.frames[frame];
        }
    }
    return tile_index;
}
```

**关键**：动画会导致 VB 每帧变化，需要每帧重建 VB。但如果只有少量动画 tile，可以在 shader 中用 UBO 传入 offset，避免全量重建。MVP 阶段用 CPU 替换即可。

---

## 六、多 Tileset 支持

### 6.1 数据

```cpp
struct Tilemap : IComponent {
    // 改为数组
    std::string tileset_name;               // 兼容旧数据（主 tileset）
    std::vector<std::string> tileset_names;  // 新：多 tileset 引用
    // ...
};
```

tile index 范围和 tileset 的对应关系：

```
tile 0 ~ 99       → tileset A（主 tileset，100 个 tile）
tile 100 ~ 199    → tileset B（100 个 tile）
tile 200 ~ 229    → tileset C（30 个 tile）
```

在 tileset 注册表中额外存每个 tileset 的全局起始索引。

### 6.2 渲染

VB 合并时按 tileset 分组，不同 tileset 需要切换纹理 DS：

```
for each tileset:
    bind descriptor set (set=1) for this tileset's texture
    对使用该 tileset 的所有 tile 生成 VB → draw
```

性能上从 1 draw call 变成 N draw calls，但 tileset 数量通常 ≤ 5，影响可忽略。

---

## 七、实施顺序

| 步骤 | 内容 | 状态 | 依赖 |
|------|------|------|------|
| 1 | Per-tile 碰撞数据 + 碰撞检测算法 | ✅ 已完成 | 无 |
| 2 | Autotile bitmask 算法 + 编辑器集成 | ✅ 已有 MVP 实现 | 无 |
| 3 | 瓦片动画 | ✅ 已完成（2026-09-07 MVP：CPU 帧替换 + 帧哈希驱动 VB 重建） | 无 |
| 4 | 地形标记 | ✅ 已完成 | 无 |
| 5 | 多 Tileset | ✅ 已完成 | 步骤 1-4 后 |

**建议阶段五完成后，阶段六按此顺序推进 1+2（P0），3+4+5 按需推进。**
