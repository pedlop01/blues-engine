// SPDX-License-Identifier: GPL-3.0-or-later
// TH-15 diagnostic resources, not a playable or registered runtime.
#include "titus/pre1_amiga.hpp"
#include "titus/fox_amiga.hpp"
#include "titus/catalog.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>
using namespace titus::pre1_amiga;
using Json=nlohmann::json;
namespace fs=std::filesystem;
namespace {
Bytes read(const fs::path& path){std::ifstream f(titus::filesystem_path(path),std::ios::binary|std::ios::ate);if(!f || f.tellg()<0 || f.tellg()>4*1024*1024)throw std::runtime_error("Cannot read resource");Bytes bytes(std::size_t(f.tellg()));f.seekg(0);if(!f.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size())))throw std::runtime_error("Cannot read resource bytes");return bytes;}
std::string hash(const Bytes& bytes){return titus::sha256(std::string(bytes.begin(),bytes.end()));}
void bmp(const fs::path& path,unsigned width,unsigned height,const std::vector<std::uint32_t>& pixels){
    std::ofstream f(titus::filesystem_path(path),std::ios::binary);auto put=[&](unsigned value,unsigned size){for(unsigned i=0;i<size;++i)f.put(char(value>>(8*i)));};
    f.write("BM",2);put(54+width*height*4,4);put(0,4);put(54,4);put(40,4);put(width,4);put(height,4);put(1,2);put(32,2);put(0,4);put(width*height*4,4);for(unsigned i=0;i<4;++i)put(0,4);
    for(unsigned y=height;y>0;--y)for(unsigned x=0;x<width;++x)put(pixels[std::size_t(y-1)*width+x],4);
    if(!f)throw std::runtime_error("Cannot write private preview");
}
}
int probe_main(int argc,char** argv){try{
    fs::path root,output;for(int i=1;i<argc;i+=2){if(i+1>=argc)throw std::runtime_error("Missing argument");std::string key=argv[i];if(key=="--datapath")root=fs::u8path(argv[i+1]);else if(key=="--output")output=fs::u8path(argv[i+1]);else throw std::runtime_error("Unknown argument");}
    if(root.empty() || output.empty())throw std::runtime_error("Usage: titus-pre1-amiga-probe --datapath ORIGINALS --output NEW_PRIVATE_DIRECTORY");
    root=titus::normalized_path(root);output=titus::normalized_path(output);auto relative=output.lexically_relative(root);if((!relative.empty() && *relative.begin()!="..") || titus::path_exists(output))throw std::runtime_error("Use new output outside original resources");
    std::vector<fs::path> files;for(const auto& entry:fs::directory_iterator(titus::filesystem_path(root)))if(titus::path_is_regular_file(entry.path()))files.push_back(entry.path());std::sort(files.begin(),files.end());
    Json report={{"schema_version",1},{"playable",false},{"diagnostic_only",true},{"sprite_palette_assignment_verified",false},{"resources",Json::array()}};titus::ensure_directory(output);
    for(const auto& path:files){auto data=read(path);std::string name=path.filename().u8string(),lower=name;for(auto& c:lower)c=char(std::tolower(static_cast<unsigned char>(c)));Json row={{"name",name},{"sha256",hash(data)}};
        if(fs::path(lower).extension()==".mat"){
            bool mono=lower=="char_set1.mat";auto raw=mono?data:lzss(data);auto bank=sprites(raw,mono);row["format"]="mat";row["decoded_sha256"]=hash(raw);row["sprites"]=Json::array();
            for(const auto& image:bank)row["sprites"].push_back({{"width",image.width},{"height",image.height},{"colors_sha256",hash(image.colors)},{"mask_sha256",hash(image.opaque)}});
            // Diagnostic grayscale, not an invented game palette. Checkerboard
            // distinguishes transparent pixels from opaque black.
            unsigned count=std::min(64u,unsigned(bank.size())),cw=0,ch=0;for(unsigned i=0;i<count;++i){cw=std::max(cw,bank[i].width+4);ch=std::max(ch,bank[i].height+4);}unsigned cols=std::min({8u,count,4096/cw}),width=cw*cols,height=ch*((count+cols-1)/cols);
            if(!cols || height>8192 || std::size_t(width)*height>16777216)throw std::runtime_error("Diagnostic atlas exceeds pixel budget");
            std::vector<std::uint32_t> pixels(std::size_t(width)*height);
            for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)pixels[std::size_t(y)*width+x]=((x/8+y/8)&1)?0x303050:0x707090;
            for(unsigned i=0;i<count;++i)for(unsigned y=0;y<bank[i].height;++y)for(unsigned x=0;x<bank[i].width;++x){auto index=std::size_t(y)*bank[i].width+x;if(bank[i].opaque[index])pixels[std::size_t((i/cols)*ch+y+2)*width+(i%cols)*cw+x+2]=bank[i].colors[index]*0x111111;}
            bmp(output/(name+".bmp"),width,height,pixels);
        }else if(fs::path(lower).extension()==".pc1"){
            auto picture=titus::fox_amiga::picture(data,4,true);row["format"]="pc1";row["pixels_sha256"]=hash(picture.image.pixels);row["palette"]=picture.palette;std::vector<std::uint32_t> pixels;for(auto index:picture.image.pixels)pixels.push_back(picture.palette[index]);bmp(output/(name+".bmp"),320,200,pixels);
        }else if(data.size()>=12 && std::equal(data.begin(),data.begin()+4,"FORM") && std::equal(data.begin()+8,data.begin()+12,"8SVX")){
            auto sample=sample_8svx(data);row.update({{"format","8svx"},{"rate",sample.rate},{"one_shot",sample.one_shot},{"repeat",sample.repeat},{"cycle",sample.cycle},{"volume",sample.volume},{"pcm_size",sample.pcm.size()},{"pcm_sha256",hash(sample.pcm)}});
        }else continue;
        report["resources"].push_back(row);
    }
    std::ofstream f(titus::filesystem_path(output/"report.json"));f<<report.dump(2)<<'\n';if(!f)throw std::runtime_error("Cannot write diagnostic report");std::cout<<Json({{"playable",false},{"resources",report["resources"].size()},{"report",(output/"report.json").u8string()}}).dump()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"Prehistorik Amiga diagnostic: "<<e.what()<<'\n';return 1;}}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){std::vector<std::string> utf8;std::vector<char*> pointers;for(int i=0;i<argc;++i)utf8.push_back(fs::path(argv[i]).u8string());for(auto& arg:utf8)pointers.push_back(arg.data());return probe_main(argc,pointers.data());}
#else
int main(int argc,char** argv){return probe_main(argc,argv);}
#endif
