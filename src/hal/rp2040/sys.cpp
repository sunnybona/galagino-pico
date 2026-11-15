#include "sys.h"
#include "time.h"
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "pico/version.h"
#include "hardware/clocks.h"
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"
#include <malloc.h>
#include <stdio.h>

namespace hal {

void sysInfo(SysInfo &out) {
  out.chip = "RP2040";
  out.revision = 0;
  out.cores = 2;
  out.cpuMhz = (uint32_t)(clock_get_hz(clk_sys) / 1000000u);
  out.flashBytes = (uint32_t)PICO_FLASH_SIZE_BYTES;
  out.sdkVersion = PICO_SDK_VERSION_STRING;
}

MemInfo memInfo() {
  struct mallinfo mi = mallinfo();
  MemInfo m;
  m.free = (uint32_t)mi.fordblks;
  m.largest = m.free;
  m.minFree = m.free;
  return m;
}

MemInfo memInfoData() { return memInfo(); }

void memDump() {
  MemInfo m = memInfo();
  printf("heap: free=%u\n", (unsigned)m.free);
}

void *allocSpare(uint32_t bytes) { return malloc(bytes); }

void consoleInit() {
  stdio_init_all();
  const uint32_t start = millis();
  while (!stdio_usb_connected() && (uint32_t)(millis() - start) < 2000u) {
    tight_loop_contents();
  }
}

void printPlatformInfo() {
  SysInfo si;
  sysInfo(si);
  printf("platform: %s, %u cores at %u MHz, %u KB flash, SDK %s\n", si.chip,
         (unsigned)si.cores, (unsigned)si.cpuMhz, (unsigned)(si.flashBytes / 1024),
         si.sdkVersion);
  if (resetWasPanic()) {
    printf("last reset was a panic\n");
  }
}

bool imageVerify(bool &supported) {
  supported = false;
  return true;
}

uint32_t random32() { return get_rand_32(); }

void crashReboot(uint32_t kind, uint32_t a, uint32_t b) {
  watchdog_hw->scratch[7] = 0x47414c47u;  // 'GALG'
  watchdog_hw->scratch[6] = kind;
  watchdog_hw->scratch[5] = a;
  watchdog_hw->scratch[4] = b;
  watchdog_reboot(0, 0, 0);
  for (;;) {
    tight_loop_contents();
  }
}

bool resetWasPanic() { return watchdog_hw->scratch[7] == 0x47414c47u; }

} // namespace hal
