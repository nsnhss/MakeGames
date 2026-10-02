#pragma once
#include <cstdint>
#include <string>
#include <memory>
#include <functional>
#include "core/math/vec2.h"
#include "scene/scene.h"
#include "scene/components/tilemap.h"
#include "imgui.h"
#include "tileset_asset.h"
#include "resource/schema/project_schema.h"
#include "editor/command_history.h"
#include "resource/game_databases.h"
#include "editor/selection_manager.h"
#include "scene/play_mode_controller.h"
#include "editor/viewport_state.h"
#include "editor/tileset_editor.h"
#include "editor/editor_project_manager.h"
#include "editor/scene_session.h"
#include "editor/prefab_manager.h"
#include <unordered_map>
#include <vector>

namespace mg {
namespace scene { class EventSystem; class World; }
namespace editor { class PanelManager;

// 预留：资源管理器接口
class IAssetManager {
public:
    virtual ~IAssetManager() = default;
};

// 全局编辑器上下文
// 所有面板通过此单例共享状态，实现选择同步、播放模式切换等
class EditorContext {
public:
    // ---- 选区系统（委托 SelectionManager）----
    using SelectionType = SelectionManager::Type; // 兼容旧调用点

    void SelectEntity(uint32_t entity_id) { selection_.SelectEntity(entity_id); }
    void SelectAsset(const std::string& path) { selection_.SelectAsset(path); }
    void ClearSelection() { selection_.Clear(); }

    SelectionType GetSelectionType() const { return selection_.GetType(); }
    uint32_t GetSelectedEntity() const { return selection_.GetSelectedEntity(); }
    const std::string& GetSelectedAssetPath() const { return selection_.GetSelectedAssetPath(); }
    SelectionManager& GetSelection() { return selection_; }

    // ---- 场景系统（委托 SceneSession，F8 拆分）----
    using EntityTemplate = SceneSession::EntityTemplate; // 兼容旧调用点

    void SetActiveScene(scene::Scene* scene) { session_.SetActiveScene(scene); }
    scene::Scene* GetActiveScene() const { return session_.GetActiveScene(); }
    void CreateDefaultScene() { session_.CreateDefaultScene(); }
    SceneSession& GetSceneSession() { return session_; }

    mg::math::Vec2 GetCameraWorldCenter() const { return viewport_.GetCameraWorldCenter(ActiveWorld()); }

    // 创建实体模板（Create 菜单用）
    scene::EntityID CreateEntityFromTemplate(EntityTemplate type, const mg::math::Vec2& position) {
        return session_.CreateEntityFromTemplate(type, position);
    }
    scene::EntityID CreateEntityFromTemplateAtCameraCenter(EntityTemplate type) {
        return session_.CreateEntityFromTemplateAtCameraCenter(type);
    }

    // ---- 项目系统（路径/清单/MapInfos/最近项目委托 EditorProjectManager）----
    bool OpenProject(const std::string& project_path);     // 打开项目：读 project.json → 加载 start_scene，并加入最近项目列表
    bool NewProject(const std::string& name,
                    const std::string& path,
                    const std::string& mode) { return project_.NewProject(name, path, mode); }
    bool SaveProject();                                     // 保存项目：保存当前场景 + 更新索引
    bool LoadScene(const std::string& path) { return session_.LoadScene(path); }   // 加载地图/场景文件
    bool SaveScene(const std::string& path) { return session_.SaveScene(path); }   // 保存当前场景到指定路径
    void RebuildMapInfos() { project_.RebuildMapInfos(); }  // 扫描 maps/ 重建 MapInfos.json

    // ---- 多地图管理 ----
    MapInfos LoadMapInfos() const { return project_.LoadMapInfos(); } // 读取 maps/MapInfos.json
    bool SwitchScene(const std::string& map_name) { return session_.SwitchScene(map_name); }          // 保存当前场景并切换到目标场景（map_name 为 MapInfos 中的 name 字段）
    bool CreateAndSaveNewScene(const std::string& name, int w = kDefaultMapWidth, int h = kDefaultMapHeight, int tile_size = kDefaultTileSize) { return session_.CreateAndSaveNewScene(name, w, h, tile_size); }    // 创建并保存新场景，失败时回滚
    bool RenameScene(const std::string& old_name, const std::string& new_name) { return session_.RenameScene(old_name, new_name); } // 重命名场景文件并更新索引
    bool DeleteScene(const std::string& name) { return session_.DeleteScene(name); }              // 删除场景文件并从索引移除

    const ProjectManifest* GetProjectManifest() const { return project_.GetManifest(); }
    EditorProjectManager& GetProjectManager() { return project_; }
    PrefabManager& GetPrefabManager() { return prefabs_; }

    // ---- 场景持久化 ----
    void MarkSceneDirty() { session_.MarkSceneDirty(); }
    void ClearSceneDirty() { session_.ClearSceneDirty(); }
    bool IsSceneDirty() const { return session_.IsSceneDirty(); }
    void SetScenePath(const std::string& path) { session_.SetScenePath(path); }
    const std::string& GetScenePath() const { return session_.GetScenePath(); }

    // 保存当前场景（若未指定路径则自动落到 project/scenes/Untitled.scene.json）
    bool SaveCurrentScene() { return session_.SaveCurrentScene(); }

    // ---- 播放模式（委托 PlayModeController）----
    using PlayState = scene::PlayModeController::State;   // 兼容旧调用点
    using ScreenMode = scene::PlayModeController::Screen;
    using FadeState = scene::PlayModeController::Fade;
    using SaveSlotInfo = scene::PlayModeController::SaveSlotInfo;

    void SetPlayState(PlayState state);
    PlayState GetPlayState() const { return play_mode_.GetState(); }
    bool IsPlaying() const { return play_mode_.IsPlaying(); }
    bool IsPaused() const { return play_mode_.IsPaused(); }

    // ---- 标题画面 / 游戏画面 ----
    void SetScreenMode(ScreenMode mode) { play_mode_.SetScreen(mode); }
    ScreenMode GetScreenMode() const { return play_mode_.GetScreen(); }
    std::string GetStartScenePath() const { return session_.GetStartScenePath(); }
    void EnsurePlayer() { session_.EnsurePlayer(); }
    void FixupActiveTilemapTileset() { session_.FixupActiveTilemapTileset(); }
    void RequestStartNewGame() { play_mode_.RequestStartNewGame(); }
    void RequestContinueGame() { play_mode_.RequestContinueGame(); }
    bool ConsumeStartNewGameRequest() { return play_mode_.ConsumeStartNewGameRequest(); }
    bool ConsumeContinueGameRequest() { return play_mode_.ConsumeContinueGameRequest(); }

    // ---- 场景过渡 Fade ----
    void SetFadeState(FadeState state) { play_mode_.SetFadeState(state); }
    FadeState GetFadeState() const { return play_mode_.GetFadeState(); }
    void SetFadeAlpha(float alpha) { play_mode_.SetFadeAlpha(alpha); }
    float GetFadeAlpha() const { return play_mode_.GetFadeAlpha(); }

    // ---- 存档槽位 ----
    SaveSlotInfo ReadSaveSlotInfo(int slot) const { return play_mode_.ReadSaveSlotInfo(project_.GetProjectPath(), slot); }
    static std::string GetSaveFilePath(int slot) { return scene::PlayModeController::GetSaveFilePath(slot); }

    // ---- 存档请求（由 ViewportPanel 触发，EditorLayer 执行）----
    void RequestSave() { play_mode_.RequestSave(); }
    void RequestLoad() { play_mode_.RequestLoad(); }
    void RequestSaveToSlot(int slot) { play_mode_.RequestSaveToSlot(slot); }
    void RequestLoadFromSlot(int slot) { play_mode_.RequestLoadFromSlot(slot); }
    int ConsumeSaveRequest() { return play_mode_.ConsumeSaveRequest(); }
    int ConsumeLoadRequest() { return play_mode_.ConsumeLoadRequest(); }
    scene::PlayModeController& GetPlayMode() { return play_mode_; }

    // ---- 变换工具状态（委托 ViewportState）----
    using GizmoMode = ViewportState::Gizmo; // 兼容旧调用点
    using ViewMode = ViewportState::View;

    void SetGizmoMode(GizmoMode mode) { viewport_.SetGizmoMode(mode); }
    GizmoMode GetGizmoMode() const { return viewport_.GetGizmoMode(); }

    // ---- 网格吸附 ----
    void SetSnapToGrid(bool snap) { viewport_.SetSnapToGrid(snap); }
    bool IsSnapToGrid() const { return viewport_.IsSnapToGrid(); }

    // ---- 视图模式 ----
    void SetViewMode(ViewMode mode) { viewport_.SetViewMode(mode); }
    ViewMode GetViewMode() const { return viewport_.GetViewMode(); }

    // ---- Play Mode 快照 ----
    void TakeSnapshot();
    void RestoreSnapshot();

    // ---- Undo/Redo（委托 CommandHistory）----
    void ExecuteCommand(std::unique_ptr<ICommand> cmd) { command_history_.Execute(std::move(cmd)); }
    bool CanUndo() const { return command_history_.CanUndo(); }
    bool CanRedo() const { return command_history_.CanRedo(); }
    void Undo() { command_history_.Undo(); }
    void Redo() { command_history_.Redo(); }
    CommandHistory& GetCommandHistory() { return command_history_; }

    // ---- 资源系统（预留）----
    void SetAssetManager(IAssetManager* mgr) { asset_manager_ = mgr; }
    IAssetManager* GetAssetManager() const { return asset_manager_; }

    // ---- 项目路径（由 Launcher 传入）----
    void SetProjectPath(const std::string& path) { project_.SetProjectPath(path); }
    const std::string& GetProjectPath() const { return project_.GetProjectPath(); }

    // ---- 最近打开项目 ----
    std::vector<std::string> GetRecentProjects() const { return project_.GetRecentProjects(); }
    void AddRecentProject(const std::string& path) { project_.AddRecentProject(path); }

    // ---- Viewport 相机控制（委托 ViewportState，场景主 Camera Component）----
    void SetCameraPosition(const math::Vec2& pos) { viewport_.SetCameraPosition(ActiveWorld(), pos); }
    math::Vec2 GetCameraPosition() const { return viewport_.GetCameraPosition(ActiveWorld()); }
    void SetCameraZoom(float zoom) { viewport_.SetCameraZoom(ActiveWorld(), zoom); }
    float GetCameraZoom() const { return viewport_.GetCameraZoom(ActiveWorld()); }

    void SetRenderCamera(const math::Vec2& pos, float zoom) { viewport_.SetRenderCamera(ActiveWorld(), pos, zoom); }
    math::Vec2 GetRenderCameraPosition() const { return viewport_.GetRenderCameraPosition(ActiveWorld()); }
    float GetRenderCameraZoom() const { return viewport_.GetRenderCameraZoom(ActiveWorld()); }

    // 主相机 Entity 管理
    void SetPrimaryCameraEntity(uint32_t entity_id) { viewport_.SetPrimaryCameraEntity(entity_id); }
    uint32_t GetPrimaryCameraEntity() const { return viewport_.GetPrimaryCameraEntity(); }
    void EnsurePrimaryCamera() { viewport_.EnsurePrimaryCamera(ActiveWorld()); }

    // ---- Viewport 渲染纹理（由 EditorApplication 设置）----
    void SetViewportTexture(ImTextureID id) { viewport_.SetViewportTexture(id); }
    ImTextureID GetViewportTexture() const { return viewport_.GetViewportTexture(); }
    void SetViewportSize(uint32_t w, uint32_t h) { viewport_.SetViewportSize(w, h); }
    uint32_t GetViewportWidth() const { return viewport_.GetViewportWidth(); }
    uint32_t GetViewportHeight() const { return viewport_.GetViewportHeight(); }
    ViewportState& GetViewport() { return viewport_; }

    // ---- Tileset / Tilemap 编辑状态（委托 TilesetEditor）----
    using TileBrushMode = TilesetEditor::Brush; // 兼容旧调用点

    bool LoadTileset(const std::string& name, const std::string& image_path,
                     int tile_w, int tile_h, int columns, int rows,
                     int start_index = -1) {
        return tileset_.LoadTileset(name, image_path, tile_w, tile_h, columns, rows, start_index);
    }
    void UnloadAllTilesets() { tileset_.UnloadAllTilesets(); }
    const TilesetAsset* GetTileset(const std::string& name) const { return tileset_.GetTileset(name); }
    TilesetAsset* GetTileset(const std::string& name) { return tileset_.GetTileset(name); }
    const std::unordered_map<std::string, TilesetAsset>& GetTilesetRegistry() const { return tileset_.GetRegistry(); }
    void SetTilesetAutoTile(const std::string& name, bool value) { tileset_.SetAutoTile(name, value); }
    void SelectTileset(const std::string& name) { tileset_.SelectTileset(name); }
    const TilesetAsset* GetSelectedTileset() const { return tileset_.GetSelectedTileset(); }
    void SelectTile(int index) { tileset_.SelectTile(index); }
    int GetSelectedTileIndex() const { return tileset_.GetSelectedTileIndex(); }
    int GetSelectedGlobalTileIndex() const { return tileset_.GetSelectedGlobalTileIndex(); }

    // 按全局 tile 索引查找所属 tileset（多 tileset）
    const TilesetAsset* GetTilesetByGlobalTile(int global_tile) const { return tileset_.GetTilesetByGlobalTile(global_tile); }
    void SetCurrentTileLayer(int layer) { tileset_.SetCurrentTileLayer(layer); }
    int GetCurrentTileLayer() const { return tileset_.GetCurrentTileLayer(); }

    void SetTileBrushMode(TileBrushMode mode) { tileset_.SetBrushMode(mode); }
    TileBrushMode GetTileBrushMode() const { return tileset_.GetBrushMode(); }

    void SetTerrainBrushTag(uint8_t tag) { tileset_.SetTerrainBrushTag(tag); }
    uint8_t GetTerrainBrushTag() const { return tileset_.GetTerrainBrushTag(); }

    void SetPassageBrushMask(uint8_t mask) { tileset_.SetPassageBrushMask(mask); }
    uint8_t GetPassageBrushMask() const { return tileset_.GetPassageBrushMask(); }
    void SetTilemapEntity(uint32_t entity_id) { tileset_.SetTilemapEntity(entity_id); }
    uint32_t GetTilemapEntity() const { return tileset_.GetTilemapEntity(); }

    bool LoadTilesetsFromProject(renderer::RendererContext* ctx) { return tileset_.LoadFromProject(project_.GetProjectPath(), ctx); }
    void SaveTilesetsToProject() { tileset_.SaveToProject(project_.GetProjectPath()); }
    bool LoadDemoTileset(renderer::RendererContext* ctx) { return tileset_.LoadDemoTileset(ctx); }

    void MarkTilemapDirty() { tileset_.MarkTilemapDirty(); }
    void ClearTilemapDirty() { tileset_.ClearTilemapDirty(); }
    bool IsTilemapDirty() const { return tileset_.IsTilemapDirty(); }
    TilesetEditor& GetTilesetEditor() { return tileset_; }

    void SetEventSystem(scene::EventSystem* sys) { event_system_ = sys; }
    scene::EventSystem* GetEventSystem() const { return event_system_; }

    void SetPanelManager(PanelManager* pm) { panel_manager_ = pm; }
    PanelManager* GetPanelManager() const { return panel_manager_; }

    // 打开弹窗面板（由 Outliner/Viewport/Menu 触发）
    void OpenDetailsPanel();
    void OpenEventEditorPanel();
    void OpenTilePalettePanel();
    void OpenScriptEditorPanel();

    // ---- 游戏数据库（物品/角色/技能/敌人/商店，委托 GameDatabases）----
    const ItemDatabase& GetItemDatabase() const { return databases_.item; }
    ItemDatabase& GetItemDatabase() { return databases_.item; }
    const ActorDatabase& GetActorDatabase() const { return databases_.actor; }
    ActorDatabase& GetActorDatabase() { return databases_.actor; }
    const SkillDatabase& GetSkillDatabase() const { return databases_.skill; }
    SkillDatabase& GetSkillDatabase() { return databases_.skill; }
    const EnemyDatabase& GetEnemyDatabase() const { return databases_.enemy; }
    EnemyDatabase& GetEnemyDatabase() { return databases_.enemy; }
    const ShopDatabase& GetShopDatabase() const { return databases_.shop; }
    ShopDatabase& GetShopDatabase() { return databases_.shop; }
    GameDatabases& GetDatabases() { return databases_; }

    // 构造后由 main.cpp 持有并沿组装链注入（已去单例化）
    EditorContext();

private:
    // 当前活动场景的 World（无场景时返回 nullptr），供各门面转发方法取参
    scene::World* ActiveWorld() const { return session_.GetWorld(); }

    SelectionManager selection_;

    SceneSession session_;

    scene::PlayModeController play_mode_;
    ViewportState viewport_;

    IAssetManager* asset_manager_ = nullptr;
    EditorProjectManager project_;
    PrefabManager prefabs_;
    CommandHistory command_history_;

    // Tileset 编辑
    TilesetEditor tileset_;

    // 游戏数据库
    GameDatabases databases_;
    scene::EventSystem* event_system_ = nullptr;
    PanelManager* panel_manager_ = nullptr;
};

} // namespace editor
} // namespace mg
