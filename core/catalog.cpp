#include "titus/catalog.hpp"
#include "profiles.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
#include <random>
#include <regex>
#include <set>
#include <sstream>
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

namespace titus {
Error::Error(std::string c,const std::string& m):std::runtime_error(m),code(std::move(c)){}
namespace {
void require(bool ok,const std::string& message,const std::string& code="E_INVALID") {
    if(!ok) throw Error(code,message);
}
std::string lower(std::string s) { for(auto& c:s) if(c>='A'&&c<='Z') c+=32;return s; }
std::string read(const fs::path& p) {
    std::ifstream in(filesystem_path(p),std::ios::binary);require(bool(in),"Cannot read "+p.u8string(),"E_READ");
    std::string s;char b[8192];
    while(in) { in.read(b,sizeof b);s.append(b,static_cast<size_t>(in.gcount()));
        require(s.size()<=1024*1024,"JSON exceeds 1 MiB","E_JSON"); }
    require(in.eof(),"Cannot finish reading "+p.u8string(),"E_READ");return s;
}
void keys(const Json& j,std::initializer_list<const char*> names) {
    require(j.is_object()&&j.size()==names.size(),"Unexpected object fields");
    for(auto n:names) require(j.contains(n),std::string("Missing field ")+n);
}
void string_pattern(const Json& s,const char* pattern) {
    require(s.is_string()&&std::regex_match(s.get<std::string>(),std::regex(pattern)),"Invalid identifier/value");
}
void identifier(const Json& s) { string_pattern(s,"[a-z0-9][a-z0-9.-]{0,79}"); }
void uuid_value(const Json& s) { string_pattern(s,"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"); }
bool native_absolute(const fs::path& path) {
#ifdef _WIN32
    const auto& s=path.native();
    auto separator=[](wchar_t c){return c==L'\\'||c==L'/';};
    bool drive=s.size()>=3&&((s[0]>=L'A'&&s[0]<=L'Z')||(s[0]>=L'a'&&s[0]<=L'z'))&&s[1]==L':'&&separator(s[2]);
    bool unc=s.size()>4&&separator(s[0])&&separator(s[1])&&!separator(s[2]);
    if(unc) { auto end=s.find_first_of(L"\\/",2);unc=end!=std::wstring::npos&&end+1<s.size()&&!separator(s[end+1]); }
    return drive||unc;
#else
    return path.is_absolute();
#endif
}
bool within(const fs::path& parent,const fs::path& child) {
    auto p=normalized_path(parent),c=normalized_path(child);
    auto pi=p.begin(),ci=c.begin();
    for(;pi!=p.end();++pi,++ci) {
        if(ci==c.end()) return false;
#ifdef _WIN32
        if(_wcsicmp(pi->c_str(),ci->c_str())!=0) return false;
#else
        if(*pi!=*ci) return false;
#endif
    }
    return true;
}
Json location(const fs::path& p,const char* kind) {
    return {{"kind",kind},{"location",{{"platform",platform()},{"path",normalized_path(p).u8string()}}}};
}
Json manifest(const Json& p,const Json& files,const std::string& variant) {
    return {{"version",1},{"game_id",p.at("game_id")},{"edition_id",p.at("edition_id")},
            {"variant_id",variant},{"files",files}};
}
Json installation(const Json& p,const Json& source,const std::string& status,const Json& files,bool exact) {
    auto variant=exact?p.at("variant_id").get<std::string>():"unverified";
    return {{"id",nullptr},{"game_id",p.at("game_id")},{"edition_id",p.at("edition_id")},
      {"engine_id",p.at("engine_id")},{"variant_id",variant},
      {"fingerprint",fingerprint(manifest(p,files,variant))},
      {"save_compatibility_id",exact?p.at("save_compatibility_id"):Json("unverified."+p.at("game_id").get<std::string>()+"."+p.at("edition_id").get<std::string>())},
      {"status",status},{"locations",Json::array({source})},{"preparation_id",nullptr}};
}
class Lock {
#ifdef _WIN32
    HANDLE handle=INVALID_HANDLE_VALUE;
#else
    int handle=-1;
#endif
public:
    explicit Lock(const fs::path& file) {
#ifdef _WIN32
        handle=CreateFileW(filesystem_path(file).c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        require(handle!=INVALID_HANDLE_VALUE,"Catalog is locked or inaccessible","E_LOCKED");
#else
        handle=::open(file.c_str(),O_CREAT|O_RDWR|O_CLOEXEC,0600);
        require(handle>=0,"Cannot open catalog lock","E_LOCKED");
        if(flock(handle,LOCK_EX|LOCK_NB)!=0) { close(handle);handle=-1;throw Error("E_LOCKED","Catalog is in use"); }
#endif
    }
    ~Lock() {
#ifdef _WIN32
        if(handle!=INVALID_HANDLE_VALUE) CloseHandle(handle);
#else
        if(handle>=0) close(handle);
#endif
    }
    Lock(const Lock&)=delete;
    Lock& operator=(const Lock&)=delete;
};
void atomic_write(const fs::path& target,const std::string& bytes) {
    auto tmp=target;tmp+=fs::u8path(".tmp."+uuid());
    try {
#ifdef _WIN32
        HANDLE h=CreateFileW(filesystem_path(tmp).c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        require(h!=INVALID_HANDLE_VALUE,"Cannot create catalog temporary file","E_WRITE");
        DWORD count=0;
        bool ok=WriteFile(h,bytes.data(),static_cast<DWORD>(bytes.size()),&count,nullptr)&&count==bytes.size();
        ok=FlushFileBuffers(h)&&ok;CloseHandle(h);
        require(ok,"Cannot flush catalog","E_WRITE");
        require(MoveFileExW(filesystem_path(tmp).c_str(),filesystem_path(target).c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH),"Cannot replace catalog","E_WRITE");
#else
        int fd=::open(tmp.c_str(),O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,0600);
        require(fd>=0,"Cannot create catalog temporary file","E_WRITE");
        size_t offset=0;bool ok=true;
        while(offset<bytes.size()) { auto n=::write(fd,bytes.data()+offset,bytes.size()-offset);
            if(n<=0) { ok=false;break; } offset+=static_cast<size_t>(n); }
        ok=(fsync(fd)==0)&&ok;close(fd);require(ok,"Cannot flush catalog","E_WRITE");
        fs::rename(tmp,target);
        int dir=::open(target.parent_path().c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);
        if(dir>=0) { fsync(dir);close(dir); }
#endif
    } catch(...) { std::error_code ec;fs::remove(tmp,ec);throw; }
}
Json fresh() { return {{"schema_version",1},{"kind","catalog"},{"catalog_id",uuid()},{"revision",0},
    {"profiles",Json::array({{{"id",uuid()},{"name","Jugador"}}})},{"installations",Json::array()}}; }
Json& find_install(Json& c,const std::string& id) {
    for(auto& i:c["installations"]) if(i["id"]==id) return i;
    throw Error("E_NOT_FOUND","Installation not found: "+id);
}
bool same_identity(const Json& a,const Json& b) {
    for(auto k:{"game_id","edition_id","variant_id","fingerprint"}) if(a.at(k)!=b.at(k)) return false;
    return true;
}
void refresh_detection(Json& installation,const Json& detected) {
    // Resource identity and locations stay stable when an adapter becomes available.
    for(const auto* key:{"engine_id","status","save_compatibility_id","preparation_id"})
        installation[key]=detected.at(key);
}

}
std::string platform() {
#ifdef _WIN32
    return "windows";
#else
    return "linux";
#endif
}
fs::path filesystem_path(const fs::path& path) {
#ifdef _WIN32
    auto s=path.native();for(auto& c:s)if(c==L'/')c=L'\\';
    if(s.rfind(L"\\\\?\\",0)==0)return fs::path(s);
    if(s.size()>2&&s[0]==L'\\'&&s[1]==L'\\')return fs::path(L"\\\\?\\UNC\\"+s.substr(2));
    if(s.size()>2&&s[1]==L':'&&s[2]==L'\\')return fs::path(L"\\\\?\\"+s);
#endif
    return path;
}
#ifdef _WIN32
static DWORD path_attributes(const fs::path& path) {
    DWORD value=GetFileAttributesW(filesystem_path(path).c_str());
    if(value==INVALID_FILE_ATTRIBUTES){auto error=GetLastError();
        if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND&&error!=ERROR_INVALID_NAME)
            throw Error("E_READ","Cannot inspect path: "+path.u8string());
    }
    return value;
}
#endif
bool path_exists(const fs::path& path){
#ifdef _WIN32
    return path_attributes(path)!=INVALID_FILE_ATTRIBUTES;
#else
    return fs::exists(path);
#endif
}
bool path_is_directory(const fs::path& path){
#ifdef _WIN32
    auto a=path_attributes(path);return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY);
#else
    return fs::is_directory(path);
#endif
}
bool path_is_regular_file(const fs::path& path){
#ifdef _WIN32
    auto a=path_attributes(path);return a!=INVALID_FILE_ATTRIBUTES&&!(a&FILE_ATTRIBUTE_DIRECTORY);
#else
    return fs::is_regular_file(path);
#endif
}
bool path_is_symlink(const fs::path& path){
#ifdef _WIN32
    auto a=path_attributes(path);return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_REPARSE_POINT);
#else
    return fs::is_symlink(path);
#endif
}
bool path_is_empty(const fs::path& path){
#ifdef _WIN32
    require(path_is_directory(path),"Not a directory","E_READ");WIN32_FIND_DATAW data;
    HANDLE find=FindFirstFileW(filesystem_path(path/"*").c_str(),&data);
    if(find==INVALID_HANDLE_VALUE){require(GetLastError()==ERROR_FILE_NOT_FOUND,"Cannot list directory","E_READ");return true;}
    bool empty=true;do{if(wcscmp(data.cFileName,L".")&&wcscmp(data.cFileName,L"..")){empty=false;break;}}while(FindNextFileW(find,&data));FindClose(find);return empty;
#else
    return fs::is_empty(path);
#endif
}
uint64_t path_file_size(const fs::path& path){
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data;require(GetFileAttributesExW(filesystem_path(path).c_str(),GetFileExInfoStandard,&data),"Cannot read file size","E_READ");return (uint64_t(data.nFileSizeHigh)<<32)|data.nFileSizeLow;
#else
    return fs::file_size(path);
#endif
}
void copy_file_exact(const fs::path& from,const fs::path& to){
#ifdef _WIN32
    require(CopyFileW(filesystem_path(from).c_str(),filesystem_path(to).c_str(),TRUE),"Cannot copy legacy file: "+from.u8string(),"E_IMPORT");
#else
    fs::copy_file(from,to);
#endif
}
void move_directory(const fs::path& from,const fs::path& to){
#ifdef _WIN32
    require(MoveFileExW(filesystem_path(from).c_str(),filesystem_path(to).c_str(),MOVEFILE_WRITE_THROUGH),"Cannot publish imported profile","E_IMPORT");
#else
    fs::rename(from,to);
#endif
}
void remove_regular_file(const fs::path& path){
#ifdef _WIN32
    if(DeleteFileW(filesystem_path(path).c_str()))return;auto error=GetLastError();require(error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND,"Cannot remove private file: "+path.u8string(),"E_WRITE");
#else
    if(unlink(path.c_str())==0)return;require(errno==ENOENT,"Cannot remove private file: "+path.u8string(),"E_WRITE");
#endif
}
void remove_empty_directory(const fs::path& path){
#ifdef _WIN32
    require(RemoveDirectoryW(filesystem_path(path).c_str()),"Cannot replace empty profile directory","E_IMPORT");
#else
    fs::remove(path);
#endif
}
void ensure_directory(const fs::path& path) {
#ifdef _WIN32
    auto normal=normalized_path(path),native=filesystem_path(normal);
    auto attrs=GetFileAttributesW(native.c_str());
    if(attrs!=INVALID_FILE_ATTRIBUTES){require((attrs&FILE_ATTRIBUTE_DIRECTORY)!=0,"Not a directory: "+normal.u8string(),"E_WRITE");return;}
    auto parent=normal.parent_path();require(parent!=normal&&!parent.empty(),"Cannot create path root","E_WRITE");ensure_directory(parent);
    if(!CreateDirectoryW(native.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)throw Error("E_WRITE","Cannot create directory: "+normal.u8string());
#else
    fs::create_directories(path);
#endif
}
fs::path normalized_path(const fs::path& path) {
    require(!path.empty(),"Empty path");
    require(path.u8string().find('\0')==std::string::npos,"NUL in path");
#ifdef _WIN32
    // MinGW's filesystem does not classify UNC roots as absolute. Use Win32
    // for normalization and junction resolution, preserving the UNC prefix.
    DWORD size=GetFullPathNameW(path.c_str(),0,nullptr,nullptr);
    require(size>0,"Cannot resolve Windows path","E_PATH");
    std::wstring full(size,L'\0');
    auto written=GetFullPathNameW(path.c_str(),size,full.data(),nullptr);
    require(written>0&&written<size,"Cannot resolve Windows path","E_PATH");full.resize(written);
    fs::path probe(full);std::vector<fs::path> tail;
    for(;;) {
        auto handle=CreateFileW(filesystem_path(probe).c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,
                                OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
        if(handle!=INVALID_HANDLE_VALUE) {
            size=GetFinalPathNameByHandleW(handle,nullptr,0,FILE_NAME_NORMALIZED);
            std::wstring final(size,L'\0');
            written=size?GetFinalPathNameByHandleW(handle,final.data(),size,FILE_NAME_NORMALIZED):0;
            CloseHandle(handle);
            require(written>0&&written<size,"Cannot resolve Windows source","E_PATH");final.resize(written);
            if(final.rfind(L"\\\\?\\UNC\\",0)==0)final=L"\\\\"+final.substr(8);
            else if(final.rfind(L"\\\\?\\",0)==0)final=final.substr(4);
            fs::path result(final);for(auto it=tail.rbegin();it!=tail.rend();++it)result/=*it;
            return result;
        }
        auto error=GetLastError();
        require(error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND,
                "Cannot access Windows source","E_PATH");
        auto parent=probe.parent_path();
        require(!parent.empty()&&parent!=probe,"Cannot resolve Windows path root","E_PATH");
        tail.push_back(probe.filename());probe=parent;
    }
#else
    return fs::weakly_canonical(fs::absolute(path)).lexically_normal();
#endif
}
std::string uuid() {
    std::random_device random;unsigned char bytes[16];for(auto& b:bytes)b=static_cast<unsigned char>(random());
    bytes[6]=(bytes[6]&15)|64;bytes[8]=(bytes[8]&63)|128;
    std::ostringstream s;s<<std::hex<<std::setfill('0');
    for(int i=0;i<16;++i) { if(i==4||i==6||i==8||i==10)s<<'-';s<<std::setw(2)<<unsigned(bytes[i]); }
    return s.str();
}
Json parse_json(const std::string& text) {
    require(text.size()<=1024*1024,"JSON exceeds 1 MiB","E_JSON");
    std::vector<std::set<std::string>> scopes;
    auto callback=[&](int depth,Json::parse_event_t event,Json& parsed) {
        require(depth<=64,"JSON nesting exceeds 64","E_JSON");
        if(event==Json::parse_event_t::object_start) scopes.emplace_back();
        if(event==Json::parse_event_t::key) require(scopes.back().insert(parsed.get<std::string>()).second,"Duplicate JSON key","E_JSON");
        if(event==Json::parse_event_t::object_end) scopes.pop_back();
        return true;
    };
    try { auto j=Json::parse(text,callback); j.dump(-1,' ',false,Json::error_handler_t::strict);return j; }
    catch(const Json::exception& e) { throw Error("E_JSON",e.what()); }
}
std::optional<std::string> environment_value(const char* name) {
#ifdef _WIN32
        std::wstring w;while(*name)w+=static_cast<wchar_t>(*name++);
        auto n=GetEnvironmentVariableW(w.c_str(),nullptr,0);if(!n)return GetLastError()==ERROR_ENVVAR_NOT_FOUND?std::nullopt:std::optional<std::string>("");
        std::wstring value(n,L'\0');GetEnvironmentVariableW(w.c_str(),value.data(),n);value.resize(n-1);
        return fs::path(value).u8string();
#else
        auto p=std::getenv(name);return p?std::optional<std::string>(p):std::nullopt;
#endif
}
Paths Paths::resolve(const fs::path& override_root,std::function<std::string(const char*)> env) {
    if(!env) env=[](const char* name){return environment_value(name).value_or("");};
    auto root=override_root.empty()?fs::u8path(env("TITUS_USER_DIR")):override_root;
    if(!root.empty()) {
        require(native_absolute(root),"User directory must be absolute");root=normalized_path(root);
        return {root/"config",root/"data",root/"cache"};
    }
#ifdef _WIN32
    auto roaming=env("APPDATA"),local=env("LOCALAPPDATA");
    require(native_absolute(fs::u8path(roaming))&&native_absolute(fs::u8path(local)),"APPDATA/LOCALAPPDATA must be absolute");
    return {normalized_path(fs::u8path(roaming)/"pedlop01/titus-engine/config"),normalized_path(fs::u8path(roaming)/"pedlop01/titus-engine/data"),normalized_path(fs::u8path(local)/"pedlop01/titus-engine/cache")};
#else
    auto home=env("HOME");
    auto base=[&](const char* var,const char* fallback) {
        auto value=fs::u8path(env(var));if(value.is_absolute()) return value;
        require(!home.empty()&&fs::u8path(home).is_absolute(),"HOME must be absolute when XDG path is unavailable");
        return fs::u8path(home)/fallback;
    };
    return {normalized_path(base("XDG_CONFIG_HOME",".config")/"titus-engine"),normalized_path(base("XDG_DATA_HOME",".local/share")/"titus-engine"),normalized_path(base("XDG_CACHE_HOME",".cache")/"titus-engine")};
#endif
}
fs::path Paths::profile_directory(const std::string& profile,const std::string& game,const std::string& compatibility) const {
    uuid_value(profile);identifier(game);identifier(compatibility);
    require(compatibility!="."&&compatibility!="..","Invalid compatibility group");
    return data/"profiles"/fs::u8path(profile)/"games"/fs::u8path(game)/fs::u8path(compatibility);
}
void Paths::protect_source(const fs::path& source) const {
    for(const auto& owned:{config,data,cache}) require(!within(source,owned)&&!within(owned,source),"Storage overlaps original data","E_SOURCE_OVERLAP");
}
Detector::Detector():profiles_(parse_json(embedded_profiles)){}
Detector::Detector(Json profiles):profiles_(std::move(profiles)){}
std::vector<Detection> Detector::scan(const fs::path& input,bool recursive) const {
    auto source=normalized_path(input);require(fs::exists(source),"Source does not exist: "+source.u8string(),"E_SOURCE_MISSING");
    std::vector<Detection> result;
    auto inspect=[&](const fs::path& path) {
        if(fs::is_regular_file(path)) {
            if(lower(path.extension().u8string())!=".adf") return;
            auto hash=file_sha256(path);
            for(const auto& p:profiles_) for(const auto& h:p.at("adf_sha256")) if(h==hash) {
                auto status=p.at("engine_id").is_null()?"missing_engine":"needs_preparation";
                result.push_back({installation(p,location(path,"adf"),status,p.at("files"),true),{{"source","known_adf"},{"direct_launch",false}}});return;
            }
            result.push_back({nullptr,{{"source",path.u8string()},{"status","unverified"},{"reason","unknown_adf"}}});return;
        }
        if(!fs::is_directory(path)) return;
        std::map<std::string,std::vector<fs::path>> names;
        for(const auto& entry:fs::directory_iterator(path))
            if(entry.is_regular_file()&&!entry.is_symlink()) names[lower(entry.path().filename().u8string())].push_back(entry.path());
        std::map<std::string,Json> hashes;
        auto actual=[&](const std::string& name)->Json {
            if(hashes.count(name))return hashes.at(name);
            Json unique=Json::array();
            for(const auto& file:names.at(name)) {
                auto size=fs::file_size(file);require(size<=9007199254740991ULL,"Resource too large");
                Json value={{"name",name},{"size",size},{"sha256",file_sha256(file)}};
                if(std::find(unique.begin(),unique.end(),value)==unique.end())unique.push_back(value);
            }
            hashes[name]=unique;return unique;
        };
        for(const auto& p:profiles_) {
            bool candidate=false;for(const auto& anchor:p.at("anchors")) if(names.count(anchor.get<std::string>()))candidate=true;
            if(!candidate) continue;
            Json missing=Json::array(),changed=Json::array(),ambiguous=Json::array(),files=Json::array();
            for(const auto& expected:p.at("files")) {
                auto name=expected.at("name").get<std::string>();
                if(!names.count(name)) { missing.push_back(name);continue; }
                auto values=actual(name);
                if(values.size()>1)ambiguous.push_back(name);
                if(values.size()!=1||values[0]!=expected)changed.push_back(name);
                if(values.size()==1)files.push_back(values[0]);
            }
            bool exact=missing.empty()&&changed.empty();
            std::string status=!changed.empty()?"unverified":!missing.empty()?"incomplete":p.at("status").get<std::string>();
            Json diagnostics={{"missing",missing},{"changed",changed},{"ambiguous",ambiguous}};
            if(!ambiguous.empty()) {
                diagnostics["reason"]="ambiguous_resources";diagnostics["status"]="unverified";
                diagnostics["game_id"]=p.at("game_id");diagnostics["edition_id"]=p.at("edition_id");
                diagnostics["source"]=path.u8string();
                result.push_back({nullptr,diagnostics});
            } else result.push_back({installation(p,location(path,"directory"),status,files,exact),diagnostics});
        }
    };
    inspect(source);
    if(recursive&&fs::is_directory(source)) {
        size_t count=0;
        for(fs::recursive_directory_iterator it(source),end;it!=end;++it) {
            require(++count<=50000,"Recursive scan limit reached","E_SCAN_LIMIT");
            if(it->is_symlink()) { it.disable_recursion_pending();continue; }
            if(it->is_directory()) {
                auto name=it->path().filename().u8string();
                if(name==".git"||it.depth()>=32) { it.disable_recursion_pending();continue; }
                inspect(it->path());
            } else if(lower(it->path().extension().u8string())==".adf")inspect(it->path());
        }
    }
    return result;
}
void validate_catalog(const Json& c) {
    if(c.is_object()&&c.contains("schema_version")&&c["schema_version"].is_number_integer())
        require(c["schema_version"]==1,"Unsupported catalog schema; preserved unchanged","E_SCHEMA_VERSION");
    keys(c,{"schema_version","kind","catalog_id","revision","profiles","installations"});
    require(c["schema_version"].is_number_integer()&&c["schema_version"]==1,"Unsupported catalog schema; preserved unchanged","E_SCHEMA_VERSION");
    require(c["kind"]=="catalog","Wrong document kind");uuid_value(c["catalog_id"]);
    require(c["revision"].is_number_integer()&&c["revision"]>=0&&c["revision"]<=9007199254740991LL,"Invalid revision");
    require(c["profiles"].is_array()&&!c["profiles"].empty()&&c["profiles"].size()<=128,"Invalid profiles");
    std::set<std::string> ids;
    for(const auto& p:c["profiles"]) {
        keys(p,{"id","name"});uuid_value(p["id"]);require(ids.insert(p["id"]).second,"Duplicate profile id");
        require(p["name"].is_string()&&!p["name"].get<std::string>().empty()&&p["name"].get<std::string>().size()<=1024,"Invalid profile name");
    }
    require(c["installations"].is_array()&&c["installations"].size()<=1024,"Invalid installations");ids.clear();
    const std::map<std::string,std::string> engines={{"blues","blues-family"},{"jukebox","blues-family"},{"prehistorik2","blues-family"},{"fox","opentitus"},{"prehistorik","olduvai"}};
    for(const auto& i:c["installations"]) {
        keys(i,{"id","game_id","edition_id","engine_id","variant_id","fingerprint","save_compatibility_id","status","locations","preparation_id"});
        uuid_value(i["id"]);require(ids.insert(i["id"]).second,"Duplicate installation id");
        require(i["game_id"].is_string()&&engines.count(i["game_id"].get<std::string>()),"Unknown game");
        require(i["edition_id"]=="dos"||i["edition_id"]=="amiga","Unknown edition");
        auto game=i["game_id"].get<std::string>();bool supported=i["edition_id"]=="dos"||game=="blues"||(game=="fox" && i["edition_id"]=="amiga");
        bool legacy_fox=game=="fox" && i["edition_id"]=="amiga" && i["engine_id"].is_null() && (i["status"]=="missing_engine" || i["status"]=="unverified" || i["status"]=="incomplete");
        require(legacy_fox || i["engine_id"]==(supported?Json(game=="fox" && i["edition_id"]=="amiga"?"fox-amiga-native":engines.at(game)):Json(nullptr)),"Wrong engine");
        identifier(i["variant_id"]);identifier(i["save_compatibility_id"]);
        string_pattern(i["fingerprint"],"[0-9a-f]{64}");
        require(i["status"]=="recognized"||i["status"]=="needs_preparation"||i["status"]=="incomplete"||i["status"]=="unverified"||i["status"]=="missing_engine","Unknown status");
        require(supported||i["status"]=="missing_engine"||i["status"]=="incomplete"||i["status"]=="unverified","Unsupported edition cannot be playable");
        require(i["preparation_id"].is_null()||i["preparation_id"].is_string(),"Invalid preparation id");
        if(!i["preparation_id"].is_null())identifier(i["preparation_id"]);
        require(i["locations"].is_array()&&!i["locations"].empty()&&i["locations"].size()<=64,"Invalid locations");
        std::set<std::string> locs;
        for(const auto& l:i["locations"]) {
            keys(l,{"kind","location"});require(l["kind"]=="directory"||l["kind"]=="adf","Invalid source kind");
            keys(l["location"],{"platform","path"});
            auto plat=l["location"]["platform"];require(plat=="linux"||plat=="windows","Invalid platform");
            auto p=l["location"]["path"];require(p.is_string(),"Invalid path");auto s=p.get<std::string>();
            require(!s.empty()&&s.size()<=32768&&s.find('\0')==std::string::npos,"Invalid path");
            bool abs=plat=="linux"?s[0]=='/':(s.size()>2&&((s[1]==':'&&(s[2]=='\\'||s[2]=='/'))||(s[0]=='\\'&&s[1]=='\\')));
            require(abs&&!std::regex_search(s,std::regex("(^|[/\\\\])\\.\\.([/\\\\]|$)")),"Path must be absolute without traversal");
            require(locs.insert(l.dump()).second,"Duplicate location");
        }
    }
}
CatalogStore::CatalogStore(Paths paths,Detector detector):paths_(std::move(paths)),detector_(std::move(detector)){}
Json CatalogStore::load() const {
    require(fs::exists(paths_.catalog()),"Catalog not initialized","E_CATALOG_NOT_FOUND");
    auto c=parse_json(read(paths_.catalog()));validate_catalog(c);return c;
}
namespace {
void validate_library_preferences(const Json& p) {
    keys(p,{"schema_version","kind","selected_game","selected_installations","selected_locations","fullscreen","window_width","window_height","browse_path"});
    require(p["schema_version"]==1&&p["kind"]=="library_preferences","Unsupported library preferences; preserved unchanged","E_SCHEMA_VERSION");
    const std::set<std::string> games={"blues","jukebox","fox","prehistorik","prehistorik2"};
    require(p["selected_game"].is_string()&&(p["selected_game"]==""||games.count(p["selected_game"].get<std::string>())),"Invalid selected game");
    require(p["selected_installations"].is_object()&&p["selected_installations"].size()<=5,"Invalid selection map");
    for(auto it=p["selected_installations"].begin();it!=p["selected_installations"].end();++it) {
        require(games.count(it.key()),"Unknown selected game");uuid_value(it.value());
    }
    require(p["selected_locations"].is_object()&&p["selected_locations"].size()<=1024,"Invalid source map");
    for(auto it=p["selected_locations"].begin();it!=p["selected_locations"].end();++it) {
        uuid_value(it.key());string_pattern(it.value(),"[0-9a-f]{64}");
    }
    require(p["fullscreen"].is_boolean(),"Invalid fullscreen setting");
    require(p["window_width"].is_number_integer()&&p["window_width"]>=920&&p["window_width"]<=7680,"Invalid window width");
    require(p["window_height"].is_number_integer()&&p["window_height"]>=640&&p["window_height"]<=4320,"Invalid window height");
    require(p["browse_path"].is_string()&&p["browse_path"].get<std::string>().size()<=32768&&p["browse_path"].get<std::string>().find('\0')==std::string::npos,"Invalid browser path");
}
}
Json CatalogStore::load_library_preferences() const {
    auto file=paths_.config/"library.json";
    if(!fs::exists(file))return nullptr;
    auto p=parse_json(read(file));validate_library_preferences(p);return p;
}
void CatalogStore::save_library_preferences(const Json& preferences) {
    validate_library_preferences(preferences);
    auto catalog=load();
    for(const auto& i:catalog["installations"])for(const auto& l:i["locations"])
        if(l["location"]["platform"]==platform())paths_.protect_source(fs::u8path(l["location"]["path"].get<std::string>()));
    Lock lock(paths_.config/"catalog.lock");
    auto previous=load_library_preferences(); // Never overwrite corrupt or future preferences.
    if(previous==preferences)return;
    if(!previous.is_null())atomic_write(paths_.config/"library.json.bak",previous.dump(2)+"\n");
    atomic_write(paths_.config/"library.json",preferences.dump(2)+"\n");
}
Json CatalogStore::mutate(const std::function<bool(Json&)>& change,long long expected) {
    // Check known sources before creating lock/temporary files.
    if(fs::exists(paths_.catalog())) {
        auto old=load();for(const auto& i:old["installations"])for(const auto& l:i["locations"])
            if(l["location"]["platform"]==platform()) paths_.protect_source(fs::u8path(l["location"]["path"].get<std::string>()));
    }
    ensure_directory(paths_.config);Lock lock(paths_.config/"catalog.lock");
    bool exists=fs::exists(paths_.catalog());auto c=exists?load():fresh();auto before=c;
    require(expected<0||c["revision"]==expected,"Catalog revision changed; reload before editing","E_REVISION");
    if(!change(c)&&exists)return c;
    c["revision"]=c["revision"].get<long long>()+1;validate_catalog(c);
    auto bytes=c.dump(2)+"\n";require(bytes.size()<=1024*1024,"Catalog exceeds 1 MiB","E_WRITE");
    if(exists)atomic_write(paths_.config/"catalog.json.bak",before.dump(2)+"\n");
    atomic_write(paths_.catalog(),bytes);return c;
}
Json CatalogStore::init(long long expected) { return mutate([](Json&){return false;},expected); }
Json CatalogStore::add(const fs::path& source,bool recursive,long long expected) {
    paths_.protect_source(source);auto detections=detector_.scan(source,recursive);
    // Ambiguity requires the user to resolve conflicting files before registration.
    for(const auto& d:detections)
        require(d.diagnostics.value("reason",std::string())!="ambiguous_resources","Conflicting case-insensitive resource names","E_AMBIGUOUS");
    // Unknown ADFs stay diagnostic-only; never invent a game identity.
    detections.erase(std::remove_if(detections.begin(),detections.end(),[](const Detection& d){return d.installation.is_null();}),detections.end());
    require(!detections.empty(),"No known game resources found","E_NO_MATCH");
    for(const auto& d:detections)for(const auto& l:d.installation["locations"])
        paths_.protect_source(fs::u8path(l["location"]["path"].get<std::string>()));
    return mutate([&](Json& c){
        bool changed=false;
        for(auto d:detections) {
            Json* match=nullptr;
            for(auto& i:c["installations"]) {
                if(same_identity(i,d.installation)) { match=&i;break; }
                if(i["game_id"]==d.installation["game_id"]&&i["edition_id"]==d.installation["edition_id"])
                    for(const auto& loc:i["locations"])require(loc!=d.installation["locations"][0],"Registered source changed; use recheck or relocate","E_INSTALLATION_CHANGED");
            }
            if(match) {
                const auto& loc=d.installation["locations"][0];auto& locs=(*match)["locations"];
                if(std::find(locs.begin(),locs.end(),loc)==locs.end()) { locs.push_back(loc);changed=true; }
                // Refresh engine metadata too: an old Amiga entry can now be playable.
                if(d.installation["status"]=="recognized" || d.installation["status"]=="needs_preparation") {auto previous=*match;refresh_detection(*match,d.installation);changed=changed||previous!=*match;}
            } else { d.installation["id"]=uuid();c["installations"].push_back(d.installation);changed=true; }
        }
        return changed;
    },expected);
}
Json CatalogStore::relocate(const std::string& id,const fs::path& source,long long expected) {
    paths_.protect_source(source);auto detections=detector_.scan(source);
    return mutate([&](Json& c){auto& i=find_install(c,id);
        for(const auto& d:detections)if(!d.installation.is_null()&&same_identity(i,d.installation)) {
            auto& locations=i["locations"];
            locations.erase(std::remove_if(locations.begin(),locations.end(),[](const Json& l){return l["location"]["platform"]==platform()&&!fs::exists(fs::u8path(l["location"]["path"].get<std::string>()));}),locations.end());
            auto loc=d.installation["locations"][0];if(std::find(locations.begin(),locations.end(),loc)==locations.end())locations.push_back(loc);
            refresh_detection(i,d.installation);return true;
        }
        throw Error("E_IDENTITY_MISMATCH","Replacement resources do not match this installation");
    },expected);
}
Json CatalogStore::remove(const std::string& id,long long expected) {
    return mutate([&](Json& c){find_install(c,id);auto& a=c["installations"];
        a.erase(std::remove_if(a.begin(),a.end(),[&](const Json& i){return i["id"]==id;}),a.end());return true;},expected);
}
Json CatalogStore::recheck(const std::string& id,long long expected) {
    return mutate([&](Json& c){
        auto& i=find_install(c,id);std::string state="incomplete";Json upgrade,verified;
        bool local=false;
        auto rank=[](const Json& installation) {
            auto status=installation.at("status");
            return status=="recognized"?3:status=="missing_engine"?2:status=="needs_preparation"?1:0;
        };
        for(const auto& l:i["locations"]) {
            if(l["location"]["platform"]!=platform())continue;
            local=true;
            auto p=fs::u8path(l["location"]["path"].get<std::string>());if(!fs::exists(p))continue;
            for(const auto& d:detector_.scan(p)) {
                if(d.installation.is_null()) {
                    if(l["kind"]=="adf"||(d.diagnostics.value("reason",std::string())=="ambiguous_resources"&&
                        d.diagnostics["game_id"]==i["game_id"]&&d.diagnostics["edition_id"]==i["edition_id"]))state="unverified";
                    continue;
                }
                if(same_identity(i,d.installation)) {
                    if(verified.is_null()||rank(d.installation)>rank(verified))verified=d.installation;
                    continue;
                }
                if(i["game_id"]==d.installation["game_id"]&&i["edition_id"]==d.installation["edition_id"]) {
                    if(i["variant_id"]=="unverified"&&d.installation["variant_id"]!="unverified")upgrade=d.installation;
                    if(d.installation["status"]!="incomplete")state="unverified";
                }
            }
        }
        require(local,"Installation has no locations on this platform","E_NO_LOCAL_SOURCE");
        if(!verified.is_null()&&rank(verified)>0) { refresh_detection(i,verified);return true; }
        if(!upgrade.is_null()) {
            for(const auto& other:c["installations"])if(other["id"]!=id)
                require(!same_identity(other,upgrade),"These resources already belong to another installation; add its location and remove the incomplete reference","E_DUPLICATE_IDENTITY");
            auto saved_id=i["id"],locations=i["locations"];i=upgrade;i["id"]=saved_id;i["locations"]=locations;
        } else if(!verified.is_null())i["status"]=verified["status"];
        else i["status"]=state;
        return true;
    },expected);
}
Json CatalogStore::recover(long long expected) {
    require(expected<0,"Recovery does not accept expected revision");
    auto backup=parse_json(read(paths_.config/"catalog.json.bak"));validate_catalog(backup);
    for(const auto& i:backup["installations"])for(const auto& l:i["locations"])if(l["location"]["platform"]==platform())paths_.protect_source(fs::u8path(l["location"]["path"].get<std::string>()));
    ensure_directory(paths_.config);Lock lock(paths_.config/"catalog.lock");
    // Reload under the lock, so a concurrent writer cannot replace the backup.
    backup=parse_json(read(paths_.config/"catalog.json.bak"));validate_catalog(backup);
    if(fs::exists(paths_.catalog())) {
        try { load();throw Error("E_RECOVERY","Current catalog is valid; refusing rollback"); }
        catch(const Error& e) { if(e.code=="E_RECOVERY"||e.code=="E_SCHEMA_VERSION")throw; }
        atomic_write(paths_.config/fs::u8path("catalog.corrupt."+uuid()+".json"),read(paths_.catalog()));
    }
    backup["revision"]=backup["revision"].get<long long>()+1;validate_catalog(backup);
    atomic_write(paths_.catalog(),backup.dump(2)+"\n");return backup;
}
}
