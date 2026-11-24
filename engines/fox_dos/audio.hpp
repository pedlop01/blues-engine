// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <SDL.h>
#include <atomic>
#include <cstdint>
namespace titus::fox_dos {
struct Audio {
  SDL_AudioDeviceID device = 0;
  unsigned lock_depth = 0;
  std::atomic<bool> muted{true};
  std::atomic<std::uint64_t> samples{0}, nonzero{0}, clipped{0};
  std::atomic<unsigned> peak{0};
  void open();
  void reopen();
  void volume(unsigned, unsigned);
  void lock();
  void unlock();
  void clear_effects();
  ~Audio();
};
} // namespace titus::fox_dos
