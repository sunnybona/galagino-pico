#include "video_picoboy.h"

#include "picoboy/pb_display.h"
#include "rtos.h"
#include "time.h"

#ifdef GALAGINO_TRACE
#include <stdio.h>
#define PBTR(...) printf(__VA_ARGS__)
// 逐段进度标记：整屏填一种颜色停 400 ms。屏幕已知可达，比串口可靠。
static void pbStage(uint16_t color) {
  pb_fill_screen(color);
  hal::sleepMs(400);
}
#else
#define PBTR(...) ((void)0)
#endif

namespace hal {

#ifdef GALAGINO_VIDEO_SELFTEST
// 面板探针：整块 240x320 用 8 行条带画出来，图案能一次读出四类错误：
//   左上红 | 右上绿        左右颜色反了 -> MADCTL/MX 或 GRAM 列方向错
//   左下蓝 | 右下黄        上下颜色反了 -> MY/RASET 或行方向错
//   顶部 16 行深灰         = ESP 版黑边的位置，用来核对 TFT_Y_OFFSET
//   每 64 行一条白线       = 行标尺，线条不齐/斜 -> SPI 信号或窗口错位
static void panelSelftest() {
  static uint16_t row[240 * 8];   // 8 行 x 240 列，push_rect 一次吃一个条带
  for (uint16_t y = 0; y < 320; y += 8) {
    for (uint16_t i = 0; i < 8; i++) {
      const uint16_t yy = (uint16_t)(y + i);
      for (uint16_t x = 0; x < 240; x++) {
        uint16_t c;
        if (yy < 16)          c = 0x8410;                          // 深灰：黑边区
        else if (yy % 64 == 0) c = 0xFFFF;                         // 1 行白标尺
        else if (yy < 160)    c = (x < 120) ? 0xF800 : 0x07E0;     // 红 | 绿
        else                  c = (x < 120) ? 0x001F : 0xFFE0;     // 蓝 | 黄
        row[i * 240 + x] = c;
      }
    }
    pb_display_push_rect(0, y, 240, 8, row);
  }
}
#endif

#ifdef GALAGINO_TRACE
// 每帧走一格的白色方块：贴在底部黑边区（288..319）里，不压画面。
// 它还在动 = core0 仍在出帧（问题在画面内容）；它冻住 = 真的死锁了。
static void pbHeartbeat(uint32_t frame) {
  static const uint16_t white[8 * 8] = {
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
  };
  static uint16_t lastX = 0xFFFF;
  const uint16_t x = (uint16_t)((frame % 30) * 8);
  if (x == lastX) return;
  lastX = x;
  pb_display_push_rect(x, 304, 8, 8, white);
}
#endif

void PicoboyVideo::begin() {
  pb_display_init();          // PIO, DMA, ST7789 init, panel cleared to black
#ifdef GALAGINO_VIDEO_SELFTEST
  // 探针固件不进模拟器。
  // 第 1 段：整屏填充（BSP 原生路径 pb_fill_screen）红 2 秒、绿 2 秒
  // 第 2 段：四象限图案用 push_rect 一直重画
  // 两段对比能直接分辨"整屏填充不通"还是"多窗口直推不通"。
  pb_fill_screen(0xF800);
  hal::sleepMs(2000);
  pb_fill_screen(0x07E0);
  hal::sleepMs(2000);
  for (;;) {
    panelSelftest();
    hal::sleepMs(1000);
  }
#endif
  madctl = PB_LCD_MADCTL;
  viewport_width = 240;       // force the 224 switch in setViewport
  setViewport(224);
  PBTR("[video] begin done, madctl=0x%02x\n", (unsigned)madctl);
#ifdef GALAGINO_TRACE
  pbStage(0xF800);            // 1 红：显示后端初始化完成
#endif
}

// 240 -> 224: the side bars are painted once, every later screen is 224 wide
void PicoboyVideo::blackBars(uint16_t width) {
  static const uint16_t zeros[STRIP_ROWS * 8] = { 0 };
  if (width != 224) return;
  for (int side = 0; side < 2; side++) {
    const int x = side ? 240 - 8 : 0;
    for (int r = 0; r < PICTURE_ROWS; r += STRIP_ROWS)
      pb_display_push_rect(x, TFT_Y_OFFSET + r, 8, STRIP_ROWS, zeros);
  }
}

void PicoboyVideo::setViewport(uint16_t width) {
  if (width != 240) width = 224;
  if (width == viewport_width) return;
  if (viewport_width == 240 && width == 224) blackBars(224);
  viewport_width = width;
}

// galagino flips the picture through the panel's MADCTL, so nothing is
// re-rendered: MX mirrors left/right, MY up/down, both rotate by 180.
void PicoboyVideo::flip(char fY, char fX) {
  if (flip_y == (fY != 0) && flip_x == (fX != 0)) return;
  flip_y = fY != 0;
  flip_x = fX != 0;
  madctl = PB_LCD_MADCTL ^ (flip_y ? 0x80 : 0) ^ (flip_x ? 0x40 : 0);
  pb_display_set_madctl(madctl);
}

void PicoboyVideo::flipReset(char fY, char fX) {
  flip(0, 0);
}

void PicoboyVideo::setHalfRate(bool half) {
#ifdef VIDEO_HALF_RATE
  half_rate = true;  // a 62.5 MHz panel cannot take a 240 x 288 screen at 60 Hz
#else
  half_rate = half;
#endif
}

uint8_t PicoboyVideo::screenParts() const {
  return half_rate ? 2 : 1;
}

// one strip: width x 8 rows, RGB565, row major.
// galagino keeps its pixels BYTE SWAPPED (the ESP backend hands the raw bytes
// to the SPI DMA, so high byte first). pb_display_push_rect() sends the high
// byte of each uint16 first, so the values are swapped back here.
void PicoboyVideo::submit(const uint16_t *strip, uint32_t pixels) {
  static uint16_t xfer[240 * STRIP_ROWS];
  const uint16_t width = (uint16_t)(pixels / STRIP_ROWS);
  const uint16_t row0 = (uint16_t)(strips_done * STRIP_ROWS);
  if (width == 0 || width > 240 || row0 + STRIP_ROWS > PICTURE_ROWS) return;
  if (pixels > 240 * STRIP_ROWS) return;

  for (uint32_t i = 0; i < pixels; i++) {
    const uint16_t v = strip[i];
    xfer[i] = (uint16_t)((v >> 8) | (v << 8));
  }

  pb_display_push_rect((240 - width) / 2, TFT_Y_OFFSET + row0, width, STRIP_ROWS, xfer);
  static uint32_t traced = 0;
  if (traced < 24) {
    traced++;
    PBTR("[video] strip %lu row0=%u w=%u\n", (unsigned long)traced, (unsigned)row0, (unsigned)width);
#ifdef GALAGINO_TRACE
    if (traced == 1) pbStage(0xFD20);   // 2 橙：第一条条带推上屏
#endif
  }
  strips_done++;
}

void PicoboyVideo::endFrame(uint8_t part, uint32_t frameStartUs) {
  if (!half_rate) {
    const int32_t wait_us = pacer.next(hal::micros());
    if (wait_us > 0) hal::sleepMs((uint32_t)((wait_us + 999) / 1000));
  } else {
    // the screen is updated every second game frame: parts end at 16 and 33 ms
    const uint32_t t1 = (hal::micros() - frameStartUs) / 1000;
    const uint32_t deadline = part ? 33 : 16;
    if (t1 < deadline) hal::sleepMs(deadline - t1);
    else if (part) hal::sleepMs(1);
  }

  if (part + 1 >= screenParts()) {
    strips_done = 0;
    static uint32_t frames = 0;
    static uint32_t lastMs = 0;
    frames++;
#ifdef GALAGINO_TRACE
    // 3 黄 / 4 绿 / 5 青 / 6 蓝 / 7 白：整帧完成到第 N 帧
    if (frames == 1) pbStage(0xFFE0);
    else if (frames == 3) pbStage(0x07E0);
    else if (frames == 8) pbStage(0x07FF);
    else if (frames == 20) pbStage(0x001F);
    else if (frames == 50) pbStage(0xFFFF);
    pbHeartbeat(frames);
#endif
    const uint32_t now = hal::millis();
    if (now - lastMs >= 2000) {
      PBTR("[video] frames=%lu (%lu ms)\n", (unsigned long)frames, (unsigned long)(now - lastMs));
      lastMs = now;
    }
  }
}

} // namespace hal
