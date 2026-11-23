/* SPDX-License-Identifier: GPL-3.0-or-later
 * Managed bootstrap for OpenTitus, Copyright (C) 2008-2011 The OpenTitus team.
 * Derived from the pinned opentitus.c; see source revisions and GPL notices.
 */
#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>
#undef main
#define main fox_original_main
#include "opentitus.c"
#undef main
#include "managed_fox.h"
#include "player.h"
#include "audio.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
static char *data_root;
extern FILE *__real_fopen(const char *,const char *);
void fox_set_data_root(const char *root){data_root=strdup(root);}
FILE *__wrap_fopen(const char *name,const char *mode){
    size_t length;char *path;FILE *file;
    /* The original engine gets flat relative aliases, never user path tokens. */
    if(!data_root)return __real_fopen(name,mode);
    if(strchr(name,'/')||strchr(name,'\\')||strchr(name,':')||strstr(name,"..")||mode[0]!='r')return NULL;
    length=strlen(data_root)+strlen(name)+2;path=malloc(length);if(!path)return NULL;
    snprintf(path,length,"%s/%s",data_root,name);
#ifdef _WIN32
    {
        int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0),i;wchar_t *wide=n?malloc((n+8)*sizeof(wchar_t)):NULL,*input=n?malloc(n*sizeof(wchar_t)):NULL;wchar_t wmode[16];
        file=NULL;
        if(wide&&input&&strlen(mode)<15){MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,input,n);for(i=0;input[i];++i)if(input[i]==L'/')input[i]=L'\\';
            if(input[0]==L'\\'&&input[1]==L'\\'){wcscpy(wide,L"\\\\?\\UNC\\");wcscat(wide,input+2);}else{wcscpy(wide,L"\\\\?\\");wcscat(wide,input);}
            for(i=0;i<=(int)strlen(mode);++i)wmode[i]=(unsigned char)mode[i];file=_wfopen(wide,wmode);
        }
        free(wide);free(input);
    }
#else
    file=__real_fopen(path,mode);
#endif
    free(path);return file;
}
static Uint8 held[SDLK_LAST],blocked[SDLK_LAST];
static unsigned long long samples,audible,clipped;
static int peak;
static void (*music_callback)(void *,Uint8 *,int);
static void *music_context;
extern int __real_SDL_PollEvent(SDL_Event *);
extern Uint8 *__real_SDL_GetKeyState(int *);
extern void __real_Mix_HookMusic(void (*)(void *,Uint8 *,int),void *);
static void observe_audio(void *context,Uint8 *data,int len){
    int i,value,magnitude;Sint16 *pcm=(Sint16 *)data;
    music_callback(music_context,data,len);
    for(i=0;i<len/2;++i){value=pcm[i];magnitude=abs(value);++samples;if(value)++audible;if(value==-32768||value==32767)++clipped;if(magnitude>peak)peak=magnitude;}
}
void __wrap_Mix_HookMusic(void (*callback)(void *,Uint8 *,int),void *context){
    __real_Mix_HookMusic(NULL,NULL);music_callback=callback;music_context=context;
    if(callback)__real_Mix_HookMusic(observe_audio,NULL);
}
int __wrap_SDL_PollEvent(SDL_Event *event){
    int found=__real_SDL_PollEvent(event),key;
    if(found&&event){
        if(event->type==SDL_ACTIVEEVENT&&!event->active.gain)memset(held,0,sizeof held);
        if(event->type==SDL_KEYDOWN||event->type==SDL_KEYUP){key=event->key.keysym.sym;
            if(key>=0&&key<SDLK_LAST){if(event->type==SDL_KEYUP)blocked[key]=0;held[key]=!blocked[key]&&event->type==SDL_KEYDOWN;}}
    }
    return found;
}
Uint8 *__wrap_SDL_GetKeyState(int *count){
    int i,n;Uint8 *real=__real_SDL_GetKeyState(&n);if(count)*count=SDLK_LAST;
    for(i=0;i<n&&i<SDLK_LAST;++i){if(!real[i]&&blocked[i])blocked[i]=0;if(real[i]&&!blocked[i])held[i]=1;if(blocked[i])held[i]=0;}
    return held;
}
void fox_key(int key,int down){SDL_Event event;memset(&event,0,sizeof event);event.type=down?SDL_KEYDOWN:SDL_KEYUP;event.key.keysym.sym=key;SDL_PushEvent(&event);}
void fox_request_quit(void){SDL_Event event;memset(&event,0,sizeof event);event.type=SDL_QUIT;SDL_PushEvent(&event);}
int fox_audio_active(void){int frequency,channels;Uint16 format;return Mix_QuerySpec(&frequency,&format,&channels)&&frequency>0&&channels==2;}
void fox_audio_stats(unsigned long long *count,unsigned long long *nonzero,unsigned long long *clips,int *max){*count=samples;*nonzero=audible;*clips=clipped;*max=peak;}
void fox_capture(const char *path){if(SDL_SaveBMP(screen,path)<0){fprintf(stderr,"Cannot capture Fox frame: %s\n",SDL_GetError());}}
void fox_pause_begin(void){SDL_PauseAudio(1);fox_paused(1);}
int fox_pause_end(int code){fox_paused(0);SDL_PauseAudio(0);return code;}
int fox_run(void){
    int ret,state=1,i,count;Uint8 *keys;
    SDL_putenv("SDL12COMPAT_WINDOW_SCALING=2");
    SDL_putenv("SDL12COMPAT_SCALE_METHOD=nearest");
    ret=init();if(ret<0)return ret;
    SDL_WM_SetCaption("Titus the Fox - Titus Engine",NULL);
    keys=__real_SDL_GetKeyState(&count);for(i=0;i<count&&i<SDLK_LAST;++i)blocked[i]=keys[i];
    /* Upstream intro text is Moktar-specific; Fox uses its own logo/title. */
    if(state){ret=viewimage(tituslogofile,tituslogoformat,0,4000);if(ret<0)state=0;}
    SELECT_MUSIC(15);
    if(state){ret=viewimage(titusintrofile,titusintroformat,0,6500);if(ret<0)state=0;}
    while(state){ret=viewmenu(titusmenufile,titusmenuformat);if(ret<=0)break;
        if(ret<=levelcount){ret=playtitus(ret-1);if(ret<0)break;}}
    freeaudio();freefonts();SDL_Quit();return ret < -1?ret:0;
}
