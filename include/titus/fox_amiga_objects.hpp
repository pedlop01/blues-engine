// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "titus/fox_amiga.hpp"
namespace titus::fox_amiga {
struct ObjectRule { unsigned flags=0,fall_limit=0,width=0,height=0;int anchor_x=-1,anchor_y=-1; };
struct ObjectRules { std::array<ObjectRule,71> kinds{}; };
struct ObjectBody {
    unsigned kind=0;
    Point feet;
    int vx=0,vy=0;
    unsigned reserved=0,fall_limit=0,counter=0,held=0,flags=0,width=0,height=0;
};
struct ObjectWorld {
    std::array<ObjectBody,40> objects{};
    std::optional<unsigned> carried;
    Point attachment;
    bool special_item=false,action_latched=false;
    unsigned contact_cooldown=0,sound_event=0;
    std::array<ObjectBody,4> smoke{};
    std::vector<unsigned> impact_list;
};
ObjectWorld initialize_objects(const Level&,const ObjectRules&);
// Manipulation routines operate on objects only; carrying enemies is separate.
// Autonomous motion, support and impacts are implemented by step_objects.
bool pick_object(ObjectWorld&,const Body&);
void drop_object(const Level&,ObjectWorld&,const Body&);
void throw_object(ObjectWorld&,const Body&,bool upward);
void attach_object(ObjectWorld&,Point player_feet);
Body object_player_step(const Level&,Body,const BodyInput&,const MotionRules&,ObjectWorld&,bool action,const MotionContext& context={});
struct ObjectStepContext { unsigned frame=0,down_ticks=0,invulnerability=0;Point camera;bool up=false,down=false; };
void step_objects(const Level&,ObjectWorld&,Body&,ObjectStepContext&,const ObjectRules&);
Bytes dynamic_object_projection(const ObjectWorld&);
}
