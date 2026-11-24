// SPDX-License-Identifier: GPL-3.0-or-later
#include "titus/fox_amiga.hpp"
#include "titus/fox_amiga_objects.hpp"
#include <algorithm>
#include <stdexcept>
namespace titus::fox_amiga {
namespace {
int floor16(int v){return v>=0?v/16:-((-v+15)/16);}
void unsupported(const char* text){throw std::runtime_error(text);}
}
static Body step_impl(const Level& level,Body b,const BodyInput& input,const MotionRules& rules,ObjectWorld* world,bool action,const MotionContext* context){
    if(context && (context->camera.x<0 || context->camera.x>4096 || context->camera.y<0 || context->camera.y>int(level.height_tiles*16) || b.feet.y<-32768 || b.feet.y>32767))unsupported("Invalid full player context");
    const Body previous=b;
    bool fire=action;
    if(world && world->action_latched){world->action_latched=action;fire=false;}
    auto carrying=[&](){return world && (world->carried.has_value() || (context && context->carrying_enemy));};
    auto object_action=[&](){if(fire && carrying())throw_object(*world,b,input.up);};
    auto pose=[&](int x,int y){if(world)world->attachment={x,y};};
    if((b.state>5 && !(context && b.state==7)) || b.lock_ticks>65535 || b.energy>16 || b.crouch_jump_ticks>100 || b.ice_shift>3 || b.walk_phase>13 || (b.jump_phase>5 && b.jump_phase!=128) || (b.state==2 && b.jump_phase>4) || b.vx<-256 || b.vx>256 || b.vy<-256 || b.vy>256)
        unsupported("Unsupported terrain-only body state");
    if((!context && (b.feet.x<4 || b.feet.x>4092 || b.feet.y<20 || b.feet.y>=int(level.height_tiles*16))) || (context && (b.feet.x<-32768 || b.feet.x>32767 || b.feet.y<-32768 || b.feet.y>32767)))
        unsupported("Body outside terrain probe bounds");
    auto floor=[&](int x,int y){auto c=collision_at(level,x,y);return context && c.outside?0u:c.floor;};
    auto ceiling=[&](int x,int y){auto c=collision_at(level,x,y);return context && c.outside?0u:c.ceiling;};
    auto horizontal=[&](int x,int y){auto c=collision_at(level,x,y);return context && c.outside?0u:c.horizontal;};
    const bool moving=input.left || input.right;
    if(b.crouch_jump_ticks)--b.crouch_jump_ticks;
    // No fractional position accumulator: the original discards |v| low bits
    // on integration, but uses arithmetic shifts when undoing wall movement.
    if(!moving && !b.lock_ticks){const int drag=12>>b.ice_shift;b.vx=b.vx>0?std::max(0,b.vx-drag):std::min(0,b.vx+drag);}
    auto direction=[&](){if(input.right)b.facing_right=true;if(input.left)b.facing_right=false;};
    auto accelerate=[&](int cap=64,int amount=0){if(moving){if(!amount)amount=16>>b.ice_shift;b.vx=b.facing_right?std::min(cap,b.vx+amount):std::max(-cap,b.vx-amount);}};
    auto support=[&](){if(!(floor(b.feet.x+4,b.feet.y+1)|floor(b.feet.x-4,b.feet.y+1))){b.state=3;b.jump_phase=0;if(!b.vy)b.vy=16;}};
    auto descend=[&](){if(input.down){
        if(floor(b.feet.x,b.feet.y+16)==6){b.feet.x=((b.feet.x&~15)|8)-(carrying()?4:0);b.state=b.return_state=4;b.vx=b.vy=0;b.feet.y=(b.feet.y|15)+16;}
        else {b.state=b.return_state=5;b.vx=0;}
    }};
    auto die=[&](){b.energy=0;b.state=b.return_state=7;b.lock_ticks=100;b.vy=-160;b.death_limit=context?context->camera.y+236:0;};
    if(b.state==7){
        b.idle_ticks=0;b.lock_ticks=100;b.frame=29;b.vy=std::min(256,b.vy+16);b.feet.y+=floor16(b.vy);
        if(b.feet.y>b.death_limit){b.feet.y=context->camera.y+240;b.restart_requested=true;}
        return b;
    }
    const int carry_x=context && context->wide_carry?(b.facing_right?-4:4):2;
    if(b.state==0){
        object_action();
        if(++b.idle_ticks>350)b.idle_ticks=0;
        if(b.lock_ticks){
            b.lock_ticks=(b.lock_ticks-1)&65535;
            if(!(b.lock_ticks&32768))b.lock_ticks=0;
            else{
                if(!(b.lock_ticks&32767))b.lock_ticks=0;
                b.walk_phase=b.walk_phase>=2?0:b.walk_phase+1;b.frame=24+b.walk_phase;
                goto integrate_body;
            }
        }
        b.frame=b.vx?2:(b.idle_ticks>250?1:(b.crouch_jump_ticks?15:0));
        if(carrying()){b.frame=6;pose(carry_x,-31);}
        if(moving)b.state=b.return_state=1;
        if(input.up && !(ceiling(b.feet.x,b.feet.y-32)&1)){b.state=b.return_state=2;b.jump_phase=0;}
        support();descend();
    }else if(b.state==1){
        b.idle_ticks=0;object_action();
        b.walk_phase=(b.walk_phase+1)%14;b.frame=rules.walk_frames[b.walk_phase];
        if(b.frame>=30)unsupported("Invalid walking frame");
        if(carrying()){b.frame+=4;pose(carry_x,-31);}
        else if(b.crouch_jump_ticks)b.frame=14+(b.walk_phase&1);
        direction();accelerate();b.state=b.return_state=moving?1:0;
        if(input.up && !(ceiling(b.feet.x,b.feet.y-32)&1)){b.state=b.return_state=2;b.jump_phase=0;}
        support();descend();
    }else if(b.state==2){
        b.idle_ticks=0;object_action();b.frame=14;
        if(carrying()){b.frame=16;pose(0,-31);}
        direction();accelerate();
        unsigned next_state=input.up?2:3;
        if(input.up){
            const int impulse=rules.jump_impulses[b.jump_phase];
            if(impulse>0 || impulse<-256)unsupported("Invalid jump rule");
            b.vy+=impulse;
        }
        b.vy=std::max(-144,b.vy);
        if(++b.jump_phase==5)next_state=3;
        b.state=next_state;b.return_state=0;
    }else if(b.state==3){
        b.idle_ticks=0;b.return_state=0;
        if(b.lock_ticks){
            b.lock_ticks=(b.lock_ticks-1)&65535;const unsigned remaining=b.lock_ticks&32767;if(!remaining)b.lock_ticks=0;
            if(carrying())drop_object(level,*world,previous);
            b.frame=remaining>50?b.hit_frame:23;b.vy=std::min(128,b.vy+16);goto integrate_body;
        }
        object_action();b.frame=23;
        if(carrying()){b.frame=17;pose(context && context->wide_carry?(b.facing_right?-4:4):0,-31);}
        direction();accelerate();b.vy=std::min(128,b.vy+16);
    }else if(b.state==4){
        b.idle_ticks=0;b.crouch_jump_ticks=0;
        if(b.walk_phase>3)b.walk_phase=0;
        b.frame=12+(b.walk_phase>>1);
        if(carrying()){b.frame-=2;pose(11,-31);}
        direction();object_action();accelerate(32,8);
        b.vy=0;b.facing_right=false;
        if(input.up){++b.walk_phase;b.vy=-64;}
        else if(input.down){++b.walk_phase;b.vy=64;}
        if(floor(b.feet.x,b.feet.y+floor16(b.vy)-16)!=6 && b.vy<0){b.vy=-96;b.state=3;b.jump_phase=128;}
        if(floor(b.feet.x,b.feet.y-16)!=6 && floor(b.feet.x,b.feet.y)!=6){b.state=3;b.jump_phase=0;}
    }else{
        b.idle_ticks=0;b.frame=19;const bool carried_on_entry=carrying();
        if(carried_on_entry){b.frame=22;pose(b.facing_right?11:-11,-8);}
        direction();b.state=b.return_state=input.down?5:0;
        if(moving && !carried_on_entry){b.walk_phase=(b.walk_phase+1)%4;b.frame=20+(b.walk_phase>>1);accelerate(32);b.state=b.return_state=5;}
        if(input.up && !(ceiling(b.feet.x,b.feet.y-28)&1)){b.state=2;b.jump_phase=0;b.crouch_jump_ticks=100;}
        support();
        if(input.down && floor(b.feet.x,b.feet.y+16)==6)descend();
        if(world && fire && !world->action_latched){
            world->action_latched=true;
            if(carrying())drop_object(level,*world,previous);else pick_object(*world,previous);
        }
    }
integrate_body:
    b.feet.x=std::clamp(b.feet.x+(b.vx<0?-((-b.vx)/16):b.vx/16),4,4092);
    b.feet.y+=b.vy<0?-((-b.vy)/16):b.vy/16;
    if(b.feet.y<0){b.feet.y=16;b.vy=0;}
    if(context && b.feet.y>int(level.height_tiles*16)+16){
        b.feet.y=int(level.height_tiles*16)+16;const int velocity=b.vy;die();b.vy=velocity;
    }
    if(!context && (b.feet.y<20 || b.feet.y>=int(level.height_tiles*16)))unsupported("Terrain probe reached map boundary");
    // Leading wall check at the previous vertical position (original 0x698a).
    const int old_y=b.feet.y-floor16(b.vy),lead=b.feet.x+(b.vx>0?8:b.vx<0?-8:0);
    if((horizontal(lead,old_y)&1) && (horizontal(lead,old_y-16)&1)){
        b.feet.x-=floor16(b.vx);b.vx=0;
    }
    // Ground, head and ladder tests (0x680a), keeping the original ordering.
    int offset=b.feet.x+(b.lock_ticks?0:b.vx>0?3:b.vx<0?-3:0);
    if(b.vy>0 && ((floor(offset,b.feet.y)|floor(b.feet.x,b.feet.y))&(b.state==4?1:255))){
        b.feet.y=(b.feet.y-16)|15;b.vy=0;b.state=0;b.feet.x=offset;
    }
    offset=b.feet.x;
    if(ceiling(offset,b.feet.y-20)&1){
        if(b.state<=1){b.state=5;if(carrying())drop_object(level,*world,previous);}
        else if(b.state!=5){if(b.vy<0)b.vy=0;b.state=3;b.jump_phase=0;}
    }
    if(b.state==3 && !b.lock_ticks && b.jump_phase!=128 && ((ceiling(offset,b.feet.y)|ceiling(offset,b.feet.y-16))&2)){
        b.state=4;b.feet.x=(offset&~15)|8;b.vx=b.vy=0;
    }
    auto wall=[&](){return horizontal(b.feet.x-4,b.feet.y)|horizontal(b.feet.x+4,b.feet.y);};
    if(wall()&1){
        b.feet.x-=floor16(b.vx);b.vx=0;
        if(wall()&1){
            if(!(horizontal((b.feet.x&~15)-1,b.feet.y)&1))b.feet.x-=2;
            else if(!(horizontal((b.feet.x|15)+1,b.feet.y)&1))b.feet.x+=2;
        }
    }
    if(carrying())attach_object(*world,b.feet);
    const unsigned under=floor(b.feet.x,b.feet.y+1);
    if(under&128)b.ice_shift=under==0x81?1:under==0x83?2:3;
    else{
        b.ice_shift=0;
        const int head=b.feet.y-(b.state==5?16:32);
        if(under&64 || (head>=0 && (floor(b.feet.x,head)&64))){
            if(!context)unsupported("Damage is outside the terrain-only probe");
            if(!context->invulnerable)die();
        }
    }
    return b;
}
Body terrain_step(const Level& level,Body b,const BodyInput& input,const MotionRules& rules){return step_impl(level,b,input,rules,nullptr,false,nullptr);}
Body object_player_step(const Level& level,Body b,const BodyInput& input,const MotionRules& rules,ObjectWorld& world,bool action,const MotionContext& context){
    dynamic_object_projection(world);return step_impl(level,b,input,rules,&world,action,&context);
}
Bytes body_projection(const Body& b){
    Bytes out;
    for(int value:{b.feet.x,b.feet.y,b.vx,b.vy,int(b.state),int(b.frame),int(b.walk_phase),int(b.jump_phase),int(b.idle_ticks),int(b.ice_shift),int(b.facing_right),int(b.crouch_jump_ticks)}){
        const unsigned n=unsigned(value)&65535;out.push_back(std::uint8_t(n>>8));out.push_back(std::uint8_t(n));
    }
    return out;
}
}
