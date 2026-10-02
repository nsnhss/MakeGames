#pragma once
#include "resource/item_database.h"
#include "resource/actor_database.h"
#include "resource/skill_database.h"
#include "resource/enemy_database.h"
#include "resource/shop_database.h"

namespace mg {

// 游戏数据库聚合：物品 / 角色 / 技能 / 敌人 / 商店
// 数据定义在 resource/ 下按域拆分，此处做持有与传递（editor 与 game.exe 共用）。
struct GameDatabases {
    ItemDatabase item;
    ActorDatabase actor;
    SkillDatabase skill;
    EnemyDatabase enemy;
    ShopDatabase shop;

    // 从 data 目录加载全部数据库（文件缺失则跳过对应库）
    // data_dir 例：editor="<project>/data"，game="data"
    void LoadFromDirectory(const std::string& data_dir);
};

} // namespace mg
