#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_GALAGA)
static constexpr FlashAsset<unsigned char, COMPRESSED> galaga_rom_cpu2_blob = { nullptr, 3149, 4096, ARENA_ANY, 0xC0B973ACu };
#else
// galaga_rom_cpu2: 4096 bytes，住在 ROM 区 0x10137DC4（0x17dc4），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned char, PLAIN> galaga_rom_cpu2_blob = { (const unsigned char *)(GAL_ROM_BASE + 0x17dc4u), 4096 };
#endif
