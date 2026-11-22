/* Asset-free configuration validation, replacement and reset checks. */
#include <unistd.h>
#include <sys/stat.h>
#include "../sys_sdl2.c"
struct options_t g_options;

static void defaults() {
	memset(&g_options,0,sizeof(g_options));
	g_options.camera_mode=CAMERA_SMOOTH; g_options.scroll_rate=60;
	g_options.music_volume=g_options.effects_volume=100;
	g_options.initial_lives[0]=g_options.initial_lives[1]=3;
	g_options.preferred_amiga=-1;
	g_sys.input.view_step=0;
	memcpy(_game_keys,_default_keys,sizeof(_game_keys));
	_game_buttons[0]=SDL_CONTROLLER_BUTTON_A; _game_buttons[1]=SDL_CONTROLLER_BUTTON_B;
}
static void write_config(const char *path,const char *text) {
	FILE *fp=fopen(path,"wb"); assert(fp);
	assert(fputs(text,fp)>=0 && fclose(fp)==0);
}
int main() {
	char directory[]="/tmp/blues-preferences-XXXXXX"; assert(mkdtemp(directory));
	char path[256]; snprintf(path,sizeof(path),"%s/preferences.ini",directory);
	SDL_setenv("BLUES_CONFIG_FILE",path,1);
	defaults(); sdl2_load_preferences(); assert(g_options.scroll_rate==60);
	write_config(path,"# test\nversion=1\nmode=amiga\ncamera=classic\nhud=fixed\nrender_rate=120\nview_step=6\nfullscreen=1\ncheats=7\ninitial_lives_1=17\ninitial_lives_2=9\nmusic_volume=40\neffects_volume=0\nkey_0=97\nkey_1=100\nkey_2=119\nkey_3=115\nkey_4=120\nkey_5=99\nbutton_0=2\nbutton_1=3\n");
	/* Loading must work before SDL/video initialization in main(). */
	sdl2_load_preferences();
	assert(_game_keys[0]==SDLK_a && _game_keys[4]==SDLK_x);
	assert(g_options.preferred_amiga==1 && g_options.camera_mode==CAMERA_CLASSIC && g_options.hud_mode==HUD_FIXED);
	assert(g_options.scroll_rate==120 && g_sys.input.view_step==6 && g_options.fullscreen);
	assert(g_options.initial_lives[0]==17 && g_options.initial_lives[1]==9 && g_options.cheats==7);
	assert(g_options.music_volume==40 && g_options.effects_volume==0);
	assert(_game_buttons[0]==2 && _game_buttons[1]==3);
	sdl2_save_preferences(); defaults(); sdl2_load_preferences();
	assert(_game_keys[0]==SDLK_a && _game_keys[5]==SDLK_c && _game_buttons[1]==3);
	assert(g_options.initial_lives[0]==17 && g_options.music_volume==40);
	/* A muted bus restores its previous chosen level after relaunch. */
	g_options.music_volume=0; _last_music_volume=35; sdl2_save_preferences();
	defaults(); _last_music_volume=100; sdl2_load_preferences();
	assert(g_options.music_volume==0 && _last_music_volume==35);
	/* Non-US printable bindings survive the same pre-video load. */
	defaults(); write_config(path,"version=1\nkey_4=241\n");
	sdl2_load_preferences(); assert(_game_keys[4]==241);
	sdl2_save_preferences(); defaults(); sdl2_load_preferences(); assert(_game_keys[4]==241);
	/* Invalid/overflowing values and unknown entries cannot corrupt defaults. */
	defaults();
	write_config(path,"version=1\nrender_rate=99\nview_step=-1\ncheats=999\nfullscreen=2\ninitial_lives_1=0\ninitial_lives_2=100\nmusic_volume=999999999999999999999999\neffects_volume=30junk\ncamera=broken\nhud=wrong\nkey_0=27\nbutton_0=6\nunknown=42\n");
	sdl2_load_preferences();
	assert(g_options.scroll_rate==60 && g_sys.input.view_step==0 && g_options.music_volume==100 && g_options.effects_volume==100);
	assert(g_options.initial_lives[0]==3 && g_options.initial_lives[1]==3 && !g_options.cheats && !g_options.fullscreen);
	assert(!memcmp(_game_keys,_default_keys,sizeof(_game_keys)) && _game_buttons[0]==0);
	write_config(path,"version=1\nkey_0=1073741903\n"); /* Duplicate default Right. */
	sdl2_load_preferences(); assert(!memcmp(_game_keys,_default_keys,sizeof(_game_keys)));
	write_config(path,"version=1\nbutton_0=1\n");
	sdl2_load_preferences(); assert(_game_buttons[0]==0 && _game_buttons[1]==1);
	/* Do not reinterpret an overlong tail as a valid setting. */
	FILE *fp=fopen(path,"wb"); assert(fp); fputs("version=1\n",fp);
	for (int i=0;i<300;++i) fputc('x',fp);
	fputs("music_volume=0\neffects_volume=20\n",fp); fclose(fp);
	sdl2_load_preferences(); assert(g_options.music_volume==100 && g_options.effects_volume==20);
	/* A future file must remain untouched until an explicit reset. */
	write_config(path,"version=99\nmusic_volume=0\n");
	defaults(); sdl2_load_preferences(); assert(g_options.music_volume==100 && !_preferences_writable);
	sdl2_save_preferences();
	char contents[128]; fp=fopen(path,"rb"); assert(fp); size_t n=fread(contents,1,127,fp); contents[n]=0; fclose(fp);
	assert(!strcmp(contents,"version=99\nmusic_volume=0\n"));
	/* Failure to create the temporary next to an existing configuration must
	 * leave both the original file and a pre-existing directory untouched. */
	allow_preferences_reset();
	char blocked[320]; snprintf(blocked,sizeof(blocked),"%s.tmp.%lu",path,(unsigned long)getpid());
	assert(mkdir(blocked,0700)==0); sdl2_save_preferences();
	fp=fopen(path,"rb"); assert(fp); n=fread(contents,1,127,fp); contents[n]=0; fclose(fp);
	assert(!strcmp(contents,"version=99\nmusic_volume=0\n") && rmdir(blocked)==0);
	_preferences_writable=false;
	/* Confirmation restores options and controls without resuming the game. */
	SDL_setenv("SDL_VIDEODRIVER","dummy",1); SDL_setenv("SDL_AUDIODRIVER","dummy",1);
	sdl2_init(); sdl2_set_screen_size(320,200,"Preferences",2,"nearest",false);
	_pause_options=(struct sys_pause_options){.can_switch=true,.amiga=true,.camera=CAMERA_CLASSIC,.hud=HUD_FIXED,.render_rate=120,.view_step=8,.music_volume=20,.effects_volume=0,.invulnerable=true,.infinite_lives=true,.lives={17,9}};
	_pause_page=PAUSE_OPTIONS; _pause_selection=8;
	assert(!pause_activate() && _pause_page==PAUSE_DEFAULTS && _pause_selection==0);
	assert(!pause_activate() && !_preferences_writable && !_pause_options.reset_defaults);
	assert(!pause_activate()); pause_move(1); assert(!pause_activate());
	assert(_pause_page==PAUSE_OPTIONS && _pause_options.reset_defaults && _preferences_writable);
	assert(_pause_options.camera==CAMERA_SMOOTH && _pause_options.hud==HUD_CLASSIC && _pause_options.render_rate==60);
	assert(!_pause_options.amiga && !_pause_options.invulnerable && !_pause_options.infinite_lives && !_pause_options.view_step);
	assert(_pause_options.music_volume==100 && _pause_options.effects_volume==100 && _pause_options.lives[0]==3);
	assert(!memcmp(_edit_keys,_default_keys,sizeof(_edit_keys)));
	/* A failed temporary-file creation must preserve the old file. */
	char invalid[256]; snprintf(invalid,sizeof(invalid),"%s/missing/preferences.ini",directory);
	SDL_setenv("BLUES_CONFIG_FILE",invalid,1); sdl2_save_preferences();
	assert(access(path,F_OK)==0);
	/* Save slots use the same UTF-8 and atomic replacement path. */
	SDL_setenv("BLUES_SAVE_DIR",directory,1);
	const uint8_t original[]={1,2,3},replacement[]={9,8,7,6};uint8_t loaded[16];size_t loaded_size;
	assert(sdl2_write_save_file(0,original,sizeof(original)));
	assert(!sdl2_write_save_file(-1,original,sizeof(original)) && !sdl2_write_save_file(8,original,sizeof(original)));
	snprintf(blocked,sizeof(blocked),"%s/slot01.bbs.tmp.%lu",directory,(unsigned long)getpid());
	assert(mkdir(blocked,0700)==0);assert(!sdl2_write_save_file(0,replacement,sizeof(replacement)));
	assert(sdl2_read_save_file(0,loaded,sizeof(loaded),&loaded_size) && loaded_size==sizeof(original) && !memcmp(loaded,original,loaded_size));
	assert(!sdl2_read_save_file(0,loaded,1,&loaded_size));assert(rmdir(blocked)==0);
	char slot_path[320];snprintf(slot_path,sizeof(slot_path),"%s/slot01.bbs",directory);remove(slot_path);
	sdl2_fini(); remove(path); rmdir(directory);
	puts("preferences: pre-SDL load, round trip, validation, future versions, atomic save failure and confirmed reset passed");
	return 0;
}
