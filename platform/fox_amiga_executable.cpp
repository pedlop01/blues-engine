// SPDX-License-Identifier: GPL-3.0-or-later
#include "titus/fox_amiga_executable.hpp"
#include <algorithm>
#include <stdexcept>
namespace titus::fox_amiga {
namespace {
[[noreturn]] void invalid(){throw std::runtime_error("Invalid or unsupported Fox Amiga packed executable");}
unsigned be32(const Bytes& b,std::size_t p){if(p>b.size() || b.size()-p<4)invalid();return (unsigned(b[p])<<24)|(unsigned(b[p+1])<<16)|(unsigned(b[p+2])<<8)|b[p+3];}
class Stream {
    const Bytes& packed_;std::size_t cursor_;
    std::array<std::uint8_t,516> ring_{};
    unsigned position_=0,literals_=0,match_=0,bits_=128,source_=0;
    std::uint32_t tokens_=0;
    unsigned raw(){if(!cursor_)invalid();return packed_[--cursor_];}
    unsigned bit(){unsigned carry=bits_>>7;bits_=(bits_<<1)&255;if(!bits_){unsigned n=raw();bits_=((n<<1)|carry)&255;carry=n>>7;}return carry;}
    unsigned bits(unsigned count){if(count>16)invalid();unsigned n=0;while(count--)n=(n<<1)|bit();return n;}
    void refill(){
        unsigned output=516;
        while(output){
            while(match_ && output){if(!source_)source_=516;if(source_>516)invalid();ring_[--output]=ring_[--source_];--match_;}
            if(!output)break;
            while(literals_ && output){ring_[--output]=std::uint8_t(raw());--literals_;}
            if(!output)break;
            if(!--tokens_)break;
            unsigned selector;
            if(!bit()){match_=2;selector=0;}
            else if(!bit()){match_=3;selector=1;}
            else if(!bit()){match_=4;selector=2;}
            else if(!bit()){match_=5;selector=3;}
            else if(!bit()){match_=bits(3)+6;selector=3;}
            else {match_=raw();if(!match_)match_=256;selector=3;}
            // Literal bytes precede the following match in reverse decoding.
            if(!bit())literals_=bit();
            else if(!bit()){
                constexpr unsigned counts[]={2,3,3,4};literals_=bits(counts[selector])+2;
            }else{
                constexpr unsigned counts[]={4,5,7,12},bases[]={6,10,10,18};literals_=bits(counts[selector])+bases[selector];
            }
            unsigned base=0,count;
            if(!bit()){constexpr unsigned counts[]={5,6,7,7};count=counts[selector];}
            else if(!bit()){constexpr unsigned bases[]={32,64,128,128};base=bases[selector];count=7;}
            else {constexpr unsigned bases[]={160,192,256,256};base=bases[selector];count=0;}
            const unsigned distance=base+(count?bits(count):raw())+1;
            if(distance>516)invalid();
            source_=output+distance;if(source_>516)source_-=516;
            // Match is emitted first; the pending literals follow that match.
            while(match_ && output){if(!source_)source_=516;ring_[--output]=ring_[--source_];--match_;}
        }
        position_=516;
    }
public:
    Stream(const Bytes& packed,const Bytes& header):packed_(packed),cursor_(50323){
        if(packed.size()!=50324 || header.size()!=496 || header[0x1d9])invalid();
        literals_=unsigned(header[0x1d6])*256+header[0x1d7];bits_=header[0x1d8];tokens_=be32(header,0x1da);
        if(!tokens_ || tokens_>65535)invalid();
    }
    unsigned byte(){if(!position_)refill();return ring_[--position_];}
    unsigned word(){unsigned n=0;for(unsigned i=0;i<4;++i)n=(n<<8)|byte();return n;}
};
}
ExecutableSections executable_sections(const Bytes& file){
    constexpr unsigned sizes[]={80,8,3680,9644,74068,496,50324,516};
    std::size_t p=0;auto next=[&](){unsigned n=be32(file,p);p+=4;return n;};
    if(next()!=0x3f3 || next()!=0 || next()!=8 || next()!=0 || next()!=7)invalid();
    for(auto size:sizes)if(next()!=size/4)invalid();
    std::array<Bytes,8> hunks;
    for(unsigned i=0;i<8;++i){
        const unsigned type=next(),words=next();if(words!=sizes[i]/4 || (type!=0x3e9 && type!=0x3ea && type!=0x3eb))invalid();
        hunks[i].assign(sizes[i],0);
        if(type!=0x3eb){if(p>file.size() || file.size()-p<sizes[i])invalid();std::copy_n(file.begin()+p,sizes[i],hunks[i].begin());p+=sizes[i];}
        unsigned end=next();
        if(end==0x3ec){
            while(unsigned count=next()){
                unsigned target=next();if(target>=8 || count>sizes[i]/4)invalid();
                while(count--){unsigned offset=next();if(offset>sizes[i]-4 || (offset&1))invalid();}
            }
            end=next();
        }
        if(end!=0x3f2)invalid();
    }
    if(p!=file.size())invalid();
    Stream stream(hunks[6],hunks[5]);
    for(unsigned segment=1;segment<=4;++segment){
        unsigned records=0;
        while(true){
            if(++records>65536)invalid();
            const unsigned token=stream.word();
            if(token&0x80000000)break;
            if(token&0x40000000){
                while(unsigned count=stream.word()){
                    if(count>sizes[segment]/4)invalid();
                    const unsigned source_target=stream.word();if(source_target>=7)invalid();
                    const unsigned target=source_target+1;
                    unsigned offset=0;
                    while(count--){
                        const unsigned delta=stream.word();if(delta>sizes[segment] || offset>sizes[segment]-delta)invalid();offset+=delta;
                        if(offset>sizes[segment]-4 || (offset&1))invalid();
                        unsigned value=be32(hunks[segment],offset)+0x20000+target*0x30000;
                        for(unsigned b=0;b<4;++b)hunks[segment][offset+b]=std::uint8_t(value>>(24-b*8));
                    }
                }
            }else{
                if(token>sizes[segment]/4)invalid();
                for(unsigned n=0;n<token*4;++n)hunks[segment][n]=std::uint8_t(stream.byte());
            }
        }
    }
    ExecutableSections result;for(unsigned i=0;i<4;++i)result.sections[i]=std::move(hunks[i+1]);return result;
}
}
