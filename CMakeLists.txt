# galagino (galaga-max) on the PicoBoy: RP2040 + PicoBoy-BSP rev 2.0
#
#   - board support comes from E:\PicoBoy\bsp (add_subdirectory), single
#     picoboy static library shared with the GB/NES firmwares. On a machine
#     without that path it falls back to the bundled ./bsp (see SETUP.md).
#   - plan A: only the onboard hardware, no external I2C pad
#   - one target: galagino_arcade, currently DKONG + GALAGA
#
# Build on Windows: .\build.ps1
cmake_minimum_required(VERSION 3.13)

set(PB_BSP_DIR "E:/PicoBoy/bsp" CACHE PATH "PicoBoy-BSP (basar/CMakeLists.txt)")
# 换到别的电脑 / 用开发包时没有 E:\PicoBoy\bsp：回落到工程自带的 bsp/（见 SETUP.md）
if(NOT EXISTS "${PB_BSP_DIR}/CMakeLists.txt")
  set(PB_BSP_DIR "${CMAKE_CURRENT_LIST_DIR}/bsp" CACHE PATH "PicoBoy-BSP (bundled)" FORCE)
  message(STATUS "PB_BSP_DIR 未找到，回落到工程自带：${PB_BSP_DIR}")
endif()

include(${CMAKE_CURRENT_LIST_DIR}/pico_sdk_import.cmake)

project(galagino_arcade C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# ---- board support: the user's PicoBoy-BSP ----
# 16 MB board: PB_FLASH_SIZE_BYTES drives PICO_FLASH_SIZE_BYTES (pb_settings
# lives in the last sector) and is read by galagino's sys backend.
set(PB_FLASH_SIZE_BYTES 16777216 CACHE STRING "" FORCE)
# 资产区（ROM/图形/采样/logo）放在片内 flash 的固定偏移 0x120000，
# 即 XIP 地址 0x10120000：固件里只有指针，数据用 XIP 直接读，不占 RAM、
# 不解压。区内容由 tools/mkromregion.py 生成（rom/romregion.bin）。
set(GAL_ROM_BASE 0x10120000 CACHE STRING "" FORCE)
# ---- 面板：240x320 全幅 ----
# PB_SCREEN_MODE=1 选 1.69/2.0 寸 240x320 ST7789；PB_LCD_ROTATE=0 保持竖屏，
# 因为 galagino 的画布就是竖的 240x320（有效画面 240x288 居中，上下各 16 行黑边），
# 这样 1:1 无缩放、无裁剪，和 ESP 版几何完全一致。
set(PB_SCREEN_MODE 1 CACHE STRING "" FORCE)
set(PB_LCD_ROTATE 0 CACHE STRING "" FORCE)
# 面板反色：BSP 自己的 cache 变量，默认 1（发 0x21）。这块 240x320 屏实测不需要反色，
# 反了的话颜色是"负片"（红变青、绿变品红）。必须在这里用 FORCE 覆盖 BSP 的 cache 值，
# 否则两边各下一遍 -DPB_LCD_INVERT（后出现的赢），实测会退回到 =1。
set(PB_LCD_INVERT 0 CACHE STRING "LCD invert: 1=0x21 on, 0=0x20 off" FORCE)
# pb_fb 是 BSP 的帧缓冲，galagino 走条带直推（pb_display_push_rect）不用它：
# 缩到 2 条（224x16）只为省 RAM，264KB 要留给模拟器。
set(PB_FB_W 224 CACHE STRING "" FORCE)
set(PB_FB_H 16 CACHE STRING "" FORCE)

# ---- 音频输出后端（PWM / I2S 二选一）----
# 0 = 板载单 GPIO PWM 当 DAC：不需要任何外部器件，默认就是这版。
# 1 = PIO I2S：需要外接一颗 I2S DAC（MAX98357A / PCM5102 等），
#     BSP 用 pio1 输出，数据/位时钟引脚见 E:\PicoBoy\bsp\include\picoboy\board.h
#     （PB_PIN_I2S_DATA=13、PB_PIN_I2S_BCLK=14，LRCK 由同一状态机产生，PWM 引脚 11 不再使用）。
# 两种后端是同一套 API（pb_audio_init/submit/free）、同一 61035 帧/秒采样率、
# 同一 2048 帧环形缓冲，所以工程侧的重采样与流控代码一行都不用改，切换只影响底层。
set(PB_AUDIO_I2S "0" CACHE STRING "audio backend: 0=PWM DAC (onboard), 1=PIO I2S (external DAC)")
set_property(CACHE PB_AUDIO_I2S PROPERTY STRINGS 0 1)
if(PB_AUDIO_I2S)
  message(STATUS "音频输出后端：PIO I2S（需外接 I2S DAC，引脚 13/14）")
else()
  message(STATUS "音频输出后端：板载 PWM（单 GPIO）")
endif()

add_subdirectory(${PB_BSP_DIR} bsp)

# ---- 上机调试旋钮 ----
# 面板自检：开机先用 8 行条带把整块面板刷成 白/红/绿/蓝 四条色带，停 4 秒再回黑。
# 只走 pb_display_push_rect()，用来判断"屏幕通道是否通"。
option(GALAGINO_SELFTEST "boot with a 240x320 colour band test pattern" OFF)
# 主频与 LCD 位时钟（信号完整性/供电不稳时往下调）
set(GALAGINO_CLOCK_KHZ "250000" CACHE STRING "sys clock: 250000, 200000 or 133000")
set(PB_SPI_HZ_OVERRIDE "" CACHE STRING "override PB_LCD_SPI_HZ, e.g. 31250000")
if(PB_SPI_HZ_OVERRIDE)
  target_compile_definitions(picoboy PUBLIC PB_LCD_SPI_HZ=${PB_SPI_HZ_OVERRIDE})
endif()
# 面板反色已经在上面用 BSP 自己的 cache 变量 PB_LCD_INVERT 设成 0，这里不要再补定义

set(SRC ${CMAKE_CURRENT_LIST_DIR}/src)

# ---- galagino sources ----
# Games are switched by GAME_<NAME> (catalog.h); dkong and galaga are the two
# whose romconv assets are in the tree.
set(GAMES)
set(GAME_DEFS)

macro(add_game name)
  list(APPEND GAMES ${SRC}/machines/${name}/${name}.cpp)
  list(APPEND GAME_DEFS GAME_${name})
endmacro()

add_game(dkong)
add_game(galaga)
add_game(1942)
add_game(pacman)
add_game(yiear)
add_game(contra)
add_game(gng)
# xevious 也有完整 CPU ROM 了，本体进菜单（sxevious 继承它，必须一起编）
add_game(xevious)
add_game(sxevious)
add_game(1943)
add_game(commando)
add_game(gberet)
# galaga's sound board is the shared Namco 54xx core
list(APPEND GAMES ${SRC}/machines/namco54/namco54_program.cpp)
# machineBase is header-only: tile addressing and the DVG vector base
list(APPEND GAMES ${SRC}/machines/tileaddr.cpp ${SRC}/machines/dvgMachine.cpp)

# recursive: the CPU cores (Z80, i8048, m6502, mb88xx, ...) and the LZMA
# decoder live in subdirectories
file(GLOB_RECURSE CORE_SRC
  ${SRC}/cpus/*.c ${SRC}/cpus/*.cpp
  ${SRC}/emulation/*.c ${SRC}/emulation/*.cpp
  ${SRC}/input/*.cpp
  ${SRC}/machines/menu/*.cpp
)
file(GLOB HAL_SRC ${SRC}/hal/rp2040/*.cpp ${SRC}/hal/rp2040/*.c)

add_executable(galagino_arcade
  ${SRC}/main.cpp
  ${GAMES}
  ${CORE_SRC}
  ${HAL_SRC}
)

# 崩溃取证要在屏上画文字，用的是 hud.cpp 里的 8x8 字库 + hud_text()。那两样只在
# ASSETS_REMOTE / BOOT_SELFTEST / DEBUG_TIMING_FPS_HUD 下才编译，这里只给 hud.cpp
# 这一个翻译单元开 ASSETS_REMOTE（不会把网络加载代码带进来）。
set_source_files_properties(${SRC}/emulation/hud.cpp PROPERTIES COMPILE_DEFINITIONS "ASSETS_REMOTE")

string(TOUPPER "${GAME_DEFS}" GAME_DEFS_UP)

# 全幅 60 Hz 实验开关。TFT_SPICLK 在 galagino 里只是个"面板带宽"语义值，
# 唯一用途是让 config.h 判断 #if TFT_SPICLK < 80000000 -> VIDEO_HALF_RATE。
# 真正的 PIO 位时钟由 BSP 的 PB_LCD_SPI_HZ 决定（见上面 PB_SPI_HZ_OVERRIDE）：
# 所以把这里记成 80 MHz 就能关掉半速、每帧刷整屏，而线上实际跑多快仍由
# PB_SPI_HZ_OVERRIDE 说了算——打开本开关时请把它提到 >= 71.4 MHz
# （250 MHz / (2 * 1.75)），否则物理上推不完 138 KB。
option(GALAGINO_FULL_RATE "keep VIDEO_HALF_RATE off (full-rate screen, needs >= 71.4 MHz SPI)" OFF)
if(GALAGINO_FULL_RATE)
  set(GALAGINO_TFT_SPICLK_VALUE 80000000)
else()
  set(GALAGINO_TFT_SPICLK_VALUE 62500000)
endif()

target_compile_definitions(galagino_arcade PRIVATE
  ${GAME_DEFS_UP}
  HAL_RP2040
  BOARD_PICOBOY
  # 62.5 MHz SPI：一屏 224x288x2 = 129 KB，约 16.5 ms 在线上传，正好卡在
  # 16.7 ms 的 60 Hz 帧预算上，所以 config.h 由 TFT_SPICLK<80MHz 自动开
  # VIDEO_HALF_RATE：每两游戏帧刷一次屏（30 Hz），模拟器仍跑 60 Hz。
  # GALAGINO_FULL_RATE=ON 时这个值变 80 MHz，半速不再打开（见上面的说明）。
  TFT_SPICLK=${GALAGINO_TFT_SPICLK_VALUE}
  # Arena：ROM / 图形 / 采样 / logo 都搬去了 rom/romregion.bin，落在
  # flash 0x10120000，固件里只有指针（XIP 直读），设备侧完全不解包，
  # Arena 只剩菜单鼓形查表。8 KB 足够，留点余量。
  ARENA_A_SIZE=69632
  ARENA_B_SIZE=0
  # 资产区基址（XIP 地址），由 tools/mkromregion.py 写进各头文件
  GAL_ROM_BASE=${GAL_ROM_BASE}u
  # core stacks: galagino's call chain (Z80 step -> machine -> video) is deeper
  # than the 2 KB SDK default
  PICO_STACK_SIZE=0x2000
  PICO_CORE1_STACK_SIZE=0x1000
)

target_include_directories(galagino_arcade PRIVATE ${SRC})

if(GALAGINO_SELFTEST)
  target_compile_definitions(galagino_arcade PRIVATE GALAGINO_VIDEO_SELFTEST)
endif()
# 调试开关：完全不启用硬件看门狗，挂死时画面冻结而不是整机复位
option(GALAGINO_NO_WDT "disable the hardware watchdog (debug: freeze instead of reboot)" OFF)
if(GALAGINO_NO_WDT)
  target_compile_definitions(galagino_arcade PRIVATE GALAGINO_NO_WDT)
endif()
# 调试开关：往 USB 串口打阶段日志（打开任一串口终端即可看到）
option(GALAGINO_TRACE "printf stage traces to USB serial" OFF)
if(GALAGINO_TRACE)
  target_compile_definitions(galagino_arcade PRIVATE GALAGINO_TRACE)
endif()
# 调试开关：帧耗时打点。把一帧拆成 sim(模拟CPU) / render(绘制) / push(SPI推送，阻塞)
# / audio(混音) / wait(节拍等待) 五段，每 30 帧往 USB 串口打一行，用来判断瓶颈在算力还是显示带宽。
option(GALAGINO_FRAME_TIMING "print per-frame timing breakdown to USB serial" OFF)
if(GALAGINO_FRAME_TIMING)
  target_compile_definitions(galagino_arcade PRIVATE GAL_FRAME_TIMING)
endif()
# 性能开关：音频合成（FM/AY）挪到模拟核 core1 的空闲时段，视频核只把成品样本喂给 PWM。
# core1 每帧 run_frame() 之后本来就在等下一帧信号，音频合成正好填这段空档，
# 于是合成与 SPI 刷屏并行（1943 / sxevious 掉帧的主因）。
option(GALAGINO_AUDIO_CORE1 "render audio on the emulation core, overlapping the SPI pushes" ON)
if(GALAGINO_AUDIO_CORE1)
  target_compile_definitions(galagino_arcade PRIVATE AUDIO_RENDER_ON_EMU_CORE)
endif()
# 极速版开关：整条声音链路不参与执行——合成不跑、PWM/DMA 不启动、喂样本的调用也省掉。
# 声音芯片的寄存器写入仍发生在 run_frame() 里（那是模拟的一部分），所以关掉音频
# 不会让游戏逻辑走样，只是不再有声音输出，并把音频占用的 CPU 与 DMA 中断全部省下。
option(GALAGINO_AUDIO "produce and output sound; OFF = silent build with audio path compiled out" ON)
if(NOT GALAGINO_AUDIO)
  target_compile_definitions(galagino_arcade PRIVATE GAL_AUDIO_OFF)
endif()
# boards/picoboy.h 用 PB_SYS_CLOCK_KHZ，entry.cpp 用它超频
target_compile_definitions(galagino_arcade PRIVATE PB_SYS_CLOCK_KHZ=${GALAGINO_CLOCK_KHZ})

target_compile_options(galagino_arcade PRIVATE -O2 -w)

target_link_libraries(galagino_arcade PRIVATE
  picoboy
  pico_stdlib
  pico_multicore
  pico_rand
  hardware_exception
)

pico_enable_stdio_usb(galagino_arcade 1)
pico_enable_stdio_uart(galagino_arcade 0)

# 16 MB flash (SDK default memmap is 2 MB)
set_target_properties(galagino_arcade PROPERTIES
  PICO_TARGET_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/memmap_pico16m.ld)

pico_add_extra_outputs(galagino_arcade)

# 固件必须止步于资产区之前（默认 .text 只几百 KB，超出会踩到 ROM 区）。
# 必须排在 pico_add_extra_outputs 之后：它检查的 .bin 是上面那条命令里 objcopy
# 产出的，顺序反过来时新目录首次构建会直接报"固件 bin 不存在"。
add_custom_command(TARGET galagino_arcade POST_BUILD
  COMMAND ${CMAKE_COMMAND} -DMAP=$<TARGET_FILE_DIR:galagino_arcade>/galagino_arcade.bin
          -DMAX=786432
          -P ${CMAKE_CURRENT_LIST_DIR}/cmake/check_fw_size.cmake)



