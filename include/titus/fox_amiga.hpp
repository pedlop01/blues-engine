// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <optional>
namespace titus::fox_amiga {
using Bytes = std::vector<std::uint8_t>;
// Research asset reader only. Does not expose a playable engine capability.
Bytes unpack(const Bytes& source);
struct Image { unsigned width, height; Bytes pixels; };
std::vector<Image> sprites(const Bytes& raw);
std::vector<Image> tiles(const Bytes& raw);
struct Picture { Image image; std::array<std::uint32_t, 64> palette{}; };
Picture picture(const Bytes& raw, unsigned planes, bool repeat_encoded);
Image map_view(const Bytes& map, const std::vector<Image>& tiles,
               unsigned x, unsigned y, unsigned width, unsigned height);
}

namespace titus::fox_amiga {
struct Point { int x=0,y=0; };
struct Object {
    unsigned slot=0,raw_sprite=0,kind=0;
    Point position;
};
struct Enemy {
    unsigned slot=0,raw_sprite=0,kind=0,behavior=0;
    Point position;
    int source_speed_x=0;
    unsigned initial_power=0;
    std::array<std::uint8_t,26> parameters{};
};
struct Bonus { unsigned tile=0,replacement=0,x=0,y=0;bool active=false; };
struct Gate { unsigned entrance=0,screen=0,exit=0,flags=0;bool active=false; };
struct Elevator {
    unsigned slot=0,raw_sprite=0,kind=0,direction=0,speed=0,range=0;
    Point position;
    std::array<std::uint8_t,20> parameters{};
};
struct Level {
    unsigned number=0,height_tiles=0,gravity_wall_tile=84;
    Bytes map,dirty_rows;
    std::array<std::uint8_t,256> horizontal{},floor{},ceiling{};
    std::array<std::uint8_t,768> animation{};
    std::array<Object,40> objects{};
    std::vector<Enemy> enemies;
    std::array<Bonus,100> bonuses{};
    std::array<Gate,20> gates{};
    std::vector<Elevator> elevators;
    Point spawn,finish;
    unsigned camera_trigger_y=0,camera_lock_y=0;
};
struct LevelRules { std::array<bool,232> enemy_animation_frames{}; };
Level initialize_level(unsigned number,const Bytes& map,const Bytes& poil,const LevelRules&);
struct Collision {
    unsigned tile=0,horizontal=0,floor=0,ceiling=0;
    bool outside=false;
};
Collision collision_at(const Level& level,int x,int y);
// Explicit diagnostic overlays, not the final game's draw/occlusion ordering.
Image level_scene(const Level&,const std::vector<Image>& tiles,
                  const std::vector<Image>& fox,const std::vector<Image>& objects,
                  const std::vector<Image>& enemies,unsigned x,unsigned y,unsigned w,unsigned h);
Image collision_view(const Level&,unsigned x,unsigned y,unsigned w,unsigned h);
// Stable big-endian projections for comparisons with isolated original loaders.
Bytes object_projection(const Level&);
Bytes enemy_projection(const Level&);
Bytes elevator_projection(const Level&);
Bytes gate_projection(const Level&);
struct GateTransition { Point destination,camera;unsigned flags=0; };
std::optional<GateTransition> gate_transition(const Level&,Point feet);
bool exit_ready(const Level&,Point feet,bool blocked,unsigned carried_kind);
unsigned next_level(unsigned number); // 0 means no next regular level.
}
