// SPDX-License-Identifier: GPL-3.0-or-later
// Native readers derived from local Fox Amiga format observations. No game code
// or resources embedded. DOS SQZ and Blues SQV are different formats.
#include "titus/fox_amiga.hpp"
#include <stdexcept>
#include <algorithm>
namespace titus::fox_amiga {
namespace {
[[noreturn]] void bad(const char* message) { throw std::runtime_error(message); }
std::uint16_t word(const Bytes& d, std::size_t p, bool be=false) {
    if(p>d.size() || d.size()-p<2) bad("Truncated word");
    return be ? (d[p]<<8)|d[p+1] : d[p]|(d[p+1]<<8);
}
Image planar(const Bytes& raw, std::size_t start, unsigned w, unsigned h, unsigned planes) {
    if(!w || !h || w>4096 || h>4096 || !planes || planes>6) bad("Invalid planar dimensions");
    const std::size_t row=(w+7)/8, plane=row*h;
    if(start>raw.size() || plane*planes>raw.size()-start) bad("Truncated planar pixels");
    Image out{w,h,Bytes(std::size_t(w)*h)};
    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
        auto& v=out.pixels[std::size_t(y)*w+x];
        for(unsigned p=0;p<planes;++p)
            v|=((raw[start+p*plane+y*row+x/8]>>(7-x%8))&1)<<p;
    }
    return out;
}
Bytes huffman(const Bytes& d,std::size_t expected) {
    if(d.size()<13 || word(d,6)!=0xff76) bad("Invalid Huffman header");
    auto end=std::find(d.begin()+10,d.begin()+std::min<std::size_t>(d.size(),110),0);
    if(end==d.begin()+std::min<std::size_t>(d.size(),110)) bad("Missing Huffman name terminator");
    std::size_t pos=std::size_t(end-d.begin())+1;
    const auto count=word(d,pos); pos+=2;
    if(!count || count>257 || count*4u>d.size()-pos) bad("Invalid Huffman tree size");
    std::vector<std::array<int,2>> tree;
    for(unsigned i=0;i<count;++i) {
        std::array<int,2> edges{};
        for(unsigned j=0;j<2;++j) {
            unsigned v=word(d,pos);pos+=2;
            edges[j]=v>=32768 ? int(v)-65536 : int(v);
            if(edges[j]>=int(count) || edges[j]<-257) bad("Invalid Huffman edge");
        }
        tree.push_back(edges);
    }
    Bytes out;out.reserve(expected);unsigned node=0;
    for(;pos<d.size();++pos) for(unsigned bit=0;bit<8;++bit) {
        const int edge=tree[node][(d[pos]>>bit)&1];
        if(edge>=0) node=unsigned(edge);
        else {
            const unsigned value=unsigned(-edge-1);
            if(value==256) {
                if(out.size()!=expected) bad("Huffman size mismatch");
                return out;
            }
            if(out.size()==expected) bad("Huffman output overflow");
            out.push_back(std::uint8_t(value));node=0;
        }
    }
    bad("Missing Huffman end symbol");
}
Bytes lzw(const Bytes& d,std::size_t expected) {
    if(d.size()<8 || d[6]!=12) bad("Unsupported LZW maximum width");
    // LSB-first groups of eight codes. Width changes and clear discard group
    // padding, unlike the MSB-first DOS SQZ stream.
    unsigned width=9,limit=511,next=257,bit=0,available=0;
    std::size_t cursor=7;
    std::array<std::uint8_t,12> group{};
    auto code=[&](bool reset=false) -> int {
        if(reset || next>limit || bit>=available) {
            if(next>limit) {
                if(width>=12) bad("LZW width overflow");
                ++width;limit=width==12?4096:(1u<<width)-1;
            }
            if(reset) { width=9;limit=511; }
            const auto take=std::min<std::size_t>(width,d.size()-cursor);
            group.fill(0);std::copy_n(d.begin()+cursor,take,group.begin());cursor+=take;
            bit=0;available=take*8>=width ? unsigned(take*8-(width-1)) : 0;
            if(!available) bad("Truncated LZW group");
        }
        unsigned value=0;
        for(unsigned i=0;i<width;++i) value|=((group[(bit+i)/8]>>((bit+i)%8))&1)<<i;
        bit+=width;return int(value);
    };
    std::array<Bytes,4096> dict;
    auto reset_dict=[&]() { for(unsigned i=0;i<4096;++i) dict[i]=i<256?Bytes{std::uint8_t(i)}:Bytes{}; };
    reset_dict();int k=code();
    if(k<0 || k>255) bad("Invalid first LZW code");
    Bytes out=dict[k],old=dict[k];out.reserve(expected);
    while(out.size()<expected) {
        k=code();
        if(k==256) {
            reset_dict();next=256;k=code(true);
            if(k>255) bad("Invalid LZW code after clear");
        }
        Bytes value;
        if(k>=0 && k<int(next) && !dict[k].empty()) value=dict[k];
        else if(k==int(next)) { value=old;value.push_back(old.front()); }
        else bad("Invalid LZW dictionary reference");
        if(value.size()>expected-out.size()) bad("LZW output overflow");
        out.insert(out.end(),value.begin(),value.end());
        if(next<4096) { dict[next]=old;dict[next++].push_back(value.front()); }
        old=std::move(value);
    }
    return out;
}
}
Bytes unpack(const Bytes& d) {
    if(d.size()<6 || d.size()>4*1024*1024) bad("Invalid packed resource size");
    const std::size_t expected=std::size_t(word(d,2))|(std::size_t(word(d,4))<<16);
    if(!expected || expected>1024*1024) bad("Invalid unpacked resource size");
    const auto method=word(d,0);
    if(method==1) return huffman(d,expected);
    if(method==2) return lzw(d,expected);
    bad("Unknown Fox Amiga compression method");
}
std::vector<Image> sprites(const Bytes& d) {
    const unsigned count=word(d,0,true);std::size_t pos=2;
    if(!count || count>1024) bad("Invalid sprite count");
    std::vector<Image> result;
    for(unsigned i=0;i<count;++i) {
        const unsigned w=word(d,pos,true),h=word(d,pos+2,true),packed=word(d,pos+4,true);pos+=6;
        if(packed) bad("Unsupported inner sprite compression");
        if(w>512 || h>512) bad("Invalid sprite dimensions");
        result.push_back(planar(d,pos,w,h,4));pos+=std::size_t((w+7)/8)*h*4;
    }
    if(pos!=d.size()) bad("Trailing sprite bytes");
    return result;
}
std::vector<Image> tiles(const Bytes& d) {
    if(d.size()!=32768) bad("Invalid tileset size");
    std::vector<Image> result;
    for(unsigned i=0;i<256;++i) result.push_back(planar(d,i*128,16,16,4));
    return result;
}
Picture picture(const Bytes& d,unsigned planes,bool encoded) {
    if(planes<4 || planes>6) bad("Invalid picture plane count");
    const unsigned colors=std::min(1u<<planes,32u), header=colors*3;
    if(d.size()<header) bad("Truncated picture palette");
    Picture out;
    // Three component arrays, 32 entries each, converted as 12-bit Amiga RGB.
    for(unsigned i=0;i<colors;++i) {
        const auto r=(d[i]>>4)*17,g=(d[colors+i]>>4)*17,b=(d[colors*2+i]>>4)*17;
        out.palette[i]=(r<<16)|(g<<8)|b;
        out.palette[i+32]=(((r/17/2)*17)<<16)|(((g/17/2)*17)<<8)|((b/17/2)*17); // RGB12 extra-half-brite
    }
    Bytes raw;
    if(!encoded) {
        if(d.size()!=header+planes*8000) bad("Invalid raw picture size");
        raw.assign(d.begin()+header,d.end());
    } else {
        const std::size_t flag_count=planes*1000;
        if(d.size()<header+8) bad("Truncated picture flag header");
        const std::size_t flags_size=(std::size_t(word(d,header,true))<<16)|word(d,header+2,true);
        if(flags_size>d.size()-header-8) bad("Invalid compressed flag size");
        const std::size_t flags_end=header+4+flags_size;
        std::size_t pos=header+4,flag=0;
        Bytes flags_data;flags_data.reserve(flag_count);
        while(flags_data.size()<flag_count) {
            if(pos==d.size()) bad("Truncated PackBits flag stream");
            const auto token=d[pos++];
            if(token==128) continue;
            const unsigned run=token<128?token+1:257-token;
            if(run>flag_count-flags_data.size()) bad("PackBits flag overflow");
            if(token<128) {
                if(run>d.size()-pos) bad("Truncated PackBits literals");
                flags_data.insert(flags_data.end(),d.begin()+pos,d.begin()+pos+run);pos+=run;
            } else {
                if(pos==d.size()) bad("Truncated PackBits repeat");
                flags_data.insert(flags_data.end(),run,d[pos++]);
            }
        }
        if(pos!=flags_end) bad("Compressed flag size mismatch");
        if(d.size()-pos<4) bad("Truncated picture literal header");
        const std::size_t literal_size=(std::size_t(word(d,pos,true))<<16)|word(d,pos+2,true);
        if(literal_size!=d.size()-pos-4) bad("Picture literal size mismatch");
        pos+=4;
        raw.resize(planes*8000);std::uint8_t previous=0;
        for(unsigned p=0;p<planes;++p) for(unsigned x=0;x<40;++x)
            for(unsigned group=0;group<25;++group) {
                auto flags=flags_data[flag++];
                for(unsigned j=0;j<8;++j) {
                    if(!(flags&(1u<<j))) {
                        if(pos==d.size()) bad("Truncated picture literal");
                        previous=d[pos++];
                    }
                    raw[p*8000+(group*8+j)*40+x]=previous;
                }
            }
        if(pos!=d.size()) bad("Trailing picture literals");
    }
    out.image=planar(raw,0,320,200,planes);return out;
}
Image map_view(const Bytes& map,const std::vector<Image>& ts,unsigned x,unsigned y,unsigned w,unsigned h) {
    if(map.empty() || map.size()%256 || map.size()>65536 || ts.size()!=256 || !w || !h || w>4096 || h>4096)
        bad("Invalid map viewport");
    const unsigned mh=unsigned(map.size()/256)*16;
    if(x>=4096 || y>=mh || w>4096-x || h>mh-y) bad("Map viewport out of bounds");
    Image out{w,h,Bytes(std::size_t(w)*h)};
    for(unsigned row=0;row<h;++row) for(unsigned col=0;col<w;++col) {
        const auto& t=ts[map[((y+row)/16)*256+(x+col)/16]];
        if(t.width!=16 || t.height!=16 || t.pixels.size()!=256) bad("Invalid map tile");
        out.pixels[std::size_t(row)*w+col]=t.pixels[((y+row)%16)*16+(x+col)%16];
    }
    return out;
}
}
