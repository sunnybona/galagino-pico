#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_PACMAN)
static constexpr FlashAsset<unsigned char, COMPRESSED> pacman_rom_blob = { nullptr, 9095, 16384, ARENA_ANY, 0x91E6D484u };
#else
// pacman_rom: 16384 bytes，住在 ROM 区 0x1017C204（0x5c204），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned char, PLAIN> pacman_rom_blob = { (const unsigned char *)(GAL_ROM_BASE + 0x5c204u), 16384 };
#endif
