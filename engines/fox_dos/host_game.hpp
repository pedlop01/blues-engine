// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "game.hpp"
namespace titus::fox_dos {
struct Input {
  bool left = false, right = false, up = false, down = false, attack = false;
};
struct Assets {
  fs::path root;
  std::string fingerprint;
  explicit Assets(const fs::path &p) : root(p) {
    if (fox_dos_init(p.u8string().c_str()) < 0)
      throw std::runtime_error(fox_dos_error());
    fingerprint = file_sha256(p / "SPREXP.SQZ");
    load(1);
  }
  ~Assets() { fox_dos_close(); }
};
struct Game {
  struct State {
    unsigned current_level = 1;
    struct Player {
      int x = 0, y = 0, lives = 3;
    } player;
    bool game_over = false;
    long score = 0;
  } state;
  std::uint64_t serial = fresh_serial();
  Snapshot saved;
  std::vector<Pose> previous;
  std::uint64_t ticks = 0;
  bool ending = false, level_complete = false;
  void project() {
    auto *w = fox_dos_world();
    state.current_level = w->levelnumber + 1;
    state.player = {w->player.sprite.x, w->player.sprite.y, w->lives};
    state.score = w->extrabonus;
  }
  void bind() const {
    if (active_serial() != serial && !saved.values.empty()) {
      restore(saved);
      active_serial() = serial;
    }
    fox_dos_world()->lives = state.player.lives;
  }
  void start(Assets &, unsigned n = 1, int lives = 3, long score = 0) {
    load(n, lives);
    fox_dos_world()->extrabonus = score;
    saved = snapshot();
    serial = fresh_serial();
    active_serial() = serial;
    previous = poses();
    state.game_over = ending = level_complete = false;
    ticks = 0;
    project();
  }
  void advance(Assets &a) {
    int bonus = state.score;
    int lives = std::min(99, state.player.lives + bonus / 10);
    unsigned next = state.current_level + 1;
    start(a, next, lives, bonus % 10);
  }
  void tick(Assets &, const Input &i, bool god, bool infinite) {
    if (state.game_over || ending || level_complete)
      return;
    bind();
    previous = poses();
    int r = fox_dos_step(unsigned(i.left) | (unsigned(i.right) << 1) |
                             (unsigned(i.up) << 2) | (unsigned(i.down) << 3) |
                             (unsigned(i.attack) << 4),
                         god, infinite);
    ++ticks;
    if (r == 1) {
      if (state.current_level < 15)
        level_complete = true;
      else
        ending = true;
    }
    if (r == 2)
      state.game_over = true;
    if (r == 4)
      ending = true;
    project();
    saved = snapshot();
    serial = fresh_serial();
    active_serial() = serial;
  }
};
inline unsigned display_level(unsigned n) { return n == 15 ? 16 : n; }
inline unsigned internal_level(unsigned n) { return n == 16 ? 15 : n; }
inline Json save_game(const Game &g, const std::string &fp) {
  g.bind();
  auto doc = save(snapshot(), fp);
  doc["payload"]["host"] = {{"ticks", g.ticks},
                            {"game_over", g.state.game_over},
                            {"ending", g.ending},
                            {"level_complete", g.level_complete}};
  doc["payload_sha256"] = titus::sha256(doc["payload"].dump());
  return doc;
}
inline Game restore_game(Assets &, const Json &doc, const std::string &fp) {
  auto old = snapshot();
  auto snap = decode(doc, fp);
  auto host = doc.at("payload").value("host", Json::object());
  if (!host.is_object() ||
      (!host.empty() &&
       (host.size() != 4 || !host.at("ticks").is_number_unsigned() ||
        !host.at("game_over").is_boolean() || !host.at("ending").is_boolean() ||
        !host.at("level_complete").is_boolean())))
    throw std::runtime_error("Invalid Fox host state");
  restore(snap);
  Game g;
  g.saved = snapshot();
  g.previous = poses();
  g.project();
  g.ticks = host.value("ticks", std::uint64_t(0));
  g.state.game_over = host.value("game_over", false);
  g.ending = host.value("ending", false);
  g.level_complete = host.value("level_complete", false);
  restore(old);
  return g;
}
inline Frame render(Assets &, const Game &g, const View &v) {
  g.bind();
  return render(v, g.previous);
}
inline void text(Frame &f, const Assets &, int x, int y, const std::string &s,
                 std::uint32_t c = 0xffeeeeee, unsigned scale = 1) {
  text(f, x, y, s, c, scale);
}
} // namespace titus::fox_dos
