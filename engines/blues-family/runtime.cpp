#define SDL_MAIN_HANDLED
#include "native.h"
#include "titus/session.hpp"
#include <SDL.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
using namespace titus;
namespace {
int actor_frames=0,baseline_x=0,baseline_y=0,right_max=0,left_start=0,left_min=0,jump_min=0;
SDL_Keycode actor_key=0;SDL_Joystick* virtual_pad=nullptr;
Json request,features;fs::path directory;std::ofstream events;
int sequence=0,command_seq=0,test_ms=0;size_t command_offset=0;bool ready=false,returning=false;Uint32 ready_at=0,gameplay_at=0,last_skip=0;bool gameplay=false,gameplay_probe=false,saved_probe=false,cancelled_probe=false,skip_down=false;int probe_step=0;bool rewind_seen=false,pause_seen=false;
void emit(const char* name,const Json& body=Json::object()){
    Json event={{"schema_version",1},{"kind","event"},{"session_id",request["session_id"]},{"seq",++sequence},{"event",name},{"body",body}};
    events<<event.dump()<<'\n';events.flush();if(!events)throw Error("E_PROTOCOL","Cannot write runtime event");
}
titus_native_options options(){titus_native_options o{};o.game=request["game_id"]=="jukebox"?1:request["game_id"]=="prehistorik2"?2:0;o.probe=std::getenv("TITUS_TEST_GAMEPLAY")&&std::string(std::getenv("TITUS_TEST_GAMEPLAY"))=="1";o.edition=request["edition_id"]=="amiga";
    o.action_button=o.jump_button=-1;o.load_slot=request.contains("action")&&request["action"]["type"]=="load_slot"?request["action"]["slot"].get<int>():0;
    o.fullscreen=o.music=o.effects=o.camera=o.hud=o.view=o.invulnerable=o.infinite_lives=o.lives1=o.lives2=o.level=o.sound=-1;
    static std::string dos,amiga;dos.clear();amiga.clear();for(const auto& s:request["sources"]){auto& value=s["edition_id"]=="dos"?dos:amiga;value=s["location"]["path"].get<std::string>();}o.dos=dos.empty()?nullptr:dos.c_str();o.amiga=amiga.empty()?nullptr:amiga.c_str();
    if(o.game){
        auto config=fs::u8path(request["paths"]["work_directory"]["path"].get<std::string>())/"preferences.json";
        if(path_exists(config)){auto prefs=parse_json(read_document(config,8192));
            if(prefs.value("schema_version",0)!=1)throw Error("E_PREFERENCES","Unsupported preferences version");
            o.fullscreen=prefs.at("fullscreen").get<bool>();o.music=prefs.at("music_volume").get<int>();o.effects=prefs.at("effects_volume").get<int>();
            if(o.music<0||o.music>100||o.effects<0||o.effects>100)throw Error("E_PREFERENCES","Invalid volume");
        }
    }
    if(o.game){auto config=fs::u8path(request["paths"]["work_directory"]["path"].get<std::string>())/"preferences.json";if(path_exists(config)){auto p=parse_json(read_document(config,8192));o.action_button=p.value("action_button",0);o.jump_button=p.value("jump_button",1);}}
    if(!request.contains("settings"))return o;
    for(auto group=request["settings"].begin();group!=request["settings"].end();++group)for(auto item=group.value().begin();item!=group.value().end();++item){auto key=item.key();const auto& v=item.value();
        if(key=="action_button")o.action_button=v;else if(key=="jump_button")o.jump_button=v;else if(key=="fullscreen")o.fullscreen=v.get<bool>();else if(key=="music_volume")o.music=v;else if(key=="effects_volume")o.effects=v;
        else if(key=="camera")o.camera=v=="smooth";else if(key=="hud")o.hud=v=="fixed";else if(key=="view_step")o.view=v;
        else if(key=="invulnerable")o.invulnerable=v.get<bool>();else if(key=="infinite_lives")o.infinite_lives=v.get<bool>();
        else if(key=="lives_player1")o.lives1=v;else if(key=="lives_player2")o.lives2=v;else if(key=="start_level")o.level=v;
        else if(key=="sound_mode")o.sound=v=="dos"?1:v=="amiga"?2:0;
    }return o;
}
Json inspect_saves(){Json slots=Json::array(),details=Json::array();auto profile=fs::u8path(request["paths"]["profile_directory"]["path"].get<std::string>());
    for(int slot=1;slot<=runtime_slot_count(request);++slot){char name[32];snprintf(name,sizeof name,"saves/slot%02d.bbs",slot);auto path=profile/name;if(!path_exists(path))continue;
        Json item={{"slot",slot},{"file",name},{"format_id","blues.bbs"},{"format_version",nullptr},{"edition_id",nullptr},{"resource_fingerprint",nullptr},{"native_resource_crc32",nullptr},{"payload_sha256",nullptr},{"has_preview",false},{"status","unreadable"}};
        try{auto data=read_document(path,128*1024);item["payload_sha256"]=sha256(data);titus_slot_info info{};titus_native_inspect(reinterpret_cast<const uint8_t*>(data.data()),data.size(),&info);
            item["status"]=info.version==-1?"incompatible":"damaged";if(info.decoded){item["format_version"]=info.version;item["edition_id"]=info.amiga?"amiga":"dos";item["has_preview"]=bool(info.preview);item["status"]=info.compatible?"ready":"incompatible";
                Json detail={{"slot",slot},{"payload_sha256",item["payload_sha256"]},{"level",info.level},{"x",info.x},{"y",info.y},{"frame",info.frame},{"camera",info.camera},{"hud",info.hud},{"view",info.view},{"lives1",info.lives1},{"lives2",info.lives2},{"timestamp",info.timestamp},{"preview_rgb",nullptr}};
                uint8_t rgb[128*80*3];if(info.preview&&titus_native_preview(reinterpret_cast<const uint8_t*>(data.data()),data.size(),rgb)){std::string hex;const char* digits="0123456789abcdef";hex.reserve(sizeof(rgb)*2);for(auto b:rgb){hex+=digits[b>>4];hex+=digits[b&15];}detail["preview_rgb"]=hex;}details.push_back(detail);

                std::ostringstream hex;hex<<std::hex<<std::setfill('0')<<std::setw(8)<<info.resources;item["native_resource_crc32"]=hex.str();
                for(const auto& source:request["sources"])if(source["edition_id"]==item["edition_id"])item["resource_fingerprint"]=source["fingerprint"];
            }
        }catch(const std::exception&){}slots.push_back(item);
    }
    write_document(directory/"save-details.json",{{"schema_version",1},{"slots",details}});
    return {{"schema_version",1},{"kind","save_index"},{"profile_id",request["profile_id"]},{"game_id",request["game_id"]},{"engine_id","blues-family"},{"save_compatibility_id",request["save_compatibility_id"]},{"slot_count",runtime_slot_count(request)},{"slots",slots}};
}
void set_storage(){auto work=fs::u8path(request["paths"]["work_directory"]["path"].get<std::string>());auto profile=fs::u8path(request["paths"]["profile_directory"]["path"].get<std::string>());
    auto config=(work/"preferences.ini").u8string(),saves=(profile/"saves").u8string();
    SDL_setenv("BLUES_CONFIG_FILE",config.c_str(),1);SDL_setenv("BLUES_SAVE_DIR",saves.c_str(),1);
}
void bound_log(FILE* stream){
#ifdef _WIN32
    auto position=_ftelli64(stream);
#else
    auto position=ftello(stream);
#endif
    if(position<1024*1024)return;
    fflush(stream);
#ifdef _WIN32
    if(_chsize_s(_fileno(stream),0)!=0)return;
#else
    if(ftruncate(fileno(stream),0)!=0)return;
#endif
    fseek(stream,0,SEEK_SET);fputs("[log rotated after 1 MiB]\n",stream);fflush(stream);
}
int run(const std::vector<std::string>& args){bool inspecting=false;bool loaded=false;
    try{
        if(args.size()!=3||(args[1]!="--session-request"&&args[1]!="--inspect-request"))throw Error("E_ARGUMENT","Use --session-request FILE or --inspect-request FILE");
        inspecting=args[1]=="--inspect-request";auto file=normalized_path(fs::u8path(args[2]));request=parse_json(read_document(file,128*1024));
        if(request.value("kind",std::string())!=(inspecting?"inspect_request":"launch"))throw Error("E_REQUEST","Wrong request kind");
        directory=file.parent_path();loaded=true;
        // Only write response channels once the path and identity have passed validation.
        if(request.at("engine_id")!="blues-family")throw Error("E_GAME","Wrong runtime executable");
        validate_engine_request(request,file);verify_profile_lock(request);features=runtime_features(request["game_id"],request["sources"].size()==2);set_storage();
        auto native=options();titus_native_options_init(&native,1);
        if(inspecting){if(request["game_id"]=="blues")native_settings(request);Json result={{"schema_version",1},{"kind","inspection"},{"inspection_id",request["inspection_id"]},{"profile_id",request["profile_id"]},{"game_id",request["game_id"]},{"engine_id","blues-family"},{"save_compatibility_id",request["save_compatibility_id"]},{"status","ok"},{"features",features},{"save_slot_count",runtime_slot_count(request)},{"preparation_ids",Json::array()},{"save_index",request["game_id"]=="blues"?inspect_saves():Json(nullptr)},{"error",nullptr}};write_document(directory/"inspection.json",result);return 0;}
        events.open(filesystem_path(directory/"events.jsonl"),std::ios::binary|std::ios::app);emit("hello",{{"engine_id","blues-family"},{"build_revision",TITUS_BUILD_REVISION},{"protocol",1}});emit("progress",{{"phase","initialize"},{"percent",90}});
        auto probe=std::getenv("TITUS_TEST_GAMEPLAY");gameplay_probe=probe&&std::string(probe)=="1";
        auto test=std::getenv("TITUS_TEST_RUNTIME_MS");if(test)test_ms=std::max(0,std::min(10000,std::atoi(test)));
        std::string fault;auto fault_env=std::getenv("TITUS_TEST_FAULT");if(test_ms&&fault_env)fault=fault_env;
        if(fault=="before_ready")throw Error("E_TEST_LOAD","Deliberate failure before ready");
        if(fault=="wrong_session"){Json wrong={{"schema_version",1},{"kind","event"},{"session_id",uuid()},{"seq",3},{"event","ready"},{"body",{{"features",features},{"scene","title"},{"save_slot_count",runtime_slot_count(request)},{"audio_active",true}}}};events<<wrong.dump()<<'\n';events.flush();return 0;}
        if(fault=="log_flood")std::cout<<std::string(2*1024*1024,'x')<<std::flush;
        SDL_SetMainReady();if(test_ms&&gameplay_probe&&fault=="controller"){SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");if(SDL_Init(SDL_INIT_GAMECONTROLLER))throw Error("E_TEST_PAD",SDL_GetError());int device=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);if(device<0||(virtual_pad=SDL_JoystickOpen(device))==nullptr)throw Error("E_TEST_PAD",SDL_GetError());}int code=titus_native_run();
        if(request["game_id"]!="blues"){
            int action,jump;titus_native_buttons(&action,&jump);int full,music,effects;titus_native_preferences(&full,&music,&effects);
            write_document(fs::u8path(request["paths"]["work_directory"]["path"].get<std::string>())/"preferences.json",{{"schema_version",1},{"fullscreen",bool(full)},{"music_volume",music},{"effects_volume",effects},{"action_button",action},{"jump_button",jump}});
            if(gameplay_probe){uint64_t samples,audible,clipped;int peak,effects;titus_native_audio_stats(&samples,&audible,&clipped,&peak,&effects);
                write_document(directory/"probe-observation.json",{{"gameplay",gameplay},{"pause",pause_seen},{"frames",actor_frames},{"right",right_max-baseline_x},{"left",left_start-left_min},{"jump",baseline_y-jump_min},{"samples",samples},{"nonzero",audible},{"clipped",clipped},{"peak",peak},{"effects",effects}});}

        }
        emit("closing");
        if(gameplay_probe&&request["game_id"]=="blues"){auto index=inspect_saves();Json observations={{"rewind",rewind_seen},{"slots",index["slots"]}};
            auto profile=fs::u8path(request["paths"]["profile_directory"]["path"].get<std::string>());auto data=read_document(profile/"saves/slot01.bbs",128*1024);titus_slot_info info{};titus_native_inspect(reinterpret_cast<const uint8_t*>(data.data()),data.size(),&info);
            observations["amiga"]=bool(info.amiga);observations["camera"]=info.camera;observations["hud"]=info.hud;observations["view_step"]=info.view;write_document(directory/"probe-observation.json",observations);
        }
        if(fault=="missing_result")return 0;
        if(fault=="failed_zero"){write_document(directory/"result.json",{{"schema_version",1},{"kind","result"},{"session_id",request["session_id"]},{"game_id",request["game_id"]},{"engine_id","blues-family"},{"outcome","load_failed"},{"ready",ready},{"native_exit_code",0},{"error",{{"code","E_ENGINE_INIT"},{"message","Deliberate load failure with exit zero"}}}});return 0;}
        write_document(directory/"result.json",{{"schema_version",1},{"kind","result"},{"session_id",request["session_id"]},{"game_id",request["game_id"]},{"engine_id","blues-family"},{"outcome",ready?"returned":"cancelled"},{"ready",ready},{"native_exit_code",code},{"error",nullptr}});return code;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';
        // Failure before validation may target an untrusted path: never write there.
        if(events.is_open())try{Json error=runtime_error(e,ready);emit("error",error);emit("closing");write_document(directory/"result.json",{{"schema_version",1},{"kind","result"},{"session_id",request["session_id"]},{"game_id",request["game_id"]},{"engine_id","blues-family"},{"outcome","runtime_failed"},{"ready",ready},{"native_exit_code",1},{"error",error}});}catch(...){}
        (void)loaded;return 1;
    }
}
}
extern "C" void titus_runtime_actor(int x,int y){
    if(!gameplay_probe)return;
    int n=actor_frames++;
    if(n==145){SDL_Event e{};e.type=SDL_KEYUP;e.key.keysym.sym=SDLK_p;SDL_PushEvent(&e);}
    if(n==120)titus_native_capture((directory/"gameplay.bmp").u8string().c_str());
    if(n==10){baseline_x=right_max=x;baseline_y=jump_min=y;}
    if(n>=10&&n<40)right_max=std::max(right_max,x);
    if(n==40)left_start=left_min=x;
    if(n>=40&&n<65)left_min=std::min(left_min,x);
    if(n==65)baseline_y=jump_min=y;
    if(n>=65&&n<95)jump_min=std::min(jump_min,y);
    auto desired=n<10?0:n<40?SDLK_RIGHT:n<65?SDLK_LEFT:n<95?SDLK_LSHIFT:n<120?SDLK_SPACE:0;
    if(virtual_pad&&actor_key!=desired){int action,jump;titus_native_buttons(&action,&jump);if(actor_key==SDLK_LSHIFT||actor_key==SDLK_SPACE)SDL_JoystickSetVirtualButton(virtual_pad,actor_key==SDLK_SPACE?action:jump,0);if(desired==SDLK_LSHIFT||desired==SDLK_SPACE)SDL_JoystickSetVirtualButton(virtual_pad,desired==SDLK_SPACE?action:jump,1);}
    if(actor_key!=desired){SDL_Event e{};if(actor_key&&(!virtual_pad||(actor_key!=SDLK_LSHIFT&&actor_key!=SDLK_SPACE))){e.type=SDL_KEYUP;e.key.keysym.sym=actor_key;SDL_PushEvent(&e);}if(desired&&(!virtual_pad||(desired!=SDLK_LSHIFT&&desired!=SDLK_SPACE))){e.type=SDL_KEYDOWN;e.key.keysym.sym=desired;SDL_PushEvent(&e);}actor_key=desired;}
}
extern "C" void titus_runtime_probe_paused(int paused){if(gameplay_probe&&paused&&SDL_GetAudioStatus()==SDL_AUDIO_PAUSED)pause_seen=true;}
extern "C" void titus_runtime_probe_rewind(int active){if(active&&gameplay_probe)rewind_seen=true;}
extern "C" void titus_runtime_gameplay(void){if(!gameplay){gameplay=true;gameplay_at=SDL_GetTicks();}}
extern "C" int titus_runtime_probe_save(void){if(gameplay_probe&&gameplay&&!saved_probe&&SDL_GetTicks()-gameplay_at>800){saved_probe=true;return 1;}return 0;}
extern "C" void titus_runtime_ready(const char* scene){if(ready)return;bool audio=SDL_GetAudioStatus()!=SDL_AUDIO_STOPPED;
    if(!audio&&!request["allow_silent_audio"].get<bool>())throw Error("E_AUDIO","No se pudo abrir el dispositivo de audio.");
    ready=true;ready_at=SDL_GetTicks();emit("ready",{{"features",features},{"scene",scene},{"save_slot_count",runtime_slot_count(request)},{"audio_active",audio}});
}
extern "C" int titus_runtime_poll(void){
    bound_log(stdout);bound_log(stderr);
    auto push=[](SDL_Keycode key,Uint32 type=SDL_KEYDOWN){SDL_Event event{};event.type=type;event.key.keysym.sym=key;event.key.keysym.scancode=SDL_GetScancodeFromKey(key);SDL_PushEvent(&event);};
    if(test_ms&&gameplay_probe&&ready){
        if(!gameplay&&SDL_GetTicks()-ready_at>15000)throw Error("E_TEST_GAMEPLAY","Could not reach interactive gameplay");
        if(!gameplay&&SDL_GetTicks()-last_skip>=120){last_skip=SDL_GetTicks();skip_down=!skip_down;push(SDLK_SPACE,skip_down?SDL_KEYDOWN:SDL_KEYUP);}
        if(gameplay&&request["game_id"]=="blues"){auto elapsed=SDL_GetTicks()-gameplay_at;
            if(probe_step==0&&elapsed>100){if(request["sources"].size()==2){push(SDLK_F6);push(SDLK_F6,SDL_KEYUP);}++probe_step;}
            if(probe_step==1&&elapsed>250){push(SDLK_F7);push(SDLK_F7,SDL_KEYUP);++probe_step;}
            if(probe_step==2&&elapsed>400){push(SDLK_MINUS);push(SDLK_MINUS,SDL_KEYUP);++probe_step;}
            if(probe_step==3&&elapsed>1100){push(SDLK_r);++probe_step;}
            if(probe_step==4&&elapsed>1400){push(SDLK_r,SDL_KEYUP);++probe_step;}
        }
    }
    if(test_ms&&ready&&(!gameplay_probe||gameplay)&&
       (!gameplay_probe||request["game_id"]=="blues"||(actor_frames>=146&&pause_seen))&&SDL_GetTicks()-(gameplay_probe?gameplay_at:ready_at)>=static_cast<Uint32>(test_ms)&&!returning){returning=true;return 1;}
    auto file=directory/"commands.jsonl";if(!path_exists(file))return 0;auto bytes=read_document(file);if(command_offset>bytes.size())throw Error("E_PROTOCOL","Command channel was truncated");
    bool requested=false;
    while(true){auto end=bytes.find('\n',command_offset);if(end==std::string::npos){if(bytes.size()-command_offset>16384)throw Error("E_PROTOCOL","Command line exceeds limit");break;}
        if(end-command_offset>16384)throw Error("E_PROTOCOL","Command line exceeds limit");auto command=parse_json(bytes.substr(command_offset,end-command_offset));command_offset=end+1;
        if(command.size()!=5||command.at("schema_version")!=1||command.at("kind")!="command"||command.at("session_id")!=request["session_id"]||command.at("seq")!=++command_seq)throw Error("E_PROTOCOL","Command identity/sequence mismatch");
        if(command.at("command")!="return_to_library")throw Error("E_UNSUPPORTED_SETTING","Remote pause/resume is unsupported");requested=true;returning=true;
    }return requested;
}
extern "C" void titus_runtime_return_request(int confirmation){if(ready)emit("return_requested",{{"confirmation_required",bool(confirmation)}});
    if(test_ms&&confirmation){SDL_Event event{};
        if(gameplay_probe&&!cancelled_probe){event.type=SDL_KEYUP;event.key.keysym.sym=SDLK_ESCAPE;cancelled_probe=true;SDL_PushEvent(&event);}
        else{event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_DOWN;SDL_PushEvent(&event);event.key.keysym.sym=SDLK_RETURN;SDL_PushEvent(&event);}
    }
}
extern "C" void titus_runtime_return_cancelled(void){emit("return_cancelled");returning=false;}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){std::vector<std::string> args;for(int i=0;i<argc;++i)args.push_back(fs::path(argv[i]).u8string());return run(args);}
#else
int main(int argc,char** argv){return run(std::vector<std::string>(argv,argv+argc));}
#endif

extern "C" void titus_runtime_load_failed(void){throw Error("E_SAVE","Native save validation or restoration failed");}

extern "C" void titus_runtime_restored(int level,int x,int y,int frame,int camera,int hud,int view){titus_runtime_ready("gameplay");write_document(directory/"restored-state.json",{{"level",level},{"x",x},{"y",y},{"frame",frame},{"camera",camera},{"hud",hud},{"view",view}});}
