// SPDX-License-Identifier: GPL-3.0-or-later
// TH-11 diagnostic host only. Never installed or used by the production launcher.
#define SDL_MAIN_HANDLED
#include "entry.hpp"
#include "titus/session.hpp"
#include <SDL.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <fstream>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif
using namespace titus;
using Clock=std::chrono::steady_clock;
namespace {
Clock::time_point invocation;
double ready_ms=-1;
void observed_ready(){ready_ms=std::chrono::duration<double,std::milli>(Clock::now()-invocation).count();}
void pilot_env(const char* key,const std::string& value){
 // All three consumers must agree in a shared Windows process (EXE CRT, OS, SDL DLL CRT).
#ifdef _WIN32
 if(_putenv_s(key,value.c_str())||!SetEnvironmentVariableA(key,value.c_str()))throw Error("E_PILOT","Cannot set diagnostic environment");
#else
 if(setenv(key,value.c_str(),1))throw Error("E_PILOT","Cannot set diagnostic environment");
#endif
 if(SDL_setenv(key,value.c_str(),1))throw Error("E_PILOT","Cannot set SDL diagnostic environment");
}
void require(bool b,const char* message){if(!b)throw Error("E_PILOT",message);}
uint64_t peak_memory(){
#ifdef _WIN32
 PROCESS_MEMORY_COUNTERS memory{};memory.cb=sizeof memory;return GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof memory)?memory.PeakWorkingSetSize:0;
#else
 rusage usage{};return getrusage(RUSAGE_SELF,&usage)==0?uint64_t(usage.ru_maxrss)*1024:0;
#endif
}
struct Host {
 SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;SDL_Texture* texture=nullptr;
 SDL_AudioDeviceID audio=0;Uint32 window_id=0,event_type=0;bool audio_requested=false;
 std::atomic<uint64_t> audio_callbacks{0};
 static void mix(void* self,Uint8* bytes,int size){SDL_memset(bytes,0,size);++static_cast<Host*>(self)->audio_callbacks;}
 void open(bool with_audio){
  audio_requested=with_audio;
  require(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER)==0,SDL_GetError());
  SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS,"1");
  window=SDL_CreateWindow("TH-11 host",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,320,200,SDL_WINDOW_HIDDEN);
  require(window,SDL_GetError());window_id=SDL_GetWindowID(window);renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);require(renderer,SDL_GetError());
  texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,8,8);require(texture,SDL_GetError());
  Uint32 pixels[64];for(int i=0;i<64;++i)pixels[i]=0xff16a7c0;require(SDL_UpdateTexture(texture,nullptr,pixels,8*4)==0,SDL_GetError());
  event_type=SDL_RegisterEvents(1);require(event_type!=Uint32(-1),"No host event type available");SDL_Event event{};event.type=event_type;require(SDL_PushEvent(&event)==1,SDL_GetError());
  if(with_audio){SDL_AudioSpec desired{},obtained{};desired.freq=44100;desired.format=AUDIO_S16SYS;desired.channels=2;desired.samples=512;desired.callback=mix;desired.userdata=this;audio=SDL_OpenAudioDevice(nullptr,0,&desired,&obtained,0);require(audio,SDL_GetError());SDL_PauseAudioDevice(audio,0);}
  require(draw(),"Host could not render before engine entry");
 }
 bool draw(){
  if(!(SDL_WasInit(SDL_INIT_VIDEO)&SDL_INIT_VIDEO)||SDL_GetWindowFromID(window_id)!=window)return false;
  if(SDL_GetRenderer(window)!=renderer)return false;
  if(SDL_SetRenderDrawColor(renderer,0,0,0,255)||SDL_RenderClear(renderer)||SDL_RenderCopy(renderer,texture,nullptr,nullptr))return false;
  Uint32 pixel=0;
  SDL_Rect sample{1,1,1,1};if(SDL_RenderReadPixels(renderer,&sample,SDL_PIXELFORMAT_ARGB8888,&pixel,4)||pixel!=0xff16a7c0)return false;
  SDL_RenderPresent(renderer);return true;
 }
 Json inspect(){
  SDL_Event event{};int remaining=SDL_PeepEvents(&event,1,SDL_PEEKEVENT,event_type,event_type);
  return {{"drawable",draw()},{"video",bool(SDL_WasInit(SDL_INIT_VIDEO))},{"audio_subsystem",bool(SDL_WasInit(SDL_INIT_AUDIO))},{"controller_subsystem",bool(SDL_WasInit(SDL_INIT_GAMECONTROLLER))},{"host_event_preserved",remaining==1},{"host_audio_playing",audio&&SDL_GetAudioDeviceStatus(audio)==SDL_AUDIO_PLAYING},{"host_audio_callbacks",audio_callbacks.load()},{"controller_hint",SDL_GetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS)?Json(SDL_GetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS)):Json(nullptr)}};
 }
 void close(){
  // Never dereference renderer/window handles invalidated by the engine's SDL_Quit.
  if(SDL_WasInit(SDL_INIT_AUDIO)&&audio)SDL_CloseAudioDevice(audio);audio=0;
  if(SDL_WasInit(SDL_INIT_VIDEO)&&SDL_GetWindowFromID(window_id)==window){SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);}
  window=nullptr;renderer=nullptr;texture=nullptr;SDL_Quit();
 }
 ~Host(){close();}
};
void empty(const fs::path& path){std::ofstream stream(filesystem_path(path),std::ios::binary);require(bool(stream),"Cannot create pilot channel");}
int program(const std::vector<std::string>& args){try{
 if(args.size()==3&&args[1]=="--engine"){
  auto result=titus_pilot::enter(fs::u8path(args[2]));result["peak_rss_bytes"]=peak_memory();write_document(fs::u8path(args[2]).parent_path()/"pilot-native.json",result);return result["exit_code"];
 }
 require(args.size()==7,"Use --mode inline|child --request FILE --output FILE");
 require(args[1]=="--mode"&&args[3]=="--request"&&args[5]=="--output","Invalid pilot arguments");
 auto mode=args[2];require(mode=="inline"||mode=="child","Invalid pilot mode");auto source=normalized_path(fs::u8path(args[4])),output=normalized_path(fs::u8path(args[6]));
 auto template_request=parse_json(read_document(source));auto profile=fs::u8path(template_request["paths"]["profile_directory"]["path"].get<std::string>());auto data=profile;for(int i=0;i<5;++i)data=data.parent_path();
 ProfileLock lock(data/"profile-locks"/fs::u8path(template_request["profile_id"].get<std::string>()+".prehistorik.prehistorik.full.audited.lock"));
 SDL_SetMainReady();pilot_env("TITUS_PROFILE_LOCK",std::to_string(lock.native_handle()));pilot_env("TITUS_TEST_RUNTIME_MS","4500");pilot_env("TITUS_TEST_GAMEPLAY","1");
 auto scenario=environment_value("TITUS_PILOT_SCENARIO").value_or("sequence");require(scenario=="sequence"||scenario=="crash"||scenario=="audio","Invalid pilot scenario");
 std::vector<std::string> faults=scenario=="sequence"?std::vector<std::string>{"normal","before_ready","normal","pilot_throw","normal"}:scenario=="crash"?std::vector<std::string>{"pilot_crash"}:std::vector<std::string>{"normal"};
 Json report={{"schema_version",1},{"mode",mode},{"scenario",scenario},{"complete",false},{"runs",Json::array()}};
 Host host;host.open(scenario=="audio");
#ifdef _WIN32
 wchar_t module[32768];DWORD length=GetModuleFileNameW(nullptr,module,32768);require(length>0&&length<32768,"Cannot locate pilot executable");auto self=normalized_path(fs::path(std::wstring(module,length)));
#else
 auto self=normalized_path(fs::u8path(args[0]));
#endif
 for(const auto& fault:faults){
  auto folder=source.parent_path().parent_path()/uuid();ensure_directory(folder);auto request=template_request;request["session_id"]=folder.filename().u8string();request["paths"]["session_directory"]["path"]=folder.u8string();write_document(folder/"request.json",request);empty(folder/"commands.jsonl");empty(folder/"events.jsonl");
  // A durable marker distinguishes a whole-host crash from a handled engine error.
  report["active_fault"]=fault;report["host_before"]=host.inspect();write_document(output,report);
  ready_ms=-1;invocation=Clock::now();double begin_cpu=titus_pilot::cpu_ms();Json native;int code;
  if(mode=="inline"){
   pilot_env("TITUS_TEST_FAULT",fault);titus_pilot::on_ready=observed_ready;native=titus_pilot::enter(folder/"request.json");code=native["exit_code"];native["peak_rss_bytes"]=peak_memory();
  }else{
   Process process;process.start(self,{"--engine",(folder/"request.json").u8string()},self.parent_path(),{{"TITUS_TEST_RUNTIME_MS","4500"},{"TITUS_TEST_GAMEPLAY","1"},{"TITUS_TEST_FAULT",fault}},folder,lock);
   while(!process.poll()){
    if(ready_ms<0&&path_exists(folder/"events.jsonl")){auto bytes=read_document(folder/"events.jsonl");size_t start=0;for(;;){auto end=bytes.find('\n',start);if(end==std::string::npos)break;auto event=parse_json(bytes.substr(start,end-start));validate_runtime_event(event,request);if(event["event"]=="ready"){observed_ready();break;}start=end+1;}}
    if(Clock::now()-invocation>=std::chrono::seconds(60)){process.terminate();while(!process.poll())std::this_thread::sleep_for(std::chrono::milliseconds(5));throw Error("E_PILOT","Child pilot timeout");}std::this_thread::sleep_for(std::chrono::milliseconds(5));
   }
   code=process.exit_code();native=path_exists(folder/"pilot-native.json")?parse_json(read_document(folder/"pilot-native.json")):Json::object();
  }
  auto elapsed=std::chrono::duration<double,std::milli>(Clock::now()-invocation).count();Json host_after=host.inspect();
  Json row={{"fault",fault},{"exit_code",code},{"ready_ms",ready_ms<0?Json(nullptr):Json(ready_ms)},{"invocation_ms",elapsed},{"host_cpu_ms",titus_pilot::cpu_ms()-begin_cpu},{"native",native},{"host_after",host_after},{"result",path_exists(folder/"result.json")?parse_json(read_document(folder/"result.json")):Json(nullptr)},{"observation",path_exists(folder/"probe-observation.json")?parse_json(read_document(folder/"probe-observation.json")):Json(nullptr)}};
  if(!row["result"].is_null())validate_runtime_result(row["result"],request);
  report["runs"].push_back(row);write_document(output,report);
  // Rebuilding the entire SDL host is measured, not presented as transparent preservation.
  auto begin=Clock::now();bool rebuild=!host_after["drawable"].get<bool>()||(mode=="inline"&&fault=="pilot_throw");
  if(rebuild){host.close();host.open(scenario=="audio");}
  report["runs"].back()["host_rebuilt"]=rebuild;report["runs"].back()["host_rebuild_ms"]=std::chrono::duration<double,std::milli>(Clock::now()-begin).count();
  report["runs"].back()["host_recovered"]=host.draw();write_document(output,report);
 }
 report["complete"]=true;report["host_peak_rss_bytes"]=peak_memory();write_document(output,report);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){std::vector<std::string> args;for(int i=0;i<argc;++i)args.push_back(fs::path(argv[i]).u8string());return program(args);}
#else
int main(int argc,char** argv){return program(std::vector<std::string>(argv,argv+argc));}
#endif
