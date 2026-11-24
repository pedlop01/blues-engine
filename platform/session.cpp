#include "titus/session.hpp"
#include <algorithm>
#include <fstream>
#include <thread>
#ifdef _WIN32
#include <cwchar>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace titus {
namespace {
void ensure(bool b,const std::string& s,const char* code="E_SESSION"){if(!b)throw Error(code,s);}
Json location(const fs::path& p){return {{"platform",platform()},{"path",normalized_path(p).u8string()}};}
Json source(const Json& i,const Json& l){return {{"installation_id",i.at("id")},{"game_id",i.at("game_id")},{"edition_id",i.at("edition_id")},{"variant_id",i.at("variant_id")},{"fingerprint",i.at("fingerprint")},{"kind","directory"},{"location",l.at("location")},{"read_only",true}};}
Json directory_location(const Json& i){for(const auto& l:i.at("locations"))if(l["kind"]=="directory"&&l["location"]["platform"]==platform()){
    std::error_code e;if(fs::is_directory(fs::u8path(l["location"]["path"].get<std::string>()),e))return l;}return nullptr;}
Json make_inspection(const Json& launch){auto r=launch;for(auto k:{"action","required_features","settings","allow_silent_audio"})r.erase(k);r["kind"]="inspect_request";r["inspection_id"]=r["session_id"];r.erase("session_id");return r;}
std::vector<fs::path> profile_children(const fs::path& path){std::vector<fs::path> out;
#ifdef _WIN32
    WIN32_FIND_DATAW data;HANDLE handle=FindFirstFileW(filesystem_path(path/"*").c_str(),&data);if(handle==INVALID_HANDLE_VALUE){ensure(GetLastError()==ERROR_FILE_NOT_FOUND,"No se pudo leer el perfil.","E_IMPORT");return out;}DWORD error=ERROR_SUCCESS;do{if(wcscmp(data.cFileName,L".")&&wcscmp(data.cFileName,L".."))out.push_back(path/data.cFileName);}while(FindNextFileW(handle,&data));error=GetLastError();FindClose(handle);ensure(error==ERROR_NO_MORE_FILES,"No se pudo completar la lectura del perfil.","E_IMPORT");
#else
    for(const auto& entry:fs::directory_iterator(path))out.push_back(entry.path());
#endif
    return out;
}
void copy_profile_tree(const fs::path& from,const fs::path& to,int depth,size_t& count){ensure(depth<32&&count<4096,"Perfil demasiado grande para importar.","E_IMPORT");ensure_directory(to);for(const auto& file:profile_children(from)){++count;ensure(count<4096&&!path_is_symlink(file),"El perfil contiene enlaces o demasiados archivos.","E_IMPORT");auto target=to/file.filename();if(path_is_directory(file))copy_profile_tree(file,target,depth+1,count);else{ensure(path_is_regular_file(file)&&path_file_size(file)<=32*1024*1024,"Archivo de perfil no compatible con la importación: "+file.u8string(),"E_IMPORT");auto hash=file_sha256(file);copy_file_exact(file,target);ensure(file_sha256(target)==hash&&file_sha256(file)==hash,"El perfil cambió durante la copia.","E_IMPORT");}}}
void channel(const fs::path& p){std::ofstream out(filesystem_path(p),std::ios::binary);if(!out)throw Error("E_WRITE","Cannot create session channel");}
}
Session::Session(Paths paths,const fs::path& root,const Json& old_catalog,const std::string& installation_id,SessionOptions options):paths_(std::move(paths)),options_(std::move(options)){
    auto catalog=CatalogStore(paths_).load();ensure(catalog.at("revision")==old_catalog.at("revision"),"El catálogo cambió; actualiza la biblioteca.","E_REVISION");
    const Json* primary=nullptr;for(const auto& i:catalog["installations"])if(i["id"]==installation_id)primary=&i;
    ensure(primary&&(primary->at("game_id")=="blues"||primary->at("game_id")=="jukebox"||primary->at("game_id")=="prehistorik2"||primary->at("game_id")=="fox"||primary->at("game_id")=="prehistorik")&&(primary->at("status")=="recognized"||primary->at("status")=="needs_preparation"),"Esta instalación no está disponible para jugar.","E_DATA_VARIANT");
    auto selected=options_.primary_source.is_null()?directory_location(*primary):options_.primary_source;
    ensure(!selected.is_null()&&selected["kind"]=="directory"&&selected["location"]["platform"]==platform()&&std::find(primary->at("locations").begin(),primary->at("locations").end(),selected)!=primary->at("locations").end(),"Elige una carpeta disponible; los ADF requieren extracción.","E_DATA_MISSING");
    auto package=normalized_path(root);while(package.filename().empty()&&package.parent_path()!=package)package=package.parent_path();auto registry=parse_json(read_document(package/"engines/registry.json"));
    ensure(registry.at("schema_version")==1&&registry.at("kind")=="engine_registry"&&registry.at("role")=="package","Registro de motores no instalado.","E_ENGINE");
    std::string relative;bool declared=false;
    for(const auto& entry:registry.at("engines"))if(entry.at("engine_id")==primary->at("engine_id")&&entry.at("adapter_state")=="validated"&&entry.at("expected_protocol")==1){
        relative=entry.at("executables").at(platform());
        for(const auto& game:entry.at("games"))if(game.at("game_id")==primary->at("game_id"))
            for(const auto& edition:game.at("editions"))if(edition==primary->at("edition_id"))declared=true;
    }
    ensure(declared,"El paquete instalado no incluye este juego/edición.","E_ENGINE");
    auto rel=fs::u8path(relative);ensure(!relative.empty()&&!rel.is_absolute(),"Ruta de motor no válida.","E_ENGINE");for(const auto& part:rel)ensure(part!="..","Ruta de motor no válida.","E_ENGINE");
    executable_=normalized_path(package/rel);auto parent=package.begin(),child=executable_.begin();for(;parent!=package.end();++parent,++child)ensure(child!=executable_.end()&&
#ifdef _WIN32
        _wcsicmp(parent->c_str(),child->c_str())==0,
#else
        *parent==*child,
#endif
        "Motor fuera del paquete: "+executable_.u8string()+" ("+package.u8string()+")","E_ENGINE");
    ensure(fs::is_regular_file(executable_),"Falta el motor de esta familia de juegos.","E_ENGINE");
    auto game=primary->at("game_id").get<std::string>();
    auto profile_id=catalog["profiles"][0]["id"].get<std::string>(),compat=primary->at("save_compatibility_id").get<std::string>();
    profile_=paths_.profile_directory(profile_id,game,compat);
    Json sources=Json::array({source(*primary,selected)});
    for(const auto& i:catalog["installations"])if(game=="blues"&&i["game_id"]==game&&i["edition_id"]!=primary->at("edition_id")&&i["save_compatibility_id"]==compat&&i["status"]=="recognized"){
        auto l=directory_location(i);if(!l.is_null()){sources.push_back(source(i,l));break;}}
    for(const auto& s:sources)paths_.protect_source(fs::u8path(s["location"]["path"].get<std::string>()));
    lock_=std::make_unique<ProfileLock>(paths_.data/"profile-locks"/fs::u8path(profile_id+"."+game+"."+compat+".lock"));
    auto id=uuid();directory_=paths_.cache/"sessions"/id;
    request_={{"schema_version",1},{"kind","launch"},{"session_id",id},{"profile_id",profile_id},{"installation_id",installation_id},{"game_id",game},{"engine_id",primary->at("engine_id")},{"edition_id",primary->at("edition_id")},{"save_compatibility_id",compat},{"platform",platform()},{"sources",sources},
      {"paths",{{"profile_directory",location(profile_)},{"work_directory",location(profile_/fs::u8path("native/"+primary->at("engine_id").get<std::string>()))},{"session_directory",location(directory_)}}},
      {"action",{{"type","new_game"}}},{"required_features",Json::array({"return_to_library","native_pause","save_slots","save_thumbnails","rewind","dynamic_view","smooth_camera","hud_modes","separate_volumes"})},{"settings",Json::object()},{"allow_silent_audio",false}};
    if(game!="blues")request_["required_features"]=runtime_features(request_);
    if(options_.load_slot)request_["action"]={{"type","load_slot"},{"slot",options_.load_slot}};
    // Validate before creating profile or copying legacy files.
    validate_engine_request(request_,directory_/"request.json");migrate();
    ensure_directory(directory_);start_inspection();
}
Session::~Session()=default;
void Session::start_inspection(bool prepare){
    if(process_){auto id=uuid();directory_=paths_.cache/"sessions"/id;ensure_directory(directory_);request_["session_id"]=id;request_["paths"]["session_directory"]=location(directory_);}
    auto inspection=make_inspection(request_);write_document(directory_/"request.json",inspection);
    process_=std::make_unique<Process>();
    std::map<std::string,std::string> env={{"TITUS_TEST_RUNTIME_MS",std::to_string(options_.test_ms)},{"TITUS_TEST_FAULT",options_.test_fault}};
    process_->start(executable_,{prepare?"--prepare-request":"--inspect-request",(directory_/"request.json").u8string()},executable_.parent_path(),env,directory_,*lock_);
    state=prepare?State::Preparing:State::Inspecting;started_=last_event_=std::chrono::steady_clock::now();message_=prepare?"Preparando los datos del juego…":"Comprobando datos y perfil…";
}
void Session::migrate(){
    if(request_["game_id"]!="blues"){ensure_directory(profile_/fs::u8path("native/"+request_["engine_id"].get<std::string>()));return;}
    if(!options_.legacy_import&&path_exists(profile_)&&!path_is_empty(profile_)){ensure(path_is_directory(profile_),"El perfil no es un directorio.");ensure_directory(profile_/"native/blues-family");ensure_directory(profile_/"saves");return;}
    staging_=profile_.parent_path()/fs::u8path("."+profile_.filename().u8string()+".import-"+uuid());
    ensure_directory(staging_/"native/blues-family");ensure_directory(staging_/"saves");
    if(options_.legacy_import&&path_exists(profile_)){
        size_t count=0;copy_profile_tree(profile_,staging_,0,count);
    }
    Json files=Json::array();
    auto copy=[&](const fs::path& from,const fs::path& relative){if(from.empty()||!path_exists(from))return;ensure(path_is_regular_file(from)&&!path_is_symlink(from),"El origen de importación debe ser un archivo regular.","E_IMPORT");ensure(path_file_size(from)<=32*1024*1024,"Archivo legado demasiado grande.","E_IMPORT");
        paths_.protect_source(from);auto hash=file_sha256(from);auto target=staging_/relative;ensure_directory(target.parent_path());if(path_exists(target))remove_regular_file(target);copy_file_exact(from,target);
        ensure(hash==file_sha256(target)&&hash==file_sha256(from),"El archivo legado cambió durante la copia.","E_IMPORT");
        auto snapshot=staging_/"legacy-initial"/relative;ensure_directory(snapshot.parent_path());if(!path_exists(snapshot))copy_file_exact(target,snapshot);
        files.push_back({{"file",relative.generic_u8string()},{"source",location(normalized_path(from))},{"size",path_file_size(target)},{"sha256",hash},{"validation","pending"}});
    };
    copy(options_.legacy.preferences,"native/blues-family/preferences.ini");for(int n=1;n<=8;++n)if(!options_.legacy.saves.empty()){char name[32];snprintf(name,sizeof name,"slot%02d.bbs",n);copy(options_.legacy.saves/name,fs::path("saves")/name);}
    if(options_.legacy_import)remove_regular_file(staging_/"native/blues-family/managed-settings-pending.json");
    write_document(staging_/"migration-receipt.json",{{"schema_version",1},{"kind","legacy_import"},{"state","pending_validation"},{"files",files}});
    request_["paths"]["profile_directory"]=location(staging_);request_["paths"]["work_directory"]=location(staging_/"native/blues-family");
}
bool Session::stalled()const{auto elapsed=std::chrono::steady_clock::now()-last_event_;
    return ((state==State::Inspecting||state==State::Preparing)&&elapsed>std::chrono::seconds(30))||(state==State::Starting&&elapsed>std::chrono::seconds(events_.empty()?5:30))||(state==State::Returning&&closing_&&elapsed>std::chrono::seconds(5));}
void Session::force_stop(){if(process_->poll()){poll();return;}ensure(stalled(),"El proceso aún está respondiendo; solicita la vuelta normal.");process_->terminate();failure_="Cierre forzado. El progreso sin guardar se ha perdido.";terminal_error_=true;}
void Session::request_return(){if(finished())return;cancel_=true;if(state==State::Inspecting||state==State::Preparing){message_="Cancelando el inicio…";return;}
    Json command={{"schema_version",1},{"kind","command"},{"session_id",request_["session_id"]},{"seq",command_seq_+1},{"command","return_to_library"}};
    std::ofstream out(filesystem_path(directory_/"commands.jsonl"),std::ios::binary|std::ios::app);out<<command.dump()<<'\n';out.flush();ensure(bool(out),"No se pudo pedir la vuelta.");++command_seq_;state=State::Returning;last_event_=std::chrono::steady_clock::now();message_="Esperando la confirmación del juego…";
}
void Session::consume_events(){auto bytes=read_document(directory_/"events.jsonl");ensure(offset_<=bytes.size(),"El canal de eventos fue truncado.","E_PROTOCOL");
    while(true){auto end=bytes.find('\n',offset_);if(end==std::string::npos){ensure(bytes.size()-offset_<=16384,"Línea de evento demasiado larga.","E_PROTOCOL");break;}ensure(end-offset_<=16384,"Línea de evento demasiado larga.","E_PROTOCOL");
        auto event=parse_json(bytes.substr(offset_,end-offset_));offset_=end+1;validate_runtime_event(event,request_);ensure(event.size()==6&&event.at("schema_version")==1&&event.at("kind")=="event"&&event.at("session_id")==request_["session_id"]&&event.at("seq")==events_.size()+1&&!closing_,"Evento de otra sesión o secuencia incorrecta.","E_PROTOCOL");
        auto name=event.at("event").get<std::string>();const auto& body=event.at("body");ensure((events_.empty())==(name=="hello"),"Falta hello o está repetido.","E_PROTOCOL");
        if(name=="hello")ensure(body.at("engine_id")==request_["engine_id"]&&body.at("protocol")==1&&body.at("build_revision").is_string(),"Motor/protocolo incorrecto.","E_PROTOCOL");
        else if(name=="ready"){ensure(!ready_&&!terminal_error_&&body.at("audio_active")==true&&body.at("save_slot_count")==runtime_slot_count(request_),"Arranque incompleto o sin audio.","E_PROTOCOL");
            for(const auto& f:request_["required_features"])ensure(std::find(body.at("features").begin(),body.at("features").end(),f)!=body.at("features").end(),"Falta una capacidad requerida.","E_PROTOCOL");
            ready_=true;state=cancel_?State::Returning:State::Playing;message_="Juego en ejecución";}
        else if(name=="progress")ensure(!ready_&&!terminal_error_&&body.at("percent").is_number_integer(),"Progress fuera de arranque.","E_PROTOCOL");
        else if(name=="return_requested"){ensure(ready_&&!terminal_error_&&body.at("confirmation_required").is_boolean(),"Petición de vuelta inválida.","E_PROTOCOL");confirmation_=body["confirmation_required"];state=State::Returning;}
        else if(name=="return_cancelled"){ensure(confirmation_&&ready_&&!terminal_error_,"Cancelación de vuelta inesperada.","E_PROTOCOL");confirmation_=false;cancel_=false;state=State::Playing;}
        else if(name=="closing")closing_=true;
        else if(name=="error"){terminal_error_=true;failure_=body.at("message");}
        else throw Error("E_PROTOCOL","Evento desconocido.");
        events_.push_back(event);last_event_=std::chrono::steady_clock::now();
    }
}
void Session::collect(){
    result_=parse_json(read_document(directory_/"result.json",32*1024));validate_runtime_result(result_,request_);
    ensure(result_.size()==9&&result_.at("schema_version")==1&&result_.at("kind")=="result"&&result_.at("session_id")==request_["session_id"]&&result_.at("game_id")==request_["game_id"]&&result_.at("engine_id")==request_["engine_id"]&&result_.at("ready")==ready_,"Resultado de sesión no válido.","E_PROTOCOL");
    auto outcome=result_.at("outcome");ensure(process_->exit_code()==0&&result_.at("native_exit_code")==0&&closing_&&!terminal_error_&&result_.at("error").is_null()&&((ready_&&(outcome=="returned"||outcome=="quit"))||(!ready_&&outcome=="cancelled")),failure_.empty()?"El juego terminó sin completar la sesión.":failure_,"E_RUNTIME");
    if(ready_)SettingsStore(paths_).collect(request_,options_.settings);
    state=State::Done;message_=ready_?"De vuelta en tu biblioteca.":"Inicio cancelado.";
}
void Session::poll(){if(finished())return;try{
    if(state==State::Inspecting||state==State::Preparing){if(!process_->poll())return;ensure(process_->exit_code()==0,"No se pudieron validar los datos (salida "+std::to_string(process_->exit_code())+"). Consulta el registro de la sesión.","E_INSPECTION");
        inspection_=parse_json(read_document(directory_/"inspection.json",32*1024));validate_runtime_inspection(inspection_,request_);ensure(inspection_.at("kind")=="inspection"&&inspection_.at("schema_version")==1&&inspection_.at("inspection_id")==request_["session_id"]&&inspection_.at("profile_id")==request_["profile_id"]&&inspection_.at("game_id")==request_["game_id"]&&inspection_.at("engine_id")==request_["engine_id"]&&inspection_.at("save_compatibility_id")==request_["save_compatibility_id"]&&inspection_.at("error").is_null()&&inspection_.at("save_slot_count")==runtime_slot_count(request_),"La inspección devolvió un error o pertenece a otro perfil.","E_INSPECTION");
        if(state==State::Preparing){ensure(inspection_.at("status")=="ok","La preparación no se completó.","E_PREPARATION");if(cancel_){state=State::Done;message_="Inicio cancelado.";return;}start_inspection();return;}
        if(inspection_.at("status")=="preparation_required"){
            if(options_.inspect_only||cancel_){state=State::Done;message_=cancel_?"Inicio cancelado.":"Preparación nativa pendiente.";return;}
            ensure(!prepared_,"La preparación no produjo datos válidos.","E_PREPARATION");prepared_=true;start_inspection(true);return;
        }
        if(options_.legacy_import&&cancel_){state=State::Done;message_="Importación cancelada; perfil conservado.";return;}
        if(!staging_.empty()){
            auto receipt=parse_json(read_document(staging_/"migration-receipt.json"));receipt["state"]="complete";
            for(auto& file:receipt["files"]){auto name=file["file"].get<std::string>();file["validation"]=name.find("preferences.ini")!=std::string::npos?"native_versioned_preferences":"unreadable";
                for(const auto& slot:inspection_.at("save_index").at("slots"))if(slot.at("file")==name)file["validation"]=slot.at("status");}
            write_document(staging_/"migration-receipt.json",receipt);
            if(options_.legacy_import&&path_exists(profile_)){auto backup=profile_;backup+=fs::u8path(".before-import-"+uuid());move_directory(profile_,backup);try{move_directory(staging_,profile_);}catch(...){move_directory(backup,profile_);throw;}message_="Importación completada; perfil anterior conservado en "+backup.u8string();}
            else {if(path_exists(profile_)){ensure(path_is_directory(profile_)&&path_is_empty(profile_),"El perfil cambió durante la importación.","E_IMPORT");remove_empty_directory(profile_);}move_directory(staging_,profile_);}staging_.clear();
        }
        if(runtime_slot_count(request_))write_document(profile_/"save-index.json",inspection_["save_index"]);
        if(options_.inspect_only||cancel_){state=State::Done;message_=cancel_?"Inicio cancelado.":"Datos y perfil verificados.";return;}
        auto id=uuid();directory_=paths_.cache/"sessions"/id;ensure_directory(directory_);request_["session_id"]=id;request_["paths"]["session_directory"]=location(directory_);
        request_["paths"]["profile_directory"]=location(profile_);request_["paths"]["work_directory"]=location(profile_/fs::u8path("native/"+request_["engine_id"].get<std::string>()));
        write_document(directory_/"request.json",request_); // Keep a diagnostic request even if preference preflight fails.
        if(options_.load_slot){bool found=false;for(const auto& slot:inspection_["save_index"]["slots"])if(slot["slot"]==options_.load_slot&&slot["status"]=="ready")found=true;ensure(found,"La partida no está disponible o no es compatible.","E_SAVE");}
        SettingsStore settings(paths_);settings.recover(request_);settings.seed(request_,native_settings(request_));request_["settings"]=settings.resolve(settings.load(),request_,options_.settings);
        for(const auto& spec:game_settings(request_["game_id"].get<std::string>())){auto& f=request_["required_features"];auto available=runtime_features(request_);if(std::find(available.begin(),available.end(),spec.feature)==available.end()){if(request_["settings"].contains(spec.group)){request_["settings"][spec.group].erase(spec.key);if(request_["settings"][spec.group].empty())request_["settings"].erase(spec.group);}continue;}if(std::find(f.begin(),f.end(),spec.feature)==f.end())f.push_back(spec.feature);}
        validate_engine_request(request_,directory_/"request.json");settings.begin(request_);
        write_document(directory_/"request.json",request_);channel(directory_/"commands.jsonl");channel(directory_/"events.jsonl");
        process_=std::make_unique<Process>();std::map<std::string,std::string> env={{"TITUS_TEST_RUNTIME_MS",std::to_string(options_.test_ms)},{"TITUS_TEST_GAMEPLAY",options_.test_gameplay?"1":"0"},{"TITUS_TEST_FAULT",options_.test_fault}};
        process_->start(executable_,{"--session-request",(directory_/"request.json").u8string()},executable_.parent_path(),env,directory_,*lock_);
        state=State::Starting;started_=last_event_=std::chrono::steady_clock::now();message_="Abriendo el juego…";return;
    }
    if(path_exists(directory_/"events.jsonl"))consume_events();if(process_->poll()){consume_events();collect();}
    }catch(const std::exception& e){if(failure_.empty())failure_=e.what();
        // A protocol problem never silently kills a live game or releases its lock.
        if(process_&&process_->started()&&!process_->poll()){message_="Error de sesión. El juego sigue abierto; ciérralo desde su menú.";return;}
        state=State::Failed;message_=failure_;
    }
}
}
