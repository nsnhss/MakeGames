#include "resource/game_databases.h"
#include "resource/data_manager.h"
#include <filesystem>

namespace mg {

namespace fs = std::filesystem;

void GameDatabases::LoadFromDirectory(const std::string& data_dir) {
    std::string items_path = data_dir + "/items.json";
    if (fs::exists(items_path)) {
        auto raw = DataManager::Get().LoadJsonRaw(items_path);
        if (raw) {
            auto db = ItemDatabase::FromJson(*raw);
            if (db) item = std::move(*db);
        }
    }

    std::string actors_path = data_dir + "/actors.json";
    if (fs::exists(actors_path)) {
        auto raw = DataManager::Get().LoadJsonRaw(actors_path);
        if (raw) {
            auto db = ActorDatabase::FromJson(*raw);
            if (db) actor = std::move(*db);
        }
    }

    std::string skills_path = data_dir + "/skills.json";
    if (fs::exists(skills_path)) {
        auto raw = DataManager::Get().LoadJsonRaw(skills_path);
        if (raw) {
            auto db = SkillDatabase::FromJson(*raw);
            if (db) skill = std::move(*db);
        }
    }

    std::string enemies_path = data_dir + "/enemies.json";
    if (fs::exists(enemies_path)) {
        auto raw = DataManager::Get().LoadJsonRaw(enemies_path);
        if (raw) {
            auto db = EnemyDatabase::FromJson(*raw);
            if (db) enemy = std::move(*db);
        }
    }

    std::string shops_path = data_dir + "/shops.json";
    if (fs::exists(shops_path)) {
        auto raw = DataManager::Get().LoadJsonRaw(shops_path);
        if (raw) {
            auto db = ShopDatabase::FromJson(*raw);
            if (db) shop = std::move(*db);
        }
    }
}

} // namespace mg
