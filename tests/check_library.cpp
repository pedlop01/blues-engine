#include "library.hpp"
#include <fstream>
#include <iostream>
using namespace titus;
int main(){fs::path root=fs::temp_directory_path()/fs::u8path("titus-library-á "+uuid());
    try{
        auto paths=Paths::resolve(root/"user");Library library(paths);
        auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
        check(library.healthy&&library.games.empty(),"empty library initialized");
        Json catalog=library.catalog;
        auto add=[&](const std::string& game,const std::string& edition,const std::string& status){
            auto dir=root/fs::u8path(game+edition);fs::create_directories(dir);
            auto id=uuid();catalog["installations"].push_back({{"id",id},{"game_id",game},{"edition_id",edition},
              {"engine_id",game=="fox"?Json(nullptr):Json("blues-family")},{"variant_id","test-full"},{"fingerprint",sha256(game+edition)},
              {"save_compatibility_id",game+".test"},{"status",status},{"preparation_id",nullptr},
              {"locations",Json::array({{{"kind","directory"},{"location",{{"platform",platform()},{"path",dir.u8string()}}}}})}});return id;};
        auto dos=add("blues","dos","recognized"),amiga=add("blues","amiga","recognized"),fox=add("fox","amiga","missing_engine");
        validate_catalog(catalog);std::ofstream(paths.catalog())<<catalog.dump();library.reload();
        check(library.games.size()==2&&library.selected()->at("id")==dos,"games grouped, DOS preferred");
        library.select_edition("amiga");check(library.selected()->at("id")==amiga,"edition selected");
        library.choose_game("fox");check(library.selected()->at("id")==fox&&library.data_status().find("sin motor")!=std::string::npos,"unsupported Amiga explained");
        library.choose_game("blues");check(library.selected()->at("id")==amiga,"per-game choice retained");
        auto disk=root/"source.adf";std::ofstream(disk)<<"test image";
        for(auto& i:catalog["installations"])if(i["id"]==amiga)i["locations"].push_back({{"kind","adf"},{"location",{{"platform",platform()},{"path",disk.u8string()}}}});
        std::ofstream(paths.catalog())<<catalog.dump();library.reload();
        library.select_source(1);check(library.source()->at("kind")=="adf"&&library.data_status().find("extraer")!=std::string::npos,"ADF selection explained");
        library.persist();Library image_choice(paths);check(image_choice.source()->at("kind")=="adf","source choice restored");
        library.select_source(1);library.persist();check(library.preference_error.empty(),"preferences saved");
        Library reopened(paths);check(reopened.selected()->at("id")==amiga,"selection restored after restart");
        check(!reopened.launch_reason().empty(),"launch remains gated until adapter exists");
        fs::remove_all(root/"bluesamiga");check(!reopened.source_available()&&reopened.data_status()=="Ubicación no disponible","moved source detected");
        reopened.catalog["installations"].erase(reopened.catalog["installations"].begin()+1);reopened.reconcile();
        check(reopened.selected()->at("id")==dos,"removed selection repaired");
        auto previous=reopened.preferences;previous["schema_version"]=2;std::ofstream(paths.config/"library.json")<<previous.dump();
        reopened.dirty=true;reopened.persist();check(!reopened.preference_error.empty(),"future preferences cannot be overwritten");
        std::ifstream in(paths.config/"library.json");Json kept;in>>kept;check(kept["schema_version"]==2,"future preferences retained");in.close();
        fs::remove_all(root);std::cout<<"OK: library identity, selection, availability and preference persistence\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';std::error_code error;fs::remove_all(root,error);return 1;}}
