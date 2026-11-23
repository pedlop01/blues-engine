#include "titus/process.hpp"
#include <thread>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#endif
using namespace titus;
int run(const std::vector<std::string>& args){
    if(args.size()>1&&args[1]=="--child"){
        auto value=std::getenv("TITUS_PROFILE_LOCK");if(!value)return 8;auto handle=std::stoll(value);
#ifdef _WIN32
        DWORD flags;if(!GetHandleInformation(reinterpret_cast<HANDLE>(static_cast<intptr_t>(handle)),&flags))return 9;
#else
        if(fcntl(static_cast<int>(handle),F_GETFD)<0)return 9;
#endif
        if(args.size()>2){std::cout<<args[2];std::cerr<<"redirected error";return 0;}
        std::this_thread::sleep_for(std::chrono::seconds(8));return 0;
    }
    auto root=fs::temp_directory_path()/fs::u8path("titus-process ñ "+uuid());
    try{
        fs::create_directories(root);ProfileLock lock(root/"profile.lock");
#ifdef _WIN32
        wchar_t module[32768];auto length=GetModuleFileNameW(nullptr,module,32768);if(!length||length>=32768)throw std::runtime_error("module path failed");auto original=fs::path(std::wstring(module,length));auto program=root/fs::u8path("helper ñ.exe");
#else
        auto original=normalized_path(fs::u8path(args[0]));auto program=root/fs::u8path("helper ñ");
#endif
        fs::copy_file(original,program);
        std::string token="Unicode á, spaces, quote \" and shell $() ; ending\\";
        Process child;child.start(program,{"--child",token},root,{},root/"logs",lock);
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        while(!child.poll()){if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("child timeout");std::this_thread::sleep_for(std::chrono::milliseconds(10));}
        if(child.exit_code()!=0||read_document(root/"logs/stdout.log")!=token||read_document(root/"logs/stderr.log")!="redirected error")throw std::runtime_error("argv, inheritance or logs failed");
        Process waiting;waiting.start(program,{"--child"},root,{},root/"wait",lock);waiting.terminate();
        while(!waiting.poll()){if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("force close timeout");std::this_thread::sleep_for(std::chrono::milliseconds(10));}
        if(waiting.exit_code()==0)throw std::runtime_error("force close must report actual failure");
        std::cout<<"OK: native argv quoting, Unicode, logs, inherited lock and explicit termination\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';std::error_code ec;fs::remove_all(root,ec);return 1;}
    std::error_code ec;fs::remove_all(root,ec);return 0;
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){std::vector<std::string> args;for(int i=0;i<argc;++i)args.push_back(fs::path(argv[i]).u8string());return run(args);}
#else
int main(int argc,char** argv){return run(std::vector<std::string>(argv,argv+argc));}
#endif
