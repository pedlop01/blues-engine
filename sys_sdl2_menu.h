/* SDL pause-menu implementation. Kept private to sys_sdl2.c. */
enum pause_page { PAUSE_MAIN, PAUSE_CONTROLS, PAUSE_QUIT };
static enum pause_page _pause_page;
static int _pause_selection, _pause_axis, _pause_hat;
static bool _pause_focused;

/* A small built-in bitmap font keeps this foundation asset-independent. */
static const struct { char c; uint8_t rows[7]; } _pause_font[] = {
 {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
 {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
 {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
 {'G',{14,17,16,23,17,17,15}}, {'H',{17,17,17,31,17,17,17}},
 {'I',{14,4,4,4,4,4,14}}, {'J',{7,2,2,2,18,18,12}},
 {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
 {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
 {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
 {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
 {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
 {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
 {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
 {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
 {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
 {'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
 {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
 {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
 {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
 {':',{0,4,4,0,4,4,0}}, {'/',{1,2,2,4,8,8,16}},
 {'-',{0,0,0,31,0,0,0}}, {'+',{0,4,4,31,4,4,0}},
 {'>',{16,8,4,2,4,8,16}}, {'?',{14,17,1,2,4,0,4}}
};

static const SDL_Color PAUSE_TEXT = {221, 231, 245, 255};
static const SDL_Color PAUSE_GOLD = {255, 215, 104, 255};
static const SDL_Color PAUSE_MUTED = {146, 171, 191, 255};
static const SDL_Color PAUSE_CYAN = {105, 207, 223, 255};
static const SDL_Color PAUSE_DANGER = {255, 132, 119, 255};

static void pause_text_pass(const char *text, int x, int y, int scale) {
	for (; *text; ++text, x += 6 * scale) {
		for (unsigned i = 0; i < ARRAYSIZE(_pause_font); ++i) {
			if (_pause_font[i].c != *text) continue;
			for (int row = 0; row < 7; ++row) {
				for (int col = 0; col < 5; ++col) {
					if (!(_pause_font[i].rows[row] & (1 << (4 - col)))) continue;
					SDL_Rect pixel = {x + col * scale, y + row * scale, scale, scale};
					SDL_RenderFillRect(_renderer, &pixel);
				}
			}
			break;
		}
	}
}

static void pause_text_color(const char *text, int x, int y, int scale, SDL_Color color) {
	SDL_SetRenderDrawColor(_renderer, 4, 10, 20, 255);
	pause_text_pass(text, x + 1, y + 1, scale);
	SDL_SetRenderDrawColor(_renderer, color.r, color.g, color.b, color.a);
	pause_text_pass(text, x, y, scale);
}

static void pause_text(const char *text, int x, int y, int scale, bool selected) {
	pause_text_color(text, x, y, scale, selected ? PAUSE_GOLD : PAUSE_TEXT);
}

static void pause_center_text(const char *text, int y, int scale, bool selected) {
	pause_text(text, (320 - ((int)strlen(text) * 6 - 1) * scale) / 2, y, scale, selected);
}

static void sdl2_draw_pause_menu() {
	SDL_BlendMode blend;
	SDL_GetRenderDrawBlendMode(_renderer, &blend);
	SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_BLEND);
	SDL_RenderSetLogicalSize(_renderer, 0, 0);
	SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 155);
	SDL_RenderFillRect(_renderer, NULL);
	/* Whole-pixel scaling keeps bitmap strokes uniform at fractional window
	 * sizes. A smaller output still fits instead of clipping the dialog. */
	int w, h;
	SDL_GetRendererOutputSize(_renderer, &w, &h);
	const float sx = w / 320.f, sy = h / 200.f;
	float scale = sx < sy ? sx : sy;
	if (scale >= 1.f) scale = (int)scale;
	SDL_Rect viewport = {(int)((w - 320 * scale) / 2.f), (int)((h - 200 * scale) / 2.f),
		(int)(320 * scale), (int)(200 * scale)};
	SDL_RenderSetViewport(_renderer, &viewport);
	SDL_RenderSetScale(_renderer, scale, scale);
	SDL_Rect box = _pause_page == PAUSE_CONTROLS
		? (SDL_Rect){12, 13, 296, 174} : (SDL_Rect){28, 31, 264, 138};
	SDL_Rect shadow = {box.x + 4, box.y + 4, box.w, box.h};
	SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 155);
	SDL_RenderFillRect(_renderer, &shadow);
	SDL_SetRenderDrawColor(_renderer, 13, 22, 40, 255);
	SDL_RenderFillRect(_renderer, &box);
	SDL_SetRenderDrawColor(_renderer, 69, 189, 213, 255);
	SDL_RenderDrawRect(_renderer, &box);
	box.x += 3; box.y += 3; box.w -= 6; box.h -= 6;
	SDL_SetRenderDrawColor(_renderer, 38, 68, 91, 255);
	SDL_RenderDrawRect(_renderer, &box);
	if (_pause_page == PAUSE_CONTROLS) {
		pause_center_text("CONTROLES", 26, 2, true);
		SDL_SetRenderDrawColor(_renderer, 38, 68, 91, 255);
		SDL_RenderDrawLine(_renderer, 26, 45, 293, 45);
		const struct { const char *action, *binding; } lines[] = {
			{"MOVER", "FLECHAS / STICK / CRUCETA"},
			{"ACCION", "ESPACIO O ENTER / A"},
			{"SALTAR", "SHIFT / B"},
			{"PAUSA", "ESC O P / START"},
			{"ZOOM", "- / + / 0"},
			{"DOS/AMIGA", "F6"},
			{"HUD", "F7: CLASICO / FIJO"}
		};
		for (unsigned i = 0; i < ARRAYSIZE(lines); ++i) {
			pause_text_color(lines[i].action, 26, 53 + i * 14, 1, PAUSE_CYAN);
			pause_text_color(lines[i].binding, 100, 53 + i * 14, 1, PAUSE_TEXT);
		}
		SDL_SetRenderDrawColor(_renderer, 38, 68, 91, 255);
		SDL_RenderDrawLine(_renderer, 26, 158, 293, 158);
		pause_center_text("ENTER / A / ESC / B: VOLVER", 169, 1, true);
	} else {
		pause_center_text(_pause_page == PAUSE_QUIT ? "SALIR DEL JUEGO?" : "PAUSA", 44,
			2, true);
		if (_pause_page == PAUSE_QUIT) {
			pause_center_text("CONFIRMA PARA SALIR", 65, 1, false);
		} else {
			SDL_SetRenderDrawColor(_renderer, 38, 68, 91, 255);
			SDL_RenderDrawLine(_renderer, 43, 65, 276, 65);
		}
		const char *main_items[] = {"CONTINUAR", "CONTROLES", "SALIR DEL JUEGO"};
		const char *quit_items[] = {"VOLVER", "SALIR"};
		const int count = _pause_page == PAUSE_QUIT ? 2 : 3;
		for (int i = 0; i < count; ++i) {
			const int y = (_pause_page == PAUSE_QUIT ? 87 : 77) + i * 21;
			const SDL_Color accent = _pause_page == PAUSE_QUIT && i == 1 ? PAUSE_DANGER : PAUSE_GOLD;
			if (i == _pause_selection) {
				SDL_Rect selected = {43, y - 5, 234, 17};
				SDL_SetRenderDrawColor(_renderer, 29, 55, 77, 255);
				SDL_RenderFillRect(_renderer, &selected);
				SDL_Rect stripe = {43, y - 5, 2, 17};
				SDL_SetRenderDrawColor(_renderer, accent.r, accent.g, accent.b, 255);
				SDL_RenderFillRect(_renderer, &stripe);
				pause_text_color(">", 51, y, 1, accent);
			}
			pause_text_color(_pause_page == PAUSE_QUIT ? quit_items[i] : main_items[i], 69, y, 1,
				i == _pause_selection ? accent : PAUSE_TEXT);
		}
		pause_text_color("FLECHAS / STICK   ENTER / A", 85, 145, 1, PAUSE_MUTED);
		pause_text_color("ESC / P / START: VOLVER", 94, 156, 1, PAUSE_MUTED);
	}
	SDL_RenderSetScale(_renderer, 1.f, 1.f);
	SDL_RenderSetViewport(_renderer, NULL);
	SDL_SetRenderDrawBlendMode(_renderer, blend);
	SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 255);
	SDL_RenderSetLogicalSize(_renderer, _screen_w, _screen_h);
}

static void clear_pause_input() {
	g_sys.input.direction = 0;
	g_sys.input.space = g_sys.input.jump = false;
	g_sys.input.digit1 = g_sys.input.digit2 = g_sys.input.digit3 = false;
	g_sys.input.switch_mode = g_sys.input.switch_hud = g_sys.input.menu = false;
}

static void sdl2_set_pause_menu_enabled(bool enabled) {
	_pause_menu_enabled = enabled;
	g_sys.input.menu = false;
}

static bool pause_back() {
	if (_pause_page == PAUSE_MAIN) return true;
	_pause_selection = _pause_page == PAUSE_CONTROLS ? 1 : 2;
	_pause_page = PAUSE_MAIN;
	return false;
}

static bool pause_activate() {
	if (_pause_page == PAUSE_CONTROLS) return pause_back();
	if (_pause_page == PAUSE_QUIT) {
		if (_pause_selection == 1) { g_sys.input.quit = true; return true; }
		return pause_back();
	}
	if (_pause_selection == 0) return true;
	_pause_page = _pause_selection == 1 ? PAUSE_CONTROLS : PAUSE_QUIT;
	_pause_selection = 0; /* Confirmation always defaults to cancel. */
	return false;
}

static void pause_move(int direction) {
	if (_pause_page == PAUSE_CONTROLS) return;
	const int count = _pause_page == PAUSE_QUIT ? 2 : 3;
	_pause_selection = (_pause_selection + direction + count) % count;
}

static void remember_held_controls() {
	const uint8_t *keys = SDL_GetKeyboardState(NULL);
	for (int i = 1; i < SDL_NUM_SCANCODES; ++i) _blocked_keys[i] |= keys[i] != 0;
	if (_controller) {
		for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i)
			_blocked_buttons[i] |= SDL_GameControllerGetButton(_controller, i) != 0;
		for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i)
			_blocked_axes[i] |= abs(SDL_GameControllerGetAxis(_controller, i)) > 3200;
	}
	if (_joystick) {
		for (int i = 0; i < MIN(32, SDL_JoystickNumButtons(_joystick)); ++i)
			_blocked_joy_buttons[i] |= SDL_JoystickGetButton(_joystick, i) != 0;
		for (int i = 0; i < MIN(2, SDL_JoystickNumAxes(_joystick)); ++i)
			_blocked_joy_axes[i] |= abs(SDL_JoystickGetAxis(_joystick, i)) > 3200;
		if (SDL_JoystickNumHats(_joystick)) _blocked_hat |= SDL_JoystickGetHat(_joystick, 0) != SDL_HAT_CENTERED;
	}
}

/* Menu events never pass through gameplay input. Held controls remain blocked
 * after returning until their release/neutral event is received. */
static bool pause_event(const SDL_Event *ev) {
	if (ev->type == SDL_QUIT) { g_sys.input.quit = true; return true; }
	if (ev->type == SDL_WINDOWEVENT) {
		if (ev->window.event == SDL_WINDOWEVENT_FOCUS_LOST) _pause_focused = _window_has_focus = false;
		if (ev->window.event == SDL_WINDOWEVENT_FOCUS_GAINED) _pause_focused = _window_has_focus = true;
		return false;
	}
	if (ev->type == SDL_KEYDOWN || ev->type == SDL_KEYUP) {
		const int code = ev->key.keysym.scancode;
		const bool down = ev->type == SDL_KEYDOWN;
		if (code > 0 && code < SDL_NUM_SCANCODES) _blocked_keys[code] = down;
		if (!_pause_focused) return false;
		const int key = ev->key.keysym.sym;
		if (!down && (key == SDLK_ESCAPE || key == SDLK_p)) return pause_back();
		if (!down) return false;
		if (key == SDLK_UP) pause_move(-1);
		if (key == SDLK_DOWN) pause_move(1);
		if (!ev->key.repeat && (key == SDLK_RETURN || key == SDLK_SPACE)) return pause_activate();
	}
	if (ev->type == SDL_CONTROLLERBUTTONDOWN || ev->type == SDL_CONTROLLERBUTTONUP) {
		const bool down = ev->type == SDL_CONTROLLERBUTTONDOWN;
		const int button = ev->cbutton.button;
		if (button >= SDL_CONTROLLER_BUTTON_MAX) return false;
		_blocked_buttons[button] = down;
		if (!_pause_focused) return false;
		if (down && button == SDL_CONTROLLER_BUTTON_DPAD_UP) pause_move(-1);
		if (down && button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) pause_move(1);
		if (down && button == SDL_CONTROLLER_BUTTON_A) return pause_activate();
		if ((down && button == SDL_CONTROLLER_BUTTON_B) ||
			(!down && button == SDL_CONTROLLER_BUTTON_START)) return pause_back();
	}
	if (ev->type == SDL_CONTROLLERAXISMOTION || ev->type == SDL_JOYAXISMOTION) {
		const bool controller = ev->type == SDL_CONTROLLERAXISMOTION;
		const int axis = controller ? ev->caxis.axis : ev->jaxis.axis;
		const int value = controller ? ev->caxis.value : ev->jaxis.value;
		if (controller && axis < SDL_CONTROLLER_AXIS_MAX) _blocked_axes[axis] = abs(value) > 3200;
		if (!controller && axis < 2) _blocked_joy_axes[axis] = abs(value) > 3200;
		if (axis == (controller ? SDL_CONTROLLER_AXIS_LEFTY : 1)) {
			const int direction = value < -16000 ? -1 : (value > 16000 ? 1 : 0);
			if (_pause_focused && direction && direction != _pause_axis) pause_move(direction);
			_pause_axis = direction;
		}
	}
	if (ev->type == SDL_JOYHATMOTION) {
		_blocked_hat = ev->jhat.value != SDL_HAT_CENTERED;
		const int direction = ev->jhat.value & SDL_HAT_UP ? -1 : (ev->jhat.value & SDL_HAT_DOWN ? 1 : 0);
		if (_pause_focused && direction && direction != _pause_hat) pause_move(direction);
		_pause_hat = direction;
	}
	if (ev->type == SDL_JOYBUTTONDOWN || ev->type == SDL_JOYBUTTONUP) {
		const int button = ev->jbutton.button;
		const bool down = ev->type == SDL_JOYBUTTONDOWN;
		if (button < 32) _blocked_joy_buttons[button] = down;
		if (!_pause_focused) return false;
		if (down && button == 0) return pause_activate();
		if ((down && button == 1) || (!down && button == 7)) return pause_back();
	}
	if (ev->type == SDL_CONTROLLERDEVICEADDED || ev->type == SDL_CONTROLLERDEVICEREMOVED) {
		bool paused = false;
		handle_event(ev, &paused);
	}
	return false;
}

static void sdl2_run_pause_menu() {
	if (!_pause_menu_enabled || g_sys.input.quit) return;
	const SDL_AudioStatus audio_status = SDL_GetAudioStatus();
	const bool resume_audio = audio_status == SDL_AUDIO_PLAYING ||
		(audio_status == SDL_AUDIO_PAUSED && !_window_has_focus);
	SDL_PauseAudio(1);
	clear_pause_input();
	remember_held_controls();
	sdl2_set_scroll_interpolation(0, 0, 0, false);
	_pause_page = PAUSE_MAIN;
	_pause_selection = _pause_axis = _pause_hat = 0;
	_pause_focused = _window_has_focus;
	_pause_menu_open = true;
	bool done = false;
	while (!done && !g_sys.input.quit) {
		sdl2_draw_screen(0.f, 0.f, 1.f);
		SDL_Event ev;
		if (SDL_WaitEventTimeout(&ev, 16)) {
			done = pause_event(&ev);
			while (!done && SDL_PollEvent(&ev)) done = pause_event(&ev);
		}
	}
	remember_held_controls();
	clear_pause_input();
	_pause_menu_open = false;
	sdl2_draw_screen(0.f, 0.f, 1.f);
	if (resume_audio && _window_has_focus && !g_sys.input.quit) SDL_PauseAudio(0);
}
