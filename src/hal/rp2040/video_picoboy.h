#ifndef HAL_RP2040_VIDEO_PICOBOY_H
#define HAL_RP2040_VIDEO_PICOBOY_H

#include <stdint.h>
#include "../../config.h"
#include "../frame_pacer.h"

namespace hal {

// PicoBoy: ST7789 240 x 320 through PicoBoy-BSP 2.0 (PIO + DMA).
//
// galagino renders a 240 x 288 picture as 36 strips of 8 rows, 224 columns
// (the arcade screen, 8 px back porch each side). The 240 x 320 panel is the
// full canvas, so the picture goes up 1:1, no scaling and no cropping: the
// window is (TFT_X_OFFSET, TFT_Y_OFFSET, width, 288), the same geometry the
// ESP build uses, leaving a 16 px black bar top and bottom.
//
//   submit(strip)  -> pb_display_push_rect() puts those 8 rows on the panel
//   endFrame()     -> half rate only: sleeps to the 16/33 ms part deadline
//
// No frame buffer is involved: the picture is streamed strip by strip, so the
// BSP's pb_fb is sized tiny in this build and the RP2040's 264 KB stay free
// for the emulation (the ESP build needs a 115 KB frame buffer).
class PicoboyVideo {
public:
  // game pixels available: the whole panel
  enum : uint16_t { CANVAS_W = 240, CANVAS_H = 320 };
  // widest strip (video_width.h): the panel width
  enum : uint16_t { MAX_WIDTH = 240 };

  void begin();
  void setViewport(uint16_t width);
  void flip(char flipY, char flipX);
  void flipReset(char flipY, char flipX);

  void setHalfRate(bool half);
  uint8_t screenParts() const;

  void submit(const uint16_t *strip, uint32_t pixels);
  void endFrame(uint8_t part, uint32_t frameStartUs);
  uint8_t stripIndex(uint8_t i) const { return i; }

  // 上一次 endFrame 里推块花的时间（含等上一块传完），给帧耗时打点用
  uint32_t lastPushUs() const { return last_push_us; }

private:
  void blackBars(uint16_t width);
  void pushBlock();

  static const uint8_t STRIP_ROWS = 8;
  static const uint16_t PICTURE_ROWS = 288;

  uint16_t viewport_width = 224;
  uint16_t strips_done = 0;  // strips written since the screen started
  bool half_rate = false;
  bool flip_y = false;
  bool flip_x = false;
  uint8_t madctl = 0;
  FramePacer pacer;

  // 异步块推送：条带先打包进一块（48 行）的缓冲，一块凑齐再交给 DMA，
  // 这样 SPI 传输下一块的同时 CPU 可以继续渲染。block_words == 0 表示当前块空。
  uint32_t block_words = 0;
  uint16_t block_row = 0;      // 当前块在画面里的起始行
  uint16_t block_width = 0;    // 当前块的条带宽度
  uint8_t block_buf = 0;       // 正在填充的缓冲索引
  uint32_t last_push_us = 0;
};

} // namespace hal

#endif
