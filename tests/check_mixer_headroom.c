/* Exercise the actual mixer callback with two simultaneous full-scale buses. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../mixer.c"
#include <stdarg.h>
static sys_audio_cb callback;
static void *context;
static void start(sys_audio_cb cb,void *p){callback=cb;context=p;}
static void noop(void){}
struct sys_t g_sys={.start_audio=start,.stop_audio=noop,.lock_audio=noop,.unlock_audio=noop};
void print_debug(int mask,const char *format,...){(void)mask;(void)format;}
static void music(void *unused,int16_t *pcm,int count){for(int i=0;i<count;++i)pcm[i]=i%2?-32768:32767;}
static void render(int headroom,int limited){
    static const int8_t sound[]={127,-128,127,-128};int16_t pcm[4];
    g_mix.set_headroom(headroom);g_mix.init();g_mix.set_music_callback(music,0,0);
    g_mix.play_sound((const uint8_t *)sound,4,SYS_AUDIO_FREQ,0);callback(context,(uint8_t *)pcm,sizeof pcm);
    for(int i=0;i<4;++i){if(limited)assert(abs(pcm[i])<31500);else assert(pcm[i]==(i%2?-32768:32767));}
    g_mix.fini();
}
int main(void){render(100,0);render(48,1);render(100,0);puts("mixer: bounded P2 summation and unchanged default/restored headroom passed");return 0;}
