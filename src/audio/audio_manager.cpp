// audio_manager.cpp
// AudioManager — 软件混音音频系统。
// WAV 加载后经 SDL_AudioStream 归一到设备格式（48k/S16/stereo），
// Update() 用 MixVoices 混音所有声部后一次性 SDL_QueueAudio。
#include "audio_manager.h"
#include "audio_decoder.h"
#include <SDL.h>
#include <algorithm>
#include "src/core/log.h"

namespace mg {

namespace {

// 格式归一：任意源 → 48k/S16/stereo（设备格式）
bool ConvertToDeviceFormat(SDL_AudioFormat src_format, int src_freq, int src_channels,
                           const void* buffer, int len, audio::SoundData& out) {
    // SDL_NewAudioStream 参数顺序为 (format, channels, rate)——勿再写反
    SDL_AudioStream* stream = SDL_NewAudioStream(
        src_format, (Uint8)src_channels, src_freq,
        AUDIO_S16SYS, (Uint8)audio::kChannels, audio::kSampleRate);
    if (!stream) {
        MG_LOG_ERROR("[AudioManager] Failed to create audio stream: " << SDL_GetError());
        return false;
    }
    bool ok = false;
    if (SDL_AudioStreamPut(stream, buffer, len) == 0 && SDL_AudioStreamFlush(stream) == 0) {
        int got = SDL_AudioStreamAvailable(stream);
        if (got > 0) {
            out.samples.resize((size_t)got / sizeof(int16_t));
            SDL_AudioStreamGet(stream, out.samples.data(), got);
            ok = !out.samples.empty();
        }
    }
    SDL_FreeAudioStream(stream);
    return ok;
}

} // namespace

AudioManager& AudioManager::Get() {
    static AudioManager instance;
    return instance;
}

bool AudioManager::Init() {
    SDL_AudioSpec desired = {};
    desired.freq = audio::kSampleRate;
    desired.format = AUDIO_S16SYS;
    desired.channels = audio::kChannels;
    desired.samples = 4096;

    SDL_AudioSpec obtained = {};
    device_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (device_ == 0) {
        MG_LOG_ERROR("[AudioManager] Failed to open audio device: " << SDL_GetError());
        return false;
    }
    SDL_PauseAudioDevice(device_, 0); // 开始播放
    return true;
}

void AudioManager::Shutdown() {
    if (device_ != 0) {
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
    voices_.clear();
    sounds_.clear();
    path_to_sound_.clear();
}

size_t AudioManager::LoadSound(const std::string& path) {
    auto it = path_to_sound_.find(path);
    if (it != path_to_sound_.end()) return it->second;

    audio::SoundData data;
    const audio::AudioCodec codec = audio::DetectAudioCodec(path);
    if (codec == audio::AudioCodec::OGG || codec == audio::AudioCodec::MP3) {
        // OGG / MP3：第三方解码为 S16 PCM 后统一格式归一
        std::vector<int16_t> pcm;
        int rate = 0, channels = 0;
        if (!audio::DecodeAudioFile(path, pcm, rate, channels)) return kInvalidSound;
        if (!ConvertToDeviceFormat(AUDIO_S16SYS, rate, channels,
                                   pcm.data(), (int)(pcm.size() * sizeof(int16_t)), data)) {
            MG_LOG_ERROR("[AudioManager] Empty or failed conversion: " << path);
            return kInvalidSound;
        }
    } else {
        // WAV（默认路径）
        SDL_AudioSpec src_spec;
        Uint8* buffer = nullptr;
        Uint32 len = 0;
        if (!SDL_LoadWAV(path.c_str(), &src_spec, &buffer, &len)) {
            MG_LOG_ERROR("[AudioManager] Failed to load WAV: " << path << " (" << SDL_GetError() << ")");
            return kInvalidSound;
        }
        bool ok = ConvertToDeviceFormat(src_spec.format, src_spec.freq, src_spec.channels,
                                        buffer, (int)len, data);
        SDL_FreeWAV(buffer);
        if (!ok) {
            MG_LOG_ERROR("[AudioManager] Empty or failed conversion: " << path);
            return kInvalidSound;
        }
    }

    size_t index = sounds_.size();
    sounds_.push_back(std::move(data));
    path_to_sound_[path] = index;
    return index;
}

audio::Voice* AudioManager::FindVoice(audio::VoiceKind kind) {
    for (auto& v : voices_) {
        if (v.kind == kind) return &v;
    }
    return nullptr;
}

void AudioManager::StopVoiceOfKind(audio::VoiceKind kind, float fade_out_sec) {
    auto* v = FindVoice(kind);
    if (!v) return;
    float step = audio::FadeStep(fade_out_sec);
    if (step <= 0.0f) {
        // 立即移除
        voices_.erase(std::remove_if(voices_.begin(), voices_.end(),
            [kind](const audio::Voice& x) { return x.kind == kind; }), voices_.end());
    } else {
        v->loop = false;
        v->suspended = false;
        v->env_step = -step;
        v->remove_at_env_end = true;
    }
}

void AudioManager::PlaySE(const std::string& path, float volume) {
    size_t idx = LoadSound(path);
    if (idx == kInvalidSound || device_ == 0) return;

    audio::Voice v;
    v.sound_index = idx;
    v.volume = volume;
    v.kind = audio::VoiceKind::SE;
    voices_.push_back(v);
}

void AudioManager::PlayBGM(const std::string& path, float volume, float fade_in_sec) {
    size_t idx = LoadSound(path);
    if (idx == kInvalidSound || device_ == 0) return;

    StopVoiceOfKind(audio::VoiceKind::BGM, 0.0f);   // 旧 BGM 立即移除
    current_bgm_path_ = path;

    audio::Voice v;
    v.sound_index = idx;
    v.volume = volume;
    v.loop = true;
    v.kind = audio::VoiceKind::BGM;
    float step = audio::FadeStep(fade_in_sec);
    if (step > 0.0f) { v.env = 0.0f; v.env_step = step; }
    // ME 播放中则 BGM 先挂起
    if (FindVoice(audio::VoiceKind::ME)) v.suspended = true;
    voices_.push_back(v);
}

void AudioManager::StopBGM(float fade_out_sec) {
    current_bgm_path_.clear();
    StopVoiceOfKind(audio::VoiceKind::BGM, fade_out_sec);
}

void AudioManager::PlayBGS(const std::string& path, float volume, float fade_in_sec) {
    size_t idx = LoadSound(path);
    if (idx == kInvalidSound || device_ == 0) return;

    StopVoiceOfKind(audio::VoiceKind::BGS, 0.0f);

    audio::Voice v;
    v.sound_index = idx;
    v.volume = volume;
    v.loop = true;
    v.kind = audio::VoiceKind::BGS;
    float step = audio::FadeStep(fade_in_sec);
    if (step > 0.0f) { v.env = 0.0f; v.env_step = step; }
    voices_.push_back(v);
}

void AudioManager::StopBGS(float fade_out_sec) {
    StopVoiceOfKind(audio::VoiceKind::BGS, fade_out_sec);
}

void AudioManager::PlayME(const std::string& path, float volume) {
    size_t idx = LoadSound(path);
    if (idx == kInvalidSound || device_ == 0) return;

    StopVoiceOfKind(audio::VoiceKind::ME, 0.0f);
    if (auto* bgm = FindVoice(audio::VoiceKind::BGM)) bgm->suspended = true;

    audio::Voice v;
    v.sound_index = idx;
    v.volume = volume;
    v.kind = audio::VoiceKind::ME;
    voices_.push_back(v);
}

void AudioManager::Update() {
    if (device_ == 0) return;

    // ME 结束后恢复挂起的 BGM
    if (!FindVoice(audio::VoiceKind::ME)) {
        if (auto* bgm = FindVoice(audio::VoiceKind::BGM)) bgm->suspended = false;
    }

    constexpr Uint32 kTargetQueued = 32768;                 // ~170ms @ 48kHz stereo 16bit
    Uint32 queued = SDL_GetQueuedAudioSize(device_);
    if (queued >= kTargetQueued) return;

    size_t bytes_needed = kTargetQueued - queued;
    size_t frames = bytes_needed / (audio::kChannels * sizeof(int16_t));
    if (frames == 0) return;

    if (voices_.empty()) return;   // 静默时不补零数据，让队列自然排空

    mix_buffer_.resize(frames * audio::kChannels);
    audio::MixVoices(voices_, sounds_, mix_buffer_.data(), frames);
    SDL_QueueAudio(device_, mix_buffer_.data(),
                   (Uint32)(mix_buffer_.size() * sizeof(int16_t)));
}

} // namespace mg
