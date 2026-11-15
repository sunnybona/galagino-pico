// 手写的全 PLAIN 版本（原文件由 romconv/romconv/plan/arena.py 生成）。
//
// PicoBoy 上没有 Arena 资产池：1943 的 ROM / 图块 / 精灵 / 调色板全部躺在片内
// flash 的固定偏移区（GAL_ROM_BASE + offset，见 tools/mkromregion.py 生成的头），
// XIP 直读、零解包。所以：
//   - 每个资产都是 <name>_PLAIN = 1，typedef 一律 Asset<T, PLAIN>
//   - ArenaPlan 置零（Arena 只留给运行时缓冲，靠 CMake 的 ARENA_A_SIZE 提供）
#ifndef _1943_ARENA_PLAN_H
#define _1943_ARENA_PLAN_H
#include "../../emulation/asset.h"
#include "../../emulation/arena.h"

static constexpr uint32_t _1943_arena_extras_key = 0u;

#define _1943_bg2_PLAIN 1
using _1943_bg2_asset = Asset<uint32_t[128], PLAIN>;

#define _1943_chars_PLAIN 1
using _1943_chars_asset = Asset<unsigned short[8], PLAIN>;

#define _1943_cmap_bg1_PLAIN 1
using _1943_cmap_bg1_asset = Asset<unsigned short[16], PLAIN>;

#define _1943_cmap_bg2_PLAIN 1
using _1943_cmap_bg2_asset = Asset<unsigned short[16], PLAIN>;

#define _1943_cmap_chars_PLAIN 1
using _1943_cmap_chars_asset = Asset<unsigned short[4], PLAIN>;

#define _1943_cmap_sprites_PLAIN 1
using _1943_cmap_sprites_asset = Asset<unsigned short[16], PLAIN>;

#define _1943_pen_masks_PLAIN 1
using _1943_pen_masks_asset = Asset<unsigned short[16], PLAIN>;

#define _1943_rom_fixed_PLAIN 1
using _1943_rom_fixed_asset = Asset<unsigned char, PLAIN>;

#define _1943_rom_p03_PLAIN 1
using _1943_rom_p03_asset = Asset<unsigned char, PLAIN>;

#define _1943_rom_p47_PLAIN 1
using _1943_rom_p47_asset = Asset<unsigned char, PLAIN>;

#define _1943_sound_PLAIN 1
using _1943_sound_asset = Asset<unsigned char, PLAIN>;

#define _1943_tilemap_PLAIN 1
using _1943_tilemap_asset = Asset<unsigned char, PLAIN>;

static constexpr ArenaPlan _1943_arena_plan = { 0, 0 };
#endif
