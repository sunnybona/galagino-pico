#include "../../emulation/asset.h"
#include "commando_arena_plan.h"
// arena-asset commando_chars 16384 2 unsigned short[8]
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_COMMANDO)
static constexpr FlashAsset<unsigned short[8], PLAIN> commando_chars_blob = { nullptr, 1024, 0x86B01825u };
#else
alignas(4) // commando_chars: 16384 bytes，住在 ROM 区 0x103AD6F4（0x28d6f4），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned short[8], PLAIN> commando_chars_blob = { (const unsigned short (*)[8])(GAL_ROM_BASE + 0x28d6f4u), 1024 };
#endif
