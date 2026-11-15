// Xevious CPU1 (main) Z80 ROM 0x0000-0x3FFF (xvi_1.3p+xvi_2.3m+xvi_3.2m+xvi_4.2l)
#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_XEVIOUS)
static constexpr FlashAsset<unsigned char, COMPRESSED> xevious_rom_cpu1_blob = { nullptr, 8687, 16384, ARENA_ANY, 0xA23F736Fu };
#else
// xevious_rom_cpu1: 16384 bytes，住在 ROM 区 0x102B8B74（0x198b74），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned char, PLAIN> xevious_rom_cpu1_blob = { (const unsigned char *)(GAL_ROM_BASE + 0x198b74u), 16384 };
#endif
