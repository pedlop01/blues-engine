// SPDX-License-Identifier: GPL-3.0-or-later
#include "titus/fox_amiga_game.hpp"
#include <algorithm>
#include <cmath>
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
        if(tile>=253){if(game.player.energy<16)++game.player.energy;else game.bonus_credits=std::min(99u,game.bonus_credits+1);game.actors.sound_event=3;}
        else if(game.level.horizontal[tile]==0x86){game.checkpoint=game.player.feet;game.actors.sound_event=3;}
        else if(game.level.horizontal[tile]==0x82)game.message_level=game.level.number;
        else if(game.level.horizontal[tile]==0x8e)game.message_level=14;
    }
}
void camera_step(GameState& game){
    if(game.player.state==7 || game.gate_flags)return;
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
    assets.logo=picture(unpack(read("titus.pc1")),4,true);assets.title=picture(unpack(read("titre2.pc1")),5,true);assets.presentation=picture(unpack(read("present2.pc1")),6,false);
    auto bank=[&](const char* name){return sprites(unpack(read(name)));};
    assets.fox=bank("fox.spr");assets.objects=bank("objet2.spr");assets.enemies=bank("monster2.spr");assets.font=bank("fonte.spr");assets.hud=characters(read("indic.cha"));assets.kiss=bank("bisou2.spr");assets.ending=bank("theend.spr");
    const char* names[]={"ressort","ballon","fox","tirperso","coup"};
    for(unsigned i=0;i<5;++i)assets.effects[i]=std::make_shared<EffectSample>(effect_sample(read(names[i]),assets.rules.effect_frequencies[i]));
    return assets;
}
GameState start_game(const LevelAssets& assets,const NativeRules& rules,unsigned lives){
    if(lives>99)throw std::runtime_error("Too many starting lives");
    GameState game;game.level=assets.initial;game.player.feet=game.checkpoint=game.level.spawn;game.player.facing_right=true;game.lives=lives;
    game.objects=initialize_objects(game.level,rules.objects);game.enemies=initialize_enemies(game.level,rules);game.platforms=initialize_platforms(game.level);
    game.camera={std::clamp(game.player.feet.x-160,0,3776),std::clamp(game.player.feet.y-80,0,std::max(0,int(game.level.height_tiles)*16-176))};return game;
}
void finish_level_bonuses(GameState& game){unsigned total=game.bonus_credits+game.completion_spent;game.lives=std::min(99u,game.lives+total/10);game.bonus_credits=total%10;game.completion_spent=0;}
void game_step(GameState& game,const BodyInput& input,bool action,const NativeRules& rules){
    game.actors.sound_event=0;
    if(game.phase==GamePhase::level_complete){
        if(++game.completion_ticks%5==0 && (game.bonus_credits>=10 || game.completion_spent)){
            --game.bonus_credits;if(++game.completion_spent==10){game.completion_spent=0;game.lives=std::min(99u,game.lives+1);game.actors.sound_event=3;}
        }
        return;
    }
    if(game.phase!=GamePhase::playing || game.message_level)return;
    ++game.frame;if(game.gate_cooldown)--game.gate_cooldown;
    if(game.actors.invulnerability)--game.actors.invulnerability;
    game.actors.sound_event=0;game.down_ticks=input.down?std::min(255u,game.down_ticks+1):0;
    const bool new_action=action && !game.objects.action_latched;
    const bool releasing_enemy=new_action && game.enemies.carried.has_value();
    if(releasing_enemy){
        if(game.player.state==5 || input.down)drop_enemy(game.level,game.enemies,game.player);
        else throw_enemy(game.enemies,game.player,input.up);
        game.objects.action_latched=true;
    }
    game.player=object_player_step(game.level,game.player,input,rules.motion,game.objects,action,{game.camera,game.invulnerable,true,bool(game.enemies.carried)});
    if(new_action && !releasing_enemy && (input.down || game.player.state==5) && !game.objects.carried && !game.enemies.carried){
        if(pick_enemy(game.enemies,game.player))game.objects.attachment=game.enemies.attachment;
    }
    if(game.enemies.carried)attach_enemy(game.enemies,game.player.feet,game.objects.attachment);
    bonuses(game);camera_step(game);
    std::optional<Viewport> view;
    if(game.view_step && !game.gate_flags){
        if(game.view_step>8)throw std::runtime_error("Invalid expanded game view");
        unsigned width=320*(4+game.view_step)/4,height=176*(4+game.view_step)/4;if(height>game.level.height_tiles*16 && game.level.height_tiles*16>=176){height=game.level.height_tiles*16;width=320*height/176;}
        view=Viewport{{std::clamp(game.camera.x+160-int(width/2),0,4096-int(width)),std::clamp(game.camera.y+88-int(height/2),0,std::max(0,int(game.level.height_tiles)*16-int(height)))},width,height};
    }
    step_platforms(game.platforms,game.player,game.camera,rules,view);
    ObjectStepContext object_context{game.frame,game.down_ticks,game.invulnerable?1:game.actors.invulnerability,game.camera,input.up,input.down};
    object_context.view=view;step_objects(game.level,game.objects,game.player,object_context,rules.objects);
    game.actors.view=view;game.actors.frame=game.frame;game.actors.camera=game.camera;game.actors.invulnerable=game.invulnerable;
    if(!game.invulnerable)game.actors.invulnerability=object_context.invulnerability;
    step_enemies(game.level,game.enemies,game.player,game.actors,rules);step_enemy_projectiles(game.player,game.actors,rules);resolve_enemy_impacts(game.enemies,game.objects,game.actors);
    if(game.objects.sound_event)game.actors.sound_event=game.objects.sound_event;
    if(game.player.restart_requested){
        if(!game.infinite_lives && game.lives<=1){game.lives=0;game.phase=GamePhase::game_over;return;}
        if(!game.infinite_lives)--game.lives;
        game.player={};game.player.facing_right=true;game.player.feet=game.checkpoint;game.actors.invulnerability=100;game.gate_flags=0;game.camera={std::clamp(game.checkpoint.x-160,0,3776),std::clamp(game.checkpoint.y-80,0,std::max(0,int(game.level.height_tiles)*16-176))};
        game.objects=initialize_objects(game.level,rules.objects);game.enemies=initialize_enemies(game.level,rules);game.platforms=initialize_platforms(game.level);game.actors.projectiles={};return;
    }
    if(game.player.state==7)return;
    if(input.down && game.down_ticks>=30 && game.player.state==5 && !game.objects.carried && !game.enemies.carried && !game.gate_cooldown){
        game.down_ticks=0;
        if(collision_at(game.level,game.player.feet.x,game.player.feet.y+16).floor==5){game.player.feet.x=(game.player.feet.x&~15)|8;game.player.feet.y+=32;game.player.vx=game.player.vy=0;}
        else if(auto gate=gate_transition(game.level,game.player.feet)){
        if(gate->destination.x>=0 && gate->destination.x<4096 && gate->destination.y>=0 && gate->destination.y<int(game.level.height_tiles)*16){
            game.player.feet=gate->destination;game.player.vx=game.player.vy=0;game.camera={std::clamp(gate->camera.x,0,3776),std::clamp(gate->camera.y,0,std::max(0,int(game.level.height_tiles)*16-176))};game.gate_cooldown=20;game.gate_flags=gate->flags;
        }
        }
    }
    unsigned carried=game.objects.carried?game.objects.objects[*game.objects.carried].kind:0;
    if(exit_ready(game.level,game.player.feet,game.enemies.boss_power!=0,carried))game.phase=GamePhase::level_complete;
    if(game.level.number==16)game.phase=GamePhase::ending;
}
RgbFrame game_frame(const GameState& game,const GameAssets& assets,Point camera,unsigned width,unsigned height){
    if(!width || !height || width>4096 || height>4096 || !assets.levels.at(game.level.number))throw std::runtime_error("Invalid game frame inputs");
    RgbFrame frame{width,height,std::vector<std::uint32_t>(std::size_t(width)*height,0xff000000)};
    auto palette=assets.rules.palette;if(game.level.number<=16)palette[14]=assets.rules.level_color14[game.level.number-1];
    const auto& bricks=assets.levels[game.level.number]->bricks;
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){int wx=camera.x+int(x),wy=camera.y+int(y);if(wx<0 || wx>=4096 || wy<0 || wy>=int(game.level.height_tiles)*16)continue;
        unsigned tile=animated_tile(game.level,game.level.map[unsigned(wy/16)*256+unsigned(wx/16)],game.frame);if(tile>=bricks.size())throw std::runtime_error("Invalid game tile");
        const auto& image=bricks[tile];frame.pixels[std::size_t(y)*width+x]=rgb(image.pixels[unsigned(wy&15)*16+unsigned(wx&15)]==14?assets.rules.copper_color14.at(game.level.number-1)[y*176/height]:palette.at(image.pixels[unsigned(wy&15)*16+unsigned(wx&15)]));
    }
    auto draw=[&](const std::vector<Image>& bank,unsigned index,unsigned global,Point feet,bool flip,bool player=false){
        if(index>=bank.size() || global>=assets.rules.sprites.size())throw std::runtime_error("Invalid sprite render index");
        const auto& image=bank[index];const auto& anchor=assets.rules.sprites[global];int left=feet.x-camera.x-(player?16:(flip?int(image.width)-anchor.anchor_x:anchor.anchor_x)),top=feet.y-camera.y-(player?int(image.height):anchor.anchor_y)+1;
        for(unsigned y=0;y<image.height;++y)for(unsigned x=0;x<image.width;++x){int sx=left+int(x),sy=top+int(y);if(sx<0 || sy<0 || sx>=int(width) || sy>=int(height))continue;
            unsigned color=image.pixels[std::size_t(y)*image.width+(flip?image.width-1-x:x)];if(color)frame.pixels[std::size_t(sy)*width+unsigned(sx)]=rgb(!player && color==14?assets.rules.copper_color14.at(game.level.number-1)[unsigned(sy)*176/height]:(player?assets.rules.player_palette:palette).at(color));}
    };
    auto object=[&](const ObjectBody& body){if(body.kind && body.kind<=assets.objects.size())draw(assets.objects,body.kind-1,30+body.kind-1,body.feet,body.vx<0);};
    for(const auto& platform:game.platforms)object(platform.body);
    for(const auto& body:game.objects.objects)object(body);
    for(const auto& body:game.objects.smoke)object(body);
    for(const auto& enemy:game.enemies.enemies)if(enemy.get(0) && enemy.get(0)!=0x55ab){unsigned index=(enemy.get(0)-1)&8191;draw(assets.enemies,index,101+index,{enemy.signed_get(2),enemy.signed_get(4)},enemy.signed_get(6)<0);}
    for(const auto& shot:game.actors.projectiles)object(shot);
    // Original BPLCON2=0x24: attached Fox hardware sprites cover the playfield.
    if(game.phase!=GamePhase::ending && game.phase!=GamePhase::game_over && (!game.actors.invulnerability || !(game.frame&2)))draw(assets.fox,game.player.frame,game.player.frame,game.player.feet,!game.player.facing_right,true);
    return frame;
}
unsigned animated_tile(const Level& level,unsigned tile,unsigned frame){
    if(tile>255)throw std::runtime_error("Invalid animated tile");
    unsigned offset=level.animation[((frame/8)%3)*256+tile];return offset&128?tile:(tile+offset-2)&255;
}
RgbFrame smooth_game_frame(const GameState& game,const GameState& previous,const GameAssets& assets,double camera_x,double camera_y,double scale,unsigned width,unsigned height,double alpha){
    if(!std::isfinite(camera_x) || !std::isfinite(camera_y) || !std::isfinite(scale) || !std::isfinite(alpha) || !width || !height || width>4096 || height>4096 || scale<.25 || scale>16 || alpha<0 || alpha>1 || !assets.levels.at(game.level.number))throw std::runtime_error("Invalid smooth render inputs");
    RgbFrame frame{width,height,std::vector<std::uint32_t>(std::size_t(width)*height,0xff000000)};
    auto palette=assets.rules.palette;if(game.level.number<=16)palette[14]=assets.rules.level_color14[game.level.number-1];std::array<std::uint32_t,16> colors{},player_colors{};for(unsigned i=0;i<16;++i){colors[i]=rgb(palette[i]);player_colors[i]=rgb(assets.rules.player_palette[i]);}
    const auto& bricks=assets.levels[game.level.number]->bricks;
    std::vector<int> world_x(width);for(unsigned x=0;x<width;++x)world_x[x]=int(std::floor(camera_x+x/scale));
    for(unsigned y=0;y<height;++y){colors[14]=rgb(assets.rules.copper_color14.at(game.level.number-1)[y*176/height]);int wy=int(std::floor(camera_y+y/scale));if(wy<0 || wy>=int(game.level.height_tiles)*16)continue;
        for(unsigned x=0;x<width;++x){int wx=world_x[x];if(wx<0 || wx>=4096)continue;unsigned tile=animated_tile(game.level,game.level.map[unsigned(wy/16)*256+unsigned(wx/16)],game.frame);const auto& image=bricks.at(tile);frame.pixels[std::size_t(y)*width+x]=colors.at(image.pixels[unsigned(wy&15)*16+unsigned(wx&15)]);}
    }
    auto interpolate=[&](int now,int before){return std::abs(now-before)<=32?before+(now-before)*alpha:double(now);};
    auto draw=[&](const std::vector<Image>& bank,unsigned index,unsigned global,double x,double y,bool flip,bool player=false){
        const auto& image=bank.at(index);const auto& anchor=assets.rules.sprites.at(global);double left=x-camera_x-(player?16:(flip?int(image.width)-anchor.anchor_x:anchor.anchor_x)),top=y-camera_y-(player?int(image.height):anchor.anchor_y)+1;
        int min_x=std::max(0,int(std::ceil(left*scale))),max_x=std::min(int(width),int(std::ceil((left+image.width)*scale))),min_y=std::max(0,int(std::ceil(top*scale))),max_y=std::min(int(height),int(std::ceil((top+image.height)*scale)));
        for(int dy=min_y;dy<max_y;++dy){unsigned sy=std::min(image.height-1,unsigned(std::max(0.,std::floor(dy/scale-top))));for(int dx=min_x;dx<max_x;++dx){unsigned sx=std::min(image.width-1,unsigned(std::max(0.,std::floor(dx/scale-left))));if(flip)sx=image.width-1-sx;unsigned color=image.pixels[std::size_t(sy)*image.width+sx];if(color)frame.pixels[std::size_t(dy)*width+unsigned(dx)]=(!player && color==14?rgb(assets.rules.copper_color14.at(game.level.number-1)[unsigned(dy)*176/height]):(player?player_colors:colors).at(color));}}
    };
    auto object=[&](const ObjectBody& body,const ObjectBody& before){if(!body.kind || body.kind>assets.objects.size())return;draw(assets.objects,body.kind-1,30+body.kind-1,body.kind==before.kind?interpolate(body.feet.x,before.feet.x):double(body.feet.x),body.kind==before.kind?interpolate(body.feet.y,before.feet.y):double(body.feet.y),body.vx<0);};
    for(unsigned i=0;i<game.platforms.size();++i)object(game.platforms[i].body,i<previous.platforms.size()?previous.platforms[i].body:game.platforms[i].body);
    for(unsigned i=0;i<40;++i)object(game.objects.objects[i],previous.objects.objects[i]);
    for(unsigned i=0;i<4;++i)object(game.objects.smoke[i],previous.objects.smoke[i]);
    for(unsigned i=0;i<game.enemies.enemies.size();++i){const auto& body=game.enemies.enemies[i];if(!body.get(0) || body.get(0)==0x55ab)continue;const auto& before=i<previous.enemies.enemies.size()?previous.enemies.enemies[i]:body;unsigned index=(body.get(0)-1)&8191;draw(assets.enemies,index,101+index,interpolate(body.signed_get(2),before.signed_get(2)),interpolate(body.signed_get(4),before.signed_get(4)),body.signed_get(6)<0);}
    for(unsigned i=0;i<4;++i)object(game.actors.projectiles[i],previous.actors.projectiles[i]);
    if(game.phase!=GamePhase::ending && game.phase!=GamePhase::game_over && (!game.actors.invulnerability || !(game.frame&2)))draw(assets.fox,game.player.frame,game.player.frame,interpolate(game.player.feet.x,previous.player.feet.x),interpolate(game.player.feet.y,previous.player.feet.y),!game.player.facing_right,true);
    return frame;
}

RgbFrame ending_frame(const GameState& game,const GameAssets& assets,unsigned tick){
    auto frame=game_frame(game,assets,game.camera);auto palette=assets.rules.palette;
    if(game.level.number<=16)palette[14]=assets.rules.level_color14[game.level.number-1];
    auto draw=[&](const Image& image,int left,int top){for(unsigned y=0;y<image.height;++y)for(unsigned x=0;x<image.width;++x){int dx=left+int(x),dy=top+int(y);if(dx<0 || dy<0 || dx>=320 || dy>=176)continue;unsigned color=image.pixels[std::size_t(y)*image.width+x];if(color)frame.pixels[std::size_t(dy)*320+unsigned(dx)]=rgb(color==14?assets.rules.copper_color14.at(game.level.number-1)[unsigned(dy)]:palette.at(color));}};
    auto character=[&](unsigned index,int x,int y){const auto& image=assets.kiss.at(index);draw(image,x-int(image.width/2),y-int(image.height));};
    auto word=[&](unsigned address){if(address+2>assets.rules.table_data.size())throw std::runtime_error("Ending animation outside table");return unsigned(assets.rules.table_data[address])*256+assets.rules.table_data[address+1];};
    unsigned progress=0,base=game.phase==GamePhase::ending?2:0;
    if(game.phase==GamePhase::ending){
        if(tick<156){int distance=318-int(tick)*2;character(word(0x56e + ((tick+1)&15)*2),160-distance,170);character(word(0x58e + ((tick+1)%24)*2),160+distance,170);}
        else{character(10,160,170);unsigned after=tick-156;
            if(after<16)character(11+after/2,160,150);
            else{unsigned phase=((after-16+1)%24)/2;character(9,int(word(0x5be + phase*4)),int(word(0x5c0+phase*4)));progress=std::min(200u,(after-16)*2);}
        }
    }else progress=std::min(200u,(tick+1)*2);
    const auto& left=assets.ending.at(base);const auto& right=assets.ending.at(base+1);
    draw(left,160-int(left.width)-200+int(progress),base==2?57:64);draw(right,360-int(progress),64);return frame;
}

}
