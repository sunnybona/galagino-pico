#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_DKONG)
static constexpr FlashAsset<unsigned char, COMPRESSED> dkong_rom_cpu1_blob = { nullptr, 11477, 16384, ARENA_ANY, 0x61FAB4A9u };
#else
// dkong_rom_cpu1: 16384 bytes，住在 ROM 区 0x10124A20（0x4a20），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned char, PLAIN> dkong_rom_cpu1_blob = { (const unsigned char *)(GAL_ROM_BASE + 0x4a20u), 16384 };
#endif
