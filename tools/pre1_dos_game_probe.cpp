// SPDX-License-Identifier: GPL-3.0-or-later
#include "titus/pre1_dos_render.hpp"
#include "titus/pre1_dos_save.hpp"
#include <iostream>
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      throw std::runtime_error("Usage: pre1-dos-game-probe PREPARED_DOS_PATH");
    titus::pre1_dos::Assets a(std::filesystem::u8path(argv[1]));
    for (unsigned n = 1; n <= 7; ++n) {
      titus::pre1_dos::Game g;
      g.start(a, n);
      for (unsigned t = 0; t < 90; ++t) {
        olduvai::systems::FrameInputs in;
        g.tick(a, in, true, true);
      }
      auto saved = titus::pre1_dos::save_game(g, "probe-dos");
      auto restored = titus::pre1_dos::restore_game(a, saved, "probe-dos");
      if (titus::pre1_dos::save_game(restored, "probe-dos") != saved)
        throw std::runtime_error("State roundtrip mismatch");
      for (unsigned t = 0; t < 30; ++t) {
        g.tick(a, {}, true, true);
        restored.tick(a, {}, true, true);
      }
      if (titus::pre1_dos::save_game(g, "probe-dos") !=
          titus::pre1_dos::save_game(restored, "probe-dos"))
        throw std::runtime_error("State continuation mismatch");
      for (double zoom : {1., .8, .5}) {
        titus::pre1_dos::View view;
        view.zoom = zoom;
        auto f = titus::pre1_dos::render(a, g, view);
        if (f.pixels.size() != f.width * f.height)
          throw std::runtime_error("Frame size");
      }
      std::cout << "level " << n << " ok\n";
    }
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
