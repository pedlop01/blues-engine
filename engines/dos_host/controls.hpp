// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "titus/catalog.hpp"
#include <SDL.h>
#include <algorithm>
#include <array>
namespace titus::dos_host {
struct Keyboard {
  std::array<SDL_Scancode, 6> codes = {SDL_SCANCODE_LEFT,  SDL_SCANCODE_RIGHT,
                                       SDL_SCANCODE_UP,    SDL_SCANCODE_DOWN,
                                       SDL_SCANCODE_SPACE, SDL_SCANCODE_R};
  static bool reserved(SDL_Scancode k) {
    return k == SDL_SCANCODE_UNKNOWN || k == SDL_SCANCODE_ESCAPE ||
           k == SDL_SCANCODE_P || k == SDL_SCANCODE_F11 ||
           k == SDL_SCANCODE_EQUALS || k == SDL_SCANCODE_MINUS;
  }
  void read(const titus::Json &j) {
    if (!j.is_array() || j.size() != 6)
      throw std::runtime_error("Invalid keyboard preferences");
    auto next = codes;
    for (unsigned i = 0; i < 6; ++i) {
      if (!j[i].is_number_integer())
        throw std::runtime_error("Invalid keyboard binding");
      int k = j[i];
      if (k < 1 || k >= SDL_NUM_SCANCODES || reserved(SDL_Scancode(k)))
        throw std::runtime_error("Reserved keyboard binding");
      next[i] = SDL_Scancode(k);
    }
    auto sorted = next;
    std::sort(sorted.begin(), sorted.end());
    if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end())
      throw std::runtime_error("Duplicate keyboard binding");
    codes = next;
  }
  bool bind(unsigned i, SDL_Scancode k) {
    if (i >= codes.size() || reserved(k))
      return false;
    auto it = std::find(codes.begin(), codes.end(), k);
    if (it != codes.end())
      *it = codes[i];
    codes[i] = k;
    return true;
  }
  titus::Json json() const {
    titus::Json j = titus::Json::array();
    for (auto c : codes)
      j.push_back(int(c));
    return j;
  }
  std::string label(unsigned i) const {
    static const char *names[] = {"IZQUIERDA", "DERECHA", "SUBIR/SALTAR",
                                  "BAJAR",     "ACCION",  "REBOBINAR"};
    return std::string(names[i]) + "  " + SDL_GetScancodeName(codes[i]);
  }
};
} // namespace titus::dos_host
