// Xevious CPU2 (motion) Z80 ROM 0x0000-0x1FFF (xvi_5.3f+xvi_6.3j)
#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_XEVIOUS)
static constexpr FlashAsset<unsigned char, COMPRESSED> xevious_rom_cpu2_blob = { nullptr, 4441, 8192, ARENA_ANY, 0xA33F7502u };
#else
// xevious_rom_cpu2: 8192 bytes，住在 ROM 区 0x102BCB74（0x19cb74），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned char, PLAIN> xevious_rom_cpu2_blob = { (const unsigned char *)(GAL_ROM_BASE + 0x19cb74u), 8192 };
#endif
