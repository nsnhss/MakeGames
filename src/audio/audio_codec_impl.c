// ---------------------------------------------------------------------------
// OGG/MP3 解码器实现单元（以 C 编译；声明部分见 audio_decoder.cpp 的 include）
// stb_vorbis: public domain（OGG Vorbis）
// dr_mp3:     public domain / MIT-0（MP3）
// ---------------------------------------------------------------------------
#define STB_VORBIS_IMPLEMENTATION
#define STB_VORBIS_NO_PUSHDATA_API
#include "third_party/stb_vorbis.c"

#define DR_MP3_IMPLEMENTATION
#include "third_party/dr_mp3.h"
