#include "test_framework.h"
#include "audio/audio_mixer.h"
#include "audio/audio_decoder.h"
#include <vector>

using namespace mg::audio;

// 构造恒定采样值的立体声 SoundData
static SoundData MakeSound(size_t frames, int16_t value) {
    SoundData s;
    s.samples.assign(frames * kChannels, value);
    return s;
}

// 单 voice 音量缩放：10000 × 0.5 = 5000
MG_TEST(mixer_single_voice_volume) {
    std::vector<SoundData> sounds = { MakeSound(4, 10000) };
    std::vector<Voice> voices;
    Voice v;
    v.sound_index = 0;
    v.volume = 0.5f;
    voices.push_back(v);

    int16_t out[8] = {};
    MixVoices(voices, sounds, out, 4);

    MG_EXPECT_EQ(out[0], 5000);
    MG_EXPECT_EQ(out[1], 5000);
    // 一次性 voice 播完移除
    MG_EXPECT(voices.empty());
}

// 两 voice 叠加 + int16 截断防爆音（20000+20000 = 40000 → clamp 32767）
MG_TEST(mixer_two_voices_clamp) {
    std::vector<SoundData> sounds = { MakeSound(2, 20000), MakeSound(2, 20000) };
    std::vector<Voice> voices;
    Voice a; a.sound_index = 0; voices.push_back(a);
    Voice b; b.sound_index = 1; voices.push_back(b);

    int16_t out[4] = {};
    MixVoices(voices, sounds, out, 2);

    MG_EXPECT_EQ(out[0], 32767);
    MG_EXPECT(voices.empty());
}

// fade in：env 从 0 线性升，首帧接近 0，播满 fade 时长后 env=1
MG_TEST(mixer_fade_in_envelope) {
    std::vector<SoundData> sounds = { MakeSound(kSampleRate * 2, 10000) };
    std::vector<Voice> voices;
    Voice v;
    v.sound_index = 0;
    v.loop = true;
    v.env = 0.0f;
    v.env_step = FadeStep(1.0f);   // 1 秒淡入
    voices.push_back(v);

    std::vector<int16_t> out(kSampleRate * kChannels);
    MixVoices(voices, sounds, out.data(), kSampleRate);   // 混 1 秒

    // 第一帧增益接近 0，最后一帧接近满幅
    MG_EXPECT(out[0] < 100);
    MG_EXPECT(out[(kSampleRate - 1) * kChannels] > 9900);
    MG_EXPECT_EQ(voices[0].env, 1.0f);
    MG_EXPECT_EQ(voices[0].env_step, 0.0f);   // fade 完成后 step 清零
}

// fade out + remove_at_env_end：env 到 0 后 voice 被移除
MG_TEST(mixer_fade_out_removes_voice) {
    std::vector<SoundData> sounds = { MakeSound(kSampleRate * 2, 10000) };
    std::vector<Voice> voices;
    Voice v;
    v.sound_index = 0;
    v.loop = true;                       // loop 也应被 fade out 终止
    v.env = 1.0f;
    v.env_step = -FadeStep(0.5f);        // 0.5 秒淡出
    v.remove_at_env_end = true;
    voices.push_back(v);

    std::vector<int16_t> out(kSampleRate * kChannels);
    MixVoices(voices, sounds, out.data(), kSampleRate);   // 混 1 秒，远超 fade 时长

    MG_EXPECT(voices.empty());           // fade 结束后移除
    // 淡出完成后输出为静音
    MG_EXPECT_EQ(out[(kSampleRate - 1) * kChannels], 0);
}

// loop voice 回绕且不被移除
MG_TEST(mixer_loop_wraps) {
    std::vector<SoundData> sounds = { MakeSound(4, 8000) };
    std::vector<Voice> voices;
    Voice v;
    v.sound_index = 0;
    v.loop = true;
    voices.push_back(v);

    int16_t out[20] = {};
    MixVoices(voices, sounds, out, 10);  // 10 帧 > 4 帧音源

    MG_EXPECT_EQ(out[18], 8000);         // 回绕后仍有声
    MG_EXPECT_EQ(voices.size(), 1u);     // loop 不移除
    MG_EXPECT_EQ(voices[0].pos, 2u);     // 10 % 4 = 2
}

// suspended voice 不发声不推进
MG_TEST(mixer_suspended_voice_silent) {
    std::vector<SoundData> sounds = { MakeSound(4, 10000) };
    std::vector<Voice> voices;
    Voice v;
    v.sound_index = 0;
    v.suspended = true;
    voices.push_back(v);

    int16_t out[8] = {};
    MixVoices(voices, sounds, out, 4);

    MG_EXPECT_EQ(out[0], 0);
    MG_EXPECT_EQ(voices[0].pos, 0u);     // 位置未推进
    MG_EXPECT_EQ(voices.size(), 1u);     // 挂起不移除
}

// FadeStep：0/负秒数无 fade
MG_TEST(mixer_fade_step_edge) {
    MG_EXPECT_EQ(FadeStep(0.0f), 0.0f);
    MG_EXPECT_EQ(FadeStep(-1.0f), 0.0f);
    MG_EXPECT(FadeStep(1.0f) > 0.0f);
}

// ---- 音频格式分派（DetectAudioCodec 纯逻辑）----

MG_TEST(audio_codec_detect_dispatch) {
    using mg::audio::DetectAudioCodec;
    using mg::audio::AudioCodec;

    MG_EXPECT(DetectAudioCodec("bgm/theme.ogg") == AudioCodec::OGG);
    MG_EXPECT(DetectAudioCodec("bgm/theme.OGG") == AudioCodec::OGG);   // 大小写不敏感
    MG_EXPECT(DetectAudioCodec("se/sword.mp3") == AudioCodec::MP3);
    MG_EXPECT(DetectAudioCodec("se/sword.Mp3") == AudioCodec::MP3);
    MG_EXPECT(DetectAudioCodec("se/click.wav") == AudioCodec::WAV);
    MG_EXPECT(DetectAudioCodec("se/click.WAV") == AudioCodec::WAV);
    MG_EXPECT(DetectAudioCodec("no_extension") == AudioCodec::Unknown);
    MG_EXPECT(DetectAudioCodec("a/b.flac") == AudioCodec::Unknown);
    MG_EXPECT(DetectAudioCodec("") == AudioCodec::Unknown);
    // 目录名中的点不应误判
    MG_EXPECT(DetectAudioCodec("dir.name/file") == AudioCodec::Unknown);
}

MG_TEST(audio_codec_decode_rejects_wav) {
    // WAV 不在 OGG/MP3 解码路径内（走 SDL_LoadWAV），DecodeAudioFile 应返回 false
    std::vector<int16_t> pcm;
    int rate = 0, channels = 0;
    MG_EXPECT(!mg::audio::DecodeAudioFile("anything.wav", pcm, rate, channels));
    MG_EXPECT(!mg::audio::DecodeAudioFile("anything.flac", pcm, rate, channels));
}
