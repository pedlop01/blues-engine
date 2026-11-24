// SPDX-License-Identifier: GPL-3.0-or-later
#include "audio.hpp"
extern "C" {
#include "opl3.h"
void fox_dos_audio_tick(void);
void fox_dos_cancel_effects(void);
}
#include <algorithm>
#include <stdexcept>
namespace {
opl3_chip chips[2];
int bus = 0;
unsigned music_gain = 80, effects_gain = 80;
unsigned remaining = 0, phase = 0, sample_remainder = 0;
float music_current = 80, effects_current = 80, master = 0;
int last_pcm[2] = {0, 0};
void render(titus::fox_dos::Audio &s, std::int16_t *out, unsigned frames) {
  std::fill(out, out + frames * 2, 0);
  if (s.muted) {
    for (unsigned n = 0; n < frames; ++n)
      for (unsigned c = 0; c < 2; ++c) {
        last_pcm[c] = last_pcm[c] * 98 / 100;
        out[n * 2 + c] = last_pcm[c];
      }
    master = 0;
    return;
  }
  unsigned maximum = 0;
  std::uint64_t active = 0, limits = 0;
  for (unsigned n = 0; n < frames; ++n) {
    music_current += (float(music_gain) - music_current) * .005f;
    effects_current += (float(effects_gain) - effects_current) * .005f;
    master = std::min(1.f, master + 1.f / 256);
    if (!remaining) {
      fox_dos_audio_tick();
      sample_remainder += (++phase % 4 ? 14 : 13) * 44100;
      remaining = sample_remainder / 1000;
      sample_remainder %= 1000;
    }
    --remaining;
    std::int16_t m[2], f[2];
    OPL3_GenerateResampled(&chips[0], m);
    OPL3_GenerateResampled(&chips[1], f);
    for (unsigned c = 0; c < 2; ++c) {
      int value =
          int((int(m[c]) * music_current + int(f[c]) * effects_current) *
              master / 150);
      limits += value < -32768 || value > 32767;
      value = std::clamp(value, -32768, 32767);
      out[n * 2 + c] = value;
      last_pcm[c] = value;
      maximum = std::max(maximum, unsigned(std::abs(value)));
      active += value != 0;
    }
  }
  s.samples += frames * 2;
  s.nonzero += active;
  s.clipped += limits;
  auto old = s.peak.load();
  while (old < maximum && !s.peak.compare_exchange_weak(old, maximum)) {
  }
}
} // namespace
extern "C" {
void fox_opl_bus(int n) { bus = n; }
void fox_opl_write(int reg, int value) {
  OPL3_WriteRegBuffered(&chips[bus], reg, value);
}
void OPL_SetSampleRate(int r) {}
int OPL_Init(unsigned int port) {
  for (auto &chip : chips)
    OPL3_Reset(&chip, 44100);
  remaining = phase = sample_remainder = 0;
  return 1;
}
void OPL_SetCallback(unsigned long delay, void (*fn)(void *), void *data) {}
void OPL_Shutdown(void) {}
}
namespace titus::fox_dos {
void Audio::open() {
  SDL_AudioSpec spec{};
  spec.freq = 44100;
  spec.channels = 2;
  spec.format = AUDIO_S16SYS;
  spec.samples = 1024;
  spec.userdata = this;
  spec.callback = [](void *p, Uint8 *d, int len) {
    render(*static_cast<Audio *>(p), reinterpret_cast<std::int16_t *>(d),
           len / 4);
  };
  device = SDL_OpenAudioDevice(nullptr, 0, &spec, nullptr, 0);
  if (!device)
    throw std::runtime_error(SDL_GetError());
  SDL_PauseAudioDevice(device, 0);
}
void Audio::reopen() {
  if (device)
    SDL_CloseAudioDevice(device);
  device = 0;
  open();
}
void Audio::volume(unsigned m, unsigned f) {
  lock();
  music_gain = std::min(m, 100u);
  effects_gain = std::min(f, 100u);
  if (samples.load() == 0) {
    music_current = float(music_gain);
    effects_current = float(effects_gain);
  }
  unlock();
}
void Audio::lock() {
  if (device && lock_depth++ == 0)
    SDL_LockAudioDevice(device);
}
void Audio::unlock() {
  if (device && --lock_depth == 0)
    SDL_UnlockAudioDevice(device);
}
void Audio::clear_effects() {
  lock();
  fox_dos_cancel_effects();
  OPL3_Reset(&chips[1], 44100);
  unlock();
}
Audio::~Audio() {
  if (device)
    SDL_CloseAudioDevice(device);
}
} // namespace titus::fox_dos
