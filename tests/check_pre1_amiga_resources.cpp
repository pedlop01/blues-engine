// SPDX-License-Identifier: GPL-3.0-or-later
#include "titus/pre1_amiga.hpp"
#include "titus/pre1_amiga_level.hpp"
#include <iostream>
#include <stdexcept>
using namespace titus::pre1_amiga;
namespace {int count=0;void check(bool value){++count;if(!value)throw std::runtime_error("Unexpected resource result");}template<class F>void rejects(F f){++count;try{f();}catch(const std::runtime_error&){return;}throw std::runtime_error("Malformed resource accepted");}}
int main(){try{
    // One 8x1 masked sprite: opaque black must remain distinct from transparency.
    Bytes mat={0,1,0,8,0,1,0,0,0x40,0,0,0,0xc0};auto decoded=sprites(mat);check(decoded.size()==1 && decoded[0].opaque[0]==1 && decoded[0].colors[0]==0 && decoded[0].colors[1]==1 && decoded[0].opaque[2]==0);
    auto packed=mat;packed[7]=6;packed.insert(packed.begin()+8,4);check(sprites(packed)[0].opaque==decoded[0].opaque);
    Bytes mono={0,1,0,8,0,1,0,0,0x80};check(sprites(mono,true)[0].opaque==Bytes({1,0,0,0,0,0,0,0}));
    for(std::size_t n=0;n<mat.size();++n)rejects([&]{sprites(Bytes(mat.begin(),mat.begin()+n));});
    mat.push_back(0);rejects([&]{sprites(mat);});packed[8]=127;rejects([&]{sprites(packed);});
    rejects([]{lzss({0,0,0,8,0});});rejects([]{lzss({255,255,255,255,0});});
    // Literal 'A' followed by a distance-one overlapping run of four A's.
    std::string bits="0010000011100000000";Bytes compressed={0,0,0,5};for(std::size_t i=0;i<bits.size();i+=8){unsigned v=0;for(unsigned b=0;b<8;++b)v=(v<<1)|(i+b<bits.size() && bits[i+b]=='1');compressed.push_back(v);}check(lzss(compressed)==Bytes(5,'A'));
    check(lzss({0,0,0,5,255,224})==Bytes(5,0));
    Bytes iff;auto tag=[&](const char* t){iff.insert(iff.end(),t,t+4);};auto put=[&](unsigned v,unsigned n){for(unsigned i=n;i>0;--i)iff.push_back(std::uint8_t(v>>(8*(i-1))));};
    tag("FORM");put(42,4);tag("8SVX");tag("VHDR");put(20,4);put(1,4);put(0,4);put(0,4);put(8000,2);put(1,1);put(0,1);put(65536,4);tag("BODY");put(1,4);put(128,1);put(0,1);
    auto sample=sample_8svx(iff);check(sample.rate==8000 && sample.one_shot==1 && sample.pcm==Bytes({128}));
    for(std::size_t n=0;n<iff.size();++n)rejects([&]{sample_8svx(Bytes(iff.begin(),iff.begin()+n));});
    auto compressed_iff=iff;compressed_iff[35]=1;rejects([&]{sample_8svx(compressed_iff);});
    rejects([]{sample_8svx(Bytes(12));});
    CollisionRules terrain;terrain[0].push_back({0,1,3});auto map=collision({TilePlacement{0,0,0,0}},terrain);check(!empty(map,1,1) && empty(map,4,1) && !empty(map,-1,1) && empty(map,0,-1));
    auto negative=collision({TilePlacement{-2,0,0,0}},terrain);check(!empty(negative,0,1) && empty(negative,1,1));
    rejects([]{rules(Bytes{});});rejects([]{collision_rules(Bytes(959));});rejects([]{object_screens({0,40,0,255});});
    auto records=object_screens({0,2,0,10,0,20,0,0,0,255});check(records.size()==2 && records[0][0].type==2 && records[0][0].words==std::vector<int>({10,20}));
    Surface level;level.number=3;level.screens[0]={{0,0,28,0},{0,0,29,0}};auto stage=scene(level,0,28,terrain);check(stage.commands.size()==10 && stage.commands[8].a==30);
    std::cout<<"OK: "<<count<<" Prehistorik Amiga resource checks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
