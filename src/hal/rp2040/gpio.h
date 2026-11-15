#ifndef HAL_RP2040_GPIO_H
#define HAL_RP2040_GPIO_H

#include <stdint.h>

namespace hal {

// Out of line (gpio.cpp), see time.h.
void pinOutput(int pin, bool level);
// PicoBoy's buttons are active HIGH (external pull-down, switch to VCC), while
// galagino expects "pull-up, pressed reads low". This sets a pull-down and
// pinRead() inverts, so the contract holds for the buttons.
void pinInputPullup(int pin);
void pinWrite(int pin, bool level);
bool pinRead(int pin);

} // namespace hal

#endif // HAL_RP2040_GPIO_H
