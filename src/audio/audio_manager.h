#pragma once
#include "audio_mixer.h"
#include <SDL_audio.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace mg {

// 软件混音音频系统：
// - SE（一次性，多实例叠加）/ BGM（循环，唯一）/ BGS（循环，唯一，与 BGM 并行）
// - ME（一次性音乐）：播放期间 BGM 挂起，结束自动恢复
// - BGM/BGS 支持淡入淡出（线性包络，秒）
// 混音核心见 audio_mixer.h（纯逻辑，可单测）；本类负责 SDL 设备、音频加载与格式归一。
// 格式支持：WAV（SDL_LoadWAV）、OGG（stb_vorbis）、MP3（dr_mp3），见 audio_decoder.h。
class AudioManager {
public:
    static AudioManager& Get();

    bool Init();
    void Shutdown();

    void PlaySE(const std::string& path, float volume = 1.0f);

    void PlayBGM(const std::string& path, float volume = 1.0f, float fade_in_sec = 0.0f);
    void StopBGM(float fade_out_sec = 0.0f);

    void PlayBGS(const std::string& path, float volume = 1.0f, float fade_in_sec = 0.0f);
    void StopBGS(float fade_out_sec = 0.0f);

    void PlayME(const std::string& path, float volume = 1.0f);

    // 当前 BGM 路径（无播放中 BGM 时为空串），供存档记录/恢复
    const std::string& GetCurrentBGMPath() const { return current_bgm_path_; }

    // 每帧调用：混音一块并补设备队列
    void Update();

private:
    // 加载并转换到设备格式（48k/S16/stereo），返回 sound bank 索引；失败返回 npos
    static constexpr size_t kInvalidSound = (size_t)-1;
    size_t LoadSound(const std::string& path);

    void StopVoiceOfKind(audio::VoiceKind kind, float fade_out_sec);
    audio::Voice* FindVoice(audio::VoiceKind kind);

    SDL_AudioDeviceID device_ = 0;

    std::vector<audio::SoundData> sounds_;                 // sound bank（索引稳定）
    std::unordered_map<std::string, size_t> path_to_sound_;
    std::vector<audio::Voice> voices_;

    std::vector<int16_t> mix_buffer_;                      // Update 复用
    std::string current_bgm_path_;                         // 当前 BGM（存档用）
};

} // namespace mg
