// SPDX-License-Identifier: GPL-3.0-or-later
#include "titus/fox_amiga_game.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace titus::fox_amiga {
namespace {
int shift(int n,unsigned bits){int unit=1<<bits;return n>=0?n/unit:-((-n+unit-1)/unit);}
std::uint32_t rgb(unsigned color){return 0xff000000|((color&0xf00)<<12)|((color&0xf00)<<8)|((color&0xf0)<<8)|((color&0xf0)<<4)|((color&15)<<4)|(color&15);}
void bonuses(GameState& game){
    if(game.bonus_delay<4){++game.bonus_delay;return;}
    int x=game.player.feet.x/16,y=(game.player.feet.y+1)/16;
    if(x<0 || x>=256 || y<0 || y>=int(game.level.height_tiles))return;
    for(int row=y;row>=0 && row>=y-2;--row){unsigned index=unsigned(row)*256+unsigned(x),tile=game.level.map[index];
        if(!(game.level.horizontal[tile]&2))continue;
        for(auto& bonus:game.level.bonuses)if(bonus.active && bonus.x==unsigned(x) && bonus.y==unsigned(row)){
            bonus.active=false;game.level.map[index]=std::uint8_t(bonus.replacement);break;
        }
        if(tile>=253){if(game.player.energy<16)++game.player.energy;else game.lives=std::min(99u,game.lives+1);game.actors.sound_event=3;}
        else if(game.level.horizontal[tile]==0x86){game.checkpoint=game.player.feet;game.actors.sound_event=3;}
        // 0x82 and 0x8e enter the original bonus/secret presentation. Their
        // transition is retained separately from ordinary campaign completion.
    }
}
void camera_step(GameState& game){
    if(game.player.state==7)return;
    int dx=shift(game.player.feet.x-game.camera.x-160,4),dy;
    if(game.player.feet.y<int(game.level.camera_trigger_y) && game.camera.y>int(game.level.camera_trigger_y)-176)
        dy=-std::max(1,shift(game.camera.y-int(game.level.camera_trigger_y)+176,3));
    else dy=shift(game.player.feet.y-game.camera.y-80,3);
    if(dx>0 && game.camera.x+dx>int(game.level.camera_lock_y) && game.player.feet.x<int(game.level.camera_lock_y)+320)dx=int(game.level.camera_lock_y)-game.camera.x;
    if(dy>0 && game.player.feet.y<int(game.level.camera_trigger_y) && game.camera.y+dy>=int(game.level.camera_trigger_y)-176)dy=int(game.level.camera_trigger_y)-176-game.camera.y;
    game.camera.x=std::clamp(game.camera.x+dx,0,4096-320);
    game.camera.y=std::clamp(game.camera.y+dy,0,std::max(0,int(game.level.height_tiles)*16-176));
}
}
LevelAssets load_level_assets(unsigned number,const ReadResource& read,const NativeRules& rules){
    if(!number || number>16 || number==15)throw std::runtime_error("Invalid Amiga campaign level");
    std::ostringstream suffix;suffix<<std::setw(2)<<std::setfill('0')<<number;
    LevelAssets result;result.initial=initialize_level(number,unpack(read("level"+suffix.str()+".tbl")),unpack(read("poil"+suffix.str()+".bin")),rules.level);
    result.bricks=tiles(unpack(read("brik"+suffix.str()+".bin")));return result;
}
GameAssets load_game_assets(const ReadResource& read){
    GameAssets assets;assets.rules=native_rules(read("main"));
    auto bank=[&](const char* name){return sprites(unpack(read(name)));};
    assets.fox=bank("fox.spr");assets.objects=bank("objet2.spr");assets.enemies=bank("monster2.spr");assets.font=bank("fonte.spr");assets.kiss=bank("bisou2.spr");assets.ending=bank("theend.spr");
    const char* names[]={"ressort","ballon","fox","tirperso","coup"};
    for(unsigned i=0;i<5;++i)assets.effects[i]=std::make_shared<EffectSample>(effect_sample(read(names[i]),assets.rules.effect_frequencies[i]));
    return assets;
}
GameState start_game(const LevelAssets& assets,const NativeRules& rules,unsigned lives){
    if(lives>99)throw std::runtime_error("Too many starting lives");
    GameState game;game.level=assets.initial;game.player.feet=game.checkpoint=game.level.spawn;game.lives=lives;
    game.objects=initialize_objects(game.level,rules.objects);game.enemies=initialize_enemies(game.level,rules);game.platforms=initialize_platforms(game.level);
    game.camera={std::clamp(game.player.feet.x-160,0,3776),std::clamp(game.player.feet.y-80,0,std::max(0,int(game.level.height_tiles)*16-176))};return game;
}
void game_step(GameState& game,const BodyInput& input,bool action,const NativeRules& rules){
    if(game.phase!=GamePhase::playing)return;
    ++game.frame;if(game.gate_cooldown)--game.gate_cooldown;
    if(game.actors.invulnerability)--game.actors.invulnerability;
    game.actors.sound_event=0;game.down_ticks=input.down?std::min(255u,game.down_ticks+1):0;
    game.player=object_player_step(game.level,game.player,input,rules.motion,game.objects,action,{game.camera,game.invulnerable,false});
    bonuses(game);camera_step(game);step_platforms(game.platforms,game.player,game.camera,rules);
    ObjectStepContext object_context{game.frame,game.down_ticks,game.invulnerable?1:game.actors.invulnerability,game.camera,input.up,input.down};
    step_objects(game.level,game.objects,game.player,object_context,rules.objects);
    game.actors.frame=game.frame;game.actors.camera=game.camera;game.actors.invulnerable=game.invulnerable;
    if(!game.invulnerable)game.actors.invulnerability=object_context.invulnerability;
    step_enemies(game.level,game.enemies,game.player,game.actors,rules);step_enemy_projectiles(game.player,game.actors,rules);resolve_enemy_impacts(game.enemies,game.objects,game.actors);
    if(game.objects.sound_event)game.actors.sound_event=game.objects.sound_event;
    if(game.player.restart_requested){
        if(!game.infinite_lives && !game.lives){game.phase=GamePhase::game_over;return;}
        if(!game.infinite_lives)--game.lives;
        game.player={};game.player.feet=game.checkpoint;game.actors.invulnerability=100;
        game.objects=initialize_objects(game.level,rules.objects);game.enemies=initialize_enemies(game.level,rules);game.platforms=initialize_platforms(game.level);game.actors.projectiles={};return;
    }
    if(game.player.state==7)return;
    if(input.down && !game.gate_cooldown){if(auto gate=gate_transition(game.level,game.player.feet)){
        if(gate->destination.x>=0 && gate->destination.x<4096 && gate->destination.y>=0 && gate->destination.y<int(game.level.height_tiles)*16){
            game.player.feet=gate->destination;game.player.vx=game.player.vy=0;game.camera={std::clamp(gate->camera.x,0,3776),std::clamp(gate->camera.y,0,std::max(0,int(game.level.height_tiles)*16-176))};game.gate_cooldown=20;
        }
    }}
    unsigned carried=game.objects.carried?game.objects.objects[*game.objects.carried].kind:0;
    if(exit_ready(game.level,game.player.feet,game.enemies.boss_power!=0,carried))game.phase=GamePhase::level_complete;
    if(game.level.number==16)game.phase=GamePhase::ending;
}
RgbFrame game_frame(const GameState& game,const GameAssets& assets,Point camera,unsigned width,unsigned height){
    if(!width || !height || width>4096 || height>4096 || !assets.levels.at(game.level.number))throw std::runtime_error("Invalid game frame inputs");
    RgbFrame frame{width,height,std::vector<std::uint32_t>(std::size_t(width)*height,0xff000000)};
    auto palette=assets.rules.palette;if(game.level.number<=14)palette[14]=assets.rules.level_color14[game.level.number-1];
    const auto& bricks=assets.levels[game.level.number]->bricks;
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){int wx=camera.x+int(x),wy=camera.y+int(y);if(wx<0 || wx>=4096 || wy<0 || wy>=int(game.level.height_tiles)*16)continue;
        unsigned tile=game.level.map[unsigned(wy/16)*256+unsigned(wx/16)];if(tile>=bricks.size())throw std::runtime_error("Invalid game tile");
        const auto& image=bricks[tile];frame.pixels[std::size_t(y)*width+x]=rgb(palette.at(image.pixels[unsigned(wy&15)*16+unsigned(wx&15)]));
    }
    auto draw=[&](const std::vector<Image>& bank,unsigned index,unsigned global,Point feet,bool flip,bool player=false){
        if(index>=bank.size() || global>=assets.rules.sprites.size())throw std::runtime_error("Invalid sprite render index");
        const auto& image=bank[index];const auto& anchor=assets.rules.sprites[global];int left=feet.x-camera.x-(flip?int(image.width)-anchor.anchor_x:anchor.anchor_x),top=feet.y-camera.y-anchor.anchor_y;
        for(unsigned y=0;y<image.height;++y)for(unsigned x=0;x<image.width;++x){int sx=left+int(x),sy=top+int(y);if(sx<0 || sy<0 || sx>=int(width) || sy>=int(height))continue;
            unsigned color=image.pixels[std::size_t(y)*image.width+(flip?image.width-1-x:x)];if(color)frame.pixels[std::size_t(sy)*width+unsigned(sx)]=rgb((player?assets.rules.player_palette:palette).at(color));}
    };
    if(!game.actors.invulnerability || !(game.frame&2))draw(assets.fox,game.player.frame,game.player.frame,game.player.feet,!game.player.facing_right,true);
    auto object=[&](const ObjectBody& body){if(body.kind && body.kind<=assets.objects.size())draw(assets.objects,body.kind-1,30+body.kind-1,body.feet,body.vx<0);};
    for(const auto& platform:game.platforms)object(platform.body);
    for(const auto& body:game.objects.objects)object(body);
    for(const auto& body:game.objects.smoke)object(body);
    for(const auto& enemy:game.enemies.enemies)if(enemy.get(0) && enemy.get(0)!=0x55ab){unsigned index=(enemy.get(0)-1)&8191;draw(assets.enemies,index,101+index,{enemy.signed_get(2),enemy.signed_get(4)},enemy.signed_get(6)<0);}
    for(const auto& shot:game.actors.projectiles)object(shot);
    return frame;
}
}
