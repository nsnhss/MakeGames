#include "audio_mixer.h"
#include <algorithm>
#include <cstring>

namespace mg {
namespace audio {

void MixVoices(std::vector<Voice>& voices,
               const std::vector<SoundData>& sounds,
               int16_t* out, size_t frames)
{
    std::memset(out, 0, frames * kChannels * sizeof(int16_t));

    for (auto& v : voices) {
        if (v.suspended) continue;
        if (v.sound_index >= sounds.size()) { v.env = 0.0f; v.remove_at_env_end = true; continue; }
        const auto& samples = sounds[v.sound_index].samples;
        const size_t total_frames = samples.size() / kChannels;
        if (total_frames == 0) { v.remove_at_env_end = true; v.env = 0.0f; }

        for (size_t f = 0; f < frames && (v.env > 0.0f || v.env_step > 0.0f); ++f) {
            if (v.pos >= total_frames) {
                if (v.loop) v.pos = 0;
                else break;   // 播完
            }
            if (v.pos >= total_frames) break;   // loop 且空音源

            float gain = v.volume * v.env;
            for (int c = 0; c < kChannels; ++c) {
                int32_t mixed = (int32_t)out[f * kChannels + c]
                              + (int32_t)(samples[v.pos * kChannels + c] * gain);
                out[f * kChannels + c] = (int16_t)std::clamp(mixed, -32768, 32767);
            }
            ++v.pos;

            // 包络推进
            if (v.env_step != 0.0f) {
                v.env += v.env_step;
                if (v.env >= 1.0f && v.env_step > 0.0f) { v.env = 1.0f; v.env_step = 0.0f; }
                if (v.env <= 0.0f && v.env_step < 0.0f) { v.env = 0.0f; break; }
            }
        }
    }

    // 移除播完（非 loop）或 fade out 到 0 的 voice
    voices.erase(std::remove_if(voices.begin(), voices.end(),
        [&](const Voice& v) {
            if (v.suspended) return false;
            if (v.sound_index >= sounds.size()) return true;
            const size_t total_frames = sounds[v.sound_index].samples.size() / kChannels;
            if (v.env <= 0.0f && v.remove_at_env_end) return true;
            if (!v.loop && v.pos >= total_frames) return true;
            return false;
        }), voices.end());
}

} // namespace audio
} // namespace mg
