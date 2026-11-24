#include "native.h"
#include "sys.h"
#include "mixer.h"
#include "bb/game.h"
struct options_t g_options;
extern struct game_t game_bb,game_ja,game_p2;
static struct game_t *game;
static int load_slot;
int titus_native_load_slot(void){return load_slot;}
static int probe_enabled,effect_count,peak;
static uint64_t samples,nonzero,clipped;
static void (*start_audio)(sys_audio_cb,void *);
static sys_audio_cb audio_callback;
static void *audio_context;
static void (*engine_play_sound)(const uint8_t *,uint32_t,int,mix_repeat_sound_cb);
static void observe_audio(void *context,uint8_t *buffer,int size){
    audio_callback(audio_context,buffer,size);
    const int16_t *pcm=(const int16_t *)buffer;
    for(int i=0;i<size/2;++i){int value=pcm[i],magnitude=abs(value);++samples;if(value)++nonzero;if(value==-32768||value==32767)++clipped;if(magnitude>peak)peak=magnitude;}
}
static void observe_start(sys_audio_cb callback,void *context){audio_callback=callback;audio_context=context;start_audio(observe_audio,0);}
static void observe_sound(const uint8_t *data,uint32_t size,int freq,mix_repeat_sound_cb repeat){if(data&&size)++effect_count;engine_play_sound(data,size,freq,repeat);}
void titus_native_audio_stats(uint64_t *count,uint64_t *audible,uint64_t *clips,int *max,int *effects){*count=samples;*audible=nonzero;*clips=clipped;*max=peak;*effects=effect_count;}

void titus_native_options_init(const struct titus_native_options *o,int preferences){
    probe_enabled=o->probe;load_slot=o->load_slot;
    game=o->game==1?&game_ja:o->game==2?&game_p2:&game_bb;
    memset(&g_options,0,sizeof g_options);
    g_options.initial_lives[0]=g_options.initial_lives[1]=3;g_options.preferred_amiga=-1;
    g_options.start_xpos16=g_options.start_ypos16=-1;g_options.scroll_rate=60;
    g_options.music_volume=g_options.effects_volume=100;g_options.camera_mode=CAMERA_SMOOTH;
    g_options.amiga_copper_bars=g_options.amiga_colors=true;g_options.screen_w=320;g_options.screen_h=200;
    if(preferences&&g_sys.load_preferences)g_sys.load_preferences();
    titus_native_set_buttons(o->action_button,o->jump_button);
    if(o->game)g_options.jump_button=true;
    g_options.dos_datapath=o->dos;g_options.amiga_datapath=o->amiga;g_options.preferred_amiga=o->edition;
    if(o->fullscreen>=0)g_options.fullscreen=o->fullscreen;
    if(o->music>=0)g_options.music_volume=o->music;if(o->effects>=0)g_options.effects_volume=o->effects;
    if(o->camera>=0)g_options.camera_mode=o->camera;if(o->hud>=0)g_options.hud_mode=o->hud;
    if(o->view>=0)g_sys.input.view_step=o->view;
    if(o->rate>=0)g_options.scroll_rate=o->rate;
    if(o->invulnerable>=0){g_options.cheats&=~CHEATS_INVULNERABLE;if(o->invulnerable)g_options.cheats|=CHEATS_INVULNERABLE;}
    if(o->infinite_lives>=0)g_options.family_infinite_lives=o->infinite_lives;
    if(o->game!=1&&o->infinite_lives>=0){g_options.cheats&=~CHEATS_UNLIMITED_LIFES;if(o->infinite_lives)g_options.cheats|=CHEATS_UNLIMITED_LIFES;}
    if(o->lives1>=0)g_options.initial_lives[0]=o->lives1;if(o->lives2>=0)g_options.initial_lives[1]=o->lives2;
    if(o->level>=0)g_options.start_level=o->level;if(o->sound>=0)g_options.sound_mode=o->sound;
}
int titus_native_run(void){
    const char* source=g_options.preferred_amiga?g_options.amiga_datapath:g_options.dos_datapath;
    g_sys.init();g_sys.set_screen_size(GAME_SCREEN_W,GAME_SCREEN_H,game->name,2,0,g_options.fullscreen);
    g_sys.set_scroll_render_rate(g_options.scroll_rate);game->res_init(source,GAME_SCREEN_W*GAME_SCREEN_H);
    game->snd_init();
    if(probe_enabled){start_audio=g_sys.start_audio;g_sys.start_audio=observe_start;engine_play_sound=g_mix.play_sound;g_mix.play_sound=observe_sound;}
    g_mix.set_volumes(g_options.music_volume,g_options.effects_volume);g_mix.init();
    if(!titus_runtime_poll())game->run();
    if(g_sys.save_preferences)g_sys.save_preferences();
    g_mix.fini();game->snd_fini();game->res_fini();g_sys.fini();return 0;
}

void titus_native_preferences(int *fullscreen,int *music,int *effects){*fullscreen=g_options.fullscreen;*music=g_options.music_volume;*effects=g_options.effects_volume;}

void titus_native_family_options(struct titus_native_options *o){
    o->camera=g_options.camera_mode;o->hud=g_options.hud_mode;o->view=g_sys.input.view_step;
    o->rate=g_options.scroll_rate;o->invulnerable=(g_options.cheats&CHEATS_NO_HIT)!=0;
    o->infinite_lives=g_options.family_infinite_lives;o->lives1=g_options.initial_lives[0];
}

extern int titus_blues_prepare_load(int slot);
extern int titus_blues_preview(const uint8_t*,size_t,uint8_t*);
extern void titus_blues_inspect(const uint8_t*,size_t,struct titus_slot_info*);
int titus_native_prepare_load(int slot){return game->save_prepare?game->save_prepare(slot-1):titus_blues_prepare_load(slot);}
int titus_native_preview(const uint8_t *data,size_t size,uint8_t *rgb){return game->save_preview?game->save_preview(data,size,rgb):titus_blues_preview(data,size,rgb);}
void titus_native_inspect(const uint8_t *data,size_t size,struct titus_slot_info *info){if(game->save_inspect)game->save_inspect(data,size,info);else titus_blues_inspect(data,size,info);}
