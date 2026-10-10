#include "editor_context.h"
#include "panel.h"
#include "core/debug_log.h"
#include "scene/scene_serializer.h"
#include "scene/components/transform.h"
#include "scene/components/name.h"
#include "scene/components/sprite.h"
#include "scene/components/tilemap.h"
#include "scene/components/movement.h"
#include "resource/resource_manager.h"
#include "resource/data_manager.h"
#include "scene/components/camera.h"
#include "scene/components/camera_follow.h"
#include "scene/components/event_trigger.h"
#include "backends/imgui_impl_vulkan.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

namespace mg {
namespace editor {

EditorContext::EditorContext() {
    // F8：SceneSession 持有场景状态与逻辑，协作者为本对象内的长生命周期成员
    session_.Bind(&command_history_, &selection_, &viewport_, &tileset_, &project_);
    // 去单例化：命令历史注入选区管理器（Undo/Redo 后同步选区）
    command_history_.Bind(&selection_);
}

// 选区系统已移至 editor/selection_manager.h（F3 拆分），方法为内联转发
// 活动场景 / 场景持久化 / 多地图管理已移至 editor/scene_session.h/.cpp（F8 拆分）

// Play Mode 快照：委托 PlayModeController（F4 拆分），这里只做场景取参
void EditorContext::TakeSnapshot() {
    auto* scene = GetActiveScene();
    play_mode_.TakeSnapshot(scene ? scene->GetWorld() : nullptr);
}

void EditorContext::RestoreSnapshot() {
    auto* scene = GetActiveScene();
    play_mode_.RestoreSnapshot(scene ? scene->GetWorld() : nullptr);
}

// 视口相机控制 / 主相机管理 / Gizmo / 视图模式已移至 editor/viewport_state.h/.cpp（F5 拆分），
// EditorContext 头文件中以 ActiveWorld() 取参内联转发。

// 实体模板创建与 CreateDefaultScene 已移至 SceneSession（F8 拆分），头文件内联转发

// 切换播放状态（Edit / Playing / Paused），状态变化时触发回调
// 状态存于 PlayModeController（F4），此处保留编排：清 Undo 历史 + 回调
void EditorContext::SetPlayState(PlayState state) {
    PlayState old_state = play_mode_.GetState();
    if (old_state != state) {
        bool was_edit = (old_state == PlayState::Edit);
        bool is_edit = (state == PlayState::Edit);
        play_mode_.SetState(state);
        // Pause/Resume 不清空 Undo 栈，只有 Edit <-> Playing 切换才清空
        if (was_edit || is_edit) {
            command_history_.Clear();
        }
    }
}

// Gizmo/视图模式转发见头文件（F5 拆分）
// Undo/Redo 与 Entity 命令已移至 editor/command_history.h/.cpp（F1 拆分）
// Tileset / Tilemap 编辑状态已移至 editor/tileset_editor.h/.cpp（F6 拆分），头文件内联转发

// ------------------------------------------------------------------
// 项目系统
// ------------------------------------------------------------------

bool EditorContext::OpenProject(const std::string& project_path) {
    EDITOR_LOG("EditorContext::OpenProject enter path=" << project_path);
    namespace fs = std::filesystem;
    if (!fs::exists(project_path + "/project.json")) {
        EDITOR_LOG("EditorContext::OpenProject project.json not found, returning false");
        return false;
    }

    auto manifest = DataManager::Get().Load<ProjectManifest>(project_path + "/project.json");
    if (!manifest) {
        EDITOR_LOG("EditorContext::OpenProject manifest load failed, returning false");
        return false;
    }

    project_.SetProjectPath(project_path);
    project_.SetManifest(manifest);

    // 确保 ResourceManager 后续加载纹理时能以项目根目录解析相对路径
    EDITOR_LOG("EditorContext::OpenProject SetProjectPath");
    ResourceManager::Get().SetProjectPath(project_.GetProjectPath());

    // 项目路径此刻才确定：重载 tileset（OnInit 时路径为空，只能回退 Demo tileset）。
    // 必须在 LoadScene 之前——FixupActiveTilemapTileset 与 RebuildTilemapVB 依赖注册表。
    if (tileset_renderer_) {
        tileset_.UnloadAllTilesets();
        if (!tileset_.LoadFromProject(project_.GetProjectPath(), tileset_renderer_)) {
            tileset_.LoadDemoTileset(tileset_renderer_);
        }
    }

    // 加载全部游戏数据库（物品/角色/技能/敌人/商店）
    databases_.LoadFromDirectory(project_.GetProjectPath() + "/data");

    // 加载 start_scene（maps/Map001.json 等）
    EDITOR_LOG("EditorContext::OpenProject loading start_scene=" << (project_.GetManifest() ? project_.GetManifest()->start_scene : "(no manifest)"));
    if (!project_.GetManifest()->start_scene.empty()) {
        std::string scene_path = project_.GetProjectPath() + "/" + project_.GetManifest()->start_scene;
        if (fs::exists(scene_path)) {
            if (LoadScene(scene_path)) {
                EDITOR_LOG("EditorContext::OpenProject start_scene loaded");
                AddRecentProject(project_path);
                EDITOR_LOG("EditorContext::OpenProject return true (start_scene path)");
                return true;
            }
        }
    }

    // 无 start_scene 或加载失败：创建默认地图并保存
    CreateDefaultScene();
    if (!project_.GetManifest()->start_scene.empty()) {
        SaveScene(project_.GetProjectPath() + "/" + project_.GetManifest()->start_scene);
    } else {
        fs::create_directories(project_.GetProjectPath() + "/maps");
        SaveScene(project_.GetProjectPath() + "/maps/Map001.json");
        ProjectManifest updated = *project_.GetManifest();
        updated.start_scene = "maps/Map001.json";
        project_.SetManifest(std::make_shared<const ProjectManifest>(updated));
        DataManager::Get().Save(project_.GetProjectPath() + "/project.json", updated);
    }
    project_.RebuildMapInfos();
    AddRecentProject(project_path);
    EDITOR_LOG("EditorContext::OpenProject return true (default scene path)");
    return true;
}

// NewProject 已移至 EditorProjectManager（F7 拆分），头文件内联转发

// GetStartScenePath / EnsurePlayer / FixupActiveTilemapTileset 已移至 SceneSession（F8 拆分）

// ReadSaveSlotInfo 已移至 PlayModeController（F4 拆分），头文件内联转发

bool EditorContext::SaveProject() {
    if (project_.GetProjectPath().empty() || !project_.GetManifest()) return false;
    // 保存当前场景
    if (!GetScenePath().empty()) {
        SaveScene(GetScenePath());
    } else if (!project_.GetManifest()->start_scene.empty()) {
        SaveScene(project_.GetProjectPath() + "/" + project_.GetManifest()->start_scene);
    }
    // 更新项目清单（可选：同步修改后的字段）
    DataManager::Get().Save(project_.GetProjectPath() + "/project.json", *project_.GetManifest());
    // 保存物品数据库
    DataManager::Get().Save(project_.GetProjectPath() + "/data/items.json", databases_.item);
    DataManager::Get().Save(project_.GetProjectPath() + "/data/actors.json", databases_.actor);
    DataManager::Get().Save(project_.GetProjectPath() + "/data/skills.json", databases_.skill);
    DataManager::Get().Save(project_.GetProjectPath() + "/data/enemies.json", databases_.enemy);
    DataManager::Get().Save(project_.GetProjectPath() + "/data/shops.json", databases_.shop);
    RebuildMapInfos();
    return true;
}

// LoadScene / SaveScene / 多地图管理已移至 SceneSession（F8 拆分），头文件内联转发

// RebuildMapInfos / LoadMapInfos 已移至 EditorProjectManager（F7 拆分），头文件内联转发

// SwitchScene / CreateAndSaveNewScene / RenameScene / DeleteScene 已移至 SceneSession（F8 拆分）

// CreateAndSaveNewScene 已移至 SceneSession（F8 拆分）

// RenameScene 已移至 SceneSession（F8 拆分）

// DeleteScene 已移至 SceneSession（F8 拆分）

// 最近打开项目列表已移至 EditorProjectManager（F7 拆分），头文件内联转发

// tilesets.json 持久化与 Demo tileset 已移至 TilesetEditor（F6 拆分）

void EditorContext::OpenDetailsPanel() {
    if (!panel_manager_) return;
    if (auto* panel = panel_manager_->FindPanel("Scene Explorer")) panel->SetOpen(true);
}

void EditorContext::OpenEventEditorPanel() {
    if (!panel_manager_) return;
    if (auto* panel = panel_manager_->FindPanel("Event Editor")) panel->SetOpen(true);
}

void EditorContext::OpenTilePalettePanel() {
    if (!panel_manager_) return;
    if (auto* panel = panel_manager_->FindPanel("Tile Palette")) panel->SetOpen(true);
}

void EditorContext::OpenScriptEditorPanel() {
    if (!panel_manager_) return;
    if (auto* panel = panel_manager_->FindPanel("Script Editor")) panel->SetOpen(true);
}

} // namespace editor
} // namespace mg
