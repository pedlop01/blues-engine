// SPDX-License-Identifier: GPL-3.0-or-later
#include "titus/fox_amiga_rules.hpp"
#include "titus/fox_amiga_executable.hpp"
#include "titus/catalog.hpp"
#include <stdexcept>
namespace titus::fox_amiga {
namespace {
unsigned word(const Bytes& data,std::size_t offset){if(offset+2>data.size())throw std::runtime_error("Truncated Amiga rules");return unsigned(data[offset])*256+data[offset+1];}
int signed_word(unsigned n){return n<32768?int(n):int(n)-65536;}
std::string hash(const Bytes& data){return titus::sha256(std::string(data.begin(),data.end()));}
}
NativeRules native_rules(const Bytes& main_file){
    if(hash(main_file)!="18ca46dd02a3ad3623b03d425f0e0ae489b5090498882aea6d2578161010bc45")throw std::runtime_error("Unsupported Fox Amiga executable revision");
    auto unpacked=executable_sections(main_file);NativeRules result;
    for(unsigned i=0;i<4;++i)result.section_sha256[i]=hash(unpacked.sections[i]);
    const auto& data=unpacked.sections[2];const auto& code=unpacked.sections[3];
    for(unsigned i=0;i<16;++i){result.palette[i]=word(data,0x2aa+i*2);result.level_color14[i]=word(data,0x340+i*2);result.player_palette[i]=word(data,0x1dc+i*2);result.hud_palette[i]=word(data,0x2cc+i*2);}
    for(unsigned i=0;i<16;++i){
        const unsigned p=0x4e6+i*4,address=(word(data,p)<<16)|word(data,p+2);
        if(address<0xb0000 || address>=0xb0000+data.size())throw std::runtime_error("Amiga song name outside data");
        std::string name;unsigned cursor=address-0xb0000;
        while(cursor<data.size() && data[cursor] && name.size()<128)name+=char(data[cursor++]);
        if(cursor>=data.size() || data[cursor] || name.rfind("df0:",0)!=0)throw std::runtime_error("Invalid Amiga song name");
        name=name.substr(4);if(name.empty() || name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-")!=std::string::npos)throw std::runtime_error("Unsafe Amiga song name");
        result.level_songs[i]=std::move(name);
    }
    for(unsigned i=0;i<5;++i)result.effect_frequencies[i]=(word(data,0x1cb0+i*4)<<16)|word(data,0x1cb2+i*4);
    for(unsigned i=0;i<14;++i)result.motion.walk_frames[i]=word(code,0x7a8e + i*2);
    for(unsigned i=0;i<5;++i)result.motion.jump_impulses[i]=signed_word(word(code,0x7a82+i*2));
    for(unsigned i=0;i<333;++i)result.sprites[i]={word(data,0xd4a+i*4),word(data,0xd4c+i*4),signed_word(word(data,0x7ba+i*4)),signed_word(word(data,0x7bc+i*4))};
    for(unsigned i=0;i<71;++i){const auto& sprite=result.sprites[30+i];result.objects.kinds[i]={data[0x72c+i],unsigned(data[0x773+i])*16,sprite.width,sprite.height,sprite.anchor_x,sprite.anchor_y};}
    bool end=false;
    for(unsigned p=0x12da;p+2<=data.size();p+=2){unsigned n=word(data,p);if(n==0xfc18){end=true;break;}if(n<232)result.level.enemy_animation_frames[n]=true;}
    if(!end)throw std::runtime_error("Missing Amiga animation terminator");
    result.table_data=data;return result;
}
}
