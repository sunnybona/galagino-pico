// 手写的全 PLAIN 版本（原文件由 romconv/romconv/plan/arena.py 生成）。
//
// PicoBoy 上没有 Arena 资产池：commando 的 ROM / 三组精灵 / 字符 / 图块 / SID
// 全部躺在片内 flash 的固定偏移区（GAL_ROM_BASE + offset，见
// tools/mkromregion.py 生成的头），XIP 直读、零解包。所以：
//   - 每个资产都是 <name>_PLAIN = 1，typedef 一律 Asset<T, PLAIN>
//   - ArenaPlan 置零（Arena 只留给 commando 自己的 TUNE_RAM（SID 播放器 4 KB），
//     靠 CMake 的 ARENA_A_SIZE 提供）
#ifndef COMMANDO_ARENA_PLAN_H
#define COMMANDO_ARENA_PLAN_H
#include "../../emulation/asset.h"
#include "../../emulation/arena.h"

static constexpr uint32_t commando_arena_extras_key = 0u;

#define commando_chars_PLAIN 1
using commando_chars_asset = Asset<unsigned short[8], PLAIN>;

#define commando_cmap_chars_PLAIN 1
using commando_cmap_chars_asset = Asset<unsigned short[4], PLAIN>;

#define commando_cmap_sprites_PLAIN 1
using commando_cmap_sprites_asset = Asset<unsigned short[16], PLAIN>;

#define commando_cmap_tiles_PLAIN 1
using commando_cmap_tiles_asset = Asset<unsigned short[8], PLAIN>;

#define commando_rom_PLAIN 1
using commando_rom_asset = Asset<unsigned char, PLAIN>;

#define commando_sid_PLAIN 1
using commando_sid_asset = Asset<unsigned char, PLAIN>;

#define commando_sound_PLAIN 1
using commando_sound_asset = Asset<unsigned char, PLAIN>;

#define commando_sprites0_PLAIN 1
using commando_sprites0_asset = Asset<uint32_t[32], PLAIN>;

#define commando_sprites1_PLAIN 1
using commando_sprites1_asset = Asset<uint32_t[32], PLAIN>;

#define commando_sprites2_PLAIN 1
using commando_sprites2_asset = Asset<uint32_t[32], PLAIN>;

#define commando_tiles_a_PLAIN 1
using commando_tiles_a_asset = Asset<uint32_t[32], PLAIN>;

#define commando_tiles_b_PLAIN 1
using commando_tiles_b_asset = Asset<uint32_t[32], PLAIN>;

static constexpr ArenaPlan commando_arena_plan = { 0, 0 };
#endif
