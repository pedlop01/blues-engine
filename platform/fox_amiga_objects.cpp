// SPDX-License-Identifier: GPL-3.0-or-later
#include "titus/fox_amiga_objects.hpp"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>
namespace titus::fox_amiga {
namespace {
void check_player(const Body& player){if(player.feet.x<-32768 || player.feet.x>32767 || player.feet.y<-32768 || player.feet.y>32767)throw std::runtime_error("Invalid player position");}
void check(const ObjectWorld& world){
    if(world.carried && *world.carried>=world.objects.size())throw std::runtime_error("Invalid carried object slot");
    for(const auto& o:world.objects){
        if(((o.kind==10 || o.kind==26) && o.height<5) || o.kind>71 || o.vx<-32768 || o.vx>32767 || o.vy<-32768 || o.vy>32767 || o.reserved>65535 || o.fall_limit>65535 || o.counter>65535 || o.held>1 || o.width>4096 || o.height>4096 || o.flags>65535 || o.feet.x<-32768 || o.feet.x>32767 || o.feet.y<-32768 || o.feet.y>32767)
            throw std::runtime_error("Invalid dynamic object state");
    }
    if(world.carried && (!world.objects[*world.carried].kind || !world.objects[*world.carried].held))throw std::runtime_error("Invalid carried object state");
}
void put(Bytes& bytes,int value){unsigned v=unsigned(value)&65535;bytes.push_back(std::uint8_t(v>>8));bytes.push_back(std::uint8_t(v));}
}
ObjectWorld initialize_objects(const Level& level,const ObjectRules& rules){
    ObjectWorld world;
    for(unsigned n=0;n<world.objects.size();++n){
        const auto& source=level.objects[n];auto& o=world.objects[n];o.kind=source.kind;o.feet=source.position;
        if(!o.kind)continue;
        if(o.kind>rules.kinds.size())throw std::runtime_error("Unknown object kind");
        const auto& rule=rules.kinds[o.kind-1];
        if(rule.flags>255 || rule.fall_limit>4080 || !rule.width || rule.width>4096 || !rule.height || rule.height>4096)throw std::runtime_error("Invalid object initialization rule");
        o.flags=rule.flags;o.fall_limit=rule.fall_limit;o.width=rule.width;o.height=rule.height;o.vy=16;
    }
    check(world);return world;
}
bool pick_object(ObjectWorld& world,const Body& player){
    check_player(player);check(world);if(world.carried)throw std::runtime_error("Cannot pick a second object");
    for(unsigned n=0;n<world.objects.size();++n){
        auto& o=world.objects[n];if(!o.kind || std::abs(player.feet.y-o.feet.y)>4)continue;
        const int distance=o.feet.x-player.feet.x;
        if(player.facing_right ? !(distance>0 && distance<int(o.width)) : !(distance<0 && -distance<=int(o.width)))continue;
        o.held=1;world.contact_cooldown=4;
        world.attachment={o.feet.x-player.feet.x,o.feet.y-player.feet.y};
        if(o.kind==27 || o.kind==28)world.special_item=true;
        if(o.kind==25){o.kind=26;o.height+=5;}
        else if(o.kind==41){o.kind=10;o.height+=5;}
        // The original keeps the carried object at the end of the active list.
        const unsigned last=unsigned(world.objects.size()-1);
        std::swap(o,world.objects[last]);world.carried=last;return true;
    }
    return false;
}
void drop_object(const Level& level,ObjectWorld& world,const Body& player){
    check_player(player);check(world);if(!world.carried)return;
    auto& o=world.objects[*world.carried];world.carried.reset();
    o.vx=0;o.held=0;o.vy=16;o.counter=0;world.contact_cooldown=4;
    const int half=int(o.width/2);o.feet.x=std::clamp(o.feet.x,half,4096-half);
    if(collision_at(level,o.feet.x,o.feet.y).horizontal&1)o.feet.x=player.feet.x;
    const auto c=collision_at(level,o.feet.x,o.feet.y);
    if((c.floor&1) || c.ceiling)o.feet.y=player.feet.y;
}
void throw_object(ObjectWorld& world,const Body& player,bool upward){
    check_player(player);check(world);if(!world.carried)return;
    auto& o=world.objects[*world.carried];world.carried.reset();
    world.sound_event=4;o.held=0;o.counter=0;world.contact_cooldown=4;
    if(upward)o.vy=-160;
    else{
        o.vx=player.facing_right?160:-160;
        if(o.flags&4)o.vy=1;
        else {o.vy=0;o.flags|=128;}
        o.feet.y=player.feet.y-16;
    }
}
void attach_object(ObjectWorld& world,Point feet){
    check(world);if(!world.carried)return;
    const long long x=static_cast<long long>(feet.x)+world.attachment.x,y=static_cast<long long>(feet.y)+world.attachment.y;
    if(x<-32768 || x>32767 || y<-32768 || y>32767)throw std::runtime_error("Carried object position overflow");
    world.objects[*world.carried].feet={int(x),int(y)};
}
Bytes dynamic_object_projection(const ObjectWorld& world){
    check(world);Bytes bytes;
    for(const auto& o:world.objects)for(int value:{int(o.kind),o.feet.x,o.feet.y,o.vx,o.vy,int(o.reserved),int(o.fall_limit),int(o.counter),int(o.held),int(o.flags),int(o.width),int(o.height)})put(bytes,value);
    return bytes;
}
}
