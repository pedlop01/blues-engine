// SPDX-License-Identifier: GPL-3.0-or-later
// Native Amiga simulation and managed SDL2 runtime.
#include "titus/fox_amiga_game.hpp"
#include "titus/catalog.hpp"
#include "titus/process.hpp"
#include "managed.hpp"
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
std::unique_ptr<ManagedHost> managed;
Bytes read_resource(const fs::path& root,std::string name){
    std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});fs::path path;
    for(const auto& entry:fs::directory_iterator(root)){auto candidate=entry.path().filename().string();std::transform(candidate.begin(),candidate.end(),candidate.begin(),[](unsigned char c){return char(std::tolower(c));});if(candidate==name){if(!path.empty())throw std::runtime_error("Ambiguous original resource");path=entry.path();}}
    std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file || file.tellg()<0 || file.tellg()>2*1024*1024)throw std::runtime_error("Cannot read "+name);
    Bytes data(std::size_t(file.tellg()));file.seekg(0);if(!file.read(reinterpret_cast<char*>(data.data()),std::streamsize(data.size())))throw std::runtime_error("Cannot read "+name);return data;
}
std::uint32_t rgb(unsigned color){unsigned r=(color>>8)&15,g=(color>>4)&15,b=color&15;return 0xff000000|(r*17<<16)|(g*17<<8)|(b*17);}
struct Audio {
    SDL_AudioDeviceID device=0;std::unique_ptr<PatPlayer> player;std::atomic<bool> muted{false};std::atomic<std::uint64_t> samples{0},nonzero{0},clipped{0};std::atomic<unsigned> peak{0};
    static void callback(void* opaque,Uint8* data,int length){auto& self=*static_cast<Audio*>(opaque);std::fill(data,data+length,0);if(!self.muted && self.player){self.player->render_into(reinterpret_cast<std::int16_t*>(data),unsigned(length)/4);unsigned maximum=0;std::uint64_t active=0,limits=0;for(int i=0;i<length/2;++i){int value=reinterpret_cast<const std::int16_t*>(data)[i];maximum=std::max(maximum,unsigned(std::abs(value)));active+=value!=0;limits+=value==32767 || value==-32768;}self.samples.fetch_add(unsigned(length)/2,std::memory_order_relaxed);self.nonzero.fetch_add(active,std::memory_order_relaxed);self.clipped.fetch_add(limits,std::memory_order_relaxed);unsigned old=self.peak.load(std::memory_order_relaxed);while(old<maximum && !self.peak.compare_exchange_weak(old,maximum,std::memory_order_relaxed)){} }}
    void open(){SDL_AudioSpec desired{};desired.freq=44100;desired.format=AUDIO_S16SYS;desired.channels=2;desired.samples=1024;desired.callback=callback;desired.userdata=this;device=SDL_OpenAudioDevice(nullptr,0,&desired,nullptr,0);if(!device)throw std::runtime_error(SDL_GetError());}
    void song(PatSong song){SDL_LockAudioDevice(device);player=std::make_unique<PatPlayer>(std::move(song));SDL_UnlockAudioDevice(device);SDL_PauseAudioDevice(device,0);}
    void effect(std::shared_ptr<const EffectSample> sample){SDL_LockAudioDevice(device);if(player)player->play_effect(std::move(sample));SDL_UnlockAudioDevice(device);}
    void volume(unsigned music,unsigned effects){SDL_LockAudioDevice(device);if(player)player->volume(music,effects);SDL_UnlockAudioDevice(device);}
    ~Audio(){if(device)SDL_CloseAudioDevice(device);}
};
void text(RgbFrame& frame,const GameAssets& assets,int x,int y,std::string value,std::uint32_t tint=0){
    for(unsigned char c:value){if(c>='a' && c<='z')c-=32;int index=c>='A'&&c<='Z'?1+c-'A':c>='0'&&c<='9'?27+c-'0':c=='!'?37:c=='?'?38:c=='.'?39:c==','?39:c=='$'?40:c=='_'?41:-1;
        if(index>=0){const auto& glyph=assets.font.at(unsigned(index));for(unsigned py=0;py<glyph.height;++py)for(unsigned px=0;px<glyph.width;++px){int dx=x+int(px),dy=y+int(py);if(dx<0 || dy<0 || dx>=int(frame.width)||dy>=int(frame.height))continue;unsigned color=glyph.pixels[std::size_t(py)*glyph.width+px];if(color!=1 && color)frame.pixels[std::size_t(dy)*frame.width+unsigned(dx)]=tint?tint:rgb(assets.rules.hud_palette.at(color));}}
        x+=8;
    }
}
int run(const std::vector<std::string>& input_args){
    auto args=input_args;
    if(args.size()==3 && (args[1]=="--inspect-request" || args[1]=="--session-request")){
        managed=std::make_unique<ManagedHost>(args);
        if(managed->inspection()){managed->inspect([&](const std::string& name){return read_resource(managed->root(),name);});return 0;}
        args=managed->arguments();
    }
    fs::path root,capture,work,test_script;unsigned level=1,test_ms=0;bool level_argument=false;
    for(std::size_t i=1;i<args.size();i+=2){if(i+1>=args.size())throw std::runtime_error("Missing runtime argument");if(args[i]=="--datapath")root=fs::u8path(args[i+1]);else if(args[i]=="--test-script")test_script=fs::u8path(args[i+1]);else if(args[i]=="--workdir")work=fs::u8path(args[i+1]);else if(args[i]=="--level"){level=unsigned(std::stoul(args[i+1]));level_argument=true;}else if(args[i]=="--test-ms")test_ms=unsigned(std::stoul(args[i+1]));else if(args[i]=="--capture")capture=fs::u8path(args[i+1]);else throw std::runtime_error("Unknown runtime argument");}
    if(root.empty() || level<1 || level>16 || level==15 || test_ms>120000)throw std::runtime_error("Use --datapath DIRECTORY [--level 1..14/16]");
    root=titus::normalized_path(root);if(work.empty())work=titus::Paths::resolve().data/"standalone/fox-amiga";work=titus::normalized_path(work);
    auto inside=[](const fs::path& child,const fs::path& parent){auto relative=child.lexically_relative(parent);return !relative.empty() && *relative.begin()!="..";};if(inside(work,root) || inside(root,work))throw std::runtime_error("Native storage overlaps original resources");
    titus::ensure_directory(work);
    auto read=[&](const std::string& name){return read_resource(root,name);};auto assets=load_game_assets(read);
    auto ensure_level=[&](unsigned n){if(!assets.levels.at(n))assets.levels[n]=load_level_assets(n,read,assets.rules);};ensure_level(level);
    auto game=start_game(*assets.levels[level],assets.rules),previous=game;
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_EVENTS|SDL_INIT_TIMER|SDL_INIT_GAMECONTROLLER))throw std::runtime_error(SDL_GetError());
    struct Quit{~Quit(){SDL_Quit();}} quit;
    std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window(SDL_CreateWindow("Titus the Fox Amiga",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,960,600,SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI),SDL_DestroyWindow);
    if(!window)throw std::runtime_error(SDL_GetError());
    std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)> renderer(SDL_CreateRenderer(window.get(),-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC),SDL_DestroyRenderer);
    if(!renderer)renderer.reset(SDL_CreateRenderer(window.get(),-1,SDL_RENDERER_SOFTWARE));if(!renderer)throw std::runtime_error(SDL_GetError());
    std::unique_ptr<SDL_GameController,decltype(&SDL_GameControllerClose)> controller(nullptr,SDL_GameControllerClose);
    auto connect_controller=[&]{if(controller && SDL_GameControllerGetAttached(controller.get()))return;controller.reset();for(int i=0;i<SDL_NumJoysticks();++i)if(SDL_IsGameController(i)){controller.reset(SDL_GameControllerOpen(i));if(controller)break;}};connect_controller();
    Audio audio;audio.open();auto song=[&]{audio.song(pat_song(unpack(read(assets.rules.level_songs[game.level.number-1])),read));};song();
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> texture(nullptr,SDL_DestroyTexture);
    std::deque<GameState> history;bool running=true,menu=false,controls=false,smooth=true,classic_hud=true,fullscreen=false,rewinding=false;unsigned selected=0,music=80,effects=80,render_frames=0,initial_lives=3,initial_level=level,action_button=0,jump_button=1,page=0,presentation_ticks=0,intro=test_ms?0:1;double intro_elapsed=0;std::string menu_message;std::array<RgbFrame,8> thumbnails;std::array<unsigned,8> slot_levels{};
    double zoom=1,target_zoom=1,camera_x=game.camera.x,camera_y=game.camera.y,accumulator=0;
    const std::string fingerprint=managed?managed->fingerprint():"fox.amiga.full-audited.18ca46dd02a3ad36";
    const auto preferences=work/"preferences.json";
    if(titus::path_exists(preferences)){
        auto prefs=titus::parse_json(titus::read_document(preferences,8192));if(prefs.value("schema_version",0)!=1)throw std::runtime_error("Invalid native preferences");
        classic_hud=prefs.value("hud",std::string("classic"))=="classic";smooth=prefs.value("camera",std::string(prefs.value("smooth_camera",true)?"smooth":"classic"))=="smooth";if(!level_argument)initial_level=prefs.value("start_level",level);fullscreen=prefs.value("fullscreen",false);music=prefs.value("music_volume",80u);effects=prefs.value("effects_volume",80u);target_zoom=prefs.value("zoom",1.);
        game.invulnerable=prefs.value("invulnerable",false);game.infinite_lives=prefs.value("infinite_lives",false);initial_lives=game.lives=prefs.value("lives",3u);action_button=prefs.value("action_button",0u);jump_button=prefs.value("jump_button",1u);
        if(action_button>14 || jump_button>14 || action_button==jump_button || music>100 || effects>100 || game.lives>99 || target_zoom<1./3 || target_zoom>1.)throw std::runtime_error("Invalid native preference values");if(prefs.contains("view_step") && prefs.at("view_step").get<unsigned>()>8)throw std::runtime_error("Invalid native view step");if(prefs.contains("view_step"))target_zoom=1./(1.+prefs.at("view_step").get<unsigned>()*.25);zoom=target_zoom;
        SDL_SetWindowFullscreen(window.get(),fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0);
    }
    if(managed){const auto& settings=managed->settings();if(settings.contains("video"))fullscreen=settings["video"].value("fullscreen",fullscreen);
        if(settings.contains("audio")){music=settings["audio"].value("music_volume",music);effects=settings["audio"].value("effects_volume",effects);}
        if(settings.contains("input")){action_button=settings["input"].value("action_button",action_button);jump_button=settings["input"].value("jump_button",jump_button);}
        if(settings.contains("fox")){const auto& fox=settings["fox"];classic_hud=fox.value("hud",std::string(classic_hud?"classic":"fixed"))=="classic";smooth=fox.value("camera",std::string(smooth?"smooth":"classic"))=="smooth";game.invulnerable=fox.value("invulnerable",game.invulnerable);game.infinite_lives=fox.value("infinite_lives",game.infinite_lives);initial_lives=game.lives=fox.value("lives",initial_lives);initial_level=fox.value("start_level",initial_level);target_zoom=1./(1.+fox.value("view_step",0u)*.25);zoom=target_zoom;}
        SDL_SetWindowFullscreen(window.get(),fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0);
    }
    if(intro)audio.song(pat_song(unpack(read("pres2.pat")),read));
    auto slot_path=[&](unsigned slot){return work/("slot-"+std::to_string(slot+1)+".json");};
    auto refresh_slots=[&]{for(unsigned i=0;i<8;++i){slot_levels[i]=0;thumbnails[i]={};try{if(titus::path_exists(slot_path(i))){auto raw=titus::read_document(slot_path(i),2*1024*1024);slot_levels[i]=saved_game_level(Bytes(raw.begin(),raw.end()));}
        auto path=work/("slot-"+std::to_string(i+1)+".bmp");if(!titus::path_exists(path) || !titus::path_exists(slot_path(i)))continue;auto document=titus::parse_json(titus::read_document(slot_path(i),2*1024*1024));if(!document.contains("preview_sha256") || document["preview_sha256"]!=titus::file_sha256(path))continue;std::unique_ptr<SDL_Surface,decltype(&SDL_FreeSurface)> loaded(SDL_LoadBMP(path.u8string().c_str()),SDL_FreeSurface);if(!loaded || loaded->w!=320 || loaded->h!=176)continue;
        std::unique_ptr<SDL_Surface,decltype(&SDL_FreeSurface)> converted(SDL_ConvertSurfaceFormat(loaded.get(),SDL_PIXELFORMAT_ARGB8888,0),SDL_FreeSurface);if(!converted)continue;
        auto& image=thumbnails[i];image={320,176,std::vector<std::uint32_t>(320*176)};for(unsigned y=0;y<176;++y)std::copy_n(reinterpret_cast<std::uint32_t*>(static_cast<Uint8*>(converted->pixels)+y*converted->pitch),320,image.pixels.data()+y*320);
    }catch(const std::exception&){slot_levels[i]=99;}}};refresh_slots();
    titus::Json scripted=titus::Json::array();std::size_t next_script=0;bool test_rewind=false;
    if(!test_script.empty()){if(!test_ms)throw std::runtime_error("Scripts require bounded test runtime");scripted=titus::parse_json(titus::read_document(test_script,65536));if(!scripted.is_array() || scripted.size()>256)throw std::runtime_error("Invalid runtime test script");}
    auto started=std::chrono::steady_clock::now(),last=started;RgbFrame last_frame;
    auto switch_level=[&](unsigned n){ensure_level(n);unsigned lives=game.lives,credits=game.bonus_credits;bool inv=game.invulnerable,inf=game.infinite_lives;game=start_game(*assets.levels[n],assets.rules,lives);game.invulnerable=inv;game.infinite_lives=inf;game.bonus_credits=credits;previous=game;history.clear();camera_x=game.camera.x;camera_y=game.camera.y;accumulator=0;presentation_ticks=0;song();};
    if(initial_level!=game.level.number)switch_level(initial_level);
    if(managed && managed->action().at("type")=="load_slot"){
        unsigned slot=managed->action().at("slot");auto raw=titus::read_document(slot_path(slot-1),2*1024*1024);Bytes bytes(raw.begin(),raw.end());unsigned n=saved_game_level(bytes);ensure_level(n);game=restore_game(bytes,*assets.levels[n],assets.rules,fingerprint);previous=game;camera_x=game.camera.x;camera_y=game.camera.y;intro=0;song();
    }
    unsigned right_pixels=0,left_pixels=0,jump_pixels=0,rewind_steps=0;bool pause_seen=false;
    if(managed)managed->ready(intro?"title":"gameplay");
    while(running){
        if(managed && managed->poll())running=false;
        auto now=std::chrono::steady_clock::now();double elapsed=std::min(.1,std::chrono::duration<double>(now-last).count());last=now;
        if(test_ms && now-started>=std::chrono::milliseconds(test_ms))running=false;
        while(next_script<scripted.size() && scripted[next_script].at("ms").get<unsigned>()<=unsigned(std::chrono::duration_cast<std::chrono::milliseconds>(now-started).count())){
            const auto& step=scripted[next_script++];if(step.contains("rewind"))test_rewind=step.at("rewind");
            if(step.contains("key")){SDL_Event input{};input.type=SDL_KEYDOWN;input.key.keysym.sym=SDL_GetKeyFromName(step.at("key").get<std::string>().c_str());SDL_PushEvent(&input);}
        }
        SDL_Event event;while(SDL_PollEvent(&event)){
            if(event.type==SDL_QUIT)running=false;
            if(event.type==SDL_CONTROLLERDEVICEADDED || event.type==SDL_CONTROLLERDEVICEREMOVED)connect_controller();
            if(event.type==SDL_CONTROLLERBUTTONDOWN){SDL_Keycode mapped=event.cbutton.button==SDL_CONTROLLER_BUTTON_START?SDLK_ESCAPE:event.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_UP?SDLK_UP:event.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_DOWN?SDLK_DOWN:event.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_LEFT?SDLK_LEFT:event.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_RIGHT?SDLK_RIGHT:event.cbutton.button==SDL_CONTROLLER_BUTTON_A?SDLK_RETURN:SDLK_UNKNOWN;if(mapped){event.type=SDL_KEYDOWN;event.key.keysym.sym=mapped;event.key.repeat=0;}}

            if(event.type!=SDL_KEYDOWN || event.key.repeat)continue;
            auto key=event.key.keysym.sym;
            if(!menu && game.message_level && (key==SDLK_RETURN || key==SDLK_SPACE)){game.message_level=0;continue;}
            if(intro && (key==SDLK_RETURN || key==SDLK_SPACE)){if(intro<3){++intro;intro_elapsed=0;}else{intro=0;intro_elapsed=0;song();}continue;}
            if(key==SDLK_ESCAPE || key==SDLK_F1 || key==SDLK_p){if(controls)controls=false;else if(menu && page){page=0;selected=0;}else menu=!menu;accumulator=0;menu_message.clear();}
            if(key==SDLK_F11){fullscreen=!fullscreen;SDL_SetWindowFullscreen(window.get(),fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0);}
            if(key==SDLK_MINUS || key==SDLK_KP_MINUS){unsigned step=unsigned(std::clamp(int(std::lround((1./target_zoom-1.)*4))+1,0,8));target_zoom=1./(1.+step*.25);}
            if(key==SDLK_EQUALS || key==SDLK_PLUS || key==SDLK_KP_PLUS){unsigned step=unsigned(std::clamp(int(std::lround((1./target_zoom-1.)*4))-1,0,8));target_zoom=1./(1.+step*.25);}
            if(menu){
                unsigned count=page==0?6:page==1?12:8;
                if(key==SDLK_UP)selected=(selected+count-(page>=2?2:1))%count;
                if(key==SDLK_DOWN)selected=(selected+(page>=2?2:1))%count;
                if(page>=2 && (key==SDLK_LEFT || key==SDLK_RIGHT))selected=(selected+(key==SDLK_LEFT?7:1))%8;
                bool activate=key==SDLK_RETURN || (page==1 && (key==SDLK_LEFT || key==SDLK_RIGHT));int direction=key==SDLK_LEFT?-1:1;
                if(activate && page==0){switch(selected){case 0:menu=false;break;case 1:page=1;selected=0;break;case 2:page=2;selected=0;refresh_slots();break;case 3:page=3;selected=0;refresh_slots();break;case 4:controls=!controls;break;case 5:running=false;break;}}
                else if(activate && page==1)switch(selected){
                case 0:smooth=!smooth;break;case 1:classic_hud=!classic_hud;break;case 2:game.invulnerable=!game.invulnerable;break;case 3:game.infinite_lives=!game.infinite_lives;break;
                case 4:initial_lives=game.lives=unsigned(std::clamp(int(game.lives)+direction,0,99));break;
                case 5:{unsigned n=game.level.number;if(direction>0)n=n==16?1:next_level(n);else n=n==1?16:n==16?14:n-1;switch_level(n);initial_level=n;break;}
                case 6:music=unsigned(std::clamp(int(music)+direction*10,0,100));break;case 7:effects=unsigned(std::clamp(int(effects)+direction*10,0,100));break;
                case 8:case 9:{const unsigned buttons[]={0,1,2,3,7,8,10};auto& button=selected==8?action_button:jump_button;unsigned other=selected==8?jump_button:action_button;auto position=std::find(std::begin(buttons),std::end(buttons),button);unsigned index=position==std::end(buttons)?0:unsigned(position-std::begin(buttons));do{index=(index+7+direction)%7;}while(buttons[index]==other);button=buttons[index];break;}
                case 10:switch_level(game.level.number);menu=false;break;case 11:page=0;selected=0;break;
                }
                else if(activate && page>=2){try{
                    if(page==2){auto bytes=save_game(game,fingerprint);auto document=titus::Json::parse(bytes.begin(),bytes.end());
                        auto image=game_frame(game,assets,game.camera);std::unique_ptr<SDL_Surface,decltype(&SDL_FreeSurface)> surface(SDL_CreateRGBSurfaceWithFormatFrom(image.pixels.data(),320,176,32,320*4,SDL_PIXELFORMAT_ARGB8888),SDL_FreeSurface);
                        if(!surface || SDL_SaveBMP(surface.get(),(work/("slot-"+std::to_string(selected+1)+".bmp")).u8string().c_str()))throw std::runtime_error(SDL_GetError());document["preview_sha256"]=titus::file_sha256(work/("slot-"+std::to_string(selected+1)+".bmp"));titus::write_document(slot_path(selected),document);refresh_slots();menu_message="PARTIDA GUARDADA";
                    }else if(slot_levels[selected] && slot_levels[selected]!=99){auto raw=titus::read_document(slot_path(selected),2*1024*1024);Bytes bytes(raw.begin(),raw.end());unsigned n=saved_game_level(bytes);ensure_level(n);auto restored=restore_game(bytes,*assets.levels[n],assets.rules,fingerprint);game=std::move(restored);previous=game;history.clear();camera_x=game.camera.x;camera_y=game.camera.y;accumulator=0;song();menu=false;page=0;selected=0;}
                }catch(const std::exception& e){menu_message=page==2?"ERROR AL GUARDAR":"ERROR AL CARGAR";std::cerr<<e.what()<<'\n';}}
            }else if(game.phase!=GamePhase::playing && key==SDLK_RETURN){if(game.phase==GamePhase::level_complete){finish_level_bonuses(game);switch_level(next_level(game.level.number));}else{game.lives=initial_lives;game.bonus_credits=0;switch_level(1);}}
        }
        auto button=[&](unsigned id){return controller && SDL_GameControllerGetButton(controller.get(),SDL_GameControllerButton(id));};
        auto axis=[&](SDL_GameControllerAxis id){return controller?SDL_GameControllerGetAxis(controller.get(),id):0;};
        const Uint8* keys=SDL_GetKeyboardState(nullptr);rewinding=!menu && (keys[SDL_SCANCODE_R] || test_rewind || button(SDL_CONTROLLER_BUTTON_BACK)) && !history.empty();audio.muted=menu||rewinding;audio.volume(music,effects);
        if(intro && !menu){intro_elapsed+=elapsed;if((intro==1 || intro==2) && intro_elapsed>=2){++intro;intro_elapsed=0;}}
        accumulator+=elapsed;
        while(accumulator>=.02 && !menu && !intro){
            accumulator-=.02;previous=game;game.view_step=unsigned(std::clamp(int(std::lround((1./target_zoom-1.)*4)),0,8));
            if(rewinding){if(!history.empty()){++rewind_steps;game=std::move(history.back());history.pop_back();if(game.phase!=previous.phase){presentation_ticks=0;song();}}}
            else if(game.phase!=GamePhase::playing){++presentation_ticks;game_step(game,{},false,assets.rules);if(game.actors.sound_event && game.actors.sound_event<=5)audio.effect(assets.effects[game.actors.sound_event-1]);}
            else if(game.phase==GamePhase::playing){history.push_back(game);if(history.size()>500)history.pop_front();BodyInput input{bool(keys[SDL_SCANCODE_LEFT] || button(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || axis(SDL_CONTROLLER_AXIS_LEFTX)<-12000),bool(keys[SDL_SCANCODE_RIGHT] || button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || axis(SDL_CONTROLLER_AXIS_LEFTX)>12000),bool(keys[SDL_SCANCODE_UP] || button(SDL_CONTROLLER_BUTTON_DPAD_UP) || button(jump_button) || axis(SDL_CONTROLLER_AXIS_LEFTY)<-12000),bool(keys[SDL_SCANCODE_DOWN] || button(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || axis(SDL_CONTROLLER_AXIS_LEFTY)>12000)};
                if(test_ms){unsigned t=game.frame;input.right=t<60;input.left=t>=60&&t<100;input.up=t>=100&&t<125;}
                game_step(game,input,keys[SDL_SCANCODE_SPACE] || button(action_button),assets.rules);int dx=game.player.feet.x-previous.player.feet.x,dy=game.player.feet.y-previous.player.feet.y;if(input.right && dx>0 && dx<32)right_pixels+=unsigned(dx);if(input.left && dx<0 && dx>-32)left_pixels+=unsigned(-dx);if(input.up && dy<0 && dy>-32)jump_pixels+=unsigned(-dy);if(std::abs(game.player.feet.x-previous.player.feet.x)>128 || std::abs(game.player.feet.y-previous.player.feet.y)>128){camera_x=game.camera.x;camera_y=game.camera.y;previous=game;}if(game.phase!=previous.phase){presentation_ticks=0;if(game.phase==GamePhase::game_over)audio.song(pat_song(unpack(read("over2.pat")),read));}if(game.actors.sound_event && game.actors.sound_event<=5)audio.effect(assets.effects[game.actors.sound_event-1]);
            }
        }
        if(menu)pause_seen=true;
        if(menu || intro)accumulator=0;
        double alpha=std::min(1.,accumulator/.02),blend=1-std::exp(-elapsed*14);zoom+=((game.gate_flags?1.:std::max(target_zoom,std::min(1.,176./(game.level.height_tiles*16))))-zoom)*blend;
        double target_x=previous.camera.x+(game.camera.x-previous.camera.x)*alpha,target_y=previous.camera.y+(game.camera.y-previous.camera.y)*alpha;
        if(smooth){camera_x+=(target_x-camera_x)*blend;camera_y+=(target_y-camera_y)*blend;}else{camera_x=game.camera.x;camera_y=game.camera.y;}
        unsigned width=unsigned(std::ceil(320/zoom)),height=unsigned(std::ceil(176/zoom));
        double focus=std::clamp(zoom-1.,0.,1.),player_x=previous.player.feet.x+(game.player.feet.x-previous.player.feet.x)*alpha,player_y=previous.player.feet.y+(game.player.feet.y-previous.player.feet.y)*alpha;
        double view_camera_x=std::clamp((camera_x+160)*(1-focus)+player_x*focus-160/zoom,0.,std::max(0.,4096-320/zoom));
        double view_camera_y=std::clamp((camera_y+88)*(1-focus)+(player_y-16)*focus-88/zoom,0.,std::max(0.,game.level.height_tiles*16.-176/zoom));
        Point camera{int(std::floor(view_camera_x)),int(std::floor(view_camera_y))};
        auto rendered=game;if(smooth && game.player.state!=7 && previous.phase==game.phase){rendered.player.feet.x=int(std::lround(previous.player.feet.x+(game.player.feet.x-previous.player.feet.x)*alpha));rendered.player.feet.y=int(std::lround(previous.player.feet.y+(game.player.feet.y-previous.player.feet.y)*alpha));}
        auto image=game_frame(rendered,assets,camera,width,height);
        if(rewinding){for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){auto& pixel=image.pixels[std::size_t(y)*width+x];unsigned stripe=(y+render_frames*7)%43;if(stripe<3)pixel=((pixel&0xfefefe)>>1)|0xff606060;if(y%3==0)pixel=(pixel&0xfefefe)>>1|0xff000000;}text(image,assets,12,12,"REWIND",0xffeeeeee);}
        // A fixed 320x200 presentation preserves the gameplay/HUD framing at
        // every zoom. Only the world viewport changes its map coverage.
        RgbFrame screen{320,200,std::vector<std::uint32_t>(320*200,0xff000000)};
        for(unsigned y=0;y<176;++y)for(unsigned x=0;x<320;++x)screen.pixels[y*320+x]=image.pixels[std::size_t(y*height/176)*width+x*width/320];
        if(classic_hud){
            for(unsigned segment=0;segment<8;++segment){unsigned energy=game.player.energy>segment*2?std::min(2u,game.player.energy-segment*2):0;const auto& glyph=assets.hud.at(11+energy);
                for(unsigned y=0;y<glyph.height;++y)for(unsigned x=0;x<glyph.width;++x)if(glyph.pixels[std::size_t(y)*glyph.width+x])screen.pixels[(179+y)*320+112+segment*16+x]=rgb(assets.rules.hud_palette[std::min(15u,2+y*13/glyph.height)]);
            }
        }else{text(screen,assets,8,181,"VIDAS "+std::to_string(game.lives));text(screen,assets,208,181,"ENERGIA");for(unsigned y=194;y<198;++y)for(unsigned x=0;x<game.player.energy*6;++x)screen.pixels[y*320+208+x]=0xffeedd66;}
        if(game.phase==GamePhase::ending || game.phase==GamePhase::game_over){auto end=ending_frame(game,assets,presentation_ticks);for(unsigned y=0;y<176;++y)std::copy_n(end.pixels.data()+y*320,320,screen.pixels.data()+y*320);}
        if(intro){const auto& picture=intro==1?assets.logo:intro==2?assets.presentation:assets.title;for(unsigned y=0;y<200;++y)for(unsigned x=0;x<320;++x)screen.pixels[y*320+x]=0xff000000|picture.palette.at(picture.image.pixels[y*320+x]);if(intro==3)text(screen,assets,48,182,"PULSA ENTER PARA JUGAR",0xffffdf63);}
        if(!menu && game.message_level){for(unsigned y=52;y<124;++y)for(unsigned x=24;x<296;++x)screen.pixels[y*320+x]=(x==24||x==295||y==52||y==123)?0xffccb66a:0xff101828;text(screen,assets,48,64,"NIVEL "+std::to_string(game.message_level),0xffffdf63);text(screen,assets,48,84,"CODIGO "+assets.rules.level_codes.at(game.message_level-1),0xffeeeeee);text(screen,assets,48,104,"ENTER PARA CONTINUAR",0xffeeeeee);}
        if(menu){for(unsigned y=12;y<174;++y)for(unsigned x=16;x<304;++x)screen.pixels[y*320+x]=(x==16||x==303||y==12||y==173)?0xffccb66a:0xff101828;
            text(screen,assets,40,18,"TITUS THE FOX AMIGA");
            std::vector<std::string> rows=page==0?std::vector<std::string>{"CONTINUAR","OPCIONES","GUARDAR PARTIDA","CARGAR PARTIDA","CONTROLES","VOLVER A BIBLIOTECA"}:
                std::vector<std::string>{std::string("CAMARA ")+(smooth?"SUAVE":"ORIGINAL"),std::string("HUD ")+(classic_hud?"ORIGINAL":"FIJO"),std::string("INVULNERABLE ")+(game.invulnerable?"SI":"NO"),std::string("VIDAS INFINITAS ")+(game.infinite_lives?"SI":"NO"),"VIDAS "+std::to_string(game.lives),"NIVEL "+std::to_string(game.level.number),"MUSICA "+std::to_string(music),"EFECTOS "+std::to_string(effects),"BOTON ACCION "+std::to_string(action_button),"BOTON SALTO "+std::to_string(jump_button),"REINICIAR NIVEL","VOLVER"};
            if(page<2)for(unsigned i=0;i<rows.size();++i)text(screen,assets,32,(page==1?30:36)+int(i)*(page==1?11:18),rows[i],i==selected?0xffffdf63:0xffd0d7e0);
            else{for(unsigned i=0;i<8;++i){int left=24+int(i%2)*140,top=32+int(i/2)*34;for(int y=0;y<32;++y)for(int x=0;x<136;++x){auto& pixel=screen.pixels[unsigned(top+y)*320+unsigned(left+x)];pixel=(x==0||x==135||y==0||y==31)?(i==selected?0xffffdf63:0xff48566a):0xff101828;}
                const auto& thumbnail=thumbnails[i];if(!thumbnail.pixels.empty())for(unsigned y=0;y<28;++y)for(unsigned x=0;x<56;++x)screen.pixels[unsigned(top+2+int(y))*320+unsigned(left+2+int(x))]=thumbnail.pixels[std::size_t(y*thumbnail.height/28)*thumbnail.width+x*thumbnail.width/56];
                text(screen,assets,left+64,top+3,std::to_string(i+1),i==selected?0xffffdf63:0xffd0d7e0);text(screen,assets,left+64,top+17,slot_levels[i]==99?"ERROR":slot_levels[i]?"N "+std::to_string(slot_levels[i]):"VACIO",0xffd0d7e0);
            }text(screen,assets,24,176,menu_message.empty()?(page==2?"ENTER GUARDAR   ESC VOLVER":"ENTER CARGAR    ESC VOLVER"):menu_message,0xffffdf63);}
            if(controls){std::fill(screen.pixels.begin(),screen.pixels.end(),0xff101828);text(screen,assets,16,24,"FLECHAS MOVER Y SALTAR");text(screen,assets,16,44,"ABAJO ESPACIO RECOGER");text(screen,assets,16,64,"ESPACIO LANZAR");text(screen,assets,16,84,"R REBOBINAR DIEZ SEGUNDOS");text(screen,assets,16,104,"MAS MENOS ENCUADRE");text(screen,assets,16,124,"F11 PANTALLA COMPLETA");text(screen,assets,16,144,"ESC VOLVER");}
        }else if(game.phase==GamePhase::level_complete){text(screen,assets,32,60,"NIVEL COMPLETADO");text(screen,assets,32,80,"BONUS "+std::to_string(game.bonus_credits));text(screen,assets,32,100,"VIDAS "+std::to_string(game.lives));text(screen,assets,32,120,"PULSA ENTER");}else if(!intro && game.phase!=GamePhase::playing && presentation_ticks>280)text(screen,assets,64,185,"PULSA ENTER",0xffffdf63);
        RgbFrame high;const RgbFrame* display=&screen;
        if(smooth && !menu && !intro && !game.message_level && game.phase==GamePhase::playing){
            int output_width,output_height;SDL_GetRendererOutputSize(renderer.get(),&output_width,&output_height);unsigned resolution=unsigned(std::clamp(std::min(output_width/320,output_height/200),1,4));
            if(resolution>1){unsigned scaled_width=320*resolution,world_height=176*resolution;
                high=smooth_game_frame(game,previous,assets,view_camera_x,view_camera_y,zoom*resolution,scaled_width,world_height,alpha);
                high.height=200*resolution;high.pixels.resize(std::size_t(scaled_width)*high.height);
                for(unsigned y=world_height;y<high.height;++y)for(unsigned x=0;x<scaled_width;++x)high.pixels[std::size_t(y)*scaled_width+x]=screen.pixels[std::size_t(y/resolution)*320+x/resolution];
                if(rewinding)for(unsigned y=0;y<world_height;++y)if(((y/resolution+render_frames*7)%43)<3)for(unsigned x=0;x<scaled_width;++x)high.pixels[std::size_t(y)*scaled_width+x]=((high.pixels[std::size_t(y)*scaled_width+x]&0xfefefe)>>1)|0xff606060;
                display=&high;
            }
        }
        int texture_width=0,texture_height=0;if(texture)SDL_QueryTexture(texture.get(),nullptr,nullptr,&texture_width,&texture_height);
        if(texture_width!=int(display->width) || texture_height!=int(display->height)){texture.reset(SDL_CreateTexture(renderer.get(),SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,int(display->width),int(display->height)));if(!texture)throw std::runtime_error(SDL_GetError());}
        SDL_UpdateTexture(texture.get(),nullptr,display->pixels.data(),int(display->width)*4);SDL_SetRenderDrawColor(renderer.get(),0,0,0,255);SDL_RenderClear(renderer.get());int rw,rh;SDL_GetRendererOutputSize(renderer.get(),&rw,&rh);double scale=std::min(rw/320.,rh/200.);SDL_Rect dst{int((rw-320*scale)/2),int((rh-200*scale)/2),int(320*scale),int(200*scale)};SDL_RenderCopy(renderer.get(),texture.get(),nullptr,&dst);SDL_RenderPresent(renderer.get());++render_frames;last_frame=std::move(screen);SDL_Delay(1);
    }
    titus::write_document(preferences,{{"schema_version",1},{"smooth_camera",smooth},{"hud",classic_hud?"classic":"fixed"},{"camera",smooth?"smooth":"classic"},{"view_step",unsigned(std::clamp(int(std::lround((1./target_zoom-1.)*4)),0,8))},{"start_level",initial_level},{"fullscreen",fullscreen},{"music_volume",music},{"effects_volume",effects},{"zoom",target_zoom},{"invulnerable",game.invulnerable},{"infinite_lives",game.infinite_lives},{"lives",initial_lives},{"action_button",action_button},{"jump_button",jump_button}});
    if(!capture.empty() && !last_frame.pixels.empty()){SDL_Surface* surface=SDL_CreateRGBSurfaceWithFormatFrom(last_frame.pixels.data(),320,200,32,320*4,SDL_PIXELFORMAT_ARGB8888);if(!surface || SDL_SaveBMP(surface,capture.u8string().c_str()))throw std::runtime_error(SDL_GetError());SDL_FreeSurface(surface);}
    if(managed && test_ms)managed->observe({{"frames",render_frames},{"ticks",game.frame},{"level",game.level.number},{"right",right_pixels},{"left",left_pixels},{"jump",jump_pixels},{"pause",pause_seen},{"rewind_steps",rewind_steps},{"samples",audio.samples.load()},{"nonzero",audio.nonzero.load()},{"clipped",audio.clipped.load()},{"peak",audio.peak.load()}});
    if(managed)managed->finish();
    std::cout<<"{\"frames\":"<<render_frames<<",\"ticks\":"<<game.frame<<",\"level\":"<<game.level.number<<",\"audio_active\":true}\n";return 0;
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv)try{std::vector<std::string> args;for(int i=0;i<argc;++i)args.push_back(fs::path(argv[i]).u8string());return run(args);}catch(const std::exception& e){if(managed)try{managed->fail(e);}catch(...){}std::cerr<<e.what()<<'\n';return 1;}
#else
int main(int argc,char** argv)try{return run(std::vector<std::string>(argv,argv+argc));}catch(const std::exception& e){if(managed)try{managed->fail(e);}catch(...){}std::cerr<<e.what()<<'\n';return 1;}
#endif
