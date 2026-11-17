# Build · Flash · Usage Guide

Target: galagino-pico (RP2040 + PicoBoy, 12 arcade machines).
Chinese version: `编译上传使用说明.md` in the same folder.

A local archive copy lives in `E:\galagpico`. **When you clone this repo, the repository root is the project root** — replace `E:\galagpico\源码\galagino-pico` below with your own clone path. All other paths (SDK, BSP, firmware output) are relative to the project root.

Every path, key mapping and parameter below was checked against the source tree and the real board. Values that come from code carry a file reference; if a reading no longer matches after a rebuild, check that file first.

---

# 1. Build

## 1.1 Required software

| Component | Version / location | Notes |
|---|---|---|
| Pico SDK | 1.5.1, `E:\Program Files\Raspberry Pi\Pico SDK v1.5.1\pico-sdk` | Must be 1.5.x (`PICO_SDK_PATH` points here) |
| CMake | `...\Pico SDK v1.5.1\cmake\bin` | Ships with the SDK installer |
| Ninja | `...\Pico SDK v1.5.1\ninja` | Same |
| arm-none-eabi-gcc | `...\Pico SDK v1.5.1\gcc-arm-none-eabi\bin` (10.3.1) | Same |
| picotool | `...\Pico SDK v1.5.1\picotool\picotool.exe` | Command-line flashing, optional |
| Python | `E:\Program Files\python313\python.exe` | Only for merging the asset region (`mkromregion.py merge`) |

The Raspberry Pi Pico Windows installer brings all of this in one go (Pico SDK 1.5.1 + ARM toolchain + CMake + Ninja).

## 1.2 Environment variables for every new PowerShell

PowerShell does not carry variables across processes, so set them again in each new window:

```powershell
$env:PICO_SDK_PATH='E:\Program Files\Raspberry Pi\Pico SDK v1.5.1\pico-sdk'
$env:PATH='E:\Program Files\Raspberry Pi\Pico SDK v1.5.1\cmake\bin;E:\Program Files\Raspberry Pi\Pico SDK v1.5.1\ninja;E:\Program Files\Raspberry Pi\Pico SDK v1.5.1\gcc-arm-none-eabi\bin;C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem'
```

Verify:

```powershell
cmake --version      # 3.2x
ninja --version      # 1.1x
arm-none-eabi-gcc --version
```

## 1.3 Enter the project

```powershell
cd E:\galagpico\源码\galagino-pico
```

By default the board support comes from `E:/PicoBoy/bsp`. To build from this archive alone, point the configure step at the bundled copy:

```powershell
-DPB_BSP_DIR=E:/galagpico/源码/picoboy-bsp
```

Both copies are identical; either works.

## 1.4 Release build (PWM audio · 71.4 MHz · per-machine half rate)

First-time configure (run once, when `build-full71` does not exist yet):

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

Build:

```powershell
cmake --build build-full71 -j2
```

**`-j2` is mandatory.** On an 8 GB machine, `-j3` and above makes arm-gcc's `cc1` run out of memory and die.

Success looks like this:

```
[133/133] Linking CXX executable galagino_arcade.elf
-- 固件体积 227604 B / 上限 786432 B
```

Anything at or below 786,432 B passes.

## 1.5 I2S build (only the audio backend changes)

Use exactly the same configuration as 1.4 and **change `-DPB_AUDIO_I2S=0` to `1`**:

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

The configure step prints the active backend; use that line to confirm the choice took effect:

```
-- 音频输出后端：PIO I2S（需外接 I2S DAC，引脚 13/14）
-- 音频输出后端：板载 PWM（单 GPIO）
```

Expected size: `-- 固件体积 227912 B / 上限 786432 B`, i.e. 308 B larger than the PWM build, which is the I2S driver plus `pb_audio_free()`.

**Both backends share one API, the same 61035 frames/s carrier and the same 2048-frame ring buffer**, so none of the resampling or flow-control code changes — switching only affects the driver underneath. `src/hal/rp2040/audio_pwm.cpp` has "pwm" in its name but serves both backends; no per-backend branch is needed there.

## 1.6 Reference build (62.5 MHz)

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

With `GALAGINO_FULL_RATE=OFF`, `config.h` defines `VIDEO_HALF_RATE`, which forces **all 12 machines to half rate** — it is not the per-machine policy. So this build changes two variables at once (clock and machine policy) and is only good for a global "run everything at 62.5" comparison, not for isolating one cause.

## 1.7 Silent build

Compiles the whole audio synthesis path out, handing core1 back to the emulator:

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

Expected: `-- 固件体积 225444 B / 上限 786432 B`, roughly 2 KB smaller than the audio build.

In a silent build `Audio::init`, `stop`, `produce` and `transmit` all return immediately (`GAL_AUDIO_OFF` in `src/emulation/audio.cpp`). `audio.transmit()` in the main loop is a no-op and the frame pacing relies only on the software clock in `pacer.next()`, never on audio. Verify that audio really is gone from the binary:

```powershell
& 'E:\Program Files\Raspberry Pi\Pico SDK v1.5.1\gcc-arm-none-eabi\bin\arm-none-eabi-nm.exe' `
  build-silent62\galagino_arcade.elf | Select-String 'pb_audio_'
```

No output means the audio path is fully stripped. If you see `pb_audio_init` and friends, `GALAGINO_AUDIO=OFF` did not take effect — usually a stale build directory that was never reconfigured.

## 1.8 Merge the asset region into a flashable UF2

The build only produces `galagino_arcade.uf2` (about 387 KB, firmware only). The game ROMs live in a separate asset region, so the two have to be merged into one file:

```powershell
& 'E:\Program Files\python313\python.exe' tools\mkromregion.py merge `
  --fw build-full71\galagino_arcade.uf2 `
  --out dist\galagino_16m_mixed71d.uf2
```

Expected output:

```
固件 890 块 / ROM 区 3145084 B = 12286 块
合并后 dist\galagino_16m_mixed71d.uf2 (6746112 B)
```

**890 blocks** are the firmware region (from `0x10000000`) and **12286 blocks** the asset region (from `0x10120000`). They do not overlap, so the merged file is one continuous UF2 — dropping it on the BOOTSEL drive writes both regions in one go.

## 1.9 Build matrix

| Build dir | Key switches | Firmware | Size | text | Blocks |
|---|---|---|---|---|---|
| `build-full71` | FULL_RATE=ON, SPI 71.4, `PB_AUDIO_I2S=0` | `galagino_16m_mixed71d.uf2` | 6,746,112 B | 227,604 B | 890 |
| `build-i2s71` | same, `PB_AUDIO_I2S=1` | `galagino_16m_mixed71d_i2s.uf2` | 6,746,624 B | 227,912 B | 891 |
| `build-mix62` | FULL_RATE=OFF, SPI 62.5 | `galagino_16m_mixed62c.uf2` | 6,746,112 B | 227,612 B | 890 |
| `build-silent62` | 62.5, `GALAGINO_AUDIO=OFF` | `galagino_16m_silent62.uf2` | 6,741,504 B | 225,444 B | 881 |

## 1.10 Common CMake switches

| Switch | Default | Effect |
|---|---|---|
| `PB_AUDIO_I2S` | 0 | **Audio backend**: 0 = onboard single-GPIO PWM; 1 = PIO I2S (needs an external DAC) |
| `GALAGINO_AUDIO` | ON | OFF = silent build (`GAL_AUDIO_OFF`, whole synthesis path compiled out) |
| `GALAGINO_AUDIO_CORE1` | ON | Render audio on the emulation core (`AUDIO_RENDER_ON_EMU_CORE`) |
| `GALAGINO_FULL_RATE` | OFF | ON: TFT at 80 MHz and `VIDEO_HALF_RATE` undefined, per-machine policy active; OFF: 62.5 MHz and everything half rate |
| `GALAGINO_TFT_SPICLK_VALUE` | 80M / 62.5M | Derived from the switch above, rarely set by hand |
| `PB_SPI_HZ_OVERRIDE` | empty | Overrides the LCD bit clock; the release build uses `71428571` (= 250 MHz/(2×1.75)) |
| `GALAGINO_FRAME_TIMING` | OFF | Prints one `[timing]` line every 30 frames |
| `GALAGINO_TRACE` | OFF | Colour blocks on screen during boot to see where it stops |
| `GALAGINO_NO_WDT` | OFF | Disable the hardware watchdog (freeze instead of reboot) |
| `PB_BSP_DIR` | `E:/PicoBoy/bsp` | Board support location |
| `PB_LCD_ROTATE` / `PB_LCD_INVERT` | 0 / 0 | Adjust when the picture is rotated or inverted |
| `PB_FB_W` / `PB_FB_H` | 224 / 16 | The BSP frame buffer is only used by GB/NES; shrunk to two strips to save RAM |

## 1.11 Build errors

| Symptom | Cause / fix |
|---|---|
| `cc1.exe: out of memory` or killed | More than `-j2` in use → drop to `-j2` |
| `Generator: execution of make failed` / incomplete configure | `-G Ninja` missing, so it fell back to the NMake generator → delete that build dir and reconfigure |
| `check_fw_size: 固件 bin 不存在` | The POST_BUILD size check used to run before `pico_add_extra_outputs`; already fixed here |
| `undefined reference to 'pb_display_push_start'` | The BSP lacks the three async push functions → use the bundled `picoboy-bsp` |
| `'pb_audio_free' was not declared in this scope` | The BSP lacks `pb_audio_free()`. The I2S backend needs it too; without it an I2S build fails at link time |
| Access denied writing `dist\...uf2` | The file is held by Explorer or a flashing tool → rename it to `_prev.uf2` first |
| A macro changed but behaviour did not | The value lives in `CMakeCache.txt`; re-run `cmake -S . -B <dir>`, and after changing the panel or backend consider deleting the build dir |
| Still links PWM after changing `-DPB_AUDIO_I2S` | Stale cache → delete the build dir and reconfigure, then read the "音频输出后端" status line |

---

# 2. Flashing

## 2.1 Easiest way: BOOTSEL drive

1. **Unplug USB**, hold the **BOOTSEL** button down.
2. Plug USB in, release BOOTSEL after about a second.
3. A drive named `RPI-RP2` appears (here usually `I:\`; the letter can differ, go by the label).
4. Drag the **merged** UF2 onto it (sizes are in table 1.9).
5. It reboots by itself and the drive disappears — that is normal, not a failure.

Always use the merged image: flashing the 387 KB `galagino_arcade.uf2` alone gives you a menu but no games, because the ROMs are missing.

Check the file sizes:

```powershell
Get-ChildItem 'E:\galagpico\固件' | Select-Object Name, Length
```

## 2.2 Using picotool

```powershell
& 'E:\Program Files\Raspberry Pi\Pico SDK v1.5.1\picotool\picotool.exe' load -v -x `
  'E:\galagpico\固件\galagino_16m_mixed71d.uf2'
```

`-x` runs the image right after flashing. Adjust the picotool path to your installation.

## 2.3 Firmware only, leaving the asset region alone

A UF2 only writes the addresses it covers. The asset region starts at `0x10120000`, so **flashing the 387 KB `galagino_arcade.uf2` does not erase it** — a code-only change can therefore be flashed quickly.

The reverse matters too: **if the `rom` assets or their offsets change, you must flash the merged image**, otherwise the firmware reads at stale offsets and graphics or sound will be wrong.

## 2.4 Reading the serial log

The project sets `pico_enable_stdio_usb=1` and `pico_enable_stdio_uart=0`, so logs go over the **USB virtual serial port (CDC)**, not the UART pins.

1. Boot normally (plug USB without holding BOOTSEL).
2. A COM port shows up in Device Manager (the number can change).
3. Open it with PuTTY, Tera Term or `python -m serial.tools.miniterm COMx 115200`; **115200 is fine** (CDC does not care).

Sample output:

```
[video] frames=1200 (2003 ms)
[timing] n=30 frame=33012us parts=2 sim=1 render=18420 push=2630 audio=410 wait=11930 pr=452 pw=451 ring=640 fps=303.0
```

| Field | Meaning |
|---|---|
| `frame` | Whole frame in µs, including sleeps |
| `parts` | 1 = full width (60 Hz refresh); 2 = half rate (30 Hz refresh) |
| `sim` | Emulation marker (**the probe is misplaced and reads 0–2**; real emulation time is inside `render`) |
| `render` | Render time (emulation plus pixel conversion) |
| `push` | Push time (with double buffering this is only "start the DMA") |
| `audio` | Audio transmit time; always 0 in a silent build |
| `wait` | Idle time waiting for the frame deadline |
| `pr` / `pw` | Chunks produced / chunks handed out (cumulative); +370–460 per 30 frames is normal |
| `ring` | Samples queued in the audio ring. 300–1000 = real time; **stuck at 0 = underrun** |
| `fps` | `10000000 / frame`: `600.0` = 60 fps, `303.x` = 30.x fps |

How to read it: a few milliseconds of `wait` means the frame is deadline-bound; `wait` near 0 with `render` approaching `frame` means it genuinely cannot keep up.

## 2.5 I2S wiring (I2S build only)

I2S is a digital bus and **requires an external I2S DAC** (MAX98357A has an amp and can drive a speaker directly; PCM5102 gives an analogue output). The BSP drives it from `pio1`; pins are defined in `bsp/include/picoboy/board.h`:

| Signal | GPIO | Notes |
|---|---|---|
| DATA (DIN) | 13 | Serial data |
| BCLK | 14 | Bit clock |
| LRCLK | 15 | Word/left-right clock, **must be adjacent to BCLK** (PIO side-set constraint) |
| GND | board ground | Must be common with the DAC |
| VCC | 3V3 / 5V | Per the module's requirement |

The PWM pin (GPIO11) is unused in I2S mode. Volume is still controlled in software on two levels (`volumeSetting` 1..30 plus the BSP's `pb_audio_set_volume(10)`), so no DAC gain pin needs touching unless the module has a GAIN pin you want to set.

To check that it is really streaming, watch `ring` in the serial log: it should move between 300 and 1000. If `ring` sits at 0 or 2048, either nothing is feeding it or the DAC is not pulling data (swapped BCLK/LRCLK is the most common cause).

---

# 3. Using the handheld

## 3.1 Boot sequence

Power on → logo → `[####      ]` progress bar while the selected ROM loads → **game selection menu** (machines arranged in a drum, current entry highlighted) → pick with up/down → press **X (FIRE)** or **A (START)**.

A load failure shows `ERROR <reason>` and `PRESS ANY BUTTON`; any button returns to the menu. The menu remembers the last machine played.

## 3.2 Key mapping (8 onboard buttons)

| Button | Maps to | In the menu | In game |
|---|---|---|---|
| D-pad ▲ | UP | previous machine | up |
| D-pad ▼ | DOWN | next machine | down |
| D-pad ◀ | LEFT | — | left |
| D-pad ▶ | RIGHT | — | right |
| **X** | FIRE | **enter the selected game** | fire / attack |
| **A** | START | enter the selected game | start after inserting a credit |
| **B** | COIN | wake or reset attract | **insert credit**; volume combo; long press returns to menu |
| **Y** | unbound | — | — |

Source: `src/boards/picoboy.h` (`BTN_START_PIN=PB_PIN_BTN_A`, `BTN_COIN_PIN=PB_PIN_BTN_B`, `BTN_FIRE_PIN=PB_PIN_BTN_X`). Y is free; to add autofire, define a `BTN_*_PIN` in `picoboy.h` and OR it into the mask in `input.cpp`.

## 3.3 Credits and start

1. Press **B** to insert a credit (CREDIT on screen goes up).
2. Press **A** to start.

Some machines (commando, for example) only leave attract after a credit; if a game entered with X sits on the demo screen, insert a credit first.

## 3.4 Volume

**Hold B (COIN) and press ▲ to get louder, ▼ to get quieter.**

Internally this is `volumeSetting`, 1..30 where the smaller number is louder (`1` loudest, `30` quietest), default **3**. It is a software scale and sits on top of the BSP's `pb_audio_set_volume(10)`, a second, separate gain stage.

## 3.5 Leaving a game

**Hold B (COIN) for more than 3 seconds** to reset back to the menu (the reset branch in `input.cpp`).

Two automatic behaviours as well: the menu falls into attract mode when left alone, and a game left idle too long (`attractGameStart` timeout) returns to the menu with the **next** machine preselected.

## 3.6 The 12 machines and their refresh policy (release build)

| Machine | Frame width | Refresh | Notes |
|---|---|---|---|
| 1943 | 224 | **half rate, 30 Hz** | dual YM2203, heaviest load |
| xevious | — | **half rate, 30 Hz** | Namco WSG |
| sxevious | — | **half rate, 30 Hz** | inherits xevious, only the CPU ROM differs |
| contra | — | **half rate, 30 Hz** | chosen after a 21.5 ms render peak |
| commando | 224 | **half rate, 30 Hz** | dual YM2203; coin bit fixed |
| 1942 | — | full width, 60 Hz | dual Z80 plus dual AY8910 |
| dkong | — | full width, 60 Hz | |
| galaga | — | full width, 60 Hz | |
| pacman | — | full width, 60 Hz | palette PROM from the puckman set |
| yiear | — | full width, 60 Hz | |
| gng | — | full width, 60 Hz | |
| gberet | — | full width, 60 Hz | single SN76489 |

"Half rate" means the panel refreshes at 30 Hz while the emulation still runs at 60 Hz logic — the game is not slowed down. To change a machine, flip the return value of `useVideoHalfRate()` (`1` half, `0` full) in its header and rebuild.

## 3.7 Screen artefacts

At 71.4 MHz occasional interference patterns appear and the cause is still open. Compare builds:

1. Flash `galagino_16m_mixed62c.uf2` (identical except SPI dropped to 62.5).
2. Compare the same machine at the same screen position.

- **Artefacts gone at 62.5** → panel clock limit. Either accept 71.4 with the slight artefacts, drop back to 62.5 for a clean picture, or shrink the block size (`kBlockWords`) to lower the instantaneous SPI pressure.
- **Artefacts still there at 62.5** → not frequency-related but the push implementation (block boundaries, window resets, PIO mode switches). Continue from `pushBlock()` in `video_picoboy.cpp` and `pb_display_push_start()`.

## 3.8 No sound: check in this order

1. **Confirm the firmware has sound.** The silent build (`galagino_16m_silent62.uf2`) has none by design and its `audio` field is always 0. `nm` searching for `pb_audio_` symbols settles it.
2. **Check the backend.** A `PB_AUDIO_I2S=0` image drives the PWM pin and works on a board with no DAC. A `PB_AUDIO_I2S=1` image **requires a DAC** and is completely silent without one — this is the most common false alarm.
3. **Then watch `ring`.** Stuck at 0 = upstream underrun (resampling fell behind or `pb_audio_free()` was broken). Pinned at 2048 = nothing consuming (DMA not running; in PWM mode check that `pb_audio_init` ran, in I2S mode check BCLK/LRCLK).

## 3.9 Troubleshooting

| Symptom | Cause / fix |
|---|---|
| White or garbled screen, upside down | Panel orientation or invert: `-DPB_LCD_ROTATE=1`, `-DPB_LCD_INVERT=0` (this project defaults to 0/0) |
| Distorted audio, no recognisable music | Fixed (samples are no longer dropped). If it comes back, check that `pb_audio_free()` still exists in the BSP and the space check in `audio_pwm.cpp` is intact |
| PANIC "Out of memory" entering a machine | Too many static arrays, heap under 100 KB; read `avail=` printed by `switchTo()` (**not `Heap: Free`, that number is unreliable**) |
| A machine is visibly slow | Look at `wait` in `[timing]`: large = deadline-bound (fine); near 0 = genuinely too slow, mark that machine half rate |
| commando jumps to the test screen on a credit | Historical bug (COIN1 mapped to the SERVICE bit7), fixed to `~0x10`; if it returns, inspect `case 0xc000` in `commando.cpp` |
| Nothing happens after flashing / old program runs | Make sure the merged image and its size match table 1.9; reflash via BOOTSEL to overwrite |
| I2S build is completely silent | No I2S DAC on the board, or BCLK/LRCLK swapped (the two pins must stay adjacent, do not swap them) |
| Silent build hangs in a game | Rebuild the current code as described in 1.7; `async2_silent.uf2` (2026-11-16) is old code and retired |

---

# 4. Continuing development

- Source: `源码\galagino-pico` (`src`, `rom`, `tools`, `cmake`, `CMakeLists.txt`, `*.ld`)
- BSP: `源码\picoboy-bsp` (has `pb_audio_free()`, the async push trio and the I2S backend)
- Engineering log: `文档\PROGRESS.md` (measured readings per step and the hypotheses already disproved)
- Release parameters and conclusions: `README.md` in the archive root

After any change, run `mkromregion.py merge` and flash the merged image. A `src`-only change that does not touch `rom` can be flashed as `build-*\galagino_arcade.uf2` to save time.

## 4.1 Adding a machine

1. `CMakeLists.txt`: add `add_game(<name>)`;
2. `src/catalog.h`: register the machine and its display name;
3. `src/machines/<name>/`: `<name>.h` (`WIDTH`, `useVideoHalfRate()`, memory layout), `<name>.cpp`, DIP and logo headers;
4. `rom/plain_src/<name>/`: plain assets, then regenerate `romregion.bin` with `mkromregion.py`;
5. Rebuild and re-merge (asset offsets moved, so flash the merged image).

## 4.2 Where audio changes go

| Want to change | File |
|---|---|
| Synthesis cadence, ring size | `src/emulation/audio.cpp` (`produce()` / `transmit()`), `audio.h` (`RING_SAMPLES`) |
| Samples per chunk | `src/emulation/audio_mix.h` (`AUDIO_CHUNK=64`) |
| Resampling, volume, flow control | `src/hal/rp2040/audio_pwm.cpp` (`stepQ16`, `LEAD`, the `pb_audio_free()` check) |
| PWM backend | `bsp/src/pb_audio.cpp` |
| I2S backend | `bsp/src/pb_audio_i2s.cpp` + `pb_i2s.pio` |
| Sample rate, pins | `bsp/include/picoboy/board.h` (`PB_AUDIO_SAMPLE_RATE=61035`, `PB_PIN_I2S_*`) |

## 4.3 Where video changes go

`src/hal/rp2040/video_picoboy.cpp`: `setHalfRate`, `screenParts`, `submit`, `packPairs`, `pushBlock`, `endFrame`;
`bsp/src/pb_display.cpp`: `pb_display_push_start`, `push_words`, `push_flush`, `pb_display_push_rect`.

Block size is set by `kBlockWords` (currently `2*8*240/2` = 1920 words = 7,680 B per block, 15,360 B for the double buffer). Check `avail=` before raising it — the 18-strip experiment pushed sbrk down to 102 KB and 1943 panicked.
