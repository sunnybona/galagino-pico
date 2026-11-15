// Xevious foreground text tiles (xvi_12.3b) — 512 char 8x8 1bpp
// pen0=transparent (mapped via colormap), direct index to fg_videoram
#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_XEVIOUS)
static constexpr FlashAsset<unsigned char[8], COMPRESSED> xevious_fgtilemap_blob = { nullptr, 1800, 512, ARENA_ANY, 0xBA3626A8u };
#else
// xevious_fgtilemap: 4096 bytes，住在 ROM 区 0x102B1154（0x191154），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned char[8], PLAIN> xevious_fgtilemap_blob = { (const unsigned char (*)[8])(GAL_ROM_BASE + 0x191154u), 512 };
#endif
