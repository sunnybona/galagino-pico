#include "../../emulation/asset.h"
#include "commando_arena_plan.h"
// arena-asset commando_rom 49152 1 unsigned char
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_COMMANDO)
static constexpr FlashAsset<unsigned char, PLAIN> commando_rom_blob = { nullptr, 49152, 0x9310B69Au };
#else
alignas(4) // commando_rom: 49152 bytes，住在 ROM 区 0x103B4314（0x294314），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned char, PLAIN> commando_rom_blob = { (const unsigned char *)(GAL_ROM_BASE + 0x294314u), 49152 };
#endif
