#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_DKONG)
static constexpr FlashAsset<unsigned short[8], COMPRESSED> dkong_tilemap_blob = { nullptr, 1196, 256, ARENA_ANY, 0xE3DB1563u };
#else
// dkong_tilemap: 4096 bytes，住在 ROM 区 0x101303A4（0x103a4），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned short[8], PLAIN> dkong_tilemap_blob = { (const unsigned short (*)[8])(GAL_ROM_BASE + 0x103a4u), 256 };
#endif
