#include "titus/catalog.hpp"
#include <fstream>
#include <iostream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif
using namespace titus;
namespace {
int checks=0;
void check(bool ok,const std::string& message) { ++checks;if(!ok)throw std::runtime_error(message); }
void fails(const std::function<void()>& f,const std::string& code) {
    ++checks;try { f(); }catch(const Error& e) { if(e.code==code)return;throw; }
    throw std::runtime_error("Expected "+code);
}
void write(const fs::path& p,const std::string& data) { fs::create_directories(p.parent_path());std::ofstream out(p,std::ios::binary);out<<data; }
Json resource(const std::string& name,const std::string& data) {return {{"name",name},{"size",data.size()},{"sha256",sha256(data)}};}
Json profile(const std::string& game,const std::string& edition="dos") {
    bool supported=edition=="dos"||game=="blues";
    return {{"game_id",game},{"edition_id",edition},{"variant_id","test-full"},{"save_compatibility_id",game+".test"},
      {"engine_id",supported?Json("blues-family"):Json(nullptr)},{"status",supported?"recognized":"missing_engine"},
      {"anchors",Json::array({game+".dat"})},{"files",Json::array({resource(game+".dat",game),resource(game+".gfx","pixels")})},
      {"adf_sha256",Json::array({sha256(game+"image")})}};
}
struct Temporary {
    fs::path root=fs::temp_directory_path()/fs::u8path("titus-catalog-pruebas-á "+uuid());
    ~Temporary(){std::error_code e;fs::remove_all(root,e);}
};
}
int main(int argc,char** argv) {
    try {
        check(sha256("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","empty SHA");
        check(sha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","abc SHA");
        check(sha256(std::string(1000000,'a'))=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0","stream SHA");
        if(argc==2) {
            std::ifstream in(fs::u8path(argv[1]));Json v;in>>v;
            check(fingerprint(v["manifest"])==v["sha256"],"TH-03 canonical fingerprint vector");
        }
        fails([]{parse_json("{\"a\":1,\"a\":2}");},"E_JSON");
        fails([]{parse_json("{\"a\":NaN}");},"E_JSON");
        fails([]{parse_json(std::string(1024*1024+1,' '));},"E_JSON");
        fails([]{parse_json(std::string(70,'[')+std::string(70,']'));},"E_JSON");
        Temporary temp;auto source=temp.root/"juegos originales";
        auto storage=Paths::resolve(temp.root/"usuario");
        auto p=profile("blues"),q=profile("jukebox");Detector detector(Json::array({p,q}));CatalogStore store(storage,detector);
        fails([&]{store.load();},"E_CATALOG_NOT_FOUND");
        write(source/"BLUES.DAT","blues");write(source/"blues.gfx","pixels");write(source/"readme.txt","irrelevant");
        check(file_sha256(source/"BLUES.DAT")==sha256("blues"),"file SHA");
        auto detected=detector.scan(source);check(detected.size()==1&&detected[0].installation["status"]=="recognized","case-insensitive full set");
        check(!fs::exists(storage.config),"scan read-only");
        auto first=store.add(source);auto id=first["installations"][0]["id"].get<std::string>();auto fp=first["installations"][0]["fingerprint"];
        check(first["revision"]==1,"first transaction revision");validate_catalog(first);
        check(store.add(source)==first,"idempotent add");
#ifdef _WIN32
        auto lock=CreateFileW((storage.config/"catalog.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        check(lock!=INVALID_HANDLE_VALUE,"test holds Windows lock");
        fails([&]{store.init();},"E_LOCKED");CloseHandle(lock);
#else
        int lock=::open((storage.config/"catalog.lock").c_str(),O_RDWR);
        check(lock>=0&&flock(lock,LOCK_EX|LOCK_NB)==0,"test holds POSIX lock");
        fails([&]{store.init();},"E_LOCKED");close(lock);
#endif
        auto mixed=source/"mezcla";write(mixed/"blues.dat","blues");write(mixed/"blues.gfx","pixels");write(mixed/"jukebox.dat","jukebox");write(mixed/"jukebox.gfx","pixels");
        check(detector.scan(mixed).size()==2,"mixed folder");
        // Force backup replacement to fail: the current catalog must remain intact.
        auto backup_path=storage.config/"catalog.json.bak";fs::create_directory(backup_path);
        bool write_failed=false;
        try { store.add(mixed); }catch(const std::exception&) { write_failed=true; }
        check(write_failed&&store.load()==first,"failed atomic write preserves current catalog");
        fs::remove(backup_path);
        for(const auto& entry:fs::directory_iterator(storage.config))
            check(entry.path().filename().u8string().find(".tmp.")==std::string::npos,"failed write removes temporary file");
        auto second=store.add(mixed);check(second["installations"].size()==2&&second["installations"][0]["locations"].size()==2,"dedupe copies / mixed registration");
        fails([&]{store.remove(id,1);},"E_REVISION");check(store.load()==second,"stale edit unchanged");
        auto moved=temp.root/fs::u8path("trasladado á");fs::rename(source,moved);
        auto relocated=store.relocate(id,moved);check(relocated["installations"][0]["id"]==id&&relocated["installations"][0]["fingerprint"]==fp,"relocation preserves identity");
        check(relocated["installations"][0]["locations"].size()==1,"missing locations pruned");
        write(moved/"blues.gfx","broken");check(detector.scan(moved)[0].installation["status"]=="unverified","modified resource");
        fails([&]{store.add(moved);},"E_INSTALLATION_CHANGED");
        fails([&]{store.relocate(id,moved);},"E_IDENTITY_MISMATCH");
        auto damaged=store.recheck(id);check(damaged["installations"][0]["status"]=="unverified"&&damaged["installations"][0]["fingerprint"]==fp,"retain intended identity on damage");
        fs::remove(moved/"blues.gfx");check(detector.scan(moved)[0].installation["status"]=="incomplete","missing resource");
        write(moved/"blues.gfx","pixels");check(store.recheck(id)["installations"][0]["status"]=="recognized","repair recheck");
#ifndef _WIN32
        write(moved/"blues.dat","other");check(!detector.scan(moved)[0].diagnostics["ambiguous"].empty(),"case conflict");
        fails([&]{store.add(moved);},"E_AMBIGUOUS");
        write(moved/"blues.dat","blues");check(detector.scan(moved)[0].installation["fingerprint"]==fp,"identical case duplicates dedupe");
#endif
        auto image=temp.root/"blues.adf";write(image,"bluesimage");auto image_result=detector.scan(image);
        check(image_result[0].installation["fingerprint"]==fp&&image_result[0].installation["status"]=="needs_preparation","ADF linkage");
        check(store.add(image)["installations"].size()==2,"ADF deduplication");
        write(temp.root/"unknown.adf","unknown");check(detector.scan(temp.root/"unknown.adf")[0].installation.is_null(),"unknown image not assigned game");
        fails([&]{store.add(temp.root/"unknown.adf");},"E_NO_MATCH");
        auto saved=storage.data/"profiles"/"keep.bbs";write(saved,"saved progress");
        auto removed=store.remove(id);check(fs::exists(moved/"BLUES.DAT")&&fs::exists(saved)&&removed["installations"].size()==1,"remove only references");
        auto invalid=removed;invalid["schema_version"]=2;write(storage.catalog(),invalid.dump());
        fails([&]{store.init();},"E_SCHEMA_VERSION");fails([&]{store.recover();},"E_SCHEMA_VERSION");
        write(storage.catalog(),"{broken");fails([&]{store.add(moved);},"E_JSON");
        auto recovered=store.recover();validate_catalog(recovered);check(recovered["installations"].size()==2,"backup recovery");
        bool preserved=false;for(const auto& e:fs::directory_iterator(storage.config))if(e.path().filename().u8string().find("catalog.corrupt.")==0)preserved=true;
        check(preserved,"corrupt file preserved");fails([&]{store.recover();},"E_RECOVERY");
        auto overlap=Paths::resolve(moved/"private");CatalogStore unsafe(overlap,detector);
        fails([&]{unsafe.add(moved);},"E_SOURCE_OVERLAP");check(!fs::exists(overlap.config),"overlap rejected before writes");
        fails([&]{storage.profile_directory(uuid(),"blues","..");},"E_INVALID");
        auto env=[&](const char* key){return std::string(key)=="TITUS_USER_DIR"?(temp.root/"env").u8string():std::string();};
        check(Paths::resolve({},env).config==temp.root/"env/config","env root");
        check(Paths::resolve(temp.root/"explicit",env).config==temp.root/"explicit/config","CLI overrides env");
        // Register an incomplete set, then complete and explicitly upgrade it.
        auto partial=temp.root/"partial";write(partial/"blues.dat","blues");
        auto c=store.add(partial);std::string partial_id;
        for(auto i:c["installations"])if(i["variant_id"]=="unverified")partial_id=i["id"];
        check(!partial_id.empty(),"incomplete can be catalogued");write(partial/"blues.gfx","pixels");
        fails([&]{store.recheck(partial_id);},"E_DUPLICATE_IDENTITY");
        store.remove(id);
        auto upgraded=store.recheck(partial_id);bool found=false;
        for(const auto& i:upgraded["installations"])if(i["id"]==partial_id)found=i["variant_id"]=="test-full";
        check(found,"explicit incomplete upgrade keeps UUID");
        // Existing Amiga entries must gain the new engine on verification,
        // without losing UUIDs, locations or existing profile files.
        auto fox_source=temp.root/"fox amiga";write(fox_source/"fox.dat","fox");write(fox_source/"fox.gfx","pixels");
        auto fox_paths=Paths::resolve(temp.root/"fox-user");auto legacy=profile("fox","amiga");
        CatalogStore old_fox(fox_paths,Detector(Json::array({legacy})));auto old_catalog=old_fox.add(fox_source);
        auto fox_id=old_catalog["installations"][0]["id"].get<std::string>();
        auto modern=legacy;modern["engine_id"]="fox-amiga-native";modern["status"]="recognized";modern["save_compatibility_id"]="fox.amiga.full.audited";
        CatalogStore new_fox(fox_paths,Detector(Json::array({modern})));
        auto keep=fox_paths.data/"profiles/old-profile/sentinel";write(keep,"preserved");
        for(unsigned operation=0;operation<3;++operation){
            write(fox_paths.catalog(),old_catalog.dump());
            auto migrated=operation==0?new_fox.recheck(fox_id):operation==1?new_fox.add(fox_source):new_fox.relocate(fox_id,fox_source);
            const auto& installation=migrated["installations"][0];validate_catalog(migrated);
            check(migrated["installations"].size()==1 && installation["id"]==fox_id && installation["locations"]==old_catalog["installations"][0]["locations"],"Fox migration preserves identity and locations");
            check(installation["engine_id"]=="fox-amiga-native" && installation["status"]=="recognized" && installation["save_compatibility_id"]=="fox.amiga.full.audited","Fox migration refreshes engine and compatibility");
            check(fs::exists(keep),"Fox migration preserves profile files");
        }
        std::cout<<"OK: "<<checks<<" catalog checks\n";return 0;
    }catch(const std::exception& e) {std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}
}
