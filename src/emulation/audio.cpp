#include "audio.h"
#include <string.h>
#include "../machines/machineBase.h"
#include "../config.h"
#ifdef HAL_AUDIO_ES8311
#include <stdio.h>
#include "../audio/codec_es8311.h"

// codec output level, 0..100 (machines mix at full scale)
static const uint8_t ES8311_VOLUME = 80;
#endif

void Audio::init() {
#ifdef GAL_AUDIO_OFF
  // 静音构建：不配置 PWM/DMA，DMA 完成中断也不会来
  return;
#endif
  memset(pending, 0, sizeof(pending));
#ifdef AUDIO_RENDER_ON_EMU_CORE
  ringHead.store(0);
  ringTail.store(0);
#endif
#ifdef HAL_AUDIO_ES8311
  // codec registers before the I2S stream starts; the controller shares the bus
  if (!hal::I2c::init(0, ES8311_I2C_SDA, ES8311_I2C_SCL, ES8311_I2C_CLK) ||
      !Es8311::init(0, ES8311_I2C_ADDR, SAMPLE_RATE, ES8311_VOLUME)) {
    printf("es8311: no codec at 0x%02x\n", ES8311_I2C_ADDR);
  }
#endif
  hal::AudioConfig cfg = {SAMPLE_RATE, AUDIO_CHUNK, DMA_BUF_COUNT};
  hal::AudioOut::init(cfg);
}

void Audio::start(machineBase *machine) {
  currentMachine = machine;
}

void Audio::volumeUpDown(bool up, bool down) {
  auto entry = volumeSetting;

  if (up && !volumeUpLast) {
    if (volumeSetting > 1)
      volumeSetting--;
  }
  volumeUpLast = up;

  if (down && !volumeDownLast) {
    if (volumeSetting < 30)
      volumeSetting++;
  }
  volumeDownLast = down;

  if (entry != volumeSetting)
    printf("VolumeUpDown: %d\n", volumeSetting);
}

void Audio::mute(int m) {
  muted = m;
}

// Machine is deleted after this; stop reading its registers.
void Audio::stop() {
  currentMachine = nullptr;
#ifdef GAL_AUDIO_OFF
  return;
#endif
  memset(pending, 0, sizeof(pending));
#ifdef AUDIO_RENDER_ON_EMU_CORE
  // the emulation core is stopped before this, so the ring is ours alone
  ringHead.store(0);
  ringTail.store(0);
#endif
  hal::AudioOut::flushSilence();
}

#ifdef AUDIO_RENDER_ON_EMU_CORE
// Emulation core: render as many chunks as the ring takes. The sound chips
// belong to this core (run_frame() has just run), and the video core only
// copies finished chunks out of the ring, so FM/AY synthesis overlaps the SPI
// pushes instead of blocking them.
void Audio::produce() {
#ifdef GAL_AUDIO_OFF
  return;  // 静音构建：不做任何合成，renderAudio 也不调用
#endif
  if (!currentMachine) {
    return;
  }

  // fill the ring as far as its room allows. One frame at 24 kHz needs 800-980
  // samples (13-16 chunks), so a lower cap starves the sink and the sound breaks up.
  // The loop bound is only a guard against spinning if the indices go wrong.
  for (uint32_t produced = 0; produced < RING_SAMPLES; produced += AUDIO_CHUNK) {
    const uint32_t head = ringHead.load(std::memory_order_relaxed);
    const uint32_t tail = ringTail.load(std::memory_order_acquire);
    // filled slots, then the room left (one slot is kept empty to tell
    // full from empty: head == tail always means empty)
    const uint32_t used = (head + RING_SAMPLES - tail) % RING_SAMPLES;
    const uint32_t freeSlots = RING_SAMPLES - used - 1;
    if (freeSlots < AUDIO_CHUNK) {
      return;
    }

    int16_t samples[AUDIO_CHUNK];
    currentMachine->renderAudio(samples, AUDIO_CHUNK);
    for (uint32_t i = 0; i < AUDIO_CHUNK; i++) {
      ringSamples[(head + i) % RING_SAMPLES] = scaled(samples[i]);
    }
    ringHead.store((head + AUDIO_CHUNK) % RING_SAMPLES, std::memory_order_release);
    chunksIn++;
  }
}

// Video core: hand finished chunks to the sink. The sink takes whole chunks
// (all or nothing), so a refused chunk stays in the ring for the next call.
void Audio::transmit() {
#ifdef GAL_AUDIO_OFF
  return;  // 静音构建：不往 sink 写一个样本
#endif
  for (;;) {
    const uint32_t head = ringHead.load(std::memory_order_acquire);
    const uint32_t tail = ringTail.load(std::memory_order_relaxed);
    const uint32_t used = (head + RING_SAMPLES - tail) % RING_SAMPLES;
    if (used < AUDIO_CHUNK) {
      return;
    }

    for (uint32_t i = 0; i < AUDIO_CHUNK; i++) {
      pending[i] = ringSamples[(tail + i) % RING_SAMPLES];
    }
    if (!hal::AudioOut::write(pending)) {
      return;
    }
    ringTail.store((tail + AUDIO_CHUNK) % RING_SAMPLES, std::memory_order_release);
    chunksOut++;
  }
}
#else
void Audio::transmit() {
  // (try to) transmit as much audio data as possible. The sink takes whole
  // chunks: either all or nothing is actually written.
#ifdef GAL_AUDIO_OFF
  return;
#endif
  for (;;) {
    if (!hal::AudioOut::write(pending)) {
      return;
    }

    // stopped: pending stays silent
    if (!currentMachine) {
      continue;
    }

    // render the next audio chunk if data has actually been sent
    int16_t samples[AUDIO_CHUNK];
    currentMachine->renderAudio(samples, AUDIO_CHUNK);
    for (int i = 0; i < AUDIO_CHUNK; i++) {
      pending[i] = scaled(samples[i]);
    }
  }
}
#endif

int16_t Audio::scaled(int16_t value) const {
  if (muted) value = 0;

  // value is now in the range of +/- 512, so expand to +/- 15 bit
  value = value * 64;

  return value / volumeSetting;
}
