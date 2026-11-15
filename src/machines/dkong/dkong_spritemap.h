#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_DKONG)
static constexpr FlashAsset<uint32_t[16], COMPRESSED> dkong_sprites_blob = { nullptr, 2742, 128, ARENA_ANY, 0x01326027u };
#else
// dkong_sprites: 8192 bytes，住在 ROM 区 0x1012E3A4（0xe3a4），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<uint32_t[16], PLAIN> dkong_sprites_blob = { (const uint32_t (*)[16])(GAL_ROM_BASE + 0xe3a4u), 128 };
#endif
