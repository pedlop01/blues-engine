
#include <getopt.h>
#include <sys/stat.h>
#include "sys.h"
#include "mixer.h"
#include "util.h"

struct options_t g_options;

static const char *DEFAULT_DATA_PATH = ".";

static const int DEFAULT_SCALE_FACTOR = 2;

static const char *DEFAULT_SCALE_FILTER = 0; // nearest pixel sampling

static const char *USAGE =
	"Usage: %s [OPTIONS]...\n"
	"  --datapath=PATH   Path to data files (default '.')\n"
	"  --dos-datapath=PATH    DOS resources for live mode switching\n"
	"  --amiga-datapath=PATH  Amiga resources for live mode switching\n"
	"  F6 during Blues Brothers gameplay switches DOS/Amiga\n"
	"  --level=NUM       Start at level NUM\n"
	"  --cheats=MASK     Cheats mask\n"
	"  --startpos=XxY    Start at position (X,Y)\n"
	"  --fullscreen      Enable fullscreen\n"
	"  --scale=N         Graphics scaling factor (default 2)\n"
	"  --filter=NAME     Graphics scaling filter (default 'nearest')\n"
	"  --screensize=WxH  Graphics screen size (default 320x200)\n"
	"  --scrollrate=HZ   Scroll presentation rate: 30, 60, or 120 (default 60)\n"
	"  --sound=MODE      Blues Brothers audio: auto, dos, or amiga (default auto)\n"
	"  In-game framing (Blues Brothers): - reveals more map, + moves closer, 0 resets\n"
	"  --cga             Enable CGA colors\n"
	"  --hud=MODE       Blues Brothers HUD: classic or fixed (default classic)\n"
	"  --camera=MODE    Blues Brothers camera: classic or smooth (default smooth)\n"
	"  --dosscroll       Enable DOS style screen scrolling\n"
	"  --hybrid          Enable fuchsia color as in Hybrid crack\n"
;

extern struct game_t game_bb;

static struct game_t *detect_game(const char *data_path) {
#if !defined(BLUES_ONLY)
	extern struct game_t game_ja;
	extern struct game_t game_p2;
#endif
	static struct {
		struct game_t *game;
		const char *filename;
		uint16_t size;
	} games[] = {
		{ &game_bb, "MAGASIN.BIN",  2560 },
#if !defined(BLUES_ONLY)
		{ &game_ja, "JARDIN.EAT",  24876 },
		{ &game_p2, "MOTIF.SQZ",    9396 },
#endif
		{ 0, 0, 0 }
	};
	for (int i = 0; games[i].game; ++i) {
		FILE *fp = fopen_nocase(data_path, games[i].filename);
		if (fp) {
			fclose(fp);
			return games[i].game;
		}
	}
	return 0;
}

int main(int argc, char *argv[]) {
	const char *data_path = DEFAULT_DATA_PATH;
	int scale_factor = DEFAULT_SCALE_FACTOR;
	const char *scale_filter = DEFAULT_SCALE_FILTER;
	bool fullscreen = false;
	g_options.initial_lives[0] = g_options.initial_lives[1] = 3;
	g_options.preferred_amiga = -1;
	g_options.start_xpos16 = -1;
	g_options.start_ypos16 = -1;
	g_options.scroll_rate = 60;
	g_options.music_volume = g_options.effects_volume = 100;
	g_options.camera_mode = CAMERA_SMOOTH;
	g_options.amiga_copper_bars = true;
	g_options.amiga_colors = true;
	// g_options.amiga_status_bar = true;
#if defined(PSP)
	g_options.screen_w = 480;
	g_options.screen_h = 272;
	g_options.jump_button = true;
#else
	g_options.screen_w = 320;
	g_options.screen_h = 200;
	// g_options.jump_button = true;
	if (g_sys.load_preferences) g_sys.load_preferences();
	fullscreen = g_options.fullscreen;
	if (argc == 2) {
		struct stat st;
		if (stat(argv[1], &st) == 0 && S_ISDIR(st.st_mode)) {
			data_path = strdup(argv[1]);
		}
	}
	while (1) {
		static struct option options[] = {
			{ "datapath",   required_argument, 0, 1 },
			{ "level",      required_argument, 0, 2 },
			{ "debug",      required_argument, 0, 3 },
			{ "cheats",     required_argument, 0, 4 },
			{ "startpos",   required_argument, 0, 5 },
			{ "fullscreen", no_argument,       0, 6 },
			{ "scale",      required_argument, 0, 7 },
			{ "filter",     required_argument, 0, 8 },
			{ "screensize", required_argument, 0, 9 },
			{ "cga",        no_argument,       0, 10 },
			{ "dosscroll",  no_argument,       0, 11 },
			{ "hybrid",     no_argument,       0, 12 },
			{ "scrollrate", required_argument, 0, 13 },
			{ "sound",      required_argument, 0, 14 },
			{ "dos-datapath", required_argument, 0, 15 },
			{ "amiga-datapath", required_argument, 0, 16 },
			{ "camera", required_argument, 0, 17 },
			{ "hud", required_argument, 0, 18 },
			{ 0, 0, 0, 0 },
		};
		int index;
		const int c = getopt_long(argc, argv, "", options, &index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 1:
			data_path = strdup(optarg);
			break;
		case 2:
			g_options.start_level = atoi(optarg);
			break;
		case 3:
			g_debug_mask = atoi(optarg);
			break;
		case 4:
			g_options.cheats = atoi(optarg);
			break;
		case 5:
			sscanf(optarg, "%dx%d", &g_options.start_xpos16, &g_options.start_ypos16);
			break;
		case 6:
			fullscreen = true;
			break;
		case 7:
			scale_factor = atoi(optarg);
			break;
		case 8:
			scale_filter = strdup(optarg);
			break;
		case 9:
			if (sscanf(optarg, "%dx%d", &g_options.screen_w, &g_options.screen_h) == 2) {
				// align to tile 16x16
				g_options.screen_w =  (g_options.screen_w + 15) & ~15;
				g_options.screen_h = ((g_options.screen_h + 15) & ~15) + 40; // PANEL_H;
				// do not allow lower resolution than the original
				if (g_options.screen_w < 320) {
					g_options.screen_w = 320;
				}
				if (g_options.screen_h < 200) {
					g_options.screen_h = 200;
				}
			}
			break;
		case 10:
			g_options.cga_colors = true;
			break;
		case 11:
			g_options.dos_scrolling = true;
			g_options.camera_mode = CAMERA_CLASSIC;
			break;
		case 12:
			g_options.hybrid_color = true;
			break;
		case 13: {
			char *end;
			const long scroll_rate = strtol(optarg, &end, 10);
			if (*optarg == '\0' || *end != '\0' || (scroll_rate != 30 && scroll_rate != 60 && scroll_rate != 120)) {
				fprintf(stderr, "Unsupported scroll rate '%s' (use 30, 60, or 120)\n", optarg);
				return -1;
			}
			g_options.scroll_rate = (int)scroll_rate;
			break;
		}
		case 14:
			if (strcmp(optarg, "auto") == 0) g_options.sound_mode = SOUND_AUTO;
			else if (strcmp(optarg, "dos") == 0) g_options.sound_mode = SOUND_DOS;
			else if (strcmp(optarg, "amiga") == 0) g_options.sound_mode = SOUND_AMIGA;
			else {
				fprintf(stderr, "Unsupported sound mode '%s' (use auto, dos, or amiga)\n", optarg);
				return -1;
			}
			break;
		case 15:
			free((char *)g_options.dos_datapath);
			g_options.dos_datapath = strdup(optarg);
			break;
		case 18:
			if (strcmp(optarg, "classic") == 0) g_options.hud_mode = HUD_CLASSIC;
			else if (strcmp(optarg, "fixed") == 0) g_options.hud_mode = HUD_FIXED;
			else {
				fprintf(stderr, "Unsupported HUD mode '%s' (use classic or fixed)\n", optarg);
				return -1;
			}
			break;
		case 17:
			if (strcmp(optarg, "classic") == 0) g_options.camera_mode = CAMERA_CLASSIC;
			else if (strcmp(optarg, "smooth") == 0) g_options.camera_mode = CAMERA_SMOOTH;
			else {
				fprintf(stderr, "Unsupported camera mode '%s' (use classic or smooth)\n", optarg);
				return -1;
			}
			break;
		case 16:
			free((char *)g_options.amiga_datapath);
			g_options.amiga_datapath = strdup(optarg);
			break;
		default:
			fprintf(stdout, USAGE, argv[0]);
			return -1;
		}
	}
#endif
	if (data_path == DEFAULT_DATA_PATH) {
		const char *initial = g_options.preferred_amiga==1 && g_options.amiga_datapath ? g_options.amiga_datapath :
			(g_options.dos_datapath ? g_options.dos_datapath : g_options.amiga_datapath);
		if (initial) data_path = strdup(initial);
	}
	struct game_t *game = detect_game(data_path);
	if (!game) {
		fprintf(stdout, "No data files found\n");
	} else {
		g_sys.init();
		g_sys.set_screen_size(GAME_SCREEN_W, GAME_SCREEN_H, game->name, scale_factor, scale_filter, fullscreen);
		g_sys.set_scroll_render_rate(g_options.scroll_rate);
		game->res_init(data_path, GAME_SCREEN_W * GAME_SCREEN_H);
		game->snd_init();
		g_mix.set_volumes(g_options.music_volume,g_options.effects_volume);
		g_mix.init();
		game->run();
		if (game == &game_bb && g_sys.save_preferences) g_sys.save_preferences();
		g_mix.fini();
		game->snd_fini();
		game->res_fini();
		g_sys.fini();
	}
	if (data_path != DEFAULT_DATA_PATH) {
		free((char *)data_path);
	}
	if (scale_filter != DEFAULT_SCALE_FILTER) {
		free((char *)scale_filter);
	}
	free((char *)g_options.dos_datapath);
	free((char *)g_options.amiga_datapath);
	return 0;
}
