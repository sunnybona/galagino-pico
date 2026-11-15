// Xevious CPU3 (sound) Z80 ROM 0x0000-0x0FFF (xvi_7.2c)
#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_XEVIOUS)
static constexpr FlashAsset<unsigned char, COMPRESSED> xevious_rom_cpu3_blob = { nullptr, 1268, 4096, ARENA_ANY, 0xA43F7695u };
#else
// xevious_rom_cpu3: 4096 bytes，住在 ROM 区 0x102BEB74（0x19eb74），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned char, PLAIN> xevious_rom_cpu3_blob = { (const unsigned char *)(GAL_ROM_BASE + 0x19eb74u), 4096 };
#endif
