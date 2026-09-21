# 阶段五：导出系统 — 详细方案 v2

> 2026-06-22。阶段一～四已完成。
> 经评审调整：修正 DLL 收集、依赖解析、链接依赖、代码组织等问题。

---

## 1. Runtime 剥离 —— 拆库

### 1.1 目标结构

```
engine_runtime (纯运行时，零 ImGui 依赖)        engine_editor (编辑器/Launcher 共用)
  ├── core/                                     ├── editor/imgui/mg_imgui_theme.cpp
  ├── renderer/                                 ├── editor/imgui/mg_imgui_extensions.cpp
  ├── scene/                                    ├── editor/imgui/mg_imgui_panel.cpp
  ├── audio/                                    ├── editor/imgui/mg_imgui_backend.cpp
  ├── script/                                   └── editor/file_dialog.cpp
  ├── resource/
  ├── platform/
  └── app/

editor.exe ← engine_runtime + engine_editor + editor/
launcher.exe ← engine_runtime + engine_editor + launcher/
game.exe ← engine_runtime + src/game_main.cpp
```

### 1.2 关键修正

#### a) `app/application.cpp` 去除 editor 依赖

```cpp
// 删掉: #include "src/editor/debug_log.h"
// 所有 EDITOR_LOG(...) 替换为 MG_LOG_INFO / MG_LOG_ERROR
// 注意：EDITOR_LOG 宏定义在 debug_log.h 的 mg::editor 命名空间内，
// 直接删 include 后这些宏调用会编译失败，必须逐行替换。
```

#### b) `engine_editor` 链接补齐

```cmake
# engine_editor — mg_imgui_backend.cpp 依赖 Vulkan + SDL2
add_library(engine_editor STATIC
    editor/imgui/mg_imgui_theme.cpp
    editor/imgui/mg_imgui_extensions.cpp
    editor/imgui/mg_imgui_panel.cpp
    editor/imgui/mg_imgui_backend.cpp
    editor/file_dialog.cpp
)
target_link_libraries(engine_editor PUBLIC
    engine_runtime
    imgui
    freetype
    ${MG_LINK_LIBS}   # ← 包含 Vulkan + SDL2，避免符号解析问题
)
```

#### c) game.exe 不依赖 LayerStack

`GameApplication` 直接继承 `Application`，只重写 4 个虚函数，不需要 PushLayer/PushOverlay：

```cpp
class GameApplication : public mg::Application {
    void OnInit() override;       // Vulkan 初始化 + 加载场景
    void OnShutdown() override;   // 清理
    void BeginFrame() override;   // acquire swapchain image
    void EndFrame() override;     // present
    bool OnProcessEvent(SDL_Event*) override;  // ESC 退出
    // 主循环在 Run() 中，子类在 OnUpdate 虚函数（需新增）中驱动 ECS
};
```

> **注意**：当前 `Application::Run()` 驱动的是 Layer 栈（UpdateLayers/RenderLayers）。GameApplication 不走 Layer，
> 需要 `Application` 增加一个 `virtual void OnUpdate(float dt) {}` 扩展点，在 Run() 的帧循环中调用。

---

## 2. 资源打包

### 2.1 入口：`ExportGame()` 放在独立文件

按建议放在 `src/editor/export_game.cpp` / `export_game.h`，不往 `EditorContext` 里塞：

```cpp
// export_game.h
namespace mg { namespace editor {
    bool ExportGame(const EditorContext& ctx, const std::string& output_dir);
}}
```

### 2.2 依赖收集：用 SceneSerializer，不手写 JSON

```cpp
// 对 MapInfos 中每个场景：
auto scene = scene::SceneSerializer::LoadFromFile("maps/Village.json");
auto* world = scene->GetWorld();

// 遍历所有实体收集真实依赖
std::vector<EntityID> all;
world->GetAllEntities(all);
for (auto e : all) {
    // Sprite → texture_path
    if (auto* spr = world->GetComponent<Sprite>(e))
        textures.insert(spr->texture_path);

    // Animator → spritesheet（和 Sprite 共用 texture_path，已覆盖）

    // Tilemap → tileset image
    if (auto* tm = world->GetComponent<Tilemap>(e)) {
        auto* ts = ctx.GetTilesetRegistry().find(tm->tileset_name);
        if (ts) textures.insert(ts->image_path);
    }

    // EventTrigger → PlaySound/PlayBGM → wav path（从 actions 数组中提取）
    if (auto* ev = world->GetComponent<EventTrigger>(e)) {
        for (auto& page : ev->pages)
            for (auto& action : page.actions)
                if (action.type == "PlaySound" || action.type == "PlayBGM")
                    audio_files.insert(action.params.get("path", ""));
    }
}

// 收集 scripts
for (auto e : all) {
    if (auto* ev = world->GetComponent<EventTrigger>(e))
        for (auto& page : ev->pages)
            for (auto& action : page.actions)
                if (action.type == "CallScript")
                    scripts.insert(action.params.get("script", ""));
}
```

### 2.3 DLL 收集：从 editor.exe 所在目录自动复制

```cpp
// 工具函数
static std::vector<std::string> CollectRuntimeDlls() {
    // GetModuleFileNameW(nullptr, ...) 获取当前 editor.exe 完整路径
    // 从同目录复制以下文件到输出目录：
    return {
        "SDL2.dll",
        "vulkan-1.dll",
        // MinGW 动态链接（如果是静态链接则不需要）：
        // "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll"
    };
}

// ExportGame 中：
auto exe_dir = fs::path(GetModuleFileNameExeDir());  // editor.exe 所在目录
auto dlls = CollectRuntimeDlls();
for (auto& dll : dlls) {
    fs::copy_file(exe_dir / dll, output_dir / dll, fs::copy_options::overwrite_existing);
}
// 复制 game.exe 自身
fs::copy_file(exe_dir / "game.exe", output_dir / "game.exe", fs::copy_options::overwrite_existing);
```

> 如果 MinGW 是静态链接（CMakeLists.txt 里 `-static-libgcc -static-libstdc++`），无需复制 MinGW runtime DLL。

### 2.4 输出结构

```
export/MyGame/
  ├── game.exe
  ├── SDL2.dll
  ├── vulkan-1.dll
  ├── assets/
  │   ├── shaders/       ← sprite.vert.spv, sprite.frag.spv
  │   ├── textures/      ← 实际引用的 png
  │   └── audio/         ← 实际引用的 wav
  ├── data/
  │   ├── project.json   ← 精简版
  │   ├── maps/          ← *.json 场景文件
  │   └── *.json         ← databases
  └── scripts/           ← *.lua
```

---

## 3. game.exe 入口

### `src/game_main.cpp` (~100 行)

```cpp
// 不依赖 Layer/LayerStack，Application 直接驱动 ECS 主循环

class GameApplication : public mg::Application {
    // Vulkan 资源
    mg::renderer::RendererContext renderer_;
    mg::renderer::SwapChain swapchain_;

    // ECS
    std::unique_ptr<mg::scene::Scene> scene_;
    mg::scene::MovementSystem movement_;
    mg::scene::CameraFollowSystem camera_follow_;
    mg::scene::AnimatorSystem animator_;
    mg::scene::SpriteRenderSystem sprite_render_;
    // EventSystem, TilemapRenderSystem ...

    void OnInit() override {
        // 1. Vulkan Instance/Device/Surface
        // 2. SwapChain
        // 3. Shader → Pipeline (复用 editor_application.cpp 的 pipeline 创建逻辑)
        // 4. ResourceManager::Initialize
        // 5. AudioManager::Init
        // 6. 读 project.json → 加载 start_scene
    }

    void BeginFrame() override { swapchain_.AcquireNextImage(); }
    void EndFrame() override { swapchain_.Present(); }

    void OnUpdate(float dt) override {
        // 驱动 ECS：Movement → CameraFollow → Animator → EventSystem → Render
        auto* world = scene_->GetWorld();
        movement_.Update(dt, world);
        camera_follow_.Update(dt, world);
        animator_.Update(dt, world);
        event_system_.Update(dt);
        // Render 在 OnRender 中
    }

    void OnRender() override {
        auto cmd = swapchain_.GetCurrentCommandBuffer();
        // BeginRenderPass → SpriteRender → TilemapRender → EndRenderPass
    }

    bool OnProcessEvent(SDL_Event* e) override {
        if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE) { Close(); return true; }
        return false;
    }
};

int main(int, char**) {
    GameApplication app({"MyGame", 1280, 720});
    app.Run();
    return 0;
}
```

> **需要给 `Application` 基类加 `OnUpdate(float dt)` 和 `OnRender()` 虚函数**——目前 Run() 只驱动 Layer 栈。
> 改动很小：在 Run() 的帧循环中，如果 Layer 栈为空，调用 `OnUpdate(timer_.GetDeltaTime())` + `OnRender()`。

---

## 4. 改动清单

| 文件 | 改动 | 行数 |
|------|------|------|
| `src/app/application.h` | 加 `OnUpdate(float dt)` `OnRender()` 虚函数 | +5 |
| `src/app/application.cpp` | Run() 中：Layer 栈为空时调 OnUpdate/OnRender；删 `#include debug_log.h`，EDITOR_LOG → MG_LOG | ~10 |
| `src/CMakeLists.txt` | 拆 `engine_core` → `engine_runtime` + `engine_editor`；加 `game` target | ~40 |
| `src/game_main.cpp` | **新建** GameApplication + main | ~100 |
| `src/editor/export_game.h` | **新建** `ExportGame()` 声明 | ~8 |
| `src/editor/export_game.cpp` | **新建** `ExportGame()` 实现（收集依赖 + 复制） | ~80 |
| `src/editor/editor_menu_bar.cpp` | File → Export Game 菜单 | ~30 |
| `editor/CMakeLists.txt` | 更新 link 依赖 | ~5 |
| `launcher/CMakeLists.txt` | 更新 link 依赖 | ~5 |
| `docs/QualityOverhaul.md` | 展开阶段五 | ~20 |

**总计 ~300 行（3 个新文件，其余改已有文件）。**

---

## 5. 顺带清理

| 编号 | 内容 | 处理 |
|------|------|------|
| TD-1 | launcher/editor Vulkan+ImGui 初始化重复 | 拆库后共用代码在 `engine_editor` |
| — | `application.cpp` 的 EDITOR_LOG | 替换为 MG_LOG，解除 editor 依赖 |
| — | `Application::Run()` 只驱动 Layer | 加 OnUpdate/OnRender 扩展点给 game.exe |

---

## 6. 延后项目

| 内容 | 原因 |
|------|------|
| .pak / .zip 资源包 | MVP 阶段复制文件即可，打包格式是阶段六的事 |
| Sprite 批处理 (TD-N) | 当前实体数不构成瓶颈 |
| 碰撞合并 (TD-P) | 同上 |
| game.exe 的 Layer 系统 | 不需要，GameApplication 直接在主循环驱动 ECS |
