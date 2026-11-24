// SPDX-License-Identifier: GPL-3.0-or-later
#include "titus/pre1_amiga_save.hpp"
#include <cmath>
#include <stdexcept>
namespace olduvai::core {
void to_json(titus::Json &j, const CollisionBitmap &c) { j = c.bytes(); }
void from_json(const titus::Json &j, CollisionBitmap &c) {
  if (!j.is_array() || j.size() != 8000)
    throw std::runtime_error("Invalid saved collision bitmap");
  std::array<std::uint8_t, 8000> bytes;
  for (unsigned n = 0; n < 8000; ++n) {
    if (!j[n].is_number_integer() || j[n] < 0 || j[n] > 255)
      throw std::runtime_error("Invalid collision bit");
    bytes[n] = j[n].get<unsigned>();
  }
  c.restore(bytes);
}
} // namespace olduvai::core
namespace titus::pre1_amiga {
namespace {
void bounded(const Json &j, unsigned depth = 0) {
  if (depth > 12)
    throw std::runtime_error("Save nesting limit");
  if (j.is_array() || j.is_object()) {
    if (j.size() > 8000)
      throw std::runtime_error("Save collection limit");
    for (const auto &v : j)
      bounded(v, depth + 1);
  } else if (j.is_number()) {
    double v = j.get<double>();
    if (!std::isfinite(v) || v < -1000000 || v > 4294967295.)
      throw std::runtime_error("Saved number outside supported range");
  } else if (j.is_string() && j.get_ref<const std::string &>().size() > 128)
    throw std::runtime_error("Save string limit");
}
void valid(const Game &g) {
  const auto &s = g.state;
  const auto &p = s.player;
  auto range = [](int v, int lo, int hi) {
    if (v < lo || v > hi)
      throw std::runtime_error(
          "Invalid saved gameplay state: " + std::to_string(v) + " outside " +
          std::to_string(lo) + ".." + std::to_string(hi));
  };
  range(s.current_level, 1, 7);
  range(s.current_screen, 0, 152);
  if (s.current_screen > 18 && s.current_screen < 100 &&
      !(g.ending && s.current_level == 7 && s.current_screen == 19))
    throw std::runtime_error("Invalid saved room");
  range(p.walk_frame, 0, 5);
  range(p.club_flag, 0, 2);
  range(p.facing_left, 0, 1);
  range(p.sprite, 0, 511);
  range(p.energy, 0, 10);
  range(p.lives, -1, 99);
  range(p.gravity_flag, 0, 1000);
  range(p.death_counter, 0, 1000);
  range(g.club_phase, 0, 2);
  range(s.cave_index, -1, 52);
  range(s.secret_index, -1, 10);
  range(s.frame_counter, 0, 62);
  range(s.food_count, 0, 1000);
  range(g.boss_player.walk_frame, 0, 5);
  range(g.boss_player.club_flag, 0, 2);
  range(g.boss4.walk_frame, 0, 4);
  range(g.boss2.arm_frame, 0, 5);
  range(g.boss2.jaw_state, 0, 2);
  range(g.boss6.frame_counter, 0, 32);
  if (!g.ending && (s.current_level & 1)) {
    int key = s.current_screen < 100
                  ? s.current_screen
                  : (s.secret_flag ? 2000 : 1000) + s.current_screen - 100;
    if (g.bound_key != key)
      throw std::runtime_error("Saved room binding mismatch");
    if (s.current_screen >= 100 &&
        ((s.secret_flag &&
          (s.cave_flag || s.current_screen != 100 + s.secret_index)) ||
         (!s.secret_flag &&
          (!s.cave_flag || s.current_screen != 100 + s.cave_index))))
      throw std::runtime_error("Saved cave context mismatch");
  }
  if (s.entities.size() > 256 || g.store.size() > 90)
    throw std::runtime_error("Invalid saved entity population");
  unsigned total = s.entities.size();
  auto check = [&](const auto &entities) {
    for (const auto &e : entities) {
      range(int(e.obj_type), 1, 39);
      range(e.sprite, -1000, 511);
      range(e.state, -1, 1000);
      range(e.direction, 0, 2);
      if ((!e.walk_offsets.empty() && e.walk_offsets.size() != 8) ||
          (!e.alt_walk_offsets.empty() && e.alt_walk_offsets.size() != 8))
        throw std::runtime_error("Invalid saved animation");
    }
  };
  check(s.entities);
  for (const auto &slot : g.store) {
    range(slot.first, 0, 2010);
    if (slot.second.size() > 256)
      throw std::runtime_error("Invalid saved screen population");
    total += slot.second.size();
    check(slot.second);
  }
  if (total > 1024)
    throw std::runtime_error("Invalid saved world population");
}
} // namespace
Json save_game(const Game &g, const std::string &fingerprint) {
  Json payload = {{"state", g.state},
                  {"boss_player", g.boss_player},
                  {"boss2", g.boss2},
                  {"boss4", g.boss4},
                  {"boss6", g.boss6},
                  {"store", g.store},
                  {"bound_key", g.bound_key},
                  {"rng", g.rng},
                  {"ticks", g.ticks},
                  {"ending", g.ending},
                  {"club_phase", g.club_phase},
                  {"secret_scatter", g.secret_scatter},
                  {"transition_ticks", g.transition_ticks},
                  {"trunk_ticks", g.trunk_ticks},
                  {"trunk_overlay", g.trunk_overlay},
                  {"boss2_pose", g.boss2_pose},
                  {"previous_screen", g.previous_screen},
                  {"spring_pose", g.spring_pose}};
  return {{"schema_version", 1},
          {"format_id", "prehistorik.amiga.json"},
          {"fingerprint", fingerprint},
          {"payload_sha256", titus::sha256(payload.dump())},
          {"payload", payload}};
}
Game restore_game(Assets &a, const Json &doc, const std::string &fingerprint) {
  if (doc.value("schema_version", 0) != 1 ||
      doc.value("format_id", std::string()) != "prehistorik.amiga.json" ||
      doc.at("fingerprint") != fingerprint)
    throw std::runtime_error("Incompatible Prehistorik Amiga save");
  const auto &p = doc.at("payload");
  bounded(p);
  if (p.size() != 18 || doc.at("payload_sha256") != titus::sha256(p.dump()))
    throw std::runtime_error("Damaged Prehistorik Amiga save");
  Game g;
  g.state = p.at("state").get<olduvai::systems::SystemsState>();
  g.boss_player = p.at("boss_player").get<olduvai::systems::BossPlayerState>();
  g.boss2 = p.at("boss2").get<olduvai::systems::L2BossState>();
  g.boss4 = p.at("boss4").get<olduvai::systems::L4BossState>();
  g.boss6 = p.at("boss6").get<olduvai::systems::L6BossState>();
  g.store = p.at("store").get<decltype(g.store)>();
  g.bound_key = p.at("bound_key");
  g.rng = p.at("rng");
  g.ticks = p.at("ticks");
  g.ending = p.at("ending");
  g.club_phase = p.at("club_phase");
  g.previous_screen = p.at("previous_screen");
  g.spring_pose = p.at("spring_pose").get<decltype(g.spring_pose)>();
  if (g.previous_screen < 0 || g.previous_screen > 152 ||
      g.spring_pose[0] < 0 || g.spring_pose[0] > 3 || g.spring_pose[1] < 0 ||
      g.spring_pose[1] > 3)
    throw std::runtime_error("Invalid saved render phase");
  g.boss2_pose = p.at("boss2_pose").get<decltype(g.boss2_pose)>();
  if (g.boss2_pose[0] < 0 || g.boss2_pose[0] > 2 || g.boss2_pose[1] < 0 ||
      g.boss2_pose[1] > 5)
    throw std::runtime_error("Invalid boss pose");
  g.transition_ticks = p.at("transition_ticks");
  g.trunk_ticks = p.at("trunk_ticks");
  g.trunk_overlay = p.at("trunk_overlay");
  if (g.trunk_ticks > 65 ||
      ((g.trunk_ticks || g.trunk_overlay) && g.state.current_level != 3))
    throw std::runtime_error("Invalid saved trunk sequence");
  if (g.transition_ticks > 18)
    throw std::runtime_error("Invalid saved victory sequence");
  g.secret_scatter = p.at("secret_scatter").get<decltype(g.secret_scatter)>();
  if (g.secret_scatter.size() > 209)
    throw std::runtime_error("Invalid saved secret scatter");
  valid(g);
  a.level(g.state.current_level);
  return g;
}
} // namespace titus::pre1_amiga
