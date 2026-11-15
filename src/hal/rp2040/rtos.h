#ifndef HAL_RP2040_RTOS_H
#define HAL_RP2040_RTOS_H

#include <stdint.h>

// Bare metal two core shim instead of FreeRTOS (an RP2040 has neither the RAM
// nor a proven SMP port for galagino): core 0 runs galagino_setup/loop, core 1
// runs the emulation task. The interface is the FreeRTOS one galagino uses.
namespace hal {

// The video/audio loop runs on core 0, the emulation task on core 1.
static const int VIDEO_CORE = 0;
static const int EMULATION_CORE = 1;

static const uint32_t WAIT_FOREVER = 0xFFFFFFFFu;

typedef void *TaskHandle;

// Core 1 is started on the first taskSpawn() there and then keeps a trampoline
// alive: every later spawn (a new game) is another run of that core's function.
// Returns NULL only if the core could not be started.
TaskHandle taskSpawn(void (*fn)(void *), void *arg, const char *name,
                     uint32_t stackBytes, uint32_t priority, int core);

// Ends the calling task (core 1: back to the trampoline, ready for a respawn).
void taskExit();

void yield();

int coreId();

uint32_t rtosHeapFree();
uint32_t rtosHeapMinFree();

// At least one tick for any non-zero request: never returns without giving the
// other core time.
void sleepMs(uint32_t ms);

// Latched event between the cores: give() sets, take() consumes. Several
// gives before a take count once.
class Signal {
public:
  Signal();
  void give();
  bool take(uint32_t timeoutMs);
  void clear();

private:
  volatile int pending;
};

// One process-wide lock, safe across cores. Keep sections short (a hardware
// spinlock, interrupts stay enabled).
class CriticalSection {
public:
  CriticalSection();
  ~CriticalSection();
};

} // namespace hal

#endif // HAL_RP2040_RTOS_H
