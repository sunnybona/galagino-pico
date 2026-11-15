#include "rtos.h"
#include "time.h"

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/sync.h"
#include "hardware/structs/sio.h"

#include <setjmp.h>
#include <stdint.h>
#include <malloc.h>

#ifdef GALAGINO_TRACE
#include <stdio.h>
#define RTTR(...) printf(__VA_ARGS__)
#else
#define RTTR(...) ((void)0)
#endif

namespace hal {
namespace {

// --- core 1 trampoline ------------------------------------------------------
// One run of the core 1 function at a time. taskExit() longjmps back here, so a
// new game can spawn a fresh emulation task without rebooting the chip.
jmp_buf trampoline;
void (*core1Fn)(void *) = nullptr;
void *core1Arg = nullptr;
volatile int core1State = 0;   // 0 idle, 1 armed, 2 running
bool core1Started = false;

void core1Main() {
  RTTR("[rtos] core1 alive\n");
  for (;;) {
    while (core1State != 1) {
      __wfe();
    }
    core1State = 2;
    RTTR("[rtos] core1 running task fn=%p arg=%p\n", (void *)core1Fn, core1Arg);
    if (setjmp(trampoline) == 0) {
      core1Fn(core1Arg);
    }
    RTTR("[rtos] core1 task returned/exited\n");
    core1State = 0;
    __sev();
  }
}

// --- one cross-core lock ----------------------------------------------------
spin_lock_t *getLock() {
  static spin_lock_t *lock = nullptr;
  if (!lock) {
    lock = spin_lock_instance(spin_lock_claim_unused(true));
  }
  return lock;
}

} // namespace

TaskHandle taskSpawn(void (*fn)(void *), void *arg, const char *name,
                     uint32_t stackBytes, uint32_t priority, int core) {
  (void)name;
  (void)stackBytes;   // core 1 stack is PICO_CORE1_STACK_SIZE at build time
  (void)priority;
  if (core != EMULATION_CORE) {
    return nullptr;
  }

  RTTR("[rtos] taskSpawn fn=%p stack=%lu started=%d\n", (void *)fn,
       (unsigned long)stackBytes, (int)core1Started);

  if (!core1Started) {
    core1Fn = fn;
    core1Arg = arg;
    multicore_launch_core1(core1Main);
    core1Started = true;
    core1State = 1;
  } else {
    // wait for the previous run to unwind, then hand over the new function
    while (core1State != 0) {
      tight_loop_contents();
    }
    core1Fn = fn;
    core1Arg = arg;
    core1State = 1;
  }
  __sev();
  return (TaskHandle)&core1State;
}

void taskExit() {
  if (get_core_num() == EMULATION_CORE) {
    longjmp(trampoline, 1);
  }
  for (;;) {
    tight_loop_contents();
  }
}

void yield() { tight_loop_contents(); }

int coreId() { return (int)get_core_num(); }

uint32_t rtosHeapFree() {
  struct mallinfo mi = mallinfo();
  return (uint32_t)mi.fordblks;
}

uint32_t rtosHeapMinFree() { return rtosHeapFree(); }

void sleepMs(uint32_t ms) {
  const uint32_t end = millis() + ms;
  while ((int32_t)(millis() - end) < 0) {
    tight_loop_contents();
  }
}

Signal::Signal() : pending(0) {}

void Signal::give() {
  pending = 1;
  __sev();
}

bool Signal::take(uint32_t timeoutMs) {
  const uint32_t start = millis();
  for (;;) {
    if (pending) {
      pending = 0;
      return true;
    }
    if (timeoutMs != WAIT_FOREVER) {
      if (timeoutMs == 0 || (uint32_t)(millis() - start) >= timeoutMs) {
        return false;
      }
    }
    // give() raises the event, so a signal that arrived between the test above
    // and this wait does not get lost
    __wfe();
  }
}

void Signal::clear() { pending = 0; }

CriticalSection::CriticalSection() { spin_lock_blocking(getLock()); }

CriticalSection::~CriticalSection() { spin_unlock(getLock(), get_core_num()); }

} // namespace hal
