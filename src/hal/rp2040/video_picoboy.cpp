#include "video_picoboy.h"

#include "picoboy/pb_display.h"
#include "rtos.h"
#include "time.h"

#include <stdint.h>

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

// 一块 = 2 条带（16 行），像素对按面板最宽 240 列留足：
//   2 * 8 * 240 / 2 = 1,920 words = 7,680 B，双缓冲合计 15,360 B。
// 粒度定得这么细，是因为静态区每多占 1 KB，malloc 就少 1 KB：pico 的 _sbrk
// 上限是 __StackLimit（0x20040000），堆起点就是 end。早期用「半个画面」一块
// （2 × 69 KB）把可用堆从 240 KB 压到 102 KB，切 1943 直接 panic Out of memory。
// 16 行一块时传输约 0.98 ms、渲染约 0.93 ms，两者仍能互相覆盖，整屏 17.7 ms
// 的 SPI 时间照样被藏进渲染时间里。
static constexpr uint32_t kBlockWords = 2u * 8u * 240u / 2u;
static uint32_t s_blockBuf[2][kBlockWords];

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
  // blackBars 走同步直推，先把在飞的块推完，免得两路共用同一 SPI 状态机
  pushBlock();
  pb_display_push_flush();
  if (viewport_width == 240 && width == 224) blackBars(224);
  viewport_width = width;
}

// galagino flips the picture through the panel's MADCTL, so nothing is
// re-rendered: MX mirrors left/right, MY up/down, both rotate by 180.
void PicoboyVideo::flip(char fY, char fX) {
  if (flip_y == (fY != 0) && flip_x == (fX != 0)) return;
  // MADCTL 命令要独占 SPI：先把在飞的块推完
  pushBlock();
  pb_display_push_flush();
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
// to the SPI DMA, so high byte first). The panel wants each value high byte
// first AND packed two pixels per 32-bit word ((c0 << 16) | c1), so one REV
// instruction per pixel pair does both jobs at once.
static void packPairs(uint32_t *dst, const uint16_t *strip, uint32_t pixels) {
  const bool aligned = ((((uintptr_t)strip | (uintptr_t)dst) & 3u) == 0);
  uint32_t i = 0;
  if (aligned) {
    for (; i + 1 < pixels; i += 2) {
      // little endian word = [a][b]; REV reverses the four bytes, which turns
      // it into (swap(a) << 16) | swap(b) -- exactly mk_pair(swap(a), swap(b))
      uint32_t w = ((const uint32_t *)strip)[i >> 1];
      asm volatile("rev %0, %1" : "=l"(w) : "l"(w));
      dst[i >> 1] = w;
    }
  }
  for (; i < pixels; i++) {
    const uint16_t v = strip[i];
    dst[i >> 1] = (uint32_t)(uint16_t)((v >> 8) | (v << 8)) << 16;
  }
}

void PicoboyVideo::pushBlock() {
  if (block_words == 0) {
    last_push_us = 0;
    return;
  }
  const uint32_t t = hal::micros();
  const int w = (int)block_width;
  const int h = (int)(block_words * 2 / w);
  // pb_display_push_start 内部会先等上一块传完（改窗口必须等 SPI 空闲），
  // 所以这里的耗时同时包含"等上一块"——那部分同样是 CPU 花在显示上的时间。
  pb_display_push_start((240 - w) / 2, TFT_Y_OFFSET + block_row, w, h);
  pb_display_push_words(s_blockBuf[block_buf], block_words);
  block_buf ^= 1;
  block_words = 0;
  block_width = 0;
  last_push_us = hal::micros() - t;
}

void PicoboyVideo::submit(const uint16_t *strip, uint32_t pixels) {
  const uint16_t width = (uint16_t)(pixels / STRIP_ROWS);
  const uint16_t row0 = (uint16_t)(strips_done * STRIP_ROWS);
  if (width == 0 || width > 240 || row0 + STRIP_ROWS > PICTURE_ROWS) return;
  if (pixels > 240 * STRIP_ROWS) return;

  const uint32_t words = pixels / 2;      // pixels is always even (width * 8)

  // 一个块 = 半个画面（18 条带 = 144 行），宽 224 时 16,128 words；
  // 宽 240 时正好填满 17,280 words 并自动提前开新块。
  if ((width & 1) || block_words + words > kBlockWords) pushBlock();

  if ((width & 1) || words > kBlockWords) {
    // 落不进块缓冲的病态调用（奇数宽度）：退回每像素换序 + 同步直推。
    // 注意这里必须给 BSP 原始的 RGB565（它自己会做像素对打包），不能传打包结果。
    static uint16_t xfer[240 * STRIP_ROWS];
    for (uint32_t k = 0; k < pixels; k++) {
      const uint16_t v = strip[k];
      xfer[k] = (uint16_t)((v >> 8) | (v << 8));
    }
    pb_display_push_rect((240 - width) / 2, TFT_Y_OFFSET + row0, width, STRIP_ROWS, xfer);
    strips_done++;
    return;
  }

  if (block_words == 0) {
    block_width = width;
    block_row = row0;
  }
  packPairs(s_blockBuf[block_buf] + block_words, strip, pixels);
  block_words += words;
  strips_done++;
}

void PicoboyVideo::endFrame(uint8_t part, uint32_t frameStartUs) {
  // 这个 part 的条带已经全部打包好：交给 DMA，后面的节拍睡眠与下一块的
  // 渲染/打包都会和这次传输重叠。
  pushBlock();

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
