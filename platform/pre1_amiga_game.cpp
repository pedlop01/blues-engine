// SPDX-License-Identifier: GPL-3.0-or-later
// Native Amiga asset binding. Simulation derives from the isolated GPL Olduvai
// kernel (see engines/pre1_amiga/logic/UPSTREAM.json), with Amiga tables/RNG.
#include "titus/pre1_amiga_game.hpp"
#include "core/game_tables.hpp"
#include "core/rng.hpp"
#include "systems/cave_logic.hpp"
#include "systems/monster_ai.hpp"
#include "systems/screen_topology.hpp"
#include "systems/secret.hpp"
#include "systems/transitions.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
namespace titus::pre1_amiga {
namespace {
std::string lower(std::string s) {
  for (auto &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
int word(const Bytes &b, unsigned p) {
  unsigned v = (unsigned(b.at(p)) << 8) | b.at(p + 1);
  return v < 32768 ? int(v) : int(v) - 65536;
}
std::vector<olduvai::prepare::ObjectRecord>
records(const std::vector<ObjectRecord> &input) {
  std::vector<olduvai::prepare::ObjectRecord> out;
  for (const auto &r : input) {
    olduvai::prepare::ObjectRecord v;
    v.type = r.type;
    v.words.assign(r.words.begin(), r.words.end());
    out.push_back(std::move(v));
  }
  return out;
}
olduvai::prepare::MonsterTable monster(const Bytes &b, unsigned off) {
  auto w = [&](unsigned n) { return word(b, off + n * 2); };
  olduvai::prepare::MonsterTable m;
  m.dat00 = w(0);
  m.dat02 = w(1);
  m.di = w(3);
  m.si = w(4);
  m.var18 = w(5);
  m.var16 = w(6);
  m.score = w(7);
  m.ko_spr = w(8) - 1;
  m.init_spr = w(9) - 1;
  m.move_spr = w(11) - 1;
  m.away_spr = w(13) - 1;
  m.energy = w(14);
  m.direction_flag = w(15);
  for (unsigned n = 0; n < 8; ++n)
    m.walk_offsets[n] = w(n + 16);
  return m;
}
void stamp(Scene &out, const CollisionRules &rules, unsigned id, int x, int y) {
  auto b = collision({TilePlacement{x, y, id, 0}}, rules);
  for (unsigned n = 0; n < b.size(); ++n)
    out.collision[n] |= b[n];
}
} // namespace
Files::Files(const std::filesystem::path &root) {
  if (!std::filesystem::is_directory(root))
    throw std::runtime_error("Prehistorik Amiga data directory is missing");
  for (const auto &e : std::filesystem::directory_iterator(root)) {
    if (!e.is_regular_file())
      continue;
    auto name = lower(e.path().filename().u8string());
    if (!paths_.emplace(name, e.path()).second)
      throw std::runtime_error("Ambiguous Amiga resource name: " + name);
  }
}
Bytes Files::read(const std::string &name) const {
  auto it = paths_.find(lower(name));
  if (it == paths_.end())
    throw std::runtime_error("Missing Prehistorik Amiga resource: " + name);
  std::ifstream f(it->second, std::ios::binary);
  if (!f)
    throw std::runtime_error("Cannot open Amiga resource: " + name);
  f.seekg(0, std::ios::end);
  auto size = f.tellg();
  if (size < 0 || size > 16 * 1024 * 1024)
    throw std::runtime_error("Invalid Amiga resource size");
  f.seekg(0);
  Bytes out(static_cast<std::size_t>(size));
  if (!out.empty() && !f.read(reinterpret_cast<char *>(out.data()), out.size()))
    throw std::runtime_error("Cannot read Amiga resource: " + name);
  return out;
}
Assets::Assets(const std::filesystem::path &root)
    : files(root), rules(pre1_amiga::rules(files.read("main"))) {
  const auto &d = rules.sections[2];
  olduvai::core::GameTables t;
  for (unsigned n = 0; n < 22; ++n)
    t.cave_sizes[n] = word(d, 0x98c + 2 * n);
  for (unsigned n = 22; n < 26; ++n)
    t.cave_sizes[n] = 224;
  for (unsigned n = 0; n < 13; ++n)
    t.cave_sizes[26 + n] = word(d, 0x9b8 + 2 * n);
  for (unsigned n = 0; n < 14; ++n)
    t.cave_sizes[39 + n] = word(d, 0x9d2 + 2 * n);
  for (unsigned n = 0; n < 10; ++n)
    t.secret_scores[n] = word(d, 0x960 + 2 * n);
  olduvai::core::install_game_tables(t);
  constexpr unsigned types[] = {10, 11, 12, 13, 26, 27, 33, 34, 38, 39},
                     offsets[] = {0xd68,  0xd98,  0xdc8,  0xdf8,  0x18c2,
                                  0x1922, 0x1978, 0x19a8, 0x1a00, 0x1a30};
  for (unsigned n = 0; n < 10; ++n)
    monsters.main[types[n]] = monster(d, offsets[n]);
  monsters.alt[26] = monster(d, 0x18f2);
  secrets = object_screens(d, 0xe28, false);
  caves = object_screens(d, 0xeba, false);
  for (unsigned block = 0; block < 2; ++block)
    for (unsigned n = 0; n < 32; ++n) {
      unsigned p = 0xb0e + block * 192 + n * 6;
      int raw = word(d, p), id = word(d, p + 4);
      id = id == 30 ? 31 : id == 29 ? 30 : id == 20 ? 32 : id == 5 ? 29 : id;
      olduvai::prepare::CaveTileRecord r{raw - (raw & 15), word(d, p + 2),
                                         raw & 15, id};
      (block ? l3_caves.even : l3_caves.odd).push_back(r);
    }
  font = sprites(files.read("char_set1.mat"), true);
}
LevelAssets &Assets::level(unsigned n) {
  auto found = levels.find(n);
  if (found != levels.end())
    return found->second;
  LevelAssets l;
  l.number = n;
  auto bank = [&](std::string name) {
    return sprites(lzss(files.read(name + ".mat")));
  };
  l.actors = bank("l" + std::to_string(n) + "spr");
  if (n & 1) {
    l.surface =
        surface(n, files.read("level" + std::to_string(n) + ".dat"), rules);
    l.dur = collision_rules(files.read("level" + std::to_string(n) + ".dur"));
    l.elements = bank("eleml" + std::to_string(n));
    l.element_count = l.elements.size();
    if (n == 3) {
      auto extra = bank("eleml3b");
      l.elements.insert(l.elements.end(), extra.begin(), extra.end());
      l.cave = bank("grot3");
    } else if (n != 7) {
      l.cave = bank("grot" + std::to_string(n));
      if (n == 5)
        l.elements.insert(l.elements.end(), l.cave.begin(), l.cave.end());
    }
    if (n != 3)
      l.background = fox_amiga::picture(
          files.read("fond" + std::to_string(n) + ".pc1"), 4, true);
    for (unsigned s = 0; s < 19; ++s)
      l.scenes[s] = scene(l.surface, s, l.element_count, l.dur);
  } else {
    l.background = fox_amiga::picture(files.read("ring.pc1"), 4, true);
    l.elements = bank("eleml2");
    if (n == 6)
      for (unsigned h = 0; h < 4; ++h)
        l.giant[h] = bank("h" + std::to_string(h + 1));
  }
  return levels.emplace(n, std::move(l)).first->second;
}
Scene trunk_platform(const LevelAssets &l, int y_offset) {
  Scene result;
  std::vector<TilePlacement> placements;
  const auto &source = l.surface.screens[17];
  for (unsigned n = 0; n < std::min<std::size_t>(16, source.size()); ++n) {
    auto tile = source[n];
    unsigned id = tile.sprite;
    tile.sprite = id == 29   ? 30
                  : id == 28 ? 29
                  : id == 19 ? 31
                  : id == 4  ? 28
                             : id;
    tile.y += y_offset;
    result.commands.push_back({0, tile.x, tile.y, tile.sprite, 0, 0});
    placements.push_back(tile);
  }
  result.collision = collision(placements, l.dur);
  return result;
}
unsigned next_level(unsigned n) {
  switch (n) {
  case 1:
    return 2;
  case 2:
    return 5;
  case 5:
    return 4;
  case 4:
    return 3;
  case 3:
    return 6;
  case 6:
    return 7;
  case 7:
    return 0;
  default:
    throw std::runtime_error("Invalid campaign level");
  }
}
Scene room_scene(const Assets &assets, const LevelAssets &l,
                 const olduvai::systems::SystemsState &st) {
  if (st.current_screen < 100) {
    if (st.current_screen < 0 || st.current_screen >= 19)
      throw std::runtime_error("Invalid surface screen");
    return l.scenes[st.current_screen];
  }
  Scene out;
  auto sprite = [&](int x, int y, unsigned id) {
    out.commands.push_back({0, x, y, id, 0, 0});
  };
  if (st.secret_flag) {
    for (int x = 0; x < 320; x += 48)
      sprite(x, 168, 1);
    for (int x = 0; x < 320; ++x)
      out.collision[168 * 40 + x / 8] |= 128 >> (x & 7);
    return out;
  }
  int cave = st.cave_index;
  if (cave < 0 || cave >= 53)
    throw std::runtime_error("Invalid cave index");
  int width = olduvai::core::game_tables().cave_sizes[cave];
  if (l.number == 3) {
    const auto &recs =
        ((cave - 22) & 1) ? assets.l3_caves.odd : assets.l3_caves.even;
    for (const auto &r : recs) {
      unsigned id = r.final_si >= 30 ? l.elements.size() + (r.final_si - 30)
                                     : r.final_si - 1;
      sprite(r.x, r.y, id);
      if (r.final_si < 30)
        stamp(out, l.dur, id, r.x, r.y);
    }
    out.commands.push_back({1, 74, 160, 10, 38, 0});
    sprite(0, 0, 32);
    return out;
  }
  out.commands.push_back({1, 0, 9, 319, 190, 0});
  for (int x = 0; x < 320; ++x)
    out.collision[168 * 40 + x / 8] |= 128 >> (x & 7);
  if (l.number == 7) {
    int x = -16;
    for (; x <= width - 32; x += 64) {
      sprite(x, 87, 29);
      sprite(x + 16, 50, 31);
    }
    sprite(x, 87, 30);
  } else {
    int x = -32;
    for (; x < width; x += 64)
      sprite(x, 50, 1);
    sprite(x, 50, 0);
  }
  return out;
}
void Game::start(Assets &a, unsigned level, int lives, long score,
                 std::uint32_t seed) {
  if (level < 1 || level > 7)
    throw std::runtime_error("Invalid Prehistorik level");
  state = {};
  club_phase = 0;
  transition_ticks = 0;
  trunk_ticks = 0;
  trunk_overlay = false;
  secret_scatter.clear();
  store.clear();
  bound_key = -1;
  previous_screen = 0;
  ending = false;
  ticks = 0;
  rng = seed;
  boss2 = {};
  boss2_pose = {0, 0};
  spring_pose = {0, 0};
  boss4 = {};
  boss6 = {};
  state.current_level = level;
  state.player.lives = lives;
  state.score = score;
  auto &assets = a.level(level);
  if (!(level & 1)) {
    boss_player = olduvai::systems::init_boss_player(lives, score);
    if (level == 4)
      boss_player.x = boss_player.prev_x = 210;
    return;
  }
  int x = level == 1   ? 100
          : level == 3 ? 115
          : level == 5 ? 140
                       : 95,
      y = level == 1   ? 66
          : level == 3 ? 110
          : level == 5 ? 65
                       : 10;
  state.player.reset_for_level(x, y);
  state.player.lives = lives;
  olduvai::core::global_rng().reseed(rng);
  auto spawn = [&](const ObjectScreens &screens, int base, bool cave) {
    for (unsigned s = 0; s < screens.size(); ++s)
      store[base + s] = olduvai::systems::spawn_screen_entities(
          records(screens[s]), a.monsters, cave ? int(s) : -1);
  };
  spawn(assets.surface.objects, 0, false);
  spawn(a.secrets, 2000, false);
  spawn(a.caves, 1000, true);
  rng = olduvai::core::global_rng().state();
  bind(a);
}
void Game::bind(Assets &a) {
  auto &l = a.level(state.current_level);
  if (!(l.number & 1))
    return;
  int key = state.current_screen < 100 ? state.current_screen
                                       : (state.secret_flag ? 2000 : 1000) +
                                             state.current_screen - 100;
  if (key != bound_key) {
    if (bound_key >= 0)
      store[bound_key] = std::move(state.entities);
    state.entities = std::move(store[key]);
    bound_key = key;
    if (state.current_screen >= 100 || previous_screen >= 100 ||
        std::abs(state.current_screen - previous_screen) > 1 ||
        !olduvai::systems::seam_contiguous(state.current_level, previous_screen,
                                           state.current_screen)) {
      previous_screen = state.current_screen;
      state.player.prev_x = state.player.x;
      state.player.prev_y = state.player.y;
      state.player.prev_dx = state.player.dx;
      state.player.prev_dy = state.player.dy;
    }
    olduvai::systems::clear_per_screen_state(state);
    olduvai::systems::refresh_entity_sprites_on_screen_bind(
        state.entities, state.l3a_phase_counter);
  }
  state.collision.restore(room_scene(a, l, state).collision);
  if (l.number == 3 && state.current_screen == 18 && trunk_overlay) {
    auto overlay = trunk_platform(l);
    auto bytes = state.collision.bytes();
    for (unsigned n = 0; n < 8000; ++n)
      bytes[n] |= overlay.collision[n];
    state.collision.restore(bytes);
  }
  state.screen_change = false;
}
void Game::tick(Assets &a, const olduvai::systems::FrameInputs &input,
                bool invulnerable, bool infinite_lives) {
  if (ending || state.game_over)
    return;
  if (transition_ticks) {
    ++ticks;
    if (++transition_ticks > 18)
      start(a, next_level(state.current_level), state.player.lives, state.score,
            rng);
    return;
  }
  olduvai::core::global_rng().reseed(rng);
  if (trunk_ticks) {
    ++ticks;
    if (++trunk_ticks == 45) {
      olduvai::systems::roll_l3_descent_smoke_jitter(state);
      rng = olduvai::core::global_rng().state();
    }
    if (trunk_ticks > 65) {
      trunk_ticks = 0;
      trunk_overlay = true;
      bind(a);
    }
    return;
  }
  previous_screen = state.current_screen;
  const int old_screen = state.current_screen;
  const int lives =
      (state.current_level & 1) ? state.player.lives : boss_player.lives;
  bool complete = false;
  if (state.current_level & 1) {
    if (state.secret_flag) {
      secret_scatter.clear();
      for (unsigned i = 0; i < 10000; i += 48) {
        int x = olduvai::core::global_rng().bounded(320),
            y = olduvai::core::global_rng().bounded(134) + 10,
            id = olduvai::core::global_rng().bounded(2) + 17;
        secret_scatter.push_back({x, y, id});
      }
    }
    if (invulnerable) {
      state.player.hit_counter = std::max(state.player.hit_counter, 2);
      state.player.energy = 10;
      state.timer = 99;
    }
    state.player.prev_x = state.player.x;
    state.player.prev_y = state.player.y;
    state.player.prev_dx = state.player.dx;
    state.player.prev_dy = state.player.dy;
    for (auto &e : state.entities) {
      e.prev_x = e.x;
      e.prev_y = e.y;
      e.prev_current_y = e.current_y;
      e.prev_throw_x = e.throw_x;
      e.prev_throw_y = e.throw_y;
      e.prev_draw_dy = e.draw_dy;
    }
    olduvai::systems::wrap_frame_counter(state, false);
    olduvai::systems::run_tick(state, input, false);
    club_phase = state.player.club_flag;
    if (state.player.death_counter || state.player.cave_warp_freeze)
      state.player.club_flag = club_phase = 0;
    else if (state.player.club_flag > 0)
      --state.player.club_flag;
    olduvai::systems::end_tick(state, false);
    olduvai::systems::tick_teleport_fx(state);
    olduvai::systems::tick_get_ready(state);
    olduvai::systems::tick_cave_emerge(state);
    if (infinite_lives) {
      state.player.lives = lives;
      state.game_over = false;
    }
    if (state.current_level == 3 && old_screen == 17 &&
        state.current_screen == 18) {
      trunk_ticks = 1;
      trunk_overlay = false;
      club_phase = 0;
    }
    if (state.screen_change && !state.level_complete)
      bind(a);
    state.food_count = std::min(46, state.food_count);
    complete = state.level_complete;
  } else {
    boss_player.prev_x = boss_player.x;
    boss_player.prev_y = boss_player.y;
    boss4.prev_x = boss4.boss_x;
    boss4.prev_y = boss4.boss_y;
    for (auto &slot : boss2.slots)
      slot.prev_x = slot.x;
    if (invulnerable)
      boss_player.hit_counter = std::max(boss_player.hit_counter, 2);
    olduvai::systems::BossInputs in{input.left, input.right,
                                    input.up || input.jump, input.attack};
    switch (state.current_level) {
    case 2:
      olduvai::systems::update_l2_boss_frame(boss_player, boss2, in);
      boss2_pose = {boss2.jaw_state, boss2.arm_frame};
      olduvai::systems::tick_l2_boss_post_render(boss_player, boss2);
      complete = boss2.win_flag;
      break;
    case 4:
      spring_pose[0] = boss4.spring_state;
      olduvai::systems::update_l4_boss_frame(boss_player, boss4, in);
      if (spring_pose[0] == 0 && boss4.spring_state)
        spring_pose[0] = 1;
      complete = boss4.win_flag == 100;
      break;
    case 6:
      spring_pose[1] = boss6.ground_punch_state;
      olduvai::systems::tick_l6_boss_post_render(boss6);
      if (boss6.win_flag)
        olduvai::systems::update_l6_victory_frame(boss_player, boss6);
      else
        olduvai::systems::update_l6_boss_frame(boss_player, boss6, in);
      if (spring_pose[1] == 0 && boss6.ground_punch_state)
        spring_pose[1] = 1;
      complete = boss6.win_flag == 100;
      break;
    }
    if (infinite_lives)
      boss_player.lives = lives;
    state.player.lives = boss_player.lives;
    state.score = boss_player.score;
    state.game_over = boss_player.lives < 0;
  }
  rng = olduvai::core::global_rng().state();
  ++ticks;
  if (complete) {
    if (state.current_level == 2) {
      transition_ticks = 1;
      return;
    }
    unsigned next = next_level(state.current_level);
    if (!next) {
      ending = true;
      return;
    }
    int saved_lives = state.player.lives;
    long score = state.score;
    start(a, next, saved_lives, score, rng);
  }
}
} // namespace titus::pre1_amiga
