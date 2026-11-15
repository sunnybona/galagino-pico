#include "../../emulation/asset.h"
#include "commando_arena_plan.h"
// arena-asset commando_sprites1 32768 4 uint32_t[32]
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_COMMANDO)
static constexpr FlashAsset<uint32_t[32], PLAIN> commando_sprites1_blob = { nullptr, 256, 0x78A51765u };
#else
alignas(4) // commando_sprites1: 32768 bytes，住在 ROM 区 0x103CD35C（0x2ad35c），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<uint32_t[32], PLAIN> commando_sprites1_blob = { (const uint32_t (*)[32])(GAL_ROM_BASE + 0x2ad35cu), 256 };
#endif
