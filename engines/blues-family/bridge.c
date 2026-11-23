#include "native.h"
#include "sys.h"
#include "mixer.h"
#include "bb/game.h"
struct options_t g_options;
extern struct game_t game_bb;
void titus_native_options_init(const struct titus_native_options *o,int preferences){
    memset(&g_options,0,sizeof g_options);
    g_options.initial_lives[0]=g_options.initial_lives[1]=3;g_options.preferred_amiga=-1;
    g_options.start_xpos16=g_options.start_ypos16=-1;g_options.scroll_rate=60;
    g_options.music_volume=g_options.effects_volume=100;g_options.camera_mode=CAMERA_SMOOTH;
    g_options.amiga_copper_bars=g_options.amiga_colors=true;g_options.screen_w=320;g_options.screen_h=200;
    if(preferences&&g_sys.load_preferences)g_sys.load_preferences();
    g_options.dos_datapath=o->dos;g_options.amiga_datapath=o->amiga;g_options.preferred_amiga=o->edition;
    if(o->fullscreen>=0)g_options.fullscreen=o->fullscreen;
    if(o->music>=0)g_options.music_volume=o->music;if(o->effects>=0)g_options.effects_volume=o->effects;
    if(o->camera>=0)g_options.camera_mode=o->camera;if(o->hud>=0)g_options.hud_mode=o->hud;
    if(o->view>=0)g_sys.input.view_step=o->view;
    if(o->invulnerable>=0){g_options.cheats&=~CHEATS_INVULNERABLE;if(o->invulnerable)g_options.cheats|=CHEATS_INVULNERABLE;}
    if(o->infinite_lives>=0){g_options.cheats&=~CHEATS_UNLIMITED_LIFES;if(o->infinite_lives)g_options.cheats|=CHEATS_UNLIMITED_LIFES;}
    if(o->lives1>=0)g_options.initial_lives[0]=o->lives1;if(o->lives2>=0)g_options.initial_lives[1]=o->lives2;
    if(o->level>=0)g_options.start_level=o->level;if(o->sound>=0)g_options.sound_mode=o->sound;
}
int titus_native_run(void){
    const char* source=g_options.preferred_amiga?g_options.amiga_datapath:g_options.dos_datapath;
    g_sys.init();g_sys.set_screen_size(GAME_SCREEN_W,GAME_SCREEN_H,"The Blues Brothers — Titus Engine",2,0,g_options.fullscreen);
    g_sys.set_scroll_render_rate(g_options.scroll_rate);game_bb.res_init(source,GAME_SCREEN_W*GAME_SCREEN_H);
    game_bb.snd_init();g_mix.set_volumes(g_options.music_volume,g_options.effects_volume);g_mix.init();
    if(!titus_runtime_poll())game_bb.run();
    if(g_sys.save_preferences)g_sys.save_preferences();
    g_mix.fini();game_bb.snd_fini();game_bb.res_fini();g_sys.fini();return 0;
}
