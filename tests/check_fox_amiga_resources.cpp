// SPDX-License-Identifier: GPL-3.0-or-later
#include "titus/fox_amiga.hpp"
#include "titus/fox_amiga_audio.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace titus::fox_amiga;
void require(bool b){if(!b)throw std::runtime_error("Assertion failed");}
template<class F> void rejects(F f){bool failed=false;try{f();}catch(const std::runtime_error&){failed=true;}require(failed);}
Bytes lzw(unsigned size,const std::vector<unsigned>& codes) {
    Bytes d={2,0,std::uint8_t(size),std::uint8_t(size>>8),0,0,12};
    std::uint64_t bits=0;unsigned count=0;
    for(auto c:codes) {bits|=std::uint64_t(c)<<count;count+=9;
        while(count>=8){d.push_back(std::uint8_t(bits));bits>>=8;count-=8;}}
    if(count)d.push_back(std::uint8_t(bits));return d;
}
int main() try {
    // Independent hand-encoded streams: special LZW next-code case and signed
    // Huffman leaves, mandatory end symbol, overflow and malformed tree edges.
    require(unpack(lzw(4,{65,257,66}))==Bytes({'A','A','A','B'}));
    rejects([]{unpack(lzw(4,{65,300,66}));});
    rejects([]{auto d=lzw(4,{65,257,66});d.pop_back();unpack(d);});
    Bytes h={1,0,1,0,0,0,0x76,0xff,0,0,0,1,0,0xbe,0xff,0xff,0xfe,2};
    require(unpack(h)==Bytes({'A'}));
    rejects([&]{auto d=h;d.back()=0;unpack(d);});
    rejects([&]{auto d=h;d[13]=2;d[14]=0;unpack(d);});
    rejects([]{unpack({2,0,0xff,0xff,0xff,0xff,12,0});});
    Bytes s={0,1,0,8,0,1,0,0,0x80,0x40,0x20,0x10};
    auto all=sprites(s);require(all.size()==1 && all[0].pixels==Bytes({1,2,4,8,0,0,0,0}));
    rejects([&]{auto d=s;d.pop_back();sprites(d);});
    rejects([&]{auto d=s;d.push_back(0);sprites(d);});
    auto ts=tiles(Bytes(32768));auto m=map_view(Bytes(256),ts,0,0,32,16);
    require(m.pixels.size()==512);
    rejects([&]{map_view(Bytes(256),ts,4090,0,16,16);});
    rejects([&]{map_view(Bytes(255),ts,0,0,16,16);});
    Bytes raw(48+32000);raw[1]=0xf0;raw[48]=0x80;
    auto p=picture(raw,4,false);require(p.image.pixels[0]==1 && p.palette[1]==0xff0000);
    rejects([&]{raw.pop_back();picture(raw,4,false);});
    Bytes ehb(96+48000);ehb[1]=0xf0;ehb[96]=0x80;ehb[96+40000]=0x80;
    auto half=picture(ehb,6,false);require(half.image.pixels[0]==33 && half.palette[33]==0x770000);
    // A 4-plane repeat picture: 4000 flag bytes compressed with PackBits,
    // all flags repeat the initially zero literal. No commercial fixture.
    Bytes flags;
    for(unsigned left=4000;left;) {unsigned run=left>128?128:left;flags.push_back(std::uint8_t(257-run));flags.push_back(255);left-=run;}
    Bytes repeat(48,0);
    auto be32=[&](unsigned n){repeat.push_back(std::uint8_t(n>>24));repeat.push_back(std::uint8_t(n>>16));repeat.push_back(std::uint8_t(n>>8));repeat.push_back(std::uint8_t(n));};
    be32(unsigned(flags.size()));repeat.insert(repeat.end(),flags.begin(),flags.end());be32(0);
    auto black=picture(repeat,4,true);require(black.image.pixels==Bytes(64000,0));
    rejects([&]{auto d=repeat;d.pop_back();picture(d,4,true);});
    rejects([&]{auto d=repeat;d[51]=0;picture(d,4,true);});
    Bytes poil(3060,0);
    auto le=[&](unsigned at,unsigned v){poil.at(at)=std::uint8_t(v);poil.at(at+1)=std::uint8_t(v>>8);};
    for(unsigned i=0;i<40;++i){le(768+i*6,65535);le(770+i*6,65535);le(772+i*6,65535);}
    for(unsigned i=0;i<50;++i)le(1018+i*26,65535);
    for(unsigned i=0;i<100;++i)le(2316+i*4,65535);
    for(unsigned i=0;i<20;++i)le(2716+i*7,65535);
    for(unsigned i=0;i<10;++i)le(2860+i*20,65535);
    le(1012,1);le(3058,16);
    poil[4]=4;poil[256+5]=8;poil[512+6]=4;poil[512+253]=128;
    le(768,0x2024);le(770,20);le(772,16);
    le(1014,5);le(1016,12);le(1018,0x8001);le(1020,1);le(1022,65534);
    poil[2314]=77;poil[2315]=253;le(2316,2);
    Bytes raw_map(256,0);raw_map[0]=0x11;raw_map[1]=4;
    LevelRules rules;rules.enemy_animation_frames.fill(true);
    auto logical=initialize_level(1,raw_map,poil,rules);
    require(logical.map[0]==0x91 && logical.map[2]==77);
    require(logical.horizontal[4]==0x82 && logical.floor[5]==0x41 && logical.floor[6]==0x47);
    require(logical.objects[0].kind==7 && logical.objects[0].position.y==15);
    require(logical.enemies.size()==1 && logical.enemies[0].kind==2 && logical.enemies[0].source_speed_x==-2);
    require(logical.spawn.y==0 && logical.finish.y==0 && logical.dirty_rows[0]==1);
    require(logical.animation[253]==2 && logical.animation[256+255]==0 && logical.animation[512+253]==4);
    require(collision_at(logical,16,0).horizontal==0x82 && collision_at(logical,-1,0).outside);
    require(!collision_at(logical,4095,15).outside && collision_at(logical,4096,0).outside);
    require(collision_at(logical,0,16).outside);
    auto doorway=logical;doorway.gates[0]={258,0x0304,0x0506,1,true};
    auto transfer=gate_transition(doorway,{40,15});require(bool(transfer));
    require(transfer->destination.x==96 && transfer->destination.y==78 && transfer->camera.x==64 && transfer->camera.y==64 && transfer->flags==1);
    require(!gate_transition(doorway,{24,15}) && !gate_transition(doorway,{-1,15}));
    require(exit_ready(logical,{0,0},false,0) && !exit_ready(logical,{0,0},true,0));
    auto concert=logical;concert.number=11;
    require(!exit_ready(concert,{0,0},false,0) && exit_ready(concert,{0,0},false,27));
    require(next_level(13)==14 && next_level(14)==16 && next_level(16)==0);
    rejects([]{next_level(15);});
    rules.enemy_animation_frames[1]=false;
    require(initialize_level(1,raw_map,poil,rules).enemies[0].kind==0);
    rules.enemy_animation_frames[1]=true;
    rejects([&]{auto d=poil;d.pop_back();initialize_level(1,raw_map,d,rules);});
    rejects([&]{auto d=poil;d[512+253]=0;d[512+255]=128;initialize_level(1,raw_map,d,rules);});
    rejects([&]{auto d=poil;d[1011]=16;initialize_level(1,raw_map,d,rules);});
    rejects([&]{initialize_level(15,raw_map,poil,rules);});
    Level plane;plane.number=1;plane.height_tiles=8;plane.map.assign(2048,0);plane.floor[1]=1;
    for(unsigned x=0;x<256;++x)plane.map[7*256+x]=1;
    MotionRules motion;motion.walk_frames.fill(3);motion.jump_impulses.fill(-16);
    Body walker;walker.feet={100,111};
    for(unsigned i=0;i<5;++i)walker=terrain_step(plane,walker,{false,true,false},motion);
    require(walker.feet.x==110 && walker.vx==64 && walker.feet.y==111);
    for(unsigned i=0;i<6;++i)walker=terrain_step(plane,walker,{},motion);
    require(walker.feet.x==117 && walker.vx==0 && walker.state==0);
    Body jumper;jumper.feet={100,111};
    jumper=terrain_step(plane,jumper,{false,false,true},motion);require(jumper.state==2 && jumper.feet.y==111);
    jumper=terrain_step(plane,jumper,{false,false,true},motion);require(jumper.vy==-16 && jumper.feet.y==110);
    jumper=terrain_step(plane,jumper,{},motion);require(jumper.state==3 && jumper.feet.y==109);
    jumper=terrain_step(plane,jumper,{},motion);require(jumper.vy==0 && jumper.feet.y==109);
    for(unsigned i=0;i<20;++i)jumper=terrain_step(plane,jumper,{},motion);
    require(jumper.feet.y==111 && jumper.vy==0 && jumper.state==0);
    plane.floor[1]=0x85;Body ice;ice.feet={100,111};ice.ice_shift=3;
    ice=terrain_step(plane,ice,{false,true,false},motion);
    for(unsigned i=0;i<7;++i)ice=terrain_step(plane,ice,{false,true,false},motion);
    require(ice.vx==14 && ice.feet.x==100);
    ice=terrain_step(plane,ice,{false,true,false},motion);require(ice.vx==16 && ice.feet.x==101);
    rejects([&]{auto b=ice;b.state=6;terrain_step(plane,b,{},motion);});
    rejects([&]{auto b=ice;b.feet.x=0;terrain_step(plane,b,{},motion);});
    Bytes pat(1624,0);pat[0]='l';pat[1]='a';pat[2]='g';pat[20]='t';pat[21]='o';pat[22]='n';pat[23]='e';
    pat[43]=8;pat[45]=64;pat[49]=1;pat[470]=1;
    pat[600]=1;pat[601]=172;pat[602]=0x10;
    pat[618]=12; // Volume-zero command, including its zero parameter.
    auto sample=[](const std::string& name){require(name=="tone");return Bytes(14,127);};
    auto song=pat_song(pat,sample);require(song.instruments[0].samples.size()==16 && song.instruments[0].samples[0]==0);
    PatPlayer sequencer(song);for(unsigned n=0;n<10;++n)sequencer.tick();
    require(sequencer.channels()[0].period==428 && sequencer.channels()[0].volume==64);
    for(unsigned n=0;n<11;++n)sequencer.tick();require(sequencer.channels()[0].volume==0);
    auto shared=pat;shared[604]=1;shared[605]=172;shared[606]=0x10;shared[618]=0x1c;shared[619]=34;
    PatPlayer shared_volume(pat_song(shared,sample));for(unsigned n=0;n<20;++n)shared_volume.tick();
    require(shared_volume.channels()[0].volume==34 && shared_volume.channels()[1].volume==34);
    PatPlayer whole(song),chunked(song);auto pcm=whole.render(44100);std::vector<std::int16_t> chunks;
    for(unsigned n=0;n<44;++n){auto part=chunked.render(997);chunks.insert(chunks.end(),part.begin(),part.end());}
    auto part=chunked.render(232);chunks.insert(chunks.end(),part.begin(),part.end());
    require(pcm==chunks && std::any_of(pcm.begin(),pcm.end(),[](auto v){return v!=0;}));
    rejects([&]{chunked.render(1,48000);});
    rejects([&]{auto d=pat;d.pop_back();pat_song(d,sample);});
    rejects([&]{auto d=pat;d[20]='.';pat_song(d,sample);});
    rejects([&]{auto d=pat;d[470]=129;pat_song(d,sample);});
    rejects([&]{auto d=pat;d[49]=16;pat_song(d,sample);});
    rejects([&]{pat_song(pat,[](const auto&){return Bytes(1);});});
    std::cout<<"Fox Amiga resource boundaries passed\n";return 0;
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
