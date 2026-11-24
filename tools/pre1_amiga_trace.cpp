// SPDX-License-Identifier: GPL-3.0-or-later
// Private original-comparison projections; never installed as a game.
#include "core/rng.hpp"
#include "systems/collision_dispatch.hpp"
#include "systems/monster_ai.hpp"
#include "systems/transitions.hpp"
#include "titus/pre1_amiga_render.hpp"
#include "titus/pre1_amiga_save.hpp"
#include <fstream>
#include <iostream>
using namespace titus::pre1_amiga;
int run(const std::vector<std::string> &args) {
  if (args.size() != 5)
    throw std::runtime_error("Usage: trace DATAPATH MODE CASES OUTPUT");
  Assets assets(std::filesystem::u8path(args[1]));
  titus::Json cases;
  std::ifstream input(std::filesystem::u8path(args[3]));
  input >> cases;
  titus::Json results = titus::Json::array();
  for (const auto &c : cases) {
    const auto &mode = args[2];
    if (mode == "rng") {
      olduvai::core::RandLcg16 rng(c.at("seed"));
      titus::Json rows = titus::Json::array();
      for (const auto &bound : c.at("bounds")) {
        unsigned value = rng.bounded(bound.get<unsigned>());
        rows.push_back({value, rng.state()});
      }
      results.push_back(rows);
      continue;
    }
    unsigned n = c.at("level");
    Game game;
    game.start(assets, n);
    if (mode == "scroll") {
      game.state.current_screen = c.at("screen");
      game.bind(assets);
      game.state.entities.clear();
      for (auto &item : game.store)
        item.second.clear();
      game.state.player.sprite = -1;
      game.state.player.hit_counter = 0;
      game.state.get_ready_counter = 0;
      View v;
      v.width = 320;
      v.height = 200;
      v.x = c.at("camera");
      auto f = render(assets, game, v);
      std::string rgb;
      for (unsigned y = 22; y < 200; ++y)
        for (unsigned x = 0; x < 320; ++x) {
          auto px = f.pixels[y * 320 + x];
          for (int shift : {16, 8, 0})
            rgb += char(px >> shift);
        }
      results.push_back({{"sha256_rgb", titus::sha256(rgb)}});
      continue;
    }
    if (mode == "trunk") {
      auto &l = assets.level(3);
      titus::Json first = titus::Json::array(), second = titus::Json::array();
      auto commands = [&](int dy) {
        titus::Json result = titus::Json::array();
        for (const auto &cmd : trunk_platform(l, dy).commands)
          result.push_back({cmd.x, cmd.y, cmd.a});
        return result;
      };
      for (int frame = 0; frame < 44; ++frame)
        first.push_back(commands(frame * 4));
      for (int frame = 0; frame < 21; ++frame)
        second.push_back(commands(-80 + frame * 4));
      auto bits = l.scenes[18].collision, overlay = trunk_platform(l).collision;
      for (unsigned i = 0; i < 8000; ++i)
        bits[i] |= overlay[i];
      olduvai::core::global_rng().reseed(0);
      olduvai::systems::roll_l3_descent_smoke_jitter(game.state);
      results.push_back({{"first", first},
                         {"second", second},
                         {"jitter", game.state.l3_descent_smoke_jitter},
                         {"rng", olduvai::core::global_rng().state()},
                         {"collision_sha256", titus::sha256(std::string(
                                                  bits.begin(), bits.end()))}});
      continue;
    }
    if (mode == "bosses") {
      titus::Json rows = titus::Json::array();
      unsigned pattern = c.value("pattern", 0u);
      for (unsigned tick = 0; tick < c.value("ticks", 240u); ++tick) {
        olduvai::systems::FrameInputs in;
        in.right = pattern == 1 || (pattern == 3 && tick % 120 < 60);
        in.left = pattern == 2 || (pattern == 3 && tick % 120 >= 60);
        in.up = pattern == 3 && tick % 60 < 20;
        in.attack = pattern == 3 && tick % 45 < 5;
        game.tick(assets, in, false, false);
        const auto &p = game.boss_player;
        const auto &b = game.boss2;
        titus::Json boss =
            n == 2 ? titus::Json({b.stomp_timer, b.jaw_state, b.arm_frame,
                                  b.health, b.cooldown})
            : n == 4
                ? titus::Json({game.boss4.health, game.boss4.boss_x,
                               game.boss4.boss_y, game.boss4.walk_frame,
                               game.boss4.direction, game.boss4.stun_counter,
                               game.boss4.hit_flash, game.boss4.spring_state})
                : titus::Json({game.boss6.health, game.boss6.frame_counter,
                               game.boss6.ground_punch_state});
        rows.push_back(
            {{"player",
              {p.x, p.y, int(p.facing_left), p.walk_frame, p.jump_counter,
               p.club_flag, p.death_counter, p.dead_counter, p.halo_flag,
               p.hit_counter, p.halo_toggle, p.lives, int(p.attack_latch)}},
             {"boss", boss},
             {"slots", b.slots},
             {"corpse", {p.dead_x_save, p.dead_anim_y}},
             {"rng", game.rng}});
      }
      results.push_back(rows);
      continue;
    }
    if (mode == "spawns") {
      results.push_back(
          {{"rng", game.rng}, {"state", game.state}, {"store", game.store}});
      continue;
    }
    if (mode == "rooms") {
      auto &st = game.state;
      st.current_screen = 100 + c.at("room").get<int>();
      st.cave_index = c.at("room");
      st.cave_flag = 1;
      auto scene = room_scene(assets, assets.level(n), st);
      titus::Json commands = titus::Json::array();
      for (const auto &cmd : scene.commands)
        commands.push_back({cmd.kind, cmd.x, cmd.y, cmd.a, cmd.b, cmd.color});
      std::string bits(scene.collision.begin(), scene.collision.end());
      results.push_back(
          {{"commands", commands}, {"collision_sha256", titus::sha256(bits)}});
      continue;
    }
    auto &s = game.state;
    s.current_screen = c.value("screen", 0);
    s.player.x = c.value("x", 100);
    s.player.y = c.value("y", 70);
    s.player.restart_x = s.player.x;
    s.player.restart_y = s.player.y;
    s.player.restart_screen = 0;
    game.bind(assets);
    if (c.contains("player"))
      for (auto it = c["player"].begin(); it != c["player"].end(); ++it) {
        titus::Json p = s.player;
        p[it.key()] = it.value();
        s.player = p.get<olduvai::systems::PlayerState>();
      }
    s.continuous_world = false;
    game.connect(assets);
    s.frame_counter = 0;
    s.get_ready_counter = 0;
    s.player.hit_counter = 0;
    s.player.y_vel = 20;
    s.entities.clear();
    if (mode == "entities") {
      auto screen = assets.level(n).surface.objects.at(s.current_screen);
      std::vector<olduvai::prepare::ObjectRecord> records;
      for (const auto &r : screen) {
        olduvai::prepare::ObjectRecord value;
        value.type = r.type;
        value.words.assign(r.words.begin(), r.words.end());
        records.push_back(value);
      }
      olduvai::core::global_rng().reseed(0);
      s.entities =
          olduvai::systems::spawn_screen_entities(records, assets.monsters);
      if (c.contains("monster_state"))
        for (auto &e : s.entities)
          if (e.ko_spr) {
            e.state = c.at("monster_state");
            e.state_counter = 0;
            e.ko_counter = e.state == 5 ? 40 : 0;
          }
    }
    titus::Json rows = titus::Json::array();
    unsigned ticks = c.value("ticks", 120u), pattern = c.value("pattern", 0u);
    for (unsigned tick = 0; tick < ticks; ++tick) {
      olduvai::systems::FrameInputs in;
      in.right = pattern == 1 || (pattern == 3 && tick % 120 < 60);
      in.left = pattern == 2 || (pattern == 3 && tick % 120 >= 60);
      in.up = pattern == 3 && tick % 60 < 20;
      in.attack = pattern == 3 && tick % 45 < 5;
      if (mode == "player") {
        olduvai::systems::apply_inputs(s, in);
        olduvai::systems::update_player(s);
        if (s.player.club_flag)
          --s.player.club_flag;
        const auto &p = s.player;
        rows.push_back({p.x, p.y, p.gravity_flag, p.y_vel, p.walk_frame,
                        p.facing_left, p.dx, p.dy, p.sprite, p.club_flag,
                        p.attack_latch, p.restart_x, p.restart_y,
                        p.restart_screen});
      } else if (mode == "entities") {
        olduvai::systems::EntityTick t;
        t.player_x = s.player.x;
        t.player_y = s.player.y;
        t.frame = tick % 62;
        t.collision = &s.collision;
        t.l3a_phase_counter = s.l3a_phase_counter;
        auto result = olduvai::systems::update_entities(s.entities, t);
        s.l3a_phase_counter = result.l3a_phase_counter;
        rows.push_back({{"rng", olduvai::core::global_rng().state()},
                        {"entities", s.entities}});
      } else
        throw std::runtime_error("Unknown projection mode");
    }
    results.push_back(rows);
  }
  std::ofstream output(std::filesystem::u8path(args[4]));
  output << results.dump();
  if (!output)
    throw std::runtime_error("Cannot write trace");
  return 0;
}
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) try {
  std::vector<std::string> args;
  for (int i = 0; i < argc; ++i)
    args.push_back(std::filesystem::path(argv[i]).u8string());
  return run(args);
} catch (const std::exception &e) {
  std::cerr << e.what() << '\n';
  return 1;
}
#else
int main(int argc, char **argv) try {
  return run(std::vector<std::string>(argv, argv + argc));
} catch (const std::exception &e) {
  std::cerr << e.what() << '\n';
  return 1;
}
#endif
