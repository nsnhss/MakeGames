#include "scene/save_system.h"
#include "scene/world.h"
#include "scene/event_system.h"
#include "scene/play_mode_controller.h"
#include "scene/components/transform.h"
#include "scene/components/movement.h"
#include "audio/audio_manager.h"
#include <ctime>
#include <filesystem>
#include <fstream>

namespace mg {
namespace scene {

namespace fs = std::filesystem;

nlohmann::json BuildSaveGameJson(World* world, EventSystem& event_system,
                                 const std::string& scene_rel) {
    float px = 0.0f, py = 0.0f;
    if (world) {
        std::vector<EntityID> players;
        world->Query<Movement>(players);
        if (!players.empty()) {
            if (auto* pt = world->GetComponent<Transform>(players[0])) {
                px = pt->position.x;
                py = pt->position.y;
            }
        }
    }
    nlohmann::json save;
    save["scene"] = scene_rel;
    save["player_x"] = px;
    save["player_y"] = py;
    save["globals"] = event_system.SaveState()["globals"];
    save["bgm"] = AudioManager::Get().GetCurrentBGMPath();
    std::time_t now = std::time(nullptr);
    std::tm* tm = std::localtime(&now);
    char time_buf[64] = {};
    if (tm) std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S", tm);
    save["last_save_time"] = time_buf;
    return save;
}

bool WriteSaveSlot(const std::string& project_root, int slot, const nlohmann::json& save) {
    fs::create_directories(project_root + "/saves");
    std::string save_path = project_root + "/" + PlayModeController::GetSaveFilePath(slot);
    std::ofstream f(save_path);
    if (!f) return false;
    f << save.dump(4);
    return true;
}

bool ReadSaveSlot(const std::string& project_root, int slot, nlohmann::json& out) {
    std::string save_path = project_root + "/" + PlayModeController::GetSaveFilePath(slot);
    if (!fs::exists(save_path)) return false;
    std::ifstream f(save_path);
    return (bool)(f >> out);
}

int FindLatestSaveSlot(PlayModeController& play_mode, const std::string& project_root) {
    int latest_slot = 0;
    std::string latest_time;
    for (int i = 1; i <= 3; ++i) {
        auto info = play_mode.ReadSaveSlotInfo(project_root, i);
        if (info.exists && info.last_save_time > latest_time) {
            latest_slot = i;
            latest_time = info.last_save_time;
        }
    }
    return latest_slot;
}

} // namespace scene
} // namespace mg
