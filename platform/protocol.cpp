#include "titus/session.hpp"
#include <algorithm>
#include <set>
#include <regex>
namespace titus {
Json blues_features(bool dual){Json features=Json::array({"return_to_library","native_pause","fullscreen","save_slots","save_thumbnails","rewind","dynamic_view","smooth_camera","hud_modes","invulnerability","lives_options","level_select","character_select","separate_volumes","audio_mode_selection"});if(dual)features.push_back("live_edition_switch");return features;}
namespace {
void must(bool ok,const std::string& text,const char* code="E_REQUEST"){if(!ok)throw Error(code,text);}
bool inside(const fs::path& a,const fs::path& b){auto p=normalized_path(a),q=normalized_path(b);auto i=p.begin(),j=q.begin();for(;i!=p.end();++i,++j){if(j==q.end())return false;
#ifdef _WIN32
 if(_wcsicmp(i->c_str(),j->c_str()))return false;
#else
 if(*i!=*j)return false;
#endif
}return true;}
void fields(const Json& j,const std::set<std::string>& names){must(j.is_object()&&j.size()==names.size(),"Unexpected request fields");for(const auto& n:names)must(j.contains(n),"Missing request field: "+n);}
}
void validate_blues_request(const Json& r,const fs::path& file){
    bool inspect=r.value("kind",std::string())=="inspect_request";
    std::set<std::string> names={"schema_version","kind",inspect?"inspection_id":"session_id","profile_id","installation_id","game_id","engine_id","edition_id","save_compatibility_id","platform","sources","paths"};
    if(!inspect)for(auto k:{"action","required_features","settings","allow_silent_audio"})names.insert(k);fields(r,names);
    must(r["schema_version"]==1&&r["kind"]==(inspect?"inspect_request":"launch"),"Unsupported request schema");
    must(r["game_id"]=="blues"&&r["engine_id"]=="blues-family","Runtime only implements Blues Brothers","E_GAME");
    must(r["platform"]==platform(),"Request belongs to another platform");
    must(r["sources"].is_array()&&!r["sources"].empty()&&r["sources"].size()<=2,"Invalid sources");
    Json installations=Json::array();std::set<std::string> editions,ids;bool primary=false;
    for(const auto& source:r["sources"]){fields(source,{"installation_id","game_id","edition_id","variant_id","fingerprint","kind","location","read_only"});
        must(source["kind"]=="directory"&&source["read_only"]==true&&source["game_id"]=="blues","Sources must be read-only Blues directories");
        must(source["location"].is_object()&&source["location"].value("platform",std::string())==platform(),"Source platform mismatch");
        Json entry={{"id",source["installation_id"]},{"game_id","blues"},{"edition_id",source["edition_id"]},{"engine_id","blues-family"},
          {"variant_id",source["variant_id"]},{"fingerprint",source["fingerprint"]},{"save_compatibility_id",r["save_compatibility_id"]},
          {"status","recognized"},{"locations",Json::array({{{"kind","directory"},{"location",source["location"]}}})},{"preparation_id",nullptr}};
        installations.push_back(entry);must(editions.insert(source["edition_id"]).second&&ids.insert(source["installation_id"]).second,"Duplicate source");
        if(source["installation_id"]==r["installation_id"]){must(source["edition_id"]==r["edition_id"],"Primary edition mismatch");primary=true;}
    }
    validate_catalog({{"schema_version",1},{"kind","catalog"},{"catalog_id",r[inspect?"inspection_id":"session_id"]},{"revision",1},{"profiles",Json::array({{{"id",r["profile_id"]},{"name","Jugador"}}})},{"installations",installations}});
    must(primary&&r["save_compatibility_id"]=="blues.full.1991","Invalid primary source or save context");
    fields(r["paths"],{"profile_directory","work_directory","session_directory"});std::vector<fs::path> paths;
    for(auto name:{"profile_directory","work_directory","session_directory"}){
        const auto& p=r["paths"][name];fields(p,{"platform","path"});must(p["platform"]==platform()&&p["path"].is_string(),"Invalid storage location");
        auto native=fs::u8path(p["path"].get<std::string>());must(normalized_path(native).u8string()==native.u8string(),"Storage path must be absolute and normalized");paths.push_back(native);
    }
    must(paths[0]!=paths[1]&&inside(paths[0],paths[1]),"Native workspace must belong to profile");
    must(!inside(paths[0],paths[2])&&!inside(paths[2],paths[0]),"Session and profile overlap");
    must(normalized_path(file.parent_path())==paths[2]&&file.filename()=="request.json","Request must belong to its session directory");
    must(paths[2].filename().u8string()==r[inspect?"inspection_id":"session_id"],"Session directory identity mismatch");
    for(const auto& s:r["sources"]){auto source=fs::u8path(s["location"]["path"].get<std::string>());
        must(normalized_path(source)==source,"Source must be normalized");for(const auto& p:paths)must(!inside(source,p)&&!inside(p,source),"Storage overlaps original resources");
        bool verified=false;for(const auto& d:Detector().scan(source))if(!d.installation.is_null()&&d.installation["game_id"]=="blues"&&d.installation["edition_id"]==s["edition_id"]&&d.installation["variant_id"]==s["variant_id"]&&d.installation["fingerprint"]==s["fingerprint"]&&d.installation["status"]=="recognized")verified=true;
        must(verified,"Resources changed or unsupported variant","E_DATA_VARIANT");
    }
    must(!editions.count("amiga")||editions.count("dos"),"Blues Amiga requires its verified DOS supplement for Concert","E_DATA_MISSING");
    if(inspect)return;
    must(r["allow_silent_audio"].is_boolean(),"Invalid audio policy");
    must(r["action"].is_object()&&r["action"].value("type",std::string())=="new_game","Global load is not implemented; use native eight-slot menu","E_UNSUPPORTED_SETTING");fields(r["action"],{"type"});must(r["action"]["type"]=="new_game","Global load is not implemented; use native eight-slot menu","E_UNSUPPORTED_SETTING");
    auto available=blues_features(editions.size()==2);std::set<std::string> required;
    must(r["required_features"].is_array(),"Invalid required features");for(const auto& f:r["required_features"]){must(f.is_string()&&std::find(available.begin(),available.end(),f)!=available.end(),"Unsupported required feature","E_UNSUPPORTED_SETTING");must(required.insert(f).second,"Duplicate feature");}
    must(r["settings"].is_object(),"Invalid settings");
    const std::map<std::string,std::string> gates={{"camera","smooth_camera"},{"hud","hud_modes"},{"view_step","dynamic_view"},{"invulnerable","invulnerability"},{"infinite_lives","lives_options"},{"lives_player1","lives_options"},{"lives_player2","lives_options"},{"start_level","level_select"},{"sound_mode","audio_mode_selection"}};
    for(auto group=r["settings"].begin();group!=r["settings"].end();++group){must(group.key()=="video"||group.key()=="audio"||group.key()=="blues","Unknown settings group","E_UNSUPPORTED_SETTING");must(group.value().is_object(),"Invalid settings group");
        for(auto v=group.value().begin();v!=group.value().end();++v){auto key=v.key();const auto& value=v.value();
            if(group.key()=="video"){must(key=="fullscreen"&&value.is_boolean()&&required.count("fullscreen"),"Unsupported video setting","E_UNSUPPORTED_SETTING");}
            else if(group.key()=="audio"){must((key=="music_volume"||key=="effects_volume")&&value.is_number_integer()&&value>=0&&value<=100&&required.count("separate_volumes"),"Unsupported volume setting","E_UNSUPPORTED_SETTING");}
            else if(group.key()=="blues"){
                must(gates.count(key)&&required.count(gates.at(key)),"Unsupported Blues setting","E_UNSUPPORTED_SETTING");
                if(key=="camera")must(value=="classic"||value=="smooth","Invalid camera");
                else if(key=="hud")must(value=="classic"||value=="fixed","Invalid HUD");
                else if(key=="sound_mode")must(value=="auto"||value=="dos"||value=="amiga","Invalid sound mode");
                else if(key=="invulnerable"||key=="infinite_lives")must(value.is_boolean(),"Invalid assist");
                else must(value.is_number_integer()&&value>=(key.find("lives_")==0?1:0)&&value<=(key=="start_level"?5:key=="view_step"?8:99),"Invalid numeric setting");
            }else throw Error("E_UNSUPPORTED_SETTING","Unknown settings group");
        }
    }
}
}

namespace titus {
namespace {
void features_valid(const Json& list,const Json& r){must(list.is_array(),"Features must be an array","E_PROTOCOL");std::set<std::string> unique;auto available=blues_features(r.at("sources").size()==2);
    for(const auto& f:list)must(f.is_string()&&std::find(available.begin(),available.end(),f)!=available.end()&&unique.insert(f.get<std::string>()).second,"Invalid/duplicate capability","E_PROTOCOL");
}
void error_valid(const Json& e){fields(e,{"code","message"});must(e["code"].is_string()&&std::regex_match(e["code"].get<std::string>(),std::regex("[A-Z][A-Z0-9_]{0,63}"))&&e["message"].is_string()&&!e["message"].get<std::string>().empty()&&e["message"].get<std::string>().size()<=1024,"Invalid protocol error","E_PROTOCOL");}
void response_identity(const Json& d,const Json& r){must(d["schema_version"]==1&&d["game_id"]=="blues"&&d["engine_id"]=="blues-family","Runtime response identity mismatch","E_PROTOCOL");if(d.contains("profile_id"))must(d["profile_id"]==r["profile_id"]&&d["save_compatibility_id"]==r["save_compatibility_id"],"Runtime response belongs to another profile","E_PROTOCOL");}
}
void validate_runtime_event(const Json& e,const Json& r){
    fields(e,{"schema_version","kind","session_id","seq","event","body"});must(e["schema_version"]==1&&e["kind"]=="event"&&e["session_id"]==r["session_id"]&&e["seq"].is_number_integer()&&e["seq"]>0&&e["event"].is_string()&&e["body"].is_object(),"Invalid event header","E_PROTOCOL");
    auto name=e["event"].get<std::string>();const auto& b=e["body"];
    if(name=="hello"){fields(b,{"engine_id","build_revision","protocol"});must(b["engine_id"]=="blues-family"&&b["protocol"]==1&&b["build_revision"].is_string()&&!b["build_revision"].get<std::string>().empty()&&b["build_revision"].get<std::string>().size()<=1024,"Invalid hello","E_PROTOCOL");}
    else if(name=="ready"){fields(b,{"features","scene","save_slot_count","audio_active"});features_valid(b["features"],r);must((b["scene"]=="title"||b["scene"]=="menu"||b["scene"]=="gameplay")&&b["save_slot_count"].is_number_integer()&&b["save_slot_count"]==8&&b["audio_active"].is_boolean(),"Invalid ready","E_PROTOCOL");}
    else if(name=="progress"){fields(b,{"phase","percent"});must((b["phase"]=="validate"||b["phase"]=="prepare"||b["phase"]=="initialize")&&b["percent"].is_number_integer()&&b["percent"]>=0&&b["percent"]<=100,"Invalid progress","E_PROTOCOL");}
    else if(name=="return_requested"){fields(b,{"confirmation_required"});must(b["confirmation_required"].is_boolean(),"Invalid return request","E_PROTOCOL");}
    else if(name=="closing"||name=="return_cancelled")fields(b,{});
    else if(name=="error")error_valid(b);
    else throw Error("E_PROTOCOL","Unknown event");
}
void validate_runtime_result(const Json& d,const Json& r){fields(d,{"schema_version","kind","session_id","game_id","engine_id","outcome","ready","native_exit_code","error"});response_identity(d,r);
    must(d["kind"]=="result"&&d["session_id"]==r["session_id"]&&d["ready"].is_boolean()&&(d["native_exit_code"].is_null()||d["native_exit_code"].is_number_integer()),"Invalid result","E_PROTOCOL");
    auto outcome=d["outcome"];must(outcome=="returned"||outcome=="quit"||outcome=="cancelled"||outcome=="load_failed"||outcome=="runtime_failed"||outcome=="unsupported_protocol","Unknown outcome","E_PROTOCOL");
    if(!d["error"].is_null())error_valid(d["error"]);
    if(outcome=="returned"||outcome=="quit")must(d["ready"]==true&&d["native_exit_code"]==0&&d["error"].is_null(),"Unsuccessful result claimed success","E_PROTOCOL");
}
void validate_runtime_inspection(const Json& d,const Json& r){fields(d,{"schema_version","kind","inspection_id","profile_id","game_id","engine_id","save_compatibility_id","status","features","save_slot_count","preparation_ids","save_index","error"});response_identity(d,r);
    must(d["kind"]=="inspection"&&d["inspection_id"]==r["session_id"]&&d["status"]=="ok"&&d["save_slot_count"]==8&&d["preparation_ids"].is_array()&&d["preparation_ids"].empty()&&d["error"].is_null(),"Failed or invalid inspection","E_PROTOCOL");features_valid(d["features"],r);
    const auto& index=d["save_index"];fields(index,{"schema_version","kind","profile_id","game_id","engine_id","save_compatibility_id","slot_count","slots"});response_identity(index,r);must(index["kind"]=="save_index"&&index["slot_count"]==8&&index["slots"].is_array()&&index["slots"].size()<=8,"Invalid save index","E_PROTOCOL");
    std::set<int> slots;for(const auto& slot:index["slots"]){fields(slot,{"slot","file","format_id","format_version","edition_id","resource_fingerprint","native_resource_crc32","payload_sha256","has_preview","status"});
        must(slot["slot"].is_number_integer()&&slot["slot"]>=1&&slot["slot"]<=8&&slots.insert(slot["slot"].get<int>()).second,"Invalid/duplicate slot","E_PROTOCOL");char filename[32];snprintf(filename,sizeof filename,"saves/slot%02d.bbs",slot["slot"].get<int>());
        must(slot["file"]==filename&&slot["format_id"]=="blues.bbs"&&(slot["format_version"].is_null()||slot["format_version"]==1||slot["format_version"]==2)&&(slot["edition_id"].is_null()||slot["edition_id"]=="dos"||slot["edition_id"]=="amiga")&&slot["has_preview"].is_boolean(),"Invalid slot metadata","E_PROTOCOL");
        for(auto key:{"resource_fingerprint","native_resource_crc32","payload_sha256"})if(!slot[key].is_null())must(slot[key].is_string()&&std::regex_match(slot[key].get<std::string>(),std::regex(key==std::string("native_resource_crc32")?"[0-9a-f]{8}":"[0-9a-f]{64}")),"Invalid slot hash","E_PROTOCOL");
        auto status=slot["status"];must(status=="ready"||status=="incompatible"||status=="damaged"||status=="unreadable","Invalid save status","E_PROTOCOL");
        if(status=="ready")must(!slot["format_version"].is_null()&&!slot["edition_id"].is_null()&&!slot["native_resource_crc32"].is_null()&&!slot["payload_sha256"].is_null(),"Incomplete ready slot","E_PROTOCOL");
        must(slot["format_version"]!=1||slot["has_preview"]==false,"v1 cannot contain preview","E_PROTOCOL");
    }
}
}
