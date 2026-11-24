// SPDX-License-Identifier: GPL-3.0-or-later
// DAT/Hunk and palette readers from the pinned original Amiga executable.
// Object record layout also agrees with GPL Olduvai prepare/exe_tables.cpp,
// Copyright (C) 2026 Krzysztof Sokolowski
// (a69e9204fc311c8810d696a6b799000d1bcd1642).
#include "titus/pre1_amiga_level.hpp"
#include "titus/catalog.hpp"
#include <algorithm>
#include <stdexcept>
namespace titus::pre1_amiga {
namespace {
[[noreturn]] void bad(const char *reason) { throw std::runtime_error(reason); }
unsigned read(const Bytes &b, std::size_t p, unsigned size = 2,
              bool little = false) {
  if (p > b.size() || size > b.size() - p)
    bad("Truncated Amiga level field");
  unsigned n = 0;
  for (unsigned i = 0; i < size; ++i)
    n = (n << 8) | b[p + (little ? size - i - 1 : i)];
  return n;
}
int signed_word(unsigned n) { return n >= 32768 ? int(n) - 65536 : int(n); }
unsigned index(unsigned n) {
  if (n != 1 && n != 3 && n != 5 && n != 7)
    bad("Invalid Amiga surface level");
  return (n - 1) / 2;
}
std::string hash(const Bytes &b) {
  return titus::sha256(std::string(b.begin(), b.end()));
}
} // namespace
Rules rules(const Bytes &main) {
  if (hash(main) !=
      "d48f952eca0d74547d9e99531b54ac05beaabb42a55203fec510279bd9bd5295")
    bad("Unsupported Prehistorik Amiga executable revision");
  Rules out;
  std::size_t p = 0;
  auto next = [&]() {
    auto v = read(main, p, 4);
    p += 4;
    return v;
  };
  constexpr unsigned sizes[] = {8, 5332, 8520, 89164},
                     bases[] = {0x20000, 0x50000, 0x80000, 0xb0000};
  if (next() != 0x3f3 || next() != 0 || next() != 4 || next() != 0 ||
      next() != 3)
    bad("Invalid Prehistorik Amiga Hunk header");
  for (auto size : sizes)
    if (next() != size / 4)
      bad("Unexpected Amiga section size");
  for (unsigned n = 0; n < 4; ++n) {
    unsigned type = next(), length = next();
    if (length > sizes[n] / 4 ||
        (type != 0x3e9 && type != 0x3ea && type != 0x3eb))
      bad("Invalid Amiga section");
    auto &data = out.sections[n];
    data.resize(sizes[n]);
    if (type != 0x3eb) {
      if (p > main.size() || length * 4 > main.size() - p)
        bad("Truncated Amiga section");
      std::copy_n(main.begin() + p, length * 4, data.begin());
      p += length * 4;
    }
    auto chunk = next();
    if (chunk == 0x3ec) {
      while (unsigned count = next()) {
        unsigned target = next();
        if (target >= 4 || count > sizes[n] / 2)
          bad("Invalid Amiga relocations");
        while (count--) {
          unsigned offset = next();
          if (offset > sizes[n] - 4 || (offset & 1))
            bad("Invalid Amiga relocation offset");
          unsigned value = read(data, offset, 4) + bases[target];
          for (unsigned b = 0; b < 4; ++b)
            data[offset + b] = std::uint8_t(value >> (24 - 8 * b));
        }
      }
      chunk = next();
    }
    if (chunk != 0x3f2)
      bad("Unexpected Amiga Hunk extension");
    out.section_sha256[n] = hash(data);
  }
  if (p != main.size())
    bad("Trailing Amiga Hunk data");
  const auto &data = out.sections[2];
  constexpr unsigned counts[] = {0xd40, 0x189a, 0x1952, 0x19d8};
  for (unsigned level = 0; level < 4; ++level)
    for (unsigned screen = 0; screen < 19; ++screen) {
      unsigned count = read(data, counts[level] + screen * 2) + 1;
      if (count > 4096)
        bad("Invalid tile count");
      out.tile_counts[level][screen] = count;
    }
  for (unsigned palette = 0; palette < 6; ++palette)
    for (unsigned color = 0; color < 16; ++color) {
      unsigned off = 0x9ee + palette * 48 + color * 3, r = data.at(off),
               g = data.at(off + 1), b = data.at(off + 2);
      if (r > 7 || g > 7 || b > 7)
        bad("Invalid Amiga palette component");
      out.palettes[palette][color] = (r << 9) | (g << 5) | (b << 1);
    }
  return out;
}
ObjectScreens object_screens(const Bytes &data, std::size_t offset,
                             bool require_end) {
  constexpr unsigned sizes[] = {2,  8,  6,  8,  8,  12, 10, 10, 14, 10,
                                26, 26, 26, 26, 14, 18, 16, 8,  8,  8,
                                10, 8,  8,  12, 10, 8,  26, 26, 8,  8,
                                12, 12, 14, 26, 26, 18, 14, 6,  26, 26};
  ObjectScreens result(1);
  unsigned records = 0;
  while (true) {
    unsigned type = read(data, offset);
    offset += 2;
    if (type == 255)
      break;
    if (type >= 40 || ++records > 4096 || result.size() > 256)
      bad("Invalid Amiga object table");
    if (!type) {
      result.emplace_back();
      continue;
    }
    ObjectRecord record;
    record.type = type;
    for (unsigned word = 0; word < (sizes[type] - 2) / 2; ++word) {
      record.words.push_back(signed_word(read(data, offset)));
      offset += 2;
    }
    result.back().push_back(std::move(record));
  }
  if (require_end && offset != data.size())
    bad("Trailing Amiga object records");
  return result;
}
Surface surface(unsigned number, const Bytes &dat, const Rules &rules) {
  unsigned n = index(number), tile_size = read(dat, 0),
           object_size = read(dat, 2);
  if (dat.size() != 4 + tile_size + object_size || tile_size % 6)
    bad("Invalid Amiga DAT lengths");
  Surface out;
  out.number = number;
  unsigned pos = 4;
  for (unsigned screen = 0; screen < 19; ++screen)
    for (unsigned i = 0; i < rules.tile_counts[n][screen]; ++i) {
      if (pos + 6 > 4 + tile_size)
        bad("Truncated Amiga tile list");
      unsigned raw = read(dat, pos), sprite = read(dat, pos + 4);
      if (!sprite || sprite > 256)
        bad("Invalid Amiga tile sprite");
      out.screens[screen].push_back({signed_word(raw) - int(raw & 15),
                                     signed_word(read(dat, pos + 2)),
                                     sprite - 1, raw & 15});
      pos += 6;
    }
  if (pos != 4 + tile_size)
    bad("Amiga tile count mismatch");
  out.objects = object_screens(Bytes(dat.begin() + pos, dat.end()));
  if (out.objects.size() != 20)
    bad("Unexpected Amiga object screen count");
  return out;
}
CollisionRules collision_rules(const Bytes &dur) {
  if (dur.size() != 960)
    bad("Amiga DUR must contain forty records");
  CollisionRules out;
  for (unsigned tile = 0; tile < 40; ++tile)
    for (unsigned part = 0; part < 4; ++part) {
      unsigned p = tile * 24 + part * 6;
      int dx = signed_word(read(dur, p, 2, true));
      if (dx == -1)
        break;
      int dy = signed_word(read(dur, p + 2, 2, true));
      unsigned width = read(dur, p + 4, 2, true);
      if (width > 4096)
        bad("Invalid Amiga collision width");
      if (width)
        out[tile].push_back({dx, dy, width});
    }
  return out;
}
Collision collision(const std::vector<TilePlacement> &placements,
                    const CollisionRules &rules) {
  Collision bitmap{};
  for (const auto &tile : placements) {
    if (tile.sprite >= rules.size())
      continue;
    for (const auto &segment : rules[tile.sprite]) {
      int y = tile.y + segment.dy;
      if (y < 0 || y >= 200)
        continue;
      for (unsigned pixel = 0; pixel < segment.width; ++pixel) {
        int x = tile.x + segment.dx + int(pixel);
        if (x >= 0 && x < 320)
          bitmap[unsigned(y) * 40 + unsigned(x) / 8] |=
              std::uint8_t(128 >> (x & 7));
      }
    }
  }
  return bitmap;
}
Scene scene(const Surface &surface, unsigned screen, unsigned element_count,
            const CollisionRules &rules) {
  if (screen >= 19 || !element_count || element_count > 256)
    bad("Invalid Amiga scene");
  Scene result;
  std::vector<TilePlacement> solid;
  auto sprite = [&](int x, int y, unsigned index, bool collide) {
    result.commands.push_back({0, x, y, index, 0, 0});
    if (collide)
      solid.push_back({x, y, index, 0});
  };
  auto fill = [&](int x, int y, unsigned w, unsigned h, unsigned color) {
    result.commands.push_back({1, x, y, w, h, color});
  };
  unsigned effective = surface.number == 1 && screen == 11 ? 10 : screen;
  if (surface.number == 3) {
    for (int y : {167, 128, 89, 50})
      sprite(98, y, element_count, false);
    sprite(98, 23, element_count + 1, false);
    if (screen != 10 && screen != 11) {
      fill(0, 9, 319, 190, 0);
      sprite(0, 9, 31, false);
      sprite(160, 9, 31, false);
    }
    if (screen > 11 && screen != 18)
      fill(0, 9, 319, 190, 0);
  } else if (surface.number == 7) {
    if (screen > 9 && screen < 13) {
      for (int x = 0; x <= 320; x += 64) {
        sprite(x, 79, 29, true);
        sprite(x, 40, 31, true);
      }
    } else
      for (int y = 9; y <= 200; y += 63)
        for (int x = 0; x <= 320; x += 64)
          sprite(x, y, 19, false);
  }
  for (auto tile : surface.screens[effective]) {
    if (surface.number == 3) {
      unsigned original = tile.sprite + 1;
      unsigned mapped = original == 30   ? 31
                        : original == 29 ? 30
                        : original == 20 ? 32
                        : original == 5  ? 29
                                         : original;
      if (mapped == 30)
        continue;
      tile.sprite = mapped - 1;
    }
    sprite(tile.x, tile.y, tile.sprite, true);
  }
  if (surface.number == 3) {
    sprite(0, 0, 32, false);
    if (screen == 11)
      fill(90, 9, 10, 52, 0);
  }
  result.collision = collision(solid, rules);
  return result;
}
bool empty(const Collision &bitmap, int x, int y) {
  x = std::clamp(x, 0, 319);
  if (y < 0 || y > 199)
    y = 199;
  return !(bitmap[unsigned(y) * 40 + unsigned(x) / 8] & (128 >> (x & 7)));
}
} // namespace titus::pre1_amiga
