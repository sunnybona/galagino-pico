#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_GALAGA)
static constexpr FlashAsset<unsigned char, COMPRESSED> galaga_rom_cpu3_blob = { nullptr, 1831, 4096, ARENA_ANY, 0xC1B9753Fu };
#else
// galaga_rom_cpu3: 4096 bytes，住在 ROM 区 0x10138DC4（0x18dc4），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned char, PLAIN> galaga_rom_cpu3_blob = { (const unsigned char *)(GAL_ROM_BASE + 0x18dc4u), 4096 };
#endif
