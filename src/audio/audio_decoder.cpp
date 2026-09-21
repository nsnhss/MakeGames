// audio_decoder.cpp — OGG/MP3 → 交错 S16 PCM
// 声明部分直接 include 第三方源（extern "C" 保护），实现编译于 audio_codec_impl.c。
#include "audio_decoder.h"
#include "src/core/log.h"
#include <algorithm>
#include <cctype>

#define STB_VORBIS_HEADER_ONLY
#include "third_party/stb_vorbis.c"   // 声明模式（实现编译于 audio_codec_impl.c）
#undef STB_VORBIS_HEADER_ONLY
#include "third_party/dr_mp3.h"       // 声明模式（未定义 DR_MP3_IMPLEMENTATION）

namespace mg {
namespace audio {

AudioCodec DetectAudioCodec(const std::string& path) {
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return AudioCodec::Unknown;
    std::string ext = path.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    if (ext == "wav") return AudioCodec::WAV;
    if (ext == "ogg") return AudioCodec::OGG;
    if (ext == "mp3") return AudioCodec::MP3;
    return AudioCodec::Unknown;
}

static bool DecodeOGG(const std::string& path, std::vector<int16_t>& out_pcm,
                      int& out_rate, int& out_channels) {
    int err = 0;
    stb_vorbis* v = stb_vorbis_open_filename(path.c_str(), &err, nullptr);
    if (!v) {
        MG_LOG_ERROR("[AudioDecoder] stb_vorbis open failed (" << err << "): " << path);
        return false;
    }
    stb_vorbis_info info = stb_vorbis_get_info(v);
    out_rate = (int)info.sample_rate;
    out_channels = (int)info.channels;

    // 分块解码，避免一次性分配超大缓冲
    constexpr int kChunkShorts = 16384;
    std::vector<int16_t> buf(kChunkShorts);
    for (;;) {
        int got = stb_vorbis_get_samples_short_interleaved(
            v, out_channels, buf.data(), kChunkShorts);
        if (got <= 0) break;
        out_pcm.insert(out_pcm.end(), buf.data(), buf.data() + got * out_channels);
    }
    stb_vorbis_close(v);
    if (out_pcm.empty()) {
        MG_LOG_ERROR("[AudioDecoder] OGG decode produced no samples: " << path);
        return false;
    }
    return true;
}

static bool DecodeMP3(const std::string& path, std::vector<int16_t>& out_pcm,
                      int& out_rate, int& out_channels) {
    drmp3_config config = {};
    drmp3_uint64 frame_count = 0;
    drmp3_int16* pcm = drmp3_open_file_and_read_pcm_frames_s16(
        path.c_str(), &config, &frame_count, nullptr);
    if (!pcm || frame_count == 0) {
        MG_LOG_ERROR("[AudioDecoder] dr_mp3 decode failed: " << path);
        if (pcm) drmp3_free(pcm, nullptr);
        return false;
    }
    out_rate = (int)config.sampleRate;
    out_channels = (int)config.channels;
    size_t total = (size_t)frame_count * (size_t)out_channels;
    out_pcm.assign(pcm, pcm + total);
    drmp3_free(pcm, nullptr);
    return true;
}

bool DecodeAudioFile(const std::string& path, std::vector<int16_t>& out_pcm,
                     int& out_sample_rate, int& out_channels) {
    switch (DetectAudioCodec(path)) {
        case AudioCodec::OGG: return DecodeOGG(path, out_pcm, out_sample_rate, out_channels);
        case AudioCodec::MP3: return DecodeMP3(path, out_pcm, out_sample_rate, out_channels);
        default: return false;
    }
}

} // namespace audio
} // namespace mg
