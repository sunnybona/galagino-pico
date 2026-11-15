#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_GALAGA)
static constexpr FlashAsset<uint32_t[16], COMPRESSED> galaga_sprites_blob = { nullptr, 3580, 128, ARENA_ANY, 0x82A7A30Bu };
#else
// galaga_sprites: 8192 bytes，住在 ROM 区 0x10139DC4（0x19dc4），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<uint32_t[16], PLAIN> galaga_sprites_blob = { (const uint32_t (*)[16])(GAL_ROM_BASE + 0x19dc4u), 128 };
#endif
