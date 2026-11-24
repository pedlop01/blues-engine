/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <SDL.h>
#define SDL_SWSURFACE 0
#define SDL_HWSURFACE 0
#define SDL_DOUBLEBUF 0
#define SDL_FULLSCREEN 0
#define SDL_SRCCOLORKEY 1
#define SDL_SRCALPHA 1
#define SDLK_UP 273
#define SDLK_DOWN 274
#define SDLK_RIGHT 275
#define SDLK_LEFT 276
#define SDLK_F1 282
#define SDLK_F2 283
#define SDLK_F3 284
#define SDLK_F4 285
#define SDLK_LAST 512
#define SDL_DisplayFormat(s) SDL_ConvertSurface(s,(s)->format,0)
#define SDL_SetColors(s,c,f,n) (SDL_SetPaletteColors((s)->format->palette,c,f,n)==0)
#define SDL_Flip(s) 0
#ifdef __cplusplus
extern "C" {
#endif
Uint8* fox_legacy_keys(int*);
int fox_legacy_event(SDL_Event*);
#define SDL_GetKeyState fox_legacy_keys
#define SDL_PollEvent fox_legacy_event
#define SDL_PumpEvents() ((void)0)
#ifdef __cplusplus
}
#endif
#define SDL_SetAlpha(s,f,a) SDL_SetSurfaceAlphaMod(s,a)
