// SPDX-License-Identifier: GPL-3.0-or-later
#include "managed_fox.h"
#include "titus/fox_prepare.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
using namespace titus;
namespace {
Json request;fs::path session,work;std::ofstream events;
int sequence=0,command_seq=0,test_ms=0,frames=0,bx=0,rx=0,lstart=0,lx=0,by=0,jy=0,key=0;
size_t command_offset=0;bool ready=false,returning=false,probe=false,playing=false,pause_seen=false,paused=false;
auto started=std::chrono::steady_clock::now(),ready_at=started,playing_at=started,last_skip=started;
void emit(const char* name,const Json& body=Json::object()){
    events<<Json({{"schema_version",1},{"kind","event"},{"session_id",request["session_id"]},{"seq",++sequence},{"event",name},{"body",body}}).dump()<<'\n';events.flush();if(!events)throw Error("E_PROTOCOL","Cannot write Fox event");
}
Json inspection(){bool valid=fox_prepared_valid(request);return {{"schema_version",1},{"kind","inspection"},{"inspection_id",request["inspection_id"]},{"profile_id",request["profile_id"]},{"game_id","fox"},{"engine_id","opentitus"},{"save_compatibility_id",request["save_compatibility_id"]},{"status",valid?"ok":"preparation_required"},{"features",runtime_features("fox",false)},{"save_slot_count",0},{"preparation_ids",valid?Json::array():Json::array({"fox-lzw-v1"})},{"save_index",nullptr},{"error",nullptr}};}
void config(bool full){
    std::string text="reswidth 320\nresheight 200\nbitdepth 32\ndevmode 0\ningamewidth 320\ningameheight 192\nvideomode "+std::to_string(full?1:0)+"\nlevelcount 15\ngame 0\nsprites SPREXP.SQZ\nlogo TITUS.SQZ\nlogoformat 2\nintro TITRE.SQZ\nintroformat 2\nmenu MENU.SQZ\nmenuformat 2\nfinish LEVELA.SQZ\nfinishformat 0\nfont FONTS.SQZ\n";
    const char* levels[]={"0","J","1","2","3","4","5","6","7","8","9","B","C","E","G"};for(int i=0;i<15;++i)text+="level "+std::to_string(i+1)+" LEVEL"+levels[i]+".SQZ\n";
    std::ofstream out(filesystem_path(fox_prepared_directory(request)/"titus.conf"),std::ios::binary);out<<text;out.flush();if(!out)throw Error("E_WRITE","Cannot write private Fox configuration");
}
void result(const char* outcome,int code,const Json& error=nullptr){write_document(session/"result.json",{{"schema_version",1},{"kind","result"},{"session_id",request["session_id"]},{"game_id","fox"},{"engine_id","opentitus"},{"outcome",outcome},{"ready",ready},{"native_exit_code",code},{"error",error}});}
void request_quit(){if(!returning){if(ready)emit("return_requested",{{"confirmation_required",false}});returning=true;}fox_request_quit();}
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
    if(request.at("game_id")!="fox"||request.at("engine_id")!="opentitus")throw Error("E_GAME","OpenTitus runtime only supports Fox DOS");
    bool inspect=args[1]!="--session-request";if(request.at("kind")!=(inspect?"inspect_request":"launch"))throw Error("E_REQUEST","Wrong request kind");
    validate_engine_request(request,file);verify_profile_lock(request);session=file.parent_path();work=fs::u8path(request["paths"]["work_directory"]["path"].get<std::string>());
    if(inspect){if(args[1]=="--prepare-request"){auto fault=environment_value("TITUS_TEST_FAULT");prepare_fox(request,environment_value("TITUS_TEST_RUNTIME_MS").value_or("0")!="0"&&fault&&*fault=="prepare_fail");}write_document(session/"inspection.json",inspection());return 0;}
    if(!fox_prepared_valid(request))throw Error("E_PREPARATION","Fox preparation is missing or damaged");
    bool full=false;auto prefs=work/"preferences.json";
    if(path_exists(prefs)){auto p=parse_json(read_document(prefs,8192));if(p.value("schema_version",0)!=1||!p.at("fullscreen").is_boolean())throw Error("E_PREFERENCES","Unsupported Fox preferences");full=p["fullscreen"];}
    if(request["settings"].contains("video"))full=request["settings"]["video"]["fullscreen"];
    config(full);
    events.open(filesystem_path(session/"events.jsonl"),std::ios::binary|std::ios::app);emit("hello",{{"engine_id","opentitus"},{"build_revision",TITUS_BUILD_REVISION},{"protocol",1}});emit("progress",{{"phase","initialize"},{"percent",90}});
    test_ms=std::min(10000,std::max(0,std::atoi(environment_value("TITUS_TEST_RUNTIME_MS").value_or("0").c_str())));probe=environment_value("TITUS_TEST_GAMEPLAY").value_or("0")=="1";
    auto fault=environment_value("TITUS_TEST_FAULT").value_or("");if(test_ms&&fault=="before_ready")throw Error("E_TEST_LOAD","Deliberate Fox load failure");
    fox_set_data_root(fox_prepared_directory(request).u8string().c_str());
    started=std::chrono::steady_clock::now();
    int code=fox_run();
    if(code){auto e=runtime_error(Error("E_NATIVE","OpenTitus reported an error: "+std::to_string(code)),ready);emit("error",e);emit("closing");result("runtime_failed",1,e);return 1;}
    write_document(prefs,{{"schema_version",1},{"fullscreen",full}});
    if(probe){unsigned long long samples,nonzero,clipped;int peak;fox_audio_stats(&samples,&nonzero,&clipped,&peak);write_document(session/"probe-observation.json",{{"frames",frames},{"right",rx-bx},{"left",lstart-lx},{"jump",by-jy},{"pause",pause_seen},{"samples",samples},{"nonzero",nonzero},{"clipped",clipped},{"peak",peak}});}
    emit("closing");if(test_ms&&fault=="missing_result")return 0;result(ready?"returned":"cancelled",0);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';if(events.is_open())try{Json error=runtime_error(e,ready);emit("error",error);emit("closing");result("runtime_failed",1,error);}catch(...){}return 1;}}
}
extern "C" void fox_ready(){if(ready)return;if(!fox_audio_active())throw Error("E_AUDIO","Fox audio device is not active");ready=true;ready_at=std::chrono::steady_clock::now();emit("ready",{{"features",runtime_features("fox",false)},{"scene","menu"},{"save_slot_count",0},{"audio_active",true}});}
extern "C" void fox_playing(){playing=true;playing_at=std::chrono::steady_clock::now();}
extern "C" void fox_paused(int value){paused=value!=0;if(paused)pause_seen=true;}
extern "C" void fox_actor(int x,int y){if(!playing)fox_playing();if(!probe)return;
    int n=frames++;if(n==10)bx=rx=x;if(n>=10&&n<40)rx=std::max(rx,x);if(n==40)lstart=lx=x;if(n>=40&&n<65)lx=std::min(lx,x);if(n==65)by=jy=y;if(n>=65&&n<95)jy=std::min(jy,y);
    int desired=n<10?0:n<40?275:n<65?276:n<95?273:n<120?32:0;if(desired!=key){if(key)fox_key(key,0);if(desired)fox_key(desired,1);key=desired;}
    if(n==120)fox_capture((session/"gameplay.bmp").u8string().c_str());if(n==145){fox_key(112,1);fox_key(112,0);}
}
extern "C" void fox_tick(){
    bound_log(stdout);bound_log(stderr);auto now=std::chrono::steady_clock::now();
    if(test_ms){
        if(!ready&&now-started>std::chrono::seconds(25))throw Error("E_TEST_TIMEOUT","Fox menu was not reached");
        if((!ready||(probe&&!playing))&&now-last_skip>std::chrono::milliseconds(120)){last_skip=now;fox_key(32,1);fox_key(32,0);}
        if(ready&&(!probe||frames>=146)&&now-(probe?playing_at:ready_at)>=std::chrono::milliseconds(test_ms))request_quit();
        if(ready&&probe&&!playing&&now-ready_at>std::chrono::seconds(20))throw Error("E_TEST_TIMEOUT","Fox gameplay was not reached");
    }
    auto bytes=read_document(session/"commands.jsonl");if(command_offset>bytes.size())throw Error("E_PROTOCOL","Fox command channel truncated");
    while(true){auto end=bytes.find('\n',command_offset);if(end==std::string::npos){if(bytes.size()-command_offset>16384)throw Error("E_PROTOCOL","Oversized Fox command");break;}if(end-command_offset>16384)throw Error("E_PROTOCOL","Oversized Fox command");auto c=parse_json(bytes.substr(command_offset,end-command_offset));command_offset=end+1;
        if(c.size()!=5||c["schema_version"]!=1||c["kind"]!="command"||c["session_id"]!=request["session_id"]||c["seq"]!=++command_seq||c["command"]!="return_to_library")throw Error("E_PROTOCOL","Invalid Fox command");request_quit();
    }
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){std::vector<std::string> args;for(int i=0;i<argc;++i)args.push_back(fs::path(argv[i]).u8string());return run(args);}
#else
int main(int argc,char** argv){return run(std::vector<std::string>(argv,argv+argc));}
#endif
