// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/rng.hpp"
#include "systems/cave_logic.hpp"
#include "systems/screen_topology.hpp"
#include "systems/secret.hpp"
#include "systems/transitions.hpp"
#include "titus/catalog.hpp"
#include "titus/pre1_amiga_game.hpp"
#include "titus/pre1_amiga_render.hpp"
#include "titus/pre1_amiga_save.hpp"
#include <fstream>
#include <iostream>
int main(int argc, char **argv) {
  try {
    if (argc != 2 && argc != 3)
      throw std::runtime_error("Usage: pre1-game-probe DATAPATH");
    titus::pre1_amiga::Assets a(std::filesystem::u8path(argv[1]));
    unsigned seam_cases = 0;
    for (unsigned level : {1u, 3u, 5u, 7u})
      for (int screen = 0; screen < 18; ++screen) {
        if (!olduvai::systems::continuous_walk_seam(level, screen, screen + 1))
          continue;
        for (int dir : {-1, 1}) {
          titus::pre1_amiga::Game g;
          g.start(a, level);
          auto &s = g.state;
          s.current_screen = screen + (dir < 0);
          g.bind(a);
          s.player.x = dir > 0 ? 323 : -4;
          s.player.y = 80;
          int before = s.current_screen * 320 + s.player.x;
          olduvai::systems::clamp_player_position(s);
          olduvai::systems::check_screen_transition(s);
          if (s.current_screen != screen + (dir > 0) ||
              s.current_screen * 320 + s.player.x != before)
            throw std::runtime_error("Seam discarded player displacement");
          g.bind(a);
          if (titus::pre1_amiga::player_world_x(g, 1) != before)
            throw std::runtime_error("Seam render placement mismatch");
          auto saved = titus::pre1_amiga::save_game(g, "probe");
          auto restored = titus::pre1_amiga::restore_game(a, saved, "probe");
          for (int x : {-24, 0, 300, 328})
            for (int y : {30, 80, 160})
              if (s.collision.test(x, y) != restored.state.collision.test(x, y))
                throw std::runtime_error(
                    "Seam collision neighbours not restored");
          ++seam_cases;
        }
      }
    {
      titus::pre1_amiga::Game g;
      g.start(a);
      auto doc = titus::pre1_amiga::save_game(g, "probe");
      doc["payload"]["state"].erase("continuous_world");
      doc["payload_sha256"] = titus::sha256(doc["payload"].dump());
      auto old = titus::pre1_amiga::restore_game(a, doc, "probe");
      if (!old.state.continuous_world)
        throw std::runtime_error("Legacy save migration failed");
    }
    for (int direction : {-1, 1}) {
      titus::pre1_amiga::Game g;
      g.start(a);
      auto &st = g.state;
      st.current_screen = direction > 0 ? 0 : 1;
      g.bind(a);
      st.entities.clear();
      for (auto &item : g.store)
        item.second.clear();
      st.get_ready_counter = 0;
      st.player.x = direction > 0 ? 310 : 5;
      st.player.y = 60;
      st.player.gravity_flag = 1;
      bool crossed = false;
      for (unsigned i = 0; i < 8; ++i) {
        int old_screen = st.current_screen;
        double before = st.current_screen * 320 + st.player.x;
        olduvai::systems::FrameInputs in;
        in.right = direction > 0;
        in.left = direction < 0;
        g.tick(a, in, true, true);
        double after = st.current_screen * 320 + st.player.x;
        if (st.current_screen != old_screen) {
          if (direction * (after - before) <= 0 ||
              direction * (after - before) > 8 ||
              titus::pre1_amiga::player_world_x(g, 0) != before)
            throw std::runtime_error(
                "Actual cave-seam traversal jumped or reversed");
          crossed = true;
          break;
        }
      }
      if (!crossed)
        throw std::runtime_error("Actual cave-seam traversal did not cross");
    }
    {
      titus::pre1_amiga::Game g;
      g.start(a);
      g.state.entities.clear();
      for (auto &item : g.store)
        item.second.clear();
      g.state.player.sprite = -1;
      g.state.player.hit_counter = 0;
      g.state.get_ready_counter = 0;
      olduvai::core::Entity e;
      e.active = e.visible = true;
      e.x = e.prev_x = 310;
      e.y = e.prev_y = 90;
      e.sprite = 0;
      g.state.entities.push_back(e);
      titus::pre1_amiga::View v;
      v.width = 320;
      v.height = 200;
      v.x = 240;
      auto frame = titus::pre1_amiga::render(a, g, v);
      auto &l = a.level(1);
      auto &sprite = l.actors[0];
      unsigned checked = 0;
      for (unsigned y = 0; y < sprite.height; ++y)
        for (unsigned x = 10; x < sprite.width; ++x) {
          unsigned i = y * sprite.width + x;
          if (sprite.opaque[i]) {
            auto expected = 0xff000000 | l.background.palette[sprite.colors[i]];
            if (frame.pixels[(90 + y) * 320 + 70 + x] != expected)
              throw std::runtime_error("Neighbour scenery erased an actor");
            ++checked;
          }
        }
      if (!checked)
        throw std::runtime_error("Empty seam actor fixture");
    }
    titus::Json report = titus::Json::array();
    titus::Json render_hashes = titus::Json::array();
    for (unsigned n = 1; n <= 7; ++n) {
      titus::pre1_amiga::Game g;
      g.start(a, n);
      if (argc == 3) {
        titus::pre1_amiga::View v;
        v.width = 320;
        v.height = 200;
        auto f = titus::pre1_amiga::render(a, g, v);
        std::ofstream out(std::filesystem::path(argv[2]) /
                              ("level-" + std::to_string(n) + ".ppm"),
                          std::ios::binary);
        out << "P6\n320 200\n255\n";
        for (auto px : f.pixels) {
          out.put(char(px >> 16));
          out.put(char(px >> 8));
          out.put(char(px));
        }
      }
      unsigned count = 0;
      for (const auto &p : g.store)
        count += p.second.size();
      count += g.state.entities.size();
      for (unsigned tick = 0; tick < 1200; ++tick) {
        olduvai::systems::FrameInputs in;
        in.right = tick % 240 < 120;
        in.left = !in.right;
        in.jump = tick % 40 < 15;
        in.attack = tick % 30 < 10;
        g.tick(a, in, true, true);
        if (tick % 100 == 0) {
          auto saved = titus::pre1_amiga::save_game(g, "probe");
          auto restored = titus::pre1_amiga::restore_game(a, saved, "probe");
          if (titus::pre1_amiga::save_game(restored, "probe") != saved)
            throw std::runtime_error("Save round-trip mismatch");
          auto expected = g;
          expected.tick(a, in, true, true);
          restored.tick(a, in, true, true);
          if (titus::pre1_amiga::save_game(expected, "probe") !=
              titus::pre1_amiga::save_game(restored, "probe"))
            throw std::runtime_error("Continuation mismatch");
        }
      }
      for (double zoom : {.75, 1., 1.5})
        for (bool classic : {false, true}) {
          titus::pre1_amiga::View v;
          v.width = 960;
          v.height = 600;
          v.zoom = zoom;
          v.alpha = .5;
          v.classic_hud = classic;
          v.x = g.state.current_screen < 100 && (n & 1)
                    ? g.state.current_screen * 320
                    : 0;
          auto f = titus::pre1_amiga::render(a, g, v);
          std::string bytes;
          bytes.reserve(f.pixels.size() * 4);
          for (auto px : f.pixels)
            for (int shift : {24, 16, 8, 0})
              bytes += char(px >> shift);
          render_hashes.push_back({{"level", n},
                                   {"zoom", zoom},
                                   {"classic_hud", classic},
                                   {"sha256", titus::sha256(bytes)}});
        }
      report.push_back({{"level", n},
                        {"entities", count},
                        {"screen", g.state.current_screen},
                        {"x", g.state.player.x},
                        {"y", g.state.player.y},
                        {"rng", g.rng}});
    }
    {
      titus::pre1_amiga::Game g;
      g.start(a, 3);
      g.state.current_screen = 17;
      g.bind(a);
      g.state.entities.clear();
      g.state.food_count = 46;
      g.state.player.x = 65;
      g.state.player.y = 68;
      g.state.player.gravity_flag = 0;
      g.tick(a, {}, true, true);
      if (g.trunk_ticks != 1)
        throw std::runtime_error("Trunk descent did not start");
      for (unsigned tick = 0; tick < 65; ++tick) {
        g.tick(a, {}, true, true);
        if (tick == 12 || tick == 50)
          g = titus::pre1_amiga::restore_game(
              a, titus::pre1_amiga::save_game(g, "probe"), "probe");
      }
      if (g.trunk_ticks || !g.trunk_overlay || g.state.current_screen != 18)
        throw std::runtime_error("Trunk descent did not finish");
      olduvai::systems::FrameInputs in;
      in.right = true;
      for (unsigned tick = 0; tick < 300 && g.state.current_level == 3; ++tick)
        g.tick(a, in, true, true);
      if (g.state.current_level != 6)
        throw std::runtime_error(
            "Cannot cross the descended trunk to the exit");
    }
    {
      titus::pre1_amiga::Game g;
      g.start(a);
      titus::pre1_amiga::View v;
      v.width = 320;
      v.height = 200;
      auto frame = titus::pre1_amiga::render(a, g, v);
      if (frame.pixels[24 * 320 + 160] != 0xff44aaee)
        throw std::runtime_error("Original FOND1 sky palette changed");
      g.ending = true;
      frame = titus::pre1_amiga::render(a, g, v);
      auto picture =
          titus::fox_amiga::picture(a.files.read("theend.pc1"), 4, true);
      for (unsigned i = 0; i < 64000; ++i)
        if (frame.pixels[i] !=
            (0xff000000 | picture.palette[picture.image.pixels[i]]))
          throw std::runtime_error("Ending PC1 palette/frame mismatch");
    }
    unsigned checked_rooms = 0;
    for (unsigned cave = 0; cave < 53; ++cave) {
      unsigned level = cave < 22 ? 1 : cave < 26 ? 3 : cave < 39 ? 5 : 7;
      titus::pre1_amiga::Game g;
      g.start(a, level);
      olduvai::systems::enter_cave(g.state, cave);
      g.bind(a);
      for (unsigned tick = 0; tick < 24; ++tick) {
        olduvai::systems::FrameInputs in;
        in.right = true;
        in.attack = tick % 8 < 3;
        g.tick(a, in, true, true);
      }
      auto saved = titus::pre1_amiga::save_game(g, "probe");
      auto copy = titus::pre1_amiga::restore_game(a, saved, "probe");
      titus::pre1_amiga::View v;
      v.width = 320;
      v.height = 200;
      auto frame = titus::pre1_amiga::render(a, copy, v);
      if (frame.pixels.size() != 64000 ||
          saved != titus::pre1_amiga::save_game(copy, "probe"))
        throw std::runtime_error("Cave save/render changed the state");
      ++checked_rooms;
    }
    for (unsigned screen : {5u, 14u}) {
      titus::pre1_amiga::Game g;
      g.start(a);
      g.state.current_screen = screen;
      g.bind(a);
      g.state.player.x = 50;
      g.state.player.y = 160;
      if (!olduvai::systems::check_secret_entry(g.state))
        throw std::runtime_error("Secret entrance failed");
      g.bind(a);
      for (unsigned tick = 0; tick < 30; ++tick)
        g.tick(a, {}, true, true);
      auto restored = titus::pre1_amiga::restore_game(
          a, titus::pre1_amiga::save_game(g, "probe"), "probe");
      if (!restored.state.secret_flag || restored.secret_scatter.size() != 209)
        throw std::runtime_error("Secret room/scatter was not retained");
      ++checked_rooms;
    }
    {
      titus::pre1_amiga::Game g;
      g.start(a);
      auto replay = g;
      for (unsigned tick = 0; tick < 80; ++tick) {
        olduvai::systems::FrameInputs in;
        in.right = tick < 40;
        in.left = !in.right;
        in.attack = tick % 12 < 4;
        g.tick(a, in);
        if (tick == 39)
          replay = titus::pre1_amiga::restore_game(
              a, titus::pre1_amiga::save_game(g, "probe"), "probe");
        if (tick >= 40)
          replay.tick(a, in);
      }
      if (titus::pre1_amiga::save_game(g, "probe") !=
          titus::pre1_amiga::save_game(replay, "probe"))
        throw std::runtime_error("Rewind replay diverged");
      auto doc = titus::pre1_amiga::save_game(g, "probe");
      doc["payload"]["state"]["player"]["x"] = 999;
      bool rejected = false;
      try {
        titus::pre1_amiga::restore_game(a, doc, "probe");
      } catch (const std::exception &) {
        rejected = true;
      }
      if (!rejected)
        throw std::runtime_error("Damaged save accepted");
    }
    // Explicit fixtures exercise actual exit/defeat handlers. They do not
    // claim that a human or an input-only bot completed the campaign.
    titus::Json exits = titus::Json::array();
    for (unsigned level : {1u, 3u, 5u, 7u}) {
      titus::pre1_amiga::Game g;
      g.start(a, level);
      g.state.current_screen = 18;
      g.bind(a);
      g.state.entities.clear();
      g.state.food_count = 46;
      g.state.player.x = level == 7 ? 240 : level == 3 ? 300 : 299;
      g.state.player.y = level == 7 ? 127 : level == 5 ? 0 : 100;
      if (level == 5)
        g.state.glider_active = true;
      if (level == 7) {
        g.state.player.cave_warp_freeze = 4001;
        g.state.player.x = 240;
      }
      olduvai::systems::FrameInputs in;
      in.right = level != 7;
      in.down = level == 7;
      for (unsigned tick = 0;
           tick < 20 && !g.ending && g.state.current_level == int(level);
           ++tick)
        g.tick(a, in, true, true);
      if (level == 7 ? !g.ending
                     : g.state.current_level !=
                           int(titus::pre1_amiga::next_level(level)))
        throw std::runtime_error("Surface exit failed at " +
                                 std::to_string(level));
      if (g.ending) {
        auto saved = titus::pre1_amiga::save_game(g, "probe");
        auto restored = titus::pre1_amiga::restore_game(a, saved, "probe");
        if (!restored.ending ||
            saved != titus::pre1_amiga::save_game(restored, "probe"))
          throw std::runtime_error("Ending save failed");
      }
      exits.push_back(
          {{"level", level}, {"next", g.ending ? 0 : g.state.current_level}});
    }
    for (unsigned level : {2u, 4u, 6u}) {
      titus::pre1_amiga::Game g;
      g.start(a, level);
      g.boss_player.halo_flag = 0;
      g.boss_player.hit_counter = 40;
      if (level == 2) {
        g.boss2.health = 273;
        g.boss2.jaw_state = 2;
      }
      if (level == 4) {
        g.boss4.health = 272;
        g.boss_player.x = 140;
      }
      if (level == 6) {
        g.boss6.health = 272;
        g.boss_player.y = 160;
      }
      for (unsigned tick = 0; tick < 600 && g.state.current_level == int(level);
           ++tick)
        g.tick(a, {}, true, true);
      if (g.state.current_level != int(titus::pre1_amiga::next_level(level)))
        throw std::runtime_error("Boss exit failed at " +
                                 std::to_string(level));
      exits.push_back({{"level", level}, {"next", g.state.current_level}});
    }
    std::cout << titus::Json({{"levels", report},
                              {"seam_cases", seam_cases},
                              {"render_hashes", render_hashes},
                              {"exit_fixtures", exits},
                              {"room_fixtures", checked_rooms},
                              {"trunk_save_and_crossing", true}})
                     .dump(2)
              << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
