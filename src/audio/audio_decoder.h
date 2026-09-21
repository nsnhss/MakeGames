#pragma once
// ---------------------------------------------------------------------------
// 音频文件解码（OGG 经 stb_vorbis / MP3 经 dr_mp3；WAV 走 SDL_LoadWAV 旧路径）
// 解码产物：交错 PCM S16 + 采样率 + 声道数，由 AudioManager 统一格式归一。
// DetectAudioCodec 为纯逻辑（扩展名分派），可单测。
// ---------------------------------------------------------------------------
#include <cstdint>
#include <string>
#include <vector>

namespace mg {
namespace audio {

enum class AudioCodec { WAV, OGG, MP3, Unknown };

// 按扩展名分派（大小写不敏感，无扩展名 → Unknown）
AudioCodec DetectAudioCodec(const std::string& path);

// 解码 OGG/MP3 为交错 S16 PCM；成功返回 true。WAV/未知格式返回 false（调用方走 SDL_LoadWAV）。
bool DecodeAudioFile(const std::string& path, std::vector<int16_t>& out_pcm,
                     int& out_sample_rate, int& out_channels);

} // namespace audio
} // namespace mg
