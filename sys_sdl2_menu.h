#include <time.h>
/* SDL pause-menu implementation. Kept private to sys_sdl2.c. */
enum pause_page { PAUSE_MAIN, PAUSE_CONTROLS, PAUSE_QUIT, PAUSE_OPTIONS, PAUSE_BINDINGS, PAUSE_KEYS, PAUSE_PAD, PAUSE_AUDIO, PAUSE_ASSISTS, PAUSE_LEVELS, PAUSE_LEVEL_CONFIRM, PAUSE_CHARACTER_CONFIRM, PAUSE_DEFAULTS, PAUSE_GAME, PAUSE_SAVE_CONFIRM, PAUSE_LOAD_CONFIRM, PAUSE_SLOTS };
static struct sys_save_slot _save_slots[SAVE_SLOT_COUNT];
static int _save_slot, _preview_texture_slot=-1;
static bool _save_browser_load;
static int _selected_level;
static const char *_level_names[] = {"1 CENTRO COMERCIAL","2 ALMACEN","3 PRISION","4 ALCANTARILLAS","5 CIUDAD","6 CONCIERTO"};
static enum pause_page _pause_page;
static int _pause_selection, _pause_axis, _pause_hat;
static bool _pause_focused;
static bool _assist_invulnerable, _assist_lives;

enum binding_action { BIND_LEFT, BIND_RIGHT, BIND_UP, BIND_DOWN, BIND_ACTION, BIND_JUMP, BIND_COUNT };
static const int _default_keys[BIND_COUNT] = {SDLK_LEFT, SDLK_RIGHT, SDLK_UP,SDLK_DOWN,SDLK_SPACE,SDLK_LSHIFT};
static int _game_keys[BIND_COUNT] = {SDLK_LEFT, SDLK_RIGHT, SDLK_UP,SDLK_DOWN,SDLK_SPACE,SDLK_LSHIFT};
static int _edit_keys[BIND_COUNT] = {SDLK_LEFT, SDLK_RIGHT, SDLK_UP,SDLK_DOWN,SDLK_SPACE,SDLK_LSHIFT};
static int _game_buttons[2] = {SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_B};
static int _edit_buttons[2] = {SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_B};
static struct sys_pause_options _pause_options = {.camera=CAMERA_SMOOTH,.hud=HUD_CLASSIC,.render_rate=60,.music_volume=100,.effects_volume=100};
static int pause_level_count() { return _pause_options.level_count ? _pause_options.level_count : 6; }
static const char *pause_level_name(int level) {
	if (_pause_options.level_names) return _pause_options.level_names[level];
	return _level_names[level];
}
static int _last_music_volume = 100, _last_effects_volume = 100;
static int _pause_capture = -1, _pause_xaxis;
static int _capture_ignore_key = SDLK_UNKNOWN, _capture_ignore_button = -1;
static int _capture_key_candidate = SDLK_UNKNOWN, _capture_button_candidate = -1;
static const char *_pause_notice;
static const char *_bind_names[] = {"IZQUIERDA","DERECHA","ARRIBA","ABAJO","ACCION","SALTAR"};

static void sdl2_set_pause_options(const struct sys_pause_options *options) { _pause_options = *options; }
static void sdl2_get_pause_options(struct sys_pause_options *options) { *options = _pause_options; }

static bool handle_bound_key(int key, bool down, struct input_t *input) {
	for (int i = 0; i < BIND_COUNT; ++i) {
		if (key != _game_keys[i] && !(i == BIND_JUMP && _game_keys[i] == SDLK_LSHIFT && key == SDLK_RSHIFT)) continue;
		if (i < BIND_ACTION) {
			const int mask = 1 << i;
			if (down) input->direction |= mask;
			else input->direction &= ~mask;
		} else if (i == BIND_ACTION) input->space = down;
		else input->jump = down;
		return true;
	}
	/* Default controls cease to act when reassigned; Enter remains an alias. */
	for (int i = 0; i < BIND_COUNT; ++i) if (key == _default_keys[i]) return true;
	return key == SDLK_RSHIFT;
}

static bool handle_bound_button(int button, bool down, struct input_t *input) {
	if (button == _game_buttons[0]) { input->space = down; return true; }
	if (button == _game_buttons[1]) { input->jump = down; return true; }
	return button == SDL_CONTROLLER_BUTTON_A || button == SDL_CONTROLLER_BUTTON_B;
}

static bool pause_large_page() {
	return _pause_page == PAUSE_SLOTS || _pause_page == PAUSE_MAIN || _pause_page == PAUSE_GAME || _pause_page == PAUSE_SAVE_CONFIRM || _pause_page == PAUSE_LOAD_CONFIRM || _pause_page == PAUSE_CONTROLS || _pause_page == PAUSE_OPTIONS ||
		_pause_page == PAUSE_BINDINGS || _pause_page == PAUSE_KEYS || _pause_page == PAUSE_PAD || _pause_page == PAUSE_AUDIO || _pause_page == PAUSE_ASSISTS || _pause_page == PAUSE_LEVELS || _pause_page == PAUSE_LEVEL_CONFIRM || _pause_page == PAUSE_CHARACTER_CONFIRM || _pause_page == PAUSE_DEFAULTS;
}

static int pause_item_count() {
	switch (_pause_page) {
	case PAUSE_MAIN: return 7;
	case PAUSE_GAME: return 3;
	case PAUSE_SLOTS: return SAVE_SLOT_COUNT+1;
	case PAUSE_SAVE_CONFIRM: case PAUSE_LOAD_CONFIRM: return 2;
	case PAUSE_LEVELS: return pause_level_count()+1;
	case PAUSE_LEVEL_CONFIRM: case PAUSE_CHARACTER_CONFIRM: return 2;
	case PAUSE_QUIT: return 2;
	case PAUSE_OPTIONS: return 10;
	case PAUSE_DEFAULTS: return 2;
	case PAUSE_ASSISTS: return 5;
	case PAUSE_AUDIO: return 3;
	case PAUSE_BINDINGS: return 5;
	case PAUSE_KEYS: return BIND_COUNT + 1;
	case PAUSE_PAD: return 3;
	default: return 0;
	}
}

static void uppercase_binding(char *out, size_t size, const char *name) {
	size_t n = 0;
	for (; name && *name && n + 1 < size; ++name) {
		unsigned char c = *name;
		out[n++] = c >= 'a' && c <= 'z' ? c - 'a' + 'A' : (c < 128 ? c : '?');
	}
	out[n] = 0;
}

static const char *keyboard_binding_name(int key) {
	switch (key) {
	case SDLK_LEFT: return "IZQUIERDA";
	case SDLK_RIGHT: return "DERECHA";
	case SDLK_UP: return "ARRIBA";
	case SDLK_DOWN: return "ABAJO";
	case SDLK_SPACE: return "ESPACIO";
	case SDLK_LSHIFT: return "SHIFT IZQ";
	case SDLK_RSHIFT: return "SHIFT DER";
	case SDLK_LCTRL: return "CTRL IZQ";
	case SDLK_RCTRL: return "CTRL DER";
	default: return SDL_GetKeyName(key);
	}
}

static void pause_adjust(int direction) {
	if (_pause_page == PAUSE_ASSISTS) {
		if (_pause_selection == 0) _pause_options.invulnerable = !_pause_options.invulnerable;
		if (_pause_selection == 1) _pause_options.infinite_lives = !_pause_options.infinite_lives;
		if (_pause_selection == 2 || _pause_selection == 3) {
			const int player = _pause_selection-2;
			if (_pause_options.player_alive[player]) _pause_options.lives[player] = MAX(1,MIN(99,_pause_options.lives[player]+direction));
		}
		return;
	}
	if (_pause_page == PAUSE_AUDIO) {
		int *value = _pause_selection == 0 ? &_pause_options.music_volume : &_pause_options.effects_volume;
		int *last = _pause_selection == 0 ? &_last_music_volume : &_last_effects_volume;
		if (_pause_selection < 2) {
			if (*value > 0) *last = *value;
			*value = MAX(0,MIN(100,*value+direction*10));
		}
		return;
	}
	if (_pause_page != PAUSE_OPTIONS) return;
	_pause_notice = NULL;
	switch (_pause_selection) {
	case 0:
		if (_pause_options.can_switch) _pause_options.amiga = !_pause_options.amiga;
		else _pause_notice = "REQUIERE DATOS DOS Y AMIGA VALIDOS";
		break;
	case 1: _pause_options.camera = _pause_options.camera == CAMERA_CLASSIC ? CAMERA_SMOOTH : CAMERA_CLASSIC; break;
	case 2: _pause_options.hud = _pause_options.hud == HUD_CLASSIC ? HUD_FIXED : HUD_CLASSIC; break;
	case 3: {
		const bool full = (SDL_GetWindowFlags(_window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
		if (SDL_SetWindowFullscreen(_window, full ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP) != 0)
			_pause_notice = "NO SE PUDO CAMBIAR LA PANTALLA";
		break;
	}
	case 4: {
		const int rates[] = {30,60,120};
		int index = _pause_options.render_rate == 30 ? 0 : (_pause_options.render_rate == 60 ? 1 : 2);
		_pause_options.render_rate = rates[(index + direction + 3) % 3];
		break;
	}
	case 5: _pause_options.view_step = MAX(0,MIN(8,_pause_options.view_step + direction)); break;
	}
}

static bool reserved_binding(int key) {
	return key == SDLK_ESCAPE || key == SDLK_p || key == SDLK_r || key == SDLK_RETURN || key == SDLK_F6 || key == SDLK_F7 ||
		key == SDLK_MINUS || key == SDLK_KP_MINUS || key == SDLK_PLUS || key == SDLK_EQUALS || key == SDLK_KP_PLUS ||
		key == SDLK_0 || key == SDLK_1 || key == SDLK_2 || key == SDLK_3 || key == SDLK_UNKNOWN;
}

static void assign_binding(int *bindings, int count, int index, int value) {
	for (int i = 0; i < count; ++i) if (bindings[i] == value) bindings[i] = bindings[index];
	bindings[index] = value;
}

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
 {'%',{24,25,2,4,8,19,3}}, {'.',{0,0,0,0,0,12,12}}, {'_', {0,0,0,0,0,0,31}},
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

static void sdl2_set_assist_status(bool invulnerable,bool infinite_lives) {
	_assist_invulnerable=invulnerable; _assist_lives=infinite_lives;
}

static void sdl2_draw_assist_status() {
	if (!_pause_menu_enabled || (!_assist_invulnerable && !_assist_lives)) return;
	const char *text = _assist_invulnerable
		? (_assist_lives ? "INVULNERABLE / VIDAS INFINITAS" : "INVULNERABLE") : "VIDAS INFINITAS";
	if (_hud_width > 0) {
		SDL_RenderSetLogicalSize(_renderer,320,200);
		pause_text_color(text,8,191,1,PAUSE_GOLD);
		SDL_RenderSetLogicalSize(_renderer,_screen_w,_screen_h);
	} else {
		pause_text_color(text,(_screen_w-320)/2+8,_screen_h-9,1,PAUSE_GOLD);
	}
	SDL_SetRenderDrawColor(_renderer,0,0,0,255);
}

static void pause_center_text(const char *text, int y, int scale, bool selected) {
	pause_text(text, (320 - ((int)strlen(text) * 6 - 1) * scale) / 2, y, scale, selected);
}

static void pause_row(const char *label, const char *value, int index, bool enabled) {
	const int y = 50 + index * (_pause_page == PAUSE_OPTIONS ? 11 : 14);
	const bool selected = index == _pause_selection + ((_pause_page == PAUSE_LEVEL_CONFIRM || _pause_page == PAUSE_CHARACTER_CONFIRM || _pause_page == PAUSE_DEFAULTS || _pause_page == PAUSE_SAVE_CONFIRM || _pause_page == PAUSE_LOAD_CONFIRM) ? 4 : 0);
	if (selected) {
		SDL_Rect rect = {25,y-3,269,12};
		SDL_SetRenderDrawColor(_renderer,29,55,77,255);
		SDL_RenderFillRect(_renderer,&rect);
		rect.w=2;
		SDL_SetRenderDrawColor(_renderer,255,215,104,255);
		SDL_RenderFillRect(_renderer,&rect);
	}
	pause_text_color(label,34,y,1,selected ? PAUSE_GOLD : PAUSE_TEXT);
	if (value) pause_text_color(value,_pause_page == PAUSE_LEVELS ? 220 : 150,y,1,enabled ? PAUSE_CYAN : PAUSE_MUTED);
}

static void pause_refresh_save(int slot) {
	memset(&_save_slots[slot],0,sizeof(_save_slots[slot]));
	if(_pause_options.inspect_save) _pause_options.inspect_save(slot,&_save_slots[slot]);
	_preview_texture_slot=-1;
}
static void pause_draw_save_slots() {
	pause_center_text(_save_browser_load ? "CARGAR PARTIDA" : "GUARDAR PARTIDA",26,2,true);
	for(int i=0;i<=SAVE_SLOT_COUNT;++i) {
		const int y=50+i*12;
		if(i==_pause_selection) {
			SDL_SetRenderDrawColor(_renderer,29,55,77,255);SDL_Rect r={25,y-3,140,12};SDL_RenderFillRect(_renderer,&r);
			r.w=2;SDL_SetRenderDrawColor(_renderer,255,215,104,255);SDL_RenderFillRect(_renderer,&r);
		}
		char label[24];SDL_Color color=PAUSE_MUTED;
		if(i==SAVE_SLOT_COUNT) {strcpy(label,"VOLVER");color=PAUSE_TEXT;}
		else {
			const struct sys_save_slot *info=&_save_slots[i];
			if(info->status==SAVE_READY) {snprintf(label,sizeof(label),"%d  NIVEL %d",i+1,info->level+1);color=PAUSE_CYAN;}
			else snprintf(label,sizeof(label),"%d  %s",i+1,info->status==SAVE_EMPTY ? "VACIO" : (info->status==SAVE_INCOMPATIBLE ? "INCOMPATIBLE" : (info->status==SAVE_DAMAGED ? "DANADO" : "ERROR LECTURA")));
			if(info->status==SAVE_INCOMPATIBLE || info->status==SAVE_DAMAGED || info->status==SAVE_UNREADABLE)color=PAUSE_DANGER;
		}
		pause_text_color(label,34,y,1,i==_pause_selection ? PAUSE_GOLD : color);
	}
	const int slot=_pause_selection<SAVE_SLOT_COUNT ? _pause_selection : _save_slot;
	const struct sys_save_slot *info=&_save_slots[slot];
	SDL_Rect image={172,48,SAVE_PREVIEW_WIDTH,SAVE_PREVIEW_HEIGHT},border={170,46,SAVE_PREVIEW_WIDTH+4,SAVE_PREVIEW_HEIGHT+4};
	SDL_SetRenderDrawColor(_renderer,69,189,213,255);SDL_RenderDrawRect(_renderer,&border);
	SDL_SetRenderDrawColor(_renderer,4,10,20,255);SDL_RenderFillRect(_renderer,&image);
	if(info->has_preview) {
		if(!_save_preview_texture)_save_preview_texture=SDL_CreateTexture(_renderer,SDL_PIXELFORMAT_RGB24,SDL_TEXTUREACCESS_STATIC,SAVE_PREVIEW_WIDTH,SAVE_PREVIEW_HEIGHT);
		if(_save_preview_texture && _preview_texture_slot!=slot) {SDL_UpdateTexture(_save_preview_texture,NULL,info->preview,SAVE_PREVIEW_WIDTH*3);_preview_texture_slot=slot;}
		if(_save_preview_texture)SDL_RenderCopy(_renderer,_save_preview_texture,NULL,&image);
	} else {
		const char *text=info->status==SAVE_EMPTY ? "SLOT VACIO" : "SIN MINIATURA";
		pause_text_color(text,172+(SAVE_PREVIEW_WIDTH-((int)strlen(text)*6-1))/2,83,1,PAUSE_MUTED);
	}
	if(info->has_metadata) {
		char text[48];snprintf(text,sizeof(text),"NIVEL %d - %s",info->level+1,info->amiga ? "AMIGA" : "DOS");pause_text_color(text,172,134,1,PAUSE_CYAN);
		strcpy(text,"FECHA NO DISPONIBLE");
		if(info->timestamp && info->timestamp<=253402300799ULL) {
			const time_t date=(time_t)info->timestamp;
			struct tm *local=(uint64_t)date==info->timestamp ? localtime(&date) : NULL;
			if(local)strftime(text,sizeof(text),"%d/%m/%Y %H:%M",local);
		}
		pause_text_color(text,172,145,1,PAUSE_TEXT);
		if(info->infinite_lives)strcpy(text,"VIDAS: INFINITAS");
		else if(info->two_players)snprintf(text,sizeof(text),"VIDAS: %02d / %02d",info->lives[0],info->lives[1]);
		else snprintf(text,sizeof(text),"VIDAS: %02d",info->lives[0]);
		pause_text_color(text,172,156,1,PAUSE_TEXT);
	}
	const char *hint=_pause_notice;
	if(!hint && _save_browser_load && info->status!=SAVE_READY && _pause_selection<SAVE_SLOT_COUNT)hint=info->reason;
	pause_center_text(hint ? hint : "ENTER / A: ELEGIR  ESC: VOLVER",176,1,true);
}

static void pause_draw_settings() {
	if(_pause_page==PAUSE_SLOTS){pause_draw_save_slots();return;}
	if (_pause_page == PAUSE_GAME) {
		pause_center_text("PARTIDA",26,2,true);
		pause_row("GUARDAR PARTIDA",NULL,0,_pause_options.save_game!=NULL);
		pause_row("CARGAR PARTIDA",NULL,1,_pause_options.prepare_load!=NULL);
		pause_row("VOLVER",NULL,2,true);
		pause_center_text("8 SLOTS CON MINIATURAS",115,1,false);
		pause_center_text("NIVEL, FECHA Y VIDAS",131,1,false);
		pause_center_text(_pause_notice ? _pause_notice : "ENTER / A: ELEGIR  ESC: VOLVER",163,1,true);
		return;
	}
	if (_pause_page == PAUSE_SAVE_CONFIRM || _pause_page == PAUSE_LOAD_CONFIRM) {
		const bool load=_pause_page == PAUSE_LOAD_CONFIRM;
		char title[32];snprintf(title,sizeof(title),load ? "CARGAR SLOT %d?" : (_save_slots[_save_slot].status==SAVE_EMPTY ? "GUARDAR EN SLOT %d?" : "SOBRESCRIBIR SLOT %d?"),_save_slot+1);
		pause_center_text(title,26,2,true);
		pause_center_text(load ? "SE PERDERA EL PROGRESO ACTUAL" : (_save_slots[_save_slot].status==SAVE_EMPTY ? "SE GUARDARA EL ESTADO ACTUAL" : "REEMPLAZARA LA PARTIDA GUARDADA"),65,1,false);
		pause_row("VOLVER",NULL,4,true);
		pause_row(load ? "CARGAR" : "GUARDAR",NULL,5,true);
		pause_center_text("ENTER / A: ELEGIR  ESC: VOLVER",163,1,true);
		return;
	}

	if (_pause_page == PAUSE_LEVEL_CONFIRM || _pause_page == PAUSE_CHARACTER_CONFIRM || _pause_page == PAUSE_DEFAULTS) {
		const bool character = _pause_page == PAUSE_CHARACTER_CONFIRM;
		pause_center_text(_pause_page == PAUSE_DEFAULTS ? "RESTAURAR AJUSTES?" : (character ? "ELEGIR PERSONAJE?" : "INICIAR NIVEL?"),26,2,true);
		pause_center_text(_pause_page == PAUSE_DEFAULTS ? "OPCIONES Y CONTROLES ORIGINALES" : (character ? "REINICIARAS EL NIVEL ACTUAL" : pause_level_name(_selected_level)),60,1,true);
		pause_center_text(_pause_page == PAUSE_DEFAULTS ? "LA PARTIDA NO SE REINICIA" : "SE PERDERA EL PROGRESO ACTUAL",82,1,false);
		pause_row("VOLVER",NULL,4,true);
		pause_row(_pause_page == PAUSE_DEFAULTS ? "RESTAURAR" : "INICIAR",NULL,5,true);
		pause_center_text("ENTER / A: ELEGIR  ESC: VOLVER",163,1,true);
		return;
	}
	if (_pause_page == PAUSE_LEVELS) {
		pause_center_text("SELECCIONAR NIVEL",26,2,true);
		const int first = (_pause_selection < pause_level_count() ? _pause_selection / 6 : (pause_level_count()-1)/6)*6;
		for (int i=first;i<MIN(first+6,pause_level_count());++i) {
			const bool available = _pause_options.available_levels[_pause_options.amiga][i];
			const char *status = !available ? "SIN DATOS" : (i == _pause_options.current_level ? "ACTUAL" : NULL);
			const int selected=_pause_selection;_pause_selection-=first;
			pause_row(pause_level_name(i),status,i-first,available);_pause_selection=selected;
		}
		const int selected=_pause_selection; if(selected==pause_level_count())_pause_selection=6;
		pause_row("VOLVER",NULL,6,true);_pause_selection=selected;
		pause_center_text(_pause_notice ? _pause_notice : "ENTER / A: ELEGIR  ESC: VOLVER",163,1,true);
		pause_center_text("INICIO DESDE EL PRINCIPIO",175,1,false);
		return;
	}

	const char *title = _pause_page == PAUSE_OPTIONS ? "OPCIONES" :
		(_pause_page == PAUSE_ASSISTS ? "AYUDAS" : (_pause_page == PAUSE_AUDIO ? "SONIDO" : (_pause_page == PAUSE_BINDINGS ? "CONTROLES" : (_pause_page == PAUSE_KEYS ? "TECLADO" : "MANDO"))));
	pause_center_text(title,26,2,true);
	char value[24];
	if (_pause_page == PAUSE_OPTIONS) {
		pause_row("VERSION",_pause_options.amiga ? "AMIGA" : "DOS",0,_pause_options.can_switch);
		pause_row("CAMARA",_pause_options.camera == CAMERA_CLASSIC ? "CLASICA" : "SUAVIZADA",1,true);
		pause_row("HUD",_pause_options.hud == HUD_CLASSIC ? "CLASICO" : "FIJO",2,true);
		pause_row("PANTALLA",SDL_GetWindowFlags(_window)&SDL_WINDOW_FULLSCREEN_DESKTOP ? "COMPLETA" : "VENTANA",3,true);
		snprintf(value,sizeof(value),"%d HZ",_pause_options.render_rate);
		pause_row("FRECUENCIA",value,4,true);
		if (_pause_options.view_step) snprintf(value,sizeof(value),"PASO %d",_pause_options.view_step);
		else strcpy(value,"NORMAL");
		pause_row("ENCUADRE",value,5,true);
		pause_row("SONIDO",NULL,6,true);
		pause_row("AYUDAS",NULL,7,true);
		pause_row("RESTAURAR AJUSTES",NULL,8,true);
		pause_row("VOLVER",NULL,9,true);
	} else if (_pause_page == PAUSE_ASSISTS) {
		pause_row("INVULNERABLE",_pause_options.invulnerable ? "ACTIVADA" : "DESACTIVADA",0,true);
		pause_row("VIDAS INFINITAS",_pause_options.infinite_lives ? "ACTIVADAS" : "DESACTIVADAS",1,true);
		for(int i=0;i<2;++i) {
			if (_pause_options.player_alive[i]) snprintf(value,sizeof(value),"%d",_pause_options.lives[i]);
			else strcpy(value,i==1 && !_pause_options.two_players ? "NO PARTICIPA" : "NO ACTIVO");
			pause_row(i==0 ? "VIDAS J1" : "VIDAS J2",value,i+2,_pause_options.player_alive[i]);
		}
		pause_row("VOLVER",NULL,4,true);
		pause_center_text(_pause_options.infinite_lives ? "CUENTA FINITA CONSERVADA" : "VIDAS: INCLUYE LA VIDA ACTUAL",145,1,false);
	} else if (_pause_page == PAUSE_AUDIO) {
		for (int i=0;i<2;++i) {
			const int volume = i==0 ? _pause_options.music_volume : _pause_options.effects_volume;
			if (volume) snprintf(value,sizeof(value),"%d%%",volume);
			else strcpy(value,"SILENCIO");
			pause_row(i==0 ? "MUSICA" : "EFECTOS",value,i,true);
			SDL_Rect meter = {150,59+i*14,100,2};
			SDL_SetRenderDrawColor(_renderer,38,68,91,255); SDL_RenderFillRect(_renderer,&meter);
			meter.w = volume;
			const SDL_Color color = i==_pause_selection ? PAUSE_GOLD : PAUSE_CYAN;
			SDL_SetRenderDrawColor(_renderer,color.r,color.g,color.b,255);
			if(volume) SDL_RenderFillRect(_renderer,&meter);
		}
		pause_row("VOLVER",NULL,2,true);
	} else if (_pause_page == PAUSE_BINDINGS) {
		pause_row("TECLADO",NULL,0,true);
		pause_row("MANDO",NULL,1,true);
		pause_row("VER ASIGNACIONES",NULL,2,true);
		pause_row("RESTAURAR CONTROLES",NULL,3,true);
		pause_row("VOLVER",NULL,4,true);
	} else {
		const int count = _pause_page == PAUSE_KEYS ? BIND_COUNT : 2;
		for (int i = 0; i < count; ++i) {
			if (_pause_capture == i) strcpy(value,"...");
			else if (_pause_page == PAUSE_KEYS) uppercase_binding(value,sizeof(value),keyboard_binding_name(_edit_keys[i]));
			else uppercase_binding(value,sizeof(value),SDL_GameControllerGetStringForButton(_edit_buttons[i]));
			pause_row(_bind_names[_pause_page == PAUSE_KEYS ? i : BIND_ACTION + i],value,i,true);
		}
		pause_row("VOLVER",NULL,count,true);
	}
	const char *hint = _pause_capture >= 0 ? (_pause_page == PAUSE_PAD ? "PULSA UN BOTON - ESC CANCELA" : "PULSA UNA TECLA - ESC CANCELA") :
		(_pause_page == PAUSE_ASSISTS ? "IZQ / DER: AJUSTAR  ENTER: CAMBIAR" :
		(_pause_page == PAUSE_AUDIO ? "IZQ / DER: VOLUMEN  ENTER: SILENCIO" :
		(_pause_page == PAUSE_OPTIONS ? "IZQ / DER: CAMBIAR  ENTER: ELEGIR" : "ENTER / A: REASIGNAR  ESC: VOLVER")));
	if (_pause_notice) hint = _pause_notice;
	pause_center_text(hint,163,1,true);
	pause_center_text("SE APLICA AL CONTINUAR",175,1,false);
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
	SDL_Rect box = pause_large_page()
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
	if (_pause_page >= PAUSE_OPTIONS) {
		pause_draw_settings();
	} else if (_pause_page == PAUSE_CONTROLS) {
		pause_center_text("CONTROLES", 26, 2, true);
		SDL_SetRenderDrawColor(_renderer, 38, 68, 91, 255);
		SDL_RenderDrawLine(_renderer, 26, 45, 293, 45);
		char binding[48];
		for (int i = 0; i < BIND_COUNT; ++i) {
			uppercase_binding(binding,sizeof(binding),keyboard_binding_name(_edit_keys[i]));
			pause_text_color(_bind_names[i],26,53+i*14,1,PAUSE_CYAN);
			pause_text_color(binding,100,53+i*14,1,PAUSE_TEXT);
		}
		char a[20], b[20];
		uppercase_binding(a,sizeof(a),SDL_GameControllerGetStringForButton(_edit_buttons[0]));
		uppercase_binding(b,sizeof(b),SDL_GameControllerGetStringForButton(_edit_buttons[1]));
		snprintf(binding,sizeof(binding),"%s / %s",a,b);
		pause_text_color("MANDO",26,137,1,PAUSE_CYAN);
		pause_text_color(binding,100,137,1,PAUSE_TEXT);
		pause_center_text("F6 VERSION  F7 HUD  - / + / 0 ZOOM",153,1,false);
		pause_center_text("MANTENER R / LB: REBOBINAR",164,1,false);
		pause_center_text("ENTER / A / ESC / B: VOLVER",175,1,true);
	} else {
		pause_center_text(_pause_page == PAUSE_QUIT ? "SALIR DEL JUEGO?" : "PAUSA", _pause_page == PAUSE_QUIT ? 44 : 26,
			2, true);
		if (_pause_page == PAUSE_QUIT) {
			pause_center_text("CONFIRMA PARA SALIR", 65, 1, false);
		} else {
			SDL_SetRenderDrawColor(_renderer, 38, 68, 91, 255);
			SDL_RenderDrawLine(_renderer, 43, 47, 276, 47);
		}
		const char *main_items[] = {"CONTINUAR", "OPCIONES", "CONTROLES", "SELECCIONAR NIVEL", "PARTIDA", "ELEGIR PERSONAJE", "SALIR DEL JUEGO"};
		const char *quit_items[] = {"VOLVER", "SALIR"};
#ifdef TITUS_MANAGED
        if(_pause_options.no_character_select)main_items[5]="PERSONAJE: UNICO";
        main_items[6]="VOLVER A BIBLIOTECA";quit_items[1]="VOLVER A BIBLIOTECA";
#endif
		const int count = pause_item_count();
		for (int i = 0; i < count; ++i) {
			const int y = _pause_page == PAUSE_QUIT ? 87 + i * 21 : 56 + i * 14;
			const SDL_Color accent = _pause_page == PAUSE_QUIT && i == 1 ? PAUSE_DANGER : PAUSE_GOLD;
			if (i == _pause_selection) {
				SDL_Rect selected = {43, y - (_pause_page == PAUSE_QUIT ? 5 : 3), 234, _pause_page == PAUSE_QUIT ? 17 : 12};
				SDL_SetRenderDrawColor(_renderer, 29, 55, 77, 255);
				SDL_RenderFillRect(_renderer, &selected);
				SDL_Rect stripe = {43, y - (_pause_page == PAUSE_QUIT ? 5 : 3), 2, _pause_page == PAUSE_QUIT ? 17 : 12};
				SDL_SetRenderDrawColor(_renderer, accent.r, accent.g, accent.b, 255);
				SDL_RenderFillRect(_renderer, &stripe);
				pause_text_color(">", 51, y, 1, accent);
			}
			pause_text_color(_pause_page == PAUSE_QUIT ? quit_items[i] : main_items[i], 69, y, 1,
				i == _pause_selection ? accent : PAUSE_TEXT);
		}
		pause_text_color("FLECHAS / STICK   ENTER / A", 85, _pause_page == PAUSE_QUIT ? 145 : 163, 1, PAUSE_MUTED);
		pause_text_color("ESC / P / START: VOLVER", 94, _pause_page == PAUSE_QUIT ? 156 : 175, 1, PAUSE_MUTED);
	}
	SDL_RenderSetScale(_renderer, 1.f, 1.f);
	SDL_RenderSetViewport(_renderer, NULL);
	SDL_SetRenderDrawBlendMode(_renderer, blend);
	SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 255);
	SDL_RenderSetLogicalSize(_renderer, _screen_w, _screen_h);
}

static void clear_pause_input() {
	g_sys.input.rewind=false;_rewind_key=_rewind_button=false;
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
	_pause_notice = NULL;
	if (_pause_capture >= 0) { _pause_capture = -1; return false; }
	if (_pause_page == PAUSE_MAIN) return true;
	if (_pause_page == PAUSE_SAVE_CONFIRM || _pause_page == PAUSE_LOAD_CONFIRM) {
		_pause_selection=_save_slot; _pause_page=PAUSE_SLOTS;
	} else if (_pause_page == PAUSE_SLOTS) {
		_pause_page=PAUSE_GAME;_pause_selection=_save_browser_load ? 1 : 0;
	} else if (_pause_page == PAUSE_GAME) {
		_pause_page=PAUSE_MAIN; _pause_selection=4;
	} else if (_pause_page == PAUSE_DEFAULTS) {
		_pause_page=PAUSE_OPTIONS; _pause_selection=8;
	} else if (_pause_page == PAUSE_CHARACTER_CONFIRM) {
		_pause_page=PAUSE_MAIN; _pause_selection=5;
	} else if (_pause_page == PAUSE_LEVEL_CONFIRM) {
		_pause_page=PAUSE_LEVELS; _pause_selection=_selected_level;
	} else if (_pause_page == PAUSE_LEVELS) {
		_pause_page=PAUSE_MAIN; _pause_selection=3;
	} else if (_pause_page == PAUSE_ASSISTS) {
		_pause_page=PAUSE_OPTIONS; _pause_selection=7;
	} else if (_pause_page == PAUSE_AUDIO) {
		_pause_page = PAUSE_OPTIONS; _pause_selection = 6;
	} else if (_pause_page == PAUSE_KEYS || _pause_page == PAUSE_PAD) {
		_pause_selection = _pause_page == PAUSE_KEYS ? 0 : 1;
		_pause_page = PAUSE_BINDINGS;
	} else if (_pause_page == PAUSE_CONTROLS) {
		_pause_page = PAUSE_BINDINGS; _pause_selection = 2;
	} else if (_pause_page == PAUSE_BINDINGS) {
		_pause_page = PAUSE_MAIN; _pause_selection = 2;
	} else {
		_pause_selection = _pause_page == PAUSE_CONTROLS ? 2 : (_pause_page == PAUSE_QUIT ? 6 : 1);
		_pause_page = PAUSE_MAIN;
	}
	return false;
}

static bool pause_activate() {
	_pause_notice = NULL;
	if(_pause_page==PAUSE_GAME) {
		if(_pause_selection==2)return pause_back();
		_save_browser_load=_pause_selection==1;
		for(int i=0;i<SAVE_SLOT_COUNT;++i)pause_refresh_save(i);
		_pause_page=PAUSE_SLOTS;_pause_selection=_save_slot;return false;
	}
	if(_pause_page==PAUSE_SLOTS) {
		if(_pause_selection==SAVE_SLOT_COUNT)return pause_back();
		_save_slot=_pause_selection;
		if(_save_browser_load && _save_slots[_save_slot].status!=SAVE_READY){_pause_notice=_save_slots[_save_slot].reason;return false;}
		_pause_page=_save_browser_load ? PAUSE_LOAD_CONFIRM : PAUSE_SAVE_CONFIRM;
		_pause_selection=0;return false;
	}
	if(_pause_page==PAUSE_SAVE_CONFIRM || _pause_page==PAUSE_LOAD_CONFIRM) {
		if(_pause_selection==0)return pause_back();
		const bool load=_pause_page==PAUSE_LOAD_CONFIRM;
		const bool success=load ? (_pause_options.prepare_load && _pause_options.prepare_load(_save_slot)) : (_pause_options.save_game && _pause_options.save_game(_save_slot));
		if(load && success){_pause_options.load_game=true;return true;}
		pause_back();pause_refresh_save(_save_slot);
		_pause_notice=_pause_options.save_error ? _pause_options.save_error() : "OPERACION NO DISPONIBLE";
		return false;
	}

	if (_pause_page == PAUSE_DEFAULTS) {
		if (_pause_selection == 0) return pause_back();
		_pause_options.camera=CAMERA_SMOOTH; _pause_options.hud=HUD_CLASSIC;
		_pause_options.render_rate=60; _pause_options.view_step=0;
		_pause_options.music_volume=_pause_options.effects_volume=100;
		_pause_options.invulnerable=_pause_options.infinite_lives=false;
		_pause_options.lives[0]=_pause_options.lives[1]=3;
		if (_pause_options.can_switch) _pause_options.amiga=false;
		_pause_options.reset_defaults=true;
		_last_music_volume=_last_effects_volume=100;
		memcpy(_edit_keys,_default_keys,sizeof(_edit_keys));
		_edit_buttons[0]=SDL_CONTROLLER_BUTTON_A; _edit_buttons[1]=SDL_CONTROLLER_BUTTON_B;
		SDL_SetWindowFullscreen(_window,0);
		allow_preferences_reset();
		pause_back(); _pause_notice="AJUSTES RESTAURADOS";
		return false;
	}

	if (_pause_page == PAUSE_CHARACTER_CONFIRM) {
		if (_pause_selection == 0) return pause_back();
		_pause_options.select_character = true;
		return true;
	}
	if (_pause_page == PAUSE_LEVEL_CONFIRM) {
		if (_pause_selection == 0) return pause_back();
		_pause_options.requested_level=_selected_level;
		return true;
	}
	if (_pause_page == PAUSE_LEVELS) {
		if (_pause_selection == pause_level_count()) return pause_back();
		if (!_pause_options.available_levels[_pause_options.amiga][_pause_selection]) { _pause_notice="FALTAN ARCHIVOS DE ESTE NIVEL"; return false; }
		_selected_level=_pause_selection; _pause_page=PAUSE_LEVEL_CONFIRM; _pause_selection=0;
		return false;
	}

	if (_pause_page == PAUSE_CONTROLS) return pause_back();
	if (_pause_page == PAUSE_QUIT) {
		if (_pause_selection == 1) { g_sys.input.quit = true; return true; }
		return pause_back();
	}
	if (_pause_page == PAUSE_ASSISTS) {
		if (_pause_selection == 4) return pause_back();
		pause_adjust(1); return false;
	}
	if (_pause_page == PAUSE_AUDIO) {
		if (_pause_selection == 2) return pause_back();
		int *value = _pause_selection == 0 ? &_pause_options.music_volume : &_pause_options.effects_volume;
		int *last = _pause_selection == 0 ? &_last_music_volume : &_last_effects_volume;
		if (*value) { *last = *value; *value = 0; }
		else *value = *last;
		return false;
	}
	if (_pause_page == PAUSE_OPTIONS) {
		if (_pause_selection < 6) pause_adjust(1);
		else if (_pause_selection == 6) { _pause_page = PAUSE_AUDIO; _pause_selection = 0; }
		else if (_pause_selection == 7) { _pause_page = PAUSE_ASSISTS; _pause_selection = 0; }
		else if (_pause_selection == 8) { _pause_page = PAUSE_DEFAULTS; _pause_selection = 0; }
		else return pause_back();
		return false;
	}
	if (_pause_page == PAUSE_BINDINGS) {
		if (_pause_selection < 2) { _pause_page = _pause_selection == 0 ? PAUSE_KEYS : PAUSE_PAD; _pause_selection = 0; }
		else if (_pause_selection == 2) { _pause_page = PAUSE_CONTROLS; _pause_selection = 0; }
		else if (_pause_selection == 3) {
			memcpy(_edit_keys,_default_keys,sizeof(_edit_keys));
			_edit_buttons[0]=SDL_CONTROLLER_BUTTON_A; _edit_buttons[1]=SDL_CONTROLLER_BUTTON_B;
			_pause_notice="CONTROLES RESTAURADOS";
		} else return pause_back();
		return false;
	}
	if (_pause_page == PAUSE_KEYS || _pause_page == PAUSE_PAD) {
		if (_pause_selection == pause_item_count()-1) return pause_back();
		_pause_capture = _pause_selection;
		_capture_ignore_key = SDLK_UNKNOWN; _capture_ignore_button = -1;
		_capture_key_candidate = SDLK_UNKNOWN; _capture_button_candidate = -1;
		return false;
	}
	if (_pause_selection == 0) return true;
	if (_pause_selection == 3) { _pause_page=PAUSE_LEVELS; _pause_selection=_pause_options.current_level; return false; }
	if (_pause_selection == 4 && !_pause_options.save_game) { _pause_notice="GUARDADOS TODAVIA NO DISPONIBLES"; return false; }
	if (_pause_selection == 4) { _pause_page=PAUSE_GAME; _pause_selection=0; return false; }
	if (_pause_selection == 5 && _pause_options.no_character_select) { _pause_notice="ESTE JUEGO NO TIENE SELECCION DE PERSONAJE"; return false; }
	if (_pause_selection == 5) { _pause_page=PAUSE_CHARACTER_CONFIRM; _pause_selection=0; return false; }
	_pause_page = _pause_selection == 1 ? PAUSE_OPTIONS : (_pause_selection == 2 ? PAUSE_BINDINGS : PAUSE_QUIT);
	_pause_selection = 0;
	return false;
}

static void pause_move(int direction) {
	const int count = pause_item_count();
	if (!count || _pause_capture >= 0) return;
	_pause_notice = NULL;
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
	if (ev->type == SDL_QUIT) {
#ifdef TITUS_MANAGED
        _pause_page=PAUSE_QUIT;_pause_selection=0;return false;
#else
        g_sys.input.quit = true; return true;
#endif
    }
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
		if (_pause_capture >= 0) {
			if (down) {
				if (!ev->key.repeat && key != _capture_ignore_key) _capture_key_candidate = key;
				return false;
			}
			if (key == _capture_ignore_key) { _capture_ignore_key = SDLK_UNKNOWN; return false; }
			if (key == SDLK_ESCAPE) { _pause_capture = -1; _pause_notice = NULL; return false; }
			if (_pause_page != PAUSE_KEYS || key != _capture_key_candidate) return false;
			_capture_key_candidate = SDLK_UNKNOWN;
			if (reserved_binding(key)) { _pause_notice = "TECLA RESERVADA"; return false; }
			assign_binding(_edit_keys,BIND_COUNT,_pause_capture,key);
			_pause_capture = -1; _pause_notice = NULL;
			return false;
		}
		if (!down && (key == SDLK_ESCAPE || key == SDLK_p)) return pause_back();
		if (!down) return false;
		if (key == SDLK_UP) pause_move(-1);
		if (key == SDLK_DOWN) pause_move(1);
		if (key == SDLK_LEFT) pause_adjust(-1);
		if (key == SDLK_RIGHT) pause_adjust(1);
		if (!ev->key.repeat && (key == SDLK_RETURN || key == SDLK_SPACE)) {
			const bool done = pause_activate();
			if (_pause_capture >= 0) _capture_ignore_key = key;
			return done;
		}
	}
	if (ev->type == SDL_CONTROLLERBUTTONDOWN || ev->type == SDL_CONTROLLERBUTTONUP) {
		const bool down = ev->type == SDL_CONTROLLERBUTTONDOWN;
		const int button = ev->cbutton.button;
		if (button >= SDL_CONTROLLER_BUTTON_MAX) return false;
		_blocked_buttons[button] = down;
		if (!_pause_focused) return false;
		if (_pause_capture >= 0) {
			if (down) {
				if (button != _capture_ignore_button) _capture_button_candidate = button;
				return false;
			}
			if (button == _capture_ignore_button) { _capture_ignore_button = -1; return false; }
			if (button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) { _pause_notice="BOTON RESERVADO";return false; }
			if (button == SDL_CONTROLLER_BUTTON_START) { _pause_capture = -1; _pause_notice = NULL; return false; }
			if (_pause_page != PAUSE_PAD || button != _capture_button_candidate) return false;
			_capture_button_candidate = -1;
			if (button >= SDL_CONTROLLER_BUTTON_DPAD_UP || button == SDL_CONTROLLER_BUTTON_BACK || button == SDL_CONTROLLER_BUTTON_GUIDE) {
				_pause_notice = "BOTON RESERVADO"; return false;
			}
			assign_binding(_edit_buttons,2,_pause_capture,button);
			_pause_capture = -1; _pause_notice = NULL;
			return false;
		}
		if (down && button == SDL_CONTROLLER_BUTTON_DPAD_UP) pause_move(-1);
		if (down && button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) pause_move(1);
		if (down && button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) pause_adjust(-1);
		if (down && button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) pause_adjust(1);
		if (down && button == SDL_CONTROLLER_BUTTON_A) {
			const bool done = pause_activate();
			if (_pause_capture >= 0) _capture_ignore_button = button;
			return done;
		}
		if ((down && button == SDL_CONTROLLER_BUTTON_B) ||
			(!down && button == SDL_CONTROLLER_BUTTON_START)) return pause_back();
	}
	if (ev->type == SDL_CONTROLLERAXISMOTION || ev->type == SDL_JOYAXISMOTION) {
		const bool controller = ev->type == SDL_CONTROLLERAXISMOTION;
		const int axis = controller ? ev->caxis.axis : ev->jaxis.axis;
		const int value = controller ? ev->caxis.value : ev->jaxis.value;
		if (controller && axis < SDL_CONTROLLER_AXIS_MAX) _blocked_axes[axis] = abs(value) > 3200;
		if (!controller && axis < 2) _blocked_joy_axes[axis] = abs(value) > 3200;
		if (_pause_capture < 0 && axis == (controller ? SDL_CONTROLLER_AXIS_LEFTX : 0)) {
			const int direction = value < -16000 ? -1 : (value > 16000 ? 1 : 0);
			if (_pause_focused && direction && direction != _pause_xaxis) pause_adjust(direction);
			_pause_xaxis = direction;
		}
		if (axis == (controller ? SDL_CONTROLLER_AXIS_LEFTY : 1)) {
			const int direction = value < -16000 ? -1 : (value > 16000 ? 1 : 0);
			if (_pause_focused && direction && direction != _pause_axis) pause_move(direction);
			_pause_axis = direction;
		}
	}
	if (ev->type == SDL_JOYHATMOTION) {
		_blocked_hat = ev->jhat.value != SDL_HAT_CENTERED;
		const int direction = ev->jhat.value & SDL_HAT_UP ? -1 : (ev->jhat.value & SDL_HAT_DOWN ? 1 :
			(ev->jhat.value & SDL_HAT_LEFT ? -2 : (ev->jhat.value & SDL_HAT_RIGHT ? 2 : 0)));
		if (_pause_focused && direction && direction != _pause_hat) {
			if (abs(direction) == 1) pause_move(direction);
			else pause_adjust(direction/2);
		}
		_pause_hat = direction;
	}
	if (ev->type == SDL_JOYBUTTONDOWN || ev->type == SDL_JOYBUTTONUP) {
		const int button = ev->jbutton.button;
		const bool down = ev->type == SDL_JOYBUTTONDOWN;
		if (button < 32) _blocked_joy_buttons[button] = down;
		if (_pause_capture >= 0) {
			if (!down && button == 7) _pause_capture = -1;
			return false;
		}
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
#ifdef TITUS_MANAGED
    titus_runtime_probe_paused(1);
#endif
	clear_pause_input();
	remember_held_controls();
	sdl2_set_scroll_interpolation(0, 0, 0, false);
	_pause_page = PAUSE_MAIN;
	_pause_selection = _pause_axis = _pause_xaxis = _pause_hat = 0;
	_pause_capture = -1; _pause_notice = _pause_options.load_notice;
	if (_pause_notice) _pause_page=PAUSE_GAME;
	if (_pause_options.music_volume > 0) _last_music_volume = _pause_options.music_volume;
	if (_pause_options.effects_volume > 0) _last_effects_volume = _pause_options.effects_volume;
	_capture_ignore_key = SDLK_UNKNOWN; _capture_ignore_button = -1;
	memcpy(_edit_keys,_game_keys,sizeof(_game_keys));
	memcpy(_edit_buttons,_game_buttons,sizeof(_game_buttons));
	_pause_focused = _window_has_focus;
	_pause_menu_open = true;
#ifdef TITUS_MANAGED
    if(_managed_return_pending){_pause_page=PAUSE_QUIT;_pause_selection=0;_managed_return_pending=false;}
    bool managed_confirmation=false;
#endif
	bool done = false;
	while (!done && !g_sys.input.quit) {
#ifdef TITUS_MANAGED
        if(titus_runtime_poll()){_pause_page=PAUSE_QUIT;_pause_selection=0;}
        if(_pause_page==PAUSE_QUIT&&!managed_confirmation){titus_runtime_return_request(1);managed_confirmation=true;}
        if(_pause_page!=PAUSE_QUIT&&managed_confirmation){titus_runtime_return_cancelled();managed_confirmation=false;}
#endif
		sdl2_draw_screen(0.f, 0.f, 1.f);
		SDL_Event ev;
		if (SDL_WaitEventTimeout(&ev, 16)) {
			done = pause_event(&ev);
			while (!done && SDL_PollEvent(&ev)) done = pause_event(&ev);
		}
	}
#ifdef TITUS_MANAGED
    if(managed_confirmation&&!g_sys.input.quit)titus_runtime_return_cancelled();
#endif
	remember_held_controls();
	if (!g_sys.input.quit) {
		memcpy(_game_keys,_edit_keys,sizeof(_game_keys));
		memcpy(_game_buttons,_edit_buttons,sizeof(_game_buttons));
		/* Set ramps before audio resumes, so the first callback sees them. */
		if (_pause_options.apply_volumes) _pause_options.apply_volumes(_pause_options.music_volume,_pause_options.effects_volume);
	}
	clear_pause_input();
	_pause_menu_open = false;
	SDL_DestroyTexture(_save_preview_texture);_save_preview_texture=NULL;_preview_texture_slot=-1;
	sdl2_draw_screen(0.f, 0.f, 1.f);
	if (resume_audio && _window_has_focus && !g_sys.input.quit) SDL_PauseAudio(0);
}
