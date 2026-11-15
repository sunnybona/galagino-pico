#include "audio.h"
#include "../../config.h"
#include "picoboy/pb_audio.h"
#include "picoboy/board.h"
#include "time.h"
#include <stdint.h>

namespace hal {
namespace {

// 16.16 output-samples-per-input-sample, from the PWM carrier of this board
uint32_t stepQ16 = 0;
uint32_t acc = 0;
uint32_t chunk = 64;   // samples per write(), from AudioConfig
int16_t last = 0;

// 按"实时"限流：Audio::transmit() 是 for(;;) 循环，只有 write() 返回 false 才会退出。
// 若永远返回 true，第一次喂音频就把 core0 卡死在死循环里（实测就死在这）。
// 这里用生成侧 24 kHz 的真实时间来算预算，超前就返回 false。
uint32_t inSent = 0;      // 已写入的输入样本数
uint32_t tBase = 0;
bool   haveBase = false;
const uint32_t IN_RATE = 24000;
const uint32_t LEAD = 256;   // 允许超前 ~10 ms，避免欠载

// 64 input samples at 24 kHz become ~163 at 61 kHz
const uint32_t OUT_MAX = 512;
int16_t out[OUT_MAX];

} // namespace

void AudioOut::init(const AudioConfig &cfg) {
  pb_audio_init();
  // galagino applies its own volume (config.h AUDIO_VOLUME); do not attenuate
  // a second time here
  pb_audio_set_volume(10);

  const uint32_t inRate = cfg.rate ? cfg.rate : 24000;
  stepQ16 = (uint32_t)(((uint64_t)PB_AUDIO_SAMPLE_RATE << 16) / inRate);
  chunk = cfg.chunk ? cfg.chunk : 64;
  if (chunk > 128) chunk = 128;
  acc = 0;
  last = 0;
  inSent = 0;
  tBase = millis();
  haveBase = true;
}

bool AudioOut::write(const int16_t *samples) {
  // 实时预算：生成侧速率 24 kHz。超前就返回 false，让 Audio::transmit() 退出。
  if (!haveBase) {
    tBase = millis();
    haveBase = true;
  }
  const uint32_t allowed = (uint32_t)((uint64_t)(millis() - tBase) * IN_RATE / 1000u) + LEAD;
  if (inSent + chunk > allowed) {
    return false;
  }
  // linear resample to the carrier rate; the BSP ring holds 2048 samples, so a
  // dropped chunk here only happens when the game runs ahead of real time
  uint32_t n = 0;
  const uint32_t count = chunk;
  for (uint32_t i = 0; i < count; i++) {
    const int16_t s = samples[i];
    acc += stepQ16;
    uint32_t m = acc >> 16;
    acc &= 0xFFFF;
    if (m) {
      if (n + m > OUT_MAX) {
        m = OUT_MAX - n;
      }
      const int32_t delta = (int32_t)s - (int32_t)last;
      for (uint32_t j = 1; j <= m; j++) {
        out[n++] = (int16_t)((int32_t)last + delta * (int32_t)j / (int32_t)m);
      }
    }
    last = s;
  }
  pb_audio_submit(out, n);
  inSent += chunk;
  return true;
}

void AudioOut::writeBlocking(const int16_t *samples) { write(samples); }

void AudioOut::flushSilence() {
  static int16_t silence[128] = {0};
  for (int i = 0; i < 3; i++) {
    write(silence);
  }
  last = 0;
  acc = 0;
}

} // namespace hal
