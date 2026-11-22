/* Integration test: actual keyboard handling, game loop, map redraw and SDL.
 * Includes private routines to verify camera bounds and preserve world state.
 * Build/run instructions are in README.md. */
#include "../sys_sdl2.c"
#include "../bb/level.c"
#define main blues_program_main
#include "../main.c"
#undef main

static int test_frames, saved_window_w, saved_window_h, base_w, base_h;
static int previous_w, previous_h, out_frames, closer_frames;
static int doors_tested;
static int death_trajectories_tested;
static int modes_tested;
static void snapshot(const char *name);
static sys_audio_cb captured_audio;
static void *captured_audio_param;
static void capture_start_audio(sys_audio_cb callback, void *param) {
	captured_audio = callback;
	captured_audio_param = param;
	sdl2_start_audio(callback, param);
}
static void no_sleep(int duration) {}
static void no_fade() {}
static void no_transition(enum sys_transition_e type, bool open) {}

static void test_modes() {
	const bool initial_amiga = g_res.amiga_data;
	struct object_t *player = &g_vars.objects[OBJECT_NUM_PLAYER1];
	player->lifes_count = 2;
	player->vinyls_count = 13;
	g_vars.found_music_instrument_flag = true;
	g_res.sql[100] ^= 1;
	const int edited = g_res.sql[100];
	g_res.triggers[5].unk16 = 7;
	update_level_view(base_w, base_h, 3);
	resize_level_view(GAME_SCREEN_W - 7, GAME_SCREEN_H - 5, true);
	const struct vars_t before = g_vars;
	const int w = GAME_SCREEN_W, h = GAME_SCREEN_H;
	/* Missing secondary resources must reject the switch before freeing data. */
	const char **other = initial_amiga ? &g_options.dos_datapath : &g_options.amiga_datapath;
	const char *valid_path = *other;
	*other = "/nonexistent/blues-resources";
	assert(!switch_data_mode());
	*other = valid_path;
	assert(memcmp(&before, &g_vars, sizeof(before)) == 0 && g_res.amiga_data == initial_amiga);
	for (int i = 0; i < 4; ++i) {
		/* Keep a sample active while replacing buffers to catch audio UAFs. */
		play_sound(SOUND_11);
		bool paused = false;
		handle_keyevent(SDLK_F6, false, &g_sys.input, &paused);
		assert(g_sys.input.switch_mode);
		g_sys.input.switch_mode = false;
		assert(switch_data_mode());
		assert(g_res.amiga_data == ((i & 1) ? initial_amiga : !initial_amiga));
		/* The atlas must match the same release as sprite anchor dimensions.
		 * Several DOS walking frames are 16px wide versus 32px on Amiga. */
		for (int frame = 0; frame < 101; ++frame) {
			assert(_spritesheets[RENDER_SPR_GAME].r[frame].w == READ_LE_UINT16(g_res.spr_frames[frame] - 2));
			assert(_spritesheets[RENDER_SPR_GAME].r[frame].h == READ_LE_UINT16(g_res.spr_frames[frame] - 4));
		}
		assert(memcmp(&before, &g_vars, sizeof(before)) == 0);
		assert(GAME_SCREEN_W == w && GAME_SCREEN_H == h);
		assert(g_res.sql[100] == edited && g_res.triggers[5].unk16 == 7);
		int16_t audio[SYS_AUDIO_FREQ];
		assert(captured_audio);
		g_sys.lock_audio();
		captured_audio(captured_audio_param, (uint8_t *)audio, sizeof(audio));
		g_sys.unlock_audio();
		int peak = 0;
		for (unsigned sample = 0; sample < ARRAYSIZE(audio); ++sample) peak = MAX(peak, abs(audio[sample]));
		assert(peak > 100);
		screen_clear_sprites(0);
		do_level_update_scrolling2();
		draw_level_panel();
		if (!g_res.amiga_data) draw_foreground_tiles();
		g_sys.copy_bitmap(g_res.vga, GAME_SCREEN_W, GAME_SCREEN_H);
		g_sys.update_screen();
		snapshot(g_res.amiga_data ? "mode-amiga" : "mode-dos");
		/* Run real actor updates with the newly decoded sprite atlases. */
		for (int frame = 0; frame < 3; ++frame) {
			screen_clear_sprites(0);
			do_level_update_objects();
			g_sys.copy_bitmap(g_res.vga, GAME_SCREEN_W, GAME_SCREEN_H);
			g_sys.update_screen();
		}
		SDL_Delay(100);
		/* Rendering can animate map tiles, but should not move objects. */
		g_vars = before;
		++modes_tested;
	}
	/* The same resource swap must preserve bonus room coordinates. */
	for (int i = 0; i < MAX_DOORS; ++i) {
		const struct door_t *door = &g_vars.doors[i];
		if (door->dst_y16 * 16 <= g_vars.screen_tilemap_h) continue;
		player->xpos16 = door->src_x16;
		player->ypos16 = door->src_y16;
		_view_camera_cut = false;
		do_level_enter_door(player);
		const struct vars_t room = g_vars;
		for (int j = 0; j < 2; ++j) {
			assert(switch_data_mode());
			assert(memcmp(&room, &g_vars, sizeof(room)) == 0);
			assert(camera_in_room() && GAME_SCREEN_W == 320 && GAME_SCREEN_H == 200);
			++modes_tested;
		}
		break;
	}
	g_sys.input.quit = true;
}

static void test_enemy_death() {
	int slot = -1, empty_tile = -1;
	for (int i = 0; i < MAX_OBJECTS; ++i) {
		int type = g_vars.objects[i].type;
		if (type > 2 && type < 100 && level_obj_type[type] == 2) { slot = i; break; }
	}
	assert(slot >= 0);
	for (int i = 0; i < MAX_TRIGGERS; ++i) {
		if (g_res.triggers[i].tile_type <= 1 && g_res.triggers[i].tile_flags == 0) { empty_tile = i; break; }
	}
	assert(empty_tile >= 0);
	uint8_t sql[640 * 25];
	memcpy(sql, g_res.sql, sizeof(sql));
	const struct vars_t saved = g_vars;
	static const int steps[] = { 0, 1, 4, 8 };
	for (unsigned z = 0; z < ARRAYSIZE(steps); ++z) {
		for (int left = 0; left < 2; ++left) {
			g_vars = saved;
			memset(g_res.sql, empty_tile, sizeof(sql));
			for (int i = 0; i < MAX_OBJECTS; ++i) {
				if (i != slot) { g_vars.objects[i].type = 100; g_vars.objects[i].visible_flag = 0; }
			}
			g_vars.objects[OBJECT_NUM_PLAYER1].grab_state = 0;
			g_vars.objects[OBJECT_NUM_PLAYER2].grab_state = 0;
			g_vars.screen_tilemap_xorigin = g_vars.screen_tilemap_yorigin = 0;
			update_level_view(base_w, base_h, steps[z]);
			if (z == 1) resize_level_view(GAME_SCREEN_W - 7, GAME_SCREEN_H - 5, false);
			g_vars.screen_tilemap_xorigin = g_vars.screen_tilemap_yorigin = 0;
			struct object_t *enemy = &g_vars.objects[slot];
			enemy->visible_flag = 1;
			enemy->xpos = GAME_SCREEN_W / 2;
			enemy->ypos = TILEMAP_SCREEN_H * 3 / 4;
			enemy->xpos16 = enemy->xpos >> 4;
			enemy->ypos16 = enemy->ypos >> 4;
			enemy->yvelocity = 0;
			enemy->yacc = 2;
			enemy->ymaxvelocity = 15;
			struct object_t crate = { 0 };
			crate.type = 2;
			crate.xpos = enemy->xpos;
			crate.ypos = enemy->ypos;
			crate.facing_left = left;
			do_level_handle_object_collision(&crate);
			assert(enemy->collide_flag && crate.type == 100 && enemy->yfriction == 12);
			int rose = 0, fell = 0, frames = 0, initial_y = enemy->ypos;
			while (enemy->visible_flag && frames < 120) {
				int before_x = enemy->xpos, before_y = enemy->ypos;
				screen_clear_sprites(0);
				do_level_update_objects();
				if (enemy->ypos < before_y) ++rose;
				if (enemy->ypos > before_y) ++fell;
				assert(left ? enemy->xpos <= before_x : enemy->xpos >= before_x);
				if (enemy->screen_ypos <= GAME_SCREEN_H) assert(enemy->visible_flag);
				++frames;
			}
			assert(rose >= 3 && fell >= 3 && frames < 120);
			assert(enemy->ypos > initial_y && enemy->screen_ypos > GAME_SCREEN_H);
			assert(!enemy->visible_flag && enemy->collide_flag);
			++death_trajectories_tested;
		}
	}
	memcpy(g_res.sql, sql, sizeof(sql));
	g_vars = saved;
	g_sys.input.quit = true;
}

static void test_doors() {
	struct object_t *player = &g_vars.objects[OBJECT_NUM_PLAYER1];
	const struct object_t saved = *player;
	static const int steps[] = { 0, 1, 4, 8 };
	for (unsigned z = 0; z < ARRAYSIZE(steps); ++z) {
		for (int i = 0; i < MAX_DOORS; ++i) {
			const struct door_t *door = &g_vars.doors[i];
			if (!door->src_x16 && !door->src_y16) continue;
			*player = saved;
			g_sys.input.view_step = steps[z];
			update_level_view(base_w, base_h, steps[z]);
			/* Include non-tile-aligned sizes produced by an active zoom. */
			if (z == 1) resize_level_view(GAME_SCREEN_W - 7, GAME_SCREEN_H - 5, true);
			player->xpos = door->src_x16 * 16;
			player->ypos = door->src_y16 * 16;
			player->xpos16 = door->src_x16;
			player->ypos16 = door->src_y16;
			const int lifes = player->lifes_count;
			_view_camera_cut = false;
			do_level_enter_door(player);
			assert(player->xpos == door->dst_x16 * 16 && player->ypos == door->dst_y16 * 16);
			do_level_enter_door(player);
			assert(player->xpos == door->dst_x16 * 16 && player->ypos == door->dst_y16 * 16);
			assert(player->lifes_count == lifes && !g_vars.game_over_flag);
			assert(player->screen_xpos >= 0 && player->screen_xpos <= TILEMAP_SCREEN_W);
			assert(player->screen_ypos >= 0 && player->screen_ypos <= TILEMAP_SCREEN_H);
			if (player->ypos > g_vars.screen_tilemap_h) {
				assert(player->scrolling_lock_flag);
				assert(GAME_SCREEN_W == 320 && GAME_SCREEN_H == 200);
				assert(g_vars.screen_tilemap_yorigin > g_vars.screen_tilemap_h);
				/* Zoom requests inside a room must not expose the map below it
				 * or pull the fixed camera back into the outdoor level. */
				update_level_view(base_w, base_h, 8);
				assert(GAME_SCREEN_W == 320 && GAME_SCREEN_H == 200);
				for (int frame = 0; frame < 3; ++frame) {
					screen_clear_sprites(0);
					do_level_update_scrolling2();
					do_level_update_objects();
					assert(!g_vars.game_over_flag && player->lifes_count == lifes);
					assert(player->ypos > g_vars.screen_tilemap_h && player->scrolling_lock_flag);
					assert(player->ypos - g_vars.screen_tilemap_yorigin >= 0);
					assert(player->ypos - g_vars.screen_tilemap_yorigin <= TILEMAP_SCREEN_H);
				}
			} else {
				assert(!player->scrolling_lock_flag);
				assert(g_vars.screen_tilemap_yorigin >= 0);
				assert(g_vars.screen_tilemap_yorigin + TILEMAP_SCREEN_H <= g_vars.screen_tilemap_h);
			}
			do_level_update_scrolling2();
			assert(player->ypos - g_vars.screen_tilemap_yorigin >= 0);
			assert(player->ypos - g_vars.screen_tilemap_yorigin <= TILEMAP_SCREEN_H);
			++doors_tested;
		}
	}
	*player = saved;
	g_sys.input.quit = true;
}

static void snapshot(const char *name) {
	const char *directory = getenv("BLUES_VIEW_TEST_OUTPUT");
	if (!directory) return;
	int w, h;
	assert(SDL_GetRendererOutputSize(_renderer, &w, &h) == 0);
	SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
	assert(surface);
	SDL_RenderSetLogicalSize(_renderer, 0, 0);
	assert(SDL_RenderReadPixels(_renderer, NULL, surface->format->format, surface->pixels, surface->pitch) == 0);
	SDL_RenderSetLogicalSize(_renderer, _screen_w, _screen_h);
	char path[1024];
	snprintf(path, sizeof(path), "%s/%s.bmp", directory, name);
	assert(SDL_SaveBMP(surface, path) == 0);
	SDL_FreeSurface(surface);
}

static uint32_t *read_output(int *w, int *h) {
	assert(SDL_GetRendererOutputSize(_renderer, w, h) == 0);
	uint32_t *pixels = malloc((size_t)*w * *h * sizeof(*pixels));
	assert(pixels);
	/* Read the complete output, including letterboxing. SDL's NULL rect
	 * otherwise starts at the current world viewport's offset. */
	SDL_RenderSetLogicalSize(_renderer, 0, 0);
	assert(SDL_RenderReadPixels(_renderer, NULL, SDL_PIXELFORMAT_ARGB8888, pixels,
		*w * sizeof(*pixels)) == 0);
	SDL_RenderSetLogicalSize(_renderer, _screen_w, _screen_h);
	return pixels;
}

static void assert_hud_pixels(const uint32_t *expected, int w, int h) {
	int actual_w, actual_h;
	uint32_t *actual = read_output(&actual_w, &actual_h);
	assert(w == actual_w && h == actual_h);
	const double scale = w / 320. < h / 200. ? w / 320. : h / 200.;
	const int left = (w - 320 * scale) / 2;
	const int top = (h - 200 * scale) / 2;
	for (int y = top; y < top + 200 * scale; ++y) {
		if (y >= top + 14 * scale && y < top + 174 * scale) continue;
		if (memcmp(expected + y * w + left, actual + y * w + left,
			(size_t)(320 * scale) * sizeof(*actual)) != 0) {
			fprintf(stderr, "HUD mismatch: amiga=%d players=%d viewport=%dx%d output=%dx%d row=%d\n",
				g_res.amiga_data, g_vars.two_players_flag + 1, GAME_SCREEN_W, GAME_SCREEN_H, w, h, y);

			abort();
		}
	}
	free(actual);
}

static void assert_world_fits_hud() {
	sdl2_draw_screen(0.f, 0.f, 1.f);
	int w, h;
	uint32_t *pixels = read_output(&w, &h);
	const double hud_scale = w / 320. < h / 200. ? w / 320. : h / 200.;
	const double sx = 320 * hud_scale / TILEMAP_SCREEN_W;
	const double sy = 160 * hud_scale / TILEMAP_SCREEN_H;
	const double scale = sx < sy ? sx : sy;
	const int left = (int)((w - TILEMAP_SCREEN_W * scale) / 2. + .5);
	const int top = (int)((h - 200 * hud_scale) / 2. + 14 * hud_scale +
		(160 * hud_scale - TILEMAP_SCREEN_H * scale) / 2. + .5);
	assert(top + .5 >= (h - 200 * hud_scale) / 2. + 14 * hud_scale);
	assert(top + TILEMAP_SCREEN_H * scale <= (h - 200 * hud_scale) / 2. + 174 * hud_scale + 1);
	for (int corner = 0; corner < 4; ++corner) {
		const int x = left + (int)((corner & 1 ? TILEMAP_SCREEN_W - 4 : 4) * scale);
		const int y = top + (int)((corner & 2 ? TILEMAP_SCREEN_H - 4 : 4) * scale);
		assert(pixels[y * w + x] == (0xff000000 | _screen_palette[corner + 1]));
	}
	free(pixels);
}

static void render_hud_test_frame() {
	g_sys.render_clear_sprites();
	do_level_redraw_tilemap(g_vars.screen_tilemap_xorigin, g_vars.screen_tilemap_yorigin);
	draw_level_panel();
	/* Colored markers at the four world corners detect cropping under HUD. */
	for (int corner = 0; corner < 4; ++corner) {
		const int x = corner & 1 ? TILEMAP_SCREEN_W - 8 : 0;
		const int y = TILEMAP_OFFSET_Y + (corner & 2 ? TILEMAP_SCREEN_H - 8 : 0);
		for (int row = 0; row < 8; ++row)
			memset(g_res.vga + (y + row) * GAME_SCREEN_W + x, corner + 1, 8);
	}
	g_sys.copy_bitmap(g_res.vga, GAME_SCREEN_W, GAME_SCREEN_H);
	/* Keep interpolation active to verify scrolling cannot move the HUD. */
	g_sys.set_scroll_interpolation(6, 3, 33, true);
	sdl2_draw_screen(3.f, 1.5f, .5f);
}

static void test_hud() {
	g_options.hud_mode = HUD_FIXED;
	configure_level_hud();
	const int sizes[][2] = {{640, 400}, {960, 600}, {800, 600}};
	for (int mode = 0; mode < 2; ++mode) {
		for (int players = 1; players <= 2; ++players) {
			g_vars.two_players_flag = players == 2;
			g_vars.player = PLAYER_ELWOOD;
			for (int i = 0; i < 2; ++i) {
				struct object_t *p = &g_vars.objects[OBJECT_NUM_PLAYER1 + i];
				p->type = i;
				p->data51 = 3;
				p->lifes_count = 3 + i;
				p->vinyls_count = 27 + i;
			}
			for (unsigned size = 0; size < ARRAYSIZE(sizes); ++size) {
				SDL_SetWindowSize(_window, sizes[size][0], sizes[size][1]);
				update_level_view(base_w, base_h, 0);
				/* The unmodified native presentation is the pixel reference. */
				g_sys.set_hud_layout(0, 0, 0, 0);
				render_hud_test_frame();
				int w, h;
				uint32_t *expected = read_output(&w, &h);
				g_sys.set_hud_layout(320, 200, 14, 26);
				render_hud_test_frame();
				assert_hud_pixels(expected, w, h);
				assert_world_fits_hud();
				for (int step = 1; step <= 8; ++step) {
					update_level_view(base_w, base_h, step);
					render_hud_test_frame();
					assert_hud_pixels(expected, w, h);
					assert_world_fits_hud();
					/* Intermediate easing dimensions need the same exact HUD. */
					resize_level_view(GAME_SCREEN_W - 3, GAME_SCREEN_H - 1, true);
					render_hud_test_frame();
					assert_hud_pixels(expected, w, h);
					assert_world_fits_hud();
				}
				free(expected);
			}
		}
		if (mode == 0) assert(switch_data_mode());
	}
	puts("HUD: original DOS/Amiga pixels, complete world framing, 1/2 players, 3 window sizes, 8 zoom steps and intermediate sizes passed");
	g_sys.input.quit = true;
}

/* Exercise the camera against each real level and several viewport sizes. */
static void test_camera() {
	for (int step = 0; step <= 8; step += 4) {
		update_level_view(base_w, base_h, step);
		struct object_t *player = &g_vars.objects[OBJECT_NUM_PLAYER1];
		g_vars.player2_scrolling_flag = 0;
		player->scrolling_lock_flag = 0;
		const int max_x = g_vars.screen_tilemap_w * 16 - TILEMAP_SCREEN_W;
		const int max_y = g_vars.screen_tilemap_h - TILEMAP_SCREEN_H;
		g_vars.screen_tilemap_xorigin = max_x / 2;
		g_vars.screen_tilemap_yorigin = max_y / 2;
		player->xpos = g_vars.screen_tilemap_xorigin + TILEMAP_SCREEN_W / 2;
		player->ypos = g_vars.screen_tilemap_yorigin + TILEMAP_SCREEN_H * 5 / 8;
		player->xvelocity = 32;
		_camera.valid = false;
		const struct object_t before = *player;
		const int start = g_vars.screen_tilemap_xorigin;
		for (int tick = 0; tick < 90; ++tick) update_smooth_camera();
		assert(g_vars.screen_tilemap_xorigin > start);
		assert(memcmp(&before, player, sizeof(before)) == 0);
		const double right_lead = _camera.lead;
		const int right = g_vars.screen_tilemap_xorigin;
		player->xvelocity = -32;
		update_smooth_camera();
		assert(_camera.lead < right_lead && _camera.lead > 0);
		assert(abs(g_vars.screen_tilemap_xorigin - right) <= 4);
		for (int tick = 0; tick < 90; ++tick) update_smooth_camera();
		assert(g_vars.screen_tilemap_xorigin < start);
		/* Stopping retains look-ahead; small vertical steps stay in the band. */
		player->xvelocity = 0;
		const double lead = _camera.lead;
		const int y = g_vars.screen_tilemap_yorigin;
		player->ypos -= 4;
		update_smooth_camera();
		assert(_camera.lead == lead && g_vars.screen_tilemap_yorigin == y);
		for (int corner = 0; corner < 4; ++corner) {
			player->xpos = corner & 1 ? g_vars.screen_tilemap_w * 16 : 0;
			player->ypos = corner & 2 ? g_vars.screen_tilemap_h : 0;
			for (int tick = 0; tick < 120; ++tick) {
				update_smooth_camera();
				assert(g_vars.screen_tilemap_xorigin >= 0 && g_vars.screen_tilemap_xorigin <= max_x);
				assert(g_vars.screen_tilemap_yorigin >= 0 && g_vars.screen_tilemap_yorigin <= max_y);
			}
		}
		/* A fast fall may accelerate the camera, but never lose the player. */
		player->xpos = g_vars.screen_tilemap_w * 8;
		player->ypos = TILEMAP_SCREEN_H;
		g_vars.screen_tilemap_yorigin = 0;
		_camera.valid = false;
		for (int tick = 0; tick < 40; ++tick) {
			player->ypos = MIN(player->ypos + 24, g_vars.screen_tilemap_h);
			update_smooth_camera();
			assert(player->ypos - g_vars.screen_tilemap_yorigin <= TILEMAP_SCREEN_H);
		}
		/* External camera cuts must discard the old following position. */
		g_vars.screen_tilemap_xorigin = max_x / 2;
		player->xpos = max_x / 2 + TILEMAP_SCREEN_W / 2;
		update_smooth_camera();
		assert(abs(g_vars.screen_tilemap_xorigin - max_x / 2) <= 16);
		player->scrolling_lock_flag = 1;
		const int locked = g_vars.screen_tilemap_xorigin;
		update_smooth_camera();
		assert(g_vars.screen_tilemap_xorigin == locked && !_camera.valid);
		/* The classic branch still performs its original 16px scroll step. */
		g_options.camera_mode = CAMERA_CLASSIC;
		g_vars.screen_scrolling_dirmask = 2;
		player->ypos = 100;
		do_level_update_scrolling2();
		assert(g_vars.screen_tilemap_xorigin == MIN(locked + 16, max_x));
		g_options.camera_mode = CAMERA_SMOOTH;
	}
	puts("camera: anticipation, reversal, stopping, dead zone, bounds, cuts, locks and classic steps passed");
	g_sys.input.quit = true;
}


static void test_hud_switch_events() {
	static enum hud_mode initial;
	if (!test_frames) {
		initial = g_options.hud_mode;
		base_w = GAME_SCREEN_W;
		base_h = GAME_SCREEN_H;
		g_sys.input.view_step = 4;
	} else if (test_frames == 1 || test_frames == 3) {
		bool paused = false;
		handle_keyevent(SDLK_F7, true, &g_sys.input, &paused);
		assert(!g_sys.input.switch_hud);
		handle_keyevent(SDLK_F7, false, &g_sys.input, &paused);
		assert(g_sys.input.switch_hud);
	} else if (test_frames == 2 || test_frames == 4) {
		assert(g_options.hud_mode == (test_frames == 2 ? !initial : initial));
		assert((_hud_width > 0) == (g_options.hud_mode == HUD_FIXED));
		int w, h;
		level_view_size(base_w, base_h, g_sys.input.view_step, &w, &h);
		assert(GAME_SCREEN_W == w && GAME_SCREEN_H == h);
		if (test_frames == 2) g_sys.input.view_step = 6;
		else {
			puts("HUD: F7 toggles both directions during zoom without stale layout/interpolation passed");
			g_sys.input.quit = true;
		}
	}
	++test_frames;
}

static void test_events() {
	sdl2_process_events();
	if (_sprites_cliprect.y != TILEMAP_OFFSET_Y) {
		g_sys.input.space = true; /* Skip title, character choice and intermission. */
		return;
	}
	g_sys.input.space = false;
	if (getenv("BLUES_TEST_HUD_SWITCH")) {
		test_hud_switch_events();
		return;
	}
	int window_w, window_h;
	SDL_GetWindowSize(_window, &window_w, &window_h);
	if (!test_frames) {
		base_w = previous_w = GAME_SCREEN_W;
		base_h = previous_h = GAME_SCREEN_H;
		saved_window_w = window_w;
		saved_window_h = window_h;
		if (getenv("BLUES_TEST_HUD")) {
			test_hud();
			return;
		}
		if (getenv("BLUES_TEST_CAMERA")) {
			test_camera();
			return;
		}
		if (getenv("BLUES_TEST_MODES")) {
			test_modes();
			return;
		}
		if (getenv("BLUES_TEST_ENEMY_DEATH")) {
			test_enemy_death();
			return;
		}
		if (getenv("BLUES_TEST_DOORS")) {
			test_doors();
			return;
		}
	} else {
		assert(window_w == saved_window_w && window_h == saved_window_h);
		assert(_screen_w == GAME_SCREEN_W && _screen_h == GAME_SCREEN_H);
		assert(g_res.vga_size >= GAME_SCREEN_W * GAME_SCREEN_H);
		assert(g_vars.screen_tilemap_xorigin >= 0);
		assert(g_vars.screen_tilemap_yorigin >= 0);
		assert(g_vars.screen_tilemap_xorigin + TILEMAP_SCREEN_W <= g_vars.screen_tilemap_w * 16);
		assert(g_vars.screen_tilemap_yorigin + TILEMAP_SCREEN_H <= g_vars.screen_tilemap_h);
		if (test_frames >= 2 && test_frames <= 81) {
			assert(GAME_SCREEN_W >= previous_w && GAME_SCREEN_H >= previous_h);
			if (GAME_SCREEN_W > previous_w || GAME_SCREEN_H > previous_h) ++out_frames;
		} else if (test_frames >= 82 && test_frames <= 161) {
			assert(GAME_SCREEN_W <= previous_w && GAME_SCREEN_H <= previous_h);
			if (GAME_SCREEN_W < previous_w || GAME_SCREEN_H < previous_h) ++closer_frames;
		}
		if (test_frames >= 11 && test_frames <= 161 && (test_frames - 1) % 10 == 0) {
			int stage = (test_frames - 1) / 10;
			int step = stage <= 8 ? stage : 16 - stage;
			int target_w, target_h;
			level_view_size(base_w, base_h, step, &target_w, &target_h);
			assert(GAME_SCREEN_W == target_w && GAME_SCREEN_H == target_h);
		}
		if (test_frames == 161) snapshot("initial");
		if (test_frames == 81) snapshot("wide");
		if (test_frames == 5) snapshot("transition");
		if (test_frames == 163) {
			assert(GAME_SCREEN_W > base_w && GAME_SCREEN_W < base_w * 3);
		}
		if (test_frames >= 165 && test_frames <= 175) {
			assert(GAME_SCREEN_W <= previous_w && GAME_SCREEN_H <= previous_h);
		}
		previous_w = GAME_SCREEN_W;
		previous_h = GAME_SCREEN_H;
	}
	bool paused = false;
	if (test_frames == 0) {
		/* First frame establishes the level; zoom keys then use animation. */
	} else if (test_frames <= 160) {
		if ((test_frames - 1) % 10 == 0) {
			handle_keyevent(test_frames <= 80 ? SDLK_MINUS : SDLK_PLUS, true, &g_sys.input, &paused);
		}
	} else if (test_frames == 161) {
		for (int i = 0; i < 8; ++i) handle_keyevent(SDLK_MINUS, true, &g_sys.input, &paused);
	} else if (test_frames == 163) {
		handle_keyevent(SDLK_0, true, &g_sys.input, &paused);
	} else if (test_frames < 175) {
		/* Let the reversal finish; new input must not snap to either end. */
	} else {
		assert(GAME_SCREEN_W == base_w && GAME_SCREEN_H == base_h);
		/* Exercise large viewports at all four map corners without changing
		 * positions or velocities in the game simulation. */
		struct object_t *player = &g_vars.objects[OBJECT_NUM_PLAYER1];
		int x = player->xpos, y = player->ypos, vx = player->xvelocity, vy = player->yvelocity;
		for (int corner = 0; corner < 4; ++corner) {
			g_vars.screen_tilemap_xorigin = (corner & 1) ? g_vars.screen_tilemap_w * 16 - TILEMAP_SCREEN_W : 0;
			g_vars.screen_tilemap_yorigin = (corner & 2) ? g_vars.screen_tilemap_h - TILEMAP_SCREEN_H : 0;
			for (int step = 8; step >= 0; --step) {
				update_level_view(base_w, base_h, step);
				assert(g_vars.screen_tilemap_xorigin >= 0 && g_vars.screen_tilemap_yorigin >= 0);
				assert(g_vars.screen_tilemap_xorigin + TILEMAP_SCREEN_W <= g_vars.screen_tilemap_w * 16);
				assert(g_vars.screen_tilemap_yorigin + TILEMAP_SCREEN_H <= g_vars.screen_tilemap_h);
				assert(player->xpos == x && player->ypos == y && player->xvelocity == vx && player->yvelocity == vy);
				do_level_redraw_tilemap(g_vars.screen_tilemap_xorigin, g_vars.screen_tilemap_yorigin);
				if (!g_res.amiga_data) draw_foreground_tiles();
				draw_level_panel();
				g_sys.copy_bitmap(g_res.vga, GAME_SCREEN_W, GAME_SCREEN_H);
				g_sys.update_screen();
				g_sys.render_clear_sprites();
			}
		}
		g_sys.input.quit = true;
	}
	++test_frames;
}

int main(int argc, char **argv) {
	if (getenv("BLUES_TEST_MODES")) g_sys.start_audio = capture_start_audio;
	g_sys.process_events = test_events;
	g_sys.sleep = no_sleep;
	g_sys.fade_in_palette = no_fade;
	g_sys.fade_out_palette = no_fade;
	g_sys.transition_screen = no_transition;
	int result = blues_program_main(argc, argv);
	if (getenv("BLUES_TEST_MODES")) {
		printf("modes: %d live DOS/Amiga swaps preserving state, map edits, zoom and rooms passed\n", modes_tested);
		return result;
	}
	if (getenv("BLUES_TEST_ENEMY_DEATH")) {
		printf("enemy death: %d crate hits with rising/falling arcs, both directions and full-height visibility passed\n", death_trajectories_tested);
		return result;
	}
	if (getenv("BLUES_TEST_DOORS")) {
		printf("doors: %d transitions at normal, wide and intermediate zoom sizes passed\n", doors_tested);
		return result;
	}
	if (getenv("BLUES_TEST_HUD")) {
		assert(_hud_width == 0); /* Disabled before menus/inter-level screens. */
		return result;
	}
	if (getenv("BLUES_TEST_HUD_SWITCH")) return result;
	if (getenv("BLUES_TEST_CAMERA")) return result;
	assert(test_frames == 176 && out_frames >= 40 && closer_frames >= 40);
	assert(GAME_SCREEN_W == base_w && GAME_SCREEN_H == base_h);
	printf("viewport: eased zoom-out/in, mid-transition reversal, reset, four map corners, fixed window, restored menus: passed\n");
	return result;
}
