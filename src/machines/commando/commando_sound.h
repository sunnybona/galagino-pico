#include "../../emulation/asset.h"
#include "commando_arena_plan.h"
// arena-asset commando_sound 16384 1 unsigned char
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_COMMANDO)
static constexpr FlashAsset<unsigned char, PLAIN> commando_sound_blob = { nullptr, 16384, 0xD518EAC7u };
#else
alignas(4) // commando_sound: 16384 bytes，住在 ROM 区 0x103C135C（0x2a135c），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned char, PLAIN> commando_sound_blob = { (const unsigned char *)(GAL_ROM_BASE + 0x2a135cu), 16384 };
#endif
