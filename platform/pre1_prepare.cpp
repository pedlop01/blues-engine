// DIET decoder adapted from Deark modules/diet.c, Copyright (C) 2023 Jason Summers.
// MIT-style license: engines/olduvai/licenses/Deark-MIT.txt.
// Restricted to the audited DIET 1.02-1.20 PRE1.EXE; never executes DOS code.
#include "titus/pre1_prepare.hpp"
#include <fstream>
#include <map>
namespace titus {
namespace {
constexpr auto packed_sha="b03ce6913d07a4c5b66dfc4843d185ffdff6561e9bf91f3333b096016690406f";
constexpr auto decoded_sha="5cea262f9610657af8b55c10b277ec30ffe99df35294d5f8c9f520a30e029c96";
void need(bool b,const char* msg){if(!b)throw Error("E_PREPARATION",msg);}
unsigned word(const std::string& s,size_t p){need(p+2<=s.size(),"Truncated DIET word");return static_cast<unsigned char>(s[p])|(static_cast<unsigned char>(s[p+1])<<8);}
void putword(std::string& s,unsigned v){s+=char(v);s+=char(v>>8);}
Json profile(){Detector detector;for(const auto& p:detector.profiles())if(p["game_id"]=="prehistorik"&&p["edition_id"]=="dos")return p;throw Error("E_PREPARATION","Missing Prehistorik profile");}
std::string upper(std::string s){for(auto& c:s)if(c>='a'&&c<='z')c-=32;return s;}
}
std::string decode_pre1_executable(const std::string& s){
    need(sha256(s)==packed_sha,"Unrecognised PRE1.EXE variant");
    constexpr size_t start=98,length=77487;need(s.size()>=start+length&&s.substr(87,3)=="dlz"&&word(s,93)==0x2f77,"Invalid DIET header");
    unsigned crc=0;for(size_t i=start;i<start+length;++i){crc^=static_cast<unsigned char>(s[i]);for(int n=0;n<8;++n)crc=(crc>>1)^((crc&1)?0xa001:0);}need(crc==0x2f77,"DIET CRC mismatch");
    size_t pos=start;unsigned bits=0,left=0;std::string raw;bool ended=false;
    auto byte=[&](){need(pos<start+length,"Truncated DIET input");return unsigned(static_cast<unsigned char>(s[pos++]));};
    auto refill=[&](){bits=byte();bits|=byte()<<8;left=16;};
    auto bit=[&](){if(!left)refill();unsigned b=bits&1;bits>>=1;if(!--left)refill();return b;};
    for(unsigned steps=0;steps<500000;++steps){
        need(raw.size()<=200000,"DIET output limit exceeded");
        if(bit()){raw+=char(byte());continue;}
        unsigned x2=bit(),v=byte(),distance=0,count=2;
        if(!x2){if(bit()){unsigned a=bit(),b=bit(),c=bit();distance=2304-(1024*a+512*b+256*c+v);}
            else if(v!=255)distance=256-v;
            else{if(!bit()){ended=true;break;}continue;}}
        else{
            unsigned a=bit(),b=bit();
            if(b)distance=512-(256*a+v);
            else if(bit())distance=1024-(256*a+v);
            else{unsigned c=bit(),d=bit();if(d)distance=2048-(512*a+256*c+v);
                else{unsigned e=bit(),f=bit();if(f)distance=4096-(1024*a+512*c+256*e+v);else{unsigned g=bit();distance=8192-(2048*a+1024*c+512*e+256*g+v);}}}
            unsigned n=0;while(n<4){++n;if(bit()){count=2+n;break;}}
            if(n==4&&count==2){unsigned a1=bit(),a2=bit();if(a1)count=7+a2;else if(!a2){unsigned a3=bit(),a4=bit(),a5=bit();count=9+4*a3+2*a4+a5;}else count=17+byte();}
        }
        need(distance>0&&distance<=raw.size()&&count<=distance&&raw.size()+count<=200000,"Invalid DIET history reference");raw.append(raw,raw.size()-distance,count);
    }
    need(ended&&pos==start+length&&raw.size()==162962,"DIET stream did not end at audited boundary");
    // DIET stores the EXE header/encoded relocations after the code image and elides zero padding.
    constexpr size_t mz=155974;raw.resize(200000,0);std::string h=raw.substr(mz,28);need(h.substr(0,2)=="MZ","Missing reconstructed MZ header");h[3]&=1;
    unsigned relocations=word(h,6),code_start=word(h,8)*16,reloc_pos=word(h,24);size_t p=mz+reloc_pos;unsigned segment=0,offset=0;std::string table;
    need(code_start==8192&&relocations==1917&&reloc_pos==28,"Unsupported DIET relocation layout");
    for(unsigned i=0;i<relocations;++i){unsigned n=word(raw,p);p+=2;if(n&0x8000)offset=(offset+(n>=0xc000?n:n-0x8000))&65535;else{segment=n;offset=word(raw,p);p+=2;}putword(table,offset);putword(table,segment);}
    std::string out=h+raw.substr(mz+28,reloc_pos-28)+table+raw.substr(p,code_start-(reloc_pos+4*relocations))+raw.substr(0,mz);
    need(out.size()==164166&&sha256(out)==decoded_sha,"Reconstructed Prehistorik executable signature mismatch");return out;
}
fs::path pre1_prepared_directory(const Json& r){return fs::u8path(r["paths"]["work_directory"]["path"].get<std::string>())/"prepared"/fs::u8path(r["sources"][0]["fingerprint"].get<std::string>()+".pre1-diet-v1");}
static bool valid_directory(const Json& r,const fs::path& root){try{
    if(path_is_symlink(root)||normalized_path(root)!=root.lexically_normal())return false;
    auto m=parse_json(read_document(root/"manifest.json",8192));if(m.at("schema_version")!=1||m.at("preparer_id")!="pre1-diet-v1"||m.at("source_fingerprint")!=r["sources"][0]["fingerprint"])return false;
    auto p=profile();for(const auto& f:p["files"]){auto name=f["name"].get<std::string>();auto file=root/(name=="pre1.exe"?"HISTORIK.EXE":upper(name));if(path_is_symlink(file)||file_sha256(file)!=(name=="pre1.exe"?Json(decoded_sha):f["sha256"]))return false;}return true;
}catch(const std::exception&){return false;}}
bool pre1_prepared_valid(const Json& r){return valid_directory(r,pre1_prepared_directory(r));}
void prepare_pre1(const Json& r,bool fail){if(pre1_prepared_valid(r))return;auto target=pre1_prepared_directory(r),stage=target;need(normalized_path(target.parent_path())==target.parent_path().lexically_normal(),"Prehistorik cache directory contains an alias");stage+=fs::u8path(".tmp."+uuid());ensure_directory(stage);
    auto source=fs::u8path(r["sources"][0]["location"]["path"].get<std::string>());std::map<std::string,fs::path> files;
    for(const auto& e:fs::directory_iterator(filesystem_path(source))){auto n=e.path().filename().u8string();for(auto& c:n)if(c>='A'&&c<='Z')c+=32;need(files.emplace(n,e.path()).second,"Ambiguous source filenames");}
    auto p=profile();for(const auto& f:p["files"]){auto name=f["name"].get<std::string>();auto input=files.at(name);need(!path_is_symlink(input)&&file_sha256(input)==f["sha256"],"Prehistorik source changed during preparation");if(name=="pre1.exe"){auto data=decode_pre1_executable(read_document(input,1024*1024));std::ofstream out(filesystem_path(stage/"HISTORIK.EXE"),std::ios::binary);out.write(data.data(),data.size());out.flush();need(bool(out),"Cannot write prepared executable data");}else copy_file_exact(input,stage/upper(name));if(fail)throw Error("E_TEST_PREPARATION","Deliberate preparation failure");}
    write_document(stage/"manifest.json",{{"schema_version",1},{"kind","prehistorik_preparation"},{"preparer_id","pre1-diet-v1"},{"source_fingerprint",r["sources"][0]["fingerprint"]},{"decoded_sha256",decoded_sha},{"crc16","2f77"}});
    need(valid_directory(r,stage),"Invalid staged Prehistorik data");if(path_exists(target)){auto bad=target;bad+=fs::u8path(".invalid."+uuid());move_directory(target,bad);}move_directory(stage,target);
}
}
