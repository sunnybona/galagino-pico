// Xevious sprites (xvi_15.4m+xvi_17.4p+xvi_16.4n+xvi_18.4r, dopo
// init_xevious() unpack) — 320 sprite 16x16 3bpp (valori pixel 0-7),
// impacchettati a nibble (4 bit) come mappy/gaplus_spritemap.h.
#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_XEVIOUS)
static constexpr FlashAsset<uint32_t[32], COMPRESSED> xevious_sprites_blob = { nullptr, 8677, 320, ARENA_ANY, 0x08C0892Du };
#else
// xevious_sprites: 40960 bytes，住在 ROM 区 0x102BFB74（0x19fb74），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<uint32_t[32], PLAIN> xevious_sprites_blob = { (const uint32_t (*)[32])(GAL_ROM_BASE + 0x19fb74u), 320 };
#endif
