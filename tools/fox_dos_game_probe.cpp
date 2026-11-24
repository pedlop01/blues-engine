// SPDX-License-Identifier: GPL-3.0-or-later
#include "../engines/fox_dos/host_game.hpp"
#include <iostream>
int main(int argc, char **argv) {
  try {
    if (argc != 2 && argc != 3)
      throw std::runtime_error("Usage: fox-dos-game-probe PREPARED_PATH");
    if (SDL_Init(SDL_INIT_TIMER))
      throw std::runtime_error(SDL_GetError());
    struct Quit {
      ~Quit() { SDL_Quit(); }
    } quit;
    titus::fox_dos::Assets assets(std::filesystem::u8path(argv[1]));
    for (unsigned n = 0; n < 3; ++n) {
      auto pic = titus::fox_dos::picture(n);
      if (pic.pixels.size() != 64000)
        throw std::runtime_error("Intro frame size");
    }
    for (unsigned n = 1; n <= 15; ++n) {
      titus::fox_dos::load(n);
      for (unsigned t = 0; t < 30; ++t)
        fox_dos_step(t < 15 ? 2 : 0, 1, 1);
      auto s = titus::fox_dos::snapshot();
      auto doc = titus::fox_dos::save(s, "probe");
      auto read = titus::fox_dos::decode(doc, "probe");
      for (unsigned t = 0; t < 20; ++t)
        fox_dos_step(0, 1, 1);
      auto expected = titus::fox_dos::snapshot();
      titus::fox_dos::restore(read);
      for (unsigned t = 0; t < 20; ++t)
        fox_dos_step(0, 1, 1);
      if (titus::fox_dos::snapshot().values != expected.values)
        throw std::runtime_error("Fox continuation mismatch");
      if (argc == 3)
        for (unsigned t = 0; t < 1800; ++t) {
          unsigned input = (t % 1000 < 500 ? 2u : 1u) |
                           (t % 80 < 12 ? 4u : 0u) | (t % 130 < 15 ? 16u : 0u);
          int result = fox_dos_step(input, 1, 1);
          if (result == 1 || result == 2)
            break;
          if (t % 100 == 0) {
            auto copy = titus::fox_dos::snapshot();
            titus::fox_dos::restore(copy);
          }
        }
      auto f = titus::fox_dos::render({}, {});
      std::cout << "level " << n << " ok " << s.values.size() << '\n';
    }
    // The nonblocking original ending remains saveable through the approach,
    // smoke, hearts and THE END stages.
    titus::fox_dos::load(15);
    int result = 0;
    for (unsigned t = 0; t < 200; ++t) {
      result = fox_dos_step(t > 110 ? 16 : 0, 1, 1);
      if (t == 95 || t == 130) {
        auto s = titus::fox_dos::snapshot();
        titus::fox_dos::restore(s);
      }
    }
    if (result != 4)
      throw std::runtime_error("Original ending did not reach THE END");
    // Level completion awards the original ten-bonus extra lives exactly once.
    titus::fox_dos::Game game;
    game.start(assets, 1, 2, 23);
    auto finish = [](void *, const char *name, long long v) {
      return std::string(name) == "globals.NEWLEVEL_FLAG" ? 1LL : v;
    };
    if (!fox_dos_state(finish, nullptr, 1))
      throw std::runtime_error("Completion fixture state");
    game.saved = titus::fox_dos::snapshot();
    game.tick(assets, {}, true, true);
    if (!game.level_complete || game.state.current_level != 1)
      throw std::runtime_error("Missing level-complete pause");
    auto doc = titus::fox_dos::save_game(game, "probe");
    auto restored = titus::fox_dos::restore_game(assets, doc, "probe");
    if (!restored.level_complete)
      throw std::runtime_error("Lost completion state");
    restored.advance(assets);
    if (restored.state.current_level != 2 || restored.state.player.lives != 4 ||
        restored.state.score != 3)
      throw std::runtime_error("Bonus extra-life accounting");
    std::cout
        << "original intro, ending, completion state and bonus accounting ok\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
