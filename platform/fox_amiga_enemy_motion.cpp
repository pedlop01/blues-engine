// SPDX-License-Identifier: GPL-3.0-or-later
#include "titus/fox_amiga_enemies.hpp"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>
namespace titus::fox_amiga {
namespace {
int signed_word(unsigned n){return n<32768?int(n):int(n)-65536;}
int shift4(int n){return n>=0?n/16:-((-n+15)/16);}
unsigned table(const NativeRules& r,std::uint32_t address){
    if(address<0xb0000 || address-0xb0000+2>r.table_data.size() || (address&1))throw std::runtime_error("Enemy animation outside original table");
    unsigned p=address-0xb0000;return unsigned(r.table_data[p])*256+r.table_data[p+1];
}
void sequence(EnemyBody& e,const NativeRules& r,bool next){
    auto p=e.animation();
    for(unsigned n=0;n<r.table_data.size()/2;++n){
        if(!next)p-=2;
        int value=signed_word(table(r,p));
        if(next)p+=2;
        if(value<0){if(!next)p-=2;e.animation(p);e.set(18,e.get(18)|0x8000);return;}
    }
    throw std::runtime_error("Unterminated enemy animation sequence");
}
void animate(EnemyBody& e,const NativeRules& r){
    auto p=e.animation();int value=signed_word(table(r,p));p+=2;
    if(value<0){p-=unsigned(-value)+2;value=int(table(r,p));p+=2;}
    e.set(0,value+1);e.animation(p);
}
Collision sample(const Level& level,int x,int y){if(x<0 || x>=4096 || y<0 || y>=int(level.height_tiles*16))return {};return collision_at(level,x,y);}
void hurt(Body& body,EnemyStepContext& ctx,int x,unsigned force,unsigned frame){
    ctx.invulnerability=100;ctx.sound_event=5;body.lock_ticks=200;body.state=body.return_state=3;
    body.jump_phase=0;body.vy=-128;body.vx=x>body.feet.x?-int(force):int(force);body.hit_frame=frame;
    for(unsigned i=0;i<2;++i){if(body.energy)--body.energy;else if(body.state!=7){body.state=body.return_state=7;body.lock_ticks=100;body.vy=-160;body.death_limit=ctx.camera.y+220;}}
}
bool player_overlap(const Body& player,int x,int y,unsigned width,unsigned height){
    return x+int(width)>player.feet.x-16 && x-int(width)<player.feet.x+16 &&
        y>player.feet.y-(player.state==5?16:28) && y-int(height)<player.feet.y;
}
struct Actor {
    EnemyBody& e;const Level& level;const NativeRules& rules;Body& player;EnemyStepContext& ctx;
    int x,y;unsigned index;
    int w(unsigned p)const{return e.signed_get(p);}unsigned u(unsigned p)const{return e.get(p);}
    void set(unsigned p,int v){e.set(p,v);}void next(){sequence(e,rules,true);}void prev(){sequence(e,rules,false);}
    int dx()const{return std::abs(x-player.feet.x);}int dy()const{return std::abs(y-player.feet.y);}
    bool visible()const{return u(18)&0x8000;}
    void face(){set(6,x<player.feet.x?-int(u(60)&255):int(u(60)&255));}
    void turn(unsigned center=50){set(6,x<w(center)?-std::abs(w(6)):std::abs(w(6)));}
    void walk(){x-=w(6);if(std::abs(x-w(32))>w(34))set(6,x>w(32)?std::abs(w(6)):-std::abs(w(6)));}
    void move_ground(bool gravity,bool turn_at_edge){
        if(!sample(level,x,y+15).floor){
            if(gravity){set(8,std::min(8,w(8)+1));y+=w(8);return;}
            turn();
        }else if(gravity){if(w(8))face();y|=15;set(8,0);}
        if(sample(level,x-(w(6)<0?-16:16),y).horizontal&1){if(turn_at_edge)turn();else set(6,-w(6));}
        x-=w(6);if(x<0){set(6,-w(6));x-=w(6);}
    }
    bool reset_far(int horizontal,int vertical)const{return std::abs(ctx.camera.x+160-w(50))>=horizontal || std::abs(ctx.camera.y+86-w(52))>vertical;}
    void reset(unsigned sequences=1){x=w(50);y=w(52);set(30,0);while(sequences--)prev();}
    bool shoot(bool drop=false){
        auto found=std::find_if(ctx.projectiles.begin(),ctx.projectiles.end(),[](const ObjectBody& o){return !o.kind;});
        if(found==ctx.projectiles.end())return false;
        *found={};found->feet={x,y};
        if(drop){next();found->kind=(table(rules,e.animation())&8191)-29;prev();found->vy=1;}
        else{auto p=e.animation();found->kind=(table(rules,p-4)&255)+1;found->feet.y-=int(table(rules,p-2)&255);found->vx=player.feet.x<=x?11:-11;ctx.sound_event=4;}
        if(!found->kind || found->kind>71)throw std::runtime_error("Invalid enemy projectile frame");
        return true;
    }
    int decrement_byte(unsigned p){unsigned v=(u(p)-1)&255;set(p,(u(p)&0xff00)|v);return v<128?int(v):int(v)-256;}
    void death(){
        if(!(u(24)&128)){set(8,-160);unsigned frame=u(90)>>8;if(!frame)set(24,u(24)|128);else{set(0,frame+1);if(!visible())set(0,0);}return;}
        if(w(6)){x+=shift4(w(6));ctx.attacks.push_back(index);set(18,u(18)|128);if(!visible())set(0,0);}
        else{y+=shift4(w(8));if(shift4(w(8))<16)set(8,w(8)+16);else if(!visible())set(0,0);}
    }
    void update(unsigned type){
        if(type!=12 && type!=14 && type!=15 && type!=17 && (u(24)&192)){death();return;}
        unsigned phase=u(30);
        switch(type){
        case 0:case 1:walk();break;
        case 2:{
            if(!visible())break;
            unsigned direction=(u(40)>>8)&192;
            set(6,direction?(direction==64?0:-1):(player.feet.x>x?-1:0));
            if(!phase){if(decrement_byte(36)>=0 || dy()>=24 || (w(6)<0?player.feet.x<=x:player.feet.x>=x) || dx()>=int(u(40)&16383))break;set(30,30);next();}
            else{set(30,phase-1);if(u(0)&8192){e.animation(e.animation()+4);if(shoot())set(36,w(38));set(30,0);}}break;
        }
        case 3:case 4:
            if(!phase){walk();int height=y-player.feet.y;if(!visible() || height<0 || height>=w(42) || dx()>=48 || (w(6)>=0?player.feet.x>=x:player.feet.x<=x) || std::abs(player.feet.x-w(32))>=w(34))break;
                int n=0,sum=0;do{sum+=++n;}while(sum<height);set(44,-n);set(8,0);set(48,y);set(30,1);next();}
            else if(phase==1){if(!visible())break;x-=w(6)*4;y+=w(44);set(44,w(44)+1);if(!w(44) || y<=w(48)-w(42)){next();set(30,2);set(44,0);turn(32);}}
            else{x-=w(6);y+=w(44);set(44,w(44)+1);if(y>w(48)){y=w(48);x-=w(6);set(30,0);prev();prev();}}break;
        case 5:case 6:
            walk();if(!visible())break;
            if(!phase){if(dy()>w(42) || dy()>40)break;set(48,y);set(44,y>=player.feet.y?-2:2);set(30,1);next();}
            else if(phase==1){y+=w(44);if(std::abs(y-w(48))>w(42)){set(44,-w(44));next();set(30,2);}}
            else{y+=w(44);if(y==w(48)){prev();prev();set(30,0);}}break;
        case 7:
            if(!phase){if(((ctx.frame+2)&3) || y>player.feet.y || dx()>=w(58) || dy()>=200)break;set(30,1);next();face();}
            else if(phase==2){if(reset_far(190,140))reset();}
            else{if(phase==3 && (u(0)&8192)){set(30,1);break;}move_ground(true,false);if(w(8))break;
                if(dx()>=640 || dy()>400)set(30,2);else if(phase==1 && dx()<=w(20)+6 && dy()<=8){set(30,3);next();}}break;
        case 8:case 14:
            if(type==14)set(24,u(24)&63);
            if(!phase){if(dx()<640 && (dx()>=340 || (dy()>230 && dy()<300))){set(30,1);next();face();}}
            else if(phase==1){move_ground(true,false);if(!w(8) && dx()>640)set(30,2);}
            else if(reset_far(190,140))reset();
            break;
        case 9:
            if(!phase){if(!((ctx.frame+1)&3) && dx()<w(58) && dy()<60){set(30,1);next();face();}}
            else if(phase==2){if(reset_far(400,199))reset();}
            else if(phase==3){if(visible()){if(u(0)&8192){set(30,1);face();}}else{next();e.animation(e.animation()-2);animate(e,rules);set(30,1);face();}}
            else{set(10,w(10)+1);if(!(u(10)&4) && !(ctx.frame&511))next();if(!(ctx.frame&127)){set(30,3);next();if(visible()){if(u(0)&8192){set(30,1);face();}}else{next();e.animation(e.animation()-2);animate(e,rules);set(30,1);face();}break;}move_ground(false,false);if(dx()>640)set(30,2);}break;
        case 10:
            if(!phase){if(!(ctx.frame&3) && !player.lock_ticks && dx()<w(58) && dy()<26){set(30,1);next();face();}}
            else if(phase==1){if(player.lock_ticks)break;int distance=x-player.feet.x;if(distance>=w(58)){prev();set(30,0);}else if(distance+50<w(58) && dy()<60){set(30,2);next();}}
            else if(phase==2){move_ground(false,true);if(dx()>640)set(30,3);}
            else if(reset_far(180,100))reset(2);
            break;
        case 11:
            if(!phase){if(!(ctx.frame&3) && dx()<w(58) && dy()<26){set(30,1);next();face();}}
            else if(phase==1){move_ground(false,false);if(dx()>640){set(30,2);break;}if(decrement_byte(54)<0 && dx()<64 && dy()<20){face();set(30,3);next();set(54,(u(54)&0xff00)|20);}}
            else if(phase==2){if(reset_far(180,100))reset();}
            else if(u(0)&8192){e.animation(e.animation()+4);shoot();prev();set(30,1);}
            break;
        case 12:
            set(24,u(24)&63);
            if(!phase){next();set(64,w(62));set(70,y);set(30,1);}
            else if(phase==1){y-=w(64);set(64,w(64)-1);if(!w(64))set(30,2);}
            else if(phase==2){y+=w(64);set(64,w(64)+1);if(y>=w(70)){y=w(70);set(66,w(68));set(30,3);prev();}}
            else if(!decrement_byte(66))set(30,0);
            break;
        case 13:
            if(!phase){set(6,x>player.feet.x?std::abs(w(6)):-std::abs(w(6)));if(dx()<w(58) && dy()<40){next();set(30,1);set(56,10);}}
            else if(phase==1){x-=w(6);y-=w(56);set(56,w(56)-1);if(!w(56)){next();set(30,2);}}
            else if(phase==2){x-=w(6);y+=w(56);set(56,w(56)+1);if(w(56)>10){set(30,3);next();set(56,w(54));}}
            else{set(56,w(56)-1);if(!w(56)){prev();prev();prev();set(30,0);}}break;
        case 15:set(24,u(24)&63);break;
        case 16:break;
        case 17:{
            set(24,u(24)&63);int timer=w(76)+1;if(timer<=w(74))set(76,timer);
            else if(dx()<w(72)){int distance=player.feet.y-y;if(distance>=0 && distance<w(78)){shoot(true);set(76,0);}}
            else set(76,0);
            break;
        }
        case 18:{
            bool active=player.feet.x>w(86)-w(80) && player.feet.x<w(86)+w(80) && player.feet.y>w(88)-w(82) && player.feet.y<w(88)+w(82);
            int target_x=active?player.feet.x:w(86),target_y=active?player.feet.y:w(88);
            if(x!=target_x){set(6,x<target_x?-std::abs(w(6)):std::abs(w(6)));x-=w(6);}
            if(y!=target_y)y-=y<target_y?-w(84):w(84);
            break;
        }
        default:throw std::runtime_error("Invalid enemy behavior");
        }
    }
};
}
void step_enemies(const Level& level,EnemyWorld& world,Body& player,EnemyStepContext& ctx,const NativeRules& rules){
    if(world.enemies.size()>50)throw std::runtime_error("Too many native enemies");
    ctx.attacks.clear();ctx.visible_enemies.clear();const auto view=ctx.view.value_or(Viewport{ctx.camera,320,176});if(!view.width || !view.height || view.width>4096 || view.height>4096)throw std::runtime_error("Invalid enemy viewport");
    for(unsigned i=0;i<world.enemies.size();++i){auto& e=world.enemies[i];if(!e.get(0))continue;
        if(!e.get(16)){Actor actor{e,level,rules,player,ctx,e.signed_get(2),e.signed_get(4),i};actor.update(e.get(24)&63);if(!e.get(0))continue;e.set(2,actor.x);e.set(4,actor.y);if((e.get(18)&0x8000) && !(e.get(24)&192))animate(e,rules);}
        e.set(18,e.get(18)&0x7fff);int x=e.signed_get(2)-view.camera.x,y=e.signed_get(4)-view.camera.y;
        if(x<=-48 || x>=int(view.width)+32 || y<=-48 || y>=int(view.height)+64 || e.get(0)==0x55ab)continue;
        unsigned frame=(e.get(0)-1)&8191;if(frame>=232)throw std::runtime_error("Invalid native enemy sprite");
        const auto& sprite=rules.sprites[101+frame];e.set(20,sprite.width);e.set(22,sprite.height);
        int left=x-(e.signed_get(6)<0?int(sprite.width)-sprite.anchor_x:sprite.anchor_x)+(view.camera.x&15),top=y-sprite.anchor_y+1;
        if(top>=int(view.height) || left>=int(view.width)+16 || left+int(sprite.width)<=0 || top+int(sprite.height)<=0)continue;
        e.set(18,e.get(18)|0x8000);unsigned type=e.get(24)&63;
        if(!(e.get(24)&192) && type!=12 && type!=15){
            ctx.visible_enemies.push_back(i);
            if(type!=16 && type!=17 && e.get(14) && !ctx.invulnerable && !ctx.invulnerability && !player.lock_ticks &&
                player_overlap(player,e.signed_get(2),e.signed_get(4),e.get(20)/4,e.get(22)>=3?e.get(22)-3:0)){
                if(type!=11 && (e.get(0)&8191)!=179)e.set(6,-e.signed_get(6));
                const unsigned kind=e.get(0)&8191;hurt(player,ctx,e.signed_get(2),e.get(14),kind>53 && kind<57?27:28);
            }
        }
    }
}
void step_enemy_projectiles(Body& player,EnemyStepContext& ctx,const NativeRules& rules){
    const auto view=ctx.view.value_or(Viewport{ctx.camera,320,176});if(!view.width || !view.height || view.width>4096 || view.height>4096)throw std::runtime_error("Invalid enemy viewport");
    for(auto& shot:ctx.projectiles){if(!shot.kind)continue;
        if(shot.kind>71)throw std::runtime_error("Invalid native projectile");
        shot.feet.x-=shot.vx;shot.feet.y+=shot.vy;if(shot.vy && shot.vy<4)++shot.vy;
        shot.flags&=0x7fff;const auto& sprite=rules.sprites[30+shot.kind-1];shot.width=sprite.width;shot.height=sprite.height;
        int left=shot.feet.x-view.camera.x-(shot.vx<0?int(sprite.width)-sprite.anchor_x:sprite.anchor_x)+(view.camera.x&15);
        int top=shot.feet.y-view.camera.y-sprite.anchor_y+1;
        if(top>=int(view.height) || left>=int(view.width)+16 || left+int(sprite.width)<=0 || top+int(sprite.height)<=0){shot.kind=0;continue;}
        shot.flags|=0x8000;
        if(!ctx.invulnerable && !ctx.invulnerability && !player.lock_ticks && player_overlap(player,shot.feet.x,shot.feet.y,shot.vy?2:shot.width/2,shot.height)){
            shot.kind=0;hurt(player,ctx,shot.feet.x,70,28);
        }
    }
}
void resolve_enemy_impacts(EnemyWorld& world,ObjectWorld& objects,EnemyStepContext& ctx){
    if(world.hit_cooldown)--world.hit_cooldown;
    auto overlap=[](const EnemyBody& victim,int x,int y,unsigned width,unsigned height){
        int left=victim.signed_get(2)-int(victim.get(20)/2),top=victim.signed_get(4)-int(victim.get(22));
        return x+int(width/2)>left && x-int(width/2)<left+int(victim.get(20)) && y>top && y-int(height)<victim.signed_get(4);
    };
    for(unsigned index:ctx.visible_enemies){
        if(index>=world.enemies.size())throw std::runtime_error("Invalid enemy contact index");
        auto& victim=world.enemies[index];
        if(victim.get(18)&1)victim.set(18,(victim.get(18)&~2u)|(world.hit_cooldown?2:0));
        auto impact=[&](unsigned kind,int x,int y,unsigned width,unsigned height,unsigned flags,auto reverse,auto remove){
            if(!overlap(victim,x,y,width,height))return;
            ctx.sound_event=5;if(kind!=44)reverse();bool kill=true;
            if(victim.get(18)&1){if(world.hit_cooldown)kill=false;else{world.hit_cooldown=10;if(world.boss_power && --world.boss_power)kill=false;}}
            if(kill){if((victim.get(24)&63)!=14)victim.set(6,0);victim.set(24,victim.get(24)|64);victim.set(18,victim.get(18)&~2u);}
            if((flags&128) && !(flags&8) && kind!=44)remove();
        };
        for(unsigned slot:objects.impact_list){if(slot>=40)throw std::runtime_error("Invalid object impact index");auto& object=objects.objects[slot];
            impact(object.kind,object.feet.x,object.feet.y,object.width,object.height,object.flags,[&]{object.vx=-object.vx;},[&]{object.kind=0;});}
        for(unsigned slot:ctx.attacks){if(slot>=world.enemies.size())throw std::runtime_error("Invalid enemy attack index");auto& object=world.enemies[slot];
            impact(object.get(0),object.signed_get(2),object.signed_get(4),object.get(20),object.get(22),object.get(18),[&]{object.set(6,-object.signed_get(6));},[&]{object.set(0,0);});}
    }
}

}
