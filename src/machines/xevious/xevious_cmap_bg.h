// Xevious colormap sfondo, da xvi_7bpr.4h (lut low)/xvi_6bpr.4f (lut high)
// 128 gruppi x 4 pen (2bpp), layer OPACO -> nessuna trasparenza
#include "../../emulation/asset.h"
#if defined(ASSETS_REMOTE) && !defined(BUILTIN_XEVIOUS)
static constexpr FlashAsset<unsigned short[4], COMPRESSED> xevious_colormap_bg_blob = { nullptr, 201, 128, ARENA_ANY, 0x44FA9F6Cu };
#else
// xevious_colormap_bg: 1024 bytes，住在 ROM 区 0x102B0954（0x190954），XIP 直读
// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const
// （启动时初始化），不是 constexpr。
static const FlashAsset<unsigned short[4], PLAIN> xevious_colormap_bg_blob = { (const unsigned short (*)[4])(GAL_ROM_BASE + 0x190954u), 128 };
#endif
