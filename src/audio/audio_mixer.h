#pragma once
// ---------------------------------------------------------------------------
// 软件混音器（纯逻辑，不依赖 SDL，可单测）
// 所有 SoundData 均为设备格式：48kHz / S16 / 立体声交错。
// MixVoices 把活动声部混进输出缓冲并推进播放位置与音量包络（fade）。
// ---------------------------------------------------------------------------
#include <cstdint>
#include <vector>

namespace mg {
namespace audio {

constexpr int kSampleRate = 48000;
constexpr int kChannels = 2;

struct SoundData {
    std::vector<int16_t> samples;   // 立体声交错
};

enum class VoiceKind { SE, BGM, BGS, ME };

struct Voice {
    size_t    sound_index = 0;      // 索引 SoundBank
    size_t    pos = 0;              // 当前帧位置（帧 = 2 个采样）
    float     volume = 1.0f;        // 用户音量
    float     env = 1.0f;           // 包络当前值（fade 用）
    float     env_step = 0.0f;      // 每帧包络增量；0 = 无 fade
    bool      loop = false;         // 循环（BGM/BGS）
    bool      remove_at_env_end = false;  // fade out 到 0 后移除
    bool      suspended = false;    // 挂起（ME 播放期间的 BGM）
    VoiceKind kind = VoiceKind::SE;
};

// 便捷：fade 秒数 → 每帧包络增量
inline float FadeStep(float seconds) {
    if (seconds <= 0.0f) return 0.0f;
    return 1.0f / (seconds * (float)kSampleRate);
}

// 把 voices 混进 out（frames 帧，内部先清零），推进 pos/env，移除播完/fade 完的 voice。
void MixVoices(std::vector<Voice>& voices,
               const std::vector<SoundData>& sounds,
               int16_t* out, size_t frames);

} // namespace audio
} // namespace mg
