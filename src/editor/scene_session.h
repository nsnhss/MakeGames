#pragma once
// SceneSession：场景会话（F8 自 EditorContext 拆分）
// 职责：活动场景所有权、场景文件路径与脏标记、场景加载/保存、
//       多地图切换/新建/重命名/删除、实体模板创建、播放模式场景辅助。
// 跨组件协作者（命令历史/选区/视口/Tileset/项目）由 Bind() 注入，
// 这些对象均为 EditorContext 内的长生命周期成员，指针始终有效。

#include <functional>
#include <memory>
#include <string>
#include "core/math/vec2.h"
#include "resource/schema/project_schema.h"
#include "scene/scene.h"
#include "scene/entity.h"

namespace mg {
namespace editor {

class CommandHistory;
class SelectionManager;
class ViewportState;
class TilesetEditor;
class EditorProjectManager;

class SceneSession {
public:
    // 注入协作者（EditorContext 构造时调用一次）
    void Bind(CommandHistory* history, SelectionManager* selection,
              ViewportState* viewport, TilesetEditor* tileset,
              EditorProjectManager* project);

    // ---- 活动场景 ----
    void SetActiveScene(scene::Scene* scene);   // 接管所有权
    scene::Scene* GetActiveScene() const { return active_scene_.get(); }
    scene::World* GetWorld() const;

    // 创建空场景（最小骨架：Tilemap + Camera + Player），无硬编码内容
    void CreateDefaultScene(int w = kDefaultMapWidth, int h = kDefaultMapHeight, int tile_size = kDefaultTileSize);

    // ---- 实体模板（Create 菜单用）----
    enum class EntityTemplate {
        Empty,
        NPC,
        Monster,
        Chest,
        Transfer,
        Shop,
        Decoration
    };
    scene::EntityID CreateEntityFromTemplate(EntityTemplate type, const math::Vec2& position);
    scene::EntityID CreateEntityFromTemplateAtCameraCenter(EntityTemplate type);

    // ---- 场景持久化 ----
    void MarkSceneDirty() { scene_dirty_ = true; }
    void ClearSceneDirty() { scene_dirty_ = false; }
    bool IsSceneDirty() const { return scene_dirty_; }
    void SetScenePath(const std::string& path) { scene_file_path_ = path; }
    const std::string& GetScenePath() const { return scene_file_path_; }

    // 保存当前场景（若未指定路径则自动落到 project/scenes/Untitled.scene.json）
    bool SaveCurrentScene();
    bool LoadScene(const std::string& path);   // 加载地图/场景文件
    bool SaveScene(const std::string& path);   // 保存当前场景到指定路径

    // ---- 多地图管理 ----
    bool SwitchScene(const std::string& map_name);          // 保存当前场景并切换到目标场景
    bool CreateAndSaveNewScene(const std::string& name, int w = kDefaultMapWidth, int h = kDefaultMapHeight, int tile_size = kDefaultTileSize);    // 创建并保存新场景，失败时回滚
    bool RenameScene(const std::string& old_name, const std::string& new_name);
    bool DeleteScene(const std::string& name);

    // ---- 播放模式辅助 ----
    std::string GetStartScenePath() const;
    void EnsurePlayer();
    void FixupActiveTilemapTileset();

    // 命令历史（Details 面板组件编辑/增删走 Undo 栈）；未 Bind 时为 nullptr
    CommandHistory* GetCommandHistory() const { return history_; }

private:
    std::unique_ptr<scene::Scene> active_scene_;
    std::string scene_file_path_;
    bool scene_dirty_ = false;

    // 协作者（非拥有）
    CommandHistory* history_ = nullptr;
    SelectionManager* selection_ = nullptr;
    ViewportState* viewport_ = nullptr;
    TilesetEditor* tileset_ = nullptr;
    EditorProjectManager* project_ = nullptr;
};

} // namespace editor
} // namespace mg
