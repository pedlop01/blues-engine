/* Private SDL preferences: versioned text, validated values, atomic replacement. */
#include <errno.h>
#include <ctype.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
extern struct options_t g_options;
static bool _preferences_writable = true;
static void allow_preferences_reset() { _preferences_writable = true; }

static FILE *preferences_open(const char *path, bool write) {
#ifdef _WIN32
	wchar_t *wide=path_utf16(path);
	FILE *fp=wide ? _wfopen(wide,write ? L"wb" : L"rb") : NULL;
	free(wide);
	return fp;
#else
	return fopen(path,write ? "wb" : "rb");
#endif
}

static bool preferences_replace(const char *temporary, const char *path) {
#ifdef _WIN32
	wchar_t *from=path_utf16(temporary);
	wchar_t *to=path_utf16(path);
	const bool success=from && to && MoveFileExW(from,to,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
	if (!success && from) _wremove(from);
	free(from); free(to);
	return success;
#else
	const bool success=rename(temporary,path)==0;
	if (!success) remove(temporary);
	return success;
#endif
}

static char *preferences_path() {
	const char *override = SDL_getenv("BLUES_CONFIG_FILE");
	if (override) return *override ? SDL_strdup(override) : NULL;
	char *directory = SDL_GetPrefPath("pedlop01", "blues-engine");
	if (!directory) return NULL;
	const size_t size = strlen(directory) + sizeof("preferences.ini");
	char *path = SDL_malloc(size);
	if (path) snprintf(path,size,"%spreferences.ini",directory);
	SDL_free(directory);
	return path;
}

static char *preferences_trim(char *s) {
	while (isspace((unsigned char)*s)) ++s;
	char *end = s + strlen(s);
	while (end > s && isspace((unsigned char)end[-1])) --end;
	*end = 0;
	return s;
}

static bool preferences_number(const char *s, int low, int high, int *out) {
	char *end;
	errno = 0;
	long value = strtol(s,&end,10);
	if (!*s || *end || errno || value < low || value > high) return false;
	*out = (int)value;
	return true;
}

static bool preferences_keys_valid(const int *keys) {
	for (int i=0;i<BIND_COUNT;++i) {
		if (reserved_binding(keys[i])) return false;
		/* SDL's keymap is not initialized when main loads preferences. The
		 * static scancode names and printable keycodes work before video. */
		if (keys[i] & SDLK_SCANCODE_MASK) {
			const int scan=keys[i]&~SDLK_SCANCODE_MASK;
			if (scan<=SDL_SCANCODE_UNKNOWN || scan>=SDL_NUM_SCANCODES || !*SDL_GetScancodeName(scan)) return false;
		} else if (!((keys[i]>=32 && keys[i]<=0x10FFFF && !(keys[i]>=0x7F && keys[i]<=0x9F) && !(keys[i]>=0xD800 && keys[i]<=0xDFFF)) || keys[i]==SDLK_BACKSPACE || keys[i]==SDLK_TAB || keys[i]==SDLK_CLEAR || keys[i]==SDLK_DELETE)) return false;
		for (int j=0;j<i;++j) if (keys[i] == keys[j]) return false;
	}
	return true;
}

static bool preferences_button_valid(int button) {
	return button >= 0 && button < SDL_CONTROLLER_BUTTON_DPAD_UP &&
		button != SDL_CONTROLLER_BUTTON_LEFTSHOULDER && button != SDL_CONTROLLER_BUTTON_BACK && button != SDL_CONTROLLER_BUTTON_GUIDE && button != SDL_CONTROLLER_BUTTON_START;
}

static void sdl2_load_preferences() {
	_preferences_writable = true;
	char *path = preferences_path();
	FILE *fp = path ? preferences_open(path,false) : NULL;
	SDL_free(path);
	if (!fp) return;
	char line[256];
	bool version = false;
	int keys[BIND_COUNT], buttons[2];
	memcpy(keys,_game_keys,sizeof(keys));
	memcpy(buttons,_game_buttons,sizeof(buttons));
	for (int lines=0;lines<1024 && fgets(line,sizeof(line),fp);++lines) {
		if (!strchr(line,'\n') && !feof(fp)) {
			int c; while ((c=fgetc(fp)) != '\n' && c != EOF) {}
			continue;
		}
		char *key = preferences_trim(line);
		if (!*key || *key == '#' || *key == ';') continue;
		char *value = strchr(key,'=');
		if (!value) continue;
		*value++ = 0; key = preferences_trim(key); value = preferences_trim(value);
		int number;
		if (!version) {
			if (strcmp(key,"version") || !preferences_number(value,1,INT_MAX,&number)) break;
			if (number != 1) { _preferences_writable = false; break; }
			version = true; continue;
		}
		if (!strcmp(key,"camera")) {
			if (!strcmp(value,"classic")) g_options.camera_mode=CAMERA_CLASSIC;
			if (!strcmp(value,"smooth")) g_options.camera_mode=CAMERA_SMOOTH;
		} else if (!strcmp(key,"hud")) {
			if (!strcmp(value,"classic")) g_options.hud_mode=HUD_CLASSIC;
			if (!strcmp(value,"fixed")) g_options.hud_mode=HUD_FIXED;
		} else if (!strcmp(key,"mode")) {
			if (!strcmp(value,"dos")) g_options.preferred_amiga=0;
			if (!strcmp(value,"amiga")) g_options.preferred_amiga=1;
		} else if (!strcmp(key,"render_rate")) {
			if (preferences_number(value,30,120,&number) && (number==30 || number==60 || number==120)) g_options.scroll_rate=number;
		} else if (!strcmp(key,"view_step")) {
			if (preferences_number(value,0,8,&number)) g_sys.input.view_step=number;
		} else if (!strcmp(key,"music_volume")) {
			if (preferences_number(value,0,100,&number)) g_options.music_volume=number;
		} else if (!strcmp(key,"effects_volume")) {
			if (preferences_number(value,0,100,&number)) g_options.effects_volume=number;
		} else if (!strcmp(key,"music_restore")) {
			if (preferences_number(value,1,100,&number)) _last_music_volume=number;
		} else if (!strcmp(key,"effects_restore")) {
			if (preferences_number(value,1,100,&number)) _last_effects_volume=number;
		} else if (!strcmp(key,"fullscreen")) {
			if (preferences_number(value,0,1,&number)) g_options.fullscreen=number;
		} else if (!strcmp(key,"cheats")) {
			if (preferences_number(value,0,7,&number)) g_options.cheats=number;
		} else {
			for (int i=0;i<2;++i) {
				char name[32]; snprintf(name,sizeof(name),"initial_lives_%d",i+1);
				if (!strcmp(key,name) && preferences_number(value,1,99,&number)) g_options.initial_lives[i]=number;
				snprintf(name,sizeof(name),"button_%d",i);
				if (!strcmp(key,name) && preferences_number(value,0,SDL_CONTROLLER_BUTTON_MAX-1,&number) && preferences_button_valid(number)) buttons[i]=number;
			}
			for (int i=0;i<BIND_COUNT;++i) {
				char name[32]; snprintf(name,sizeof(name),"key_%d",i);
				if (!strcmp(key,name) && preferences_number(value,1,INT_MAX,&number)) keys[i]=number;
			}
		}
	}
	fclose(fp);
	if (preferences_keys_valid(keys)) memcpy(_game_keys,keys,sizeof(keys));
	if (buttons[0]!=buttons[1]) memcpy(_game_buttons,buttons,sizeof(buttons));
}

static void sdl2_save_preferences() {
	if (!_preferences_writable) return;
	char *path = preferences_path();
	if (!path) return;
	char *temporary = SDL_malloc(strlen(path)+32);
	if (!temporary) { SDL_free(path); return; }
#ifdef _WIN32
	const unsigned long process_id=GetCurrentProcessId();
#else
	const unsigned long process_id=(unsigned long)getpid();
#endif
	sprintf(temporary,"%s.tmp.%lu",path,process_id);
	FILE *fp = preferences_open(temporary,true);
	bool success = false;
	if (fp) {
		if (_window) g_options.fullscreen=(SDL_GetWindowFlags(_window)&SDL_WINDOW_FULLSCREEN_DESKTOP)!=0;
		fprintf(fp,"# Blues Engine preferences; game progress is stored separately.\nversion=1\n");
		fprintf(fp,"mode=%s\ncamera=%s\nhud=%s\nrender_rate=%d\nview_step=%d\n",
			g_options.preferred_amiga==1 ? "amiga" : "dos",
			g_options.camera_mode==CAMERA_CLASSIC ? "classic" : "smooth",
			g_options.hud_mode==HUD_FIXED ? "fixed" : "classic",g_options.scroll_rate,g_sys.input.view_step);
		fprintf(fp,"music_volume=%d\neffects_volume=%d\nfullscreen=%d\ncheats=%u\ninitial_lives_1=%d\ninitial_lives_2=%d\n",
			g_options.music_volume,g_options.effects_volume,g_options.fullscreen,g_options.cheats&7,
			g_options.initial_lives[0],g_options.initial_lives[1]);
		fprintf(fp,"music_restore=%d\neffects_restore=%d\n",
			g_options.music_volume>0 ? g_options.music_volume : _last_music_volume,
			g_options.effects_volume>0 ? g_options.effects_volume : _last_effects_volume);
		for (int i=0;i<BIND_COUNT;++i) fprintf(fp,"key_%d=%d\n",i,_game_keys[i]);
		for (int i=0;i<2;++i) fprintf(fp,"button_%d=%d\n",i,_game_buttons[i]);
		success = !ferror(fp);
		if (fclose(fp)) success=false;
		if (success) success=preferences_replace(temporary,path);
		else {
#ifdef _WIN32
			wchar_t *wide=path_utf16(temporary);
			if (wide) _wremove(wide);
			free(wide);
#else
			remove(temporary);
#endif
		}
	}
	if (!success) { print_warning("Could not save preferences to '%s'",path); }
	SDL_free(temporary); SDL_free(path);
}
