// SPDX-License-Identifier: GPL-3.0-or-later
#include "titus/fox_amiga_objects.hpp"
#include <algorithm>
#include <stdexcept>
namespace titus::fox_amiga {
namespace {
int shift(int value,unsigned bits){const int unit=1<<bits;return value>=0?value/unit:-((-value+unit-1)/unit);}
Collision sample(const Level& level,int x,int y){
    if(x<0 || x>=4096 || y<0 || y>=int(level.height_tiles*16))return {};
    return collision_at(level,x,y);
}
bool visible(ObjectBody& o,const ObjectRule& rule,const Viewport& view){
    if(!view.width || !view.height || view.width>4096 || view.height>4096)throw std::runtime_error("Invalid object viewport");
    const auto camera=view.camera;
    const int x=o.feet.x-camera.x,y=o.feet.y-camera.y;
    if(x<=-48 || x>=int(view.width)+32 || y<=-48 || y>=int(view.height)+64)return false;
    o.width=rule.width;o.height=rule.height;
    const int ax=rule.anchor_x<0?int(o.width/2):rule.anchor_x,ay=rule.anchor_y<0?int(o.height):rule.anchor_y;
    const int left=x-(o.vx<0?int(o.width)-ax:ax)+(camera.x&15),top=y-ay+1;
    return top<int(view.height) && left<int(view.width)+16 && left+int(o.width)>0 && top+int(o.height)>0;
}
void object_contact(ObjectWorld& world,unsigned index){
    auto& o=world.objects[index];const int foot=o.feet.y+1,left=o.feet.x-int(o.width/2);
    for(unsigned n=0;n<world.objects.size();++n){
        auto& support=world.objects[n];
        if(n==index || !support.kind || !(support.flags&1) || support.held || foot>=support.feet.y || foot<=support.feet.y-int(support.height))continue;
        const int other_left=support.feet.x-int(support.width/2);
        if(left>=other_left+int(support.width) || other_left>=left+int(o.width))continue;
        int top=support.feet.y-int(support.height);o.counter=support.counter=0;
        if(support.kind==10){support.kind=41;top+=5;support.height-=5;}
        else if(support.kind==26){support.kind=25;top+=5;support.height-=5;}
        o.feet.y=top;
        if((o.flags&2) || (support.flags&2)){
            if(!(support.flags&2)){o.vy=std::min(0,-o.vy+48);}
            else{
                const int half=shift(-o.vy,1),own=half+16,other=half+32;
                if(own>=0 || other>=0)o.vy=0;
                else {o.vy=own;support.vy=other;support.vx=shift(o.vx,1);}
            }
        }else{o.vy=support.vy;o.counter=0;}
        return;
    }
}
void carpet(ObjectWorld& world,ObjectBody& o,Body& player,unsigned frame){
    if(o.kind==20){
        if(o.vy>48 && !o.reserved){o.counter=0;o.kind=21;o.flags|=1;o.fall_limit=16;}
        return;
    }
    o.counter=0;
    if(o.kind==21){o.vx=o.vy=0;o.kind=22;return;}
    world.contact_cooldown=4;
    if(o.reserved<4096 && !(o.flags&0x4000)){
        o.kind=22+((frame&7)>>2);++o.reserved;
        if(o.reserved>200){o.kind=20;o.flags&=~1u;o.fall_limit=240;o.vy=16;return;}
        o.vy=o.feet.y==player.feet.y-12?0:o.feet.y>player.feet.y-12?-16:16;
    }else{
        if(o.reserved<4096)o.reserved=4096;
        if(++o.reserved>4596){o.kind=20;o.flags&=~1u;o.fall_limit=240;o.vy=16;return;}
        if(o.flags&0x4000){player.walk_phase=2;player.vx=player.facing_right?80:-80;o.vx=player.facing_right?64:-64;}
    }
}
void player_contact(ObjectWorld& world,ObjectBody& o,Body& player,ObjectStepContext& ctx){
    if(o.flags&1){
        if((o.flags&0x4000) || world.contact_cooldown>1){
            if(o.kind==25){o.kind=26;o.height+=5;if(o.flags&0x4000)world.contact_cooldown=4;}
            else if(o.kind==41){o.kind=10;o.height+=5;if(o.flags&0x4000)world.contact_cooldown=4;}
        }
        const unsigned flags=o.flags;o.flags&=~0x4000u;
        if(player.lock_ticks)return;
        if((o.vy<0 || player.vy>0) && o.feet.y>player.feet.y &&
            o.feet.y-int(o.height)-4<player.feet.y && o.feet.x-int(o.width/2)-4<player.feet.x &&
            o.feet.x+int(o.width/2)+4>player.feet.x){
            if(player.return_state==5 && ctx.down_ticks>42){ctx.down_ticks=0;player.feet.y=o.feet.y;player.vy=0;return;}
            player.feet.y=o.feet.y-int(o.height);player.state=player.return_state;o.flags|=0x4000;
            o.counter=0;player.idle_ticks=0;
            if(o.kind==10){o.kind=41;player.feet.y+=5;o.height-=5;}
            else if(o.kind==26){o.kind=25;player.feet.y+=5;o.height-=5;}
            if(o.kind==54 || o.kind==65)world.contact_cooldown=4;
            if((flags&2) && (player.vy<0 || player.vy>48)){
                int velocity=player.vy;if(velocity>=0)velocity=-velocity;
                velocity+=ctx.down?64:ctx.up?-80:16;
                velocity=std::clamp(velocity,-240,0);player.state=3;player.return_state=0;
                if(velocity){if(o.kind==41)world.sound_event=2;else if(o.kind==25)world.sound_event=1;}
                player.vy=velocity;
                if(!o.vy){world.contact_cooldown=4;o.vy=-velocity;}
            }else player.vy=o.vy;
            return;
        }
    }
    if(o.counter>=10 && o.vy>0 && !ctx.invulnerability && !(o.flags&16)){
        if(o.feet.x+int(o.width/4)>player.feet.x-16 && player.feet.x+16>o.feet.x-int(o.width/4) &&
            player.feet.y-32<o.feet.y && player.feet.y>o.feet.y-int(o.height)){
            player.lock_ticks=0x8064;player.state=player.return_state=3;player.hit_frame=28;player.vx=0;world.sound_event=5;
        }
    }
}
}
void step_objects(const Level& level,ObjectWorld& world,Body& player,ObjectStepContext& ctx,const ObjectRules& rules){
    if(ctx.camera.x<0 || ctx.camera.x>4096 || ctx.camera.y<0 || ctx.camera.y>int(level.height_tiles*16) || ctx.down_ticks>255 || ctx.invulnerability>65535 || player.feet.x<-32768 || player.feet.x>32767 || player.feet.y<-32768 || player.feet.y>32767 || player.vx<-32768 || player.vx>32767 || player.vy<-32768 || player.vy>32767 || player.lock_ticks>65535 || player.return_state>7)throw std::runtime_error("Invalid object step context");
    dynamic_object_projection(world);world.impact_list.clear();
    for(unsigned index=0;index<world.objects.size();++index){
        auto& o=world.objects[index];if(!o.kind)continue;
        if((o.kind==27 || o.kind==28) && !(ctx.frame&7))o.kind=((o.kind-1)^1)+1;
        bool simulate=(o.flags&128)!=0,at_rest=false;
        if(!simulate && world.contact_cooldown && !o.held){
            if(o.kind>=20 && o.kind<=23)carpet(world,o,player,ctx.frame);
            else if((o.flags&0x4000) && (o.kind==54 || o.kind==65) && !o.vy && !player.return_state){
                o.vx=player.facing_right?48:-48;player.vx=player.facing_right?64:-64;
            }
            if(o.vx || o.vy)world.contact_cooldown=4;else at_rest=true;
            simulate=true;
        }
        int x=o.feet.x,y=o.feet.y,vx=o.vx,vy=o.vy;
        if(simulate){
            bool deleted=false;
            if(at_rest)goto ground_test;
            y+=shift(vy,4);
            if(vy<0){
                if(sample(level,x,y).ceiling&1){vy=16;y=(y+16)|15;}
            }else if((o.flags&8) && o.counter>=9){
                vy+=16;if(!vy)vy=1;
                if(vy>0){++o.counter;vy=std::min(vy,int(o.fall_limit));}
                o.feet={x,y};o.vx=vx;o.vy=vy;if(vy>0)object_contact(world,index);
                goto draw;
            }else if(vy){
                if(y>int(level.height_tiles*16)+16){o.kind=0;continue;}
                const auto c=sample(level,x,y);
                bool ground=(c.horizontal&2)!=0;
                if(!ground && (c.floor&1)){
                    ground=true;
                    if(c.floor&64){
                        const unsigned type=c.floor&63;
                        if(type==3 || (type!=7 && o.kind!=10 && o.kind!=41))deleted=true;
                    }
                }
                if(deleted){o.kind=0;continue;}
                if(ground){
                    if(o.flags&2){o.counter=0;vy=-std::abs(vy)+48;}
                    if(!(o.flags&2) || vy>0){
                        vy=0;
                        if(o.kind>=20 && o.kind<=23){o.kind=20;o.reserved=0;o.flags&=~1u;o.fall_limit=240;}
                    }
                    y=(y-16)|15;
                    if(!(o.flags&2) && (o.flags&0x8000))for(auto& smoke:world.smoke)if(!smoke.kind){smoke.kind=17;smoke.feet={x,y};break;}
                }
            }
            x+=shift(vx,4);
            if((o.flags&128) && !(o.flags&4))goto integrate;
            if(x<0 || x>=4096 || (sample(level,x,y).horizontal&1)){
                y=o.feet.y;
                if(vx<0){x=((x+16)&~15)+8;vx=std::max(0,-vx-48);}
                else if(vx){x=((x-16)&~15)+8;vx=std::min(0,-vx+48);}
                else{
                    x=o.feet.x;
                    if(!(sample(level,x-16,y).horizontal&1))x-=16;
                    else if(!(sample(level,x+16,y).horizontal&1))x+=16;
                }
            }
        ground_test:
            if(!vy && (!vx && !(o.flags&0x8000)))goto store;
            if(!vy){
                const auto below=sample(level,x,y+1);
                if((below.horizontal&2) || (below.floor&1)){
                    if(vx>0)vx=std::max(0,vx-4);else if(vx<0)vx=std::min(0,vx+4);
                    goto store;
                }
            }
            vy+=16;if(!vy)vy=1;
            if(vy>0){++o.counter;vy=std::min(vy,int(o.fall_limit));}
        integrate:
        store:
            o.feet={x,y};o.vx=vx;o.vy=vy;
            if(vy>0)object_contact(world,index);
        }
    draw:
        o.flags&=~0x8000u;
        const auto& rule=rules.kinds[o.kind-1];
        if(!rule.width || rule.width>4096 || !rule.height || rule.height>4096 || rule.anchor_x<-1 || rule.anchor_x>4096 || rule.anchor_y<-1 || rule.anchor_y>4096)throw std::runtime_error("Invalid object display rule");
        const bool on_screen=visible(o,rule,ctx.view.value_or(Viewport{ctx.camera,320,176}));
        if(on_screen){o.width=rule.width;o.height=rule.height;o.flags|=0x8000;}
        if(o.held)continue;
        if((o.flags&0x8000) && !(o.flags&16) && (o.vx || (o.counter>8 && o.vy>0)))world.impact_list.push_back(index);
        if((o.flags&128) || ((o.flags&8) && o.counter>10)){
            if(!(o.flags&0x8000)){o.kind=0;continue;}
        }
        if(on_screen)player_contact(world,o,player,ctx);
    }
    if(world.contact_cooldown)--world.contact_cooldown;
    dynamic_object_projection(world);
}
}
