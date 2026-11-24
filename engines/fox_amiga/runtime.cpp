// SPDX-License-Identifier: GPL-3.0-or-later
// Native development runtime; catalog registration follows managed validation.
#include "titus/fox_amiga_game.hpp"
#include "titus/catalog.hpp"
#include <SDL.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
using namespace titus::fox_amiga;
namespace fs=std::filesystem;
namespace {
Bytes read_resource(const fs::path& root,std::string name){
    std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});fs::path path;
    for(const auto& entry:fs::directory_iterator(root)){auto candidate=entry.path().filename().string();std::transform(candidate.begin(),candidate.end(),candidate.begin(),[](unsigned char c){return char(std::tolower(c));});if(candidate==name){if(!path.empty())throw std::runtime_error("Ambiguous original resource");path=entry.path();}}
    std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file || file.tellg()<0 || file.tellg()>2*1024*1024)throw std::runtime_error("Cannot read "+name);
    Bytes data(std::size_t(file.tellg()));file.seekg(0);if(!file.read(reinterpret_cast<char*>(data.data()),std::streamsize(data.size())))throw std::runtime_error("Cannot read "+name);return data;
}
std::uint32_t rgb(unsigned color){unsigned r=(color>>8)&15,g=(color>>4)&15,b=color&15;return 0xff000000|(r*17<<16)|(g*17<<8)|(b*17);}
struct Audio {
    SDL_AudioDeviceID device=0;std::unique_ptr<PatPlayer> player;std::atomic<bool> muted{false};
    static void callback(void* opaque,Uint8* data,int length){auto& self=*static_cast<Audio*>(opaque);std::fill(data,data+length,0);if(!self.muted && self.player)self.player->render_into(reinterpret_cast<std::int16_t*>(data),unsigned(length)/4);}
    void open(){SDL_AudioSpec desired{};desired.freq=44100;desired.format=AUDIO_S16SYS;desired.channels=2;desired.samples=1024;desired.callback=callback;desired.userdata=this;device=SDL_OpenAudioDevice(nullptr,0,&desired,nullptr,0);if(!device)throw std::runtime_error(SDL_GetError());}
    void song(PatSong song){SDL_LockAudioDevice(device);player=std::make_unique<PatPlayer>(std::move(song));SDL_UnlockAudioDevice(device);SDL_PauseAudioDevice(device,0);}
    void effect(std::shared_ptr<const EffectSample> sample){SDL_LockAudioDevice(device);if(player)player->play_effect(std::move(sample));SDL_UnlockAudioDevice(device);}
    void volume(unsigned music,unsigned effects){SDL_LockAudioDevice(device);if(player)player->volume(music,effects);SDL_UnlockAudioDevice(device);}
    ~Audio(){if(device)SDL_CloseAudioDevice(device);}
};
void text(RgbFrame& frame,const GameAssets& assets,int x,int y,std::string value,std::uint32_t tint=0){
    for(unsigned char c:value){if(c>='a' && c<='z')c-=32;int index=c>='A'&&c<='Z'?c-'A':c>='0'&&c<='9'?26+c-'0':c=='!'?36:c=='?'?37:c=='.'?38:c==','?39:c=='$'?40:c=='_'?41:-1;
        if(index>=0){const auto& glyph=assets.font.at(unsigned(index));for(unsigned py=0;py<glyph.height;++py)for(unsigned px=0;px<glyph.width;++px){int dx=x+int(px),dy=y+int(py);if(dx<0 || dy<0 || dx>=int(frame.width)||dy>=int(frame.height))continue;unsigned color=glyph.pixels[std::size_t(py)*glyph.width+px];if(color!=1 && color)frame.pixels[std::size_t(dy)*frame.width+unsigned(dx)]=tint?tint:rgb(assets.rules.hud_palette.at(color));}}
        x+=8;
    }
}
int run(const std::vector<std::string>& args){
    fs::path root,capture;unsigned level=1,test_ms=0;
    for(std::size_t i=1;i<args.size();i+=2){if(i+1>=args.size())throw std::runtime_error("Missing runtime argument");if(args[i]=="--datapath")root=fs::u8path(args[i+1]);else if(args[i]=="--level")level=unsigned(std::stoul(args[i+1]));else if(args[i]=="--test-ms")test_ms=unsigned(std::stoul(args[i+1]));else if(args[i]=="--capture")capture=fs::u8path(args[i+1]);else throw std::runtime_error("Unknown runtime argument");}
    if(root.empty() || level<1 || level>16 || level==15 || test_ms>120000)throw std::runtime_error("Use --datapath DIRECTORY [--level 1..14/16]");
    auto read=[&](const std::string& name){return read_resource(root,name);};auto assets=load_game_assets(read);
    auto ensure_level=[&](unsigned n){if(!assets.levels.at(n))assets.levels[n]=load_level_assets(n,read,assets.rules);};ensure_level(level);
    auto game=start_game(*assets.levels[level],assets.rules),previous=game;
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_EVENTS|SDL_INIT_TIMER))throw std::runtime_error(SDL_GetError());
    struct Quit{~Quit(){SDL_Quit();}} quit;
    std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window(SDL_CreateWindow("Titus the Fox Amiga",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,960,600,SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI),SDL_DestroyWindow);
    if(!window)throw std::runtime_error(SDL_GetError());
    std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)> renderer(SDL_CreateRenderer(window.get(),-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC),SDL_DestroyRenderer);
    if(!renderer)renderer.reset(SDL_CreateRenderer(window.get(),-1,SDL_RENDERER_SOFTWARE));if(!renderer)throw std::runtime_error(SDL_GetError());
    Audio audio;audio.open();auto song=[&]{audio.song(pat_song(unpack(read(assets.rules.level_songs[game.level.number-1])),read));};song();
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> texture(nullptr,SDL_DestroyTexture);
    std::deque<GameState> history;bool running=true,menu=false,controls=false,smooth=true,fullscreen=false,rewinding=false;unsigned selected=0,music=80,effects=80,render_frames=0;
    double zoom=1,target_zoom=1,camera_x=game.camera.x,camera_y=game.camera.y,accumulator=0;
    auto started=std::chrono::steady_clock::now(),last=started;RgbFrame last_frame;
    auto switch_level=[&](unsigned n){ensure_level(n);unsigned lives=game.lives;bool inv=game.invulnerable,inf=game.infinite_lives;game=start_game(*assets.levels[n],assets.rules,lives);game.invulnerable=inv;game.infinite_lives=inf;previous=game;history.clear();camera_x=game.camera.x;camera_y=game.camera.y;accumulator=0;song();};
    while(running){
        auto now=std::chrono::steady_clock::now();double elapsed=std::min(.1,std::chrono::duration<double>(now-last).count());last=now;
        if(test_ms && now-started>=std::chrono::milliseconds(test_ms))running=false;
        SDL_Event event;while(SDL_PollEvent(&event)){
            if(event.type==SDL_QUIT)running=false;
            if(event.type!=SDL_KEYDOWN || event.key.repeat)continue;
            auto key=event.key.keysym.sym;
            if(key==SDLK_ESCAPE || key==SDLK_F1){menu=!menu;controls=false;accumulator=0;}
            if(key==SDLK_F11){fullscreen=!fullscreen;SDL_SetWindowFullscreen(window.get(),fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0);}
            if(key==SDLK_MINUS || key==SDLK_KP_MINUS)target_zoom=std::max(.5,target_zoom-.125);
            if(key==SDLK_EQUALS || key==SDLK_PLUS || key==SDLK_KP_PLUS)target_zoom=std::min(2.,target_zoom+.125);
            if(menu){
                if(key==SDLK_UP)selected=(selected+10)%11;if(key==SDLK_DOWN)selected=(selected+1)%11;
                bool activate=key==SDLK_RETURN || key==SDLK_LEFT || key==SDLK_RIGHT;int direction=key==SDLK_LEFT?-1:1;
                if(activate)switch(selected){
                case 0:menu=false;break;case 1:smooth=!smooth;break;case 2:game.invulnerable=!game.invulnerable;break;
                case 3:game.infinite_lives=!game.infinite_lives;break;case 4:game.lives=unsigned(std::clamp(int(game.lives)+direction,0,99));break;
                case 5:{unsigned n=game.level.number;if(direction>0)n=n==16?1:next_level(n);else n=n==1?16:n==16?14:n-1;switch_level(n);break;}
                case 6:music=unsigned(std::clamp(int(music)+direction*10,0,100));break;case 7:effects=unsigned(std::clamp(int(effects)+direction*10,0,100));break;
                case 8:controls=!controls;break;case 9:switch_level(game.level.number);menu=false;break;case 10:running=false;break;
                }
            }else if(game.phase!=GamePhase::playing && key==SDLK_RETURN){if(game.phase==GamePhase::level_complete)switch_level(next_level(game.level.number));else switch_level(1);}
        }
        const Uint8* keys=SDL_GetKeyboardState(nullptr);rewinding=!menu && keys[SDL_SCANCODE_R] && !history.empty();audio.muted=menu||rewinding;audio.volume(music,effects);
        accumulator+=elapsed;
        while(accumulator>=.02 && !menu){
            accumulator-=.02;previous=game;
            if(rewinding){if(!history.empty()){game=std::move(history.back());history.pop_back();}}
            else if(game.phase==GamePhase::playing){history.push_back(game);if(history.size()>500)history.pop_front();BodyInput input{bool(keys[SDL_SCANCODE_LEFT]),bool(keys[SDL_SCANCODE_RIGHT]),bool(keys[SDL_SCANCODE_UP]),bool(keys[SDL_SCANCODE_DOWN])};
                if(test_ms){unsigned t=game.frame;input.right=t<60;input.left=t>=60&&t<100;input.up=t>=100&&t<125;}
                game_step(game,input,keys[SDL_SCANCODE_SPACE],assets.rules);if(game.actors.sound_event && game.actors.sound_event<=5)audio.effect(assets.effects[game.actors.sound_event-1]);
            }
        }
        if(menu)accumulator=0;
        double alpha=std::min(1.,accumulator/.02),blend=1-std::exp(-elapsed*14);zoom+=(target_zoom-zoom)*blend;
        double target_x=previous.camera.x+(game.camera.x-previous.camera.x)*alpha,target_y=previous.camera.y+(game.camera.y-previous.camera.y)*alpha;
        if(smooth){camera_x+=(target_x-camera_x)*blend;camera_y+=(target_y-camera_y)*blend;}else{camera_x=game.camera.x;camera_y=game.camera.y;}
        unsigned width=unsigned(std::ceil(320/zoom)),height=unsigned(std::ceil(176/zoom));Point camera{int(std::floor(camera_x+160-int(width)/2)),int(std::floor(camera_y+88-int(height)/2))};
        auto rendered=game;if(smooth && game.player.state!=7 && previous.phase==game.phase){rendered.player.feet.x=int(std::lround(previous.player.feet.x+(game.player.feet.x-previous.player.feet.x)*alpha));rendered.player.feet.y=int(std::lround(previous.player.feet.y+(game.player.feet.y-previous.player.feet.y)*alpha));}
        auto image=game_frame(rendered,assets,camera,width,height);
        if(rewinding){for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){auto& pixel=image.pixels[std::size_t(y)*width+x];unsigned stripe=(y+render_frames*7)%43;if(stripe<3)pixel=((pixel&0xfefefe)>>1)|0xff606060;if(y%3==0)pixel=(pixel&0xfefefe)>>1|0xff000000;}text(image,assets,12,12,"REWIND",0xffeeeeee);}
        // A fixed 320x200 presentation preserves the gameplay/HUD framing at
        // every zoom. Only the world viewport changes its map coverage.
        RgbFrame screen{320,200,std::vector<std::uint32_t>(320*200,0xff000000)};
        for(unsigned y=0;y<176;++y)for(unsigned x=0;x<320;++x)screen.pixels[y*320+x]=image.pixels[std::size_t(y*height/176)*width+x*width/320];
        text(screen,assets,8,181,"LIVES "+std::to_string(game.lives));text(screen,assets,208,181,"ENERGY");
        for(unsigned y=194;y<198;++y)for(unsigned x=0;x<game.player.energy*6;++x)screen.pixels[y*320+208+x]=0xffeedd66;
        if(menu){for(unsigned y=12;y<174;++y)for(unsigned x=16;x<304;++x)screen.pixels[y*320+x]=(x==16||x==303||y==12||y==173)?0xffccb66a:0xff101828;
            text(screen,assets,40,18,"TITUS THE FOX AMIGA");
            std::vector<std::string> rows={"CONTINUE",std::string("CAMERA ")+(smooth?"SMOOTH":"ORIGINAL"),std::string("INVULNERABLE ")+(game.invulnerable?"YES":"NO"),std::string("INFINITE LIVES ")+(game.infinite_lives?"YES":"NO"),"LIVES "+std::to_string(game.lives),"LEVEL "+std::to_string(game.level.number),"MUSIC "+std::to_string(music),"EFFECTS "+std::to_string(effects),"CONTROLS","RESTART LEVEL","RETURN"};
            for(unsigned i=0;i<rows.size();++i)text(screen,assets,32,34+int(i)*12,rows[i],i==selected?0xffffdf63:0xffd0d7e0);
            if(controls){std::fill(screen.pixels.begin(),screen.pixels.end(),0xff101828);text(screen,assets,16,24,"ARROWS MOVE AND JUMP");text(screen,assets,16,44,"DOWN SPACE PICK UP");text(screen,assets,16,64,"SPACE THROW");text(screen,assets,16,84,"R REWIND TEN SECONDS");text(screen,assets,16,104,"PLUS MINUS ZOOM");text(screen,assets,16,124,"F11 FULLSCREEN");text(screen,assets,16,144,"ESC RETURN");}
        }else if(game.phase!=GamePhase::playing){text(screen,assets,32,60,game.phase==GamePhase::ending?"THE END":game.phase==GamePhase::game_over?"GAME OVER":"LEVEL COMPLETE");text(screen,assets,32,80,"PRESS ENTER");}
        if(!texture){texture.reset(SDL_CreateTexture(renderer.get(),SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,320,200));if(!texture)throw std::runtime_error(SDL_GetError());}
        SDL_UpdateTexture(texture.get(),nullptr,screen.pixels.data(),320*4);SDL_SetRenderDrawColor(renderer.get(),0,0,0,255);SDL_RenderClear(renderer.get());int rw,rh;SDL_GetRendererOutputSize(renderer.get(),&rw,&rh);double scale=std::min(rw/320.,rh/200.);SDL_Rect dst{int((rw-320*scale)/2),int((rh-200*scale)/2),int(320*scale),int(200*scale)};SDL_RenderCopy(renderer.get(),texture.get(),nullptr,&dst);SDL_RenderPresent(renderer.get());++render_frames;last_frame=std::move(screen);SDL_Delay(1);
    }
    if(!capture.empty() && !last_frame.pixels.empty()){SDL_Surface* surface=SDL_CreateRGBSurfaceWithFormatFrom(last_frame.pixels.data(),320,200,32,320*4,SDL_PIXELFORMAT_ARGB8888);if(!surface || SDL_SaveBMP(surface,capture.string().c_str()))throw std::runtime_error(SDL_GetError());SDL_FreeSurface(surface);}
    std::cout<<"{\"frames\":"<<render_frames<<",\"ticks\":"<<game.frame<<",\"level\":"<<game.level.number<<",\"audio_active\":true}\n";return 0;
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv)try{std::vector<std::string> args;for(int i=0;i<argc;++i)args.push_back(fs::path(argv[i]).u8string());return run(args);}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
#else
int main(int argc,char** argv)try{return run(std::vector<std::string>(argv,argv+argc));}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
#endif
