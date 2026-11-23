#include "titus/session.hpp"
#include <iostream>
using namespace titus;
int main(){try{
    auto check=[](bool b){if(!b)throw Error("E_TEST","Invalid family protocol acceptance");};
    auto rejected=[&](auto fn){try{fn();}catch(const std::exception&){return;}check(false);};
    for(const auto& game:{"blues","jukebox","prehistorik2"}){
        Json request={{"game_id",game},{"session_id",uuid()},{"profile_id",uuid()},{"save_compatibility_id",std::string(game)+".test"},{"sources",Json::array({Json::object()})}};
        auto features=family_features(game,false);int count=std::string(game)=="blues"?8:0;
        Json ready={{"schema_version",1},{"kind","event"},{"session_id",request["session_id"]},{"seq",2},{"event","ready"},{"body",{{"features",features},{"scene","menu"},{"save_slot_count",count},{"audio_active",true}}}};
        validate_runtime_event(ready,request);
        auto wrong=ready;wrong["body"]["save_slot_count"]=count?0:8;rejected([&]{validate_runtime_event(wrong,request);});
        if(!count){wrong=ready;wrong["body"]["features"].push_back("rewind");rejected([&]{validate_runtime_event(wrong,request);});}
        Json result={{"schema_version",1},{"kind","result"},{"session_id",request["session_id"]},{"game_id",game},{"engine_id","blues-family"},{"outcome","returned"},{"ready",true},{"native_exit_code",0},{"error",nullptr}};
        validate_runtime_result(result,request);result["game_id"]="fox";rejected([&]{validate_runtime_result(result,request);});
    }
    rejected([&]{family_features("fox",false);});
    std::cout<<"family protocol: per-game capabilities, slot counts and response identities passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
