#define SDL_MAIN_HANDLED
#include "titus/session.hpp"
#include <SDL.h>
#include <iostream>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif
using namespace titus;
int run(const std::vector<std::string>& args){try{
    fs::path root,legacy;std::string id;SessionOptions options;bool request_return=false,orphan=false;
    for(size_t i=1;i<args.size();++i){auto a=args[i];if(a=="--test-orphan")orphan=true;
        else if(a=="--gameplay-probe")options.test_gameplay=true;
        else if(a=="--import-legacy")options.legacy_import=true;
        else if(a=="--inspect-only")options.inspect_only=true;
        else if(a=="--return-on-ready")request_return=true;
        else if(a=="--user-dir"||a=="--installation"||a=="--legacy-root"||a=="--test-ms"||a=="--test-fault"||a=="--settings"||a=="--load-slot"){
            if(++i==args.size())throw Error("E_ARGUMENT","Missing argument");if(a=="--settings")options.settings=parse_json(read_document(fs::u8path(args[i])));else if(a=="--load-slot"){options.load_slot=std::stoi(args[i]);if(options.load_slot<1||options.load_slot>8)throw Error("E_ARGUMENT","Save slot must be between 1 and 8");}else if(a=="--user-dir")root=fs::u8path(args[i]);else if(a=="--installation")id=args[i];else if(a=="--legacy-root")legacy=fs::u8path(args[i]);else if(a=="--test-fault")options.test_fault=args[i];else options.test_ms=std::stoi(args[i]);
        }else throw Error("E_ARGUMENT","Unknown option");}
    if(id.empty())throw Error("E_ARGUMENT","--installation UUID is required");
    SDL_SetMainReady();char* base=SDL_GetBasePath();if(!base)throw Error("E_PATH",SDL_GetError());fs::path package=fs::u8path(base);SDL_free(base);
    if(!legacy.empty())options.legacy={legacy/"preferences.ini",legacy/"saves"};else{
        char* previous=SDL_GetPrefPath("pedlop01","blues-engine");fs::path folder=previous?fs::u8path(previous):fs::path();SDL_free(previous);
        auto config=environment_value("BLUES_CONFIG_FILE"),saves=environment_value("BLUES_SAVE_DIR");options.legacy.preferences=config?fs::u8path(*config):(folder.empty()?fs::path():folder/"preferences.ini");options.legacy.saves=saves?fs::u8path(*saves):(folder.empty()?fs::path():folder/"saves");
    }
    auto paths=Paths::resolve(root);Session session(paths,package,CatalogStore(paths).load(),id,options);bool sent=false;
    while(!session.finished()){session.poll();if(orphan&&session.ready()){std::cout<<Json({{"state","orphaned"},{"directory",session.directory().u8string()}}).dump()<<std::endl;std::_Exit(0);}if(request_return&&session.ready()&&!sent){session.request_return();sent=true;}std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    std::cout<<Json({{"state",session.state==Session::State::Done?"done":"failed"},{"ready",session.ready()},{"directory",session.directory().u8string()},{"inspection",session.inspection()},{"message",session.message()},{"error",session.error()}}).dump()<<'\n';return session.state==Session::State::Done?0:1;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){std::vector<std::string> args;for(int i=0;i<argc;++i)args.push_back(fs::path(argv[i]).u8string());return run(args);}
#else
int main(int argc,char** argv){return run(std::vector<std::string>(argv,argv+argc));}
#endif
