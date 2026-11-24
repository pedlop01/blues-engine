// SPDX-License-Identifier: GPL-3.0-or-later
// Composite offsets and HUD layout adapted from GPL Olduvai game_render.cpp,
// hud_render.cpp and boss_render.cpp, Copyright (C) 2026 Krzysztof Sokolowski.
// All pixels and palettes are decoded from the user's Amiga files.
#include "titus/pre1_dos_render.hpp"
#include "systems/screen_topology.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
namespace titus::pre1_dos {
namespace {
using Palette = std::array<std::uint32_t, 16>;
std::uint32_t rgb(unsigned c) {
  return 0xff000000 | (((c >> 8) & 15) * 17 << 16) |
         (((c >> 4) & 15) * 17 << 8) | ((c & 15) * 17);
}
Palette palette(const Assets &a, const LevelAssets &l,
                const olduvai::systems::SystemsState &s) {
  Palette p{};
  int custom = -1;
  if (s.secret_flag)
    custom = 0;
  else if (s.cave_flag)
    custom = l.number == 1 ? 1 : l.number == 3 ? 2 : l.number == 5 ? 4 : 5;
  else if (l.number == 3)
    custom = s.current_screen == 10 || s.current_screen == 11 ? 2 : 3;
  for (unsigned n = 0; n < 16; ++n)
    p[n] = custom >= 0 ? (0xff000000 | a.rules.palettes[custom][n])
                       : (0xff000000 | l.background.palette[n]);
  return p;
}
struct Canvas {
  Frame &f;
  double left, top, scale;
  int clip_left = 0, clip_right = 0x7fffffff;
  void fill(double x, double y, unsigned w, unsigned h, std::uint32_t c) {
    int x0 = std::lround((x - left) * scale),
        y0 = std::lround((y - top) * scale),
        x1 = std::lround((x + w - left) * scale),
        y1 = std::lround((y + h - top) * scale);
    x0 = std::max(x0, clip_left);
    x1 = std::min(x1, clip_right);
    rectangle(f, x0, y0, x1 - x0, y1 - y0, c);
  }
  void sprite(const std::vector<Sprite> &bank, int id, const Palette &p,
              double x, double y, bool flip = false) {
    if (id < 0 || id >= int(bank.size()))
      return;
    const auto &s = bank[id];
    int x0 = std::lround((x - left) * scale),
        y0 = std::lround((y - top) * scale),
        x1 = std::lround((x + s.width - left) * scale),
        y1 = std::lround((y + s.height - top) * scale);
    if (x1 <= x0 || y1 <= y0)
      return;
    for (int yy = std::max(y0, 0); yy < std::min(y1, int(f.height)); ++yy) {
      unsigned sy =
          std::min(s.height - 1, unsigned((yy - y0) * s.height / (y1 - y0)));
      for (int xx = std::max({x0, 0, clip_left});
           xx < std::min({x1, int(f.width), clip_right}); ++xx) {
        unsigned sx =
            std::min(s.width - 1, unsigned((xx - x0) * s.width / (x1 - x0)));
        if (flip)
          sx = s.width - 1 - sx;
        unsigned pos = sy * s.width + sx;
        if (s.opaque[pos])
          f.pixels[std::size_t(yy) * f.width + xx] = p[s.colors[pos]];
      }
    }
  }
  void picture(const fox_amiga::Picture &p, double x = 0, double y = 0,
               unsigned clean_top = 0) {
    const auto &s = p.image;
    int x0 = std::lround((x - left) * scale),
        y0 = std::lround((y - top) * scale),
        x1 = std::lround((x + s.width - left) * scale),
        y1 = std::lround((y + s.height - top) * scale);
    if (x1 <= x0 || y1 <= y0)
      return;
    for (int yy = std::max(y0, 0); yy < std::min(y1, int(f.height)); ++yy)
      for (int xx = std::max({x0, 0, clip_left});
           xx < std::min({x1, int(f.width), clip_right}); ++xx) {
        unsigned sy = std::min(s.height - 1,
                               unsigned((yy - y0) * s.height / (y1 - y0))),
                 sx = std::min(s.width - 1,
                               unsigned((xx - x0) * s.width / (x1 - x0)));
        f.pixels[std::size_t(yy) * f.width + xx] =
            0xff000000 |
            p.palette[s.pixels[std::max(clean_top, sy) * s.width + sx]];
      }
  }
};
double mix(int before, int now, double alpha) {
  return std::abs(before - now) > 48 ? now : before + (now - before) * alpha;
}
void entities(Canvas &c, const LevelAssets &l, const Palette &p,
              const std::vector<olduvai::core::Entity> &entities, double offset,
              double alpha) {
  using olduvai::core::ObjType;
  for (const auto &e : entities) {
    if (!e.active || !e.visible)
      continue;
    if (e.obj_type == ObjType::Platform) {
      double y = mix(e.prev_current_y, e.current_y, alpha);
      c.sprite(l.elements, l.number == 3 ? 12 : 0, p, offset + e.x, y);
      c.sprite(l.elements, l.number == 3 ? 14 : 2, p, offset + e.x + 16, y);
      continue;
    }
    if (e.sprite < 0 || e.sprite >= int(l.actors.size()))
      continue;
    double x = mix(e.prev_x, e.x, alpha), y = mix(e.prev_y, e.y, alpha);
    bool ko = e.probe_si > 0 && e.ko_spr > 0 &&
              (e.sprite == e.ko_spr || e.sprite == e.ko_spr + 1);
    if (e.obj_type == ObjType::Platform)
      y = mix(e.prev_current_y, e.current_y, alpha);
    else if (e.draw_dy)
      y += mix(e.prev_draw_dy, e.draw_dy, alpha);
    else if (ko)
      y += e.probe_si - l.actors[e.sprite].height;
    if (e.obj_type == ObjType::HiddenFood && e.state == 1)
      x += 40;
    if (e.obj_type == ObjType::SecretFood || e.obj_type == ObjType::FoodCave)
      y -= l.actors[e.sprite].height;
    if (e.obj_type == ObjType::SnakeL3 && e.body_sprite >= 0) {
      c.sprite(l.actors, e.body_sprite, p, offset + e.body_x, e.body_y);
      c.sprite(l.actors, e.sprite, p, offset + e.head_x, e.head_y);
    } else
      c.sprite(l.actors, e.sprite, p, offset + x, y,
               ko ? false : e.facing_left);
    if (e.obj_type == ObjType::PteriyakiL7)
      c.sprite(l.actors, e.body_sprite, p, offset + e.body_x, e.body_y);
    if (e.launcher_spr >= 0)
      c.sprite(l.actors, e.launcher_spr, p, offset + e.init_x, e.init_y);
    if ((e.obj_type == ObjType::Chimp || e.obj_type == ObjType::ChimpL5) &&
        e.throw_flag)
      c.sprite(l.actors, 95, p, offset + mix(e.prev_throw_x, e.throw_x, alpha),
               mix(e.prev_throw_y, e.throw_y, alpha));
  }
}
void draw_scene(Canvas &c, const Assets &a, const LevelAssets &l,
                const olduvai::systems::SystemsState &s, double offset,
                bool overlay = false, unsigned trunk_ticks = 0) {
  // A native screen owns exactly [0,320). Tile art can extend beyond that
  // rectangle and must not overpaint an adjacent screen during scrolling.
  const int old_left = c.clip_left, old_right = c.clip_right;
  c.clip_left =
      std::max(old_left, int(std::lround((offset - c.left) * c.scale)));
  c.clip_right =
      std::min(old_right, int(std::lround((offset + 320 - c.left) * c.scale)));
  auto p = palette(a, l, s);
  if (s.current_screen < 100 && l.number != 3)
    c.picture(l.background, offset, 0, 9);
  else
    c.fill(offset, 0, 320, 200, p[s.secret_flag ? 4 : 0]);
  auto room = room_scene(a, l, s);
  if (trunk_ticks && trunk_ticks <= 44) {
    auto surface = l.surface;
    auto &records = surface.screens[17];
    records.erase(records.begin(),
                  records.begin() + std::min<std::size_t>(16, records.size()));
    room = scene(surface, 17, l.element_count, l.dur);
    auto moving = trunk_platform(l, (trunk_ticks - 1) * 4);
    room.commands.insert(room.commands.end(), moving.commands.begin(),
                         moving.commands.end());
  } else if (overlay || trunk_ticks) {
    auto platform =
        trunk_platform(l, trunk_ticks ? -80 + int(trunk_ticks - 45) * 4 : 0);
    std::size_t tail = l.surface.screens[18].size() + 1;
    auto at = room.commands.end() - std::min(tail, room.commands.size());
    room.commands.insert(at, platform.commands.begin(),
                         platform.commands.end());
  }

  std::vector<Sprite> bank;
  const std::vector<Sprite> *tiles = &l.elements;
  if (s.current_screen >= 100 && !s.secret_flag && l.number != 7) {
    if (l.number == 3) {
      bank = l.elements;
      bank.insert(bank.end(), l.cave.begin(), l.cave.end());
      tiles = &bank;
    } else
      tiles = &l.cave;
  }
  for (const auto &cmd : room.commands)
    if (cmd.kind)
      c.fill(offset + cmd.x, cmd.y, cmd.a + 1, cmd.b + 1, p[cmd.color]);
    else
      c.sprite(*tiles, cmd.a, p, offset + cmd.x, cmd.y);
  c.clip_left = old_left;
  c.clip_right = old_right;
}
void boss(Canvas &c, const Assets &a, const LevelAssets &l, const Game &g,
          double alpha) {
  auto p = palette(a, l, g.state);
  c.picture(l.background);
  auto draw = [&](int id, double x, double y, bool flip = false) {
    c.sprite(l.actors, id, p, x, y, flip);
  };
  if (l.number == 2) {
    if (g.transition_ticks) {
      c.sprite(l.elements, 0, p, 144, 15);
      bool even = (g.transition_ticks - 1) % 2 == 0;
      unsigned base = even ? 54 : 50;
      draw(base, 160, 17);
      draw(base + 1, 224, 17);
      draw(base + 2, even ? 165 : 164, 79);
      draw(base + 3, even ? 229 : 228, 79);
      draw(even ? 38 : 39, 136, even ? 170 : 168);
      draw(28, g.boss_player.x, g.boss_player.y);
      return;
    }

    c.sprite(l.elements, 0, p, 144, 15);
    draw(29, 151, 21);
    constexpr int jaw[5][6] = {{133, 50, 35, 151, 21, 30},
                               {133, 48, 36, 151, 20, 31},
                               {133, 48, 36, 151, 20, 31},
                               {133, 50, 35, 151, 21, 30},
                               {134, 50, 34, 151, 21, 29}};
    if (!g.boss2_pose[1])
      draw(34, 134, 50);
    else {
      const auto &r = jaw[std::clamp(g.boss2_pose[1] - 1, 0, 4)];
      draw(r[2], r[0], r[1]);
      draw(r[5], r[3], r[4]);
    }
    int js = g.boss2_pose[0] == 2                      ? 39
             : g.boss2_pose[0] || g.boss2.health < 290 ? 38
                                                       : 37;
    draw(js, js == 37 ? 137 : 136, js == 37 ? 177 : js == 38 ? 170 : 168);
    for (const auto &s : g.boss2.slots)
      if (s.ptype) {
        int id =
            s.ptype == 1
                ? (s.direction ? 44 : 40) +
                      olduvai::systems::kL2ProjAnim[std::clamp(s.frame, 0, 3)]
                : (s.direction ? 41 : 45);
        draw(id, mix(s.prev_x, s.x, alpha), 164);
      }
  } else if (l.number == 4) {
    const auto &b = g.boss4;
    double x = mix(b.prev_x, b.boss_x, alpha),
           y = mix(b.prev_y, b.boss_y, alpha);
    bool flip = b.direction == 1;
    if (b.stun_counter)
      draw(33, x, y);
    else {
      struct Rec {
        int hx, hy, b1x, b1y, b1s, b2x, b2y, b2s, tx, ty, ts;
      };
      constexpr Rec right[5] = {{2, 2, -50, 0, 34, 13, 13, 35, -74, 10, 29},
                                {2, 1, -50, -1, 36, 9, 10, 37, -74, 9, 29},
                                {2, 2, -50, 0, 38, 13, 14, 39, -74, 10, 29},
                                {2, 1, -50, -1, 40, 13, 10, 41, -74, 9, 29},
                                {2, 2, -50, 0, 42, 14, 13, 43, -74, 10, 29}};
      constexpr Rec left[5] = {{0, 2, 50, 0, 34, 36, 13, 35, 107, 10, 29},
                               {0, 1, 50, -1, 36, 23, 10, 37, 107, 9, 29},
                               {0, 2, 50, 0, 38, 20, 14, 39, 107, 10, 29},
                               {0, 1, 50, -1, 40, 36, 10, 41, 107, 9, 29},
                               {0, 2, 50, 0, 42, 34, 13, 43, 107, 10, 29}};
      const auto &r = (flip ? left : right)[b.walk_frame % 5];
      draw(r.b1s, x + r.b1x, y + r.b1y, flip);
      draw(r.b2s, x + r.b2x, y + r.b2y, flip);
      if (!b.hit_flash && !b.win_flag)
        draw(r.ts, x + r.tx, y + r.ty, flip);
      else
        draw(30, x + (flip ? 86 : -55), y + 9, flip);
      bool center = b.boss_x > 100 && b.boss_x < 170;
      draw(center ? 31 : 32, x + r.hx, y + r.hy + (center ? 0 : 5), flip);
    }
    constexpr int sy[4] = {170, 169, 168, 169}, sp[4] = {59, 60, 61, 60};
    draw(sp[g.spring_pose[0] & 3], 150, sy[g.spring_pose[0] & 3]);
  } else {
    if (g.boss6.win_flag) {
      c.sprite(l.giant[2], 0, p, 80, 7);
      draw(49, 213, 7);
      draw(51 + g.boss6.cycle_idx, 208, 7);
      draw(g.boss_player.y < 160 ? 3 : 28, g.boss_player.x,
           g.boss_player.y < 160 ? g.boss_player.y : 167,
           g.boss_player.y < 160 && g.boss_player.facing_left);
      return;
    }
    auto parts = olduvai::systems::l6_select_hmat_parts(g.boss6);
    c.sprite(l.giant[std::clamp(parts.body, 0, 2)], 0, p, 80, 7);
    c.sprite(l.giant[3], parts.head, p, 208, 7);
    constexpr int sy[4] = {170, 169, 168, 169}, sp[4] = {59, 60, 61, 60};
    draw(sp[g.spring_pose[1] & 3], 110, sy[g.spring_pose[1] & 3]);
  }
  if (l.number == 4 && g.boss4.win_flag) {
    if (g.boss4.win_flag == 1)
      draw(28, g.boss_player.x, g.boss_player.y);
    else if (g.boss4.win_flag == 2)
      draw(3, g.boss_player.x, g.boss_player.y);
    else
      draw(58, g.boss4.boss_x - 12, 119);
    return;
  }
  const auto &bp = g.boss_player;
  if (bp.lives >= 0) {
    double x = mix(bp.prev_x, bp.x, alpha), y = mix(bp.prev_y, bp.y, alpha);
    draw(bp.sprite, x + bp.sprite_dx, y + bp.sprite_dy,
         bp.death_counter ? false : bp.facing_left);
    if (bp.club_spr >= 0)
      draw(bp.club_spr, x + bp.club_dx, y + bp.club_dy, bp.facing_left);
    if (bp.hit_counter && bp.halo_toggle)
      draw(26 + (bp.halo_toggle & 1), x - 5, y - 11);
  }
}
} // namespace
Frame picture_frame(const fox_amiga::Picture &picture, unsigned scale) {
  if (!scale || scale > 4 || picture.image.width != 320 ||
      picture.image.height != 200 || picture.image.pixels.size() != 64000)
    throw std::runtime_error("Invalid full-screen picture");
  Frame f{320 * scale, 200 * scale,
          std::vector<std::uint32_t>(64000 * scale * scale)};
  // PC1 decoding already expands RGB12 components into RGB24. Reinterpreting
  // those expanded bytes as RGB12 changes the original intro colours.
  for (unsigned y = 0; y < f.height; ++y)
    for (unsigned x = 0; x < f.width; ++x)
      f.pixels[std::size_t(y) * f.width + x] =
          0xff000000 |
          picture.palette[picture.image.pixels[(y / scale) * 320 + x / scale]];
  return f;
}
double player_world_x(const Game &g, double alpha) {
  const auto &s = g.state;
  double offset = s.current_screen < 100 ? s.current_screen * 320 : 0;
  if (s.current_screen < 100 && g.previous_screen < 100 &&
      std::abs(s.current_screen - g.previous_screen) == 1 &&
      olduvai::systems::seam_contiguous(s.current_level, s.current_screen,
                                        g.previous_screen)) {
    double previous = g.previous_screen * 320 + s.player.prev_x;
    return previous +
           (offset + s.player.x - previous) * std::clamp(alpha, 0., 1.);
  }
  return offset + mix(s.player.prev_x, s.player.x, alpha);
}
void rectangle(Frame &f, int x, int y, int w, int h, std::uint32_t color) {
  if (w <= 0 || h <= 0)
    return;
  for (int yy = std::max(y, 0); yy < std::min(y + h, int(f.height)); ++yy)
    for (int xx = std::max(x, 0); xx < std::min(x + w, int(f.width)); ++xx)
      f.pixels[std::size_t(yy) * f.width + xx] = color;
}
void text(Frame &f, const Assets &a, int x, int y, const std::string &value,
          std::uint32_t color, unsigned scale) {
  for (unsigned char ch : value) {
    if (ch >= 32 && unsigned(ch - 32) < a.font.size()) {
      const auto &g = a.font[ch - 32];
      for (unsigned yy = 0; yy < g.height; ++yy)
        for (unsigned xx = 0; xx < g.width; ++xx)
          if (g.opaque[yy * g.width + xx])
            rectangle(f, x + xx * scale, y + yy * scale, scale, scale, color);
      x += g.width * scale;
    } else
      x += 8 * scale;
  }
}
Frame render(Assets &a, const Game &g, const View &view) {
  Frame f{view.width, view.height,
          std::vector<std::uint32_t>(std::size_t(view.width) * view.height,
                                     0xff080c14)};
  const auto &st = g.state;
  auto &l = a.level(st.current_level);
  double alpha = std::clamp(view.alpha, 0., 1.),
         scale = std::min(view.width / 320., view.height / 200.) * view.zoom;
  Canvas c{f, view.x - (view.width / scale - 320) / 2,
           view.y - (view.height / scale - 200) / 2, scale};
  if (g.ending) {
    c.picture(fox_amiga::picture(a.files.read("theend.pc1"), 4, true));
    return f;
  }
  if (!(l.number & 1))
    boss(c, a, l, g, alpha);
  else {
    bool room = st.current_screen >= 100;
    int first = room ? st.current_screen
                     : std::max(0, int(std::floor(c.left / 320))),
        last =
            room ? first
                 : std::min(18,
                            int(std::floor((c.left + f.width / scale) / 320)));
    if (!room) {
      int lo = st.current_screen, hi = lo;
      while (lo > 0 && olduvai::systems::seam_contiguous(l.number, lo - 1, lo))
        --lo;
      while (hi < 18 && olduvai::systems::seam_contiguous(l.number, hi, hi + 1))
        ++hi;
      first = std::max(first, lo);
      last = std::min(last, hi);
    }
    if (g.trunk_ticks)
      first = last = st.current_screen;
    for (int screen = first; screen <= last; ++screen) {
      auto copy = st;
      copy.current_screen = screen;
      double offset = room ? 0 : screen * 320;
      draw_scene(c, a, l, copy, offset, g.trunk_overlay && screen == 18,
                 g.trunk_ticks);
    }
    // All scenery first: a neighbour's background must never erase an actor.
    for (int screen = first; screen <= last; ++screen) {
      auto copy = st;
      copy.current_screen = screen;
      double offset = room ? 0 : screen * 320;
      if (screen == st.current_screen && !g.trunk_ticks)
        entities(c, l, palette(a, l, copy), st.entities, offset, alpha);
      else {
        auto it = g.store.find(screen);
        if (it != g.store.end())
          entities(c, l, palette(a, l, copy), it->second, offset, alpha);
      }
    }
    auto p = palette(a, l, st);
    if (st.secret_flag)
      for (const auto &item : g.secret_scatter)
        c.sprite(l.elements, item[2], p, item[0], item[1]);
    double offset = room ? 0 : st.current_screen * 320,
           x = player_world_x(g, alpha),
           y = mix(st.player.prev_y, st.player.y, alpha);
    const auto &player = st.player;
    if (st.secret_flag)
      c.sprite(l.actors, 146, p, 6, st.secret_spring_bouncing ? 148 : 164);
    if (st.stone_state)
      c.sprite(l.actors, 108, p, offset + st.stone_x, st.stone_y);
    if (st.fireball_flag)
      c.sprite(l.actors, 108, p, offset + st.fireball_x, st.fireball_y);
    if (g.trunk_ticks) {
      int dy = g.trunk_ticks <= 44 ? int(g.trunk_ticks - 1) * 4
                                   : -80 + int(g.trunk_ticks - 45) * 4;
      if (g.trunk_ticks != 65)
        c.sprite(l.actors, 0, p, offset + player.x, player.y + dy);
      if (g.trunk_ticks >= 45 && g.trunk_ticks < 65) {
        unsigned step = g.trunk_ticks - 45;
        const auto &jitter = st.l3_descent_smoke_jitter[step];
        c.sprite(l.actors, 85 + (step & 1), p, offset + 49, 175 + jitter.first);
        c.sprite(l.actors, 86 + (step & 1), p, offset + 65,
                 170 + jitter.second);
        c.sprite(l.actors, 85 + (step & 1), p, offset + 79, 178);
        for (auto item : std::array<std::pair<int, int>, 4>{
                 {{16, 0}, {32, 1}, {80, 1}, {128, 1}}})
          c.sprite(l.elements, item.second, p, offset + item.first, 185);
      }
    } else if (st.glider_active && !player.death_counter) {
      c.sprite(l.actors, 116, p, x, y + (l.number == 1 ? -30 : 0));
      if (l.number == 5)
        c.sprite(l.actors, 124, p, x + 21, y);
    } else {
      bool never = player.death_counter || player.sprite == 38 ||
                   player.sprite == 39 ||
                   (player.sprite >= 44 && player.sprite <= 46);
      c.sprite(l.actors, st.cave_emerge_frames ? 134 : player.sprite, p,
               x + player.dx, y + player.dy,
               never ? false : player.facing_left);
      if (g.club_phase) {
        bool phase = g.club_phase == 2;
        int id = st.halo_flight_flag ? (phase ? 138 : 136) : (phase ? 13 : 6);
        c.sprite(l.actors, id, p,
                 x + (player.facing_left ? -1 : 1) * (phase ? 14 : 20),
                 y + (phase ? -15 : 3), player.facing_left);
      }
    }
    if (player.hit_counter && player.hit_blink)
      c.sprite(l.actors, 127 + (player.hit_blink & 1), p, x - 5, y - 11);
    for (const auto &b : st.score_bonuses)
      if (b.counter)
        c.sprite(l.actors, b.sprite, p, offset + b.x, b.y);
    if (st.get_ready_counter >= 2 && st.get_ready_counter <= 17) {
      c.sprite(l.actors, 132, p, offset + 128, 100);
      c.sprite(l.actors, 133, p, offset + 162, 97);
    }
  }
  // Preserve the Amiga HUD's original coordinates and palette. Only the
  // world framing changes with zoom; the alternate mode adds a dark panel.
  const unsigned hs = std::max(1u, std::min(f.width / 320, f.height / 200));
  const auto colors = palette(a, l, st);
  auto fill = [&](int x, int y, int w, int h, unsigned color) {
    rectangle(f, x * hs, y * hs, w * hs, h * hs, color);
  };
  auto label = [&](int x, int y, const std::string &value, unsigned color) {
    text(f, a, x * hs, y * hs, value, color, hs);
  };
  fill(0, 0, 320, 9, colors[l.number == 1 ? 4 : 0]);
  if (!view.classic_hud)
    fill(0, 9, 320, 13, 0xff10151c);
  if (!(l.number & 1)) {
    if (view.classic_hud) {
      for (unsigned y = 0; y < 9 * hs; ++y)
        for (unsigned x = 0; x < 320 * hs; ++x)
          f.pixels[std::size_t(y) * f.width + x] =
              0xff000000 |
              l.background
                  .palette[l.background.image.pixels[(y / hs) * 320 + x / hs]];
    } else {
      fill(0, 0, 320, 9, 0xff10151c);
      label(0, 0, "LIVES:", colors[15]);
      label(224, 0, "ENERGY:", colors[15]);
    }
    char lives[8];
    std::snprintf(lives, sizeof lives, "%02d",
                  std::clamp(g.boss_player.lives, 0, 99));
    label(48, 0, lives, colors[15]);
    int health = l.number == 2   ? g.boss2.health
                 : l.number == 4 ? g.boss4.health
                                 : g.boss6.health;
    fill(std::clamp(health + 1, 272, 318), 0,
         318 - std::clamp(health + 1, 272, 318), 6, colors[0]);
    return f;
  }
  char value[32];
  label(0, 0, "SCORE", colors[15]);
  std::snprintf(value, sizeof value, "%06ld",
                std::clamp(st.score, 0L, 999999L));
  label(48, 0, value, colors[15]);
  label(96, 0, "FOOD", colors[15]);
  label(184, 0, "LIVES", colors[15]);
  std::snprintf(value, sizeof value, "%02d", std::max(0, st.player.lives));
  label(232, 0, value, colors[15]);
  label(256, 0, "TIME", colors[15]);
  std::snprintf(value, sizeof value, "%02d", std::max(0, st.timer));
  label(296, 0, value, colors[st.timer < 11 ? 5 : 15]);
  fill(128, 0, 46, 6, colors[l.number == 1 ? 4 : 0]);
  fill(128, 0, std::clamp(st.food_count, 0, 46), 5, colors[5]);
  fill(128, 0, 1, 6, colors[15]);
  fill(129, 6, 45, 1, colors[15]);
  fill(174, 0, 1, 5, colors[15]);
  for (unsigned i = 0; i < 5; ++i)
    fill(136 + i * 7, 3, 1, 2, colors[15]);
  label(4, 10, "ENERGY:", colors[15]);
  fill(64, 10, 41, 7, colors[0]);
  for (int n = 0; n < std::clamp(st.player.energy, 0, 10); ++n)
    fill(65 + n * 4, 11, 2, 5, colors[5]);
  return f;
}
} // namespace titus::pre1_dos
