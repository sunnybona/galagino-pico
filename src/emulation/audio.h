#ifndef AUDIO_H
#define AUDIO_H

// Buffering, volume, mute. Each machine renders its own sound
// (machineBase::renderAudio, contract in audio_mix.h); the sink is hal::AudioOut.

#include <stdint.h>
#ifdef AUDIO_RENDER_ON_EMU_CORE
#include <atomic>
#endif
#include "../config.h"
#include "../hal/hal.h"
#include "audio_mix.h"

class machineBase;

class Audio {
public:
  void init();
  void start(machineBase *machine);
  void stop();
  void transmit();
#ifdef AUDIO_RENDER_ON_EMU_CORE
  // emulation core: render audio into the ring while it has room. Called right
  // after run_frame(), so the sound chips are in step with the CPU and the emulation core's
  // idle time (it waits for the next frame signal) is used instead of the video core's.
  void produce();
  // diagnostics for the frame timing line: chunks produced / handed to the sink,
  // and samples still in the ring (0 = dry, capacity = sink not consuming)
  uint32_t chunksProduced() const { return chunksIn; }
  uint32_t chunksWritten() const { return chunksOut; }
  uint32_t ringFill() const {
    return (ringHead.load(std::memory_order_relaxed) + RING_SAMPLES -
            ringTail.load(std::memory_order_relaxed)) % RING_SAMPLES;
  }
#endif
  void volumeUpDown(bool up, bool down);
  void mute(int m);

private:
  static const uint32_t SAMPLE_RATE = 24000;
  static const uint32_t DMA_BUF_COUNT = 4;

  int16_t scaled(int16_t value) const;

  machineBase *currentMachine = nullptr;

  // next chunk for the sink, volume applied; silence while no machine runs
  int16_t pending[AUDIO_CHUNK];

#ifdef AUDIO_RENDER_ON_EMU_CORE
  // finished samples between the two cores: one producer (emulation core),
  // one consumer (video core), 16 chunks ~ 42 ms at 24 kHz
  static const uint32_t RING_SAMPLES = AUDIO_CHUNK * 16;
  int16_t ringSamples[RING_SAMPLES];
  std::atomic<uint32_t> ringHead;  // producer
  std::atomic<uint32_t> ringTail;  // consumer
  volatile uint32_t chunksIn = 0;   // chunks rendered by the emulation core
  volatile uint32_t chunksOut = 0;  // chunks accepted by the sink
#endif

  short volumeSetting = AUDIO_VOLUME;
  bool volumeUpLast;
  bool volumeDownLast;
  int  muted = 0;
};

#endif
