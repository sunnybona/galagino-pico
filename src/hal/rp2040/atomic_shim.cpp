// Cortex-M0+ has no LDREX/STREX (the 4-byte variants are built in by GCC), so GCC emits calls to libatomic for atomic
// read-modify-write on small types (galagino counts voice samples with
// std::atomic<uint8_t>). The SDK toolchain ships no libatomic, and RP2040
// atomicity here is only needed between the two cores, so these are plain
// implementations behind the one hardware spinlock galagino already uses.
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/sync.h"

namespace {
spin_lock_t *lock() {
  static spin_lock_t *l = nullptr;
  if (!l) {
    l = spin_lock_instance(spin_lock_claim_unused(true));
  }
  return l;
}
} // namespace

extern "C" {

uint8_t __atomic_fetch_add_1(volatile void *ptr, uint8_t val, int) {
  spin_lock_t *l = lock();
  const uint32_t save = spin_lock_blocking(l);
  volatile uint8_t *p = (volatile uint8_t *)ptr;
  const uint8_t old = *p;
  *p = (uint8_t)(old + val);
  spin_unlock(l, save);
  return old;
}

uint8_t __atomic_fetch_sub_1(volatile void *ptr, uint8_t val, int) {
  spin_lock_t *l = lock();
  const uint32_t save = spin_lock_blocking(l);
  volatile uint8_t *p = (volatile uint8_t *)ptr;
  const uint8_t old = *p;
  *p = (uint8_t)(old - val);
  spin_unlock(l, save);
  return old;
}

uint16_t __atomic_fetch_add_2(volatile void *ptr, uint16_t val, int) {
  spin_lock_t *l = lock();
  const uint32_t save = spin_lock_blocking(l);
  volatile uint16_t *p = (volatile uint16_t *)ptr;
  const uint16_t old = *p;
  *p = (uint16_t)(old + val);
  spin_unlock(l, save);
  return old;
}



bool __atomic_compare_exchange_1(volatile void *ptr, void *expected, uint8_t desired,
                                 bool, int, int) {
  spin_lock_t *l = lock();
  const uint32_t save = spin_lock_blocking(l);
  volatile uint8_t *p = (volatile uint8_t *)ptr;
  uint8_t *e = (uint8_t *)expected;
  bool ok = false;
  if (*p == *e) {
    *p = desired;
    ok = true;
  } else {
    *e = *p;
  }
  spin_unlock(l, save);
  return ok;
}

} // extern "C"
