// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <SDL.h>
#include <array>
#include <string>
#include <algorithm>
#include <stdexcept>
namespace titus_old {
inline void (*on_tick)()=nullptr;
inline void (*on_ready)(const char*,bool)=nullptr;
inline void (*on_frame)(int,int,bool)=nullptr;
inline void (*on_audio)(const Sint16*,int)=nullptr;
inline bool audio_ok=false;
inline bool quit_seen=false;
inline SDL_Renderer* renderer=nullptr;
inline std::string capture_path;
inline std::array<int,SDL_NUM_SCANCODES> synthetic=[](){std::array<int,SDL_NUM_SCANCODES> a{};a.fill(-1);return a;}();
inline const Uint8* keys(int* count){static std::array<Uint8,SDL_NUM_SCANCODES> held{};int n=0;auto actual=SDL_GetKeyboardState(&n);if(count)*count=n;std::copy(actual,actual+std::min(n,int(held.size())),held.begin());for(int i=0;i<n&&i<int(held.size());++i)if(synthetic[i]>=0)held[i]=synthetic[i];return held.data();}
inline int poll(SDL_Event* e){if(on_tick)on_tick();int found=SDL_PollEvent(e);if(found&&e&&e->type==SDL_QUIT)quit_seen=true;return found;}
inline void ready(const char* scene,SDL_Renderer* ren){renderer=ren;if(on_ready)on_ready(scene,audio_ok);}
inline void frame(int x,int y,bool paused){if(on_frame)on_frame(x,y,paused);}
}
