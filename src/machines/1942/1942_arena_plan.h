// 手写的全 PLAIN 版本（原文件由 romconv/romconv/plan/arena.py 生成）。
//
// PicoBoy 上没有 Arena：1942 的全部资产都躺在片内 flash 的固定偏移区
// （GAL_ROM_BASE + offset，见 tools/mkromregion.py 生成的头文件），
// XIP 直读、零拷贝、零解包，所以：
//   - 每个资产都是 _<name>_PLAIN = 1，typedef 一律 Asset<T, PLAIN>
//   - ArenaPlan 置零（菜单不再为它预留 Arena）
#ifndef _1942_ARENA_PLAN_H
#define _1942_ARENA_PLAN_H
#include "../../emulation/asset.h"
#include "../../emulation/arena.h"

static constexpr uint32_t _1942_arena_extras_key = 0u;

#define _1942_charmap_PLAIN 1
using _1942_charmap_asset = Asset<unsigned short[8], PLAIN>;

#define _1942_rom_cpu1_PLAIN 1
using _1942_rom_cpu1_asset = Asset<unsigned char, PLAIN>;

#define _1942_rom_cpu1_b0_PLAIN 1
using _1942_rom_cpu1_b0_asset = Asset<unsigned char, PLAIN>;

#define _1942_rom_cpu1_b1_PLAIN 1
using _1942_rom_cpu1_b1_asset = Asset<unsigned char, PLAIN>;

#define _1942_rom_cpu1_b2_PLAIN 1
using _1942_rom_cpu1_b2_asset = Asset<unsigned char, PLAIN>;

#define _1942_rom_cpu2_PLAIN 1
using _1942_rom_cpu2_asset = Asset<unsigned char, PLAIN>;

#define _1942_sprites_PLAIN 1
using _1942_sprites_asset = Asset<uint32_t[32], PLAIN>;

#define _1942_colormap_tiles_PLAIN 1
using _1942_colormap_tiles_asset = Asset<unsigned short[32][8], PLAIN>;

#define _1942_tilemap_PLAIN 1
using _1942_tilemap_asset = Asset<uint32_t[32], PLAIN>;

static constexpr ArenaPlan _1942_arena_plan = { 0, 0 };
#endif
