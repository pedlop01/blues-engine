// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "prepare/game_archives.hpp"
#include "systems/boss_l2.hpp"
#include "systems/boss_l4.hpp"
#include "systems/boss_l6.hpp"
#include "systems/frame_runner.hpp"
#include "systems/spawning.hpp"
#include "titus/fox_amiga.hpp"
#include "titus/pre1_amiga_level.hpp"
#include <filesystem>
#include <map>
namespace titus::pre1_dos {
using namespace titus::pre1_amiga;
fox_amiga::Picture dos_picture(const Bytes &);
struct DosRules {
  std::array<Bytes, 4> sections;
  std::array<std::string, 4> section_sha256;
  std::array<std::array<unsigned, 16>, 6> palettes{};
};
class Files {
public:
  explicit Files(const std::filesystem::path &root);
  Bytes read(const std::string &name) const;

private:
  std::filesystem::path root_;
  olduvai::prepare::GameArchives archives_;
};
struct LevelAssets {
  unsigned number = 0, element_count = 0;
  Surface surface;
  CollisionRules dur;
  std::vector<Sprite> elements, actors, cave;
  std::array<std::vector<Sprite>, 4> giant;
  fox_amiga::Picture background{};
  std::array<Scene, 19> scenes;
};
struct Assets {
  explicit Assets(const std::filesystem::path &root);
  Files files;
  DosRules rules;
  olduvai::systems::MonsterTables monsters;
  olduvai::prepare::L3CaveLayouts l3_caves;
  ObjectScreens secrets, caves;
  std::vector<Sprite> font;
  std::map<unsigned, LevelAssets> levels;
  LevelAssets &level(unsigned number);
};
struct Game {
  olduvai::systems::SystemsState state;
  olduvai::systems::BossPlayerState boss_player;
  olduvai::systems::L2BossState boss2;
  olduvai::systems::L4BossState boss4;
  olduvai::systems::L6BossState boss6;
  std::map<int, std::vector<olduvai::core::Entity>> store;
  int bound_key = -1;
  int previous_screen = 0;
  std::uint32_t rng = 1;
  std::uint64_t ticks = 0;
  bool ending = false;
  int club_phase = 0;
  std::array<int, 2> boss2_pose{{0, 0}}, spring_pose{{0, 0}};
  unsigned transition_ticks = 0;
  unsigned trunk_ticks = 0;
  bool trunk_overlay = false;
  std::vector<std::array<int, 3>> secret_scatter;
  void start(Assets &, unsigned internal_level = 1, int lives = 3,
             long score = 0, std::uint32_t seed = 1);
  void bind(Assets &);
  void connect(Assets &);
  void tick(Assets &, const olduvai::systems::FrameInputs &,
            bool invulnerable = false, bool infinite_lives = false);
};
Scene trunk_platform(const LevelAssets &, int y_offset = 0);
unsigned next_level(unsigned internal);
// Campaign numbering swaps the forest/ice internal IDs. This is its own
// inverse.
inline unsigned display_level(unsigned n) {
  return n == 3 ? 5 : n == 5 ? 3 : n;
}
Scene room_scene(const Assets &, const LevelAssets &,
                 const olduvai::systems::SystemsState &);
} // namespace titus::pre1_dos
