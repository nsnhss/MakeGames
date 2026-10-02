#include "scene_session.h"
#include "command_history.h"
#include "selection_manager.h"
#include "viewport_state.h"
#include "tileset_editor.h"
#include "editor_project_manager.h"
#include "core/debug_log.h"
#include "scene/scene_serializer.h"
#include "scene/components/transform.h"
#include "scene/components/name.h"
#include "scene/components/sprite.h"
#include "scene/components/tilemap.h"
#include "scene/components/movement.h"
#include "scene/components/camera.h"
#include "scene/components/camera_follow.h"
#include "scene/components/event_trigger.h"
#include "resource/data_manager.h"
#include <filesystem>
#include <algorithm>

namespace {
// 确保场景文件名以 .json 结尾（避免重复代码和越界检查）
std::string EnsureJsonExtension(std::string filename) {
    if (filename.size() < 5 || filename.substr(filename.size() - 5) != ".json") {
        filename += ".json";
    }
    return filename;
}
} // namespace

namespace mg {
namespace editor {

void SceneSession::Bind(CommandHistory* history, SelectionManager* selection,
                        ViewportState* viewport, TilesetEditor* tileset,
                        EditorProjectManager* project) {
    history_ = history;
    selection_ = selection;
    viewport_ = viewport;
    tileset_ = tileset;
    project_ = project;
}

scene::World* SceneSession::GetWorld() const {
    return active_scene_ ? active_scene_->GetWorld() : nullptr;
}

// 设置当前活跃场景（接管所有权）
void SceneSession::SetActiveScene(scene::Scene* scene) {
    active_scene_.reset(scene);
}

// 保存当前场景（若未指定路径则自动落到 project/scenes/Untitled.scene.json）
bool SceneSession::SaveCurrentScene() {
    auto* scene = GetActiveScene();
    if (!scene) return false;
    std::string path = scene_file_path_;
    if (path.empty()) {
        if (!project_->GetProjectPath().empty()) path = project_->GetProjectPath() + "/scenes";
        else path = "scenes";
        std::filesystem::create_directories(path);
        path += "/Untitled.scene.json";
        scene_file_path_ = path;
    }
    if (scene::SceneSerializer::SaveToFile(scene, path)) {
        scene_dirty_ = false;
        DataManager::Get().Invalidate(path);  // 同步缓存
        return true;
    }
    return false;
}

scene::EntityID SceneSession::CreateEntityFromTemplateAtCameraCenter(EntityTemplate type) {
    return CreateEntityFromTemplate(type, viewport_->GetCameraWorldCenter(GetWorld()));
}

scene::EntityID SceneSession::CreateEntityFromTemplate(EntityTemplate type, const mg::math::Vec2& position) {
    auto* scene = GetActiveScene();
    if (!scene || !scene->GetWorld()) return 0;
    auto* world = scene->GetWorld();

    using namespace mg::scene;
    auto entity = world->CreateEntity();
    world->AddComponent<Transform>(entity, position);

    auto AddEventPage = [&](EventPage::TriggerType trigger, const std::vector<Action>& actions, bool run_once = false) {
        EventPage page;
        page.trigger = trigger;
        page.actions = actions;
        page.run_once = run_once;
        auto& et = world->AddComponent<EventTrigger>(entity);
        et.pages.push_back(std::move(page));
    };

    switch (type) {
        case EntityTemplate::Empty: {
            world->AddComponent<Name>(entity, "Entity");
            world->AddComponent<Sprite>(entity);
            break;
        }
        case EntityTemplate::NPC: {
            world->AddComponent<Name>(entity, "NPC");
            world->AddComponent<Sprite>(entity);
            auto& m = world->AddComponent<Movement>(entity);
            m.speed = 0.0f;
            AddEventPage(EventPage::TriggerType::OnInteract, {});
            break;
        }
        case EntityTemplate::Monster: {
            world->AddComponent<Name>(entity, "Monster");
            world->AddComponent<Sprite>(entity);
            auto& m = world->AddComponent<Movement>(entity);
            m.speed = 0.0f;
            Action battle;
            battle.cmd = ActionCmd::BattleProcessing;
            battle.target = "";
            battle.int_value = 1;
            battle.param = "player";
            AddEventPage(EventPage::TriggerType::OnTouch, {battle});
            break;
        }
        case EntityTemplate::Chest: {
            world->AddComponent<Name>(entity, "Chest");
            world->AddComponent<Sprite>(entity);
            Action toast;
            toast.cmd = ActionCmd::ShowToast;
            toast.text = "你获得了物品";
            Action add_item;
            add_item.cmd = ActionCmd::AddItem;
            add_item.target = "";
            add_item.int_value = 1;
            AddEventPage(EventPage::TriggerType::OnInteract, {toast, add_item}, true);
            break;
        }
        case EntityTemplate::Transfer: {
            world->AddComponent<Name>(entity, "Transfer");
            world->AddComponent<Sprite>(entity);
            Action transfer;
            transfer.cmd = ActionCmd::TransferPlayer;
            transfer.target = "";
            transfer.int_value = 0;
            transfer.float_value = 0.0f;
            AddEventPage(EventPage::TriggerType::OnTouch, {transfer});
            break;
        }
        case EntityTemplate::Shop: {
            world->AddComponent<Name>(entity, "Shop");
            world->AddComponent<Sprite>(entity);
            auto& m = world->AddComponent<Movement>(entity);
            m.speed = 0.0f;
            Action shop;
            shop.cmd = ActionCmd::ShopProcessing;
            shop.target = "";
            AddEventPage(EventPage::TriggerType::OnInteract, {shop});
            break;
        }
        case EntityTemplate::Decoration: {
            world->AddComponent<Name>(entity, "Decoration");
            world->AddComponent<Sprite>(entity);
            break;
        }
    }

    history_->Execute(std::make_unique<CreateEntityCommand>(world, entity));
    selection_->SelectEntity(entity);
    MarkSceneDirty();
    return entity;
}

// 创建空场景（最小骨架：Tilemap + Camera + Player），无硬编码内容；
// 用户通过编辑器添加 tiles、entities、events
void SceneSession::CreateDefaultScene(int w, int h, int tile_size) {
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (tile_size < 1) tile_size = 1;
    active_scene_ = std::make_unique<scene::Scene>();
    active_scene_->Initialize();
    active_scene_->SetName("Untitled");
    active_scene_->SetSize(w, h);
    active_scene_->SetTileSize(tile_size);

    auto* world = active_scene_->GetWorld();
    if (!world) { return; }

    // 创建 Tilemap 实体（Ground 地面层）
    auto tm_entity = world->CreateEntity();
    world->AddComponent<scene::Name>(tm_entity, "Tilemap");
    world->AddComponent<scene::Transform>(tm_entity, math::Vec2(0, 0));
    auto& tm = world->AddComponent<scene::Tilemap>(tm_entity);
    tm.tile_width = tile_size; tm.tile_height = tile_size;
    tm.layers.emplace_back("Ground", w, h);
    // 默认不填充任何瓦片（-1 表示空），避免新建项目就出现大片默认颜色背景
    std::fill(tm.layers[0].tiles.begin(), tm.layers[0].tiles.end(), -1);

    // 为 Tilemap 绑定一个可用 tileset，否则 RebuildTilemapVB 找不到纹理，地面无法渲染
    std::string fallback_ts;
    if (auto* ts = tileset_->GetSelectedTileset()) {
        fallback_ts = ts->name;
    } else if (tileset_->GetRegistry().count("Demo")) {
        fallback_ts = "Demo";
    } else if (!tileset_->GetRegistry().empty()) {
        fallback_ts = tileset_->GetRegistry().begin()->first;
    }
    if (!fallback_ts.empty()) {
        tm.tileset_name = fallback_ts;
        tm.tileset_names = { fallback_ts };
    }

    tileset_->SetTilemapEntity(tm_entity);

    // Player 实体（组件已就绪，属性全为默认）
    auto player = world->CreateEntity();
    world->AddComponent<scene::Name>(player, "Player");
    world->AddComponent<scene::Transform>(player, math::Vec2(160, 120));
    world->AddComponent<scene::Movement>(player);
    world->AddComponent<scene::CameraFollowTarget>(player);
    world->AddComponent<scene::Sprite>(player);

    // 默认 Camera（显式跟随 Player）
    auto cam_entity = world->CreateEntity();
    world->AddComponent<scene::Name>(cam_entity, "Camera");
    world->AddComponent<scene::Transform>(cam_entity, math::Vec2(320, 240));
    auto& cam = world->AddComponent<scene::Camera>(cam_entity);
    cam.is_primary = true;
    cam.ortho_size = 240.0f;
    cam.follow_target = player;
    viewport_->SetPrimaryCameraEntity(cam_entity);

    MarkSceneDirty();
}

bool SceneSession::LoadScene(const std::string& path) {
    history_->Clear();
    auto raw = DataManager::Get().LoadJsonRaw(path);
    if (!raw) return false;
    auto scene = scene::SceneSerializer::Deserialize(*raw);
    if (!scene) return false;
    scene_file_path_ = path;
    SetActiveScene(scene.release());
    viewport_->EnsurePrimaryCamera(GetWorld());

    // Scene files created before the tileset init fix may have an empty
    // tileset_name. Patch it after any load so the tilemap renders.
    FixupActiveTilemapTileset();
    return true;
}

bool SceneSession::SaveScene(const std::string& path) {
    auto* scene = GetActiveScene();
    if (!scene) return false;
    if (scene::SceneSerializer::SaveToFile(scene, path)) {
        scene_file_path_ = path;
        scene_dirty_ = false;
        return true;
    }
    return false;
}

bool SceneSession::SwitchScene(const std::string& map_name) {
    if (project_->GetProjectPath().empty()) return false;

    // 若当前场景未保存，先保存；保存失败则停止切换，避免丢失修改
    if (IsSceneDirty() && !scene_file_path_.empty()) {
        if (!SaveCurrentScene()) {
            EDITOR_LOG("SceneSession::SwitchScene SaveCurrentScene failed, abort");
            return false;
        }
    }

    auto mi = project_->LoadMapInfos();
    for (const auto& e : mi.entries) {
        if (e.name == map_name) {
            std::string path = project_->GetProjectPath() + "/maps/" + e.name;
            if (LoadScene(path)) {
                SetScenePath(path);
                ClearSceneDirty();
                selection_->Clear();
                history_->Clear();
                            return true;
            }
            return false;
        }
    }
    return false;
}

bool SceneSession::CreateAndSaveNewScene(const std::string& name, int w, int h, int tile_size) {
    if (project_->GetProjectPath().empty()) return false;
    namespace fs = std::filesystem;

    // 规范化文件名：空格转下划线，追加 .json 后缀
    std::string filename = name;
    for (char& c : filename) {
        if (c == ' ') c = '_';
    }
    filename = EnsureJsonExtension(filename);
    std::string path = project_->GetProjectPath() + "/maps/" + filename;

    // 备份当前场景，用于失败回滚
    auto old_scene = std::move(active_scene_);
    std::string old_path = scene_file_path_;

    try {
        // 创建新场景
        CreateDefaultScene(w, h, tile_size);
        auto* scene = GetActiveScene();
        if (!scene) {
            // 恢复旧场景
            active_scene_ = std::move(old_scene);
            scene_file_path_ = old_path;
            return false;
        }
        scene->SetName(name);

        // 保存新场景
        if (!SaveScene(path)) {
            // 保存失败：销毁新场景，恢复原场景
            active_scene_.reset();
            active_scene_ = std::move(old_scene);
            scene_file_path_ = old_path;
            return false;
        }

        // 成功：重建 MapInfos、更新路径、清空历史
        project_->RebuildMapInfos();
        scene_file_path_ = path;
        ClearSceneDirty();
        selection_->Clear();
        history_->Clear();

        // 释放旧场景备份
        old_scene.reset();

            return true;
    } catch (...) {
        // 异常回滚：任何中间步骤抛异常都恢复旧场景
        active_scene_.reset();
        active_scene_ = std::move(old_scene);
        scene_file_path_ = old_path;
        return false;
    }
}

bool SceneSession::RenameScene(const std::string& old_name, const std::string& new_name) {
    if (project_->GetProjectPath().empty()) return false;
    namespace fs = std::filesystem;

    std::string old_filename = EnsureJsonExtension(old_name);
    std::string new_filename = new_name;
    for (char& c : new_filename) {
        if (c == ' ') c = '_';
    }
    new_filename = EnsureJsonExtension(new_filename);

    std::string old_path = project_->GetProjectPath() + "/maps/" + old_filename;
    std::string new_path = project_->GetProjectPath() + "/maps/" + new_filename;

    if (!fs::exists(old_path)) return false;
    if (fs::exists(new_path)) return false;

    try {
        fs::rename(old_path, new_path);
    } catch (...) {
        return false;
    }

    // 如果当前打开的是被重命名的场景，同步 scene_file_path_
    if (scene_file_path_ == old_path) {
        scene_file_path_ = new_path;
    }

    // 如果 project.json 的 start_scene 指向旧路径，同步更新
    if (project_->GetManifest() && project_->GetManifest()->start_scene == "maps/" + old_filename) {
        ProjectManifest updated = *project_->GetManifest();
        updated.start_scene = "maps/" + new_filename;
        project_->SetManifest(std::make_shared<const ProjectManifest>(updated));
        DataManager::Get().Save(project_->GetProjectPath() + "/project.json", updated);
    }

    project_->RebuildMapInfos();
    return true;
}

bool SceneSession::DeleteScene(const std::string& name) {
    if (project_->GetProjectPath().empty()) return false;
    namespace fs = std::filesystem;

    std::string filename = EnsureJsonExtension(name);
    std::string path = project_->GetProjectPath() + "/maps/" + filename;

    if (!fs::exists(path)) return false;

    try {
        fs::remove(path);
    } catch (...) {
        return false;
    }

    // 如果删除的是当前打开的场景，切换到剩余第一个场景
    bool was_current = (scene_file_path_ == path);
    project_->RebuildMapInfos();

    if (was_current) {
        auto mi = project_->LoadMapInfos();
        if (!mi.entries.empty()) {
            SwitchScene(mi.entries.front().name);
        } else {
            // 无场景了，创建默认场景并保存为 Map001.json
            CreateDefaultScene();
            fs::create_directories(project_->GetProjectPath() + "/maps");
            std::string new_path = project_->GetProjectPath() + "/maps/Map001.json";
            SaveScene(new_path);
            project_->RebuildMapInfos();
            if (project_->GetManifest()) {
                ProjectManifest updated = *project_->GetManifest();
                updated.start_scene = "maps/Map001.json";
                project_->SetManifest(std::make_shared<const ProjectManifest>(updated));
                DataManager::Get().Save(project_->GetProjectPath() + "/project.json", updated);
            }
            scene_file_path_ = new_path;
            ClearSceneDirty();
            selection_->Clear();
            history_->Clear();
                }
    } else {
        }
    return true;
}

std::string SceneSession::GetStartScenePath() const {
    if (project_->GetManifest() && !project_->GetManifest()->start_scene.empty())
        return project_->GetProjectPath() + "/" + project_->GetManifest()->start_scene;
    if (!scene_file_path_.empty())
        return scene_file_path_;
    // 回退：查找 maps/ 下第一个非 MapInfos.json 的 JSON
    namespace fs = std::filesystem;
    std::string maps_dir = project_->GetProjectPath() + "/maps";
    if (fs::exists(maps_dir)) {
        for (const auto& entry : fs::directory_iterator(maps_dir)) {
            if (!entry.is_regular_file()) continue;
            std::string name = entry.path().filename().string();
            if (name == "MapInfos.json") continue;
            if (entry.path().extension() == ".json") return entry.path().string();
        }
    }
    return {};
}

void SceneSession::EnsurePlayer() {
    auto* scene = GetActiveScene();
    if (!scene || !scene->GetWorld()) return;
    auto* world = scene->GetWorld();
    std::vector<scene::EntityID> players;
    world->Query<scene::Movement>(players);
    if (!players.empty()) return;
    scene::EntityID player = world->CreateEntity();
    world->AddComponent<scene::Transform>(player);
    world->AddComponent<scene::Name>(player, "Player");
    world->AddComponent<scene::Movement>(player);
    world->AddComponent<scene::Sprite>(player);
    world->AddComponent<scene::CameraFollowTarget>(player);
}

void SceneSession::FixupActiveTilemapTileset() {
    auto* scene = GetActiveScene();
    if (!scene || !scene->GetWorld()) return;
    auto* world = scene->GetWorld();
    uint32_t tm_entity = tileset_->GetTilemapEntity();
    if (tm_entity == 0 || !world->IsValid(tm_entity)) {
        // Try to find any entity with a Tilemap component.
        std::vector<scene::EntityID> entities;
        world->GetAllEntities(entities);
        for (auto e : entities) {
            if (world->GetComponent<scene::Tilemap>(e)) {
                tm_entity = e;
                tileset_->SetTilemapEntity(e);
                break;
            }
        }
    }
    if (tm_entity == 0 || !world->IsValid(tm_entity)) return;

    auto* tm = world->GetComponent<scene::Tilemap>(tm_entity);
    if (!tm) return;
    if (!tm->tileset_names.empty()) return;
    if (tm->tileset_name.empty()) {
        if (auto* ts = tileset_->GetSelectedTileset()) {
            tm->tileset_name = ts->name;
        } else if (tileset_->GetTileset("Demo")) {
            tm->tileset_name = "Demo";
        } else {
            const auto& registry = tileset_->GetRegistry();
            if (!registry.empty()) {
                tm->tileset_name = registry.begin()->first;
            }
        }
    }
    if (!tm->tileset_name.empty() && tm->tileset_names.empty()) {
        tm->tileset_names = { tm->tileset_name };
    }
    tileset_->MarkTilemapDirty();
}

} // namespace editor
} // namespace mg
