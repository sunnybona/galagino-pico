// Program entry (RP2040): the SDK's stock boot, no FreeRTOS scheduler. Core 0
// runs galagino_setup()/galagino_loop(); the emulation task is started on core
// 1 by hal::taskSpawn() from emulation_start().
#include "../../config.h"
#include "../entry.h"

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"

#ifdef GALAGINO_TRACE
#include <stdio.h>
#include <string.h>
#include "hardware/exception.h"
#include "picoboy/pb_display.h"

// hud.cpp 里的 8x8 小字库（该文件用 ASSETS_REMOTE 单独编译出这两个符号）
extern void hud_text(uint16_t *frame_buffer, uint16_t width, uint8_t row, uint8_t flip,
                     uint16_t x, uint16_t y, uint16_t color, const char *txt);

namespace {

void hex8(char *dst, uint32_t v) {
  for (int i = 0; i < 8; i++) {
    const uint32_t d = (v >> (28 - 4 * i)) & 0xF;
    dst[i] = (char)(d < 10 ? '0' + d : 'A' + (d - 10));
  }
}

// 崩溃时把 PC / LR 直接画在屏上（串口在这块板上不通，屏幕是唯一可靠的信道）
void faultPaint(uint32_t pc, uint32_t lr) {
  static uint16_t buf[240 * 8];
  static const uint16_t WHITE = 0xFFFF;
  char linePc[12], lineLr[12];
  linePc[0] = 'P'; linePc[1] = 'C'; linePc[2] = ' ';
  hex8(linePc + 3, pc);
  linePc[11] = 0;
  lineLr[0] = 'L'; lineLr[1] = 'R'; lineLr[2] = ' ';
  hex8(lineLr + 3, lr);
  lineLr[11] = 0;

  for (;;) {
    for (uint8_t row = 0; row < 40; row++) {
      memset(buf, 0, sizeof(buf));
      hud_text(buf, 240, row, 0, 8, 96, WHITE, "HARDFAULT");
      hud_text(buf, 240, row, 0, 8, 136, WHITE, linePc);
      hud_text(buf, 240, row, 0, 8, 176, WHITE, lineLr);
      pb_display_push_rect(0, row * 8, 240, 8, buf);
    }
    for (volatile uint32_t i = 0; i < 2000000u; i++) {
    }
  }
}

} // namespace

extern "C" void faultPaintEntry(uint32_t pc, uint32_t lr) { faultPaint(pc, lr); }

// 取异常压栈帧：handler 入口时 MSP 指向 {r0,r1,r2,r3,r12,lr,pc,xpsr}
__attribute__((naked)) static void faultIsr(void) {
  __asm volatile(
      "mov r0, sp        \n"
      "ldr r1, [r0, #24] \n"   // stacked PC
      "ldr r2, [r0, #20] \n"   // stacked LR
      "mov r0, r1        \n"
      "mov r1, r2        \n"
      "bl  faultPaintEntry\n"
      "b   .             \n");
}
#endif

int main() {
#ifdef GALAGINO_CLOCK_KHZ
  // 250 MHz is above the 133 MHz default: raise the core voltage first
  vreg_set_voltage(VREG_VOLTAGE_1_20);
  sleep_ms(2);
  set_sys_clock_khz(GALAGINO_CLOCK_KHZ, true);
#endif
#ifdef GALAGINO_TRACE
  exception_set_exclusive_handler(HARDFAULT_EXCEPTION, faultIsr);
  printf("\n[boot] sys_clk=%lu Hz\n", (unsigned long)clock_get_hz(clk_sys));
#endif

  galagino_setup();
#ifdef GALAGINO_TRACE
  printf("[boot] galagino_setup() returned, entering loop\n");
#endif
  for (;;) {
    galagino_loop();
  }
}
