#include "watchdog.h"
#include "pico/stdlib.h"
#include "hardware/watchdog.h"

namespace hal {
namespace {
bool armed = false;
}

void watchdogWatch(TaskHandle, uint32_t seconds) {
#ifdef GALAGINO_NO_WDT
  // 调试用：完全不启用看门狗。挂死时画面会冻在最后一帧，而不是整机复位，
  // 这样能看清是卡在哪一步（移植初期定位问题用，编译时定义 GALAGINO_NO_WDT）。
  (void)seconds;
  armed = false;
#else
  if (seconds > 8) {
    seconds = 8;
  }
  // a hung emulation task resets the chip; the boot itself is not watched
  watchdog_enable(seconds * 1000000u, true);
  armed = true;
#endif
}

void watchdogFeed() {
  if (armed) {
    watchdog_update();
  }
}

void watchdogUnwatch(TaskHandle) {
  // RP2040 has no way to stop the watchdog once started: disarm our feed
  // instead, so the game does not keep re-arming a watchdog the next one
  // should be watching
  armed = false;
}

} // namespace hal
