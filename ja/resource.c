
#include "resource.h"
#include "unpack.h"
#include "util.h"

static const int TMP_SIZE = 72728;

static const int BACKGROUND_SIZE = 320 * 200;

static const char *_datapath;

struct resource_t g_res;

void res_init(const char *path, int vga_size) {
	_datapath = path;

	static const int FONT_SIZE = 7264;
	g_res.font = (uint8_t *)malloc(FONT_SIZE);
	if (!g_res.font) {
		print_error("Failed to allocate font buffer, %d bytes", FONT_SIZE);
	}
	unpack(_datapath, "font256.eat", g_res.font, FONT_SIZE);

	static const int BOARD_SIZE = 12800;
	g_res.board = (uint8_t *)malloc(BOARD_SIZE);
	if (!g_res.board) {
		print_error("Failed to allocate board buffer, %d bytes", BOARD_SIZE);
	}
	unpack(_datapath, "board.eat", g_res.board, BOARD_SIZE);

	static const int MOTIF_SIZE = 320 * 25 * 9;
	g_res.motif = (uint8_t *)malloc(MOTIF_SIZE);
	if (!g_res.motif) {
		print_error("Failed to allocate motif buffer, %d bytes", MOTIF_SIZE);
	}
	unpack(_datapath, "motif.eat", g_res.motif, MOTIF_SIZE);

	static const int SPRITES_SIZE = 108044;
	g_res.sprites = (uint8_t *)malloc(SPRITES_SIZE);
	if (!g_res.sprites) {
		print_error("Failed to allocate sprites buffer, %d bytes", SPRITES_SIZE);
	}
	unpack(_datapath, "sprites.eat", g_res.sprites, SPRITES_SIZE);

	static const int SAMPLES_SIZE = 49398;
	g_res.samples = (uint8_t *)malloc(SAMPLES_SIZE);
	if (!g_res.samples) {
		print_error("Failed to allocate samples buffer, %d bytes", SAMPLES_SIZE);
	}
	unpack(_datapath, "samples.eat", g_res.samples, SAMPLES_SIZE);

	g_res.tmp = (uint8_t *)malloc(TMP_SIZE);
	if (!g_res.tmp) {
		print_error("Failed to allocate tmp buffer, %d bytes", TMP_SIZE);
	}

	g_res.background = (uint8_t *)malloc(BACKGROUND_SIZE);
	if (!g_res.background) {
		print_error("Failed to allocate tmp buffer, %d bytes", BACKGROUND_SIZE);
	}

	g_res.vga = (uint8_t *)malloc(vga_size);
	if (!g_res.vga) {
		print_error("Failed to allocate vga buffer, %d bytes", vga_size);
	}
}

void res_fini() {
	free(g_res.font);
	g_res.font = 0;
	free(g_res.board);
	g_res.board = 0;
	free(g_res.motif);
	g_res.motif = 0;
	free(g_res.sprites);
	g_res.sprites = 0;
	free(g_res.samples);
	g_res.samples = 0;
	free(g_res.tmp);
	g_res.tmp = 0;
	free(g_res.background);
	g_res.background = 0;
	free(g_res.vga);
	g_res.vga = 0;
}

int load_file(const char *filename) {
	print_debug(DBG_RESOURCE, "load_file '%s'", filename);
	return unpack(_datapath, filename, g_res.tmp, TMP_SIZE);
}
