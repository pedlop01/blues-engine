/* Reference input run through the production engine and SDL keyboard/audio.
 * No game data included. Menus are accepted automatically; fades are skipped.
 * This is a short movement/jump/attack route, not a complete campaign. */
#include "../sys_sdl2.c"
#if defined(PROBE_BB)
#define g_vars game_bb_g_vars
#define g_res game_bb_g_res
#include "../bb/game.h"
#include "../bb/resource.h"
extern struct game_t game_bb;
#define PROBE_GAME game_bb
#define PLAYER_X player->xpos16
#define PLAYER_Y player->ypos16
#elif defined(PROBE_JA)
#if defined(PROBE_BUNDLED)
#define g_vars game_ja_g_vars
#define g_res game_ja_g_res
#include "../ja/game.h"
#include "../ja/resource.h"
extern struct game_t game_ja;
#else
#include "../ja/game.c"
#endif
#define PROBE_GAME game_ja
#else
#if defined(PROBE_BUNDLED)
#define g_vars game_p2_g_vars
#define g_res game_p2_g_res
#include "../p2/game.h"
#include "../p2/resource.h"
extern struct game_t game_p2;
#else
#include "../p2/game.c"
#endif
#define PROBE_GAME game_p2
#endif
#if !defined(PROBE_BB)
#define PLAYER_X player->x_pos
#define PLAYER_Y player->y_pos
#endif
#include "../mixer.h"

struct options_t g_options;
static uint32_t started;
static int ticks, baseline_x, baseline_y, right_max, left_min, jump_min;
static bool playing, sent_quit;
static SDL_Keycode held;
static sys_audio_cb engine_audio;
static void *engine_audio_context;
static unsigned long samples, nonzero, clipping;
static int peak;
static FILE *pcm_output;
static unsigned attack_effects;
static void (*engine_sound)(const uint8_t *, uint32_t, int, mix_repeat_sound_cb);

static void probe_sound(const uint8_t *data, uint32_t size, int frequency, mix_repeat_sound_cb repeat) {
	if (ticks >= 95 && ticks < 120 && size && frequency > 0) {
		for (uint32_t i = 0; i < size; ++i) if (data[i]) { ++attack_effects; break; }
	}
	engine_sound(data, size, frequency, repeat);
}

static void probe_audio(void *context, uint8_t *bytes, int length) {
	engine_audio(engine_audio_context, bytes, length);
	if (pcm_output && fwrite(bytes, 1, length, pcm_output) != (size_t)length) abort();
	const int16_t *pcm = (const int16_t *)bytes;
	for (int i = 0; i < length / 2; ++i) {
		const int value = pcm[i], magnitude = abs(value);
		++samples;
		if (value) ++nonzero;
		if (value == -32768 || value == 32767) ++clipping;
		if (magnitude > peak) peak = magnitude;
	}
}

static void probe_start_audio(sys_audio_cb callback, void *context) {
	engine_audio = callback; engine_audio_context = context;
	sdl2_start_audio(probe_audio, NULL);
}

static void key(SDL_Keycode code, bool down) {
	SDL_Event event;
	memset(&event, 0, sizeof(event));
	event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	event.key.keysym.sym = code;
	if (SDL_PushEvent(&event) < 0) abort();
}

static void probe_events(void) {
#if defined(PROBE_BB)
	const bool in_level = _pause_menu_enabled;
	const struct object_t *player = &g_vars.objects[OBJECT_NUM_PLAYER1];
#elif defined(PROBE_JA)
	const bool in_level = g_vars.tilemap_data != NULL;
	const struct object_t *player = &g_vars.players_table[0].obj;
#else
	const bool in_level = g_res.leveldat != NULL;
	const struct object_t *player = &g_vars.objects_tbl[1];
#endif
	if (!in_level) {
		/* Real keyboard path, rather than writing the engine input flags. */
		key(SDLK_SPACE, true);
	} else {
		if (!playing) {
			playing = true;
			key(SDLK_SPACE, false);
		}
		if (ticks == 10) {
			baseline_x = right_max = left_min = PLAYER_X;
			baseline_y = jump_min = PLAYER_Y;
		}
		if (ticks >= 10 && ticks < 40) right_max = MAX(right_max, PLAYER_X);
		if (ticks == 40) left_min = PLAYER_X;
		if (ticks >= 40 && ticks < 65) left_min = MIN(left_min, PLAYER_X);
		if (ticks == 65) baseline_y = jump_min = PLAYER_Y;
		if (ticks >= 65 && ticks < 95) jump_min = MIN(jump_min, PLAYER_Y);
		const SDL_Keycode desired = ticks < 10 ? 0 : ticks < 40 ? SDLK_RIGHT :
			ticks < 65 ? SDLK_LEFT : ticks < 95 ? SDLK_LSHIFT : ticks < 120 ? SDLK_SPACE : 0;
		if (held != desired) {
			if (held) key(held, false);
			if (desired) key(desired, true);
			held = desired;
		}
		if (++ticks >= 140) {
			SDL_Event quit; memset(&quit, 0, sizeof(quit)); quit.type = SDL_QUIT;
			SDL_PushEvent(&quit); sent_quit = true;
		}
		if (ticks == 120 && getenv("TITUS_PROBE_SHOT")) {
			int w, h; SDL_GetRendererOutputSize(_renderer, &w, &h);
			SDL_Surface *image = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
			if (!image || SDL_RenderReadPixels(_renderer, NULL, image->format->format, image->pixels, image->pitch) != 0 ||
			    SDL_SaveBMP(image, getenv("TITUS_PROBE_SHOT")) != 0) abort();
			SDL_FreeSurface(image);
		}
	}
	if (SDL_GetTicks() - started > 20000) abort();
	sdl2_process_events();
}

static void no_fade(void) {}

int main(int argc, char **argv) {
	if (argc != 2) return 2;
	SDL_SetMainReady();
	SDL_setenv("BLUES_CONFIG_FILE", "", 1);
	g_options.screen_w = 320; g_options.screen_h = 200;
	g_options.start_level = 0; g_options.scroll_rate = 60; g_options.jump_button = true;
	g_options.start_xpos16 = g_options.start_ypos16 = -1;
	g_options.music_volume = g_options.effects_volume = 100;
#if defined(PROBE_BB)
	g_options.initial_lives[0] = g_options.initial_lives[1] = 3;
	g_options.preferred_amiga = -1;
	g_options.dos_datapath = argv[1];
#endif
	g_sys.init();
	g_sys.set_screen_size(GAME_SCREEN_W, GAME_SCREEN_H, PROBE_GAME.name, 1, "nearest", false);
	g_sys.process_events = probe_events;
	g_sys.start_audio = probe_start_audio;
	engine_sound = g_mix.play_sound; g_mix.play_sound = probe_sound;
	if (getenv("TITUS_PROBE_PCM")) {
		pcm_output = fopen(getenv("TITUS_PROBE_PCM"), "wb");
		if (!pcm_output) abort();
	}
	g_sys.fade_in_palette = g_sys.fade_out_palette = no_fade;
	PROBE_GAME.res_init(argv[1], GAME_SCREEN_W * GAME_SCREEN_H);
	PROBE_GAME.snd_init();
	g_mix.init();
	started = SDL_GetTicks();
	PROBE_GAME.run();
	g_mix.fini(); PROBE_GAME.snd_fini(); PROBE_GAME.res_fini(); g_sys.fini();
	if (pcm_output) fclose(pcm_output);
	printf("gameplay-probe: game=%s ticks=%d right=%d left=%d jump=%d attack_effects=%u samples=%lu nonzero=%lu peak=%d clipped=%lu quit=%d\n",
		PROBE_GAME.name, ticks, right_max-baseline_x, right_max-left_min, baseline_y-jump_min,
		attack_effects, samples, nonzero, peak, clipping, sent_quit);
#if !defined(PROBE_BB)
	if (!attack_effects) return 1;
#endif
	return !playing || !sent_quit || right_max <= baseline_x || left_min >= right_max ||
		jump_min >= baseline_y || nonzero < 100 || peak < 100;
}
