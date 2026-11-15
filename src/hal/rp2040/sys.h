#ifndef HAL_RP2040_SYS_H
#define HAL_RP2040_SYS_H

#include <stdint.h>

namespace hal {

struct SysInfo {
  const char *chip;
  uint32_t revision;
  uint32_t cores;
  uint32_t cpuMhz;
  uint32_t flashBytes;
  const char *sdkVersion;
};

struct MemInfo {
  uint32_t free;
  uint32_t largest;   // not tracked separately: equals free
  uint32_t minFree;   // lowest free since boot (not tracked: equals free)
};

void sysInfo(SysInfo &out);

MemInfo memInfo();
MemInfo memInfoData();
void memDump();

void *allocSpare(uint32_t bytes);

// Console (USB serial) ready for printf; waits up to 2 s for a host.
void consoleInit();

void printPlatformInfo();

// Not supported: supported is set to false.
bool imageVerify(bool &supported);

uint32_t random32();

// Crash record: stored in watchdog scratch before the reset.
static const uint32_t CRASH_STACK = 1;
static const uint32_t CRASH_FAULT = 2;
static const uint32_t CRASH_PANIC = 3;
[[noreturn]] void crashReboot(uint32_t kind, uint32_t a, uint32_t b);

bool resetWasPanic();

} // namespace hal

#endif // HAL_RP2040_SYS_H
