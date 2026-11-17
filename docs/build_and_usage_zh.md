# 编译 · 上传 · 使用说明

对象：galagino-pico（RP2040 + PicoBoy，12 款街机）。
英文版见同目录 `BUILD_AND_USAGE.md`。

本地归档副本位于 `E:\galagpico`；**从 GitHub 克隆时，仓库根目录就是工程根目录**，下文出现的 `E:\galagpico\源码\galagino-pico` 请换成你自己的克隆路径。其余路径（SDK、BSP、固件输出）都相对工程根。

本文所有路径、按键、参数都从源码与实机复核过。凡涉及数值的地方都标了出处，改版后若读数对不上，先查对应源文件。

---

# 一、编译

## 1.1 需要的软件

| 组件 | 版本 / 位置 | 说明 |
|---|---|---|
| Pico SDK | 1.5.1，`E:\Program Files\Raspberry Pi\Pico SDK v1.5.1\pico-sdk` | 必须 1.5.x（`PICO_SDK_PATH` 指到它） |
| CMake | `...\Pico SDK v1.5.1\cmake\bin` | SDK 安装包自带 |
| Ninja | `...\Pico SDK v1.5.1\ninja` | 同上 |
| arm-none-eabi-gcc | `...\Pico SDK v1.5.1\gcc-arm-none-eabi\bin`（10.3.1） | 同上 |
| picotool | `...\Pico SDK v1.5.1\picotool\picotool.exe` | 命令行烧写用，可选 |
| Python | `E:\Program Files\python313\python.exe` | 只用于合并资产区（`mkromregion.py merge`） |

SDK 安装包通常把这些一起装好。**没有 SDK 先装**（Raspberry Pi Pico Windows 安装器，选 Pico SDK 1.5.1 + ARM 工具链 + CMake + Ninja）。

## 1.2 每个新开的 PowerShell 都要设环境变量

PowerShell 变量不跨进程保留，每次新开窗口重设一次：

```powershell
$env:PICO_SDK_PATH='E:\Program Files\Raspberry Pi\Pico SDK v1.5.1\pico-sdk'
$env:PATH='E:\Program Files\Raspberry Pi\Pico SDK v1.5.1\cmake\bin;E:\Program Files\Raspberry Pi\Pico SDK v1.5.1\ninja;E:\Program Files\Raspberry Pi\Pico SDK v1.5.1\gcc-arm-none-eabi\bin;C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem'
```

验证：

```powershell
cmake --version      # 3.2x
ninja --version      # 1.1x
arm-none-eabi-gcc --version
```

## 1.3 进入工程目录

```powershell
cd E:\galagpico\源码\galagino-pico
```

工程默认从 `E:/PicoBoy/bsp` 取 BSP。只用这份归档、不依赖 `E:\PicoBoy` 时，配置时换成：

```powershell
-DPB_BSP_DIR=E:/galagpico/源码/picoboy-bsp
```

两处 BSP 内容一致，用哪个都行。

## 1.4 定型版编译（PWM 音频 + 71.4 MHz + 按机型半速）

首次配置（`build-full71` 不存在时执行一次）：

```powershell
cmake -S . -B build-full71 -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DPICO_SDK_PATH='E:/Program Files/Raspberry Pi/Pico SDK v1.5.1/pico-sdk' `
  -DPB_BSP_DIR=E:/PicoBoy/bsp `
  -DPB_FLASH_SIZE_BYTES=16777216 -DPB_FB_W=224 -DPB_FB_H=16 `
  -DPB_LCD_INVERT=0 -DPB_LCD_ROTATE=0 -DPB_SCREEN_MODE=1 `
  -DPB_AUDIO_I2S=0 `
  -DGALAGINO_AUDIO=ON -DGALAGINO_AUDIO_CORE1=ON `
  -DGALAGINO_FULL_RATE=ON -DPB_SPI_HZ_OVERRIDE=71428571 `
  -DGALAGINO_FRAME_TIMING=ON -DGALAGINO_CLOCK_KHZ=250000
```

编译：

```powershell
cmake --build build-full71 -j2
```

**必须 `-j2`。** 8 GB 内存的机器，`-j3` 以上 arm-gcc 的 `cc1` 会内存溢出退出。

成功标志（最后两行）：

```
[133/133] Linking CXX executable galagino_arcade.elf
-- 固件体积 227604 B / 上限 786432 B
```

体积 ≤ 786,432 B 就过。

## 1.5 I2S 版编译（只换音频后端）

和 1.4 完全相同的配置，**只把 `-DPB_AUDIO_I2S=0` 改成 `1`**：

```powershell
cmake -S . -B build-i2s71 -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DPICO_SDK_PATH='E:/Program Files/Raspberry Pi/Pico SDK v1.5.1/pico-sdk' `
  -DPB_BSP_DIR=E:/PicoBoy/bsp `
  -DPB_FLASH_SIZE_BYTES=16777216 -DPB_FB_W=224 -DPB_FB_H=16 `
  -DPB_LCD_INVERT=0 -DPB_LCD_ROTATE=0 -DPB_SCREEN_MODE=1 `
  -DPB_AUDIO_I2S=1 `
  -DGALAGINO_AUDIO=ON -DGALAGINO_AUDIO_CORE1=ON `
  -DGALAGINO_FULL_RATE=ON -DPB_SPI_HZ_OVERRIDE=71428571 `
  -DGALAGINO_FRAME_TIMING=ON -DGALAGINO_CLOCK_KHZ=250000
cmake --build build-i2s71 -j2
```

配置阶段会打印当前后端，用这一行确认没选错：

```
-- 音频输出后端：PIO I2S（需外接 I2S DAC，引脚 13/14）
-- 音频输出后端：板载 PWM（单 GPIO）
```

成功标志：`-- 固件体积 227912 B / 上限 786432 B`（比 PWM 版大 308 B，是 I2S 驱动与 `pb_audio_free()` 的开销）。

**两套后端共用同一套 API、同一 61035 帧/秒载波、同一 2048 帧环形缓冲**，所以工程侧的重采样与流控代码一行都不用改，切换只影响底层驱动。`src/hal/rp2040/audio_pwm.cpp` 文件名带 pwm，但它服务两种后端，不需要按后端分支。

## 1.6 对照版编译（62.5 MHz）

```powershell
cmake -S . -B build-mix62 -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DPICO_SDK_PATH='E:/Program Files/Raspberry Pi/Pico SDK v1.5.1/pico-sdk' `
  -DPB_BSP_DIR=E:/PicoBoy/bsp `
  -DPB_FLASH_SIZE_BYTES=16777216 -DPB_FB_W=224 -DPB_FB_H=16 `
  -DPB_LCD_INVERT=0 -DPB_LCD_ROTATE=0 -DPB_SCREEN_MODE=1 `
  -DPB_AUDIO_I2S=0 `
  -DGALAGINO_AUDIO=ON -DGALAGINO_AUDIO_CORE1=ON `
  -DGALAGINO_FULL_RATE=OFF `
  -DGALAGINO_FRAME_TIMING=ON -DGALAGINO_CLOCK_KHZ=250000
cmake --build build-mix62 -j2
```

`GALAGINO_FULL_RATE=OFF` 时 `config.h` 会定义 `VIDEO_HALF_RATE`，那是**一刀切把 12 款全变半速**，不是按机型分流。所以这一档同时改了两个变量（频率 + 机型策略），只适合做"整体降频"对照，不能单独归因。

## 1.7 静音版编译

把音频合成整条路径编译掉，core1 的算力全还给模拟器：

```powershell
cmake -S . -B build-silent62 -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DPICO_SDK_PATH='E:/Program Files/Raspberry Pi/Pico SDK v1.5.1/pico-sdk' `
  -DPB_BSP_DIR=E:/PicoBoy/bsp `
  -DPB_FLASH_SIZE_BYTES=16777216 -DPB_FB_W=224 -DPB_FB_H=16 `
  -DPB_LCD_INVERT=0 -DPB_LCD_ROTATE=0 -DPB_SCREEN_MODE=1 `
  -DPB_AUDIO_I2S=0 `
  -DGALAGINO_AUDIO=OFF `
  -DGALAGINO_FULL_RATE=OFF `
  -DGALAGINO_CLOCK_KHZ=250000
cmake --build build-silent62 -j2
```

成功标志：`-- 固件体积 225444 B / 上限 786432 B`，比有声版小约 2 KB。

静音构建下 `Audio::init / stop / produce / transmit` 四处都直接 return（`src/emulation/audio.cpp` 里的 `GAL_AUDIO_OFF`），主循环里的 `audio.transmit()` 立即返回，帧节拍只靠 `pacer.next()` 的软件时钟，不依赖音频。可以这样验收是否真的没链接音频：

```powershell
& 'E:\Program Files\Raspberry Pi\Pico SDK v1.5.1\gcc-arm-none-eabi\bin\arm-none-eabi-nm.exe' `
  build-silent62\galagino_arcade.elf | Select-String 'pb_audio_'
```

没有任何输出 = 音频路径已完全剔除；若出现 `pb_audio_init` 之类，说明 `GALAGINO_AUDIO=OFF` 没生效（通常是改了 build 目录的缓存没重配）。

## 1.8 合并资产区 → 可烧的 UF2

编译只产出 `galagino_arcade.uf2`（约 387 KB，仅固件）。游戏 ROM 在独立资产区，必须合并成一个文件才能一次烧全：

```powershell
& 'E:\Program Files\python313\python.exe' tools\mkromregion.py merge `
  --fw build-full71\galagino_arcade.uf2 `
  --out dist\galagino_16m_mixed71d.uf2
```

成功输出：

```
固件 890 块 / ROM 区 3145084 B = 12286 块
合并后 dist\galagino_16m_mixed71d.uf2 (6746112 B)
```

**`890` 块与 `12286` 块**分别对应固件区（`0x10000000` 起）和资产区（`0x10120000` 起），两块互不重叠，合并后是一个连续 UF2——拖进 BOOTSEL 盘时两个区一起写。

## 1.9 各配置产物对照

| 构建目录 | 主要开关 | 固件 | 大小 | text | 块数 |
|---|---|---|---|---|---|
| `build-full71` | FULL_RATE=ON、SPI 71.4、`PB_AUDIO_I2S=0` | `galagino_16m_mixed71d.uf2` | 6,746,112 B | 227,604 B | 890 |
| `build-i2s71` | 同上，`PB_AUDIO_I2S=1` | `galagino_16m_mixed71d_i2s.uf2` | 6,746,624 B | 227,912 B | 891 |
| `build-mix62` | FULL_RATE=OFF、SPI 62.5 | `galagino_16m_mixed62c.uf2` | 6,746,112 B | 227,612 B | 890 |
| `build-silent62` | 62.5、`GALAGINO_AUDIO=OFF` | `galagino_16m_silent62.uf2` | 6,741,504 B | 225,444 B | 881 |

## 1.10 常用 CMake 开关

| 开关 | 默认 | 作用 |
|---|---|---|
| `PB_AUDIO_I2S` | 0 | **音频后端**：0 = 板载单 GPIO PWM；1 = PIO I2S（需外接 DAC） |
| `GALAGINO_AUDIO` | ON | OFF = 静音构建（`GAL_AUDIO_OFF`，整条合成路径编译掉） |
| `GALAGINO_AUDIO_CORE1` | ON | 音频合成跑模拟核（定义 `AUDIO_RENDER_ON_EMU_CORE`） |
| `GALAGINO_FULL_RATE` | OFF | ON：TFT 80 MHz 且不定义 `VIDEO_HALF_RATE`，可按机型分流；OFF：62.5 MHz + 全机型半速 |
| `GALAGINO_TFT_SPICLK_VALUE` | 80M / 62.5M | 由上一项推导，一般不手改 |
| `PB_SPI_HZ_OVERRIDE` | 空 | 直接覆盖 LCD 位时钟；定型版用 `71428571`（= 250 MHz/(2×1.75)） |
| `GALAGINO_FRAME_TIMING` | OFF | 每 30 帧打一行 `[timing]`，调性能时开 |
| `GALAGINO_TRACE` | OFF | 开机阶段在屏上打色块，判断卡在哪一步 |
| `GALAGINO_NO_WDT` | OFF | 关硬件看门狗，调试时"死住"而不是重启 |
| `PB_BSP_DIR` | `E:/PicoBoy/bsp` | BSP 源码位置 |
| `PB_LCD_ROTATE` / `PB_LCD_INVERT` | 0 / 0 | 画面方向 / 反色不对时改 |
| `PB_FB_W` / `PB_FB_H` | 224 / 16 | BSP 帧缓冲只给 GB/NES 用，这里缩到 2 条省 RAM |

## 1.11 编译常见错误

| 现象 | 原因 / 处理 |
|---|---|
| `cc1.exe: out of memory` 或进程被杀 | 用了 `-j3` 以上 → 改 `-j2` |
| `Generator: execution of make failed` / 配置 incomplete | 没加 `-G Ninja`，落到 NMake 生成器 → 删掉该 build 目录重配 |
| `check_fw_size: 固件 bin 不存在` | 老版本 POST_BUILD 检查排在 `pico_add_extra_outputs` 之前；本工程已修 |
| `undefined reference to 'pb_display_push_start'` | BSP 缺异步推屏三函数 → 用本归档的 `picoboy-bsp`（已含） |
| `'pb_audio_free' was not declared in this scope` | BSP 缺 `pb_audio_free()` → 用本归档的 `picoboy-bsp`。I2S 后端也必须有它，缺了编 I2S 版会在链接期报未定义 |
| 合并时 `dist\...uf2` 拒绝访问 | 文件被资源管理器/烧写工具占用 → 先改名 `_prev.uf2` 再写 |
| 改完宏但行为没变 | 宏在 `CMakeCache.txt` 里；改完要重新 `cmake -S . -B <dir>`，换面板/后端建议删 build 目录 |
| 换了 `-DPB_AUDIO_I2S` 但仍按 PWM 链接 | 旧 build 目录缓存没更新 → 删目录重配，并看配置阶段的"音频输出后端"提示 |

---

# 二、上传（烧写）

## 2.1 最简单的办法：BOOTSEL 拖盘

1. **拔掉 USB**，按住板上 **BOOTSEL** 键不松。
2. 插 USB，约 1 秒后松开 BOOTSEL。
3. 资源管理器出现 `RPI-RP2` 盘（本机常见 `I:\`，盘符不固定，认卷标）。
4. 把**合并版** UF2 拖进去（大小见 1.9 表）。
5. 拷完自动复位启动，U 盘消失——正常现象，不是失败。

必须用合并版：单烧 `galagino_arcade.uf2`（387 KB）只有固件没有 ROM，菜单能出但游戏进不去。

核对大小的命令：

```powershell
Get-ChildItem 'E:\galagpico\固件' | Select-Object Name, Length
```

## 2.2 用 picotool

```powershell
& 'E:\Program Files\Raspberry Pi\Pico SDK v1.5.1\picotool\picotool.exe' load -v -x `
  'E:\galagpico\固件\galagino_16m_mixed71d.uf2'
```

`-x` = 烧完立即运行。picotool 路径按实际安装位置改。

## 2.3 只换固件、不动资产区

UF2 只写自己覆盖的地址范围。资产区在 `0x10120000` 以后，**单烧 `galagino_arcade.uf2`（387 KB）不会擦掉它**，所以只改代码时可以只烧固件，快很多。

反过来注意：**改了 `rom` 资产或重排了资产偏移，就必须烧合并版**，否则固件按旧偏移去读、画面/声音会乱。

## 2.4 串口日志怎么看

工程配置 `pico_enable_stdio_usb=1`、`pico_enable_stdio_uart=0`：日志走 **USB 虚拟串口（CDC）**，不是板上 UART 引脚。

1. 板子正常启动（不按 BOOTSEL 插 USB）。
2. 设备管理器里多出一个 COM 口（编号可能变）。
3. 用 PuTTY / Tera Term / `python -m serial.tools.miniterm COMx 115200` 打开，**波特率填 115200 即可**（CDC 不限速）。

日志样例：

```
[video] frames=1200 (2003 ms)
[timing] n=30 frame=33012us parts=2 sim=1 render=18420 push=2630 audio=410 wait=11930 pr=452 pw=451 ring=640 fps=303.0
```

| 字段 | 含义 |
|---|---|
| `frame` | 整帧耗时（µs），含睡眠 |
| `parts` | 1 = 全幅（60 Hz 刷屏）；2 = 半速（30 Hz 刷屏） |
| `sim` | 模拟打点（**已知打点位置有误，恒 0~2**，真实模拟时间算在 `render` 里） |
| `render` | 渲染耗时（模拟 + 转像素） |
| `push` | 推屏耗时（双缓冲下只是"启动 DMA"的时间） |
| `audio` | 音频发送耗时；静音构建恒为 0 |
| `wait` | 等帧节拍的空余 |
| `pr` / `pw` | 已合成块数 / 已送出块数（累加），每 30 帧涨 370–460 属正常 |
| `ring` | 音频环积压样本数。300–1000 = 实时；**持续 0 = 欠载** |
| `fps` | `10000000 / frame` 的读数：`600.0` = 60 fps，`303.x` = 30.x fps |

判断思路：`wait` 还有几毫秒 = 被节拍限制；`wait` 接近 0 且 `render` 逼近 `frame` = 真的算不动。

## 2.5 I2S 接线（仅 I2S 版需要）

I2S 是数字总线，**必须外接一颗 I2S DAC**（MAX98357A 自带功放可直接推喇叭，或 PCM5102 走模拟输出）。BSP 用 `pio1` 输出，引脚在 `bsp/include/picoboy/board.h`：

| 信号 | GPIO | 说明 |
|---|---|---|
| DATA（DIN） | 13 | 串行数据 |
| BCLK | 14 | 位时钟 |
| LRCLK | 15 | 左右声道时钟，**必须与 BCLK 相邻**（PIO side-set 限制） |
| GND | 板地 | 与 DAC 共地，必接 |
| VCC | 3V3 / 5V | 按模块要求 |

PWM 引脚（GPIO11）在 I2S 模式下不再使用。音量仍由软件两层控制（`volumeSetting` 1..30 + BSP `pb_audio_set_volume(10)`），不需要动 DAC 的增益脚，除非模块自带 GAIN 引脚要单独设定。

接线后判断是否真的在出声：串口 `ring` 应保持在 300–1000 之间波动；若 `ring` 一直是 0 或 2048，说明上游没在供数或 DAC 没在拉数据（BCLK/LRCLK 接反最常见）。

---

# 三、使用

## 3.1 开机流程

上电 → 显示 logo → 加载所选机型的 ROM 时显示 `[####      ]` 进度条 → 进入**游戏选择菜单**（一圈"鼓形"排列，当前项高亮）→ 方向键上下选机型 → 按 **X（FIRE）** 或 **A（START）** 进入。

加载失败会显示 `ERROR <原因>` 和 `PRESS ANY BUTTON`，按任意键返回菜单重选。菜单会记住上次玩的机型。

## 3.2 按键映射（板载 8 键）

| 板载键 | 映射到 | 菜单里 | 游戏里 |
|---|---|---|---|
| 方向 ▲ | UP | 上一款 | 上 |
| 方向 ▼ | DOWN | 下一款 | 下 |
| 方向 ◀ | LEFT | — | 左 |
| 方向 ▶ | RIGHT | — | 右 |
| **X** | FIRE | **进入所选游戏** | 开火 / 攻击 |
| **A** | START | 进入所选游戏 | 投币后的开始键 |
| **B** | COIN | 唤醒/重置 attract | **投币**；组合键调音量、长按回菜单 |
| **Y** | 未绑定 | — | — |

来源：`src/boards/picoboy.h`（`BTN_START_PIN=PB_PIN_BTN_A`、`BTN_COIN_PIN=PB_PIN_BTN_B`、`BTN_FIRE_PIN=PB_PIN_BTN_X`）。Y 键目前空着，想绑自动连发就在 `picoboy.h` 加 `BTN_*_PIN` 后在 `input.cpp` 并进掩码。

## 3.3 投币与开始

1. 按 **B** 投币（画面 CREDIT 加 1）；
2. 按 **A** 开始。

有些机型（如 commando）必须投币才脱离 attract 演示；菜单直接按 X 进去若停在演示画面，先投一次币。

## 3.4 调音量

**按住 B（COIN）不放，再按方向 ▲ 变响、▼ 变轻。**

实现上是 `volumeSetting` 的 1..30，数值越小越响（`1` 最响、`30` 最轻），默认 **3**。这是软件层缩放，和 BSP 的 `pb_audio_set_volume(10)` 是两级不同增益。

## 3.5 退出当前游戏、回菜单

**长按 B（COIN）超过 3 秒**即复位回菜单（`input.cpp` 的 reset 分支）。

另有两个自动行为：菜单长时间不操作进入 attract 轮流演示；游戏放着不动太久（`attractGameStart` 超时）也会自动回菜单，并把**下一款**预选好。

## 3.6 12 款机型与刷新策略（定型版）

| 机型 | 画面宽度 | 刷新 | 备注 |
|---|---|---|---|
| 1943 | 224 | **半速 30 Hz** | 双 YM2203，负载重 |
| xevious | — | **半速 30 Hz** | Namco WSG |
| sxevious | — | **半速 30 Hz** | 继承 xevious，只换 CPU ROM |
| contra | — | **半速 30 Hz** | 依据 render 峰值 21.5 ms 标的 |
| commando | 224 | **半速 30 Hz** | 双 YM2203；投币位已修 |
| 1942 | — | 全幅 60 Hz | 双 Z80 + 双 AY8910 |
| dkong | — | 全幅 60 Hz | |
| galaga | — | 全幅 60 Hz | |
| pacman | — | 全幅 60 Hz | 调色板 PROM 来自 puckman 包 |
| yiear | — | 全幅 60 Hz | |
| gng | — | 全幅 60 Hz | |
| gberet | — | 全幅 60 Hz | 单 SN76489 |

"半速"= 面板 30 Hz 刷新、模拟器仍按 60 Hz 逻辑跑，不是游戏变慢。要改某款策略：在它的头文件里改 `useVideoHalfRate()` 返回值（`1` 半速 / `0` 全幅），重编即可。

## 3.7 屏幕上出现干扰纹路怎么办

71.4 MHz 下偶发干扰纹路，目前未定论。做对照：

1. 烧 `galagino_16m_mixed62c.uf2`（其它全同，只把 SPI 降到 62.5）；
2. 同一机型、同一画面位置比较。

- 62.5 版**纹路消失** → 面板时钟边限。可选：接受 71.4 的速度与轻微纹路，或降回 62.5 换干净画面；折中是把块切小（改 `kBlockWords`）降低瞬时 SPI 压力。
- 62.5 版**同样有纹路** → 与频率无关，是推屏实现问题（块边界 / 窗口重设 / PIO 模式切换），从 `video_picoboy.cpp` 的 `pushBlock()` 与 `pb_display_push_start()` 路径继续查。

## 3.8 有画面没声音怎么查

按顺序分三层排除：

1. **先确认固件带了声音**：静音版（`galagino_16m_silent62.uf2`）本来就无声，串口 `[timing]` 的 `audio` 恒为 0。用 `nm` 查有没有 `pb_audio_` 符号就能确认。
2. **再看后端对不对**：`PB_AUDIO_I2S=0` 的固件在没接 DAC 的板子上仍然走 PWM 引脚，正常应出声；`PB_AUDIO_I2S=1` 的固件**必须接 DAC**，否则完全无声——这是最常见的一次误判。
3. **最后看串口 `ring`**：持续为 0 = 上游欠载（重采样没跟上或 `pb_audio_free()` 被改坏）；持续贴 2048 = sink 没在消费（DMA 没跑起来，PWM 模式下查 `pb_audio_init` 是否被调用，I2S 模式下查 BCLK/LRCLK）。

## 3.9 常见使用问题

| 现象 | 原因 / 处理 |
|---|---|
| 白屏 / 花屏 / 上下颠倒 | 面板方向或反色不对：`-DPB_LCD_ROTATE=1`、`-DPB_LCD_INVERT=0`（本工程默认 0/0） |
| 声音失真 / 听不出旋律 | 已修（不再丢样本）。若复发，查 `pb_audio_free()` 是否还在 BSP、`audio_pwm.cpp` 的空间检查是否被改 |
| 某款进入后 PANIC "Out of memory" | 静态数组占多了，堆被压到 100 KB 以下；看 `switchTo()` 打印的 `avail=`（**别看 `Heap: Free`，那个读数不可信**） |
| 某款明显慢 | 看 `[timing]` 的 `wait`：大 = 被节拍限制（正常）；接近 0 = 真算不动，把该款标半速 |
| commando 一投币就跳测试页 | 历史 bug（COIN1 被写成 SERVICE bit7），已修为 `~0x10`；复发就查 `commando.cpp` 的 `case 0xc000` |
| 烧完没反应 / 还是旧程序 | 确认拖的是合并版且大小与 1.9 表一致；按 BOOTSEL 重烧覆盖 |
| I2S 版完全没声音 | 板上没有 I2S DAC，或 BCLK/LRCLK 接反（二脚必须相邻，别对调） |
| 静音版进游戏卡住 | 用 1.7 的方式重编当前代码；`async2_silent.uf2`（2026-11-16）是旧代码，已停用 |

---

# 四、继续开发

- 源码：`源码\galagino-pico`（`src` / `rom` / `tools` / `cmake` / `CMakeLists.txt` / `*.ld`）
- BSP：`源码\picoboy-bsp`（含 `pb_audio_free()`、异步推屏三函数、I2S 后端）
- 开发记录：`文档\PROGRESS.md`（每步实测读数与已被证伪的假设）
- 定型参数与结论：根目录 `README.md`

改完先 `mkromregion.py merge` 生成合并 UF2 再烧板。只改 `src` 且没动 `rom` 时可只烧 `build-*\galagino_arcade.uf2` 省时间。

## 4.1 加一款新机型要动哪几处

1. `CMakeLists.txt`：`add_game(<name>)`；
2. `src/catalog.h`：登记机型与显示名；
3. `src/machines/<name>/`：`<name>.h`（`WIDTH`、`useVideoHalfRate()`、内存布局）、`<name>.cpp`、DIP 与 logo 头；
4. `rom/plain_src/<name>/`：明文资产，然后 `mkromregion.py` 重新生成 `romregion.bin`；
5. 重编 + 重新 merge（资产偏移变了，必须烧合并版）。

## 4.2 音频链路改动的落点

| 想改什么 | 文件 |
|---|---|
| 合成节奏、环大小 | `src/emulation/audio.cpp`（`produce()` / `transmit()`）、`audio.h`（`RING_SAMPLES`） |
| 每块样本数 | `src/emulation/audio_mix.h`（`AUDIO_CHUNK=64`） |
| 重采样、音量、流控 | `src/hal/rp2040/audio_pwm.cpp`（`stepQ16`、`LEAD`、`pb_audio_free()` 检查） |
| PWM 后端 | `bsp/src/pb_audio.cpp` |
| I2S 后端 | `bsp/src/pb_audio_i2s.cpp` + `pb_i2s.pio` |
| 采样率/引脚 | `bsp/include/picoboy/board.h`（`PB_AUDIO_SAMPLE_RATE=61035`、`PB_PIN_I2S_*`） |

## 4.3 推屏改动的落点

`src/hal/rp2040/video_picoboy.cpp`：`setHalfRate` / `screenParts` / `submit` / `packPairs` / `pushBlock` / `endFrame`；
`bsp/src/pb_display.cpp`：`pb_display_push_start` / `push_words` / `push_flush` / `pb_display_push_rect`。

块大小由 `kBlockWords` 决定（当前 `2*8*240/2` = 1920 words = 7,680 B/块，双缓冲 15,360 B）。调大前先用 `avail=` 确认 RAM 够——18 条带时代把 sbrk 压到 102 KB，1943 会 PANIC。
