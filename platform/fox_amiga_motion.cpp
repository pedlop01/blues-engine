// SPDX-License-Identifier: GPL-3.0-or-later
#include "titus/fox_amiga.hpp"
#include <algorithm>
#include <stdexcept>
namespace titus::fox_amiga {
namespace {
int floor16(int v){return v>=0?v/16:-((-v+15)/16);}
void unsupported(const char* text){throw std::runtime_error(text);}
}
Body terrain_step(const Level& level,Body b,const BodyInput& input,const MotionRules& rules){
    if(b.state>3 || b.ice_shift>3 || b.walk_phase>13 || (b.jump_phase>5 || (b.state==2 && b.jump_phase>4)) || b.vx<-256 || b.vx>256 || b.vy<-256 || b.vy>256)
        unsupported("Unsupported terrain-only body state");
    if(b.feet.x<4 || b.feet.x>4092 || b.feet.y<20 || b.feet.y>=int(level.height_tiles*16))
        unsupported("Body outside terrain probe bounds");
    auto floor=[&](int x,int y){return collision_at(level,x,y).floor;};
    auto ceiling=[&](int x,int y){return collision_at(level,x,y).ceiling;};
    auto horizontal=[&](int x,int y){return collision_at(level,x,y).horizontal;};
    const bool moving=input.left || input.right;
    // No fractional position accumulator: the original discards |v| low bits
    // on integration, but uses arithmetic shifts when undoing wall movement.
    if(!moving){const int drag=12>>b.ice_shift;b.vx=b.vx>0?std::max(0,b.vx-drag):std::min(0,b.vx+drag);}
    auto direction=[&](){if(input.right)b.facing_right=true;if(input.left)b.facing_right=false;};
    auto accelerate=[&](){if(moving){const int amount=16>>b.ice_shift;b.vx=b.facing_right?std::min(64,b.vx+amount):std::max(-64,b.vx-amount);}};
    auto support=[&](){if(!(floor(b.feet.x+4,b.feet.y+1)|floor(b.feet.x-4,b.feet.y+1))){b.state=3;b.jump_phase=0;if(!b.vy)b.vy=16;}};
    if(b.state==0){
        if(++b.idle_ticks>350)b.idle_ticks=0;
        b.frame=b.vx?2:(b.idle_ticks>250?1:0);
        if(moving)b.state=1;
        if(input.up && !(ceiling(b.feet.x,b.feet.y-32)&1)){b.state=2;b.jump_phase=0;}
        support();
    }else if(b.state==1){
        b.idle_ticks=0;
        b.walk_phase=(b.walk_phase+1)%14;b.frame=rules.walk_frames[b.walk_phase];
        if(b.frame>=30)unsupported("Invalid walking frame");
        direction();accelerate();b.state=moving?1:0;
        if(input.up && !(ceiling(b.feet.x,b.feet.y-32)&1)){b.state=2;b.jump_phase=0;}
        support();
    }else if(b.state==2){
        b.idle_ticks=0;b.frame=14;direction();accelerate();
        unsigned next_state=input.up?2:3;
        if(input.up){
            const int impulse=rules.jump_impulses[b.jump_phase];
            if(impulse>0 || impulse<-256)unsupported("Invalid jump rule");
            b.vy+=impulse;
        }
        b.vy=std::max(-144,b.vy);
        if(++b.jump_phase==5)next_state=3;
        b.state=next_state;
    }else{
        b.idle_ticks=0;b.frame=23;direction();accelerate();b.vy=std::min(128,b.vy+16);
    }
    b.feet.x=std::clamp(b.feet.x+(b.vx<0?-((-b.vx)/16):b.vx/16),4,4092);
    b.feet.y+=b.vy<0?-((-b.vy)/16):b.vy/16;
    if(b.feet.y<0){b.feet.y=16;b.vy=0;}
    if(b.feet.y<20 || b.feet.y>=int(level.height_tiles*16))unsupported("Terrain probe reached map boundary");
    // Leading wall check at the previous vertical position (original 0x698a).
    const int old_y=b.feet.y-floor16(b.vy),lead=b.feet.x+(b.vx>0?8:b.vx<0?-8:0);
    if((horizontal(lead,old_y)&1) && (horizontal(lead,old_y-16)&1)){
        b.feet.x-=floor16(b.vx);b.vx=0;
    }
    // Ground, head and ladder tests (0x680a), keeping the original ordering.
    int offset=b.feet.x+(b.vx>0?3:b.vx<0?-3:0);
    if(b.vy>0 && (floor(offset,b.feet.y)|floor(b.feet.x,b.feet.y))){
        b.feet.y=(b.feet.y-16)|15;b.vy=0;b.state=0;b.feet.x=offset;
    }
    offset=b.feet.x;
    if(ceiling(offset,b.feet.y-20)&1){
        if(b.state<=1)unsupported("Crouching is outside the terrain-only probe");
        if(b.vy<0)b.vy=0;
        b.state=3;b.jump_phase=0;
    }
    if(b.state==3 && ((ceiling(offset,b.feet.y)|ceiling(offset,b.feet.y-16))&2))
        unsupported("Ladder interaction is outside the terrain-only probe");
    auto wall=[&](){return horizontal(b.feet.x-4,b.feet.y)|horizontal(b.feet.x+4,b.feet.y);};
    if(wall()&1){
        b.feet.x-=floor16(b.vx);b.vx=0;
        if(wall()&1){
            if(!(horizontal((b.feet.x&~15)-1,b.feet.y)&1))b.feet.x-=2;
            else if(!(horizontal((b.feet.x|15)+1,b.feet.y)&1))b.feet.x+=2;
        }
    }
    const unsigned under=floor(b.feet.x,b.feet.y+1);
    if(under&128)b.ice_shift=under==0x81?1:under==0x83?2:3;
    else{
        b.ice_shift=0;
        if(under&64 || floor(b.feet.x,b.feet.y-16)&64)
            unsupported("Damage is outside the terrain-only probe");
    }
    return b;
}
Bytes body_projection(const Body& b){
    Bytes out;
    for(int value:{b.feet.x,b.feet.y,b.vx,b.vy,int(b.state),int(b.frame),int(b.walk_phase),int(b.jump_phase),int(b.idle_ticks),int(b.ice_shift),int(b.facing_right)}){
        const unsigned n=unsigned(value)&65535;out.push_back(std::uint8_t(n>>8));out.push_back(std::uint8_t(n));
    }
    return out;
}
}
