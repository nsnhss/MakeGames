#pragma once
#include "resource/item_database.h"
#include "resource/actor_database.h"
#include "resource/skill_database.h"
#include "resource/enemy_database.h"
#include "resource/shop_database.h"

namespace mg {
namespace editor {

// 游戏数据库聚合：物品 / 角色 / 技能 / 敌人 / 商店
// 数据定义在 resource/ 下按域拆分，此处仅做编辑器侧的持有与传递。
struct GameDatabases {
    ItemDatabase item;
    ActorDatabase actor;
    SkillDatabase skill;
    EnemyDatabase enemy;
    ShopDatabase shop;
};

} // namespace editor
} // namespace mg
