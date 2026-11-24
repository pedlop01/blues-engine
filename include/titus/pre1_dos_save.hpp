// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "titus/catalog.hpp"
#include "titus/pre1_dos_game.hpp"
namespace olduvai::core {
void to_json(titus::Json &, const CollisionBitmap &);
void from_json(const titus::Json &, CollisionBitmap &);
void to_json(titus::Json &, const Entity &);
void from_json(const titus::Json &, Entity &);
} // namespace olduvai::core
namespace olduvai::systems {
#define TITUS_AMIGA_JSON(Type)                                                 \
  void to_json(titus::Json &, const Type &);                                   \
  void from_json(const titus::Json &, Type &);
TITUS_AMIGA_JSON(ScoreBonus)
TITUS_AMIGA_JSON(PlayerState)
TITUS_AMIGA_JSON(InputState)
TITUS_AMIGA_JSON(SystemsState)
TITUS_AMIGA_JSON(BossPlayerState)
TITUS_AMIGA_JSON(L2ProjectileSlot)
TITUS_AMIGA_JSON(L2BossState)
TITUS_AMIGA_JSON(L4BossState)
TITUS_AMIGA_JSON(L6BossState)
#undef TITUS_AMIGA_JSON
} // namespace olduvai::systems
namespace titus::pre1_dos {
Json save_game(const Game &, const std::string &fingerprint);
Game restore_game(Assets &, const Json &, const std::string &fingerprint);
} // namespace titus::pre1_dos
