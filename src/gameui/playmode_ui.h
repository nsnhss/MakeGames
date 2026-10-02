#pragma once
// playmode_ui.h — Play Mode 覆盖层 UI（标题/暂停/对话/选项/商店/战斗/Toast/HUD/Fade）
// editor（Viewport 面板矩形）与 game.exe（全窗口）共用；纯函数 + 显式参数，零 EditorContext 依赖。
#include "core/math/vec2.h"
#include "imgui.h"
#include <functional>
#include <string>

namespace mg {

class GameDatabases;

namespace scene {
class EventSystem;
class PlayModeController;
}

namespace gameui {

// 每帧组装的绘制输入
struct PlayUIContext {
    scene::EventSystem* es = nullptr;             // 事件系统（消息/选项/商店/战斗/Toast/HUD 数据源）
    const GameDatabases* dbs = nullptr;           // 数据库只读引用（物品/角色/技能名解析）
    scene::PlayModeController* play_mode = nullptr; // 状态切换 + 存读档请求 + 槽位信息
    ImDrawList* dl = nullptr;                     // 目标 DrawList
    ImVec2 view_min{0, 0}, view_max{0, 0};        // 覆盖层屏幕矩形
    float fade_alpha = 0.0f;
    const char* game_title = "MakeGames";
    std::string project_root;                     // 存档槽信息读取
    std::function<void()> on_exit;                // Exit 按钮（editor=回编辑模式，game=退出进程）
    const char* exit_label = "Exit";             // Exit 按钮文本（editor="Exit to Editor"，game="Quit"）
    math::Vec2 cam_pos{0, 0};                     // Toast 世界→屏幕换算
    float cam_zoom = 1.0f;
};

// 暂停菜单子页（调用方持有跨帧状态；非 Paused 时调用方负责重置为 None）
enum class PauseSub { None, SaveSlots, LoadSlots, Items, Status };

void DrawFadeOverlay(const PlayUIContext& ui);
void DrawTitleScreen(const PlayUIContext& ui);
void DrawPauseMenu(const PlayUIContext& ui, PauseSub& sub);
void DrawDialog(const PlayUIContext& ui, ImVec2 mouse);
void DrawChoices(const PlayUIContext& ui);
void DrawToasts(const PlayUIContext& ui);
void DrawShopOverlay(const PlayUIContext& ui);
void DrawBattleOverlay(const PlayUIContext& ui);
void DrawHUDOverlays(const PlayUIContext& ui);

} // namespace gameui
} // namespace mg
