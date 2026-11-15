#ifndef HAL_RP2040_TIME_H
#define HAL_RP2040_TIME_H

#include <stdint.h>

// No SDK header here: pico register headers define macros (ROM_BASE,
// SRAM_SIZE, ...) that collide with names in the machines. Out of line.
namespace hal {

// 32 bit, wrap like Arduino: callers compare with unsigned differences.
uint32_t micros();
uint32_t millis();

} // namespace hal

#endif // HAL_RP2040_TIME_H
