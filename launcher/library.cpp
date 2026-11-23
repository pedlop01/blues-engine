#include "library.hpp"
#include <algorithm>
#include <map>
namespace titus {
const std::vector<GameInfo> game_info={
 {"blues","The Blues Brothers","BB","Dos hermanos. Una banda. Mucho ritmo.",0x38d9cf},
 {"jukebox","Jukebox Adventure","JA","La siguiente aventura de los Blues Brothers.",0xeda95f},
 {"fox","Titus the Fox","TF","Una aventura de plataformas con mucho ingenio.",0xf07869},
 {"prehistorik","Prehistorik","P1","De vuelta a la edad de piedra.",0xc3ce74},
 {"prehistorik2","Prehistorik 2","P2","Más aventuras para nuestro cavernícola.",0x9b9cf0}};
std::string friendly_error(const std::exception& error) {
    auto e=dynamic_cast<const Error*>(&error);if(!e)return error.what();
    const std::map<std::string,std::string> messages={
      {"E_NO_MATCH","No se han encontrado juegos compatibles en esta ubicación."},
      {"E_AMBIGUOUS","Hay archivos con el mismo nombre y contenido distinto. Resuelve el conflicto antes de añadirlos."},
      {"E_SOURCE_OVERLAP","Elige una carpeta de juegos separada de los datos privados de Titus Engine."},
      {"E_REVISION","El catálogo ha cambiado en otra ventana. Actualízalo y vuelve a intentarlo."},
      {"E_IDENTITY_MISMATCH","La ubicación no contiene la misma versión de este juego."},
      {"E_SOURCE_MISSING","La ubicación no está disponible. Comprueba la ruta."},
      {"E_SCHEMA_VERSION","El archivo pertenece a una versión diferente de Titus Engine. Se ha conservado sin cambios."},
      {"E_JSON","No se pudo leer el archivo. Se ha conservado sin cambios."},
      {"E_LOCKED","Otra ventana está actualizando la biblioteca. Inténtalo de nuevo."},
      {"E_NO_LOCAL_SOURCE","Esta instalación solo tiene ubicaciones de otro sistema. Añade una ubicación local."},
      {"E_INSTALLATION_CHANGED","Estos archivos han cambiado. Usa Verificar antes de volver a registrarlos."},
      {"E_DUPLICATE_IDENTITY","Esta versión ya está registrada con otra instalación."}};
    auto found=messages.find(e->code);return found==messages.end()?error.what():found->second;
}
std::string source_key(const Json& source) {return sha256(source.dump());}
Library::Library(Paths paths,Detector detector):store(std::move(paths),std::move(detector)) {
    preferences={{"schema_version",1},{"kind","library_preferences"},{"selected_game",""},
        {"selected_installations",Json::object()},{"selected_locations",Json::object()},
        {"fullscreen",false},{"window_width",1100},{"window_height",720},{"browse_path",""}};
    try {auto loaded=store.load_library_preferences();if(!loaded.is_null())preferences=loaded;}
    catch(const std::exception& e){preference_error=std::string("Preferencias: ")+friendly_error(e);}
    reload();
}
void Library::reload() {
    try {catalog=store.init();healthy=true;error.clear();reconcile();}
    catch(const std::exception& e) {healthy=false;error=friendly_error(e);catalog={{"installations",Json::array()}};games.clear();}
}
void Library::reconcile() {
    games.clear();
    for(const auto& g:game_info)for(const auto& i:catalog.at("installations"))if(i.at("game_id")==g.id){games.push_back(g.id);break;}
    auto game=preferences.at("selected_game").get<std::string>();
    if(std::find(games.begin(),games.end(),game)==games.end()) {preferences["selected_game"]=games.empty()?"":games[0];dirty=true;}
    // Drop references removed by another client; keep choices for all remaining games.
    auto& chosen=preferences["selected_installations"];
    for(auto it=chosen.begin();it!=chosen.end();) {
        bool found=false;for(const auto& i:catalog["installations"])if(i["id"]==it.value()&&i["game_id"]==it.key())found=true;
        if(!found){it=chosen.erase(it);dirty=true;}else ++it;
    }
    auto& sources=preferences["selected_locations"];
    for(auto it=sources.begin();it!=sources.end();) {
        bool found=false;for(const auto& i:catalog["installations"])if(i["id"]==it.key())
            for(const auto& l:i["locations"])if(source_key(l)==it.value())found=true;
        if(!found){it=sources.erase(it);dirty=true;}else ++it;
    }
}
const GameInfo& Library::info() const {
    for(const auto& g:game_info)if(preferences["selected_game"]==g.id)return g;
    return game_info[0];
}
Json Library::installations() const {
    Json result=Json::array();for(const auto& i:catalog.at("installations"))if(i["game_id"]==preferences["selected_game"])result.push_back(i);
    std::stable_sort(result.begin(),result.end(),[](const Json& a,const Json& b){return a["edition_id"]>b["edition_id"];});
    return result;
}
const Json* Library::selected() const {
    const auto& chosen=preferences["selected_installations"];auto game=preferences["selected_game"].get<std::string>();
    if(chosen.contains(game))for(const auto& i:catalog.at("installations"))if(i["game_id"]==game&&i["id"]==chosen.at(game))return &i;
    // Prefer DOS when no per-game selection has been made.
    for(const auto& i:catalog.at("installations"))if(i["game_id"]==game&&i["edition_id"]=="dos")return &i;
    for(const auto& i:catalog.at("installations"))if(i["game_id"]==game)return &i;
    return nullptr;
}
const Json* Library::source() const {
    auto i=selected();if(!i)return nullptr;auto id=i->at("id").get<std::string>();const auto& chosen=preferences["selected_locations"];
    if(chosen.contains(id))for(const auto& l:i->at("locations"))if(source_key(l)==chosen.at(id))return &l;
    // Prefer an existing directory on this platform over an unprepared disk image.
    for(const auto& l:i->at("locations"))if(l["kind"]=="directory"&&l["location"]["platform"]==platform()) {
        std::error_code e;if(fs::is_directory(fs::u8path(l["location"]["path"].get<std::string>()),e))return &l;
    }
    for(const auto& l:i->at("locations"))if(l["location"]["platform"]==platform())return &l;
    return &i->at("locations")[0];
}
void Library::choose_game(const std::string& game) {
    if(std::find(games.begin(),games.end(),game)!=games.end()&&preferences["selected_game"]!=game){preferences["selected_game"]=game;dirty=true;}
}
void Library::select_game(int delta) {
    if(games.empty())return;auto it=std::find(games.begin(),games.end(),preferences["selected_game"].get<std::string>());
    auto n=static_cast<int>(games.size());int index=it==games.end()?0:static_cast<int>(it-games.begin());choose_game(games[(index+delta%n+n)%n]);
}
void Library::select_installation(int delta) {
    auto choices=installations();if(choices.empty())return;auto current=selected();int index=0,n=static_cast<int>(choices.size());
    for(int j=0;j<n;++j)if(current&&choices[j]["id"]==current->at("id"))index=j;
    preferences["selected_installations"][info().id]=choices[(index+delta%n+n)%n]["id"];dirty=true;
}
void Library::select_edition(const std::string& edition) {
    auto current=selected();if(current&&current->at("edition_id")==edition)return;
    for(const auto& i:installations())if(i["edition_id"]==edition){preferences["selected_installations"][info().id]=i["id"];dirty=true;return;}
}
void Library::select_source(int delta) {
    auto i=selected(),s=source();if(!i||!s)return;const auto& sources=i->at("locations");int index=0,n=static_cast<int>(sources.size());
    for(int j=0;j<n;++j)if(sources[j]==*s)index=j;
    preferences["selected_locations"][i->at("id").get<std::string>()]=source_key(sources[(index+delta%n+n)%n]);dirty=true;
}
bool Library::source_available() const {
    auto s=source();if(!s||s->at("location").at("platform")!=platform())return false;
    std::error_code e;auto p=fs::u8path(s->at("location").at("path").get<std::string>());
    return s->at("kind")=="directory"?fs::is_directory(p,e):fs::is_regular_file(p,e);
}
std::string Library::data_status() const {
    auto i=selected();if(!i)return "Sin instalaciones";
    if(!source_available())return "Ubicación no disponible";
    auto status=i->at("status");
    if(status=="incomplete")return "Faltan archivos del juego";
    if(status=="unverified")return "Archivos sin verificar";
    if(status=="missing_engine")return "Edición Amiga sin motor disponible";
    if(source()->at("kind")=="adf")return "Disco Amiga pendiente de extraer";
    if(status=="needs_preparation")return "Preparación de recursos pendiente";
    return "Conjunto de archivos verificado";
}
bool Library::can_play() const {
    auto i=selected();if(!i||!(runtime_games.count(i->at("game_id"))||(blues_runtime_installed&&i->at("game_id")=="blues"))||i->at("status")!="recognized"||!source_available()||source()->at("kind")!="directory")return false;
    if(i->at("game_id")!="blues"&&i->at("edition_id")!="dos")return false;
    if(i->at("game_id")=="blues"&&i->at("edition_id")=="amiga"){
        for(const auto& other:catalog.at("installations"))if(other.at("game_id")=="blues"&&other.at("edition_id")=="dos"&&other.at("status")=="recognized"&&other.at("save_compatibility_id")==i->at("save_compatibility_id"))
            for(const auto& l:other.at("locations"))if(l.at("kind")=="directory"&&l.at("location").at("platform")==platform()){
                std::error_code e;if(fs::is_directory(fs::u8path(l.at("location").at("path").get<std::string>()),e))return true;
            }
        return false;
    }
    return true;
}
std::string Library::launch_reason() const {
    auto i=selected();if(!i)return "Añade una carpeta para empezar tu biblioteca.";
    if(!source_available())return "Vuelve a localizar esta instalación para recuperar sus archivos.";
    if(i->at("status")=="missing_engine")return "Esta edición Amiga todavía no tiene un motor compatible.";
    if(i->at("status")=="incomplete"||i->at("status")=="unverified")return "Revisa los archivos antes de iniciar este juego.";
    if(source()->at("kind")=="adf")return "Esta imagen de disco requiere una extracción compatible.";
    if(can_play()&&i->at("game_id")!="blues")return "Flechas: mover. Shift: saltar. Espacio: accion. P: pausa. Esc: biblioteca.";
    if(i->at("game_id")=="blues"&&blues_runtime_installed){
        if(!can_play())return "Esta edición Amiga necesita también la carpeta DOS para el nivel Concierto.";
        return "Esc abre la pausa. Al salir del juego volverás a esta biblioteca.";
    }
    return "El inicio de este juego se incorporará en una próxima entrega.";
}
void Library::persist() {
    if(!healthy||!dirty)return;
    try {store.save_library_preferences(preferences);dirty=false;preference_error.clear();}
    catch(const std::exception& e){preference_error=std::string("No se pudo guardar la selección: ")+friendly_error(e);}
}
}
