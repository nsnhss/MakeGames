#include "scene/play_mode_controller.h"
#include "scene/world.h"
#include "scene/components/transform.h"
#include "third_party/json.hpp"
#include <filesystem>
#include <fstream>
#include <vector>

namespace mg {
namespace scene {

void PlayModeController::TakeSnapshot(World* world) {
    play_snapshot_.clear();
    if (!world) return;
    std::vector<EntityID> entities;
    world->GetAllEntities(entities);
    for (auto e : entities) {
        auto* t = world->GetComponent<Transform>(e);
        if (t) play_snapshot_[e] = t->position;
    }
}

void PlayModeController::RestoreSnapshot(World* world) {
    if (world) {
        for (auto& [e, pos] : play_snapshot_) {
            auto* t = world->GetComponent<Transform>(e);
            if (t) t->position = pos;
        }
    }
    play_snapshot_.clear();
}

PlayModeController::SaveSlotInfo PlayModeController::ReadSaveSlotInfo(
        const std::string& project_path, int slot) const {
    SaveSlotInfo info;
    info.slot = slot;
    if (project_path.empty() || slot <= 0) return info;
    std::string path = project_path + "/" + GetSaveFilePath(slot);
    namespace fs = std::filesystem;
    info.exists = fs::exists(path);
    if (info.exists) {
        std::ifstream f(path);
        nlohmann::json j;
        if (f >> j) {
            info.scene_name = j.value("scene", "");
            info.last_save_time = j.value("last_save_time", "");
        }
    }
    return info;
}

} // namespace scene
} // namespace mg
