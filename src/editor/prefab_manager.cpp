#include "prefab_manager.h"
#include "scene/world.h"
#include "scene/components/transform.h"
#include "scene/components/prefab_ref.h"
#include "resource/data_manager.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace mg {
namespace editor {

namespace {
// 规范化 prefab 文件名：空格转下划线
std::string NormalizePrefabName(std::string name) {
    for (char& c : name) {
        if (c == ' ') c = '_';
    }
    return name;
}
} // namespace

bool PrefabManager::SavePrefab(const std::string& project_path, scene::World* world,
                               scene::EntityID entity, const std::string& name) {
    if (project_path.empty() || !world || entity == 0 || name.empty()) return false;
    namespace fs = std::filesystem;
    try {
        std::string filename = NormalizePrefabName(name) + ".prefab.json";
        fs::path dir = fs::path(project_path) / "prefabs";
        fs::create_directories(dir);
        fs::path file = dir / filename;

        nlohmann::json j;
        j["name"] = name;
        j["entity"] = world->SnapshotEntity(entity);
        // 模板文件不存自引用链接（Instantiate 时会重新附加）
        if (j["entity"].contains("components")) j["entity"]["components"].erase("PrefabRef");

        std::ofstream f(file);
        if (!f.is_open()) return false;
        f << j.dump(4);
        f.flush();
        if (!f.good()) return false;
        DataManager::Get().Invalidate(file.string());  // 同步缓存
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> PrefabManager::ListPrefabs(const std::string& project_path) const {
    std::vector<std::string> result;
    if (project_path.empty()) return result;
    namespace fs = std::filesystem;
    fs::path dir = fs::path(project_path) / "prefabs";
    if (!fs::exists(dir)) return result;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        std::string filename = entry.path().filename().string();
        // 匹配 *.prefab.json
        const std::string suffix = ".prefab.json";
        if (filename.size() > suffix.size() &&
            filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) == 0) {
            result.push_back(filename.substr(0, filename.size() - suffix.size()));
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

scene::EntityID PrefabManager::Instantiate(scene::World* world, const std::string& project_path,
                                           const std::string& name, const math::Vec2& position) {
    if (!world || project_path.empty() || name.empty()) return 0;
    std::string path = project_path + "/prefabs/" + NormalizePrefabName(name) + ".prefab.json";
    auto raw = DataManager::Get().LoadJsonRaw(path);
    if (!raw || !raw->contains("entity")) return 0;

    scene::EntityID entity = world->RestoreEntity((*raw)["entity"]);
    if (entity == 0) return 0;

    // 内置 override：实例化位置
    if (auto* t = world->GetComponent<scene::Transform>(entity)) {
        t->position = position;
    }
    // 附加活链接标记
    world->AddComponent<scene::PrefabRef>(entity).prefab = name;
    return entity;
}

bool PrefabManager::ApplyToPrefab(const std::string& project_path, scene::World* world,
                                  scene::EntityID entity) {
    if (!world) return false;
    auto* ref = world->GetComponent<scene::PrefabRef>(entity);
    if (!ref || ref->prefab.empty()) return false;
    return SavePrefab(project_path, world, entity, ref->prefab);
}

bool PrefabManager::RevertToPrefab(scene::World* world, const std::string& project_path,
                                   scene::EntityID entity) {
    if (!world) return false;
    nlohmann::json snapshot;
    if (!BuildRevertSnapshot(world, project_path, entity, snapshot)) return false;
    return world->RestoreEntityInto(entity, snapshot);
}

bool PrefabManager::BuildRevertSnapshot(scene::World* world, const std::string& project_path,
                                        scene::EntityID entity, nlohmann::json& out_snapshot) {
    if (!world) return false;
    auto* ref = world->GetComponent<scene::PrefabRef>(entity);
    if (!ref || ref->prefab.empty()) return false;
    std::string name = ref->prefab;

    std::string path = project_path + "/prefabs/" + NormalizePrefabName(name) + ".prefab.json";
    auto raw = DataManager::Get().LoadJsonRaw(path);
    if (!raw || !raw->contains("entity")) return false;

    out_snapshot = (*raw)["entity"];
    if (!out_snapshot.contains("components") || !out_snapshot["components"].is_object()) return false;
    auto& comps = out_snapshot["components"];

    // 保留实例位置（实例化时的内置 override）
    if (auto* t = world->GetComponent<scene::Transform>(entity)) {
        if (comps.contains("Transform")) comps["Transform"]["position"] = t->position;
    }
    // 保留活链接标记
    comps["PrefabRef"] = nlohmann::json::object();
    comps["PrefabRef"]["prefab"] = name;
    return true;
}

} // namespace editor
} // namespace mg
