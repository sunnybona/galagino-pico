#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_GALAGA)
static constexpr FlashAsset<unsigned short[8], COMPRESSED> galaga_tilemap_blob = { nullptr, 1617, 256, ARENA_ANY, 0x02D3DF77u };
#else
// galaga_tilemap: 4096 bytes，住在 ROM 区 0x1013BDC4（0x1bdc4），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned short[8], PLAIN> galaga_tilemap_blob = { (const unsigned short (*)[8])(GAL_ROM_BASE + 0x1bdc4u), 256 };
#endif
