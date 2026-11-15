#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_DKONG)
static constexpr FlashAsset<unsigned char, COMPRESSED> dkong_rom_cpu2_blob = { nullptr, 2808, 4096, ARENA_ANY, 0x5EFAAFF0u };
#else
// dkong_rom_cpu2: 4096 bytes，住在 ROM 区 0x10128A20（0x8a20），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned char, PLAIN> dkong_rom_cpu2_blob = { (const unsigned char *)(GAL_ROM_BASE + 0x8a20u), 4096 };
#endif
