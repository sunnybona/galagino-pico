#ifndef HAL_RP2040_I2C_H
#define HAL_RP2040_I2C_H

#include <stdint.h>

namespace hal {

// Plan A: no external I2C pad and no codec on the bus. The interface has to
// exist for the optional boards, every call reports failure.
class I2c {
public:
  static bool init(int, int, int, uint32_t) { return false; }
  static uint32_t clockHz(int) { return 0; }
  static bool probe(int, uint8_t) { return false; }
  static bool write(int, uint8_t, const uint8_t *, uint32_t) { return false; }
  static uint32_t read(int, uint8_t, uint8_t *, uint32_t) { return 0; }
  static bool writeRead(int, uint8_t, const uint8_t *, uint32_t, uint8_t *, uint32_t) {
    return false;
  }
};

} // namespace hal

#endif // HAL_RP2040_I2C_H
