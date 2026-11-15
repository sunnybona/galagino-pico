// Xevious planet-map lookup ROM (xvi_9.2a+xvi_10.2b+xvi_11.2c), 0x4000 byte: rom2a@0x0000 rom2b@0x1000 rom2c@0x3000, read via xevious_bb_r()
#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_XEVIOUS)
static constexpr FlashAsset<unsigned char, COMPRESSED> xevious_planetmap_blob = { nullptr, 6105, 16384, ARENA_ANY, 0x31A7D853u };
#else
// xevious_planetmap: 16384 bytes，住在 ROM 区 0x102B4B74（0x194b74），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned char, PLAIN> xevious_planetmap_blob = { (const unsigned char *)(GAL_ROM_BASE + 0x194b74u), 16384 };
#endif
