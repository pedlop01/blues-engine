/* Asset-free tests of the production modal loop, rendering and input routing. */
#include "../sys_sdl2.c"

static int audio_callbacks;
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
	ev = key(SDLK_RETURN, true);
	assert(!pause_event(&ev) && _pause_page == PAUSE_CONTROLS);
	ev = key(SDLK_ESCAPE, false);
	assert(!pause_event(&ev) && _pause_page == PAUSE_MAIN && _pause_selection == 1);
	ev = key(SDLK_DOWN, true); pause_event(&ev);
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
	ev = (SDL_Event){0}; ev.type = SDL_CONTROLLERBUTTONDOWN; ev.cbutton.button = SDL_CONTROLLER_BUTTON_A;
	assert(!pause_event(&ev) && _pause_page == PAUSE_QUIT);
	ev.cbutton.button = SDL_CONTROLLER_BUTTON_B;
	assert(!pause_event(&ev) && _pause_page == PAUSE_MAIN);
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
	assert(!pause_event(&ev) && _pause_page == PAUSE_CONTROLS);
	ev.jbutton.button = 1; assert(!pause_event(&ev) && _pause_page == PAUSE_MAIN);
	ev = (SDL_Event){0}; ev.type = SDL_QUIT;
	assert(pause_event(&ev) && g_sys.input.quit);
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
	/* The cyan frame stays centered across resize and differing aspect ratios. */
	const int sizes[][2] = {{640,400},{800,600},{1280,720}};
	_pause_page = PAUSE_MAIN;
	for (unsigned i = 0; i < ARRAYSIZE(sizes); ++i) {
		SDL_SetWindowSize(_window, sizes[i][0], sizes[i][1]);
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
		int x1=w, x2=0, y1=h, y2=0;
		for (int y=0;y<h;++y) for (int x=0;x<w;++x) {
			if ((pixels[y*w+x]&0xffffff)!=0x45bdd5) continue;
			x1=MIN(x1,x); x2=MAX(x2,x); y1=MIN(y1,y); y2=MAX(y2,y);
		}
		assert(x1<x2 && y1<y2 && abs(x1+x2+1-w)<=2 && abs(y1+y2+1-h)<=2);
		free(pixels);
	}
	sdl2_stop_audio();
	free(bitmap);
	sdl2_fini();
	puts("pause menu: navigation, controls, safe quit, controller edges, input release gating, modal loop and audio status passed");
	return 0;
}
