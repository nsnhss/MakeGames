#pragma once
// ---------------------------------------------------------------------------
// SaveSystem：存读档文件与 JSON 组装（D2 从 GameRuntime 拆出）
// 存档格式冻结：{"scene","player_x","player_y","globals","bgm","last_save_time"}
// 场景加载/玩家定位/EventSystem 重建等编排仍由 GameRuntime 负责。
// ---------------------------------------------------------------------------
#include "third_party/json.hpp"
#include <string>

namespace mg {
namespace scene {

class World;
class EventSystem;
class PlayModeController;

// 组装存档 JSON：场景相对路径 + 玩家位置（首个 Movement 实体）+ 全局变量 + 当前 BGM + 时间戳
nlohmann::json BuildSaveGameJson(World* world, EventSystem& event_system,
                                 const std::string& scene_rel);

// 写入 <project_root>/saves/saveN.json（自动建目录）；写入成功返回 true
bool WriteSaveSlot(const std::string& project_root, int slot, const nlohmann::json& save);

// 读取存档 JSON；文件不存在或解析失败返回 false
bool ReadSaveSlot(const std::string& project_root, int slot, nlohmann::json& out);

// ContinueGame：扫描 1..3 号槽，返回 last_save_time 最新的槽位号；无存档返回 0
int FindLatestSaveSlot(PlayModeController& play_mode, const std::string& project_root);

} // namespace scene
} // namespace mg
