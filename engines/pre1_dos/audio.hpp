// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "formats/voc.hpp"
#include "presentation/audio/opl_music.hpp"
#include <SDL.h>
#include <algorithm>
#include <atomic>
#include <memory>
namespace titus::pre1_dos {
struct DosSample {
  std::vector<std::int16_t> pcm;
};
// Independent music/FX buses. No SDL calls or allocations in the callback.
struct Audio {
  SDL_AudioDeviceID device = 0;
  std::unique_ptr<olduvai::presentation::OplMusicPlayer> player;
  std::shared_ptr<const DosSample> voice;
  std::size_t cursor = 0;
  unsigned music_gain = 80, fx_gain = 80;
  float music_current = 80, fx_current = 80, master = 0;
  int last_pcm[2] = {0, 0};
  std::atomic<bool> muted{true};
  std::atomic<std::uint64_t> samples{0}, nonzero{0}, clipped{0};
  std::atomic<unsigned> peak{0};
  static void callback(void *opaque, Uint8 *bytes, int length) {
    auto &s = *static_cast<Audio *>(opaque);
    auto *out = reinterpret_cast<std::int16_t *>(bytes);
    std::fill(out, out + length / 2, 0);
    if (s.muted) {
      for (int n = 0; n < length / 4; ++n)
        for (int c = 0; c < 2; ++c) {
          s.last_pcm[c] = s.last_pcm[c] * 98 / 100;
          out[n * 2 + c] = s.last_pcm[c];
        }
      s.master = 0;
      return;
    }
    if (s.player)
      s.player->render(length / 4, out);
    unsigned maximum = 0;
    std::uint64_t active = 0, limits = 0;
    for (int i = 0; i < length / 4; ++i) {
      s.music_current += (float(s.music_gain) - s.music_current) * .005f;
      s.fx_current += (float(s.fx_gain) - s.fx_current) * .005f;
      s.master = std::min(1.f, s.master + 1.f / 256);
      int fx = 0;
      if (s.voice && s.cursor < s.voice->pcm.size())
        fx = s.voice->pcm[s.cursor++];
      for (int c = 0; c < 2; ++c) {
        int v = int((out[i * 2 + c] * s.music_current + fx * s.fx_current) *
                    s.master / 125);
        limits += v > 32767 || v < -32768;
        v = std::clamp(v, -32768, 32767);
        out[i * 2 + c] = v;
        s.last_pcm[c] = v;
        maximum = std::max(maximum, unsigned(std::abs(v)));
        active += v != 0;
      }
    }
    s.samples.fetch_add(length / 2);
    s.nonzero.fetch_add(active);
    s.clipped.fetch_add(limits);
    auto old = s.peak.load();
    while (old < maximum && !s.peak.compare_exchange_weak(old, maximum)) {
    }
  }
  void open() {
    SDL_AudioSpec spec{};
    spec.freq = 44100;
    spec.format = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples = 1024;
    spec.callback = callback;
    spec.userdata = this;
    device = SDL_OpenAudioDevice(nullptr, 0, &spec, nullptr, 0);
    if (!device)
      throw std::runtime_error(SDL_GetError());
    if (!player)
      player = std::make_unique<olduvai::presentation::OplMusicPlayer>(44100);
    SDL_PauseAudioDevice(device, 0);
  }
  void song(const std::vector<std::uint8_t> &data) {
    SDL_LockAudioDevice(device);
    voice.reset();
    cursor = 0;
    bool ok = player->open(data);
    SDL_UnlockAudioDevice(device);
    if (!ok)
      throw std::runtime_error("Invalid DOS MDI track");
  }
  void effect(std::shared_ptr<const DosSample> sample) {
    SDL_LockAudioDevice(device);
    voice = std::move(sample);
    cursor = 0;
    SDL_UnlockAudioDevice(device);
  }
  void volume(unsigned music, unsigned effects) {
    SDL_LockAudioDevice(device);
    music_gain = std::min(music, 100u);
    if (samples.load() == 0)
      music_current = float(music_gain);
    fx_gain = std::min(effects, 100u);
    if (samples.load() == 0)
      fx_current = float(fx_gain);
    SDL_UnlockAudioDevice(device);
  }
  void clear_effects() {
    SDL_LockAudioDevice(device);
    voice.reset();
    cursor = 0;
    SDL_UnlockAudioDevice(device);
  }
  void reopen() {
    if (device)
      SDL_CloseAudioDevice(device);
    device = 0;
    open();
  }
  ~Audio() {
    if (device)
      SDL_CloseAudioDevice(device);
  }
};
inline std::shared_ptr<const DosSample>
dos_sample(const std::vector<std::uint8_t> &raw) {
  auto file = olduvai::formats::parse_voc(raw);
  auto *v = file.audio();
  if (!v || v->codec || v->sample_rate <= 0)
    throw std::runtime_error("Invalid DOS VOC effect");
  auto s = std::make_shared<DosSample>();
  auto count = v->data.size() * 44100 / std::size_t(v->sample_rate);
  s->pcm.resize(count);
  for (std::size_t n = 0; n < count; ++n)
    s->pcm[n] = (int(v->data[n * v->sample_rate / 44100]) - 128) * 256;
  return s;
}
} // namespace titus::pre1_dos
