#define SDL_MAIN_HANDLED
#include "library.hpp"
#include "titus/session.hpp"
#include <SDL.h>
#include <SDL_ttf.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <future>
#include <iostream>
#include <map>
#include <set>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif
using namespace titus;
namespace {
constexpr int W=1100,H=720;
const SDL_Color ink{232,237,244,255},muted{155,170,190,255},aqua{56,217,207,255},gold{244,194,110,255};
SDL_Color color(unsigned n,unsigned char alpha=255){return {static_cast<Uint8>(n>>16),static_cast<Uint8>(n>>8),static_cast<Uint8>(n),alpha};}
void pop_utf8(std::string& text){if(text.empty())return;size_t i=text.size()-1;while(i&&((static_cast<unsigned char>(text[i])&192)==128))--i;text.resize(i);}
struct Widget {std::string id;SDL_Rect rect;std::function<void()> action;};
struct Text {SDL_Texture* texture;int w,h;};
class App {
    Paths paths;
    Library library;
    SDL_Window* window=nullptr;
    SDL_Renderer* renderer=nullptr;
    SDL_GameController* controller=nullptr;
    SDL_Joystick* virtual_pad=nullptr;
    int virtual_device=-1;
    SDL_JoystickID virtual_id=-1;
    std::string smoke_focus_game,smoke_pad_game;
    std::array<bool,SDL_CONTROLLER_BUTTON_MAX> blocked_buttons{};
    std::map<int,TTF_Font*> fonts;
    std::map<std::string,Text> text_cache;
    std::vector<Widget> widgets;
    std::string focus="add",notice,modal,focus_before_modal,browse_error,path_text,relocate_id;
    fs::path browse_dir;
    std::vector<fs::path> entries;
    int browse_index=0,browse_top=0;
    bool recursive=true,path_edit=false,path_select_all=false,running=true,focused=true,pending_quit=false;
    std::array<bool,SDL_NUM_SCANCODES> blocked{};
    std::array<bool,SDL_CONTROLLER_AXIS_MAX> neutral{};
    Uint32 last_axis=0,last_save=0;
    std::future<Json> job;
    std::future<std::unique_ptr<Session>> starting_session;
    std::unique_ptr<Session> session;
    fs::path package_root;
    bool session_hidden=false,cancel_start=false;
    bool test_play=false,test_family=false,test_fox=false,test_pre1=false;
    int play_test_stage=0;
    Uint32 play_test_started=0;
    bool smoke=false;
    fs::path screenshot,workflow_source;
    size_t workflow_count=0;
    int smoke_step=0,smoke_checks=0;
    std::string smoke_initial;
    void check(bool ok,const char* message){if(!ok)throw Error("E_UI_TEST",message);++smoke_checks;}
    TTF_Font* font(int size){auto it=fonts.find(size);if(it!=fonts.end())return it->second;
        char* base=SDL_GetBasePath();if(!base)throw Error("E_FONT",SDL_GetError());
        auto file=fs::u8path(base)/"assets/fonts/DejaVuSans.ttf";SDL_free(base);
        auto f=TTF_OpenFont(file.u8string().c_str(),size);if(!f)throw Error("E_FONT",TTF_GetError());fonts[size]=f;return f;}
    void fill(SDL_Rect rect,SDL_Color c){SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,c.a);SDL_RenderFillRect(renderer,&rect);}
    void panel(SDL_Rect rect,SDL_Color c,int radius=12){fill({rect.x+radius,rect.y,rect.w-2*radius,rect.h},c);
        fill({rect.x,rect.y+radius,rect.w,rect.h-2*radius},c);
        for(int y=0;y<radius;++y){int dx=static_cast<int>(radius-std::sqrt(double(radius*radius-(radius-y)*(radius-y))));
            fill({rect.x+dx,rect.y+y,rect.w-2*dx,1},c);fill({rect.x+dx,rect.y+rect.h-1-y,rect.w-2*dx,1},c);}}
    void outline(SDL_Rect rect,SDL_Color c){SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,c.a);SDL_RenderDrawRect(renderer,&rect);}
    std::string fit(std::string value,int size,int maxwidth){int width=0;TTF_SizeUTF8(font(size),value.c_str(),&width,nullptr);if(width<=maxwidth)return value;
        while(!value.empty()){pop_utf8(value);TTF_SizeUTF8(font(size),(value+"…").c_str(),&width,nullptr);if(width<=maxwidth)return value+"…";}return "…";}
    void text(const std::string& value,int x,int y,int size=18,SDL_Color c=ink,int maxwidth=0,bool wrap=false){if(value.empty())return;
        auto shown=maxwidth&&!wrap?fit(value,size,maxwidth):value;
        auto key=std::to_string(size)+":"+std::to_string(c.r)+":"+std::to_string(c.g)+":"+std::to_string(c.b)+":"+std::to_string(wrap?maxwidth:0)+":"+shown;
        auto found=text_cache.find(key);
        if(found==text_cache.end()){
            SDL_Surface* surface=wrap?TTF_RenderUTF8_Blended_Wrapped(font(size),shown.c_str(),c,maxwidth):TTF_RenderUTF8_Blended(font(size),shown.c_str(),c);
            if(!surface)throw Error("E_TEXT",TTF_GetError());Text rendered{SDL_CreateTextureFromSurface(renderer,surface),surface->w,surface->h};SDL_FreeSurface(surface);
            if(!rendered.texture)throw Error("E_RENDER",SDL_GetError());found=text_cache.emplace(key,rendered).first;
        }
        SDL_Rect dest{x,y,found->second.w,found->second.h};SDL_RenderCopy(renderer,found->second.texture,nullptr,&dest);
    }
    void widget(const std::string& id,SDL_Rect rect,std::function<void()> action){widgets.push_back({id,rect,std::move(action)});}
    void button(const std::string& id,const std::string& label,SDL_Rect rect,std::function<void()> action,bool enabled=true,bool primary=false){
        bool selected=focus==id;auto bg=enabled?(primary?color(0x38d9cf):color(selected?0x34435a:0x253246)):color(0x1b2432);
        panel(rect,bg,8);if(selected&&enabled)outline({rect.x-2,rect.y-2,rect.w+4,rect.h+4},aqua);
        text(label,rect.x+14,rect.y+(rect.h-22)/2,16,enabled?(primary?color(0x102429):ink):color(0x68788c),rect.w-28);
        if(enabled)widget(id,rect,std::move(action));
    }
    void set_modal(const std::string& page){focus_before_modal=focus;modal=page;focus="cancel";}
    void close_modal(){modal.clear();path_edit=false;path_select_all=false;SDL_StopTextInput();focus=focus_before_modal.empty()?"add":focus_before_modal;}
    void action(const std::function<Json(CatalogStore&)>& fn){
        if(job.valid())return;path_edit=false;SDL_StopTextInput();modal="busy";notice.clear();auto storage=paths;
        job=std::async(std::launch::async,[storage,fn](){CatalogStore store(storage);return fn(store);});
    }
    void finish_job(){if(!job.valid()||job.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
        try{library.catalog=job.get();library.healthy=true;library.error.clear();library.reconcile();notice="Biblioteca actualizada.";}
        catch(const std::exception& e){notice=std::string("No se pudo completar la operación: ")+friendly_error(e);}
        modal.clear();focus=library.selected()?"game:"+std::string(library.info().id):"add";if(pending_quit)running=false;
    }
    void reset_controller_input(){
        for(int i=0;i<SDL_CONTROLLER_AXIS_MAX;++i)neutral[i]=controller&&std::abs(int(SDL_GameControllerGetAxis(controller,static_cast<SDL_GameControllerAxis>(i))))<12000;
        for(int i=0;i<SDL_CONTROLLER_BUTTON_MAX;++i)blocked_buttons[i]=controller&&SDL_GameControllerGetButton(controller,static_cast<SDL_GameControllerButton>(i));
    }
    LegacySources legacy_sources(){
        auto config=environment_value("BLUES_CONFIG_FILE"),saves=environment_value("BLUES_SAVE_DIR");fs::path folder;
        if(!config||!saves){char* root=SDL_GetPrefPath("pedlop01","blues-engine");if(root)folder=fs::u8path(root);SDL_free(root);}
        return {config?fs::u8path(*config):(folder.empty()?fs::path():folder/"preferences.ini"),
                saves?fs::u8path(*saves):(folder.empty()?fs::path():folder/"saves")};
    }
    void begin_play(){
        if(!library.can_play()||session||starting_session.valid()||job.valid())return;
        library.persist();SessionOptions options;if(library.selected()->at("game_id")=="blues")options.legacy=legacy_sources();options.primary_source=*library.source();if(test_play)options.test_ms=500;
        auto storage=paths;auto root=package_root;auto catalog=library.catalog;auto id=library.selected()->at("id").get<std::string>();
        modal="session";focus="cancel-session";notice.clear();cancel_start=false;
        starting_session=std::async(std::launch::async,[storage,root,catalog,id,options](){return std::make_unique<Session>(storage,root,catalog,id,options);});
    }
    void return_to_library(){cancel_start=true;try{if(session&&!session->finished())session->request_return();}catch(const std::exception& e){notice=friendly_error(e);}}
    void poll_session(){
        if(starting_session.valid()&&starting_session.wait_for(std::chrono::seconds(0))==std::future_status::ready){
            try{session=starting_session.get();if(cancel_start)session->request_return();}
            catch(const std::exception& e){notice=friendly_error(e);modal.clear();if(pending_quit)running=false;}
        }
        if(!session)return;session->poll();
        if(session->ready()&&!session_hidden&&!session->finished()){
            SDL_StopTextInput();if(controller){SDL_GameControllerClose(controller);controller=nullptr;}neutral.fill(false);SDL_HideWindow(window);session_hidden=true;
        }
        if(session->state==Session::State::Playing&&cancel_start){cancel_start=false;pending_quit=false;}
        if(!session->finished())return;
        bool success=session->state==Session::State::Done;
        notice=success?session->message():session->error();session.reset();modal.clear();
        if(session_hidden)SDL_ShowWindow(window);SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_ShowCursor(SDL_ENABLE);session_hidden=false;
        focused=true;int count=0;auto keys=SDL_GetKeyboardState(&count);for(int i=0;i<count&&i<SDL_NUM_SCANCODES;++i)blocked[i]=keys[i]!=0;
        for(int i=0;i<SDL_NumJoysticks();++i)open_controller(i);reset_controller_input();focus=library.selected()?"game:"+std::string(library.info().id):"add";
        if(test_play){if(!success)throw Error("E_UI_SESSION_TEST",notice);++play_test_stage;}
        if(pending_quit)running=false;
    }
    void open_controller(int device){if(session_hidden||controller||!SDL_IsGameController(device))return;controller=SDL_GameControllerOpen(device);reset_controller_input();}
    void toggle_fullscreen(){bool target=(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN_DESKTOP)==0;
        if(SDL_SetWindowFullscreen(window,target?SDL_WINDOW_FULLSCREEN_DESKTOP:0)!=0){notice=std::string("No se pudo cambiar la pantalla: ")+SDL_GetError();return;}
        library.preferences["fullscreen"]=target;library.dirty=true;}
    fs::path home(){
#ifdef _WIN32
        wchar_t value[32768];auto n=GetEnvironmentVariableW(L"USERPROFILE",value,32768);if(n&&n<32768)return fs::path(value);
        return fs::path(L"C:\\");
#else
        auto value=std::getenv("HOME");return value?fs::u8path(value):fs::path("/");
#endif
    }
    void read_browser(fs::path dir){
        try{
            entries.clear();browse_index=-1;browse_top=0;browse_error.clear();
            if(dir.empty()){
#ifdef _WIN32
                auto drives=GetLogicalDrives();for(int i=0;i<26;++i)if(drives&(1u<<i))entries.push_back(fs::path(std::wstring(1,wchar_t(L'A'+i))+L":\\"));
                browse_dir.clear();path_text.clear();return;
#else
                dir="/";
#endif
            }
            dir=normalized_path(dir);
            if(fs::is_regular_file(dir)){auto parent=dir.parent_path();read_browser(parent);for(size_t i=0;i<entries.size();++i)if(entries[i]==dir)browse_index=static_cast<int>(i);return;}
            if(!fs::is_directory(dir))throw Error("E_BROWSER","La carpeta no está disponible.");
            std::vector<fs::path> found;
            for(const auto& entry:fs::directory_iterator(dir)){
                if(found.size()>=10000)throw Error("E_BROWSER","Demasiadas entradas; escribe una ruta más concreta.");
                std::error_code error;if(entry.is_symlink(error))continue;
                auto ext=entry.path().extension().u8string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return c>='A'&&c<='Z'?c+32:c;});
                if(entry.is_directory(error)||(entry.is_regular_file(error)&&ext==".adf"))found.push_back(entry.path());
            }
            std::sort(found.begin(),found.end(),[](const fs::path& a,const fs::path& b){return a.filename().u8string()<b.filename().u8string();});
            browse_dir=dir;entries=std::move(found);path_text=dir.u8string();library.preferences["browse_path"]=path_text;library.dirty=true;
        }catch(const std::exception& e){browse_error=e.what();path_text=dir.u8string();}
    }
    void browse(bool relocate){
        if(!library.healthy)return;auto i=library.selected();relocate_id=relocate&&i?i->at("id").get<std::string>():"";
        set_modal("browser");focus="files";path_edit=false;
        fs::path start=fs::u8path(library.preferences["browse_path"].get<std::string>());
        if(relocate&&library.source()&&library.source()->at("location").at("platform")==platform())start=fs::u8path(library.source()->at("location").at("path").get<std::string>());
        std::error_code error;if(start.empty()||!fs::exists(start,error))start=home();read_browser(start);
    }
    void browser_enter(){if(entries.empty()||browse_index<0||browse_index>=static_cast<int>(entries.size()))return;
        auto entry=entries[browse_index];if(fs::is_directory(entry))read_browser(entry);else use_browser(entry);}
    void use_browser(fs::path source={}){
        if(path_edit)source=fs::u8path(path_text);else if(source.empty())source=browse_dir;if(source.empty()||(!path_edit&&!browse_error.empty()))return;
        auto id=relocate_id;auto revision=library.catalog.at("revision").get<long long>();bool recurse=recursive;
        action([source,id,revision,recurse](CatalogStore& store){return id.empty()?store.add(source,recurse,revision):store.relocate(id,source,revision);});
    }
    void move_focus(int delta){if(widgets.empty())return;int index=-1;for(size_t i=0;i<widgets.size();++i)if(widgets[i].id==focus)index=static_cast<int>(i);
        int n=static_cast<int>(widgets.size());focus=widgets[(index+delta+n)%n].id;
        if(focus=="path"){path_edit=true;path_select_all=true;SDL_StartTextInput();}else{path_edit=false;SDL_StopTextInput();}}
    void activate(){for(const auto& w:widgets)if(w.id==focus){auto fn=w.action;fn();return;}}
    void back(){if(modal=="force-session"){modal="session";focus="cancel-session";return;}if(modal=="session"){return_to_library();return;}if(modal=="busy")return;if(!modal.empty()){close_modal();return;}set_modal("quit");}
    void navigate(int dx,int dy){
        if(modal=="session"){move_focus(dy?dy:dx);return;}if(modal=="busy")return;
        if(modal=="browser"&&focus=="files"){
            if(dy&&!entries.empty()){
                int target=browse_index+dy;
                if(target<0)focus="parent";
                else if(target>=static_cast<int>(entries.size()))focus=relocate_id.empty()?"recursive":"use";
                else browse_index=target;
            }
            if(dx>0)browser_enter();if(dx<0&&!browse_dir.empty())read_browser(browse_dir.parent_path());return;
        }
        if(modal.empty()&&focus.rfind("game:",0)==0){
            if(dy){library.select_game(dy);focus="game:"+std::string(library.info().id);}
            if(dx>0)focus="edition";return;
        }
        if(modal.empty()&&focus=="edition"&&dx){library.select_installation(dx);return;}
        if(modal.empty()&&focus=="source"&&dx){library.select_source(dx);return;}
        move_focus(dy?dy:dx);
    }
    void key(const SDL_KeyboardEvent& e){
        if(e.type==SDL_KEYUP){blocked[e.keysym.scancode]=false;return;}
        if(!focused||e.repeat||blocked[e.keysym.scancode])return;
        auto k=e.keysym.sym;
        if(k==SDLK_F11){toggle_fullscreen();return;}
        if(modal=="busy")return;
        if(k==SDLK_ESCAPE){back();return;}
        if(modal=="browser"&&(e.keysym.mod&KMOD_CTRL)&&k==SDLK_l){focus="path";path_edit=true;path_select_all=true;SDL_StartTextInput();return;}
        if(k==SDLK_TAB){move_focus((e.keysym.mod&KMOD_SHIFT)?-1:1);return;}
        if(modal=="browser"&&focus=="path"){
            if(k==SDLK_RETURN){path_edit=false;SDL_StopTextInput();read_browser(fs::u8path(path_text));focus="files";return;}
            if(k==SDLK_BACKSPACE){if(path_select_all)path_text.clear();else pop_utf8(path_text);path_select_all=false;path_edit=true;return;}
            if((e.keysym.mod&KMOD_CTRL)&&k==SDLK_a){path_select_all=true;return;}
            if((e.keysym.mod&KMOD_CTRL)&&k==SDLK_v){char* value=SDL_GetClipboardText();if(value){if(path_select_all)path_text.clear();path_select_all=false;path_text+=value;SDL_free(value);while(path_text.size()>32768)pop_utf8(path_text);}return;}
        }
        if(k==SDLK_UP)navigate(0,-1);else if(k==SDLK_DOWN)navigate(0,1);else if(k==SDLK_LEFT)navigate(-1,0);else if(k==SDLK_RIGHT)navigate(1,0);
        else if(k==SDLK_RETURN||k==SDLK_SPACE)activate();
        else if(modal.empty()&&k==SDLK_a)browse(false);
        else if(modal.empty()&&k==SDLK_F5){library.reload();notice="Catálogo actualizado.";}
    }
    void event(const SDL_Event& e){
        if(e.type==SDL_QUIT){if(session||starting_session.valid()){pending_quit=true;return_to_library();}else if(job.valid())pending_quit=true;else running=false;return;}
        if(e.type==SDL_CONTROLLERDEVICEADDED){open_controller(e.cdevice.which);return;}
        if(e.type==SDL_CONTROLLERDEVICEREMOVED){if(controller&&SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))==e.cdevice.which){SDL_GameControllerClose(controller);controller=nullptr;neutral.fill(false);for(int i=0;i<SDL_NumJoysticks();++i)open_controller(i);}return;}
        if(e.type==SDL_WINDOWEVENT){
            if(e.window.event==SDL_WINDOWEVENT_FOCUS_LOST){focused=false;neutral.fill(false);}
            if(e.window.event==SDL_WINDOWEVENT_FOCUS_GAINED){focused=true;int count=0;auto keys=SDL_GetKeyboardState(&count);for(int i=0;i<count&&i<SDL_NUM_SCANCODES;++i)blocked[i]=keys[i]!=0;reset_controller_input();}
            if(e.window.event==SDL_WINDOWEVENT_SIZE_CHANGED&&!(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN_DESKTOP)){
                library.preferences["window_width"]=std::clamp(e.window.data1,920,7680);library.preferences["window_height"]=std::clamp(e.window.data2,640,4320);library.dirty=true;}
        }
        if(e.type==SDL_KEYDOWN||e.type==SDL_KEYUP){key(e.key);return;}
        if(e.type==SDL_DROPFILE){fs::path p=fs::u8path(e.drop.file);SDL_free(e.drop.file);if(focused&&!job.valid()){browse(false);read_browser(p);}return;}
        if(!focused)return;
        if(e.type==SDL_TEXTINPUT&&modal=="browser"&&path_edit&&(path_select_all?0:path_text.size())+std::char_traits<char>::length(e.text.text)<=32768){if(path_select_all)path_text.clear();path_select_all=false;path_text+=e.text.text;}
        if(e.type==SDL_MOUSEBUTTONDOWN&&e.button.button==SDL_BUTTON_LEFT){float x,y;SDL_RenderWindowToLogical(renderer,e.button.x,e.button.y,&x,&y);
            for(const auto& w:widgets)if(x>=w.rect.x&&x<w.rect.x+w.rect.w&&y>=w.rect.y&&y<w.rect.y+w.rect.h){focus=w.id;auto id=w.id;auto fn=w.action;fn();if(e.button.clicks>=2&&id.rfind("entry:",0)==0)browser_enter();break;}}
        if(e.type==SDL_MOUSEWHEEL&&modal=="browser"&&focus=="files")navigate(0,-e.wheel.y);
        if(!controller)return;
        auto id=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
        if(e.type==SDL_CONTROLLERBUTTONUP&&e.cbutton.which==id&&e.cbutton.button<SDL_CONTROLLER_BUTTON_MAX)blocked_buttons[e.cbutton.button]=false;
        if(e.type==SDL_CONTROLLERBUTTONDOWN&&e.cbutton.which==id&&e.cbutton.button<SDL_CONTROLLER_BUTTON_MAX&&!blocked_buttons[e.cbutton.button]){
            if(e.cbutton.button==SDL_CONTROLLER_BUTTON_A)activate();else if(e.cbutton.button==SDL_CONTROLLER_BUTTON_B)back();
            else if(e.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_UP)navigate(0,-1);else if(e.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_DOWN)navigate(0,1);
            else if(e.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_LEFT)navigate(-1,0);else if(e.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_RIGHT)navigate(1,0);
            else if(modal.empty()&&e.cbutton.button==SDL_CONTROLLER_BUTTON_Y)browse(false);
            else if(!modal.empty()&&e.cbutton.button==SDL_CONTROLLER_BUTTON_LEFTSHOULDER)move_focus(-1);
            else if(!modal.empty()&&e.cbutton.button==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)move_focus(1);
            else if(modal.empty()&&e.cbutton.button==SDL_CONTROLLER_BUTTON_LEFTSHOULDER)library.select_installation(-1);
            else if(modal.empty()&&e.cbutton.button==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)library.select_installation(1);
        }
        if(e.type==SDL_CONTROLLERAXISMOTION&&e.caxis.which==id&&(e.caxis.axis==SDL_CONTROLLER_AXIS_LEFTX||e.caxis.axis==SDL_CONTROLLER_AXIS_LEFTY)){
            int axis=e.caxis.axis;if(std::abs(int(e.caxis.value))<12000){neutral[axis]=true;return;}
            if(neutral[axis]&&std::abs(int(e.caxis.value))>22000&&SDL_GetTicks()-last_axis>120){neutral[axis]=false;last_axis=SDL_GetTicks();int d=e.caxis.value>0?1:-1;navigate(axis==SDL_CONTROLLER_AXIS_LEFTX?d:0,axis==SDL_CONTROLLER_AXIS_LEFTY?d:0);}
        }
    }
    void main_page(){
        fill({0,0,W,H},color(0x101823));fill({0,0,326,H},color(0x151f2c));fill({0,0,6,H},aqua);
        text("TITUS",30,24,30,ink);text("ENGINE",133,32,18,aqua);text("TU COLECCIÓN, EN UN SOLO LUGAR",30,67,12,muted);
        text("BIBLIOTECA",30,119,13,muted);text(std::to_string(library.games.size())+" juegos",237,118,13,muted);
        int y=158;
        for(const auto& id:library.games){const auto& g=*std::find_if(game_info.begin(),game_info.end(),[&](const GameInfo& g){return id==g.id;});bool selected=library.preferences["selected_game"]==id;
            SDL_Rect rect{20,y,286,76};panel(rect,color(selected?0x27384a:0x192635));if(selected)fill({20,y+14,3,48},color(g.color));
            panel({34,y+16,44,44},color(g.color),9);text(g.initials,40,y+26,18,color(0x14212b));
            text(g.title,90,y+17,16,ink,200);std::set<std::string> editions;for(const auto& i:library.catalog["installations"])if(i["game_id"]==id)editions.insert(i["edition_id"]);
            text(editions.size()>1?"DOS + Amiga":(*editions.begin()=="dos"?"DOS":"Amiga"),90,y+43,13,muted);
            if(focus=="game:"+id)outline({rect.x-2,rect.y-2,rect.w+4,rect.h+4},aqua);
            widget("game:"+id,rect,[this,id](){library.choose_game(id);});y+=88;
        }
        if(library.games.empty()){text("Tu colección empieza aquí.",30,180,17,ink,260,true);text("Añade una carpeta con tus juegos para verlos en la biblioteca.",30,224,15,muted,258,true);}
        text("Basado en cyxx/blues",30,652,12,muted);text("y motores de la comunidad",30,673,12,muted);
        button("add","+ Añadir juegos",{366,27,180,42},[this](){browse(false);},library.healthy,true);
        button("reload","Actualizar",{558,27,126,42},[this](){library.reload();notice="Catálogo actualizado.";});
        button("screen","Pantalla",{824,27,118,42},[this](){toggle_fullscreen();});button("exit","Salir",{954,27,106,42},[this](){set_modal("quit");});
        if(!library.healthy){text("No se pudo abrir el catálogo",366,125,28,gold);text(library.error,366,185,16,muted,675,true);
            text("Se ha conservado el archivo original. Puedes restaurar su copia de seguridad.",366,310,17,ink,660,true);
            button("recover","Restaurar copia",{366,393,210,44},[this](){set_modal("recover");});return;}
        auto i=library.selected();if(!i){text("Un hogar para tus clásicos",366,145,32,ink);text("Añade tus juegos de Titus",366,205,20,aqua);
            text("Selecciona una carpeta con los datos originales. Detectaremos los juegos y sus versiones sin cambiar sus archivos.",366,255,18,muted,620,true);
            for(int j=0;j<5;++j){const auto& g=game_info[j];panel({366+j*134,380,112,112},color(0x213043));text(g.initials,385+j*134,406,36,color(g.color));}
            text("DOS  /  AMIGA",366,539,14,muted);return;}
        const auto& g=library.info();text("TU BIBLIOTECA  /  "+std::string(i->at("edition_id")=="dos"?"DOS":"AMIGA"),366,110,13,color(g.color));
        text(g.title,366,143,34,ink,690);text(g.description,366,195,16,muted,680);
        panel({366,240,694,327},color(0x1a2737));text("EDICIÓN",390,259,12,muted);
        bool dos=false,amiga=false;auto choices=library.installations();for(const auto& c:choices){dos|=c["edition_id"]=="dos";amiga|=c["edition_id"]=="amiga";}
        button("dos","DOS",{390,285,100,40},[this](){library.select_edition("dos");},dos,i->at("edition_id")=="dos");
        button("amiga","Amiga",{502,285,116,40},[this](){library.select_edition("amiga");},amiga,i->at("edition_id")=="amiga");
        int index=0;for(size_t n=0;n<choices.size();++n)if(choices[n]["id"]==i->at("id"))index=static_cast<int>(n);
        button("edition","Instalación "+std::to_string(index+1)+" / "+std::to_string(choices.size())+"  ›",{700,285,334,40},[this](){library.select_installation(1);});
        text(library.data_status(),390,348,18,i->at("status")=="recognized"&&library.source_available()?aqua:gold,638);
        auto source=library.source();text(source->at("kind")=="adf"?"IMAGEN DE DISCO":"CARPETA DEL JUEGO",390,391,12,muted);
        text(source->at("location").at("path").get<std::string>(),390,415,14,ink,638);
        int location_index=0;for(size_t n=0;n<i->at("locations").size();++n)if(i->at("locations")[n]==*source)location_index=static_cast<int>(n);
        button("source","Ubicación "+std::to_string(location_index+1)+" / "+std::to_string(i->at("locations").size())+"  ›",{390,453,224,38},[this](){library.select_source(1);});
        text("Los archivos originales y las partidas se conservan.",390,520,13,muted,638);
        button("play","Jugar",{366,592,132,45},[this](){begin_play();},library.can_play(),true);
        button("check","Verificar",{510,592,126,45},[this](){auto id=library.selected()->at("id").get<std::string>();auto revision=library.catalog.at("revision").get<long long>();action([id,revision](CatalogStore& store){return store.recheck(id,revision);});});
        button("relocate","Localizar",{648,592,126,45},[this](){browse(true);});button("remove","Quitar",{786,592,110,45},[this](){set_modal("remove");});
        text(library.launch_reason(),366,651,14,muted,690,true);
    }
    void draw_modal(){
        if(modal.empty())return;fill({0,0,W,H},color(0x050a12,215));widgets.clear();
        if(modal=="session"){
            panel({260,226,580,246},color(0x213146));text("Blues Brothers",294,253,27,ink);
            text(session?session->message():"Preparando tu perfil…",294,305,17,muted,510,true);
            text(session&&session->stalled()?"El proceso tarda más de lo esperado. Puedes seguir esperando.":"Tus opciones y partidas originales se conservan.",294,353,14,muted,510,true);
            button("cancel-session","Volver a la biblioteca",{294,403,session&&session->stalled()?270:510,42},[this](){return_to_library();});
            if(session&&session->stalled())button("force-session","Cerrar proceso…",{584,403,220,42},[this](){set_modal("force-session");});return;
        }
        if(modal=="browser"){
            panel({120,73,860,575},color(0x1b293b));text(relocate_id.empty()?"Añadir juegos":"Volver a localizar",146,94,27,ink);
            text(controller?"Cruceta: navegar · A: abrir · LB/RB: cambiar control · B: cancelar":"Carpetas y discos ADF · Enter para abrir · Ctrl+L para escribir una ruta",146,137,13,muted);
            panel({146,168,808,40},color(focus=="path"&&path_select_all?0x294b61:0x101b29),6);if(focus=="path")outline({145,167,810,42},aqua);
            auto path_shown=path_text;if(path_edit&&path_shown.size()>85){while(path_shown.size()>85){size_t n=1;while(n<path_shown.size()&&(static_cast<unsigned char>(path_shown[n])&192)==128)++n;path_shown.erase(0,n);}path_shown="…"+path_shown;}
            text(path_shown.empty()?"Selecciona una unidad":path_shown,156,178,14,ink,784);
            widget("path",{146,168,808,40},[this](){path_edit=true;path_select_all=false;SDL_StartTextInput();});
            button("parent","↑ Carpeta superior",{146,222,199,34},[this](){if(!browse_dir.empty())read_browser(browse_dir.parent_path());});
            button("roots","Unidades / raíz",{357,222,182,34},[this](){read_browser({});});
            text(std::to_string(entries.size())+" entradas",803,230,13,muted);
            panel({146,269,808,244},color(0x111e2c),6);if(focus=="files")outline({145,268,810,246},aqua);
            if(browse_index>=0&&browse_index<browse_top)browse_top=browse_index;if(browse_index>=browse_top+6)browse_top=browse_index-5;
            if(entries.empty())text("No hay subcarpetas ni imágenes ADF en esta ubicación.",162,291,15,muted,770);
            for(int row=0;row<6&&browse_top+row<static_cast<int>(entries.size());++row){int index=browse_top+row;const auto& p=entries[index];int y=277+row*38;
                if(index==browse_index)panel({154,y,792,35},color(focus=="files"?0x304b60:0x243749),4);
                std::error_code error;bool dir=fs::is_directory(p,error);text(dir?"›":"◉",165,y+6,16,dir?aqua:gold);
                text(p.filename().empty()?p.u8string():p.filename().u8string(),190,y+6,15,ink,737);
                widget("entry:"+std::to_string(index),{154,y,792,35},[this,index](){browse_index=index;focus="files";});
            }
            if(entries.size()>6){int height=std::max(20,228*6/static_cast<int>(entries.size()));fill({948,277,3,228},color(0x26384a));fill({948,277+browse_top*(228-height)/static_cast<int>(entries.size()-6),3,height},muted);}
            // A single logical focus target for keyboard/controller list navigation.
            widget("files",{0,0,0,0},[this](){browser_enter();});
            if(!browse_error.empty())text(browse_error,146,519,13,gold,804);
            button("recursive",recursive?"✓ Incluir subcarpetas":"Incluir subcarpetas",{146,551,245,34},[this](){recursive=!recursive;},relocate_id.empty());
            bool adf=browse_index>=0&&!entries.empty()&&browse_index<static_cast<int>(entries.size())&&fs::is_regular_file(entries[browse_index]);
            button("use",adf?"Usar imagen ADF":"Usar esta carpeta",{515,592,246,40},[this,adf](){use_browser(adf?entries[browse_index]:browse_dir);},browse_error.empty()&&(!browse_dir.empty()||adf),true);
            button("cancel","Cancelar",{775,592,179,40},[this](){close_modal();});return;
        }
        panel({260,226,580,246},color(0x213146));
        if(modal=="busy"){text("Comprobando archivos…",294,260,26,ink);text("Conservamos los originales mientras actualizamos la biblioteca.",294,315,16,muted,510,true);
            int dots=(SDL_GetTicks()/300)%4;text(std::string(dots+1,'.'),294,402,22,aqua);return;}
        std::string heading=modal=="force-session"?"¿Forzar el cierre del juego?":modal=="remove"?"¿Quitar esta instalación?":modal=="recover"?"¿Restaurar la copia del catálogo?":"¿Salir de Titus Engine?";
        text(heading,294,251,25,ink,510);
        text(modal=="force-session"?"Se perderá el progreso que no hayas guardado. Los slots y los originales se conservarán.":modal=="remove"?"Solo se quitará de la biblioteca. Los archivos del juego y las partidas se conservarán.":modal=="recover"?"El catálogo dañado se conservará con otro nombre. Se recuperará la última copia válida.":"Tu selección quedará guardada para la próxima vez.",294,305,17,muted,510,true);
        button("confirm",modal=="force-session"?"Cerrar proceso":modal=="remove"?"Quitar":modal=="recover"?"Restaurar":"Salir",{294,403,238,42},[this](){
            if(modal=="force-session"){try{if(session)session->force_stop();}catch(const std::exception& e){notice=friendly_error(e);}modal="session";return;}if(modal=="quit"){running=false;return;}if(modal=="recover"){action([](CatalogStore& store){return store.recover();});return;}
            auto id=library.selected()->at("id").get<std::string>();auto revision=library.catalog.at("revision").get<long long>();action([id,revision](CatalogStore& store){return store.remove(id,revision);});
        },true,true);
        button("cancel","Cancelar",{550,403,254,42},[this](){if(modal=="force-session"){modal="session";focus="cancel-session";}else close_modal();});
    }
    void draw(){
        if(text_cache.size()>512){for(auto& p:text_cache)SDL_DestroyTexture(p.second.texture);text_cache.clear();}
        widgets.clear();main_page();
        auto message=!notice.empty()?notice:library.preference_error;
        if(!message.empty()){fill({326,682,774,38},color(0x27384b));text(message,344,691,12,gold,738);}
        if(message.empty())text(controller?"Cruceta / stick: navegar   A: elegir   B: volver   Y: añadir":"↑↓ Navegar   Tab Cambiar control   Enter Elegir   Esc Volver   F11 Pantalla",366,699,11,muted,694);
        draw_modal();
        if(!widgets.empty()&&std::none_of(widgets.begin(),widgets.end(),[this](const Widget& w){return w.id==focus;}))focus=widgets[0].id;
    }
    void capture(){if(screenshot.empty())return;int w,h;SDL_GetRendererOutputSize(renderer,&w,&h);
        auto surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);if(!surface)throw Error("E_CAPTURE",SDL_GetError());
        auto result=SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_ARGB8888,surface->pixels,surface->pitch);
        if(result==0)result=SDL_SaveBMP(surface,screenshot.u8string().c_str());SDL_FreeSurface(surface);if(result!=0)throw Error("E_CAPTURE",SDL_GetError());}
    void capture_state(const char* label){if(screenshot.empty())return;auto original=screenshot;screenshot=original.parent_path()/fs::u8path(original.stem().u8string()+"."+label+".bmp");capture();screenshot=original;}
    void push_key(SDL_Keycode sym,SDL_Keymod mod=KMOD_NONE){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=sym;e.key.keysym.mod=mod;e.key.keysym.scancode=SDL_GetScancodeFromKey(sym);SDL_PushEvent(&e);}
    void play_test_tick(){
        if(!test_play)return;
        if(SDL_GetTicks()-play_test_started>(test_pre1?300000u:test_fox?240000u:test_family?180000u:60000u))throw Error("E_UI_SESSION_TEST","Session UI probe timed out");
        if(starting_session.valid()||session)return;
        std::vector<std::pair<std::string,std::string>> choices={{"blues","dos"}};
        if(test_family){choices.push_back({"jukebox","dos"});choices.push_back({"prehistorik2","dos"});}
        if(test_fox)choices.push_back({"fox","dos"});
        if(test_pre1)choices.push_back({"prehistorik","dos"});
        choices.push_back({"blues","amiga"});
        if(play_test_stage%2==0){
            if(play_test_stage>0)check(!session_hidden&&(SDL_GetWindowFlags(window)&SDL_WINDOW_SHOWN),"library window restored after game");
            size_t next=static_cast<size_t>(play_test_stage/2);
            if(next==choices.size()){library.persist();capture();std::cout<<Json({{"session_ui_checks",smoke_checks},{"sessions",choices.size()},{"selected_edition",library.selected()->at("edition_id")},{"notice",notice}}).dump()<<'\n';running=false;return;}
            library.choose_game(choices[next].first);library.select_edition(choices[next].second);check(library.can_play(),"native game play enabled");focus="play";draw();push_key(SDLK_RETURN);++play_test_stage;
        }
    }
    void smoke_report(){capture();library.persist();check(library.preference_error.empty(),"preferences saved");std::cout<<Json({{"ui_checks",smoke_checks},{"games",library.games.size()},{"installations",library.catalog["installations"].size()},{"selected_game",library.preferences["selected_game"]},{"fullscreen",library.preferences["fullscreen"]}}).dump()<<'\n';running=false;}
    void smoke_tick(){
        if(smoke_step==-4){focus="use";push_key(SDLK_RETURN);}
        if(smoke_step==-3&&job.valid())return;
        if(smoke_step==-2){check(library.healthy&&!library.catalog["installations"].empty()&&notice=="Biblioteca actualizada.","browser imports games asynchronously");workflow_count=library.catalog["installations"].size();notice.clear();}

        if(smoke_step==0){focused=true;blocked.fill(false);smoke_initial=library.preferences["selected_game"];check(library.healthy&&(!library.selected()||focus=="game:"+std::string(library.info().id)),"catalog and initial selection focused");if(library.selected())focus="game:"+std::string(library.info().id);push_key(SDLK_DOWN);}
        if(smoke_step==2){if(library.games.size()>1)check(library.preferences["selected_game"]!=smoke_initial,"keyboard changes game");push_key(SDLK_RIGHT);}
        if(smoke_step==4){check(focus=="edition"||!library.selected(),"keyboard reaches editions");push_key(SDLK_F11);}
        if(smoke_step==6){check((SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN_DESKTOP)!=0,"fullscreen entered");capture_state("fullscreen");push_key(SDLK_F11);}
        if(smoke_step==8){check((SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN_DESKTOP)==0,"window restored");push_key(SDLK_a);}
        if(smoke_step==10){check(modal=="browser","browser opened");capture_state("browser");push_key(SDLK_l,KMOD_CTRL);SDL_Event input{};input.type=SDL_TEXTINPUT;SDL_strlcpy(input.text.text,"carpeta ñ",sizeof input.text.text);SDL_PushEvent(&input);}
        if(smoke_step==11){check(focus=="path"&&path_text=="carpeta ñ","UTF-8 path replaces Ctrl-L selection");push_key(SDLK_BACKSPACE);push_key(SDLK_ESCAPE);}
        if(smoke_step==12){check(path_text=="carpeta ","backspace removes a full UTF-8 character");check(modal.empty(),"browser cancelled");if(library.selected()){focus="remove";push_key(SDLK_RETURN);}}
        if(smoke_step==14&&library.selected()){check(modal=="remove"&&focus=="cancel","remove confirmation defaults to cancel");capture_state("remove");push_key(SDLK_ESCAPE);}
        if(smoke_step==16){check(modal.empty(),"remove cancelled");smoke_focus_game=library.preferences["selected_game"];SDL_Event e{};e.type=SDL_WINDOWEVENT;e.window.event=SDL_WINDOWEVENT_FOCUS_LOST;event(e);push_key(SDLK_DOWN);}
        if(smoke_step==18){check(!focused&&library.preferences["selected_game"]==smoke_focus_game,"unfocused input ignored");SDL_Event e{};e.type=SDL_WINDOWEVENT;e.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;event(e);blocked.fill(false);}
        if(smoke_step==20){check(focused,"focus restored");if(!library.games.empty()){library.choose_game(library.games[0]);focus="game:"+std::string(library.info().id);}
            virtual_device=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);
            check(virtual_device>=0,"virtual controller attached");virtual_pad=SDL_JoystickOpen(virtual_device);check(virtual_pad!=nullptr,"virtual controller opened");virtual_id=SDL_JoystickInstanceID(virtual_pad);if(controller){SDL_GameControllerClose(controller);controller=nullptr;}open_controller(virtual_device);
        }
        if(smoke_step==22){check(controller!=nullptr,"controller hotplug handled");smoke_pad_game=library.preferences["selected_game"];SDL_JoystickSetVirtualButton(virtual_pad,SDL_CONTROLLER_BUTTON_DPAD_DOWN,1);}
        if(smoke_step==24){if(library.games.size()>1)check(library.preferences["selected_game"]!=smoke_pad_game,"controller changes game");SDL_JoystickSetVirtualButton(virtual_pad,SDL_CONTROLLER_BUTTON_DPAD_DOWN,0);}
        if(smoke_step==26)SDL_JoystickSetVirtualButton(virtual_pad,SDL_CONTROLLER_BUTTON_Y,1);
        if(smoke_step==28){check(modal=="browser","controller opens browser");SDL_JoystickSetVirtualButton(virtual_pad,SDL_CONTROLLER_BUTTON_Y,0);SDL_JoystickSetVirtualButton(virtual_pad,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1);}
        if(smoke_step==29){check(focus=="recursive"||focus=="use","controller reaches browser actions");SDL_JoystickSetVirtualButton(virtual_pad,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0);SDL_JoystickSetVirtualButton(virtual_pad,SDL_CONTROLLER_BUTTON_B,1);}
        if(smoke_step==30){check(modal.empty(),"controller cancels browser");SDL_JoystickSetVirtualButton(virtual_pad,SDL_CONTROLLER_BUTTON_B,0);}
        if(smoke_step==32){if(!library.games.empty()){library.choose_game(library.games[0]);focus="game:"+std::string(library.info().id);}smoke_pad_game=library.preferences["selected_game"];SDL_JoystickSetVirtualAxis(virtual_pad,SDL_CONTROLLER_AXIS_LEFTY,30000);}
        if(smoke_step==34){if(library.games.size()>1)check(library.preferences["selected_game"]!=smoke_pad_game,"analog navigation");smoke_pad_game=library.preferences["selected_game"];}
        if(smoke_step==36){check(library.preferences["selected_game"]==smoke_pad_game,"held stick does not repeat");SDL_JoystickSetVirtualAxis(virtual_pad,SDL_CONTROLLER_AXIS_LEFTY,0);}
        if(smoke_step==38)SDL_JoystickSetVirtualAxis(virtual_pad,SDL_CONTROLLER_AXIS_LEFTY,30000);
        if(smoke_step==40){if(library.games.size()>1)check(library.preferences["selected_game"]!=smoke_pad_game,"neutral stick rearms navigation");SDL_JoystickClose(virtual_pad);virtual_pad=nullptr;SDL_JoystickDetachVirtual(virtual_device);virtual_device=-1;}
        if(smoke_step==42){check(!controller||SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))!=virtual_id,"controller removal handled");SDL_SetWindowSize(window,960,660);}
        if(smoke_step==44){int width,height;SDL_GetWindowSize(window,&width,&height);check(width==960&&height==660,"window resized");capture_state("resized");SDL_SetWindowSize(window,1100,720);if(!library.games.empty()){library.choose_game(library.games[0]);focus="game:"+std::string(library.info().id);}}
        if(smoke_step==50){
            if(workflow_source.empty()){smoke_report();return;}
            focus="check";push_key(SDLK_RETURN);
        }
        if(smoke_step==51&&job.valid())return;
        if(smoke_step==52){check(notice=="Biblioteca actualizada."&&library.catalog["installations"].size()==workflow_count,"verify completes without removing installs");notice.clear();focus="remove";push_key(SDLK_RETURN);}
        if(smoke_step==54){check(modal=="remove","remove dialog opened for workflow");focus="confirm";push_key(SDLK_RETURN);}
        if(smoke_step==55&&job.valid())return;
        if(smoke_step==56){check(library.catalog["installations"].size()+1==workflow_count,"confirmed remove updates catalog");check(fs::exists(workflow_source),"remove preserves original directory");browse(false);read_browser(workflow_source);focus="use";draw();push_key(SDLK_RETURN);}
        if(smoke_step==57&&job.valid())return;
        if(smoke_step==58){check(library.catalog["installations"].size()==workflow_count,"removed installation can be added again");notice.clear();if(!library.games.empty()){library.choose_game(library.games[0]);focus="game:"+std::string(library.info().id);}}
        if(smoke_step==60)smoke_report();
        ++smoke_step;
    }
public:
    App(Paths p,bool test,fs::path capture_path,fs::path initial_browse,fs::path import_source,int play_probe):paths(std::move(p)),library(paths),smoke(test),screenshot(std::move(capture_path)),workflow_source(std::move(import_source)){
        test_play=play_probe!=0;test_family=play_probe>=2;test_fox=play_probe>=3;test_pre1=play_probe==4;play_test_started=SDL_GetTicks();
        char* base=SDL_GetBasePath();if(!base)throw Error("E_PATH",SDL_GetError());package_root=fs::u8path(base);SDL_free(base);
        try{auto registry=parse_json(read_document(package_root/"engines/registry.json"));
            for(const auto& entry:registry.at("engines"))if(entry.at("adapter_state")=="validated"&&registry.at("role")=="package"){bool installed=fs::is_regular_file(package_root/fs::u8path(entry.at("executables").at(platform()).get<std::string>()));if(entry.at("engine_id")=="blues-family")library.blues_runtime_installed=installed;
                if(installed)for(const auto& g:entry.at("games"))for(const auto& edition:g.at("editions"))if(edition=="dos")library.runtime_games.insert(g.at("game_id"));}
        }catch(const std::exception&){}
        if(!initial_browse.empty()){library.preferences["browse_path"]=normalized_path(initial_browse).u8string();library.dirty=true;}
        if(smoke)SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
        if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER)!=0)throw Error("E_SDL",SDL_GetError());
        if(TTF_Init()!=0)throw Error("E_FONT",TTF_GetError());
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"linear");
        window=SDL_CreateWindow("Titus Engine",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,library.preferences["window_width"],library.preferences["window_height"],SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
        if(!window)throw Error("E_WINDOW",SDL_GetError());SDL_SetWindowMinimumSize(window,920,640);
        renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
        if(!renderer)renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);if(!renderer)throw Error("E_RENDER",SDL_GetError());
        SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);SDL_RenderSetLogicalSize(renderer,W,H);
        if(library.preferences["fullscreen"]==true&&!smoke)toggle_fullscreen();
        if(library.selected())focus="game:"+std::string(library.info().id);
        for(int i=0;i<SDL_NumJoysticks();++i)open_controller(i);font(18);SDL_EventState(SDL_DROPFILE,SDL_ENABLE);
        if(!workflow_source.empty()){browse(false);read_browser(workflow_source);smoke_step=-4;}
    }
    ~App(){if(job.valid())job.wait();if(virtual_pad)SDL_JoystickClose(virtual_pad);if(virtual_device>=0)SDL_JoystickDetachVirtual(virtual_device);for(auto& p:text_cache)SDL_DestroyTexture(p.second.texture);for(auto& p:fonts)TTF_CloseFont(p.second);
        if(controller)SDL_GameControllerClose(controller);if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);TTF_Quit();SDL_Quit();}
    int run(){draw();SDL_RenderPresent(renderer);while(running){SDL_Event e;while(SDL_PollEvent(&e)){auto previous_modal=modal;event(e);if(modal!=previous_modal&&!session_hidden)draw();}finish_job();poll_session();
            if(!session_hidden){draw();if(smoke)smoke_tick();if(test_play)play_test_tick();SDL_RenderPresent(renderer);}
            if(!job.valid()&&!starting_session.valid()&&!session&&library.dirty&&SDL_GetTicks()-last_save>600){library.persist();last_save=SDL_GetTicks();}
            SDL_Delay(session_hidden?25:smoke?20:8);
        }library.persist();return 0;}
};
int run(const std::vector<std::string>& args){
    try{fs::path user,screenshot,browse,import_source;bool smoke=false;int play_probe=0;
        for(size_t i=1;i<args.size();++i){auto option=args[i];if(option=="--smoke-test")smoke=true;
            else if(option=="--session-smoke-test")play_probe=1;
            else if(option=="--family-session-smoke-test")play_probe=2;
            else if(option=="--fox-session-smoke-test")play_probe=3;
            else if(option=="--pre1-session-smoke-test")play_probe=4;
            else if(option=="--user-dir"||option=="--screenshot"||option=="--browse-dir"||option=="--test-import"){if(++i==args.size())throw Error("E_ARGUMENT","Missing option value");auto p=fs::u8path(args[i]);if(option=="--user-dir")user=p;else if(option=="--screenshot")screenshot=p;else if(option=="--browse-dir")browse=p;else import_source=p;}
            else if(option=="--help"){std::cout<<"titus-engine [--user-dir ABS] [--browse-dir PATH]\n  F11: fullscreen; Tab/arrows/Enter: navigation; A: add; Esc: cancel/quit\n  --smoke-test [--screenshot FILE.bmp]: bounded UI regression test\n";return 0;}
            else throw Error("E_ARGUMENT","Unknown option: "+option);}
        if((smoke||play_probe)&&user.empty())throw Error("E_ARGUMENT","--smoke-test requires an isolated --user-dir");
        if(!import_source.empty()&&!smoke)throw Error("E_ARGUMENT","--test-import requires --smoke-test");
        SDL_SetMainReady();App app(Paths::resolve(user),smoke,screenshot,browse,import_source,play_probe);return app.run();
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
}
#ifdef _WIN32
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){SetConsoleOutputCP(CP_UTF8);int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(!argv)return 1;std::vector<std::string> args;for(int i=0;i<argc;++i)args.push_back(fs::path(argv[i]).u8string());LocalFree(argv);return run(args);}
#else
int main(int argc,char** argv){return run(std::vector<std::string>(argv,argv+argc));}
#endif
