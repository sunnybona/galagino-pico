#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_PACMAN)
static constexpr FlashAsset<unsigned short[8], COMPRESSED> pacman_tilemap_blob = { nullptr, 1082, 256, ARENA_ANY, 0x1EF925F4u };
#else
// pacman_tilemap: 4096 bytes，住在 ROM 区 0x10181204（0x61204），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned short[8], PLAIN> pacman_tilemap_blob = { (const unsigned short (*)[8])(GAL_ROM_BASE + 0x61204u), 256 };
#endif
