// SPDX-License-Identifier: GPL-3.0-or-later
#define SDL_MAIN_HANDLED
#include "managed_old.hpp"
#include "managed_io.hpp"
#include "presentation/game_app.hpp"
#include "app/options_build.hpp"
#include "app/cli_args.hpp"
#include "titus/pre1_prepare.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#ifdef TITUS_INPROCESS_PILOT
#include "entry.hpp"
#ifndef _WIN32
#include <sys/resource.h>
#endif
#endif
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
using namespace titus;
void engine_ready(const char*,bool);void engine_tick();void engine_frame(int,int,bool);
unsigned long long samples=0,nonzero=0,clipped=0;int peak=0;
void engine_audio(const Sint16* data,int count){for(int i=0;i<count;++i){int v=data[i],a=std::abs(v);++samples;if(v)++nonzero;if(v==-32768||v==32767)++clipped;peak=std::max(peak,a);}}
void push_key(SDL_Keycode key){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=key;e.key.keysym.scancode=SDL_GetScancodeFromKey(key);SDL_PushEvent(&e);e.type=SDL_KEYUP;SDL_PushEvent(&e);}
void set_key(SDL_Keycode key,bool down){titus_old::synthetic[SDL_GetScancodeFromKey(key)]=down?1:0;}
void capture(){auto ren=titus_old::renderer;int w,h;SDL_GetRendererOutputSize(ren,&w,&h);auto image=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);if(!image)throw Error("E_CAPTURE","Cannot allocate capture");if(SDL_RenderReadPixels(ren,nullptr,image->format->format,image->pixels,image->pitch)||SDL_SaveBMP(image,titus_old::capture_path.c_str())){SDL_FreeSurface(image);throw Error("E_CAPTURE",SDL_GetError());}SDL_FreeSurface(image);}
namespace {
Json request;fs::path session,work;std::ofstream events;
int sequence=0,command_seq=0,test_ms=0,frames=0,bx=0,rx=0,lstart=0,lx=0,by=0,jy=0,key=0;
size_t command_offset=0;bool ready=false,returning=false,probe=false,playing=false,pause_seen=false,paused=false;
auto started=std::chrono::steady_clock::now(),ready_at=started,playing_at=started,last_skip=started;
void emit(const char* name,const Json& body=Json::object()){
    events<<Json({{"schema_version",1},{"kind","event"},{"session_id",request["session_id"]},{"seq",++sequence},{"event",name},{"body",body}}).dump()<<'\n';events.flush();if(!events)throw Error("E_PROTOCOL","Cannot write Prehistorik event");
}
Json inspection(){bool valid=pre1_prepared_valid(request);return {{"schema_version",1},{"kind","inspection"},{"inspection_id",request["inspection_id"]},{"profile_id",request["profile_id"]},{"game_id","prehistorik"},{"engine_id","olduvai"},{"save_compatibility_id",request["save_compatibility_id"]},{"status",valid?"ok":"preparation_required"},{"features",runtime_features("prehistorik",false)},{"save_slot_count",0},{"preparation_ids",valid?Json::array():Json::array({"pre1-diet-v1"})},{"save_index",nullptr},{"error",nullptr}};}
void result(const char* outcome,int code,const Json& error=nullptr){write_document(session/"result.json",{{"schema_version",1},{"kind","result"},{"session_id",request["session_id"]},{"game_id","prehistorik"},{"engine_id","olduvai"},{"outcome",outcome},{"ready",ready},{"native_exit_code",code},{"error",error}});}
void request_quit(){if(!returning){if(ready)emit("return_requested",{{"confirmation_required",false}});returning=true;}SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);}
void bound_log(FILE* file){long pos=ftell(file);if(pos<1024*1024)return;fflush(file);
#ifdef _WIN32
    if(_chsize_s(_fileno(file),0))return;
#else
    if(ftruncate(fileno(file),0))return;
#endif
    fseek(file,0,SEEK_SET);fputs("[log rotated after 1 MiB]\n",file);
}
int run(const std::vector<std::string>& args){try{
    if(args.size()!=3||(args[1]!="--inspect-request"&&args[1]!="--prepare-request"&&args[1]!="--session-request"))throw Error("E_ARGUMENT","Use a managed inspection, preparation or session request");
    auto file=normalized_path(fs::u8path(args[2]));request=parse_json(read_document(file,128*1024));
    if(request.at("game_id")!="prehistorik"||request.at("engine_id")!="olduvai")throw Error("E_GAME","Olduvai runtime only supports Prehistorik DOS");
    bool inspect=args[1]!="--session-request";if(request.at("kind")!=(inspect?"inspect_request":"launch"))throw Error("E_REQUEST","Wrong request kind");
    validate_engine_request(request,file);verify_profile_lock(request);session=file.parent_path();work=fs::u8path(request["paths"]["work_directory"]["path"].get<std::string>());
    if(inspect){if(args[1]=="--prepare-request"){auto fault=environment_value("TITUS_TEST_FAULT");prepare_pre1(request,environment_value("TITUS_TEST_RUNTIME_MS").value_or("0")!="0"&&fault&&*fault=="prepare_fail");}write_document(session/"inspection.json",inspection());return 0;}
    if(!pre1_prepared_valid(request))throw Error("E_PREPARATION","Prehistorik preparation is missing or damaged");
    bool full=false;auto prefs=work/"preferences.json";
    if(path_exists(prefs)){auto p=parse_json(read_document(prefs,8192));if(p.value("schema_version",0)!=1||!p.at("fullscreen").is_boolean())throw Error("E_PREFERENCES","Unsupported Prehistorik preferences");full=p["fullscreen"];}
    if(request["settings"].contains("video"))full=request["settings"]["video"]["fullscreen"];

    events.open(filesystem_path(session/"events.jsonl"),std::ios::binary|std::ios::app);emit("hello",{{"engine_id","olduvai"},{"build_revision",TITUS_BUILD_REVISION},{"protocol",1}});emit("progress",{{"phase","initialize"},{"percent",90}});
    test_ms=std::min(10000,std::max(0,std::atoi(environment_value("TITUS_TEST_RUNTIME_MS").value_or("0").c_str())));probe=environment_value("TITUS_TEST_GAMEPLAY").value_or("0")=="1";
    auto fault=environment_value("TITUS_TEST_FAULT").value_or("");if(test_ms&&fault=="before_ready")throw Error("E_TEST_LOAD","Deliberate Prehistorik load failure");
    olduvai::app::Config cfg=olduvai::app::builtin_profile("dos");
    cfg["render_scale"]="2";cfg["display_mode"]="cpu";cfg["music_device"]="opl";cfg["sfx_backend"]="opl";cfg["audio_rate"]="44100";
    Json stored=path_exists(prefs)?parse_json(read_document(prefs,32768)):Json::object();
    if(stored.contains("settings")){if(!stored["settings"].is_object())throw Error("E_PREFERENCES","Invalid native Prehistorik settings");for(auto it=stored["settings"].begin();it!=stored["settings"].end();++it){if(!it.value().is_string())throw Error("E_PREFERENCES","Native settings must be strings");cfg[it.key()]=it.value().get<std::string>();}}
    olduvai::app::PlaySettings ps;olduvai::app::merge_config(ps,cfg);
    olduvai::app::CliArgs native_args;native_args.game_dir=pre1_prepared_directory(request);native_args.play=true;native_args.play_level=probe?1:0;native_args.play_fullscreen=full;
    olduvai::presentation::GameOptions options;auto built=olduvai::app::build_game_options(native_args,ps,options);if(!built.ok)throw Error("E_PREFERENCES",built.error);
    titus_old::report_root=work/"reports";if(normalized_path(titus_old::report_root)!=titus_old::report_root.lexically_normal())throw Error("E_PATH","Report directory contains an alias");ensure_directory(titus_old::report_root);
    options.save_path.clear();options.persist=[&](const std::string& k,const std::string& v){cfg[k]=v;if(k=="fullscreen")full=v=="1";Json values=Json::object();for(const auto& kv:cfg)values[kv.first]=kv.second;write_document(prefs,{{"schema_version",1},{"fullscreen",full},{"settings",values}});};
    titus_old::on_ready=engine_ready;titus_old::on_tick=engine_tick;titus_old::on_frame=engine_frame;titus_old::on_audio=engine_audio;
    titus_old::capture_path=(session/"gameplay.bmp").u8string();
    started=std::chrono::steady_clock::now();SDL_SetMainReady();
    int code=olduvai::presentation::run_game(options);
    if(!ready&&!returning&&!titus_old::quit_seen&&code==0)throw Error("E_NATIVE_LOAD","Olduvai exited without loading a playable scene");
    if(code){auto e=runtime_error(Error("E_NATIVE","Olduvai reported an error: "+std::to_string(code)),ready);emit("error",e);emit("closing");result("runtime_failed",1,e);return 1;}
    Json values=Json::object();for(const auto& kv:cfg)values[kv.first]=kv.second;write_document(prefs,{{"schema_version",1},{"fullscreen",full},{"settings",values}});
    if(probe){write_document(session/"probe-observation.json",{{"frames",frames},{"right",rx-bx},{"left",lstart-lx},{"jump",by-jy},{"pause",pause_seen},{"samples",samples},{"nonzero",nonzero},{"clipped",clipped},{"peak",peak}});}
    emit("closing");if(test_ms&&fault=="missing_result")return 0;result(ready?"returned":"cancelled",0);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';if(events.is_open())try{Json error=runtime_error(e,ready);emit("error",error);emit("closing");result("runtime_failed",1,error);}catch(...){}return 1;}}
}
void engine_ready(const char* scene,bool audio){if(ready)return;if(!audio)throw Error("E_AUDIO","Prehistorik audio device is not active");ready=true;ready_at=std::chrono::steady_clock::now();
#ifdef TITUS_INPROCESS_PILOT
    if(titus_pilot::on_ready)titus_pilot::on_ready();
#endif
    emit("ready",{{"features",runtime_features("prehistorik",false)},{"scene",scene},{"save_slot_count",0},{"audio_active",true}});}
void engine_playing(){playing=true;playing_at=std::chrono::steady_clock::now();}
void engine_frame(int x,int y,bool is_paused){if(!playing)engine_playing();if(is_paused){pause_seen=true;return;}if(!probe)return;
    int n=frames++;
#ifdef TITUS_INPROCESS_PILOT
    if(n==40){auto fault=environment_value("TITUS_TEST_FAULT").value_or("");if(fault=="pilot_throw")throw Error("E_PILOT","Injected exception after real gameplay");if(fault=="pilot_crash")std::_Exit(86);}
#endif
    if(n==10)bx=rx=x;if(n>=10&&n<40)rx=std::max(rx,x);if(n==40)lstart=lx=x;if(n>=40&&n<65)lx=std::min(lx,x);if(n==65)by=jy=y;if(n>=65&&n<95)jy=std::min(jy,y);
    int desired=n<10?0:n<40?SDLK_RIGHT:n<65?SDLK_LEFT:n<95?SDLK_UP:n<120?32:0;if(desired!=key){if(key)set_key(key,false);if(desired)set_key(desired,true);key=desired;}
    if(n==120)capture();if(n==145)push_key(SDLK_ESCAPE);
}
void engine_tick(){
    bound_log(stdout);bound_log(stderr);auto now=std::chrono::steady_clock::now();
    if(test_ms){
        if(!ready&&now-started>std::chrono::seconds(25))throw Error("E_TEST_TIMEOUT","Prehistorik menu was not reached");
        if((!ready||(probe&&!playing))&&now-last_skip>std::chrono::milliseconds(120)){last_skip=now;push_key(!ready?SDLK_ESCAPE:SDLK_RETURN);}
        if(ready&&(!probe||(frames>=146&&pause_seen))&&now-(probe?playing_at:ready_at)>=std::chrono::milliseconds(test_ms))request_quit();
        if(ready&&probe&&!playing&&now-ready_at>std::chrono::seconds(20))throw Error("E_TEST_TIMEOUT","Prehistorik gameplay was not reached");
    }
    auto bytes=read_document(session/"commands.jsonl");if(command_offset>bytes.size())throw Error("E_PROTOCOL","Prehistorik command channel truncated");
    while(true){auto end=bytes.find('\n',command_offset);if(end==std::string::npos){if(bytes.size()-command_offset>16384)throw Error("E_PROTOCOL","Oversized Prehistorik command");break;}if(end-command_offset>16384)throw Error("E_PROTOCOL","Oversized Prehistorik command");auto c=parse_json(bytes.substr(command_offset,end-command_offset));command_offset=end+1;
        if(c.size()!=5||c["schema_version"]!=1||c["kind"]!="command"||c["session_id"]!=request["session_id"]||c["seq"]!=++command_seq||c["command"]!="return_to_library")throw Error("E_PROTOCOL","Invalid Prehistorik command");request_quit();
    }
}
#ifdef TITUS_INPROCESS_PILOT
namespace titus_pilot {
void (*on_ready)()=nullptr;
double cpu_ms(){
#ifdef _WIN32
    FILETIME created,exited,kernel,user;if(!GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user))throw Error("E_PILOT","Cannot measure process CPU");ULARGE_INTEGER k{},u{};k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;return (k.QuadPart+u.QuadPart)/10000.0;
#else
    rusage usage{};if(getrusage(RUSAGE_SELF,&usage))throw Error("E_PILOT","Cannot measure process CPU");return (usage.ru_utime.tv_sec+usage.ru_stime.tv_sec)*1000.0+(usage.ru_utime.tv_usec+usage.ru_stime.tv_usec)/1000.0;
#endif
}
Json enter(const fs::path& file){
    // Explicit bootstrap reset for the experiment; this is not an engine-state reset API.
    events.close();events.clear();request=Json();session.clear();work.clear();
    sequence=command_seq=test_ms=frames=bx=rx=lstart=lx=by=jy=key=0;command_offset=0;
    ready=returning=probe=playing=pause_seen=paused=false;samples=nonzero=clipped=0;peak=0;
    started=ready_at=playing_at=last_skip=std::chrono::steady_clock::now();
    titus_old::on_tick=nullptr;titus_old::on_ready=nullptr;titus_old::on_frame=nullptr;titus_old::on_audio=nullptr;
    titus_old::audio_ok=titus_old::quit_seen=false;titus_old::renderer=nullptr;titus_old::synthetic.fill(-1);
    auto begin=std::chrono::steady_clock::now();auto cpu=cpu_ms();int code=run({"pilot-engine","--session-request",file.u8string()});
    events.close();
    bool leaked=false;if(SDL_WasInit(SDL_INIT_VIDEO)&&titus_old::renderer){for(Uint32 id=1;id<64;++id){auto window=SDL_GetWindowFromID(id);if(window&&SDL_GetRenderer(window)==titus_old::renderer){leaked=true;break;}}}
    return {{"engine_window_leaked",leaked},{"exit_code",code},{"ready",ready},{"frames",frames},{"cpu_ms",cpu_ms()-cpu},{"total_ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()}};
}
}
#elif defined(_WIN32)
int wmain(int argc,wchar_t** argv){std::vector<std::string> args;for(int i=0;i<argc;++i)args.push_back(fs::path(argv[i]).u8string());return run(args);}
#else
int main(int argc,char** argv){return run(std::vector<std::string>(argv,argv+argc));}
#endif
