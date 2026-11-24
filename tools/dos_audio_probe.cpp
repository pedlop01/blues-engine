// SPDX-License-Identifier: GPL-3.0-or-later
#include "../engines/fox_dos/audio.hpp"
#include "../engines/fox_dos/host_game.hpp"
#include "../engines/pre1_dos/audio.hpp"
#include "prepare/game_archives.hpp"
#include <iostream>
extern "C" int FX_START(int);
int main(int argc, char **argv) {
  try {
    if (argc != 3)
      throw std::runtime_error(
          "Usage: dos-audio-probe FOX_PREPARED PRE1_PREPARED");
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER))
      throw std::runtime_error(SDL_GetError());
    struct Quit {
      ~Quit() { SDL_Quit(); }
    } quit;
    titus::Json report = titus::Json::array();
    auto check = [&](auto &audio, const char *game, auto effect) {
      audio.open();
      audio.volume(0, 0);
      audio.muted = false;
      SDL_Delay(120);
      if (audio.nonzero.load())
        throw std::runtime_error("Muted buses emitted samples");
      audio.volume(100, 0);
      SDL_Delay(220);
      auto music = audio.nonzero.load();
      if (music < 1000)
        throw std::runtime_error("Missing music bus");
      audio.volume(0, 100);
      SDL_Delay(180);
      audio.nonzero = 0;
      audio.peak = 0;
      SDL_Delay(80);
      if (audio.nonzero.load())
        throw std::runtime_error("Music leaked into FX-only bus");
      effect();
      SDL_Delay(300);
      auto fx = audio.nonzero.load();
      if (fx < 100)
        throw std::runtime_error("Missing FX bus");
      audio.muted = true;
      SDL_Delay(40);
      audio.volume(100, 100);
      audio.reopen();
      audio.muted = false;
      SDL_Delay(160);
      if (audio.clipped.load())
        throw std::runtime_error("Clipped DOS audio");
      report.push_back({{"game", game},
                        {"silent_mix", true},
                        {"music_only_nonzero", music},
                        {"effects_only_nonzero", fx},
                        {"reopen", true},
                        {"clipped", audio.clipped.load()},
                        {"peak", audio.peak.load()}});
    };
    {
      titus::fox_dos::Assets assets(std::filesystem::u8path(argv[1]));
      titus::fox_dos::Audio audio;
      check(audio, "fox", [&] {
        audio.lock();
        FX_START(1);
        audio.unlock();
      });
    }
    {
      olduvai::prepare::GameArchives files(std::filesystem::u8path(argv[2]));
      auto *music = files.entry("RIK1.MDI");
      auto *fx = files.entry("BONUS.VOC");
      if (!music || !fx)
        throw std::runtime_error("Missing DOS audio resources");
      titus::pre1_dos::Audio audio;
      audio.open();
      audio.song(*music);
      // The checker opens the same device once: reopening preserves the song's
      // sequencer.
      if (audio.device) {
        SDL_CloseAudioDevice(audio.device);
        audio.device = 0;
      }
      auto sample = titus::pre1_dos::dos_sample(*fx);
      check(audio, "prehistorik", [&] { audio.effect(sample); });
    }
    std::cout << report.dump() << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
