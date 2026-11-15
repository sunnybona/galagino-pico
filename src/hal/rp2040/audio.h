#ifndef HAL_RP2040_AUDIO_H
#define HAL_RP2040_AUDIO_H

#include <stdint.h>
#include "../audio_config.h"

namespace hal {

// HAL_AUDIO_PWM: PicoBoy's single GPIO PWM "DAC", fed through
// pb_audio_submit() (audio_pwm.cpp). galagino mixes at a fixed 24 kHz; the
// carrier of this board is PB_AUDIO_SAMPLE_RATE (250 MHz / 2^12 = 61.035 kHz),
// so the backend resamples on the way out.
class AudioOut {
public:
  static void init(const AudioConfig &cfg);
  static bool write(const int16_t *samples);
  static void writeBlocking(const int16_t *samples);
  static void flushSilence();
};

} // namespace hal

#endif // HAL_RP2040_AUDIO_H
