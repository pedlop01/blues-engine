// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "titus/fox_amiga_objects.hpp"
#include <string>
namespace titus::fox_amiga {
struct SpriteRule { unsigned width=0,height=0;int anchor_x=0,anchor_y=0; };
struct NativeRules {
    LevelRules level;
    MotionRules motion;
    ObjectRules objects;
    std::array<unsigned,16> palette{},level_color14{},player_palette{},hud_palette{};
    std::array<std::array<unsigned,176>,16> copper_color14{};
    std::array<std::string,16> level_songs,level_codes;
    std::array<unsigned,5> effect_frequencies{};
    std::array<SpriteRule,333> sprites{};
    std::array<std::string,4> section_sha256;
    Bytes table_data;
};
NativeRules native_rules(const Bytes& main_file);
}
