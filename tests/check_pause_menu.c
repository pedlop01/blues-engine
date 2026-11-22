/* Asset-free tests of the production modal loop, rendering and input routing. */
#include "../sys_sdl2.c"
struct options_t g_options;

static int save_calls,load_calls,last_slot;
static bool load_ready;
static bool mock_save(int slot) {last_slot=slot;++save_calls;return true;}
static bool mock_load(int slot) {last_slot=slot;++load_calls;return load_ready;}
static const char *mock_save_error() {return "PRUEBA";}
static int audio_callbacks;
static int volume_applies;
static void test_apply_volumes(int music,int effects) {
	assert(SDL_GetAudioStatus()!=SDL_AUDIO_PLAYING);
	assert(music==90 && effects==0);
	++volume_applies;
}
static void test_audio(void *param, uint8_t *data, int len) {
	memset(data, 0, len);
	++audio_callbacks;
}

static SDL_Event key(int sym, bool down) {
	SDL_Event ev = {0};
	ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	ev.key.keysym.sym = sym;
	ev.key.keysym.scancode = SDL_GetScancodeFromKey(sym);
	return ev;
}
static void send_key(int sym, bool down) {
	SDL_Event ev = key(sym, down);
	assert(SDL_PushEvent(&ev) == 1);
}

static uint32_t close_menu_timer(uint32_t interval, void *param) {
	assert(SDL_GetAudioStatus() != SDL_AUDIO_PLAYING);
	send_key(SDLK_RETURN, true);
	return 0;
}

static void reset_menu() {
	_pause_page = PAUSE_MAIN;
	_pause_selection = _pause_axis = _pause_hat = 0;
	_pause_focused = true;
	g_sys.input.quit = false;
	clear_pause_input();
}

int main() {
	SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
	SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
	sdl2_init();
	sdl2_set_screen_size(320, 200, "Pause test", 2, "nearest", false);
	sdl2_set_pause_menu_enabled(true);
	bool paused = false;
	handle_keyevent(SDLK_ESCAPE, true, &g_sys.input, &paused);
	assert(!g_sys.input.quit && !g_sys.input.menu);
	handle_keyevent(SDLK_ESCAPE, false, &g_sys.input, &paused);
	assert(g_sys.input.menu && !g_sys.input.quit);
	g_sys.input.menu = false;
	handle_controllerbutton(SDL_CONTROLLER_BUTTON_START, false, &g_sys.input, &paused);
	assert(g_sys.input.menu && !paused);
	reset_menu();
	SDL_Event ev = key(SDLK_DOWN, true);
	assert(!pause_event(&ev) && _pause_selection == 1);
	pause_event(&ev);
	ev = key(SDLK_RETURN, true);
	assert(!pause_event(&ev) && _pause_page == PAUSE_BINDINGS);
	ev = key(SDLK_ESCAPE, false);
	assert(!pause_event(&ev) && _pause_page == PAUSE_MAIN && _pause_selection == 2);
	ev = key(SDLK_DOWN, true); pause_event(&ev); pause_event(&ev); pause_event(&ev); pause_event(&ev);
	ev = key(SDLK_RETURN, true);
	assert(!pause_event(&ev) && _pause_page == PAUSE_QUIT && _pause_selection == 0);
	ev.key.repeat = 1;
	assert(!pause_event(&ev) && !g_sys.input.quit);
	ev.key.repeat = 0;
	assert(!pause_event(&ev) && _pause_page == PAUSE_MAIN && !g_sys.input.quit);
	pause_event(&ev);
	ev = key(SDLK_DOWN, true); pause_event(&ev);
	ev = key(SDLK_RETURN, true);
	assert(pause_event(&ev) && g_sys.input.quit);
	reset_menu();
	ev = (SDL_Event){0}; ev.type = SDL_CONTROLLERAXISMOTION;
	ev.caxis.axis = SDL_CONTROLLER_AXIS_LEFTY; ev.caxis.value = 24000;
	assert(!pause_event(&ev) && _pause_selection == 1);
	assert(!pause_event(&ev) && _pause_selection == 1); /* Must return to center. */
	ev.caxis.value = 0; pause_event(&ev);
	ev.caxis.value = 24000; pause_event(&ev);
	assert(_pause_selection == 2);
	ev.caxis.value = 0; pause_event(&ev);
	ev.caxis.value = 24000; pause_event(&ev);
	ev.caxis.value = 0; pause_event(&ev);
	ev.caxis.value = 24000; pause_event(&ev);
	ev.caxis.value = 0; pause_event(&ev);
	ev.caxis.value = 24000; pause_event(&ev);
	ev.caxis.value = 0; pause_event(&ev);
	ev.caxis.value = 24000; pause_event(&ev);
	ev = (SDL_Event){0}; ev.type = SDL_CONTROLLERBUTTONDOWN; ev.cbutton.button = SDL_CONTROLLER_BUTTON_A;
	assert(!pause_event(&ev) && _pause_page == PAUSE_QUIT);
	ev.cbutton.button = SDL_CONTROLLER_BUTTON_B;
	assert(!pause_event(&ev) && _pause_page == PAUSE_MAIN);

 /* Level selection requires a second deliberate confirmation; cancellation
  * and missing data never produce a pending jump. Same-level restart works. */
 for (int level=0;level<6;++level) {
  reset_menu(); _pause_options.current_level=level; _pause_options.requested_level=-1;
  _pause_options.amiga=false;
  _pause_selection=3; assert(!pause_activate() && _pause_page==PAUSE_LEVELS && _pause_selection==level);
  _pause_options.available_levels[0][level]=false;
  assert(!pause_activate() && _pause_page==PAUSE_LEVELS && _pause_options.requested_level==-1);
  _pause_options.available_levels[0][level]=true;
  assert(!pause_activate() && _pause_page==PAUSE_LEVEL_CONFIRM && _pause_selection==0);
  assert(!pause_activate() && _pause_page==PAUSE_LEVELS && _pause_options.requested_level==-1);
  assert(!pause_activate() && _pause_page==PAUSE_LEVEL_CONFIRM);
  pause_move(1); assert(pause_activate() && _pause_options.requested_level==level);
 }
 reset_menu();
 reset_menu(); _pause_options.select_character=false;
 _pause_selection=5;
 assert(!pause_activate() && _pause_page==PAUSE_CHARACTER_CONFIRM && _pause_selection==0);
 assert(!pause_activate() && _pause_page==PAUSE_MAIN && _pause_selection==5 && !_pause_options.select_character);
 assert(!pause_activate()); pause_move(1);
 assert(pause_activate() && _pause_options.select_character);
 reset_menu();
 /* Slot browser guards empty/incompatible entries and routes the chosen slot. */
 reset_menu();_pause_options.save_game=mock_save;_pause_options.prepare_load=mock_load;
 _pause_options.save_error=mock_save_error;_pause_options.load_game=false;_save_slot=0;
 _pause_selection=4;assert(!pause_activate() && _pause_page==PAUSE_GAME);
 assert(!pause_activate() && _pause_page==PAUSE_SLOTS && pause_item_count()==9);
 _pause_selection=7;assert(!pause_activate() && _pause_page==PAUSE_SAVE_CONFIRM && _pause_selection==0);
 assert(!pause_activate() && save_calls==0 && _pause_selection==7);
 assert(!pause_activate());pause_move(1);assert(!pause_activate() && save_calls==1 && last_slot==7 && _pause_page==PAUSE_SLOTS);
 pause_back();pause_move(1);assert(!pause_activate() && _pause_page==PAUSE_SLOTS);
 for(int status=SAVE_EMPTY;status<=SAVE_UNREADABLE;++status) if(status!=SAVE_READY) {
  _save_slots[7].status=status;assert(!pause_activate() && load_calls==0 && !_pause_options.load_game && _pause_page==PAUSE_SLOTS);
 }
 _save_slots[7].status=SAVE_READY;
 assert(!pause_activate() && _pause_page==PAUSE_LOAD_CONFIRM);
 assert(!pause_activate() && load_calls==0);
 assert(!pause_activate());pause_move(1);assert(!pause_activate() && load_calls==1 && !_pause_options.load_game && _pause_page==PAUSE_SLOTS);
 load_ready=true;_save_slots[7].status=SAVE_READY;assert(!pause_activate());pause_move(1);assert(pause_activate() && load_calls==2 && last_slot==7 && _pause_options.load_game);
 reset_menu();
	/* Focus loss cannot activate a menu entry or leak a window close. */
	reset_menu();
	ev = (SDL_Event){0}; ev.type = SDL_WINDOWEVENT; ev.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
	pause_event(&ev);
	ev = key(SDLK_RETURN, true); assert(!pause_event(&ev));
	ev = (SDL_Event){0}; ev.type = SDL_WINDOWEVENT; ev.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
	pause_event(&ev);
	ev = key(SDLK_RETURN, true); assert(pause_event(&ev));
	reset_menu();
	ev = (SDL_Event){0}; ev.type = SDL_JOYHATMOTION; ev.jhat.value = SDL_HAT_DOWN;
	pause_event(&ev); pause_event(&ev); assert(_pause_selection == 1);
	ev.jhat.value = SDL_HAT_CENTERED; pause_event(&ev);
	ev = (SDL_Event){0}; ev.type = SDL_JOYBUTTONDOWN; ev.jbutton.button = 0;
	assert(!pause_event(&ev) && _pause_page == PAUSE_OPTIONS);
	ev.jbutton.button = 1; assert(!pause_event(&ev) && _pause_page == PAUSE_MAIN);
	ev = (SDL_Event){0}; ev.type = SDL_QUIT;
	assert(pause_event(&ev) && g_sys.input.quit);
	reset_menu();
	/* Controls has one main-menu entry; Options ends directly at Back. */
	_pause_page=PAUSE_OPTIONS; _pause_selection=9;
	assert(pause_item_count()==10 && !pause_activate() && _pause_page==PAUSE_MAIN && _pause_selection==1);
	_pause_selection=2; pause_activate();
	assert(_pause_page==PAUSE_BINDINGS && pause_item_count()==5);
	_pause_selection=2; pause_activate(); assert(_pause_page==PAUSE_CONTROLS);
	pause_back(); assert(_pause_page==PAUSE_BINDINGS && _pause_selection==2);
	pause_back(); assert(_pause_page==PAUSE_MAIN && _pause_selection==2);
	/* General settings are staged; missing data never permits a switch. */
	reset_menu();
	struct sys_pause_options options = {.camera=CAMERA_SMOOTH,.hud=HUD_CLASSIC,.render_rate=60};
	sdl2_set_pause_options(&options);
	_pause_page=PAUSE_OPTIONS; _pause_selection=0;
	pause_adjust(1); assert(!_pause_options.amiga && _pause_notice);
	_pause_options.can_switch=true; pause_adjust(1); assert(_pause_options.amiga);
	_pause_selection=1; pause_adjust(-1); assert(_pause_options.camera==CAMERA_CLASSIC);
	_pause_selection=2; pause_adjust(1); assert(_pause_options.hud==HUD_FIXED);
	_pause_selection=3; pause_adjust(1); assert(SDL_GetWindowFlags(_window)&SDL_WINDOW_FULLSCREEN_DESKTOP);
	pause_adjust(-1); assert(!(SDL_GetWindowFlags(_window)&SDL_WINDOW_FULLSCREEN_DESKTOP));
	_pause_selection=4; pause_adjust(1); assert(_pause_options.render_rate==120);
	pause_adjust(1); assert(_pause_options.render_rate==30);
	_pause_selection=5; pause_adjust(-1); assert(_pause_options.view_step==0);
	for (int i=0; i<12; ++i) pause_adjust(1);
	assert(_pause_options.view_step==8);
	_pause_page=PAUSE_OPTIONS; _pause_selection=6; pause_activate();
	assert(_pause_page==PAUSE_AUDIO && pause_item_count()==3);
	_pause_options.music_volume=100; _pause_options.effects_volume=40;
	_pause_selection=0; pause_adjust(-1); assert(_pause_options.music_volume==90 && _pause_options.effects_volume==40);
	pause_activate(); assert(_pause_options.music_volume==0);
	pause_activate(); assert(_pause_options.music_volume==90);
	_pause_selection=1; pause_activate(); assert(_pause_options.effects_volume==0 && _pause_options.music_volume==90);
	pause_activate(); assert(_pause_options.effects_volume==40);
	for(int i=0;i<15;++i)pause_adjust(-1);
	assert(_pause_options.effects_volume==0);
	_pause_selection=2; pause_activate(); assert(_pause_page==PAUSE_OPTIONS && _pause_selection==6);
	_pause_page=PAUSE_OPTIONS; _pause_selection=7; pause_activate();
	assert(_pause_page==PAUSE_ASSISTS && pause_item_count()==5);
	_pause_options.invulnerable=false; _pause_options.infinite_lives=false;
	_pause_options.player_alive[0]=true; _pause_options.player_alive[1]=false;
	_pause_options.lives[0]=3; _pause_options.lives[1]=3;
	_pause_selection=0; pause_activate(); assert(_pause_options.invulnerable && !_pause_options.infinite_lives);
	_pause_selection=1; pause_adjust(1); assert(_pause_options.infinite_lives);
	_pause_selection=2; pause_adjust(-1); assert(_pause_options.lives[0]==2);
	for(int i=0;i<110;++i)pause_adjust(1);
	assert(_pause_options.lives[0]==99);
	for(int i=0;i<110;++i)pause_adjust(-1);
	assert(_pause_options.lives[0]==1);
	_pause_selection=3; pause_adjust(1); assert(_pause_options.lives[1]==3);
	_pause_options.player_alive[1]=true; pause_adjust(1); assert(_pause_options.lives[1]==4);
	_pause_selection=4; pause_activate(); assert(_pause_page==PAUSE_OPTIONS && _pause_selection==7);
	/* The activation key/button release must not bind itself. */
	_pause_page=PAUSE_KEYS; _pause_selection=BIND_ACTION;
	ev=key(SDLK_RETURN,true); pause_event(&ev); assert(_pause_capture==BIND_ACTION);
	ev=key(SDLK_RETURN,false); pause_event(&ev); assert(_pause_capture==BIND_ACTION && !_pause_notice);
	ev=key(SDLK_SPACE,false); pause_event(&ev); assert(_pause_capture==BIND_ACTION); /* Old held key release. */
	ev=key(SDLK_SPACE,true); ev.key.repeat=1; pause_event(&ev);
	ev=key(SDLK_SPACE,false); pause_event(&ev); assert(_pause_capture==BIND_ACTION);
	ev=key(SDLK_F6,true); pause_event(&ev);
	ev=key(SDLK_F6,false); pause_event(&ev); assert(_pause_capture==BIND_ACTION && _pause_notice);
	ev=key(SDLK_x,true); pause_event(&ev);
	ev=key(SDLK_x,false); pause_event(&ev); assert(_pause_capture==-1 && _edit_keys[BIND_ACTION]==SDLK_x);
	assert(_game_keys[BIND_ACTION]==SDLK_SPACE);
	/* Duplicate keys swap, so no action loses its binding. */
	_pause_selection=BIND_LEFT; pause_activate();
	ev=key(SDLK_RIGHT,true); pause_event(&ev);
	ev=key(SDLK_RIGHT,false); pause_event(&ev);
	assert(_edit_keys[BIND_LEFT]==SDLK_RIGHT && _edit_keys[BIND_RIGHT]==SDLK_LEFT);
	_pause_page=PAUSE_PAD; _pause_selection=1;
	ev=(SDL_Event){0}; ev.type=SDL_CONTROLLERBUTTONDOWN; ev.cbutton.button=SDL_CONTROLLER_BUTTON_A;
	pause_event(&ev); assert(_pause_capture==1);
	ev.type=SDL_CONTROLLERBUTTONUP; pause_event(&ev); assert(_pause_capture==1);
	ev.cbutton.button=SDL_CONTROLLER_BUTTON_X; ev.type=SDL_CONTROLLERBUTTONDOWN; pause_event(&ev);
	ev.type=SDL_CONTROLLERBUTTONUP; pause_event(&ev);
	assert(_pause_capture==-1 && _edit_buttons[1]==SDL_CONTROLLER_BUTTON_X);
	_pause_page=PAUSE_BINDINGS; _pause_selection=3; pause_activate();
	assert(memcmp(_edit_keys,_default_keys,sizeof(_edit_keys))==0 && _edit_buttons[1]==SDL_CONTROLLER_BUTTON_B);
	reset_menu();
	/* Menu-held input is suppressed until released. */
	reset_menu();
	ev = key(SDLK_RIGHT, true); pause_event(&ev); handle_event(&ev, &paused);
	assert(!g_sys.input.direction);
	ev = key(SDLK_RIGHT, false); handle_event(&ev, &paused);
	ev = key(SDLK_RIGHT, true); handle_event(&ev, &paused);
	assert(g_sys.input.direction == INPUT_DIRECTION_RIGHT);
	clear_pause_input();
	ev = key(SDLK_F6, true); pause_event(&ev);
	ev = key(SDLK_F6, false); handle_event(&ev, &paused);
	assert(!g_sys.input.switch_mode);
	/* Real modal loop: audio stops, frame pixels remain intact, and resume
	 * does not leak Enter into gameplay. The timer only posts SDL events. */
	uint8_t *bitmap = calloc(320 * 200, 1);
	assert(bitmap);
	sdl2_copy_bitmap(bitmap, 320, 200);
	sdl2_start_audio(test_audio, NULL);
	_pause_options.music_volume=90; _pause_options.effects_volume=0;
	_pause_options.apply_volumes=test_apply_volumes;
	for (int hud = 0; hud < 2; ++hud) {
		sdl2_set_hud_layout(hud ? 320 : 0, 200, 14, 26);
		sdl2_copy_bitmap(bitmap, 320, 200);
		assert(SDL_AddTimer(60, close_menu_timer, NULL));
		sdl2_run_pause_menu();
		assert(!_pause_menu_open && !g_sys.input.space && !g_sys.input.jump && !g_sys.input.direction);
		assert(SDL_GetAudioStatus() == SDL_AUDIO_PLAYING);
		ev = key(SDLK_RETURN, false); handle_event(&ev, &paused);
	}
	SDL_PauseAudio(1);
	const int callbacks = audio_callbacks;
	assert(SDL_AddTimer(50, close_menu_timer, NULL));
	sdl2_run_pause_menu();
	assert(SDL_GetAudioStatus() == SDL_AUDIO_PAUSED && audio_callbacks == callbacks);
	assert(volume_applies==3);
	memset(_save_slots,0,sizeof(_save_slots));
	_save_slots[0].status=SAVE_READY;_save_slots[0].has_preview=_save_slots[0].has_metadata=true;
	_save_slots[0].level=1;_save_slots[0].lives[0]=7;_save_slots[0].timestamp=(uint64_t)time(NULL);
	for(int i=0;i<SAVE_PREVIEW_BYTES;i+=3){_save_slots[0].preview[i]=200;_save_slots[0].preview[i+1]=40;_save_slots[0].preview[i+2]=160;}
	_save_slots[1].status=SAVE_DAMAGED;_save_slots[2].status=SAVE_INCOMPATIBLE;_save_slots[3].status=SAVE_UNREADABLE;
	_preview_texture_slot=-1;
	/* The cyan frame stays centered across resize and differing aspect ratios. */
	const int sizes[][2] = {{320,200},{640,400},{800,600},{1280,720},{853,533},{0,0}};
	_pause_page = PAUSE_MAIN;
	for (unsigned i = 0; i < ARRAYSIZE(sizes); ++i) {
		if (sizes[i][0]) SDL_SetWindowSize(_window, sizes[i][0], sizes[i][1]);
		else assert(SDL_SetWindowFullscreen(_window, SDL_WINDOW_FULLSCREEN_DESKTOP) == 0);
		for (int page = 0; page <= PAUSE_SLOTS+1; ++page) {
			_pause_page = page == PAUSE_SLOTS+1 ? PAUSE_QUIT : (enum pause_page)page;
			_pause_selection = page == PAUSE_SLOTS+1 ? 1 : 0;
			_pause_menu_open = true;
			sdl2_draw_screen(0,0,1);
			_pause_menu_open = false;
			int w, h;
			SDL_GetRendererOutputSize(_renderer, &w, &h);
			uint32_t *pixels = malloc((size_t)w*h*4);
			assert(pixels);
			SDL_RenderSetLogicalSize(_renderer,0,0);
			assert(SDL_RenderReadPixels(_renderer,NULL,SDL_PIXELFORMAT_ARGB8888,pixels,w*4)==0);
			SDL_RenderSetLogicalSize(_renderer,_screen_w,_screen_h);
			int x1=w, x2=0, y1=h, y2=0, gold=0, danger=0, preview_pixels=0;
			for (int y=0;y<h;++y) for (int x=0;x<w;++x) {
				const uint32_t color = pixels[y*w+x]&0xffffff;
				if (color == 0xffd768) ++gold;
				if (color == 0xff8477) ++danger;
				if (color == 0xc828a0) ++preview_pixels;
				if (color!=0x45bdd5) continue;
				x1=MIN(x1,x); x2=MAX(x2,x); y1=MIN(y1,y); y2=MAX(y2,y);
			}
			assert(x1<x2 && y1<y2 && abs(x1+x2+1-w)<=2 && abs(y1+y2+1-h)<=2);
			assert(x1>=0 && x2<w && y1>=0 && y2<h);
			const int scale = MIN(w/320,h/200);
			assert(scale >= 1 && gold > 0 && gold % (scale*scale) == 0);
			assert((danger > 0) == (page == PAUSE_SLOTS+1 || page == PAUSE_SLOTS));
			if(page==PAUSE_SLOTS)assert(preview_pixels==SAVE_PREVIEW_WIDTH*SAVE_PREVIEW_HEIGHT*scale*scale);
			free(pixels);
		}
	}
	sdl2_stop_audio();
	free(bitmap);
	sdl2_fini();
	puts("pause menu: navigation, controls, safe quit, controller edges, input release gating, modal loop and audio status passed");
	return 0;
}
