// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "titus/pre1_amiga_game.hpp"
namespace titus::pre1_amiga {
struct Frame {
  unsigned width = 320, height = 200;
  std::vector<std::uint32_t> pixels;
};
struct View {
  double x = 0, y = 0, zoom = 1, alpha = 1;
  unsigned width = 960, height = 600;
  bool classic_hud = true;
};
Frame picture_frame(const fox_amiga::Picture &, unsigned scale = 1);
double player_world_x(const Game &, double alpha = 1);
Frame render(Assets &, const Game &, const View &);
void text(Frame &, const Assets &, int x, int y, const std::string &,
          std::uint32_t color = 0xffeeeeee, unsigned scale = 1);
void rectangle(Frame &, int x, int y, int width, int height,
               std::uint32_t color);
} // namespace titus::pre1_amiga
