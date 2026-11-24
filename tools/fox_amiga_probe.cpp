// SPDX-License-Identifier: GPL-3.0-or-later
// Diagnostic viewer/exporter. Not registered as a playable engine.
#include "titus/fox_amiga.hpp"
#include "titus/fox_amiga_audio.hpp"
#include "titus/fox_amiga_objects.hpp"
#include "titus/fox_amiga_rules.hpp"
#include "titus/catalog.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
using namespace titus::fox_amiga;
using Json=nlohmann::json;
namespace fs=std::filesystem;
Bytes read(const fs::path& p) {
    std::ifstream f(titus::filesystem_path(p),std::ios::binary|std::ios::ate);
    if(!f || f.tellg()<0 || f.tellg()>4*1024*1024) throw std::runtime_error("Cannot read resource: "+p.filename().string());
    Bytes d(std::size_t(f.tellg()));f.seekg(0);if(!f.read(reinterpret_cast<char*>(d.data()),std::streamsize(d.size()))) throw std::runtime_error("Resource read failed");return d;
}
fs::path resolve(const fs::path& root,const std::string& name) {
    fs::path result;
    for(const auto& entry:fs::directory_iterator(titus::filesystem_path(root))) {
        auto a=entry.path().filename().string(),b=name;
        for(auto& c:a)c=char(std::tolower(static_cast<unsigned char>(c)));
        for(auto& c:b)c=char(std::tolower(static_cast<unsigned char>(c)));
        if(a==b && titus::path_is_regular_file(entry.path())) {if(!result.empty())throw std::runtime_error("Ambiguous resource name");result=entry.path();}
    }
    if(result.empty())throw std::runtime_error("Missing resource: "+name);
    return result;
}
void bmp(const fs::path& p,const Image& im,const std::array<std::uint32_t,64>& pal) {
    std::ofstream f(titus::filesystem_path(p),std::ios::binary);const unsigned size=54+im.width*im.height*4;
    auto put=[&](unsigned v,unsigned bytes){for(unsigned i=0;i<bytes;++i)f.put(char(v>>(8*i)));};
    f.write("BM",2);put(size,4);put(0,4);put(54,4);put(40,4);put(im.width,4);put(im.height,4);
    put(1,2);put(32,2);put(0,4);put(size-54,4);put(0,4);put(0,4);put(0,4);put(0,4);
    for(unsigned row=im.height;row>0;--row)for(unsigned x=0;x<im.width;++x)put(pal.at(im.pixels[(row-1)*im.width+x]),4);
    if(!f)throw std::runtime_error("Cannot write preview");
}
Level physics_fixture(Level level,const Json& script){
    if(!script.contains("fixture"))return level;
    const auto& f=script["fixture"];unsigned row=f.at("floor_row"),end=f.value("floor_end_column",256u);
    if(row>=level.height_tiles || end>256)throw std::runtime_error("Invalid physics floor");
    level.map.assign(level.map.size(),0);level.floor.fill(0);level.horizontal.fill(0);level.ceiling.fill(0);
    unsigned floor=f.value("floor_value",1u);if(floor>255)throw std::runtime_error("Invalid physics floor flags");
    level.floor[1]=std::uint8_t(floor);level.horizontal[2]=level.floor[2]=level.ceiling[2]=1;
    for(unsigned x=0;x<end;++x)level.map[row*256+x]=1;
    for(const auto& col:f.value("wall_columns",Json::array())){unsigned x=col;if(x>=256)throw std::runtime_error("Invalid physics wall");for(unsigned y=0;y<level.height_tiles;++y)level.map[y*256+x]=2;}
    if(f.contains("ceiling_row")){unsigned y=f["ceiling_row"];if(y>=level.height_tiles)throw std::runtime_error("Invalid physics ceiling");for(unsigned x=0;x<256;++x)level.map[y*256+x]=2;}
    return level;
}
ObjectRules read_object_rules(const Json& reference){
    if(!reference.contains("object_rules") || reference["object_rules"].size()!=71)throw std::runtime_error("Regenerate object rules");
    ObjectRules rules;
    for(unsigned i=0;i<71;++i){const auto& r=reference["object_rules"][i];rules.kinds[i]={r.at("flags"),r.at("fall_limit"),r.at("width"),r.at("height"),r.value("anchor_x",-1),r.value("anchor_y",-1)};}
    return rules;
}
ObjectWorld read_objects(const Level& level,const Json& script,const Json& reference){
    const auto rules=read_object_rules(reference);
    auto source=level;
    if(script.contains("objects")){
        for(auto& o:source.objects){o.kind=0;o.position={0,0};}
        std::array<bool,40> seen{};
        for(const auto& row:script["objects"]){unsigned slot=row.at("slot");if(slot>=40 || seen[slot])throw std::runtime_error("Invalid object fixture slot");seen[slot]=true;
            source.objects[slot].kind=row.at("kind");source.objects[slot].position={row.at("x"),row.at("y")};}
    }
    auto world=initialize_objects(source,rules);
    if(script.contains("objects"))for(const auto& row:script["objects"]){auto& o=world.objects[row.at("slot").get<unsigned>()];
        o.vx=row.value("vx",o.vx);o.vy=row.value("vy",o.vy);o.flags=row.value("flags",o.flags);o.counter=row.value("counter",o.counter);o.reserved=row.value("reserved",o.reserved);
    }
    world.contact_cooldown=script.value("cooldown",0u);
    if(script.contains("carried")){unsigned slot=script["carried"];if(slot>=40)throw std::runtime_error("Invalid carried fixture slot");world.carried=slot;world.objects[slot].held=1;}
    if(script.contains("attachment"))world.attachment={script["attachment"].at(0),script["attachment"].at(1)};
    world.action_latched=script.value("action_latched",false);dynamic_object_projection(world);return world;
}
Json object_snapshot(const ObjectWorld& world){
    std::ostringstream hex;for(auto v:dynamic_object_projection(world))hex<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(v);
    return {{"objects",hex.str()},{"state",Json::array({world.carried?int(*world.carried):-1,world.attachment.x,world.attachment.y,int(world.special_item),world.contact_cooldown,world.sound_event,int(world.action_latched)})}};
}
int probe_main(int argc,char** argv) try {
    std::string audio_name;unsigned audio_ticks=1000,rules_only=0;
    fs::path root,output,palette,motion_script,object_script;unsigned level=1,x=0,y=0,spawn_view=0;
    auto number=[](const std::string& value) {
        if(value.empty() || value.find_first_not_of("0123456789")!=std::string::npos)
            throw std::runtime_error("Expected a nonnegative integer");
        const auto n=std::stoul(value);
        if(n>65535)throw std::runtime_error("Numeric option out of range");
        return unsigned(n);
    };
    for(int i=1;i<argc;++i) {
        const std::string key=argv[i];
        if(i+1>=argc)throw std::runtime_error("Expected option value");
        const std::string value=argv[++i];
        if(key=="--datapath")root=fs::u8path(value);
        else if(key=="--output")output=fs::u8path(value);
        else if(key=="--palette")palette=fs::u8path(value);
        else if(key=="--audio")audio_name=value;
        else if(key=="--rules-only")rules_only=number(value);
        else if(key=="--audio-ticks")audio_ticks=number(value);
        else if(key=="--objects")object_script=fs::u8path(value);
        else if(key=="--motion")motion_script=fs::u8path(value);
        else if(key=="--level")level=number(value);
        else if(key=="--x")x=number(value);
        else if(key=="--y")y=number(value);
        else if(key=="--spawn-view")spawn_view=number(value);
        else throw std::runtime_error("Unknown option: "+key);
    }
    if((!object_script.empty() && !motion_script.empty()) || (!audio_name.empty() && (!object_script.empty() || !motion_script.empty())))throw std::runtime_error("Choose one diagnostic script mode");
    if(root.empty() || output.empty() || level<1 || level>16 || level==15 || spawn_view>1 || rules_only>1)
        throw std::runtime_error("Usage: titus-fox-amiga-probe --datapath FOLDER --output PRIVATE_FOLDER --palette REFERENCE_JSON [--level 1..14/16] [--x PIXEL --y PIXEL] [--spawn-view 0/1] [--motion SCRIPT | --objects SCRIPT]; audio: --datapath FOLDER --output PRIVATE_FOLDER --audio NAME [--audio-ticks N]");
    auto canonical_root=titus::normalized_path(root), canonical_output=titus::normalized_path(output);
    root=canonical_root;output=canonical_output;
    auto relative=canonical_output.lexically_relative(canonical_root);
    if(relative.empty() || *relative.begin()!="..")throw std::runtime_error("Output must be outside original data directory");
    if(titus::path_exists(output))throw std::runtime_error("Use a new output directory to preserve previous exports");
    Json report={{"schema_version",1},{"playable",false},{"resources",Json::array()}};
    if(!audio_name.empty()){
        if(!audio_ticks || audio_ticks>60000)throw std::runtime_error("Invalid audio tick count");
        auto song=pat_song(unpack(read(resolve(root,audio_name))),[&](const std::string& name){return read(resolve(root,name));});
        PatPlayer player(song);Json frames=Json::array();
        for(unsigned n=0;n<audio_ticks;++n){
            player.tick();Json row=Json::array({player.order(),player.row(),player.sub_tick(),player.speed()});
            for(const auto& c:player.channels()){row.push_back(c.period);row.push_back(c.volume);row.push_back(c.base_period);}
            frames.push_back(row);
        }
        PatPlayer pcm_player(song);auto pcm=pcm_player.render(441000);
        Bytes raw;raw.reserve(pcm.size()*2);unsigned nonzero=0,peak=0;
        for(auto sample:pcm){unsigned v=unsigned(sample)&65535;raw.push_back(std::uint8_t(v));raw.push_back(std::uint8_t(v>>8));nonzero+=sample!=0;peak=std::max(peak,unsigned(std::abs(int(sample))));}
        report["audio"]={{"name",audio_name},{"ticks",audio_ticks},{"trace_sha256",titus::sha256(frames.dump())},
            {"pcm_sha256",titus::sha256(std::string(raw.begin(),raw.end()))},{"pcm_frames",441000},{"nonzero",nonzero},{"peak",peak},{"loops",player.loops()}};
        titus::ensure_directory(output);
        std::ofstream trace(titus::filesystem_path(output/"audio.json"));trace<<frames.dump()<<'\n';if(!trace)throw std::runtime_error("Cannot write audio trace");
        std::ofstream wav(titus::filesystem_path(output/"audio.wav"),std::ios::binary);
        auto put=[&](unsigned n,unsigned count){for(unsigned k=0;k<count;++k)wav.put(char(n>>(8*k)));};
        wav.write("RIFF",4);put(unsigned(raw.size())+36,4);wav.write("WAVEfmt ",8);put(16,4);put(1,2);put(2,2);put(44100,4);put(176400,4);put(4,2);put(16,2);wav.write("data",4);put(unsigned(raw.size()),4);wav.write(reinterpret_cast<const char*>(raw.data()),std::streamsize(raw.size()));if(!wav)throw std::runtime_error("Cannot write audio preview");
        std::ofstream file(titus::filesystem_path(output/"report.json"));file<<report.dump(2)<<'\n';if(!file)throw std::runtime_error("Cannot write audio report");std::cout<<report.dump()<<'\n';return 0;
    }
    Json reference;
    if(palette.empty()){
        const auto native=native_rules(read(resolve(root,"main")));
        reference={{"main_sha256","18ca46dd02a3ad3623b03d425f0e0ae489b5090498882aea6d2578161010bc45"},{"palette",native.palette},{"level_color14",native.level_color14},
            {"walk_frames",native.motion.walk_frames},{"jump_impulses",native.motion.jump_impulses},{"enemy_animation_frames",native.level.enemy_animation_frames},{"object_rules",Json::array()}};
        for(const auto& rule:native.objects.kinds)reference["object_rules"].push_back({{"flags",rule.flags},{"fall_limit",rule.fall_limit},{"width",rule.width},{"height",rule.height},{"anchor_x",rule.anchor_x},{"anchor_y",rule.anchor_y}});
        report["native_section_sha256"]=native.section_sha256;
        if(rules_only){
            titus::ensure_directory(output);std::ofstream file(titus::filesystem_path(output/"rules.json"));file<<reference.dump(2)<<'\n';if(!file)throw std::runtime_error("Cannot write native rules");
            report["rules_sha256"]=titus::sha256(reference.dump());std::ofstream result(titus::filesystem_path(output/"report.json"));result<<report.dump(2)<<'\n';if(!result)throw std::runtime_error("Cannot write rules report");std::cout<<report.dump()<<'\n';return 0;
        }
    }else{std::ifstream input(titus::filesystem_path(palette));input>>reference;}
    std::vector<fs::path> files;
    for(const auto& e:fs::directory_iterator(titus::filesystem_path(root)))if(titus::path_is_regular_file(e.path())) files.push_back(e.path());
    std::sort(files.begin(),files.end());
    for(const auto& p:files) {
        auto raw=read(p);
        if(raw.size()<6 || raw[1]!=0 || (raw[0]!=1 && raw[0]!=2)) continue;
        const auto decoded=unpack(raw);
        const std::string bytes(decoded.begin(),decoded.end());
        report["resources"].push_back({{"name",p.filename().string()},{"method",raw[0]},
            {"decoded_size",decoded.size()},{"decoded_sha256",titus::sha256(bytes)}});
    }
    std::ostringstream suffix;suffix<<std::setw(2)<<std::setfill('0')<<level;
    auto load=[&](const std::string& name){return unpack(read(resolve(root,name)));};
    auto map=load("LEVEL"+suffix.str()+".TBL"),bricks=load("BRIK"+suffix.str()+".BIN"),meta=load("POIL"+suffix.str()+".BIN");
    if(meta.size()!=3060 || map.empty() || map.size()%256)throw std::runtime_error("Invalid level resources");
    if(reference.at("main_sha256")!="18ca46dd02a3ad3623b03d425f0e0ae489b5090498882aea6d2578161010bc45" || reference.at("enemy_animation_frames").size()!=232)
        throw std::runtime_error("Missing or incompatible Amiga initialization rules; regenerate reference metadata");
    LevelRules rules;
    for(unsigned i=0;i<232;++i)rules.enemy_animation_frames[i]=reference["enemy_animation_frames"][i].get<bool>();
    const auto state=initialize_level(level,map,meta,rules);
    if(!object_script.empty()){
        if(titus::path_file_size(object_script)>1024*1024)throw std::runtime_error("Object script too large");
        std::ifstream stream(titus::filesystem_path(object_script));Json script;stream>>script;
        if(!script.at("steps").is_array() || script["steps"].size()>4096)throw std::runtime_error("Invalid object steps");
        auto world=read_objects(state,script,reference);Body player;player.feet=state.spawn;
        ObjectStepContext object_context;const auto object_rules=read_object_rules(reference);
        const bool physics=script.value("physics",false);const auto object_terrain=physics_fixture(state,script);
        auto hex=[](const Bytes& bytes){std::ostringstream s;for(auto v:bytes)s<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(v);return s.str();};
        auto snapshot=[&](){Json r={{"objects",hex(dynamic_object_projection(world))},{"state",Json::array({world.carried?int(*world.carried):-1,world.attachment.x,world.attachment.y,int(world.special_item),world.contact_cooldown,world.sound_event})}};
            if(physics){r["player"]=Json::array({player.feet.x,player.feet.y,player.vx,player.vy,player.state,player.return_state,player.lock_ticks,player.hit_frame,player.walk_phase,player.idle_ticks,object_context.down_ticks});r["impact_list"]=world.impact_list;r["smoke"]=Json::array();for(const auto& smoke:world.smoke)r["smoke"].push_back(Json::array({smoke.kind,smoke.feet.x,smoke.feet.y}));}
            return r;};
        Json trace={{"initial",snapshot()},{"steps",Json::array()}};
        for(const auto& step:script["steps"]){
            player.feet={step.value("x",player.feet.x),step.value("y",player.feet.y)};player.facing_right=step.value("right",player.facing_right);
            player.vx=step.value("vx",player.vx);player.vy=step.value("vy",player.vy);player.state=step.value("state",player.state);player.return_state=step.value("return_state",player.return_state);player.lock_ticks=step.value("lock_ticks",player.lock_ticks);
            const std::string action=step.at("action");
            if(action=="tick"){
                if(!physics)throw std::runtime_error("Physics mode must be explicit");
                object_context.frame=step.value("frame",object_context.frame+1);object_context.camera={step.value("camera_x",object_context.camera.x),step.value("camera_y",object_context.camera.y)};
                object_context.up=step.value("up",false);object_context.down=step.value("down",false);object_context.down_ticks=step.value("down_ticks",object_context.down_ticks);object_context.invulnerability=step.value("invulnerability",object_context.invulnerability);
                step_objects(object_terrain,world,player,object_context,object_rules);
            }else if(action=="pick")pick_object(world,player);
            else if(action=="drop")drop_object(state,world,player);
            else if(action=="throw")throw_object(world,player,step.value("up",false));
            else if(action=="attach"){
                world.attachment={step.value("dx",world.attachment.x),step.value("dy",world.attachment.y)};attach_object(world,player.feet);
            }else throw std::runtime_error("Unknown object action");
            trace["steps"].push_back(snapshot());
        }
        report["objects"]={{"steps",script["steps"].size()},{"sha256",titus::sha256(trace.dump())},{"actors_simulated",physics}};
        titus::ensure_directory(output);
        std::ofstream file(titus::filesystem_path(output/"objects.json"));file<<trace.dump(2)<<'\n';if(!file)throw std::runtime_error("Cannot write object trace");
        std::ofstream summary(titus::filesystem_path(output/"report.json"));summary<<report.dump(2)<<'\n';if(!summary)throw std::runtime_error("Cannot write object report");std::cout<<report.dump()<<'\n';return 0;
    }

    if(spawn_view){
        x=unsigned(std::clamp(state.spawn.x-320,0,4096-640));
        y=unsigned(std::clamp(state.spawn.y-300,0,std::max(0,int(state.height_tiles*16)-400)));
    }
    const auto ts=tiles(bricks);auto viewport=map_view(map,ts,x,y,640,std::min(400u,unsigned(map.size()/256)*16-y));
    std::array<std::uint32_t,64> pal{};
    if(reference.at("palette").size()!=16 || reference.at("level_color14").size()!=16)
        throw std::runtime_error("Invalid reference palette");
    auto convert=[](unsigned v){if(v>4095)throw std::runtime_error("Invalid Amiga palette word");return (((v>>8)&15)*17<<16)|(((v>>4)&15)*17<<8)|((v&15)*17);};
    for(unsigned i=0;i<16;++i)pal[i]=convert(reference["palette"][i].get<unsigned>());
    pal[14]=convert(reference["level_color14"][level-1].get<unsigned>());report["palette"]="original executable RGB12";
    titus::ensure_directory(output);
    bmp(output/"level.bmp",viewport,pal);
    report["level"]={{"number",level},{"width",4096},{"height",map.size()/256*16},{"x",x},{"y",y}};
    auto digest=[](const Bytes& d){return titus::sha256(std::string(d.begin(),d.end()));};
    auto array_digest=[&](const auto& values){return digest(Bytes(values.begin(),values.end()));};
    report["level_state"]={
        {"normalized_map_sha256",digest(state.map)},
        {"horizontal_sha256",array_digest(state.horizontal)},
        {"floor_sha256",array_digest(state.floor)},
        {"ceiling_sha256",array_digest(state.ceiling)},
        {"animation_sha256",array_digest(state.animation)},
        {"dirty_rows_sha256",digest(state.dirty_rows)},
        {"object_projection_sha256",digest(object_projection(state))},
        {"enemy_projection_sha256",digest(enemy_projection(state))},
        {"elevator_projection_sha256",digest(elevator_projection(state))},
        {"gate_projection_sha256",digest(gate_projection(state))},
        {"spawn",{state.spawn.x,state.spawn.y}},{"finish",{state.finish.x,state.finish.y}},
        {"camera_trigger_y",state.camera_trigger_y},{"camera_lock_y",state.camera_lock_y},
        {"gravity_wall_tile",state.gravity_wall_tile},
        {"objects",std::count_if(state.objects.begin(),state.objects.end(),[](const auto& o){return o.kind!=0;})},
        {"enemies",state.enemies.size()},{"elevators",state.elevators.size()},
        {"bonuses",std::count_if(state.bonuses.begin(),state.bonuses.end(),[](const auto& b){return b.active;})},
        {"gates",std::count_if(state.gates.begin(),state.gates.end(),[](const auto& g){return g.active;})}};
    Bytes queries;
    for(unsigned i=0;i<132;++i){
        int qx,qy;
        if(i<128){qx=int(i*37%4096);qy=int(i*53%(state.height_tiles*16));}
        else {qx=(i&1)?4095:0;qy=(i&2)?int(state.height_tiles*16)-1:0;}
        auto c=collision_at(state,qx,qy);
        for(unsigned v:{c.tile,c.horizontal,c.floor,c.ceiling})queries.push_back(std::uint8_t(v));
    }
    report["level_state"]["collision_queries_sha256"]=digest(queries);
    Bytes transitions;unsigned transition_count=0;
    auto word_out=[&](int n){transitions.push_back(std::uint8_t(unsigned(n)>>8));transitions.push_back(std::uint8_t(n));};
    for(const auto& g:state.gates)if(g.active && (g.entrance>>8)>0 && (g.entrance>>8)<=state.height_tiles){
        auto t=gate_transition(state,{int((g.entrance&255)*16+8),int((g.entrance>>8)*16)-1});
        if(!t)throw std::runtime_error("Missing gate transition");
        for(int value:{t->destination.x,t->destination.y,t->camera.x,t->camera.y,int(t->flags)})word_out(value);
        ++transition_count;
    }
    report["level_state"]["gate_transitions_sha256"]=digest(transitions);
    report["level_state"]["gate_transitions"]=transition_count;
    Bytes exits;
    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)for(unsigned blocked=0;blocked<2;++blocked)for(unsigned carried:{0u,27u}){
        Point feet{std::clamp(state.finish.x+dx*16+7,0,4095),std::clamp(state.finish.y+dy*16+7,0,int(state.height_tiles*16)-1)};
        exits.push_back(exit_ready(state,feet,blocked!=0,carried)?1:0);
    }
    report["level_state"]["exit_queries_sha256"]=digest(exits);
    report["level_state"]["next_level"]=next_level(level);
    if(!motion_script.empty()){
        if(titus::path_file_size(motion_script)>1024*1024)throw std::runtime_error("Motion script too large");
        std::ifstream stream(titus::filesystem_path(motion_script));Json script;stream>>script;
        if(!script.at("steps").is_array() || script["steps"].size()>4096)throw std::runtime_error("Invalid motion script");
        if(reference.at("walk_frames").size()!=14 || reference.at("jump_impulses").size()!=5)throw std::runtime_error("Regenerate reference motion rules");
        MotionRules motion_rules;
        for(unsigned n=0;n<14;++n){motion_rules.walk_frames[n]=reference["walk_frames"][n].get<unsigned>();if(motion_rules.walk_frames[n]>=30)throw std::runtime_error("Invalid walking frame");}
        for(unsigned n=0;n<5;++n)motion_rules.jump_impulses[n]=reference["jump_impulses"][n].get<int>();
        auto initial=script.value("initial",Json::object());Body body;body.feet={initial.value("x",state.spawn.x),initial.value("y",state.spawn.y)};
        body.vx=initial.value("vx",0);body.vy=initial.value("vy",0);body.state=initial.value("state",0u);body.ice_shift=initial.value("ice_shift",0u);
        body.facing_right=initial.value("facing_right",false);
        body.jump_phase=initial.value("jump_phase",0u);body.crouch_jump_ticks=initial.value("crouch_jump_ticks",0u);
        body.lock_ticks=initial.value("lock_ticks",0u);body.return_state=initial.value("return_state",0u);body.hit_frame=initial.value("hit_frame",0u);body.energy=initial.value("energy",16u);body.death_limit=initial.value("death_limit",0);
        auto row=[](const Body& b){return Json::array({b.feet.x,b.feet.y,b.vx,b.vy,b.state,b.frame,b.walk_phase,b.jump_phase,b.idle_ticks,b.ice_shift,unsigned(b.facing_right),b.crouch_jump_ticks});};
        Json trace={{"terrain_only",true},{"columns",{"x","y","vx","vy","state","frame","walk_phase","jump_phase","idle_ticks","ice_shift","facing_right","crouch_jump_ticks"}},
                    {"initial",row(body)},{"frames",Json::array()}};
        auto terrain=state;
        if(script.contains("fixture")){
            const auto& fixture=script["fixture"];const unsigned row=fixture.at("floor_row").get<unsigned>(),end=fixture.value("floor_end_column",256u);
            if(row>=terrain.height_tiles || end>256)throw std::runtime_error("Invalid fixture floor row");
            terrain.map.assign(terrain.map.size(),0);terrain.horizontal.fill(0);terrain.floor.fill(0);terrain.ceiling.fill(0);
            unsigned fixture_floor=fixture.value("floor_value",1u);if(fixture_floor>255)throw std::runtime_error("Invalid floor flags");
            terrain.floor[1]=std::uint8_t(fixture_floor);terrain.horizontal[2]=terrain.floor[2]=terrain.ceiling[2]=1;
            for(unsigned col=0;col<end;++col)terrain.map[row*256+col]=1;
            for(const auto& value:fixture.value("wall_columns",Json::array())){
                unsigned col=value.get<unsigned>();if(col>=256)throw std::runtime_error("Invalid fixture wall");
                for(unsigned y=0;y<terrain.height_tiles;++y)terrain.map[y*256+col]=2;
            }
            if(fixture.contains("ceiling_row")){
                unsigned y=fixture["ceiling_row"].get<unsigned>();if(y>=terrain.height_tiles)throw std::runtime_error("Invalid fixture ceiling");
                for(unsigned col=0;col<256;++col)terrain.map[y*256+col]=2;
            }
            if(fixture.contains("ladder")){
                const auto& ladder=fixture["ladder"];unsigned col=ladder.at("column"),first=ladder.at("first_row"),last=ladder.at("last_row");
                if(col>=256 || first>last || last>=terrain.height_tiles)throw std::runtime_error("Invalid fixture ladder");
                terrain.floor[3]=6;terrain.ceiling[3]=2;
                for(unsigned y=first;y<=last;++y)terrain.map[y*256+col]=3;
            }
        }
        if(script.contains("floor_override")){
            auto value=script["floor_override"].get<unsigned>();if(value>255)throw std::runtime_error("Invalid fixture floor");
            terrain.floor.fill(std::uint8_t(value));
        }
        const bool interactions=script.value("object_interactions",false);
        ObjectWorld world;if(interactions){world=read_objects(state,script,reference);trace["objects_initial"]=object_snapshot(world);trace["object_frames"]=Json::array();}
        const bool extended=script.value("extended_player",false);
        MotionContext context;context.camera={script.value("camera_x",0),script.value("camera_y",0)};context.wide_carry=script.value("wide_carry",false);
        auto extra=[](const Body& b){return Json::array({b.lock_ticks,b.return_state,b.hit_frame,b.energy,b.death_limit,int(b.restart_requested)});};
        if(extended){trace["extra_initial"]=extra(body);trace["extra_frames"]=Json::array();}
        Bytes trajectory;
        for(const auto& input:script["steps"]){
            const BodyInput keys{input.value("left",false),input.value("right",false),input.value("up",false),input.value("down",false)};
            body=interactions?object_player_step(terrain,body,keys,motion_rules,world,input.value("action",false),context):terrain_step(terrain,body,keys,motion_rules);
            if(interactions)trace["object_frames"].push_back(object_snapshot(world));
            if(extended)trace["extra_frames"].push_back(extra(body));
            auto raw=body_projection(body);trajectory.insert(trajectory.end(),raw.begin(),raw.end());trace["frames"].push_back(row(body));
        }
        std::ofstream file(titus::filesystem_path(output/"motion.json"));file<<trace.dump(2)<<'\n';if(!file)throw std::runtime_error("Cannot write motion trace");
        report["motion"]={{"terrain_only",true},{"steps",script["steps"].size()},{"sha256",digest(trajectory)}};
        if(extended)report["motion"]["extra_frames_sha256"]=titus::sha256(trace["extra_frames"].dump());
        if(interactions)report["motion"]["object_frames_sha256"]=titus::sha256(trace["object_frames"].dump());
    }
    const auto fox_bank=sprites(load("fox.spr")),object_bank=sprites(load("objet2.spr")),enemy_bank=sprites(load("monster2.spr"));
    auto scene=level_scene(state,ts,fox_bank,object_bank,enemy_bank,x,y,viewport.width,viewport.height);
    bmp(output/"scene.bmp",scene,pal);
    std::array<std::uint32_t,64> collision_palette{};
    for(unsigned i=0;i<8;++i)collision_palette[i]=((i&1)?0xcc0000:0)|((i&2)?0x00cc00:0)|((i&4)?0x0000cc:0);
    bmp(output/"collisions.bmp",collision_view(state,x,y,viewport.width,viewport.height),collision_palette);
    unsigned sprite_total=0;
    for(const auto& name:{"fox.spr","objet2.spr","monster2.spr","fonte.spr","bisou2.spr","theend.spr"}) {
        auto all=sprites(load(name));sprite_total+=unsigned(all.size());
        unsigned cell_w=8,cell_h=8;
        for(const auto& s:all){cell_w=std::max(cell_w,s.width+8);cell_h=std::max(cell_h,s.height+8);}
        const unsigned sheet_w=cell_w*8,sheet_h=unsigned((all.size()+7)/8)*cell_h;
        Image sheet{sheet_w,sheet_h,Bytes(std::size_t(sheet_w)*sheet_h,0)};
        for(std::size_t i=0;i<all.size();++i) {
            const auto& s=all[i];
            for(unsigned row=0;row<s.height;++row)for(unsigned col=0;col<s.width;++col)
                sheet.pixels[(i/8*cell_h+row)*sheet_w+i%8*cell_w+col]=s.pixels[row*s.width+col];
        }
        bmp(output/(std::string(name)+".bmp"),sheet,pal);
        report["sprites"][name]=all.size();
    }
    report["sprite_total"]=sprite_total;
    for(const auto& entry:std::vector<std::pair<std::string,unsigned>>{{"titus.pc1",4},{"titre2.pc1",5},{"present2.pc1",6}}) {
        auto p=picture(load(entry.first),entry.second,entry.second!=6);
        bmp(output/(entry.first+".bmp"),p.image,p.palette);
    }
    std::ofstream f(titus::filesystem_path(output/"report.json"));f<<report.dump(2)<<'\n';if(!f)throw std::runtime_error("Cannot write report");
    std::cout<<report.dump(2)<<'\n';return 0;
} catch(const std::exception& e) {std::cerr<<"Fox Amiga diagnostic: "<<e.what()<<'\n';return 1;}

#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    std::vector<std::string> utf8;std::vector<char*> pointers;
    for(int i=0;i<argc;++i)utf8.push_back(fs::path(argv[i]).u8string());
    for(auto& argument:utf8)pointers.push_back(argument.data());
    return probe_main(argc,pointers.data());
}
#else
int main(int argc,char** argv){return probe_main(argc,argv);}
#endif
