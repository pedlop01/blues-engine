// SPDX-License-Identifier: GPL-3.0-or-later
#include "titus/fox_amiga_game.hpp"
#include "titus/catalog.hpp"
#include <stdexcept>
namespace titus::fox_amiga {
using Json=nlohmann::json;
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Point,x,y)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Body,feet,vx,vy,state,frame,walk_phase,jump_phase,idle_ticks,ice_shift,facing_right,crouch_jump_ticks,lock_ticks,return_state,hit_frame,energy,death_limit,restart_requested)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ObjectBody,kind,feet,vx,vy,reserved,fall_limit,counter,held,flags,width,height)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(EnemyBody,words)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlatformBody,body,direction,speed,source_range,range,counter,source_direction,origin)
namespace {
Json checked_document(const Bytes& bytes){
    if(bytes.empty() || bytes.size()>2*1024*1024)throw std::runtime_error("Invalid Amiga save size");
    auto doc=Json::parse(bytes.begin(),bytes.end());
    if(!doc.is_object() || (doc.size()!=5 && doc.size()!=6) || doc.at("schema_version")!=1 || doc.at("engine")!="fox-amiga-native" || !doc.at("state").is_object() || doc.at("sha256")!=titus::sha256(doc.at("state").dump()))throw std::runtime_error("Invalid Amiga save document");
    if(doc.size()==6 && (!doc.contains("preview_sha256") || !doc.at("preview_sha256").is_string() || doc.at("preview_sha256").get<std::string>().size()!=64 || doc.at("preview_sha256").get<std::string>().find_first_not_of("0123456789abcdef")!=std::string::npos))throw std::runtime_error("Invalid Amiga preview checksum");
    return doc;
}
void require(bool value){if(!value)throw std::runtime_error("Invalid Amiga save state");}
void point(Point p){require(p.x>=-32768 && p.x<=32767 && p.y>=-32768 && p.y<=32767);}
void object(const ObjectBody& o){point(o.feet);require(!((o.kind==10 || o.kind==26) && o.height<5));require(o.kind<=71 && o.vx>=-32768 && o.vx<=32767 && o.vy>=-32768 && o.vy<=32767 && o.reserved<=65535 && o.fall_limit<=65535 && o.counter<=65535 && o.held<=1 && o.flags<=65535 && o.width<=512 && o.height<=512);}
}
unsigned saved_game_level(const Bytes& bytes){auto state=checked_document(bytes).at("state");unsigned n=state.at("level");require(n>=1 && n<=16 && n!=15);return n;}
Bytes save_game(const GameState& game,const std::string& fingerprint){
    Json objects={{"objects",game.objects.objects},{"attachment",game.objects.attachment},{"special_item",game.objects.special_item},{"action_latched",game.objects.action_latched},{"contact_cooldown",game.objects.contact_cooldown},{"sound_event",game.objects.sound_event},{"smoke",game.objects.smoke},{"impact_list",game.objects.impact_list},{"carried",game.objects.carried?Json(*game.objects.carried):Json(nullptr)}};
    Json enemies={{"enemies",game.enemies.enemies},{"boss_power",game.enemies.boss_power},{"hit_cooldown",game.enemies.hit_cooldown},{"attachment",game.enemies.attachment},{"carried",game.enemies.carried?Json(*game.enemies.carried):Json(nullptr)}};
    Json actors={{"frame",game.actors.frame},{"invulnerability",game.actors.invulnerability},{"sound_event",game.actors.sound_event},{"camera",game.actors.camera},{"invulnerable",game.actors.invulnerable},{"projectiles",game.actors.projectiles},{"attacks",game.actors.attacks},{"visible_enemies",game.actors.visible_enemies}};
    Json bonuses=Json::array();
    for(const auto& bonus:game.level.bonuses)bonuses.push_back(bonus.active);
    Json state={{"level",game.level.number},{"map",game.level.map},{"bonuses",bonuses},{"player",game.player},{"objects",objects},{"enemies",enemies},{"platforms",game.platforms},{"actors",actors},{"camera",game.camera},{"checkpoint",game.checkpoint},{"frame",game.frame},{"lives",game.lives},{"bonus_credits",game.bonus_credits},{"completion_ticks",game.completion_ticks},{"completion_spent",game.completion_spent},{"gate_cooldown",game.gate_cooldown},{"gate_flags",game.gate_flags},{"message_level",game.message_level},{"view_step",game.view_step},{"bonus_delay",game.bonus_delay},{"down_ticks",game.down_ticks},{"invulnerable",game.invulnerable},{"infinite_lives",game.infinite_lives},{"phase",int(game.phase)}};
    Json doc={{"schema_version",1},{"engine","fox-amiga-native"},{"fingerprint",fingerprint},{"state",state},{"sha256",titus::sha256(state.dump())}};
    auto text=doc.dump();return Bytes(text.begin(),text.end());
}
GameState restore_game(const Bytes& bytes,const LevelAssets& assets,const NativeRules& rules,const std::string& fingerprint){
    auto doc=checked_document(bytes);require(doc.at("fingerprint")==fingerprint);const auto& s=doc.at("state");require(s.at("level")==assets.initial.number);
    auto game=start_game(assets,rules);require(s.at("map").is_array() && s.at("map").size()==game.level.map.size());
    for(std::size_t i=0;i<game.level.map.size();++i){const auto& value=s.at("map").at(i);require(value.is_number_unsigned() && value.get<unsigned>()<=255);game.level.map[i]=value.get<std::uint8_t>();}
    require(s.at("bonuses").is_array() && s.at("bonuses").size()==100);
    for(unsigned i=0;i<100;++i){require(s.at("bonuses")[i].is_boolean());game.level.bonuses[i].active=s.at("bonuses")[i];}
    game.player=s.at("player").get<Body>();point(game.player.feet);const auto& b=game.player;
    require(b.vx>=-256 && b.vx<=256 && b.vy>=-256 && b.vy<=256 && b.state<=7 && b.state!=6 && b.frame<30 && b.walk_phase<14 && b.jump_phase<=128 && b.idle_ticks<=65535 && b.ice_shift<=3 && b.crouch_jump_ticks<=100 && b.lock_ticks<=65535 && b.return_state<=7 && b.hit_frame<30 && b.energy<=16);
    const auto& o=s.at("objects");require(o.at("objects").is_array() && o.at("objects").size()==40 && o.at("smoke").is_array() && o.at("smoke").size()==4);game.objects.objects=o.at("objects").get<std::array<ObjectBody,40>>();game.objects.attachment=o.at("attachment").get<Point>();game.objects.special_item=o.at("special_item");game.objects.action_latched=o.at("action_latched");game.objects.contact_cooldown=o.at("contact_cooldown");game.objects.sound_event=o.at("sound_event");game.objects.smoke=o.at("smoke").get<std::array<ObjectBody,4>>();game.objects.impact_list=o.at("impact_list").get<std::vector<unsigned>>();
    if(!o.at("carried").is_null()){unsigned slot=o.at("carried");require(slot<40 && game.objects.objects[slot].kind && game.objects.objects[slot].held==1);game.objects.carried=slot;}
    point(game.objects.attachment);require(game.objects.impact_list.size()<=40 && game.objects.contact_cooldown<=65535 && game.objects.sound_event<=5);
    for(auto slot:game.objects.impact_list)require(slot<40);
    for(const auto& body:game.objects.objects)object(body);
    for(const auto& body:game.objects.smoke)object(body);
    const auto& e=s.at("enemies");require(e.at("enemies").is_array() && e.at("enemies").size()==game.enemies.enemies.size());
    for(const auto& row:e.at("enemies")){require(row.at("words").is_array() && row.at("words").size()==46);for(const auto& word:row.at("words"))require(word.is_number_unsigned() && word.get<unsigned>()<=65535);}
    game.enemies.enemies=e.at("enemies").get<std::vector<EnemyBody>>();game.enemies.boss_power=e.at("boss_power");game.enemies.hit_cooldown=e.at("hit_cooldown");game.enemies.attachment=e.at("attachment").get<Point>();
    if(!e.at("carried").is_null()){unsigned slot=e.at("carried");require(slot<game.enemies.enemies.size() && game.enemies.enemies[slot].get(16)==1);game.enemies.carried=slot;}
    require(!(game.objects.carried && game.enemies.carried));require(game.enemies.boss_power<=65535 && game.enemies.hit_cooldown<=10);point(game.enemies.attachment);
    for(const auto& enemy:game.enemies.enemies){unsigned kind=enemy.get(0);require(!kind || kind==0x55ab || ((kind-1)&8191)<232);require((enemy.get(24)&63)<=18 && enemy.get(16)<=1 && enemy.get(14)<=240 && enemy.get(20)<=512 && enemy.get(22)<=512);auto address=enemy.animation();require(!kind || kind==0x55ab || (address>=0xb0000 && address+2<=0xb0000+rules.table_data.size() && !(address&1)));}
    require(s.at("platforms").is_array() && s.at("platforms").size()==game.platforms.size());game.platforms=s.at("platforms").get<std::vector<PlatformBody>>();
    for(const auto& platform:game.platforms){object(platform.body);point(platform.origin);require(platform.direction<=255 && platform.speed<=255 && platform.counter<=65535 && platform.range<=65535);}
    const auto& a=s.at("actors");require(a.at("projectiles").is_array() && a.at("projectiles").size()==4);game.actors.frame=a.at("frame");game.actors.invulnerability=a.at("invulnerability");game.actors.sound_event=a.at("sound_event");game.actors.camera=a.at("camera").get<Point>();game.actors.invulnerable=a.at("invulnerable");game.actors.projectiles=a.at("projectiles").get<std::array<ObjectBody,4>>();game.actors.attacks=a.at("attacks").get<std::vector<unsigned>>();game.actors.visible_enemies=a.at("visible_enemies").get<std::vector<unsigned>>();
    require(game.actors.attacks.size()<=50 && game.actors.visible_enemies.size()<=50 && game.actors.invulnerability<=65535 && game.actors.sound_event<=5);point(game.actors.camera);
    for(auto index:game.actors.attacks)require(index<game.enemies.enemies.size());
    for(auto index:game.actors.visible_enemies)require(index<game.enemies.enemies.size());
    for(const auto& shot:game.actors.projectiles)object(shot);
    game.camera=s.at("camera").get<Point>();game.checkpoint=s.at("checkpoint").get<Point>();point(game.checkpoint);require(game.checkpoint.x>=0 && game.checkpoint.x<4096 && game.checkpoint.y>=0 && game.checkpoint.y<int(game.level.height_tiles)*16);require(game.camera.x>=0 && game.camera.x<=3776 && game.camera.y>=0 && game.camera.y<=std::max(0,int(game.level.height_tiles)*16-176));
    game.frame=s.at("frame");game.lives=s.at("lives");game.bonus_credits=s.at("bonus_credits");game.completion_ticks=s.at("completion_ticks");game.completion_spent=s.at("completion_spent");game.gate_cooldown=s.at("gate_cooldown");game.gate_flags=s.at("gate_flags");game.message_level=s.at("message_level");game.view_step=s.at("view_step");game.bonus_delay=s.at("bonus_delay");game.down_ticks=s.at("down_ticks");game.invulnerable=s.at("invulnerable");game.infinite_lives=s.at("infinite_lives");int phase=s.at("phase");require(game.lives<=99 && game.bonus_credits<=99 && game.completion_spent<10 && game.bonus_credits+game.completion_spent<=99 && (!game.completion_spent || game.bonus_credits>=10-game.completion_spent) && game.gate_cooldown<=20 && game.gate_flags<=255 && game.message_level<=16 && game.message_level!=15 && game.view_step<=8 && game.bonus_delay<=4 && game.down_ticks<=255 && phase>=0 && phase<=3);game.phase=GamePhase(phase);return game;
}
}
