// Super Xevious CPU1 (main) Z80 ROM 0x0000-0x3FFF
#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_SXEVIOUS)
static constexpr FlashAsset<unsigned char, COMPRESSED> sxevious_rom_cpu1_blob = { nullptr, 8738, 16384, ARENA_ANY, 0x7EB53284u };
#else
// sxevious_rom_cpu1: 16384 bytes，住在 ROM 区 0x102CC594（0x1ac594），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned char, PLAIN> sxevious_rom_cpu1_blob = { (const unsigned char *)(GAL_ROM_BASE + 0x1ac594u), 16384 };
#endif
