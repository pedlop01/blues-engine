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
Json request,features;fs::path directory;std::ofstream events;
int sequence=0,command_seq=0,test_ms=0;size_t command_offset=0;bool ready=false,returning=false;Uint32 ready_at=0,gameplay_at=0,last_skip=0;bool gameplay=false,gameplay_probe=false,saved_probe=false,cancelled_probe=false,skip_down=false;int probe_step=0;bool rewind_seen=false;
void emit(const char* name,const Json& body=Json::object()){
    Json event={{"schema_version",1},{"kind","event"},{"session_id",request["session_id"]},{"seq",++sequence},{"event",name},{"body",body}};
    events<<event.dump()<<'\n';events.flush();if(!events)throw Error("E_PROTOCOL","Cannot write runtime event");
}
void verify_lock(){auto text=std::getenv("TITUS_PROFILE_LOCK");if(!text||!*text)throw Error("E_PROFILE_LOCK","Runtime must inherit its profile lock");
    auto handle=std::stoll(text);auto profile=fs::u8path(request["paths"]["profile_directory"]["path"].get<std::string>());auto root=profile;for(int i=0;i<5;++i)root=root.parent_path();
    auto group=request["save_compatibility_id"].get<std::string>(),profile_name=profile.filename().u8string();
    if(profile_name!=group&&!(request["kind"]=="inspect_request"&&profile_name.rfind("."+group+".import-",0)==0))throw Error("E_PROFILE_LOCK","Profile compatibility directory mismatch");
    if(profile.parent_path().filename()!="blues"||profile.parent_path().parent_path().filename()!="games"||profile.parent_path().parent_path().parent_path().filename().u8string()!=request["profile_id"]||root/"profiles"!=profile.parent_path().parent_path().parent_path().parent_path())throw Error("E_PROFILE_LOCK","Invalid managed profile hierarchy");
    auto expected=root/"profile-locks"/fs::u8path(request["profile_id"].get<std::string>()+".blues."+request["save_compatibility_id"].get<std::string>()+".lock");
#ifdef _WIN32
    HANDLE native=reinterpret_cast<HANDLE>(static_cast<intptr_t>(handle));DWORD count=GetFinalPathNameByHandleW(native,nullptr,0,FILE_NAME_NORMALIZED);if(!count)throw Error("E_PROFILE_LOCK","Invalid inherited profile handle");
    std::wstring name(count,L'\0');auto n=GetFinalPathNameByHandleW(native,name.data(),count,FILE_NAME_NORMALIZED);if(!n||n>=count)throw Error("E_PROFILE_LOCK","Invalid inherited profile handle");name.resize(n);
    if(name.rfind(L"\\\\?\\UNC\\",0)==0)name=L"\\\\"+name.substr(8);else if(name.rfind(L"\\\\?\\",0)==0)name=name.substr(4);
    auto path=normalized_path(expected.parent_path())/expected.filename();if(_wcsicmp(path.c_str(),name.c_str()))throw Error("E_PROFILE_LOCK","Profile handle identity mismatch");
#else
    struct stat actual{},wanted{};if(fstat(static_cast<int>(handle),&actual)||stat(expected.c_str(),&wanted)||actual.st_dev!=wanted.st_dev||actual.st_ino!=wanted.st_ino||!S_ISREG(actual.st_mode))throw Error("E_PROFILE_LOCK","Profile descriptor identity mismatch");
#endif
}
titus_native_options options(){titus_native_options o{};o.edition=request["edition_id"]=="amiga";
    o.fullscreen=o.music=o.effects=o.camera=o.hud=o.view=o.invulnerable=o.infinite_lives=o.lives1=o.lives2=o.level=o.sound=-1;
    static std::string dos,amiga;dos.clear();amiga.clear();for(const auto& s:request["sources"]){auto& value=s["edition_id"]=="dos"?dos:amiga;value=s["location"]["path"].get<std::string>();}o.dos=dos.empty()?nullptr:dos.c_str();o.amiga=amiga.empty()?nullptr:amiga.c_str();
    if(!request.contains("settings"))return o;
    for(auto group=request["settings"].begin();group!=request["settings"].end();++group)for(auto item=group.value().begin();item!=group.value().end();++item){auto key=item.key();const auto& v=item.value();
        if(key=="fullscreen")o.fullscreen=v.get<bool>();else if(key=="music_volume")o.music=v;else if(key=="effects_volume")o.effects=v;
        else if(key=="camera")o.camera=v=="smooth";else if(key=="hud")o.hud=v=="fixed";else if(key=="view_step")o.view=v;
        else if(key=="invulnerable")o.invulnerable=v.get<bool>();else if(key=="infinite_lives")o.infinite_lives=v.get<bool>();
        else if(key=="lives_player1")o.lives1=v;else if(key=="lives_player2")o.lives2=v;else if(key=="start_level")o.level=v;
        else if(key=="sound_mode")o.sound=v=="dos"?1:v=="amiga"?2:0;
    }return o;
}
Json inspect_saves(){Json slots=Json::array();auto profile=fs::u8path(request["paths"]["profile_directory"]["path"].get<std::string>());
    for(int slot=1;slot<=8;++slot){char name[32];snprintf(name,sizeof name,"saves/slot%02d.bbs",slot);auto path=profile/name;if(!path_exists(path))continue;
        Json item={{"slot",slot},{"file",name},{"format_id","blues.bbs"},{"format_version",nullptr},{"edition_id",nullptr},{"resource_fingerprint",nullptr},{"native_resource_crc32",nullptr},{"payload_sha256",nullptr},{"has_preview",false},{"status","unreadable"}};
        try{auto data=read_document(path,128*1024);item["payload_sha256"]=sha256(data);titus_slot_info info{};titus_native_inspect(reinterpret_cast<const uint8_t*>(data.data()),data.size(),&info);
            item["status"]=info.version==-1?"incompatible":"damaged";if(info.decoded){item["format_version"]=info.version;item["edition_id"]=info.amiga?"amiga":"dos";item["has_preview"]=bool(info.preview);item["status"]=info.compatible?"ready":"incompatible";
                std::ostringstream hex;hex<<std::hex<<std::setfill('0')<<std::setw(8)<<info.resources;item["native_resource_crc32"]=hex.str();
                for(const auto& source:request["sources"])if(source["edition_id"]==item["edition_id"])item["resource_fingerprint"]=source["fingerprint"];
            }
        }catch(const std::exception&){}slots.push_back(item);
    }
    return {{"schema_version",1},{"kind","save_index"},{"profile_id",request["profile_id"]},{"game_id","blues"},{"engine_id","blues-family"},{"save_compatibility_id",request["save_compatibility_id"]},{"slot_count",8},{"slots",slots}};
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
        validate_blues_request(request,file);verify_lock();features=blues_features(request["sources"].size()==2);set_storage();
        auto native=options();titus_native_options_init(&native,1);
        if(inspecting){Json result={{"schema_version",1},{"kind","inspection"},{"inspection_id",request["inspection_id"]},{"profile_id",request["profile_id"]},{"game_id","blues"},{"engine_id","blues-family"},{"save_compatibility_id",request["save_compatibility_id"]},{"status","ok"},{"features",features},{"save_slot_count",8},{"preparation_ids",Json::array()},{"save_index",inspect_saves()},{"error",nullptr}};write_document(directory/"inspection.json",result);return 0;}
        events.open(filesystem_path(directory/"events.jsonl"),std::ios::binary|std::ios::app);emit("hello",{{"engine_id","blues-family"},{"build_revision",TITUS_BUILD_REVISION},{"protocol",1}});emit("progress",{{"phase","initialize"},{"percent",90}});
        auto probe=std::getenv("TITUS_TEST_GAMEPLAY");gameplay_probe=probe&&std::string(probe)=="1";
        auto test=std::getenv("TITUS_TEST_RUNTIME_MS");if(test)test_ms=std::max(0,std::min(10000,std::atoi(test)));
        std::string fault;auto fault_env=std::getenv("TITUS_TEST_FAULT");if(test_ms&&fault_env)fault=fault_env;
        if(fault=="before_ready")throw Error("E_TEST_LOAD","Deliberate failure before ready");
        if(fault=="wrong_session"){Json wrong={{"schema_version",1},{"kind","event"},{"session_id",uuid()},{"seq",3},{"event","ready"},{"body",{{"features",features},{"scene","title"},{"save_slot_count",8},{"audio_active",true}}}};events<<wrong.dump()<<'\n';events.flush();return 0;}
        if(fault=="log_flood")std::cout<<std::string(2*1024*1024,'x')<<std::flush;
        SDL_SetMainReady();int code=titus_native_run();emit("closing");
        if(gameplay_probe){auto index=inspect_saves();Json observations={{"rewind",rewind_seen},{"slots",index["slots"]}};
            auto profile=fs::u8path(request["paths"]["profile_directory"]["path"].get<std::string>());auto data=read_document(profile/"saves/slot01.bbs",128*1024);titus_slot_info info{};titus_native_inspect(reinterpret_cast<const uint8_t*>(data.data()),data.size(),&info);
            observations["amiga"]=bool(info.amiga);observations["camera"]=info.camera;observations["hud"]=info.hud;observations["view_step"]=info.view;write_document(directory/"probe-observation.json",observations);
        }
        if(fault=="missing_result")return 0;
        if(fault=="failed_zero"){write_document(directory/"result.json",{{"schema_version",1},{"kind","result"},{"session_id",request["session_id"]},{"game_id","blues"},{"engine_id","blues-family"},{"outcome","load_failed"},{"ready",ready},{"native_exit_code",0},{"error",{{"code","E_TEST_LOAD"},{"message","Deliberate load failure with exit zero"}}}});return 0;}
        write_document(directory/"result.json",{{"schema_version",1},{"kind","result"},{"session_id",request["session_id"]},{"game_id","blues"},{"engine_id","blues-family"},{"outcome",ready?"returned":"cancelled"},{"ready",ready},{"native_exit_code",code},{"error",nullptr}});return code;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';
        // Failure before validation may target an untrusted path: never write there.
        if(events.is_open())try{Json error={{"code","E_RUNTIME"},{"message",e.what()}};emit("error",error);emit("closing");write_document(directory/"result.json",{{"schema_version",1},{"kind","result"},{"session_id",request["session_id"]},{"game_id","blues"},{"engine_id","blues-family"},{"outcome","runtime_failed"},{"ready",ready},{"native_exit_code",1},{"error",error}});}catch(...){}
        (void)loaded;return 1;
    }
}
}
extern "C" void titus_runtime_probe_rewind(int active){if(active&&gameplay_probe)rewind_seen=true;}
extern "C" void titus_runtime_gameplay(void){if(!gameplay){gameplay=true;gameplay_at=SDL_GetTicks();}}
extern "C" int titus_runtime_probe_save(void){if(gameplay_probe&&gameplay&&!saved_probe&&SDL_GetTicks()-gameplay_at>800){saved_probe=true;return 1;}return 0;}
extern "C" void titus_runtime_ready(const char* scene){if(ready)return;bool audio=SDL_GetAudioStatus()!=SDL_AUDIO_STOPPED;
    if(!audio&&!request["allow_silent_audio"].get<bool>())throw Error("E_AUDIO","No se pudo abrir el dispositivo de audio.");
    ready=true;ready_at=SDL_GetTicks();emit("ready",{{"features",features},{"scene",scene},{"save_slot_count",8},{"audio_active",audio}});
}
extern "C" int titus_runtime_poll(void){
    bound_log(stdout);bound_log(stderr);
    auto push=[](SDL_Keycode key,Uint32 type=SDL_KEYDOWN){SDL_Event event{};event.type=type;event.key.keysym.sym=key;event.key.keysym.scancode=SDL_GetScancodeFromKey(key);SDL_PushEvent(&event);};
    if(test_ms&&gameplay_probe&&ready){
        if(!gameplay&&SDL_GetTicks()-ready_at>15000)throw Error("E_TEST_GAMEPLAY","Could not reach interactive gameplay");
        if(!gameplay&&SDL_GetTicks()-last_skip>=120){last_skip=SDL_GetTicks();skip_down=!skip_down;push(SDLK_SPACE,skip_down?SDL_KEYDOWN:SDL_KEYUP);}
        if(gameplay){auto elapsed=SDL_GetTicks()-gameplay_at;
            if(probe_step==0&&elapsed>100){if(request["sources"].size()==2){push(SDLK_F6);push(SDLK_F6,SDL_KEYUP);}++probe_step;}
            if(probe_step==1&&elapsed>250){push(SDLK_F7);push(SDLK_F7,SDL_KEYUP);++probe_step;}
            if(probe_step==2&&elapsed>400){push(SDLK_MINUS);push(SDLK_MINUS,SDL_KEYUP);++probe_step;}
            if(probe_step==3&&elapsed>1100){push(SDLK_r);++probe_step;}
            if(probe_step==4&&elapsed>1400){push(SDLK_r,SDL_KEYUP);++probe_step;}
        }
    }
    if(test_ms&&ready&&(!gameplay_probe||gameplay)&&SDL_GetTicks()-(gameplay_probe?gameplay_at:ready_at)>=static_cast<Uint32>(test_ms)&&!returning){returning=true;return 1;}
    auto file=directory/"commands.jsonl";if(!path_exists(file))return 0;auto bytes=read_document(file);if(command_offset>bytes.size())throw Error("E_PROTOCOL","Command channel was truncated");
    bool requested=false;
    while(true){auto end=bytes.find('\n',command_offset);if(end==std::string::npos){if(bytes.size()-command_offset>16384)throw Error("E_PROTOCOL","Command line exceeds limit");break;}
        if(end-command_offset>16384)throw Error("E_PROTOCOL","Command line exceeds limit");auto command=parse_json(bytes.substr(command_offset,end-command_offset));command_offset=end+1;
        if(command.size()!=5||command.at("schema_version")!=1||command.at("kind")!="command"||command.at("session_id")!=request["session_id"]||command.at("seq")!=++command_seq)throw Error("E_PROTOCOL","Command identity/sequence mismatch");
        if(command.at("command")!="return_to_library")throw Error("E_UNSUPPORTED_SETTING","Remote pause/resume is unsupported");requested=true;returning=true;
    }return requested;
}
extern "C" void titus_runtime_return_request(int confirmation){if(ready)emit("return_requested",{{"confirmation_required",bool(confirmation)}});
    if(test_ms&&gameplay_probe&&confirmation){SDL_Event event{};
        if(!cancelled_probe){event.type=SDL_KEYUP;event.key.keysym.sym=SDLK_ESCAPE;cancelled_probe=true;SDL_PushEvent(&event);}
        else{event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_DOWN;SDL_PushEvent(&event);event.key.keysym.sym=SDLK_RETURN;SDL_PushEvent(&event);}
    }
}
extern "C" void titus_runtime_return_cancelled(void){emit("return_cancelled");returning=false;}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){std::vector<std::string> args;for(int i=0;i<argc;++i)args.push_back(fs::path(argv[i]).u8string());return run(args);}
#else
int main(int argc,char** argv){return run(std::vector<std::string>(argv,argv+argc));}
#endif
