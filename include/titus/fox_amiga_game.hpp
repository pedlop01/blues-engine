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
    Picture logo,title,presentation;
    std::vector<Image> fox,objects,enemies,font,hud,kiss,ending;
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
    unsigned frame=0,lives=3,bonus_credits=0,completion_ticks=0,completion_spent=0,gate_cooldown=0,gate_flags=0,bonus_delay=0,down_ticks=0,message_level=0,view_step=0;
    bool invulnerable=false,infinite_lives=false;
    GamePhase phase=GamePhase::playing;
};
GameState start_game(const LevelAssets&,const NativeRules&,unsigned lives=3);
Bytes save_game(const GameState&,const std::string& fingerprint);
unsigned saved_game_level(const Bytes&);
GameState restore_game(const Bytes&,const LevelAssets&,const NativeRules&,const std::string& fingerprint);
void finish_level_bonuses(GameState&);
void game_step(GameState&,const BodyInput&,bool action,const NativeRules&);
// RGB software frame: presentation can interpolate the camera independently of
// the 50 Hz simulation, and can render more map area when zooming out.
struct RgbFrame { unsigned width=0,height=0;std::vector<std::uint32_t> pixels; };
unsigned animated_tile(const Level&,unsigned tile,unsigned frame);
RgbFrame smooth_game_frame(const GameState&,const GameState& previous,const GameAssets&,double camera_x,double camera_y,double scale,unsigned width,unsigned height,double alpha);
RgbFrame ending_frame(const GameState&,const GameAssets&,unsigned tick);
RgbFrame game_frame(const GameState&,const GameAssets&,Point camera,unsigned width=320,unsigned height=176);
}
