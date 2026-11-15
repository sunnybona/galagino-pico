#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_PACMAN)
static constexpr FlashAsset<uint32_t[16], COMPRESSED> pacman_sprites_blob = { nullptr, 1006, 64, ARENA_ANY, 0x47729674u };
#else
// pacman_sprites: 4096 bytes，住在 ROM 区 0x10180204（0x60204），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<uint32_t[16], PLAIN> pacman_sprites_blob = { (const uint32_t (*)[16])(GAL_ROM_BASE + 0x60204u), 64 };
#endif
