/* Standalone probe for external OpenTitus; never linked into Blues.
 * Requires the pinned reference sources and SDL 1.2/SDL_mixer SDKs. */
#include <SDL/SDL.h>
#undef main
#define main original_main
#include "opentitus.c"
#undef main
#include "player.h"
#include "audio.h"
#include "level.h"
static Uint8 keys[SDLK_LAST];
static int ticks,bx,rx,lx,by,jy;
static unsigned long count,nonzero,clipped;
static int peak;
static FILE *pcm_output;
static void (*music_callback)(void*,Uint8*,int);
static void *music_context;
void __real_Mix_HookMusic(void (*callback)(void*,Uint8*,int),void *ctx);
static void audio_probe(void *ctx,Uint8 *data,int len) {
 int i,v;Sint16 *p=(Sint16*)data;
 music_callback(music_context,data,len);
 if(pcm_output&&fwrite(data,1,len,pcm_output)!=(size_t)len)abort();
 for(i=0;i<len/2;++i){v=abs(p[i]);++count;if(v)++nonzero;if(v>peak)peak=v;if(v>=32767)++clipped;}
}
void __wrap_Mix_HookMusic(void (*callback)(void*,Uint8*,int),void *ctx) {
 __real_Mix_HookMusic(NULL,NULL);
 music_callback=callback;music_context=ctx;
 if(callback)__real_Mix_HookMusic(audio_probe,NULL);
}
Uint8 *__wrap_SDL_GetKeyState(int *n) {if(n)*n=SDLK_LAST;return keys;}
int __wrap_viewstatus(TITUS_level *level,bool first){return 0;}
int __real_move_player(TITUS_level *level);
int __wrap_move_player(TITUS_level *level) {
 int x=level->player.sprite.x,y=level->player.sprite.y;
 memset(keys,0,sizeof(keys));
 if(ticks==10)bx=rx=lx=x;
 if(ticks>=10&&ticks<40){keys[SDLK_RIGHT]=1;if(x>rx)rx=x;}
 if(ticks==40)lx=x;
 if(ticks>=40&&ticks<65){keys[SDLK_LEFT]=1;if(x<lx)lx=x;}
 if(ticks==65)by=jy=y;
 if(ticks>=65&&ticks<95){keys[SDLK_UP]=1;if(y<jy)jy=y;}
 if(ticks>=95&&ticks<120)keys[SDLK_SPACE]=1;
 if(++ticks>=140){SDL_Event ev;memset(&ev,0,sizeof(ev));ev.type=SDL_QUIT;SDL_PushEvent(&ev);}
 return __real_move_player(level);
}
int __real_SDL_Flip(SDL_Surface *screen);
int __wrap_SDL_Flip(SDL_Surface *screen) {
 if(ticks==120&&getenv("TITUS_PROBE_SHOT")&&SDL_SaveBMP(screen,getenv("TITUS_PROBE_SHOT"))<0)abort();
 return __real_SDL_Flip(screen);
}
int main(void) {
 int result;
 if(getenv("TITUS_PROBE_PCM")){pcm_output=fopen(getenv("TITUS_PROBE_PCM"),"wb");if(!pcm_output)abort();}
 result=init();if(result<0)return 2;
 result=playtitus(0);
 freeaudio();freefonts();SDL_Quit();
 if(pcm_output)fclose(pcm_output);
 printf("fox-gameplay: ticks=%d right=%d left=%d jump=%d samples=%lu nonzero=%lu peak=%d clipped=%lu result=%d\n",ticks,rx-bx,rx-lx,by-jy,count,nonzero,peak,clipped,result);
 return ticks<140||rx<=bx||lx>=rx||jy>=by||nonzero<100;
}
