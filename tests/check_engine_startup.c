/* Bounded startup/lifecycle smoke test using the production entry point.
 * Requires user-supplied game data; this does not validate gameplay or audio. */
#include <SDL.h>
static void smoke_present(SDL_Renderer *renderer);
#define SDL_RenderPresent smoke_present
#include "../sys_sdl2.c"
#undef SDL_RenderPresent
#define main production_main
#include "../main.c"
#undef main

static void (*production_events)(void);
static uint32_t started;
static unsigned presents;
static bool requested_quit;
static bool visible_content;

static void smoke_events(void) {
	production_events();
	if (SDL_GetTicks() - started >= 2500) {
		g_sys.input.quit = true;
		requested_quit = true;
	}
}

static void smoke_present(SDL_Renderer *renderer) {
	++presents;
	if (!visible_content) {
		int w, h;
		if (SDL_GetRendererOutputSize(renderer, &w, &h) != 0 || w <= 0 || h <= 0) abort();
		uint32_t *pixels = malloc((size_t)w * h * sizeof(*pixels));
		if (!pixels || SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, pixels, w * 4) != 0) abort();
		for (int i = 1; i < w * h; ++i) {
			if (pixels[i] != pixels[0]) { visible_content = true; break; }
		}
		free(pixels);
	}
	SDL_RenderPresent(renderer);
}

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s GAME_DATA_DIRECTORY\n", argv[0]);
		return 2;
	}
	/* Neither read nor write the player's preferences. */
	SDL_setenv("BLUES_CONFIG_FILE", "", 1);
	production_events = g_sys.process_events;
	g_sys.process_events = smoke_events;
	char *option = malloc(strlen(argv[1]) + sizeof("--datapath="));
	if (!option) return 2;
	sprintf(option, "--datapath=%s", argv[1]);
	char *args[] = {argv[0], option, NULL};
	started = SDL_GetTicks();
	const int result = production_main(2, args);
	free(option);
	fprintf(stdout, "startup-smoke: frames=%u visible=%d requested_quit=%d exit=%d\n",
	        presents, visible_content, requested_quit, result);
	return result || !presents || !visible_content || !requested_quit;
}
