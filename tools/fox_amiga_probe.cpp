// SPDX-License-Identifier: GPL-3.0-or-later
// Diagnostic viewer/exporter. Not registered as a playable engine.
#include "titus/fox_amiga.hpp"
#include "titus/catalog.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
using namespace titus::fox_amiga;
using Json=nlohmann::json;
namespace fs=std::filesystem;
Bytes read(const fs::path& p) {
    std::ifstream f(titus::filesystem_path(p),std::ios::binary|std::ios::ate);
    if(!f || f.tellg()<0 || f.tellg()>4*1024*1024) throw std::runtime_error("Cannot read resource: "+p.filename().string());
    Bytes d(std::size_t(f.tellg()));f.seekg(0);if(!f.read(reinterpret_cast<char*>(d.data()),std::streamsize(d.size()))) throw std::runtime_error("Resource read failed");return d;
}
fs::path resolve(const fs::path& root,const std::string& name) {
    fs::path result;
    for(const auto& entry:fs::directory_iterator(titus::filesystem_path(root))) {
        auto a=entry.path().filename().string(),b=name;
        for(auto& c:a)c=char(std::tolower(static_cast<unsigned char>(c)));
        for(auto& c:b)c=char(std::tolower(static_cast<unsigned char>(c)));
        if(a==b && titus::path_is_regular_file(entry.path())) {if(!result.empty())throw std::runtime_error("Ambiguous resource name");result=entry.path();}
    }
    if(result.empty())throw std::runtime_error("Missing resource: "+name);
    return result;
}
void bmp(const fs::path& p,const Image& im,const std::array<std::uint32_t,64>& pal) {
    std::ofstream f(titus::filesystem_path(p),std::ios::binary);const unsigned size=54+im.width*im.height*4;
    auto put=[&](unsigned v,unsigned bytes){for(unsigned i=0;i<bytes;++i)f.put(char(v>>(8*i)));};
    f.write("BM",2);put(size,4);put(0,4);put(54,4);put(40,4);put(im.width,4);put(im.height,4);
    put(1,2);put(32,2);put(0,4);put(size-54,4);put(0,4);put(0,4);put(0,4);put(0,4);
    for(unsigned row=im.height;row>0;--row)for(unsigned x=0;x<im.width;++x)put(pal.at(im.pixels[(row-1)*im.width+x]),4);
    if(!f)throw std::runtime_error("Cannot write preview");
}
int probe_main(int argc,char** argv) try {
    fs::path root,output,palette;unsigned level=1,x=0,y=0;
    auto number=[](const std::string& value) {
        if(value.empty() || value.find_first_not_of("0123456789")!=std::string::npos)
            throw std::runtime_error("Expected a nonnegative integer");
        const auto n=std::stoul(value);
        if(n>65535)throw std::runtime_error("Numeric option out of range");
        return unsigned(n);
    };
    for(int i=1;i<argc;++i) {
        const std::string key=argv[i];
        if(i+1>=argc)throw std::runtime_error("Expected option value");
        const std::string value=argv[++i];
        if(key=="--datapath")root=fs::u8path(value);
        else if(key=="--output")output=fs::u8path(value);
        else if(key=="--palette")palette=fs::u8path(value);
        else if(key=="--level")level=number(value);
        else if(key=="--x")x=number(value);
        else if(key=="--y")y=number(value);
        else throw std::runtime_error("Unknown option: "+key);
    }
    if(root.empty() || output.empty() || level<1 || level>16 || level==15)
        throw std::runtime_error("Usage: titus-fox-amiga-probe --datapath FOLDER --output PRIVATE_FOLDER [--palette JSON] [--level 1..14/16] [--x PIXEL --y PIXEL]");
    auto canonical_root=titus::normalized_path(root), canonical_output=titus::normalized_path(output);
    root=canonical_root;output=canonical_output;
    auto relative=canonical_output.lexically_relative(canonical_root);
    if(relative.empty() || *relative.begin()!="..")throw std::runtime_error("Output must be outside original data directory");
    if(titus::path_exists(output))throw std::runtime_error("Use a new output directory to preserve previous exports");
    Json report={{"schema_version",1},{"playable",false},{"resources",Json::array()}};
    std::vector<fs::path> files;
    for(const auto& e:fs::directory_iterator(titus::filesystem_path(root)))if(titus::path_is_regular_file(e.path())) files.push_back(e.path());
    std::sort(files.begin(),files.end());
    for(const auto& p:files) {
        auto raw=read(p);
        if(raw.size()<6 || raw[1]!=0 || (raw[0]!=1 && raw[0]!=2)) continue;
        const auto decoded=unpack(raw);
        const std::string bytes(decoded.begin(),decoded.end());
        report["resources"].push_back({{"name",p.filename().string()},{"method",raw[0]},
            {"decoded_size",decoded.size()},{"decoded_sha256",titus::sha256(bytes)}});
    }
    std::ostringstream suffix;suffix<<std::setw(2)<<std::setfill('0')<<level;
    auto load=[&](const std::string& name){return unpack(read(resolve(root,name)));};
    auto map=load("LEVEL"+suffix.str()+".TBL"),bricks=load("BRIK"+suffix.str()+".BIN"),meta=load("POIL"+suffix.str()+".BIN");
    if(meta.size()!=3060 || map.empty() || map.size()%256)throw std::runtime_error("Invalid level resources");
    const auto ts=tiles(bricks);auto viewport=map_view(map,ts,x,y,640,std::min(400u,unsigned(map.size()/256)*16-y));
    std::array<std::uint32_t,64> pal{};
    if(palette.empty()) {for(unsigned i=0;i<64;++i)pal[i]=(i*4)*0x010101;report["palette"]="diagnostic grayscale";}
    else {
        std::ifstream f(titus::filesystem_path(palette));Json j;f>>j;
        if(j.at("main_sha256")!="18ca46dd02a3ad3623b03d425f0e0ae489b5090498882aea6d2578161010bc45" || j.at("palette").size()!=16 || j.at("level_color14").size()!=16)
            throw std::runtime_error("Unrecognized palette provenance");
        auto convert=[](unsigned v){if(v>4095)throw std::runtime_error("Invalid Amiga palette word");return (((v>>8)&15)*17<<16)|(((v>>4)&15)*17<<8)|((v&15)*17);};
        for(unsigned i=0;i<16;++i)pal[i]=convert(j["palette"][i].get<unsigned>());
        pal[14]=convert(j["level_color14"][level-1].get<unsigned>());report["palette"]="original executable RGB12";
    }
    titus::ensure_directory(output);
    bmp(output/"level.bmp",viewport,pal);
    report["level"]={{"number",level},{"width",4096},{"height",map.size()/256*16},{"x",x},{"y",y}};
    unsigned sprite_total=0;
    for(const auto& name:{"fox.spr","objet2.spr","monster2.spr","fonte.spr","bisou2.spr","theend.spr"}) {
        auto all=sprites(load(name));sprite_total+=unsigned(all.size());
        unsigned cell_w=8,cell_h=8;
        for(const auto& s:all){cell_w=std::max(cell_w,s.width+8);cell_h=std::max(cell_h,s.height+8);}
        const unsigned sheet_w=cell_w*8,sheet_h=unsigned((all.size()+7)/8)*cell_h;
        Image sheet{sheet_w,sheet_h,Bytes(std::size_t(sheet_w)*sheet_h,0)};
        for(std::size_t i=0;i<all.size();++i) {
            const auto& s=all[i];
            for(unsigned row=0;row<s.height;++row)for(unsigned col=0;col<s.width;++col)
                sheet.pixels[(i/8*cell_h+row)*sheet_w+i%8*cell_w+col]=s.pixels[row*s.width+col];
        }
        bmp(output/(std::string(name)+".bmp"),sheet,pal);
        report["sprites"][name]=all.size();
    }
    report["sprite_total"]=sprite_total;
    for(const auto& entry:std::vector<std::pair<std::string,unsigned>>{{"titus.pc1",4},{"titre2.pc1",5},{"present2.pc1",6}}) {
        auto p=picture(load(entry.first),entry.second,entry.second!=6);
        bmp(output/(entry.first+".bmp"),p.image,p.palette);
    }
    std::ofstream f(titus::filesystem_path(output/"report.json"));f<<report.dump(2)<<'\n';if(!f)throw std::runtime_error("Cannot write report");
    std::cout<<report.dump(2)<<'\n';return 0;
} catch(const std::exception& e) {std::cerr<<"Fox Amiga diagnostic: "<<e.what()<<'\n';return 1;}

#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    std::vector<std::string> utf8;std::vector<char*> pointers;
    for(int i=0;i<argc;++i)utf8.push_back(fs::path(argv[i]).u8string());
    for(auto& argument:utf8)pointers.push_back(argument.data());
    return probe_main(argc,pointers.data());
}
#else
int main(int argc,char** argv){return probe_main(argc,argv);}
#endif
