// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "titus/pre1_amiga.hpp"
#include <array>
#include <string>
namespace titus::pre1_amiga {
struct Rules {
  // Bounded relocated Hunk sections, read as data; no 68k execution at runtime.
  std::array<Bytes, 4> sections;
  std::array<std::string, 4> section_sha256;
  std::array<std::array<unsigned, 19>, 4> tile_counts{};
  std::array<std::array<unsigned, 16>, 6> palettes{};
};
Rules rules(const Bytes &main);
struct TilePlacement {
  int x = 0, y = 0;
  unsigned sprite = 0, layer = 0;
};
struct ObjectRecord {
  unsigned type = 0;
  std::vector<int> words;
};
using ObjectScreens = std::vector<std::vector<ObjectRecord>>;
struct Surface {
  unsigned number = 0;
  std::array<std::vector<TilePlacement>, 19> screens;
  ObjectScreens objects;
};
Surface surface(unsigned number, const Bytes &dat, const Rules &);
ObjectScreens object_screens(const Bytes &, std::size_t offset = 0,
                             bool require_end = true);
struct Segment {
  int dx = 0, dy = 0;
  unsigned width = 0;
};
using CollisionRules = std::array<std::vector<Segment>, 40>;
using Collision = std::array<std::uint8_t, 8000>;
CollisionRules collision_rules(const Bytes &dur);
Collision collision(const std::vector<TilePlacement> &, const CollisionRules &);
struct DrawCommand {
  unsigned kind = 0;
  int x = 0, y = 0;
  unsigned a = 0, b = 0, color = 0;
};
struct Scene {
  std::vector<DrawCommand> commands;
  Collision collision{};
};
Scene scene(const Surface &, unsigned screen, unsigned element_count,
            const CollisionRules &);
bool empty(const Collision &, int x, int y);
} // namespace titus::pre1_amiga
