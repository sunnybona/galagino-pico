// 手写的全 PLAIN 版本（原文件由 romconv/romconv/plan/arena.py 生成，按 7 套
// 布局给出 Arena 切分；PicoBoy 上不用 Arena 装资产，故整体替换）。
//
// PicoBoy 上没有 Arena 资产池：contra 的全部 ROM / 图块 / LUT / SID 都躺在片内
// flash 的固定偏移区（GAL_ROM_BASE + offset，见 tools/mkromregion.py 生成的
// 头文件），XIP 直读、零解包。所以：
//   - 每个资产都是 <name>_PLAIN = 1，typedef 一律 Asset<T, PLAIN>
//   - ArenaPlan 置零（菜单不为它预留资产块；contra 自己的运行时缓冲仍由
//     Arena::alloc/create 申请，靠 CMake 的 ARENA_A_SIZE 提供）
#ifndef CONTRA_ARENA_PLAN_H
#define CONTRA_ARENA_PLAN_H
#include "../../emulation/asset.h"
#include "../../emulation/arena.h"

static constexpr uint32_t contra_arena_extras_key = 0u;

#define contra_lut_PLAIN 1
using contra_lut_asset = Asset<unsigned char, PLAIN>;

#define contra_rom_hi_PLAIN 1
using contra_rom_hi_asset = Asset<unsigned char, PLAIN>;

#define contra_rom_lo_PLAIN 1
using contra_rom_lo_asset = Asset<unsigned char, PLAIN>;

#define contra_sid_PLAIN 1
using contra_sid_asset = Asset<unsigned char, PLAIN>;

#define contra_tile_index_PLAIN 1
using contra_tile_index_asset = Asset<unsigned short, PLAIN>;

#define contra_tiles_PLAIN 1
using contra_tiles_asset = Asset<uint32_t[8], PLAIN>;

static constexpr ArenaPlan contra_arena_plan = { 0, 0 };
#endif
