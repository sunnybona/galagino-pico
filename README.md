# galagino → PicoBoy（RP2040）全幅移植说明

把 `galagino-max`（Z80 街机模拟器）移植到 PicoBoy 掌机。板级支持包用
`E:\PicoBoy\bsp`（PicoBoy-BSP 2.0，**已新增 240×320 面板选项**）。
方案 A：只用板载外设（ST7789 + PWM 喇叭 + 8 个板载按键），不接外接 I2C 手柄。

> **本仓库是公开发布版**：只有代码与转换工具，**不含任何 ROM 数据、不含固件二进制**。
>
> 原因有两条：一是上游 galagino 是 GPLv3，其 `NOTICE.md` 明确写了“Z80 core 的许可证禁止
> 分发、不要分发固件二进制”；二是各机型的 `*_rom.h` / `*_tilemap.h` / `*_spritemap.h` /
> `*_logo.h` 是 `romconv` 从商业街机 ROM 生成的数据，属于受版权保护的内容，上游本来就把它们
> 排除在源码库之外。
>
> 所以要得到能烧的固件，需要自己准备 ROM 包，然后按 README 里的流水线跑：
> `convert.py` → `plain_assets.py` → `mkromregion.py extract` → `ninja` → `mkromregion.py merge`。
> 图形化版本：`run_studio.bat`（`tools\pico_arcade_studio.py`）。

## 一、这次改了什么（240×240 → 240×320 全幅）

| | 上一版（240×240 面板） | 这一版（240×320 面板，全幅） |
| --- | --- | --- |
| 画面几何 | galagino 的 240×288 压进 240×240（每 6 行丢 1 行，垂直缩 5/6） | **1:1 无缩放、无裁剪**：窗口 (8, 16, 宽, 288)，和 ESP 版几何完全一致 |
| 帧缓冲 | 用 BSP 的 `pb_fb`（115 KB）整帧刷 | **不用帧缓冲**：每条 8 行条带直接 `pb_display_push_rect()` 上屏 |
| RAM | .bss 125 KB（光帧缓冲就 115 KB） | **.bss 18 KB**（含 3.8 KB 条带转存缓冲） |
| 上屏节奏 | 全速 60 Hz | 半速 30 Hz（一屏 129 KB @62.5 MHz ≈ 16.5 ms，刚好顶满 60 Hz 帧预算） |
| 画布 | `CANVAS_H=288` | `CANVAS_W=240, CANVAS_H=320`（面板全尺寸） |

**为了零帧缓冲，给 BSP 加了两个直推接口**（`E:\PicoBoy\bsp`，只加不改原有逻辑）：

```c
/* 绝对面板坐标、不经帧缓冲，把 (x,y,w,h) 的 RGB565 按行优先推给面板，宽度须为偶数 */
void pb_display_push_rect(int x, int y, int w, int h, const uint16_t *rgb565);
/* 改 MADCTL：0x40 左右镜像 / 0x80 上下镜像 / 0xC0 旋转 180°（纵向游戏翻转用） */
void pb_display_set_madctl(uint8_t madctl);
```

有了 `push_rect`，模拟器可以像 ESP 版那样"边渲染边按条带上屏"，不必先在 RAM 里
攒一整帧——264 KB 的 RP2040 因此腾出了 100 KB 以上给模拟器。

## 二、当前状态

| 项目 | 状态 |
| --- | --- |
| 编译 | **通过**（Pico SDK 1.5.1 + arm-none-eabi-gcc 10.3.1，`BUILD_EXIT=0`） |
| 产物 | `dist/galagino_arcade.uf2`（387,072 B） |
| Flash 占用 | text 193,460 B ≈ 189 KB / 4 MB |
| RAM 占用 | .bss 17,980 B；运行时另加 Arena 43 KB + RAMSIZE 16 KB + 栈 ≈ 85 KB / 264 KB |
| 已启用游戏 | GALAGA（小蜜蜂）、DKONG（大金刚），资产编译在固件里 |
| 真机 | **未烧录**（画面/按键/声音未实测） |
| ROM 走 flash tar 区 | **未接**（下一步，做法同 picoboy-nes） |

## 三、关键配置（`CMakeLists.txt` 里，全部可改）

| 宏 | 值 | 含义 |
| --- | --- | --- |
| `PB_SCREEN_MODE` | `1` | 选 240×320 面板（`0` 是 240×240） |
| `PB_LCD_ROTATE` | `0` | 竖屏原生。galagino 画布就是竖的 240×320，所以不要横屏 |
| `PB_FB_W` / `PB_FB_H` | `224` / `16` | BSP 帧缓冲，本移植不用它，缩到 2 条省 RAM |
| `TFT_SPICLK` | `62500000` | <80 MHz 时 config.h 自动开 `VIDEO_HALF_RATE`（30 Hz 刷屏） |
| `ARENA_A_SIZE` / `B` | 8192 / 0 | 资产走 flash 直读，Arena 只剩菜单鼓形查表 |

面板方向或反色不对时：`-DPB_LCD_ROTATE=1`（横屏）、`-DPB_LCD_INVERT=0`（不反色）、
`-DPB_LCD_X_OFFSET/-DPB_LCD_Y_OFFSET`（GRAM 原点偏移，多数面板是 0）。

## 四、怎么编、怎么烧

```powershell
cd C:\Users\bobo\Doubao\chats\2026-11-14\new-chat-2\galagino-pico
.\build.ps1            # 自动找 Pico SDK / cmake / ninja / arm-gcc
.\build.ps1 -Clean     # 换宏或换面板后建议先清
```
产物：`build\galagino_arcade.uf2`（也复制到 `dist\`）。烧录：按住 BOOTSEL 插 USB，
把 uf2 拖进 RPI-RP2 盘即可。

## 五、视频/音频链路

```
galagino 每屏 36 条（宽×8）
  → PicoboyVideo::submit()       字节序转回（galagino 存的是字节交换过的 RGB565）
  → pb_display_push_rect()       窗口 (8,16+条*8, 宽, 8) + PIO/DMA，阻塞到推完
  → ST7789                       面板 240×320，画面居中，上下各 16 行黑边

音频：galagino 24 kHz mono → audio_pwm.cpp 线性重采样 → pb_audio_submit()
      （BSP 的 PWM 环形缓冲，载波 61,035 Hz = 250 MHz / 2^12）
按键：BSP 高有效（外部下拉）→ hal/rp2040/gpio.cpp 取反，适配 galagino 低有效语义
      A=START  B=COIN  X=FIRE  Y=空闲  方向 = UP/DOWN/LEFT/RIGHT
```

半速说明：`screenParts()` 返回 2，`main.cpp` 把一屏拆成两半（各 18 条），
前半在帧起点后 16 ms、后半 33 ms 放开——面板 30 Hz 刷新，模拟器仍跑 60 Hz，
音画不会因为 SPI 带宽不够而漂。

## 六、资产区：ROM 不进固件，flash 固定偏移 XIP 直读（十二款）

目标形态与 `picoboy-nes` 一致：ROM/图形/采样/logo 不在固件里、也不压缩，
而是躺在一片固定偏移的 flash 区，运行时用 XIP 指针直接读。16 MB 板取

```
GAL_ROM_BASE = 0x10120000     // 片内 flash 偏移 0x120000
固件 0x10000000 .. 0x10036FD4        （实测 225,300 B，远低于 0x120000）
资产区 0x10120000 .. 0x1041FD7C     （3,145,084 B / 100 个资产：dkong + galaga + 1942 + pacman + yiear + contra + gng + xevious + sxevious + 1943 + commando + gberet）
```

当前编进去的游戏（十二款）：`dkong`、`galaga`、`1942`（双 Z80 + 双 AY8910）、`pacman`、`yiear`、`contra`、`gng`、`xevious`（三 Z80 + Namco WSG，1982 原版）、`sxevious`（继承 xevious，只换主/副 CPU ROM）、`1943`、`commando`、`gberet`（Green Beret，Konami 街机原作）。

> `sxevious` 是 Super Xevious，画面/调色板/精灵/行星地图全部复用 `xevious` 的资产（同一套 `xvi-*` ROM），只有主/副 CPU ROM 换成自己的 `xv3_*`/`cpu_*`。基类 `xevious.cpp` 同时被 `xevious` 本体（`GAME_XEVIOUS`，菜单独立一项）和 `sxevious` 使用。

> `gberet`（Green Beret）是 Konami 的 240×224 竖版街机，3.072 MHz Z80 + **单颗 SN76489 PSG**（不是 AY8910），走 `Sn76489::renderBank`，这是本移植第一次真用 SN76489 通路。画面 ROT0、面板上 1:1 居中显示。它的三个大头（Z80 ROM 49,152 B、tilemap 16,384 B、spritemap 65,536 B）用 `AnyAsset`/`AssetRef` 同时接受压缩与 PLAIN 两种 blob，改成 PLAIN 后 `data()` 直接是 flash 指针，不吃 Arena；`tile_cmap/spr_cmap/tile_transp/spr_transp` 是 16×16 的 PROGMEM 常量表（各 512 B），量太小，仍留在固件里。ROM 包用的是 parent 集（`577l01/02/03`、`577h09/10/11`、`577l04~08`），`gberetb`（bootleg）与 `rushatck`（美版 Rush'n Attack）成员不在 job 里。

> `1943` 走的是 **1943b bootleg**（没有 i8751 保护 MCU 的那套）：主 ROM 分固定 8000h + 两张 16 KB 页表，两片 YM2203 的 FM 与 SSG 分开跑。`commando` 的音乐同样来自 HVSC（Rob Hubbard 的 `Commando.sid`，4,165 B，SHA-1 命中）。这两款的 bg1/精灵是 256 KB 级的明文数组，直接躺在资产区里 XIP 直读，所以资产区从 1.78 MB 涨到 3.00 MB。
pacman 的 3 个 PROM（`82s123.7f` / `82s126.4a` / `82s126.1m`）来自用户提供的
`puckman.zip`（日版原祖包，`pm1-1.7f` / `pm1-4.4a` / `pm1-3.1m`），程序 ROM
仍是原版 `pacman.5e/5f/6e/6f/6h/6j`。上板若发现颜色/音色有偏差，先怀疑这处混搭。

图形化工具（推荐，照 rom_uploader 的路子做的）：

```
run_studio.bat            # 双击；内部会挑一个带 tkinter 的 python
```

`tools\pico_arcade_studio.py` 四个页签：

1. **① ROM 转换**：扫上游 `romconv` 的全部 job（83 个），选机型 + 选 zip，按钮 **校验 SHA-1** 会按 job 的 `_REQUIRED` 列出每个必需文件是否命中；
   **一键集成** = `convert.py -j1` → 回灌 12 款明文 → 拷机器源 → `plain_assets` → 修 `Logo` 的 `constexpr` → `add_game` + 两处 `MACHINES` → 明文快照 → `extract`。
2. **② 固件编译**：`cmake`/`ninja -j2` 流式日志，可中途停止，完事显示固件字节数与上限。
3. **③ 合并 & 烧写**：`merge` 前会自动把旧 `dist\galagino_16m.uf2` 改名成 `_prev.uf2`（防占用），合并后立刻校验块结构；烧写支持「拖 RPI-RP2 盘」（自动找 `INFO_UF2.TXT`）和 `picotool load`。
4. **④ 资产区**：读 `romregion.json` 列出 100 个资产的偏移/字节/结束地址与总占用。

无窗口自检：

```powershell
python tools\pico_arcade_studio.py --selftest       # 纯逻辑：job 数、必需文件数、资产区、UF2 校验
python tools\pico_arcade_studio.py --smoke out.txt  # 真建窗口、量控件、写报告后退出
```

> 踩坑：本机 PATH 里的 `python` 是沙箱自带的，**没有 Tcl/Tk**（报 `Can't find a usable init.tcl`）。
> `run_studio.bat` 会先试 `E:\Program Files\python313\python.exe`、`D:\Program Files\python3.13\python.exe`，
> 这两个都带 tkinter 8.6。

生成与烧写流水线（命令行版，逐条手跑）：

```powershell
# 1) 把 romconv 的 LZMA 资产还原成明文数组（一次性，产物可回查）
python tools\plain_assets.py

# 2) 抽成 romregion.bin，并把各 *_blob 改成 (GAL_ROM_BASE + offset) 指针
python tools\mkromregion.py extract

# 3) 编固件（CMake 里已把 PB_FLASH_SIZE_BYTES 设成 16 MB、
#    链接脚本换 memmap_pico16m.ld，POST_BUILD 会检查固件没越过 0x120000）
ninja -C build-diag

# 4) 固件 UF2 + 资产区 -> 一个可直接拖进 BOOTSEL 盘的合并 UF2
python tools\mkromregion.py merge
# -> dist\galagino_16m.uf2
```

几个要点：

- 头文件里是 `static const FlashAsset<..., PLAIN> x_blob = { (const T*)(GAL_ROM_BASE + off), count };`
  ——**不是 constexpr**：整数转指针不是常量表达式，所以走启动时初始化。
- `rom/romregion.bin` 是资产的真身，`rom/romregion.json` 是偏移表；
  `rom/plain_headers/` 是抽取前的明文头备份（想重排布局就从它重跑 extract）。
- 之后只更新固件时，单独烧 `galagino_arcade.uf2` 也不会擦掉资产区——
  UF2 只写自己覆盖的地址范围，0x120000 以后的内容原样保留。
- 设备侧不解包，`Asset<T,COMPRESSED>` 的 Arena 需求归零，
  `ARENA_A_SIZE` 只保留 8 KB 给菜单鼓形查表。
- 1942 是后加的，走的是"另一条等价路径"：romconv 生成的头里本来就带
  `#if _<asset>_PLAIN` 分支，所以只要把 `src/machines/1942/1942_arena_plan.h`
  手写成"所有 `_PLAIN` 都 = 1 + `ArenaPlan {0,0}`"，明文数组就直接可用，
  不需要 `plain_assets.py` 解 LZMA。`mkromregion.py` 的数组正则对这种
  "没有 `// <name>: N bytes` 注释"的头也认（注释段现在是可选的）。

## 七、下一步（按优先级）

1. **真机验证**：先进菜单、再进 dkong/galaga/1942/pacman，确认画面、按键、声音。
   （1942 是双 Z80 + 双 AY，62.5 MHz 下能不能压住 60 fps 只有上板才知道。）
2. **pacman 颜色核对**：调色板 PROM 来自 puckman 包，若迷宫/角色颜色不对，
   换回完整 `pacman.zip`（SHA-1 `dcb0bbc7e7fa5a22269715b79feb9140d4ee1b05`）重跑。
3. 资产区若以后要支持"换 ROM 不重编固件"，把 `offset` 表改成运行时扫
   `romregion.bin` 头部的目录（云主机上那套 `galagino-romtar` 就是这个做法）。
4. 手柄 Y 键可再绑一个功能（例如自动开火）；音频音量/采样率的听感微调。
