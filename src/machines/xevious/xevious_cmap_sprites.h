// Xevious colormap sprite, da xvi_4bpr.3l (lut low)/xvi_5bpr.3m (lut high)
// 64 gruppi x 8 pen (3bpp). c&0x80==0 -> pen trasparente (valore 0).
#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_XEVIOUS)
static constexpr FlashAsset<unsigned short[8], COMPRESSED> xevious_colormap_sprites_blob = { nullptr, 329, 64, ARENA_ANY, 0xC75D773Bu };
#else
// xevious_colormap_sprites: 1024 bytes，住在 ROM 区 0x102B0D54（0x190d54），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned short[8], PLAIN> xevious_colormap_sprites_blob = { (const unsigned short (*)[8])(GAL_ROM_BASE + 0x190d54u), 64 };
#endif
