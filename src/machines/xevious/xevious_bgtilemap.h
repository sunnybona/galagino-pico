// Xevious background tiles (xvi_13.3c+xvi_14.3d) — 512 tile 8x8 2bpp
// no transparency: layer bg is OPAQUE (drawn first)
#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_XEVIOUS)
static constexpr FlashAsset<unsigned short[8], COMPRESSED> xevious_bgtilemap_blob = { nullptr, 3375, 512, ARENA_ANY, 0xA17E553Cu };
#else
// xevious_bgtilemap: 8192 bytes，住在 ROM 区 0x102AE954（0x18e954），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned short[8], PLAIN> xevious_bgtilemap_blob = { (const unsigned short (*)[8])(GAL_ROM_BASE + 0x18e954u), 512 };
#endif
