#include "gpio.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

namespace hal {

void pinOutput(int pin, bool level) {
  gpio_init(pin);
  gpio_put(pin, level);
  gpio_set_dir(pin, GPIO_OUT);
}

void pinInputPullup(int pin) {
  gpio_init(pin);
  gpio_set_dir(pin, GPIO_IN);
  gpio_pull_down(pin);
}

void pinWrite(int pin, bool level) { gpio_put(pin, level); }

// inverts: pressed (high on this board) reads as false, as galagino expects
bool pinRead(int pin) { return !gpio_get(pin); }

} // namespace hal
