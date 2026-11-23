#include "titus/fox_prepare.hpp"
#include <array>
#include <fstream>
#include <algorithm>
namespace titus {
namespace {
constexpr auto source_sha="a47ab6e4f8016db90e727df2eef5450edf1e16ecb6c90f5cb31ad214fef65f7b";
constexpr auto image_sha="f398540b9bd65ef64fcd7898285cafb30f95e456d8b3d7a5123d2337c0c0df41";
constexpr auto music_sha="386dbe1cad7a67e20ec3edb98bdb116c035e68cc762cd85762d2bedbb1215986";
void need(bool b,const char* text){if(!b)throw Error("E_PREPARATION",text);}
std::string upper(std::string s){for(auto& c:s)if(c>='a'&&c<='z')c-=32;return s;}
Json fox_profile(){Detector detector;for(const auto& p:detector.profiles())if(p["game_id"]=="fox"&&p["edition_id"]=="dos")return p;throw Error("E_PREPARATION","Fox profile missing");}
}
std::string decode_fox_audio(const std::string& packed){
    need(packed.size()==44389&&sha256(packed)==source_sha,"Unrecognised packed Fox executable");
    // Audited ZMiro container: four-byte size/header followed by MSB-first LZW.
    // This reads data only; it does not emulate/execute instructions or relocate MZ.
    size_t pos=(790+4)*8;unsigned width=9,limit=512,next=258;
    std::array<std::string,4096> dictionary;for(unsigned n=0;n<256;++n)dictionary[n]=std::string(1,static_cast<char>(n));
    std::string output,previous;bool ended=false;
    for(unsigned step=0;step<100000;++step){
        if(next==limit&&width<12){++width;limit*=2;}
        need(pos+width<=packed.size()*8,"Truncated LZW input");unsigned code=0;
        for(unsigned bit=0;bit<width;++bit,++pos)code=(code<<1)|((static_cast<unsigned char>(packed[pos/8])>>(7-pos%8))&1);
        if(code==256){width=9;limit=512;next=258;previous.clear();continue;}
        if(code==257){ended=true;break;}
        std::string entry;
        if(previous.empty()){need(code<256,"Invalid initial LZW code");entry=dictionary[code];}
        else{need(code<=next&&code<4096,"Invalid LZW dictionary code");entry=code==next?previous+previous[0]:dictionary[code];need(!entry.empty()&&entry.size()<=64000,"Invalid LZW expansion");}
        need(output.size()+entry.size()<=64000,"LZW output limit exceeded");output+=entry;
        if(!previous.empty()&&next<4096)dictionary[next++]=previous+entry[0];previous=std::move(entry);
    }
    need(ended&&output.size()==63985&&sha256(output)==image_sha,"Decoded Fox image signature mismatch");
    auto music=output.substr(28677,18749);need(sha256(music)==music_sha,"Fox audio signature mismatch");return music;
}
fs::path fox_prepared_directory(const Json& request){return fs::u8path(request["paths"]["work_directory"]["path"].get<std::string>())/"prepared"/fs::u8path(request["sources"][0]["fingerprint"].get<std::string>()+".fox-lzw-v1");}
static bool fox_directory_valid(const Json& request,const fs::path& root){try{
    if(path_is_symlink(root)||normalized_path(root)!=root.lexically_normal())return false;
    auto m=parse_json(read_document(root/"manifest.json",32768));
    if(m.at("schema_version")!=1||m.at("preparer_id")!="fox-lzw-v1"||m.at("source_fingerprint")!=request["sources"][0]["fingerprint"]||m.at("decoded_image_sha256")!=image_sha)return false;
    auto expected=fox_profile()["files"];size_t count=0;
    for(const auto& f:expected){auto name=f["name"].get<std::string>();if(name=="fox.com")continue;auto p=root/upper(name);if(path_is_symlink(p)||path_file_size(p)!=f["size"]||file_sha256(p)!=f["sha256"])return false;++count;}
    if(path_is_symlink(root/"music.bin")||path_file_size(root/"music.bin")!=18749||file_sha256(root/"music.bin")!=music_sha)return false;
    return m.at("file_count")==count+1;
}catch(const std::exception&){return false;}}
bool fox_prepared_valid(const Json& request){return fox_directory_valid(request,fox_prepared_directory(request));}
void prepare_fox(const Json& request,bool inject_failure){
    if(fox_prepared_valid(request))return;
    auto target=fox_prepared_directory(request),stage=target;need(normalized_path(target.parent_path())==target.parent_path().lexically_normal(),"Fox cache directory contains an alias");stage+=fs::u8path(".tmp."+uuid());ensure_directory(stage);
    auto source=fs::u8path(request["sources"][0]["location"]["path"].get<std::string>());
    std::map<std::string,fs::path> files;for(const auto& entry:fs::directory_iterator(filesystem_path(source))){auto n=entry.path().filename().u8string();for(auto& c:n)if(c>='A'&&c<='Z')c+=32;need(files.emplace(n,entry.path()).second,"Ambiguous Fox filenames");}
    size_t count=0;
    auto profile=fox_profile();
    for(const auto& f:profile["files"]){auto name=f["name"].get<std::string>();auto p=files.at(name);need(!path_is_symlink(p)&&path_file_size(p)==f["size"]&&file_sha256(p)==f["sha256"],"Fox source changed during preparation");
        if(name=="fox.com"){auto data=decode_fox_audio(read_document(p,65536));std::ofstream out(filesystem_path(stage/"music.bin"),std::ios::binary);out.write(data.data(),data.size());out.flush();need(bool(out),"Cannot write prepared audio");}
        else copy_file_exact(p,stage/upper(name));
        ++count;if(inject_failure)throw Error("E_TEST_PREPARATION","Deliberate preparation failure");
    }
    write_document(stage/"manifest.json",{{"schema_version",1},{"kind","fox_preparation"},{"preparer_id","fox-lzw-v1"},{"source_fingerprint",request["sources"][0]["fingerprint"]},{"decoded_image_sha256",image_sha},{"music_sha256",music_sha},{"file_count",count}});
    need(fox_directory_valid(request,stage),"Incomplete staged Fox preparation");
    if(path_exists(target)){auto bad=target;bad+=fs::u8path(".invalid."+uuid());move_directory(target,bad);}
    move_directory(stage,target);need(fox_prepared_valid(request),"Published Fox preparation failed verification");
}
}
