// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "titus/catalog.hpp"
extern "C" {
#include "kernel.h"
}
#include <vector>
namespace titus::fox_dos {
struct Snapshot {
  unsigned level = 1;
  std::vector<long long> values;
};
struct Pose {
  int x = 0, y = 0;
};
struct Frame {
  unsigned width = 960, height = 600;
  std::vector<std::uint32_t> pixels;
};
struct View {
  double x = 0, y = 0, zoom = 1, alpha = 1;
  unsigned width = 960, height = 600;
  bool classic_hud = true;
};
std::uint64_t &active_serial();
std::uint64_t fresh_serial();
void load(unsigned level, int lives = 3);
Snapshot snapshot();
void restore(const Snapshot &);
Json save(const Snapshot &, const std::string &fingerprint);
Snapshot decode(const Json &, const std::string &fingerprint);
std::vector<Pose> poses();
Frame render(const View &, const std::vector<Pose> &previous);
void rectangle(Frame &, int, int, int, int, std::uint32_t);
void text(Frame &, int, int, const std::string &,
          std::uint32_t color = 0xffeeeeee, unsigned scale = 1);
Frame picture(unsigned, unsigned scale = 1);
void bmp(const fs::path &, const Frame &);
} // namespace titus::fox_dos
