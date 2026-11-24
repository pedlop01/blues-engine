// SPDX-License-Identifier: GPL-3.0-or-later
// Observed POIL semantics, including Amiga-specific initialization transforms.
#include "titus/fox_amiga.hpp"
#include <algorithm>
#include <stdexcept>
namespace titus::fox_amiga {
namespace {
[[noreturn]] void bad(const char* s){throw std::runtime_error(s);}
unsigned word(const Bytes& d,std::size_t p){if(p>d.size() || d.size()-p<2)bad("Truncated POIL word");return d[p]|(unsigned(d[p+1])<<8);}
int signed_word(unsigned v){return v>=32768?int(v)-65536:int(v);}
void be(Bytes& d,int value){unsigned v=unsigned(value)&65535;d.push_back(std::uint8_t(v>>8));d.push_back(std::uint8_t(v));}
unsigned object_kind(unsigned raw){
    if(raw==65535)return 0;
    unsigned n=raw&255;
    if(n<30 || n>100)bad("Unknown Amiga object sprite");
    n-=30;
    if(n>=19 && n<=22)n=19;
    else if(n==24)n=25;
    else if(n==27)n=26;
    else if(n==40)n=9;
    return n+1;
}
void overlay(Image& out,const Image& sprite,int world_x,int world_y,unsigned view_x,unsigned view_y,bool flip){
    if(sprite.pixels.size()!=std::size_t(sprite.width)*sprite.height)bad("Invalid entity sprite");
    const int left=world_x-int(sprite.width/2)-int(view_x),top=world_y-int(sprite.height)+1-int(view_y);
    for(unsigned y=0;y<sprite.height;++y)for(unsigned x=0;x<sprite.width;++x){
        int dx=left+int(x),dy=top+int(y);
        if(dx<0 || dy<0 || dx>=int(out.width) || dy>=int(out.height))continue;
        auto pixel=sprite.pixels[y*sprite.width+(flip?sprite.width-1-x:x)];
        if(pixel)out.pixels[std::size_t(dy)*out.width+unsigned(dx)]=pixel;
    }
}
}
Level initialize_level(unsigned number,const Bytes& map,const Bytes& d,const LevelRules& rules){
    if(!number || number>16 || number==15 || map.empty() || map.size()%256 || map.size()>65536 || d.size()!=3060)
        bad("Invalid Amiga level data");
    Level out;out.number=number;out.height_tiles=unsigned(map.size()/256);out.map=map;
    // Hidden collectible marker tiles are cleared by the original map loader,
    // then reintroduced at the valid bonus positions by the POIL loader.
    for(auto& tile:out.map){
        if(number==1){
            switch(tile){case 0x11:case 0x13:case 0x23:case 0x24:case 0x25:case 0x27:case 0x36:case 0x3b:case 0x4d:case 0x60:tile=0x91;break;default:break;}
        }else if(number==4){
            switch(tile){case 0x54:case 0x55:case 0x56:case 0x57:case 0x59:case 0x5a:case 0x68:case 0x69:case 0x6a:case 0x6b:case 0x6d:case 0x6e:tile=0xb7;break;default:break;}
        }
    }
    std::copy_n(d.begin(),256,out.horizontal.begin());
    std::copy_n(d.begin()+256,256,out.floor.begin());
    std::copy_n(d.begin()+512,256,out.ceiling.begin());
    out.animation.fill(128);
    for(unsigned i=0;i<256;){
        if(out.ceiling[i]&128){
            if(i>253)bad("Animated tile group exceeds tileset");
            const unsigned values[3][3]={{2,2,2},{3,3,0},{4,1,1}};
            for(unsigned phase=0;phase<3;++phase)for(unsigned j=0;j<3;++j)out.animation[phase*256+i+j]=std::uint8_t(values[phase][j]);
            for(unsigned j=0;j<3;++j)out.ceiling[i+j]&=127;
            i+=3;
        }else {out.ceiling[i]&=127;++i;}
    }
    for(unsigned i=0;i<256;++i){
        if(out.horizontal[i]==2){out.gravity_wall_tile=i;out.floor[i]=0;}
        if(out.horizontal[i]>=4 && out.horizontal[i]<=6){
            out.horizontal[i]=std::uint8_t((1u<<(out.horizontal[i]-2))-2+128);
            out.ceiling[i]&=128;
        }
    }
    for(unsigned i=0;i<256;++i){
        const unsigned floor=out.floor[i];
        if(floor>=8 && floor<=10)out.floor[i]=std::uint8_t((1u<<(floor-7))-1+64);
        else if(floor>=2 && floor<=4)out.floor[i]=std::uint8_t((floor-2)*2+129);
        if(out.ceiling[i]==4)out.floor[i]=0x47;
    }
    for(unsigned i=0;i<40;++i){
        const unsigned p=768+i*6,raw=word(d,p);
        auto& obj=out.objects[i];obj.slot=i;obj.raw_sprite=raw;obj.kind=object_kind(raw);
        obj.position={signed_word(word(d,p+2)),signed_word(word(d,p+4))};
        if((obj.position.y&15)==0)--obj.position.y;
    }
    out.camera_trigger_y=((((word(d,1008)+2)&65535)<<4)+176)&65535;
    out.spawn={signed_word(word(d,1010)),signed_word((word(d,1012)-1)&65535)};
    if(out.spawn.x<0 || out.spawn.x>=4096 || out.spawn.y<0 || out.spawn.y>=int(out.height_tiles*16))bad("Spawn outside Amiga map");
    for(unsigned i=0;i<50;++i){
        const unsigned p=1014+i*26,raw=word(d,p+4);
        if(raw==65535)continue;
        Enemy enemy;enemy.slot=i;enemy.raw_sprite=raw;enemy.kind=(raw&8191)+1;
        enemy.behavior=word(d,p+6)&31;
        enemy.position={signed_word(word(d,p)),signed_word(word(d,p+2))};
        if(((enemy.behavior>=7 && enemy.behavior<=11) || (enemy.behavior>=13 && enemy.behavior<=16)) && (enemy.position.y&15)==0)
            --enemy.position.y;
        enemy.source_speed_x=signed_word(word(d,p+8));enemy.initial_power=std::min(240u,word(d,p+12));
        std::copy_n(d.begin()+p,26,enemy.parameters.begin());
        // Match the original rejection of unknown behavior codes without
        // indexing engine tables. The accepted frame set comes from the original
        // animation definitions, not from the mere presence of a sprite image.
        if(enemy.behavior>18 || enemy.kind>rules.enemy_animation_frames.size() || !rules.enemy_animation_frames[enemy.kind-1])enemy.kind=0;
        out.enemies.push_back(enemy);
    }
    out.dirty_rows.assign(out.height_tiles,0);
    for(unsigned i=0;i<100;++i){
        const unsigned p=2314+i*4,index=word(d,p+2);
        auto& b=out.bonuses[i];b={d[p],d[p+1],d[p+2],d[p+3],index<out.map.size()};
        if(b.active){out.map[index]=std::uint8_t(b.tile);if(!(out.animation[b.replacement]&128))out.dirty_rows[b.y]=1;}
    }
    for(unsigned y=0;y<out.height_tiles;++y)for(unsigned x=0;x<256;++x)
        if(!(out.animation[out.map[y*256+x]]&128))out.dirty_rows[y]=1;
    out.camera_lock_y=(word(d,2714)<<4)&65535;
    for(unsigned i=0;i<20;++i){const unsigned p=2716+i*7;out.gates[i]={word(d,p),word(d,p+2),word(d,p+4),d[p+6],word(d,p)!=65535};}
    for(unsigned i=0;i<10;++i){
        const unsigned p=2856+i*20,raw=word(d,p+4);if(raw==65535)continue;
        Elevator e;e.slot=i;e.raw_sprite=raw;e.kind=(raw&511)+1;e.position={signed_word(word(d,p+12)),signed_word(word(d,p+14))};
        e.direction=d[p+16];e.speed=d[p+7];e.range=word(d,p+10);
        std::copy_n(d.begin()+p,20,e.parameters.begin());out.elevators.push_back(e);
    }
    out.finish={signed_word(word(d,3056)),signed_word((word(d,3058)-16)&65535)};
    return out;
}
Collision collision_at(const Level& level,int x,int y){
    if(x<0 || y<0 || x>=4096 || y>=int(level.height_tiles*16))return {0,1,1,1,true};
    const unsigned tile=level.map.at(std::size_t(y/16)*256+unsigned(x/16));
    return {tile,level.horizontal[tile],level.floor[tile],level.ceiling[tile],false};
}
Bytes object_projection(const Level& level){Bytes d;for(const auto& obj:level.objects){be(d,obj.kind);be(d,obj.position.x);be(d,obj.position.y);}return d;}
Bytes enemy_projection(const Level& level){Bytes d;for(const auto& e:level.enemies){be(d,e.kind);be(d,e.position.x);be(d,e.position.y);be(d,e.behavior);}return d;}
Bytes elevator_projection(const Level& level){Bytes d;for(const auto& e:level.elevators){be(d,e.kind);be(d,e.position.x);be(d,e.position.y);be(d,e.direction);}return d;}
Bytes gate_projection(const Level& level){Bytes d;for(const auto& g:level.gates){be(d,g.entrance);be(d,g.screen);be(d,g.exit);be(d,g.flags);}return d;}
std::optional<GateTransition> gate_transition(const Level& level,Point feet){
    if(feet.x<0 || feet.x>=4096 || feet.y<0 || feet.y>=int(level.height_tiles*16))return {};
    const unsigned entrance=unsigned(((feet.y/16+1)*256+feet.x/16)&65535);
    for(const auto& g:level.gates)if(g.active && g.entrance==entrance)
        return GateTransition{{int((g.exit&255)*16),int((g.exit>>8)*16)-2},
                              {int((g.screen&255)*16),int((g.screen>>8)*16)+16},g.flags};
    return {};
}
bool exit_ready(const Level& level,Point feet,bool blocked,unsigned carried_kind){
    if(blocked || feet.x<0 || feet.x>=4096 || feet.y<0 || feet.y>=int(level.height_tiles*16))return false;
    const int x=feet.x&~15,y=feet.y&~15;
    return (x==level.finish.x || x-16==level.finish.x) &&
           (y==level.finish.y || y-16==level.finish.y) &&
           (level.number!=11 || carried_kind==27);
}
unsigned next_level(unsigned number){
    if(!number || number>16 || number==15)bad("Invalid campaign level");
    return number==16?0:(number==14?16:number+1);
}
Image level_scene(const Level& level,const std::vector<Image>& ts,const std::vector<Image>& fox,
                  const std::vector<Image>& objects,const std::vector<Image>& enemies,unsigned x,unsigned y,unsigned w,unsigned h){
    auto out=map_view(level.map,ts,x,y,w,h);
    for(const auto& e:level.enemies)if(e.kind && e.kind<=enemies.size())overlay(out,enemies[e.kind-1],e.position.x,e.position.y,x,y,e.raw_sprite&0x8000);
    for(const auto& o:level.objects)if(o.kind && o.kind<=objects.size())overlay(out,objects[o.kind-1],o.position.x,o.position.y,x,y,false);
    if(fox.empty())bad("Missing Fox sprite bank");
    overlay(out,fox.front(),level.spawn.x,level.spawn.y,x,y,false);
    return out;
}
Image collision_view(const Level& level,unsigned x,unsigned y,unsigned w,unsigned h){
    if(!w || !h || w>4096 || h>4096 || x>=4096 || y>=level.height_tiles*16 || w>4096-x || h>level.height_tiles*16-y)bad("Invalid collision viewport");
    Image out{w,h,Bytes(std::size_t(w)*h)};
    for(unsigned row=0;row<h;++row)for(unsigned col=0;col<w;++col){
        auto c=collision_at(level,int(x+col),int(y+row));
        out.pixels[std::size_t(row)*w+col]=std::uint8_t((c.horizontal?1:0)|(c.floor?2:0)|(c.ceiling?4:0));
    }
    return out;
}
}
