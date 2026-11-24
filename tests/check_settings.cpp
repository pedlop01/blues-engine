#include "titus/settings.hpp"
#include <iostream>
using namespace titus;
int main(){auto root=fs::temp_directory_path()/fs::u8path("titus-settings ñ "+uuid());try{
 auto paths=Paths::resolve(root);SettingsStore store(paths);auto must=[](bool v){if(!v)throw Error("E_TEST","Settings assertion failed");};auto rejects=[&](auto fn){try{fn();}catch(const std::exception&){return;}must(false);};
 Json id={{"profile_id",uuid()},{"game_id","blues"},{"save_compatibility_id","blues.full.1991"},{"edition_id","dos"}};
 auto initial=store.load();store.seed(id,{{"audio",{{"music_volume",42}}},{"blues",{{"camera","classic"}}}});auto doc=store.load();must(store.resolve(doc,id)["audio"]["music_volume"]==42);
 doc=store.edit(doc,id,"global",{{"audio",{{"music_volume",70},{"effects_volume",80}}},{"blues",{{"camera","smooth"}}}});must(store.resolve(doc,id)["audio"]["music_volume"]==70);
 rejects([&]{store.edit(initial,id,"global",Json::object());});
 doc=store.edit(doc,id,"game",{{"audio",{{"music_volume",60}}}});doc=store.edit(doc,id,"edition",{{"audio",{{"music_volume",50}}}});must(store.resolve(doc,id)["audio"]["music_volume"]==50);
 auto amiga=id;amiga["edition_id"]="amiga";must(store.resolve(doc,amiga)["audio"]["music_volume"]==60);must(store.resolve(doc,id,{{"audio",{{"music_volume",10}}}})["audio"]["music_volume"]==10);must(store.load()==doc);
 auto fox=id;fox["game_id"]="fox";fox["save_compatibility_id"]="fox.full.audited";auto f=store.resolve(doc,fox);must(f.contains("fox")&&f["audio"]["music_volume"]==70);must(store.resolve(doc,fox,{{"audio",{{"music_volume",90}}}})["audio"]["music_volume"]==90);rejects([&]{store.resolve(doc,id,{{"input",{{"action_button",1}}}});});
 auto fox_amiga=fox;fox_amiga["edition_id"]="amiga";fox_amiga["save_compatibility_id"]="fox.amiga.full.audited";auto amiga_values=store.resolve(doc,fox_amiga);must(amiga_values["audio"]["music_volume"]==70 && amiga_values["fox"]["camera"]=="smooth" && amiga_values["fox"]["start_level"]==1);rejects([&]{store.resolve(doc,fox_amiga,{{"fox",{{"start_level",15}}}});});must(store.resolve(doc,fox_amiga,{{"fox",{{"start_level",16},{"lives",0}}}})["fox"]["lives"]==0);
 doc=store.edit(doc,id,"edition",Json::object());must(store.resolve(doc,id)["audio"]["music_volume"]==60);
 auto work=root/"native-fox";ensure_directory(work);auto request=fox;request["paths"]={{"work_directory",{{"path",work.u8string()}}}};request["action"]={{"type","new_game"}};write_document(work/"preferences.json",{{"schema_version",1},{"fullscreen",false}});store.seed(request,native_settings(request));request["settings"]=store.resolve(store.load(),request);store.begin(request);
 write_document(work/"preferences.json",{{"schema_version",1},{"fullscreen",true}});store.recover(request);must(native_settings(request)["video"]["fullscreen"]==false);must(!path_exists(work/"managed-settings-pending.json"));
 // A native menu change is persistent, whereas an explicitly temporary override is restored.
 write_document(work/"preferences.json",{{"schema_version",1},{"fullscreen",true}});store.collect(request,Json::object());must(store.resolve(store.load(),request)["video"]["fullscreen"]==true);
 request["settings"]["video"]["fullscreen"]=false;store.begin(request);write_document(work/"preferences.json",{{"schema_version",1},{"fullscreen",false}});store.collect(request,{{"video",{{"fullscreen",false}}}});must(native_settings(request)["video"]["fullscreen"]==true);doc=store.load();
 auto file=paths.config/"game-settings.json";auto future=doc;future["schema_version"]=2;write_document(file,future);auto original=read_document(file);rejects([&]{store.load();});must(read_document(file)==original);
 fs::remove_all(root); auto pre1_amiga=fox_amiga;pre1_amiga["game_id"]="prehistorik";pre1_amiga["save_compatibility_id"]="prehistorik.amiga.full.audited";auto pre1_values=store.resolve(doc,pre1_amiga);must(pre1_values["prehistorik"]["camera"]=="smooth"&&pre1_values["audio"]["music_volume"]==70);auto pre1_dos=pre1_amiga;pre1_dos["edition_id"]="dos";pre1_dos["save_compatibility_id"]="prehistorik.full.audited";must(store.resolve(doc,pre1_dos).contains("prehistorik"));must(store.resolve(doc,pre1_dos,{{"prehistorik",{{"invulnerable",true}}}})["prehistorik"]["invulnerable"]==true);
for(const auto& game:{"jukebox","prehistorik2"}){
 auto family=fox;family["game_id"]=game;family["save_compatibility_id"]=std::string(game)+".full.audited";
 auto values=store.resolve(doc,family);must(values[game]["camera"]=="smooth"&&values[game]["render_rate"]==60);
 const int last=std::string(game)=="jukebox"?31:15;
 must(store.resolve(doc,family,{{game,{{"start_level",last},{"lives",99},{"render_rate",120}}}})[game]["start_level"]==last);
 rejects([&]{store.resolve(doc,family,{{game,{{"start_level",last+1}}}});});
 rejects([&]{store.resolve(doc,family,{{game,{{"lives",0}}}});});
 rejects([&]{store.resolve(doc,family,{{"blues",{{"sound_mode","amiga"}}}});});
}
std::cout<<"settings: precedence, inheritance, edition/game isolation, session overrides, stale revision and future schema passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';fs::remove_all(root);return 1;}}
