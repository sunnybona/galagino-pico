// Super Xevious CPU2 (motion) Z80 ROM 0x0000-0x1FFF
#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_SXEVIOUS)
static constexpr FlashAsset<unsigned char, COMPRESSED> sxevious_rom_cpu2_blob = { nullptr, 4592, 8192, ARENA_ANY, 0x81B5373Du };
#else
// sxevious_rom_cpu2: 8192 bytes，住在 ROM 区 0x102D0594（0x1b0594），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned char, PLAIN> sxevious_rom_cpu2_blob = { (const unsigned char *)(GAL_ROM_BASE + 0x1b0594u), 8192 };
#endif
