// SPDX-License-Identifier: GPL-3.0-or-later
// LZSS/MAT token and planar semantics follow Olduvai formats/lzss.cpp and
// formats/mat.cpp, Copyright (C) 2026 Krzysztof Sokolowski, revision
// a69e9204fc311c8810d696a6b799000d1bcd1642 (GPL-3.0-or-later).
// Independent strict readers: reject truncated input instead of zero filling.
#include "titus/pre1_amiga.hpp"
#include <array>
#include <algorithm>
#include <stdexcept>
#include <string>
namespace titus::pre1_amiga {
namespace {
[[noreturn]] void bad(const char* message){throw std::runtime_error(message);}
unsigned be(const Bytes& d,std::size_t pos,unsigned size){if(pos>d.size() || size>d.size()-pos)bad("Truncated Prehistorik Amiga field");unsigned value=0;while(size--)value=(value<<8)|d[pos++];return value;}
Bytes packbits(const Bytes& d,std::size_t begin,std::size_t length,std::size_t expected){
    if(begin>d.size() || length>d.size()-begin)bad("Truncated MAT packed record");
    Bytes out;out.reserve(expected);auto pos=begin,end=begin+length;
    while(pos<end){unsigned token=d[pos++];if(token==128)continue;unsigned run=token<128?token+1:257-token;
        if(run>expected-out.size())bad("MAT PackBits output overflow");
        if(token<128){if(run>end-pos)bad("Truncated MAT PackBits literal");out.insert(out.end(),d.begin()+pos,d.begin()+pos+run);pos+=run;}
        else{if(pos==end)bad("Truncated MAT PackBits repeat");out.insert(out.end(),run,d[pos++]);}
    }
    if(out.size()!=expected)bad("MAT PackBits output size mismatch");
    return out;
}
}
Bytes lzss(const Bytes& d){
    const auto size=be(d,0,4);if(!size || size>1048576 || size>(d.size()-4)*40/11)bad("Invalid Amiga LZSS output size");
    std::size_t pos=32;auto bits=[&](unsigned count){if(pos>d.size()*8 || count>d.size()*8-pos)bad("Truncated Amiga LZSS token");unsigned value=0;while(count--){value=(value<<1)|((d[pos/8]>>(7-pos%8))&1);++pos;}return value;};
    Bytes out;out.reserve(size);std::array<std::uint8_t,256> window{};
    auto append=[&](std::uint8_t v){window[out.size()%256]=v;out.push_back(v);};
    while(out.size()<size){if(!bits(1))append(std::uint8_t(bits(8)));else{unsigned run=bits(2)+2,distance=bits(8)+1;while(run-- && out.size()<size)append(window[(out.size()+256-distance)%256]);}}
    // Valid streams may finish within a token and retain final padding bits.
    return out;
}
std::vector<Sprite> sprites(const Bytes& d,bool mono){
    unsigned count=be(d,0,2);if(!count || count>4096)bad("Invalid MAT sprite count");std::size_t pos=2,pixels=0;std::vector<Sprite> result;
    for(unsigned i=0;i<count;++i){unsigned w=be(d,pos,2),h=be(d,pos+2,2),packed=be(d,pos+4,2);pos+=6;
        if(!w || !h || w>2048 || h>2048 || std::size_t(w)*h>16777216-pixels)bad("Invalid MAT dimensions or pixel budget");
        pixels+=std::size_t(w)*h;
        std::size_t row=(w+7)/8,plane=row*h,expected=plane*(mono?1:5),length=packed?packed:expected;
        if(mono && packed)bad("Unsupported packed monochrome MAT");
        if(pos>d.size() || length>d.size()-pos)bad("Truncated MAT pixels");
        Bytes raw=packed?packbits(d,pos,length,expected):Bytes(d.begin()+pos,d.begin()+pos+expected);pos+=length;
        Sprite sprite{w,h,Bytes(std::size_t(w)*h),Bytes(std::size_t(w)*h)};
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){std::size_t off=y*row+x/8,index=std::size_t(y)*w+x;unsigned bit=7-x%8;
            if(mono){sprite.colors[index]=15;sprite.opaque[index]=(raw[off]>>bit)&1;}
            else{for(unsigned p=0;p<4;++p)sprite.colors[index]|=((raw[p*plane+off]>>bit)&1)<<p;sprite.opaque[index]=(raw[4*plane+off]>>bit)&1;}
        }
        result.push_back(std::move(sprite));
    }
    if(pos!=d.size())bad("Trailing MAT records");
    return result;
}
Sample sample_8svx(const Bytes& d){
    if(d.size()<12 || !std::equal(d.begin(),d.begin()+4,"FORM") || !std::equal(d.begin()+8,d.begin()+12,"8SVX") || be(d,4,4)!=d.size()-8)bad("Invalid IFF 8SVX container");
    Sample sample;bool header=false,body=false;std::size_t pos=12;
    while(pos<d.size()){
        if(d.size()-pos<8)bad("Truncated IFF chunk header");
        unsigned length=be(d,pos+4,4);auto begin=pos+8;if(length>d.size()-begin || (length&1)>d.size()-begin-length)bad("Truncated IFF chunk");
        std::string id(d.begin()+pos,d.begin()+pos+4);
        if(id=="VHDR"){
            if(header || length!=20)bad("Invalid or duplicate 8SVX VHDR");
            header=true;sample.one_shot=be(d,begin,4);sample.repeat=be(d,begin+4,4);sample.cycle=be(d,begin+8,4);sample.rate=be(d,begin+12,2);sample.volume=be(d,begin+16,4);
            if(!sample.rate || d[begin+14]!=1 || d[begin+15]!=0 || sample.volume>65536)bad("Unsupported 8SVX encoding");
        }else if(id=="BODY"){
            if(body || length>1048576)bad("Invalid or duplicate 8SVX BODY");
            body=true;sample.pcm.assign(d.begin()+begin,d.begin()+begin+length);
        }
        pos=begin+length+(length&1);
    }
    if(!header || !body || sample.pcm.empty() || sample.one_shot>sample.pcm.size() || sample.repeat>sample.pcm.size()-sample.one_shot)bad("Invalid 8SVX sample bounds");
    return sample;
}
}
