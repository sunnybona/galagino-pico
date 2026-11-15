#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_GALAGA)
static constexpr FlashAsset<unsigned char, COMPRESSED> galaga_rom_cpu1_blob = { nullptr, 9710, 16384, ARENA_ANY, 0xC3B97865u };
#else
// galaga_rom_cpu1: 16384 bytes，住在 ROM 区 0x10133DC4（0x13dc4），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned char, PLAIN> galaga_rom_cpu1_blob = { (const unsigned char *)(GAL_ROM_BASE + 0x13dc4u), 16384 };
#endif
