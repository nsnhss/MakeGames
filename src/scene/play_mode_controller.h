#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include "core/math/vec2.h"

namespace mg {
namespace scene {

class World;

// 播放模式控制：Edit/Playing/Paused 状态、标题/游戏画面、Fade 过渡、
// 存档槽位与存读档请求、Play Mode 位置快照。
// 状态切换的编排（清 Undo 历史、触发回调）由 EditorContext 门面完成。
class PlayModeController {
public:
    enum class State { Edit, Playing, Paused };
    enum class Screen { Title, Game };
    enum class Fade { None, Out, In };

    void SetState(State s) { state_ = s; }
    State GetState() const { return state_; }
    bool IsPlaying() const { return state_ == State::Playing; }
    bool IsPaused() const { return state_ == State::Paused; }

    void SetScreen(Screen m) { screen_ = m; }
    Screen GetScreen() const { return screen_; }

    void SetFadeState(Fade s) { fade_state_ = s; }
    Fade GetFadeState() const { return fade_state_; }
    void SetFadeAlpha(float alpha) { fade_alpha_ = alpha; }
    float GetFadeAlpha() const { return fade_alpha_; }

    // 开始新游戏 / 读档继续请求（标题画面触发，EditorLayer 消费）
    void RequestStartNewGame() { pending_start_new_game_ = true; }
    void RequestContinueGame() { pending_continue_game_ = true; }
    bool ConsumeStartNewGameRequest() { return std::exchange(pending_start_new_game_, false); }
    bool ConsumeContinueGameRequest() { return std::exchange(pending_continue_game_, false); }

    // 存档请求（由 ViewportPanel 触发，EditorLayer 执行）
    void RequestSave() { pending_save_slot_ = 1; }
    void RequestLoad() { pending_load_slot_ = 1; }
    void RequestSaveToSlot(int slot) { pending_save_slot_ = slot; }
    void RequestLoadFromSlot(int slot) { pending_load_slot_ = slot; }
    int ConsumeSaveRequest() { return std::exchange(pending_save_slot_, 0); }
    int ConsumeLoadRequest() { return std::exchange(pending_load_slot_, 0); }

    // Play Mode 快照：进入播放时保存所有 Transform position，停止时恢复
    void TakeSnapshot(World* world);
    void RestoreSnapshot(World* world);

    // 存档槽位
    struct SaveSlotInfo {
        int slot = 0;
        bool exists = false;
        std::string scene_name;
        std::string last_save_time; // ISO-8601，空表示无存档
    };
    SaveSlotInfo ReadSaveSlotInfo(const std::string& project_path, int slot) const;
    static std::string GetSaveFilePath(int slot) {
        return "saves/save_" + std::to_string(slot / 100 % 10) + std::to_string(slot / 10 % 10) + std::to_string(slot % 10) + ".json";
    }

private:
    State state_ = State::Edit;
    Screen screen_ = Screen::Title;
    Fade fade_state_ = Fade::None;
    float fade_alpha_ = 0.0f;
    int pending_save_slot_ = 0;
    int pending_load_slot_ = 0;
    bool pending_start_new_game_ = false;
    bool pending_continue_game_ = false;
    std::unordered_map<uint32_t, math::Vec2> play_snapshot_;
};

} // namespace scene
} // namespace mg
