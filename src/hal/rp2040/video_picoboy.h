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

private:
  void blackBars(uint16_t width);

  static const uint8_t STRIP_ROWS = 8;
  static const uint16_t PICTURE_ROWS = 288;

  uint16_t viewport_width = 224;
  uint16_t strips_done = 0;  // strips written since the screen started
  bool half_rate = false;
  bool flip_y = false;
  bool flip_x = false;
  uint8_t madctl = 0;
  FramePacer pacer;
};

} // namespace hal

#endif
