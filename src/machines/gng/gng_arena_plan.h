// 手写的全 PLAIN 版本（原文件由 romconv/romconv/plan/arena.py 生成）。
//
// PicoBoy 上没有 Arena 资产池：gng 的 ROM / 图块 / 精灵 / SID 全部躺在片内 flash
// 的固定偏移区（GAL_ROM_BASE + offset，见 tools/mkromregion.py 生成的头文件），
// XIP 直读、零解包。所以：
//   - 每个资产都是 <name>_PLAIN = 1，typedef 一律 Asset<T, PLAIN>
//   - ArenaPlan 置零（Arena 只留给 gng 自己的运行时缓冲，靠 CMake 的
//     ARENA_A_SIZE 提供）
#ifndef GNG_ARENA_PLAN_H
#define GNG_ARENA_PLAN_H
#include "../../emulation/asset.h"
#include "../../emulation/arena.h"

static constexpr uint32_t gng_arena_extras_key = 0u;

#define gng_bank_PLAIN 1
using gng_bank_asset = Asset<unsigned char, PLAIN>;

#define gng_chars_PLAIN 1
using gng_chars_asset = Asset<unsigned short[8], PLAIN>;

#define gng_rom_PLAIN 1
using gng_rom_asset = Asset<unsigned char, PLAIN>;

#define gng_sid_PLAIN 1
using gng_sid_asset = Asset<unsigned char, PLAIN>;

#define gng_sound_PLAIN 1
using gng_sound_asset = Asset<unsigned char, PLAIN>;

#define gng_sprites0_PLAIN 1
using gng_sprites0_asset = Asset<uint32_t[32], PLAIN>;

#define gng_sprites1_PLAIN 1
using gng_sprites1_asset = Asset<uint32_t[32], PLAIN>;

#define gng_sprites2_PLAIN 1
using gng_sprites2_asset = Asset<uint32_t[32], PLAIN>;

#define gng_tile_map_PLAIN 1
using gng_tile_map_asset = Asset<unsigned short, PLAIN>;

#define gng_tiles_PLAIN 1
using gng_tiles_asset = Asset<uint32_t[32], PLAIN>;

static constexpr ArenaPlan gng_arena_plan = { 0, 0 };
#endif
