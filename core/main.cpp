#include "titus/catalog.hpp"
#include <iostream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
using namespace titus;
namespace {
int run(const std::vector<std::string>& args) {
    try {
        fs::path root;bool recursive=false;long long revision=-1;std::vector<std::string> operands;
        for(size_t n=1;n<args.size();++n) {
            if(args[n]=="--user-dir") { if(++n==args.size())throw Error("E_ARGUMENT","Missing --user-dir path");root=fs::u8path(args[n]); }
            else if(args[n]=="--recursive")recursive=true;
            else if(args[n]=="--expected-revision") {
                if(++n==args.size())throw Error("E_ARGUMENT","Missing revision");
                size_t consumed=0;revision=std::stoll(args[n],&consumed);
                if(consumed!=args[n].size()||revision<0)throw Error("E_ARGUMENT","Invalid expected revision");
            } else operands.push_back(args[n]);
        }
        if(operands.empty()||operands[0]=="--help") {
            std::cout<<"titus-catalog [--user-dir ABS] [--expected-revision N] COMMAND\n"
                "  scan PATH [--recursive]   Read resources, no catalog writes\n"
                "  init                     Create catalog and default profile\n"
                "  list                     Print v1 catalog JSON\n"
                "  add PATH [--recursive]   Register known sets and alternative locations\n"
                "  relocate UUID PATH       Relink identical data; preserve identity\n"
                "  recheck UUID             Revalidate registered resources\n"
                "  remove UUID              Remove reference only\n"
                "  recover                  Restore backup, preserving corrupt catalog\n"
                "  paths                    Print resolved storage paths\n";return 0;
        }
        auto paths=Paths::resolve(root);CatalogStore catalog(paths);Json result;
        const auto& command=operands[0];
        auto arity=[&](size_t n){if(operands.size()!=n)throw Error("E_ARGUMENT","Wrong argument count for "+command);};
        if(command=="scan") {
            arity(2);result=Json::array();
            for(const auto& d:Detector().scan(fs::u8path(operands[1]),recursive))result.push_back({{"installation",d.installation},{"diagnostics",d.diagnostics}});
        } else if(command=="init") { arity(1);result=catalog.init(revision); }
        else if(command=="list") { arity(1);result=catalog.load(); }
        else if(command=="add") { arity(2);result=catalog.add(fs::u8path(operands[1]),recursive,revision); }
        else if(command=="relocate") { arity(3);result=catalog.relocate(operands[1],fs::u8path(operands[2]),revision); }
        else if(command=="recheck") { arity(2);result=catalog.recheck(operands[1],revision); }
        else if(command=="remove") { arity(2);result=catalog.remove(operands[1],revision); }
        else if(command=="recover") { arity(1);result=catalog.recover(revision); }
        else if(command=="paths") { arity(1);result={{"config",paths.config.u8string()},{"data",paths.data.u8string()},{"cache",paths.cache.u8string()}}; }
        else throw Error("E_ARGUMENT","Unknown command: "+command);
        std::cout<<result.dump(2)<<'\n';return 0;
    } catch(const Error& e) { std::cerr<<Json({{"error",e.code},{"message",e.what()}}).dump()<<'\n';return 1; }
    catch(const std::exception& e) { std::cerr<<Json({{"error","E_OPERATION"},{"message",e.what()}}).dump()<<'\n';return 1; }
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);std::vector<std::string> args;
    for(int i=0;i<argc;++i)args.push_back(fs::path(argv[i]).u8string());
    return run(args);
}
#else
int main(int argc,char** argv) { return run(std::vector<std::string>(argv,argv+argc)); }
#endif
