#include "titus/session.hpp"
#include <iostream>
using namespace titus;
int main(){try{
    auto check=[](bool b){if(!b)throw Error("E_TEST","Invalid family protocol acceptance");};
    auto rejected=[&](auto fn){try{fn();}catch(const std::exception&){return;}check(false);};
    for(const auto& game:{"blues","jukebox","prehistorik2","fox","prehistorik"}){
        Json request={{"game_id",game},{"session_id",uuid()},{"profile_id",uuid()},{"save_compatibility_id",std::string(game)+".test"},{"sources",Json::array({Json::object()})}};
        auto features=runtime_features(game,false);int count=std::string(game)=="blues"?8:0;
        Json ready={{"schema_version",1},{"kind","event"},{"session_id",request["session_id"]},{"seq",2},{"event","ready"},{"body",{{"features",features},{"scene","menu"},{"save_slot_count",count},{"audio_active",true}}}};
        validate_runtime_event(ready,request);
        auto wrong=ready;wrong["body"]["save_slot_count"]=count?0:8;rejected([&]{validate_runtime_event(wrong,request);});
        if(!count){wrong=ready;wrong["body"]["features"].push_back("rewind");rejected([&]{validate_runtime_event(wrong,request);});}
        Json result={{"schema_version",1},{"kind","result"},{"session_id",request["session_id"]},{"game_id",game},{"engine_id",runtime_engine(request)},{"outcome","returned"},{"ready",true},{"native_exit_code",0},{"error",nullptr}};
        validate_runtime_result(result,request);
        auto failed=result;failed["outcome"]="runtime_failed";failed["native_exit_code"]=1;failed["ready"]=false;failed["error"]=runtime_error(Error("E_TEST_LOAD","before ready"),false);check(failed["error"]["code"]=="E_ENGINE_INIT");validate_runtime_result(failed,request);
        failed["error"]["code"]="E_RUNTIME";rejected([&]{validate_runtime_result(failed,request);});
        check(runtime_error(Error("E_NATIVE","after ready"),true)["code"]=="E_ENGINE_RUNTIME");check(runtime_error(Error("E_READ","storage"),false)["code"]=="E_STORAGE");
        std::string long_message;for(int i=0;i<600;++i)long_message+="á";auto bounded=runtime_error(Error("E_NATIVE",long_message),true);check(bounded["message"].get<std::string>().size()<=1024);parse_json(bounded.dump());
        result["game_id"]=std::string(game)=="fox"?"blues":"fox";rejected([&]{validate_runtime_result(result,request);});
    }
    Json amiga={{"game_id","fox"},{"edition_id","amiga"},{"session_id",uuid()},{"profile_id",uuid()},{"save_compatibility_id","fox.amiga.full.audited"},{"sources",Json::array({Json::object()})}};
    check(runtime_engine(amiga)=="fox-amiga-native" && runtime_slot_count(amiga)==8);auto amiga_features=runtime_features(amiga);check(std::find(amiga_features.begin(),amiga_features.end(),"rewind")!=amiga_features.end());
    Json amiga_ready={{"schema_version",1},{"kind","event"},{"session_id",amiga["session_id"]},{"seq",1},{"event","ready"},{"body",{{"features",amiga_features},{"scene","gameplay"},{"save_slot_count",8},{"audio_active",true}}}};validate_runtime_event(amiga_ready,amiga);
    auto dos=amiga;dos["edition_id"]="dos";check(runtime_engine(dos)=="opentitus" && runtime_slot_count(dos)==0);rejected([&]{validate_runtime_event(amiga_ready,dos);});
    rejected([&]{runtime_features("unknown",false);});
    std::cout<<"family protocol: per-game capabilities, slot counts and response identities passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
