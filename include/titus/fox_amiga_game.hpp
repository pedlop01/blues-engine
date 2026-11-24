// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "titus/fox_amiga_enemies.hpp"
#include "titus/fox_amiga_platforms.hpp"
#include "titus/fox_amiga_audio.hpp"
#include <functional>
namespace titus::fox_amiga {
using ReadResource=std::function<Bytes(const std::string&)>;
struct LevelAssets { Level initial;std::vector<Image> bricks; };
struct GameAssets {
    NativeRules rules;
    std::vector<Image> fox,objects,enemies,font,kiss,ending;
    std::array<std::optional<LevelAssets>,17> levels;
    std::array<std::shared_ptr<const EffectSample>,5> effects;
};
GameAssets load_game_assets(const ReadResource&);
LevelAssets load_level_assets(unsigned,const ReadResource&,const NativeRules&);
enum class GamePhase { playing,level_complete,game_over,ending };
struct GameState {
    Level level;Body player;ObjectWorld objects;EnemyWorld enemies;
    std::vector<PlatformBody> platforms;
    EnemyStepContext actors;
    Point camera,checkpoint;
    unsigned frame=0,lives=3,gate_cooldown=0,bonus_delay=0,down_ticks=0;
    bool invulnerable=false,infinite_lives=false;
    GamePhase phase=GamePhase::playing;
};
GameState start_game(const LevelAssets&,const NativeRules&,unsigned lives=3);
void game_step(GameState&,const BodyInput&,bool action,const NativeRules&);
// RGB software frame: presentation can interpolate the camera independently of
// the 50 Hz simulation, and can render more map area when zooming out.
struct RgbFrame { unsigned width=0,height=0;std::vector<std::uint32_t> pixels; };
RgbFrame game_frame(const GameState&,const GameAssets&,Point camera,unsigned width=320,unsigned height=176);
}
