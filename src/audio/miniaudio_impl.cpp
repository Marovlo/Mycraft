// miniaudio_impl.cpp
// miniaudio 库的实现文件，整个项目只编译一次

// stb_vorbis 必须在 miniaudio 之前 include，
// 这样 miniaudio 检测到 STB_VORBIS_INCLUDE_STB_VORBIS_H 后会启用内置 Vorbis 解码
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// stb_vorbis 的实现部分（在 miniaudio 之后展开，避免重复定义）
#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
