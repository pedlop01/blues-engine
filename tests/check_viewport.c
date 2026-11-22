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


static void test_assists() {
	const struct vars_t saved=g_vars;
	const uint32_t cheats=g_options.cheats;
	uint8_t sql[640*25]; memcpy(sql,g_res.sql,sizeof(sql));
	struct object_t *p=&g_vars.objects[OBJECT_NUM_PLAYER1];
	assert(p->lifes_count>=1 && p->data51>0);
	g_options.cheats=0; p->data51=3; p->blinking_counter=0;
	do_level_player_hit(p); assert(p->data51==2 && p->blinking_counter==75);
	p->data51=3; p->blinking_counter=0; g_options.cheats=CHEATS_NO_HIT;
	do_level_player_hit(p); assert(p->data51==3 && p->blinking_counter==0);
	/* Tile hazards use the same damage protection. */
	const uint8_t hazard[]={0,10,3,104,22};
	p->trigger3=hazard; p->trigger3_num=1; p->special_anim=0;
	triggers_update_tiles2(p); assert(p->data51==3 && p->special_anim==0);
	g_options.cheats=0; p->trigger3_num=1;
	triggers_update_tiles2(p); assert(p->data51==2 && p->special_anim==22);
	g_vars=saved;
	struct sys_pause_options options={.invulnerable=true,.lives={7,9}};
	const int x=p->xpos,y=p->ypos,health=p->data51,vinyls=p->vinyls_count;
	apply_game_assists(&options);
	assert((g_options.cheats & CHEATS_INVULNERABLE)==CHEATS_INVULNERABLE && p->lifes_count==7);
	assert(p->xpos==x && p->ypos==y && p->data51==health && p->vinyls_count==vinyls);
	options.invulnerable=false; options.infinite_lives=true;
	apply_game_assists(&options); assert(g_options.cheats==CHEATS_UNLIMITED_LIFES);
	p->data51=3; p->blinking_counter=0;
	do_level_player_hit(p); assert(p->data51==2); /* Infinite lives != invulnerability. */
	for(int retry=0;retry<20;++retry) {
		p->restart_level_flag=1; init_level();
		assert(p->lifes_count==7 && p->data51==3);
	}
	g_options.cheats=0; p->restart_level_flag=1; init_level(); assert(p->lifes_count==6);
	/* Last-life death must retry under infinity and terminate normally without it. */
	for(int infinite=0;infinite<2;++infinite) {
		g_vars=saved; g_vars.play_level_flag=1;
		g_options.cheats=infinite ? CHEATS_UNLIMITED_LIFES : 0;
		p->lifes_count=1; p->data51=1; p->blinking_counter=0;
		do_level_player_hit(p); assert(g_vars.game_over_flag);
		g_vars.update_objects_counter=70;
		do_level_update_objects();
		assert(g_vars.quit_level_flag && g_vars.play_level_flag==infinite);
		if(infinite) { assert(p->restart_level_flag); init_level(); assert(p->lifes_count==1 && p->data51==3); }
	}
	/* Infinite lives do not kill or decrement either player on health depletion. */
	g_vars=saved; g_vars.two_players_flag=1;
	g_vars.player1_dead_flag=g_vars.player2_dead_flag=0;
	for(int player=0;player<2;++player) {
		struct object_t *q=&g_vars.objects[OBJECT_NUM_PLAYER1+player];
		q->type=player; q->lifes_count=player ? 9 : 1; q->data51=1; q->blinking_counter=0;
		g_options.cheats=CHEATS_UNLIMITED_LIFES;
		do_level_player_hit(q);
		assert(q->lifes_count==(player ? 9 : 1) && q->data51==3);
	}
	assert(!g_vars.player1_dead_flag && !g_vars.player2_dead_flag && !g_vars.game_over_flag);
	g_options.cheats=0; p->lifes_count=2; p->data51=1; p->blinking_counter=0;
	do_level_player_hit(p); assert(p->lifes_count==1 && p->data51==3);
	p->data51=1; p->blinking_counter=0; do_level_player_hit(p);
	assert(g_vars.player2_dead_flag && !assist_player_active(0));
	options.invulnerable=false; options.infinite_lives=false; options.lives[0]=99; options.lives[1]=4;
	apply_game_assists(&options);
	assert(p->lifes_count==1 && g_vars.objects[OBJECT_NUM_PLAYER2].lifes_count==4);
	/* Life pickups saturate at the original two-digit HUD range. */
	g_vars=saved; g_options.cheats=0;
	uint8_t *tile=lookup_sql(p->xpos16,p->ypos16); const int num=*tile;
	const struct trigger_t trigger=g_res.triggers[num];
	const uint8_t reward[]={0,12,0,0,0,0,0,0,0,0,1};
	g_res.triggers[num].unk16=num; g_res.triggers[num].op_table3=reward;
	p->lifes_count=99; triggers_update_tiles1(p); assert(p->lifes_count==99);
	*tile=num; p->lifes_count=98; triggers_update_tiles1(p); assert(p->lifes_count==99);
	*tile=num; g_options.cheats=CHEATS_UNLIMITED_LIFES; p->lifes_count=7;
	triggers_update_tiles1(p); assert(p->lifes_count==7);
	g_res.triggers[num]=trigger;
	g_vars=saved; g_options.cheats=cheats; memcpy(g_res.sql,sql,sizeof(sql));
	update_assist_status();
	puts("assists: hits, hazards, independent modes, 20 retries, final life, two players and capped pickups passed");
	g_sys.input.quit=true;
}

static void traversal_tick() {
	g_sys.begin_sprite_frame(); screen_clear_sprites(0);
	do_level_update_scrolling2(); do_level_update_objects();
	++g_vars.level_loop_counter;
}
static void traversal_player_only() {
	for(int i=0;i<MAX_OBJECTS;++i) if(i!=OBJECT_NUM_PLAYER1) {
		g_vars.objects[i].type=100; g_vars.objects[i].visible_flag=0;
	}
	g_vars.inp_key_space=g_vars.inp_key_jump=g_vars.inp_key_up=g_vars.inp_key_down=0;
	g_vars.inp_key_left=g_vars.inp_key_right=0;
}
static void test_traversal() {
	const struct vars_t initial=g_vars;
	uint8_t sql[640*25]; memcpy(sql,g_res.sql,sizeof(sql));
	const int base_w=GAME_SCREEN_W,base_h=GAME_SCREEN_H;
	int ladders=0,pipe_cases=0,entries=0;

	for(int x=1;x<127;++x)for(int start=1;start<g_vars.screen_tilemap_h/16-1;++start) {
		if(triggers_get_tile_type(x,start)!=7 || triggers_get_tile_type(x,start-1)==7)continue;
		int end=start;while(end+1<g_vars.screen_tilemap_h/16 && triggers_get_tile_type(x,end+1)==7)++end;
        /* Up/Down must never move the player sideways, including the very
         * first frame. Check short ladders as well as long shafts, using the
         * real keyboard-to-game-input path rather than setting direction_ud. */
        for (int offset=0;offset<16;++offset) for (int direction=0;direction<2;++direction) {
            g_vars=initial;memcpy(g_res.sql,sql,sizeof(sql));traversal_player_only();
            struct object_t *p=&g_vars.objects[OBJECT_NUM_PLAYER1];
            const int original_x=x*16+offset;
            p->xpos=original_x;p->ypos=(direction ? start : end)*16+8;
            p->xpos16=p->xpos/16;p->ypos16=p->ypos/16;p->floor_ypos16=p->ypos16;
            p->xvelocity=p->yvelocity=p->unk1C=p->unk2F=0;
            p->special_anim=0;p->sprite_type=2;
            bool paused=false;
            clear_pause_input();
            const int key=direction ? SDLK_DOWN : SDLK_UP;
            handle_keyevent(key,true,&g_sys.input,&paused);
            for (int tick=0;tick<8;++tick) {
                void (*process_events)()=g_sys.process_events;
                g_sys.process_events=sdl2_process_events;
                update_input();
                g_sys.process_events=process_events;
                assert(!g_vars.inp_key_left && !g_vars.inp_key_right);
                const bool on_ladder=triggers_get_tile_type(p->xpos16,p->ypos16)==7;
                traversal_tick();
                if (on_ladder) assert(p->xpos==original_x);
            }
            handle_keyevent(key,false,&g_sys.input,&paused);
            clear_pause_input();
            ++entries;
        }
        if(end-start+1<6)continue;
        /* Every entry offset, including the right half that used to snap
         * into the next column, and residual conveyor acceleration. */
        for (int offset=0;offset<16;++offset) for (int force=-3;force<=3;force+=3) {
            g_vars=initial;memcpy(g_res.sql,sql,sizeof(sql));traversal_player_only();
            struct object_t *p=&g_vars.objects[OBJECT_NUM_PLAYER1];
            p->xpos=x*16+offset;p->ypos=end*16+8;p->xpos16=p->xpos/16;p->ypos16=p->ypos/16;
            p->floor_ypos16=p->ypos16;p->xvelocity=p->yvelocity=p->unk1C=0;
            p->unk2F=force;p->special_anim=0;p->sprite_type=2;
            g_vars.inp_key_up=1;
            int highest=p->ypos;
            for (int t=0;t<(end-start+1)*8;++t) {
                const bool on_ladder=triggers_get_tile_type(p->xpos16,p->ypos16)==7;
                traversal_tick();
                if (on_ladder) assert(p->xpos==x*16+offset && p->xvelocity==0 && p->unk2F==0);
                highest=MIN(highest,p->ypos);
            }
            assert(highest <= end*16-16);
            if (initial.level==1) assert(highest <= (start+1)*16);
            ++entries;
        }
		for(int mode=0;mode<2;++mode)for(int velocity=-48;velocity<=48;velocity+=48) {
			g_vars=initial;memcpy(g_res.sql,sql,sizeof(sql));traversal_player_only();
			g_options.jump_button=mode!=0;
			struct object_t *p=&g_vars.objects[OBJECT_NUM_PLAYER1];
			p->xpos=x*16+8;p->ypos=((start+end)/2)*16+8;
			p->xpos16=p->xpos/16;p->ypos16=p->ypos/16;
			p->xvelocity=velocity;p->yvelocity=0;p->unk1C=p->unk2F=0;p->special_anim=0;p->sprite_type=2;
			const int original_y=p->ypos;
			g_vars.inp_key_down=1;
			for(int t=0;t<12;++t){traversal_tick();assert(p->xpos==x*16+8 && p->xvelocity==0 && p->direction_ud==2);}
			assert(p->ypos==original_y+24);
			g_vars.inp_key_down=0;g_vars.inp_key_up=1;
			for(int t=0;t<12;++t){traversal_tick();assert(p->xpos==x*16+8 && p->direction_ud==1);}
			assert(p->ypos==original_y);
			g_vars.inp_key_up=0;
			for(int t=0;t<12;++t)traversal_tick();
			assert(p->ypos==original_y && p->xpos==x*16+8 && p->yvelocity==0);
		}
		++ladders;
	}
	if(g_vars.level==1) {
		assert(ladders>=3);
		for(int velocity=-48;velocity<=48;velocity+=48) {
			g_vars=initial;memcpy(g_res.sql,sql,sizeof(sql));traversal_player_only();
			struct object_t *ladder=&g_vars.objects[OBJECT_NUM_PLAYER1];
			ladder->xpos=31*16+8;ladder->ypos=30*16-1;ladder->xpos16=31;ladder->ypos16=29;
			ladder->floor_ypos16=29;ladder->xvelocity=velocity;ladder->yvelocity=ladder->unk1C=ladder->unk2F=0;
			ladder->sprite_type=1;ladder->special_anim=0;g_vars.inp_key_down=1;
			for(int t=0;t<48;++t)traversal_tick();
			assert(ladder->ypos>34*16 && ladder->xpos==31*16+8); /* Enter from the cap and hold Down across its platform. */
		}
		for(int camera=0;camera<2;++camera)for(int step=0;step<=8;step+=4)for(int protected=0;protected<2;++protected) {
			resize_level_view(base_w,base_h,false);
			g_vars=initial;memcpy(g_res.sql,sql,sizeof(sql));traversal_player_only();
			g_options.camera_mode=camera;g_options.cheats=protected ? CHEATS_INVULNERABLE : 0;
			struct object_t *p=&g_vars.objects[OBJECT_NUM_PLAYER1];
			p->xpos=38*16;p->ypos=8*16;p->xpos16=38;p->ypos16=8;
			p->xvelocity=p->yvelocity=p->special_anim=0;
			update_level_view(base_w,base_h,step);
			assert(g_res.triggers[*lookup_sql(38,8)].op_func==3);
			p->data5F=0;level_call_trigger_func(p,0);assert(p->xpos==38*16);
			p->ypos=8*16;p->ypos16=8;
			p->data5F=2;g_vars.found_music_instrument_flag=true;
			level_call_trigger_func(p,0);assert(p->xpos==101*16 && p->ypos==70*16 && g_vars.switch_player_scrolling_flag);
			for(int t=0;t<140;++t)traversal_tick();
			assert(!g_vars.switch_player_scrolling_flag && p->special_anim==0 && !g_vars.game_over_flag);
			const int before=p->xpos;
			g_vars.inp_key_left=1;
			for(int t=0;t<20;++t)traversal_tick();
			assert(p->xpos<before && p->special_anim==0);
			++pipe_cases;
		}
	}
	printf("traversal: %d ladders, %d entry offsets/conveyor impulses; up/down/idle, both jump mappings; %d pipe recoveries passed\n",ladders,entries,pipe_cases);
	g_sys.input.quit=true;
}

static int menu_timer_step, menus_tested;
static void push_menu_key(int sym, bool down) {
	SDL_Event ev = {0};
	ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	ev.key.keysym.sym = sym;
	ev.key.keysym.scancode = SDL_GetScancodeFromKey(sym);
	assert(SDL_PushEvent(&ev) == 1);
}
static uint32_t menu_script(uint32_t interval, void *param) {
	assert(SDL_GetAudioStatus() != SDL_AUDIO_PLAYING);
	const int step = menu_timer_step++;
	if (step == 0) {
		push_menu_key(SDLK_DOWN, true); push_menu_key(SDLK_DOWN, false);
		push_menu_key(SDLK_DOWN, true); push_menu_key(SDLK_DOWN, false);
		push_menu_key(SDLK_RETURN, true); push_menu_key(SDLK_RETURN, false);
	} else if (step == 1) {
		push_menu_key(SDLK_ESCAPE, false);
	} else {
		push_menu_key(SDLK_UP, true); push_menu_key(SDLK_UP, false);
		push_menu_key(SDLK_UP, true); push_menu_key(SDLK_UP, false);
		push_menu_key(SDLK_RETURN, true);
	}
	return menu_timer_step == 3 ? 0 : interval;
}
static struct sys_pause_options options_before_test;
static void menu_tap(int sym) { push_menu_key(sym,true); push_menu_key(sym,false); }
static void menu_button(int button, bool down) {
	SDL_Event ev={0}; ev.type=down ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP;
	ev.cbutton.button=button; assert(SDL_PushEvent(&ev)==1);
}
static uint32_t options_script(uint32_t interval, void *param) {
	assert(SDL_GetAudioStatus()!=SDL_AUDIO_PLAYING);
	const int step=menu_timer_step++;
	if (step==0) {
		menu_tap(SDLK_DOWN); menu_tap(SDLK_RETURN); /* Options. */
		menu_tap(SDLK_RIGHT); /* Version. */
		menu_tap(SDLK_DOWN); menu_tap(SDLK_RIGHT); /* Camera. */
		menu_tap(SDLK_DOWN); menu_tap(SDLK_RIGHT); /* HUD. */
		menu_tap(SDLK_DOWN); menu_tap(SDLK_RIGHT); menu_tap(SDLK_LEFT); /* Fullscreen round trip. */
		menu_tap(SDLK_DOWN); menu_tap(SDLK_RIGHT); /* Rate. */
		menu_tap(SDLK_DOWN); menu_tap(SDLK_RIGHT); /* View. */
	} else if (step==1) {
		push_menu_key(SDLK_ESCAPE,false); menu_tap(SDLK_DOWN);
		menu_tap(SDLK_RETURN); menu_tap(SDLK_RETURN); /* Main Controls -> Keyboard. */
		for(int i=0;i<4;++i) menu_tap(SDLK_DOWN);
		menu_tap(SDLK_RETURN); menu_tap(SDLK_x); /* Capture action key. */
		push_menu_key(SDLK_ESCAPE,false); menu_tap(SDLK_DOWN); menu_tap(SDLK_RETURN); /* Pad. */
		menu_tap(SDLK_DOWN);
		menu_button(SDL_CONTROLLER_BUTTON_A,true); menu_button(SDL_CONTROLLER_BUTTON_A,false);
		menu_button(SDL_CONTROLLER_BUTTON_X,true); menu_button(SDL_CONTROLLER_BUTTON_X,false);
		push_menu_key(SDLK_ESCAPE,false); push_menu_key(SDLK_ESCAPE,false);
	} else {
		menu_tap(SDLK_UP); menu_tap(SDLK_UP); push_menu_key(SDLK_RETURN,true);
	}
	return menu_timer_step==3 ? 0 : interval;
}

static uint32_t reset_preferences_script(uint32_t interval, void *param) {
	const int step=menu_timer_step++;
	if (step==0) {
		menu_tap(SDLK_DOWN); menu_tap(SDLK_RETURN);
		for (int i=0;i<8;++i) menu_tap(SDLK_DOWN);
		menu_tap(SDLK_RETURN);
	} else if (step==1) {
		menu_tap(SDLK_DOWN); menu_tap(SDLK_RETURN);
		push_menu_key(SDLK_ESCAPE,false);
	} else {menu_tap(SDLK_UP);push_menu_key(SDLK_RETURN,true);}
	return menu_timer_step==3 ? 0 : interval;
}

static bool volume_applied_paused;
static void track_volume_apply(int music,int effects) {
	assert(SDL_GetAudioStatus()!=SDL_AUDIO_PLAYING);
	volume_applied_paused=true;
	g_mix.set_volumes(music,effects);
}
static uint32_t volume_script(uint32_t interval,void *param) {
	assert(SDL_GetAudioStatus()!=SDL_AUDIO_PLAYING);
	const int step=menu_timer_step++;
	if (step==0) {
		menu_tap(SDLK_DOWN); menu_tap(SDLK_RETURN);
		for(int i=0;i<6;++i)menu_tap(SDLK_DOWN);
		menu_tap(SDLK_RETURN);
		for(int i=0;i<6;++i)menu_tap(SDLK_LEFT);
		menu_tap(SDLK_DOWN); menu_tap(SDLK_RETURN);
	} else if(step==1) {
		push_menu_key(SDLK_ESCAPE,false); push_menu_key(SDLK_ESCAPE,false);
	} else { menu_tap(SDLK_UP); push_menu_key(SDLK_RETURN,true); }
	return menu_timer_step==3 ? 0 : interval;
}
static bool render_real_audio(int blocks) {
	int16_t samples[4096]; bool audible=false;
	assert(captured_audio);
	for(int block=0;block<blocks;++block) {
		captured_audio(captured_audio_param,(uint8_t*)samples,sizeof(samples));
		for(int i=0;i<4096;++i) audible |= samples[i]!=0;
	}
	return audible;
}
static void test_native_volumes() {
	SDL_PauseAudio(1);
	g_options.music_volume=g_options.effects_volume=0;
	g_mix.set_volumes(0,0);
	for(int mode=0;mode<2;++mode) {
		render_real_audio(1); assert(!render_real_audio(1));
		g_mix.set_volumes(100,0);
		assert(render_real_audio(12)); /* FX mute must leave native music audible. */
		g_mix.set_volumes(0,100);
		render_real_audio(1);
		play_sound(SOUND_11);
		assert(render_real_audio(3)); /* Music mute must leave native FX audible. */
		g_mix.set_volumes(0,0);
		render_real_audio(1); assert(!render_real_audio(2));
		if(mode==0) { assert(switch_data_mode()); SDL_PauseAudio(1); }
	}
	g_options.music_volume=40; g_options.effects_volume=0;
	g_mix.set_volumes(40,0);
	puts("native volumes: music/effect isolation and exact mute in DOS/Amiga, retained across mode change passed");
}

static int assist_health_before[2];
static uint32_t assist_script(uint32_t interval,void *param) {
	const int step=menu_timer_step++;
	assert(SDL_GetAudioStatus()!=SDL_AUDIO_PLAYING);
	if(step==0) {
		menu_tap(SDLK_DOWN); menu_tap(SDLK_RETURN);
		for(int i=0;i<7;++i)menu_tap(SDLK_DOWN);
		menu_tap(SDLK_RETURN);
		menu_tap(SDLK_RETURN); menu_tap(SDLK_DOWN); menu_tap(SDLK_RETURN);
		menu_tap(SDLK_DOWN); menu_tap(SDLK_RIGHT); menu_tap(SDLK_RIGHT);
		menu_tap(SDLK_DOWN); menu_tap(SDLK_RIGHT);
	} else if(step==1) {
		push_menu_key(SDLK_ESCAPE,false); push_menu_key(SDLK_ESCAPE,false);
	} else {menu_tap(SDLK_UP);push_menu_key(SDLK_RETURN,true);}
	return menu_timer_step==3 ? 0 : interval;
}

static void test_run_pause_menu() {
	const struct vars_t before = g_vars;
	uint8_t sql[640 * 25];
	memcpy(sql, g_res.sql, sizeof(sql));
	const int sprite_count = _sprites_count;
	assert(sprite_count > 0);
	const int w = GAME_SCREEN_W, h = GAME_SCREEN_H;
	const enum hud_mode hud = g_options.hud_mode;
	g_sys.get_pause_options(&options_before_test);
	for(int i=0;i<2;++i)assist_health_before[i]=g_vars.objects[OBJECT_NUM_PLAYER1+i].data51;
	if(getenv("BLUES_TEST_VOLUMES")) {
		struct sys_pause_options tracked=options_before_test;
		tracked.apply_volumes=track_volume_apply; g_sys.set_pause_options(&tracked);
	}
	/* Capture actual game frames for visual review without changing state. */
	sdl2_set_scroll_interpolation(0, 0, 0, false);
	_pause_menu_open = true;
	_pause_page = PAUSE_MAIN; _pause_selection = 0;
	sdl2_draw_screen(0, 0, 1); snapshot("menu-pause");
	_pause_page = PAUSE_CONTROLS;
	sdl2_draw_screen(0, 0, 1); snapshot("menu-controls");
	_pause_page = PAUSE_QUIT;
	sdl2_draw_screen(0, 0, 1); snapshot("menu-quit");
	_pause_page = PAUSE_OPTIONS; _pause_selection=0;
	sdl2_draw_screen(0,0,1); snapshot("menu-options");
	_pause_page = PAUSE_KEYS;
	sdl2_draw_screen(0,0,1); snapshot("menu-keys");
	_pause_page = PAUSE_PAD;
	sdl2_draw_screen(0,0,1); snapshot("menu-pad");
	_pause_page = PAUSE_AUDIO;
	sdl2_draw_screen(0,0,1); snapshot("menu-audio");
	_pause_page = PAUSE_ASSISTS;
	sdl2_draw_screen(0,0,1); snapshot("menu-assists");
	_pause_page = PAUSE_DEFAULTS; _pause_selection=0;
	sdl2_draw_screen(0,0,1); snapshot("menu-defaults");
	_pause_menu_open = false;
	menu_timer_step = 0;
	assert(SDL_AddTimer(60, getenv("BLUES_TEST_RESET_PREFS") ? reset_preferences_script : (getenv("BLUES_TEST_ASSIST_MENU") ? assist_script : (getenv("BLUES_TEST_VOLUMES") ? volume_script : (getenv("BLUES_TEST_OPTIONS") ? options_script : menu_script))), NULL));
	const uint32_t timestamp = SDL_GetTicks();
	sdl2_run_pause_menu();
	assert(SDL_GetTicks() - timestamp >= 150 && menu_timer_step == 3);
	assert(memcmp(&before, &g_vars, sizeof(before)) == 0);
	assert(memcmp(sql, g_res.sql, sizeof(sql)) == 0);
	assert(_sprites_count == sprite_count && GAME_SCREEN_W == w && GAME_SCREEN_H == h);
	assert(g_options.hud_mode == hud && !g_sys.input.menu && !g_sys.input.space && !g_sys.input.jump && !g_sys.input.direction);
	assert(!g_sys.input.switch_mode && !g_sys.input.switch_hud && !g_sys.input.quit);
	assert(_blocked_keys[SDL_SCANCODE_RETURN]);
	if(getenv("BLUES_TEST_ASSIST_MENU")) {
		struct sys_pause_options after; g_sys.get_pause_options(&after);
		assert(after.invulnerable != options_before_test.invulnerable && after.infinite_lives != options_before_test.infinite_lives);
		assert(after.lives[0]==options_before_test.lives[0]+2);
		assert(after.lives[1]==options_before_test.lives[1]+(after.player_alive[1] ? 1 : 0));
	}
	if(getenv("BLUES_TEST_VOLUMES")) {
		struct sys_pause_options after; g_sys.get_pause_options(&after);
		assert(volume_applied_paused && after.music_volume==40 && after.effects_volume==0);
		assert(g_options.music_volume==options_before_test.music_volume && g_options.effects_volume==options_before_test.effects_volume);
	}
	if (getenv("BLUES_TEST_OPTIONS")) {
		struct sys_pause_options after; g_sys.get_pause_options(&after);
		assert(after.amiga != options_before_test.amiga && after.camera != options_before_test.camera);
		assert(after.hud != options_before_test.hud && after.view_step == options_before_test.view_step+1);
		assert(after.render_rate != options_before_test.render_rate);
		assert(_game_keys[BIND_ACTION] == SDLK_x && _game_buttons[1] == SDL_CONTROLLER_BUTTON_X);
		bool paused=false;
		handle_keyevent(SDLK_SPACE,true,&g_sys.input,&paused); assert(!g_sys.input.space);
		handle_keyevent(SDLK_x,true,&g_sys.input,&paused); assert(g_sys.input.space);
		handle_keyevent(SDLK_x,false,&g_sys.input,&paused);
		handle_controllerbutton(SDL_CONTROLLER_BUTTON_X,true,&g_sys.input,&paused); assert(g_sys.input.jump);
		handle_controllerbutton(SDL_CONTROLLER_BUTTON_X,false,&g_sys.input,&paused);
	}
	push_menu_key(SDLK_RETURN, false);
	++menus_tested;
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

/* Exercise the real outer game loop, not just the menu or init helper. */
static int levels_started, level_frames, expected_level=-1;
static bool expected_amiga, choosing_character;
static int character_choice, character_events;
static void test_level_menu() {
 assert(_pause_options.current_level==g_vars.level);
 for (int i=0;i<MAX_LEVELS;++i) assert(_pause_options.available_levels[g_res.amiga_data][i]);
 const char *path=g_options.amiga_datapath;
 g_options.amiga_datapath="/nonexistent/blues-data";
 assert(!level_available(0,true)); g_options.amiga_datapath=path;
 struct object_t *player=&g_vars.objects[OBJECT_NUM_PLAYER1];
 player->lifes_count=2; player->data51=1; player->vinyls_count=35;
 player->unk60=1; player->special_anim=18; player->scrolling_lock_flag=1;
 g_vars.found_music_instrument_flag=true;
 g_vars.switch_player_scrolling_flag=true;
 g_res.sql[100]^=1;
 g_res.triggers[5].unk16=99;
 _pause_options.lives[0]=17;
 _pause_options.invulnerable=true;
 _pause_options.infinite_lives=true;
 _pause_options.camera=CAMERA_CLASSIC;
 _pause_options.hud=HUD_FIXED;
 _pause_options.render_rate=120;
 _pause_options.view_step=4;
 _pause_options.music_volume=40; _pause_options.effects_volume=30;
 _pause_options.apply_volumes(40,30);
 if (getenv("BLUES_TEST_LEVEL_MODE")) _pause_options.amiga=!g_res.amiga_data;
 expected_amiga=_pause_options.amiga;
 expected_level=MIN(5,levels_started);
 if (getenv("BLUES_TEST_CHARACTER")) {
  expected_level=g_vars.level;
  character_choice=levels_started%3; character_events=0; choosing_character=true;
  _pause_page=PAUSE_MAIN; _pause_selection=5;
  sdl2_draw_pause_menu(); snapshot("pause-main");
  assert(!pause_activate() && _pause_page==PAUSE_CHARACTER_CONFIRM && _pause_selection==0);
  sdl2_draw_pause_menu(); snapshot("character-confirm");
  _pause_selection=1; assert(pause_activate() && _pause_options.select_character);
  ++levels_started; level_frames=0;
  return;
 }
 _pause_page=PAUSE_MAIN; _pause_selection=3;
 assert(!pause_activate() && _pause_page==PAUSE_LEVELS);
 sdl2_draw_pause_menu(); snapshot("level-selector");
 _pause_selection=expected_level;
 assert(!pause_activate() && _pause_page==PAUSE_LEVEL_CONFIRM && _pause_selection==0);
 sdl2_draw_pause_menu(); snapshot("level-confirm");
 _pause_selection=1; assert(pause_activate());
 ++levels_started; level_frames=0;
}
static void test_level_events() {
 if (level_frames==0 && expected_level<0 && getenv("BLUES_TEST_LEVEL_TWO")) {g_vars.two_players_flag=true;init_level();}
 if (level_frames==0 && expected_level>=0) {
  const struct object_t *player=&g_vars.objects[OBJECT_NUM_PLAYER1];
  assert(g_vars.level==expected_level && g_res.amiga_data==expected_amiga);
  assert(player->lifes_count==17 && player->data51==3 && player->vinyls_count==0);
  if (getenv("BLUES_TEST_CHARACTER")) {
   assert(g_vars.two_players_flag==(character_choice==2));
   assert(player->type==(character_choice==1 ? PLAYER_ELWOOD : PLAYER_JAKE));
   assert(g_vars.objects[OBJECT_NUM_PLAYER2].type==(character_choice==2 ? PLAYER_ELWOOD : 100));
  }
  assert(!player->unk60 && !player->special_anim && !player->scrolling_lock_flag);
  assert(!g_vars.found_music_instrument_flag && !g_vars.switch_player_scrolling_flag);
  assert(!g_vars.game_over_flag && !g_vars.player1_dead_flag && !g_vars.player2_dead_flag);
  assert(!g_vars.level_loop_counter && g_res.triggers[5].unk16==5);
  if (getenv("BLUES_TEST_LEVEL_TWO")) {
   assert(g_vars.two_players_flag && g_vars.objects[OBJECT_NUM_PLAYER2].type==PLAYER_ELWOOD);
   assert(g_vars.objects[OBJECT_NUM_PLAYER2].data51==3);
  }
  assert(memcmp(g_res.sql,_level_base_sql,sizeof(_level_base_sql))==0);
  assert(player->xpos==g_vars.level_xpos[OBJECT_NUM_PLAYER1]);
  assert(player->ypos==g_vars.level_ypos[OBJECT_NUM_PLAYER1]);
  assert(g_options.camera_mode==CAMERA_CLASSIC && g_options.hud_mode==HUD_FIXED);
  assert(g_options.scroll_rate==120 && g_sys.input.view_step==4);
  assert(g_options.music_volume==40 && g_options.effects_volume==30);
  assert((g_options.cheats & (CHEATS_INVULNERABLE|CHEATS_UNLIMITED_LIFES))==(CHEATS_INVULNERABLE|CHEATS_UNLIMITED_LIFES));
  if (levels_started==7) { g_sys.input.quit=true; puts(getenv("BLUES_TEST_CHARACTER") ? "characters: Jake/Elwood/two players, fresh stage and retained settings passed" : "levels: six destinations, same-level restart, fresh world and retained preferences passed"); return; }
 }
 if (level_frames==2) g_sys.input.menu=true;
 ++level_frames;
}

static void test_preferences_loaded() {
	const char *expect=getenv("BLUES_TEST_PREFS_EXPECT");
	const bool reset=!strcmp(expect,"reset"), override=!strcmp(expect,"override");
	assert(g_options.camera_mode==((reset || override) ? CAMERA_SMOOTH : CAMERA_CLASSIC));
	assert(g_options.hud_mode==((reset || override) ? HUD_CLASSIC : HUD_FIXED));
	assert(g_options.scroll_rate==(override ? 30 : (reset ? 60 : 120)));
	assert(_game_keys[BIND_ACTION]==(reset ? SDLK_SPACE : SDLK_x));
	assert(_game_buttons[1]==(reset ? SDL_CONTROLLER_BUTTON_B : SDL_CONTROLLER_BUTTON_X));
	assert(!g_options.fullscreen && !(SDL_GetWindowFlags(_window)&SDL_WINDOW_FULLSCREEN_DESKTOP));
	if (!strcmp(expect,"assists")) {
		assert(g_options.music_volume==40 && g_options.effects_volume==30 && g_sys.input.view_step==4);
		assert(g_options.cheats==7 && g_options.initial_lives[0]==17);
		assert(g_vars.objects[OBJECT_NUM_PLAYER1].lifes_count==17);
	} else {
		assert(g_options.music_volume==100 && g_options.effects_volume==100);
		assert(g_sys.input.view_step==(reset ? 0 : 5));
		assert(g_options.cheats==0 && g_vars.objects[OBJECT_NUM_PLAYER1].lifes_count==3);
	}
	assert(g_res.amiga_data==!reset);
	if (getenv("BLUES_TEST_PREFS_LOSE_LIFE")) g_vars.objects[OBJECT_NUM_PLAYER1].lifes_count=1;
	g_sys.input.quit=true;
	puts("preferences: separate launch restored effective options, controls, mode and initial lives passed");
}

static void encode_current_state(uint8_t *data,size_t *size) {
	struct bb_state state;assert(capture_game_state(&state));state.timestamp=0;
	assert(bb_state_encode(&state,data,BB_STATE_MAX_SIZE,size));
}
static void state_trace() {
	for(int tick=0;tick<12;++tick) {
		g_vars.inp_key_left=tick<5;g_vars.inp_key_right=tick>=8;
		g_vars.inp_key_up=g_vars.inp_key_down=g_vars.inp_key_jump=g_vars.inp_key_space=0;
		advance_view_transition(_state_view,33);
		traversal_tick();
	}
}
static uint32_t state_frame_crc() {
	int w,h;assert(SDL_GetRendererOutputSize(_renderer,&w,&h)==0);
	uint32_t *pixels=malloc((size_t)w*h*4);assert(pixels);
	SDL_RenderSetLogicalSize(_renderer,0,0);
	assert(SDL_RenderReadPixels(_renderer,NULL,SDL_PIXELFORMAT_ARGB8888,pixels,w*4)==0);
	SDL_RenderSetLogicalSize(_renderer,_screen_w,_screen_h);
	const uint32_t crc=bb_state_crc((const uint8_t *)pixels,(size_t)w*h*4);free(pixels);return crc;
}
static void test_game_state() {
	if(getenv("BLUES_TEST_STATE_READ")) {
		uint8_t actual[BB_STATE_MAX_SIZE],expected[BB_STATE_MAX_SIZE],future[BB_STATE_MAX_SIZE];size_t actual_size,expected_size,future_size;
		assert(prepare_game_load(0));struct bb_state saved=*_prepared_state;saved.timestamp=0;
		assert(bb_state_encode(&saved,expected,sizeof(expected),&expected_size));
		assert(apply_prepared_game());encode_current_state(actual,&actual_size);
		assert(actual_size==expected_size && !memcmp(actual,expected,actual_size));
		assert(g_sys.read_save_file(1,future,sizeof(future),&future_size));
		state_trace();encode_current_state(actual,&actual_size);
		assert(actual_size==future_size && !memcmp(actual,future,actual_size));
		g_sys.input.quit=true;puts("state: separate process restored static references and reproduced identical future passed");return;
	}
	if(getenv("BLUES_TEST_STATE_TWO")) {g_vars.two_players_flag=true;init_level();}
	struct object_t *p=&g_vars.objects[OBJECT_NUM_PLAYER1];
	if(getenv("BLUES_TEST_STATE_ROOM")) {
		bool entered=false;
		for(int i=0;i<MAX_DOORS && !entered;++i) if(g_vars.doors[i].dst_y16*16>g_vars.screen_tilemap_h) {
			p->xpos=p->xpos16*16; p->xpos16=g_vars.doors[i].src_x16;p->ypos16=g_vars.doors[i].src_y16;
			p->xpos=p->xpos16*16;p->ypos=p->ypos16*16;
			do_level_enter_door(p);assert(camera_in_room());entered=true;
		}
		if(g_vars.level!=5) assert(entered);
	} else {
		g_sys.input.view_step=4;*_state_view_step=4;
		start_view_transition(_state_view,*_state_base_w,*_state_base_h,4);
		advance_view_transition(_state_view,100);
	}
	p=&g_vars.objects[OBJECT_NUM_PLAYER1];
	p->lifes_count=7;p->data51=2;p->vinyls_count=37;p->unk60=1;
	p->data5F=g_vars.level<5 ? g_vars.level+1 : 0;
	g_vars.found_music_instrument_flag=p->data5F!=0;g_vars.ending_counter=123;
	p->grab_state=1;p->grab_type=OBJECT_GRAB_CRATE;
	struct object_t *crate=&g_vars.objects[37];
	crate->type=2;crate->animframes_ptr=animframes_059d+58;crate->grab_type=0;crate->visible_flag=1;
	crate->xpos=p->xpos+8;crate->ypos=p->ypos-24;crate->xpos16=crate->xpos/16;crate->ypos16=crate->ypos/16;
	crate->anim_frame=crate->animframes_ptr[0][1];
	g_res.sql[100]^=1;g_res.triggers[5].unk16=7;
	for(int i=0;i<MAX_TRIGGERS;++i) if(g_res.triggers[i].op_table3) {p->trigger3=g_res.triggers[i].op_table3;break;}
	if(getenv("BLUES_TEST_STATE_DEAD")) {
		struct object_t *p2=&g_vars.objects[OBJECT_NUM_PLAYER2];
		p2->data51=0;p2->lifes_count=1;p2->special_anim=16;g_vars.player1_dead_flag=true;
		g_options.cheats &= ~CHEATS_UNLIMITED_LIFES;
	}
	update_assist_status();g_sys.set_scroll_interpolation(0,0,0,false);repaint_restored_state();
	const uint32_t before_pixels=state_frame_crc();
	struct bb_state state;assert(capture_game_state(&state));
	uint8_t bytes[BB_STATE_MAX_SIZE],roundtrip[BB_STATE_MAX_SIZE],expected[BB_STATE_MAX_SIZE];size_t size,other_size,expected_size;
	assert(bb_state_encode(&state,bytes,sizeof(bytes),&size));
	struct bb_state decoded;assert(bb_state_decode(&decoded,bytes,size));
	assert(!memcmp(&state.vars,&decoded.vars,sizeof(state.vars)));
	assert(save_current_game(0));
	/* Reject bad headers, truncated data, future versions, checksum errors and
	 * semantically invalid values without touching the running world. */
	for(size_t n=0;n<size;n+=17) assert(!bb_state_decode(&decoded,bytes,n));
	bytes[8]=99;assert(!bb_state_decode(&decoded,bytes,size));bytes[8]=1;
	bytes[size-1]^=1;assert(!bb_state_decode(&decoded,bytes,size));bytes[size-1]^=1;
	assert(g_sys.write_save_file(0,bytes,size-1));assert(!prepare_game_load(0));
	assert(!memcmp(&state.vars,&g_vars,sizeof(g_vars)) && !memcmp(state.map,g_res.sql,sizeof(state.map)));
	assert(g_sys.write_save_file(0,bytes,size));
	struct bb_state bad=state;bad.vars.level=6;assert(!bb_state_encode(&bad,roundtrip,sizeof(roundtrip),&other_size));
	bad=state;bad.resources^=1;assert(bb_state_encode(&bad,roundtrip,sizeof(roundtrip),&other_size));
	assert(g_sys.write_save_file(0,roundtrip,other_size));assert(!prepare_game_load(0));
	assert(!memcmp(&state.vars,&g_vars,sizeof(g_vars)));assert(g_sys.write_save_file(0,bytes,size));
	/* Advance a deterministic future and compare every serialized field after
	 * restoring and replaying the same inputs (including a zoom transition). */
	state_trace();encode_current_state(expected,&expected_size);
	if(getenv("BLUES_TEST_STATE_WRITE")) {assert(g_sys.write_save_file(1,expected,expected_size));g_sys.input.quit=true;puts("state: cross-process fixture and deterministic future saved");return;}
	assert(prepare_game_load(0));assert(apply_prepared_game());
	assert(state_frame_crc()==before_pixels);
	encode_current_state(roundtrip,&other_size);
	state.timestamp=0;assert(bb_state_encode(&state,bytes,sizeof(bytes),&size));

	assert(size==other_size && !memcmp(bytes,roundtrip,size));
	snapshot("restored-save");
	state_trace();encode_current_state(roundtrip,&other_size);
	assert(other_size==expected_size && !memcmp(expected,roundtrip,other_size));
	/* Reload from another native release, then from another level. */
	assert(switch_data_mode());
	const char **path=state.amiga ? &g_options.amiga_datapath : &g_options.dos_datapath;
	const char *valid_path=*path;*path="/nonexistent/blues-data";
	const struct vars_t before=g_vars;assert(!prepare_game_load(0));assert(!memcmp(&before,&g_vars,sizeof(before)));*path=valid_path;
	assert(prepare_game_load(0));assert(apply_prepared_game());
	assert(g_res.amiga_data==state.amiga && !memcmp(&state.vars,&g_vars,sizeof(g_vars)));
	start_selected_level((state.vars.level+1)%6);load_level_data(g_vars.level);
	g_vars.screen_tilemap_w=level_dim[g_vars.level*2];g_vars.screen_tilemap_h=level_dim[g_vars.level*2+1];init_level();g_vars.menu_level_jump=false;
	assert(prepare_game_load(0));assert(apply_prepared_game());
	encode_current_state(roundtrip,&other_size);assert(other_size==size && !memcmp(bytes,roundtrip,size));
	g_sys.input.quit=true;
	puts("state: portable full restore, corruption/resource rejection, deterministic replay, cross-mode/level load passed");
}

static bool fail_preview(uint8_t *rgb) {return false;}
static void test_save_slots() {
	const bool read=getenv("BLUES_TEST_SLOTS_READ")!=NULL;
	if(getenv("BLUES_TEST_SLOTS_ODD_SIZE")){SDL_SetWindowSize(_window,853,533);repaint_restored_state();}
	if(!read && getenv("BLUES_TEST_SLOTS_TWO")){g_vars.two_players_flag=true;init_level();}
	for(int slot=0;slot<SAVE_SLOT_COUNT;++slot) {
		if(!read) {
			g_vars.objects[OBJECT_NUM_PLAYER1].lifes_count=slot+2;
			g_vars.objects[OBJECT_NUM_PLAYER1].vinyls_count=slot+11;
			g_vars.objects[OBJECT_NUM_PLAYER2].lifes_count=20-slot;
			g_options.cheats=CHEATS_INVULNERABLE | (slot==7 ? CHEATS_UNLIMITED_LIFES : 0);
			g_sys.input.view_step=slot%3;*_state_view_step=slot%3;
			update_level_view(*_state_base_w,*_state_base_h,slot%3);_state_view->elapsed=300;
			update_assist_status();repaint_restored_state();
			uint8_t visible[SAVE_PREVIEW_BYTES],in_menu[SAVE_PREVIEW_BYTES];
			assert(sdl2_capture_save_preview(visible));
			_pause_menu_open=true;_pause_page=PAUSE_GAME;_pause_selection=0;sdl2_draw_screen(0,0,1);
			assert(sdl2_capture_save_preview(in_menu));assert(!memcmp(visible,in_menu,sizeof(visible)));
			const struct vars_t before=g_vars;
			assert(save_current_game(slot));assert(!memcmp(&before,&g_vars,sizeof(g_vars)));
			_pause_menu_open=false;
		}
		struct sys_save_slot info;inspect_game_save(slot,&info);
		assert(info.status==SAVE_READY && info.has_metadata && info.has_preview && info.lives[0]==slot+2 && info.infinite_lives==(slot==7));
		assert(info.timestamp>0);
		uint8_t *data=malloc(BB_STATE_MAX_SIZE);size_t size;assert(data);
		assert(g_sys.read_save_file(slot,data,BB_STATE_MAX_SIZE,&size) && READ_LE_UINT32(data+8)==2);
		const uint8_t *preview=bb_state_preview(data,size);assert(preview && !memcmp(info.preview,preview,SAVE_PREVIEW_BYTES));
		struct bb_state decoded;assert(bb_state_decode(&decoded,data,size));
		assert(info.level==decoded.vars.level && info.amiga==decoded.amiga && info.two_players==decoded.vars.two_players_flag);
		if(read) {
			assert(prepare_game_load(slot));assert(apply_prepared_game());
			assert(g_vars.objects[OBJECT_NUM_PLAYER1].lifes_count==slot+2 && g_vars.objects[OBJECT_NUM_PLAYER1].vinyls_count==slot+11);
			if(g_vars.two_players_flag)assert(g_vars.objects[OBJECT_NUM_PLAYER2].lifes_count==20-slot);
			uint8_t loaded[SAVE_PREVIEW_BYTES];assert(sdl2_capture_save_preview(loaded));assert(!memcmp(loaded,preview,sizeof(loaded)));
		} else if(slot==0) {
			/* Old-format saves remain loadable without a fabricated preview. */
			uint8_t *legacy=malloc(BB_STATE_MAX_SIZE);size_t legacy_size;assert(legacy);
			assert(bb_state_encode(&decoded,legacy,BB_STATE_MAX_SIZE,&legacy_size));assert(g_sys.write_save_file(slot,legacy,legacy_size));
			inspect_game_save(slot,&info);assert(info.status==SAVE_READY && !info.has_preview && info.lives[0]==2);
			assert(prepare_game_load(slot));free(_prepared_state);_prepared_state=NULL;
			assert(g_sys.write_save_file(slot,data,size));
			/* Corrupt image bytes and unsupported headers are separate states. */
			data[size-9]^=1;assert(g_sys.write_save_file(slot,data,size));inspect_game_save(slot,&info);assert(info.status==SAVE_DAMAGED && !info.has_preview);data[size-9]^=1;
			data[8]=99;assert(g_sys.write_save_file(slot,data,size));inspect_game_save(slot,&info);assert(info.status==SAVE_INCOMPATIBLE);data[8]=2;
			const char **path=decoded.amiga ? &g_options.amiga_datapath : &g_options.dos_datapath;
			const char *valid=*path;*path="/nonexistent/blues-data";assert(g_sys.write_save_file(slot,data,size));
			inspect_game_save(slot,&info);assert(info.status==SAVE_INCOMPATIBLE && info.has_preview && info.has_metadata);*path=valid;
			bool (*capture)(uint8_t *)=g_sys.capture_save_preview;g_sys.capture_save_preview=fail_preview;
			assert(!save_current_game(slot));g_sys.capture_save_preview=capture;
			uint8_t *retained=malloc(BB_STATE_MAX_SIZE);size_t retained_size;assert(retained);
			assert(g_sys.read_save_file(slot,retained,BB_STATE_MAX_SIZE,&retained_size) && retained_size==size && !memcmp(retained,data,size));
			free(retained);free(legacy);
		}
		free(data);
	}
	/* Open the production browser with all entries and their real previews. */
	_pause_options.inspect_save=inspect_game_save;_pause_options.save_game=save_current_game;_pause_options.prepare_load=prepare_game_load;
	_pause_options.save_error=game_save_error;
	for(int i=0;i<SAVE_SLOT_COUNT;++i)pause_refresh_save(i);
	_pause_menu_open=true;_pause_page=PAUSE_SLOTS;_save_browser_load=true;
	for(int i=0;i<SAVE_SLOT_COUNT;++i){_pause_selection=i;sdl2_draw_screen(0,0,1);}
	snapshot("eight-slots");_pause_menu_open=false;
	g_sys.input.quit=true;
	puts(read ? "slots: eight independent saves loaded in a fresh process with exact scene previews passed" : "slots: eight RGB previews, paused capture, metadata, legacy files, rejection states and failed capture protection passed");
}

static struct vars_t state_menu_before;
static uint32_t state_menu_script(uint32_t interval,void *param) {
	assert(SDL_GetAudioStatus()!=SDL_AUDIO_PLAYING);
	const int step=menu_timer_step++;
	if(step==0) {
		for(int i=0;i<4;++i)menu_tap(SDLK_DOWN);
		menu_tap(SDLK_RETURN);menu_tap(SDLK_DOWN);menu_tap(SDLK_RETURN);
		menu_tap(SDLK_RETURN); /* Empty slot stays in the browser. */
		push_menu_key(SDLK_ESCAPE,false);
	} else if(step==1) {
		menu_tap(SDLK_UP);menu_tap(SDLK_RETURN);menu_tap(SDLK_RETURN);
		menu_tap(SDLK_DOWN);menu_tap(SDLK_RETURN);push_menu_key(SDLK_ESCAPE,false);
	} else {
		menu_tap(SDLK_DOWN);menu_tap(SDLK_RETURN);menu_tap(SDLK_RETURN);
		menu_tap(SDLK_DOWN);push_menu_key(SDLK_RETURN,true);
	}
	return menu_timer_step==3 ? 0 : interval;
}
static void test_state_menu() {
	state_menu_before=g_vars;menu_timer_step=0;
	_pause_menu_open=true;
	_pause_page=PAUSE_MAIN;_pause_selection=4;sdl2_draw_screen(0,0,1);snapshot("state-main");
	_pause_page=PAUSE_GAME;_pause_selection=0;sdl2_draw_screen(0,0,1);snapshot("state-menu");
	_pause_page=PAUSE_SAVE_CONFIRM;_pause_selection=0;sdl2_draw_screen(0,0,1);snapshot("state-save-confirm");
	_pause_page=PAUSE_LOAD_CONFIRM;_pause_selection=0;sdl2_draw_screen(0,0,1);snapshot("state-load-confirm");
	_pause_menu_open=false;
	assert(SDL_AddTimer(60,state_menu_script,NULL));
	sdl2_run_pause_menu();
	assert(menu_timer_step==3 && _pause_options.load_game && _prepared_state);
	assert(!memcmp(&state_menu_before,&g_vars,sizeof(g_vars)));
	assert(!g_sys.input.direction && !g_sys.input.space && !g_sys.input.jump);
}

static void assert_timeline_matches(const struct bb_state *expected) {
	struct bb_state actual;assert(capture_game_state(&actual));
	struct bb_state copy=*expected;actual.timestamp=copy.timestamp=0;
	assert(!memcmp(&actual,&copy,sizeof(copy)));
}
static void test_rewind_history() {
	if(getenv("BLUES_TEST_REWIND_ODD_SIZE"))SDL_SetWindowSize(_window,853,533);
	if(getenv("BLUES_TEST_REWIND_TWO")){g_vars.two_players_flag=true;init_level();}
	struct bb_state *reference=calloc(300,sizeof(*reference));assert(reference);
	rewind_clear();
	for(int tick=0;tick<300;++tick) {
		g_vars.inp_key_left=(tick%40)<20;g_vars.inp_key_right=!g_vars.inp_key_left;
		g_vars.inp_key_up=g_vars.inp_key_down=g_vars.inp_key_jump=g_vars.inp_key_space=false;
		if(tick==40 || tick==140) {
			g_sys.input.view_step=tick==40 ? 4 : 0;*_state_view_step=g_sys.input.view_step;
			start_view_transition(_state_view,*_state_base_w,*_state_base_h,g_sys.input.view_step);
		}
		advance_view_transition(_state_view,33);traversal_tick();
		/* Persistent edits and inventory must follow the timeline as well. */
		g_res.sql[100]=(tick&1);g_res.triggers[5].unk16=tick%8;
		g_vars.objects[OBJECT_NUM_PLAYER1].vinyls_count=tick%40;
		g_vars.objects[OBJECT_NUM_PLAYER1].lifes_count=1+tick%9;
		if(tick==220 && getenv("BLUES_TEST_REWIND_ROOM")) {
			bool entered=false;
			struct object_t *p=&g_vars.objects[OBJECT_NUM_PLAYER1];
			for(int i=0;i<MAX_DOORS && !entered;++i)if(g_vars.doors[i].dst_y16*16>g_vars.screen_tilemap_h) {
				p->xpos16=g_vars.doors[i].src_x16;p->ypos16=g_vars.doors[i].src_y16;
				p->xpos=p->xpos16*16;p->ypos=p->ypos16*16;
				do_level_enter_door(p);assert(camera_in_room());entered=true;
			}
			assert(entered);
		}
		assert(capture_game_state(&reference[tick]));rewind_record();
		assert(_rewind_count==MIN(tick+1,REWIND_CAPACITY));
	}
	int frame=299;
	while(_rewind_count>1) {
		frame-=MIN(2,_rewind_count-1);rewind_step();
		assert(_rewind_running && _rewind_active && SDL_GetAudioStatus()!=SDL_AUDIO_PLAYING);
		assert_timeline_matches(&reference[frame]);
		assert(SDL_GetRenderTarget(_renderer)==NULL);
		if(frame==219 || frame==59) {
			/* Offscreen capture must preserve full framing and native HUD. */
			_rewind_active=false;sdl2_draw_screen(0,0,1);const uint32_t full=state_frame_crc();
			_rewind_active=true;sdl2_draw_screen(0,0,1);assert(_rewind_texture);
			assert(SDL_SetRenderTarget(_renderer,_rewind_texture)==0);
			assert(full==state_frame_crc());
			SDL_SetRenderTarget(_renderer,NULL);SDL_RenderSetLogicalSize(_renderer,0,0);
			SDL_RenderSetLogicalSize(_renderer,_screen_w,_screen_h);sdl2_draw_screen(0,0,1);
			if(frame==219)snapshot("rewind-vhs");
		}
	}
	assert(frame==59 && _rewind_count==1);
	for(int i=0;i<3;++i) {rewind_step();assert_timeline_matches(&reference[59]);}
	/* VHS changes pixels only, and returning to normal restores the image. */
	const uint32_t vhs=state_frame_crc();snapshot("rewind-oldest");
	rewind_finish();assert(!_rewind_active && !_rewind_running);
	repaint_restored_state();const uint32_t clean=state_frame_crc();snapshot("rewind-clean");
	assert(clean!=vhs);repaint_restored_state();assert(clean==state_frame_crc());
	assert_timeline_matches(&reference[59]);
	/* A new future starts at the rewound state, not the discarded frame 299. */
	uint8_t future[BB_STATE_MAX_SIZE],repeated[BB_STATE_MAX_SIZE];size_t nf,nr;
	state_trace();encode_current_state(future,&nf);rewind_record();assert(_rewind_count==2);
	rewind_step();assert_timeline_matches(&reference[59]);rewind_finish();
	state_trace();encode_current_state(repeated,&nr);assert(nf==nr && !memcmp(future,repeated,nf));
	/* Changing resource/settings context never crosses into an older world. */
	g_options.cheats^=CHEATS_INVULNERABLE;rewind_record();assert(_rewind_count==1);
	g_options.cheats^=CHEATS_INVULNERABLE;rewind_record();assert(_rewind_count==1);
	rewind_clear();rewind_step();assert(!_rewind_active && !_rewind_running);
	rewind_record();assert(_rewind_count==1);
	assert(switch_data_mode());rewind_record();assert(_rewind_count==1);
	assert(switch_data_mode());rewind_record();assert(_rewind_count==1);
	_prepared_state=malloc(sizeof(*_prepared_state));assert(_prepared_state);
	assert(capture_game_state(_prepared_state));assert(apply_prepared_game());
	assert(_rewind_count==0); /* Disk-load path also discards history. */
	free(reference);
	g_sys.input.view_step=0;
	puts("rewind: bounded eight-second timeline, exact full-state restore, rooms/zoom, discarded future, deterministic replay and clean VHS removal passed");
}
static uint32_t rewind_close_menu(uint32_t interval,void *context) {
	assert(!_rewind_active && SDL_GetAudioStatus()!=SDL_AUDIO_PLAYING);
	push_menu_key(SDLK_RETURN,true);return 0;
}
static void test_rewind_pause_menu() {
	assert(!_rewind_active && !_rewind_running);
	const struct vars_t before=g_vars;
	assert(SDL_AddTimer(30,rewind_close_menu,NULL));
	sdl2_run_pause_menu();
	assert(!memcmp(&before,&g_vars,sizeof(before)) && !_pause_menu_open);
}
static int rewind_expected_counter;
static void test_rewind_events() {
	bool paused=false;
	if(test_frames==2)test_rewind_history();
	if(test_frames==20 || test_frames==24 || test_frames==28 || test_frames==30) {
		assert(_rewind_count>3);
		rewind_expected_counter=_rewind_history[(_rewind_head+REWIND_CAPACITY-3)%REWIND_CAPACITY].vars.level_loop_counter;
		if(test_frames!=24)handle_keyevent(SDLK_r,true,&g_sys.input,&paused);
		else handle_controllerbutton(SDL_CONTROLLER_BUTTON_LEFTSHOULDER,true,&g_sys.input,&paused);
	}
	if(test_frames==21 || test_frames==25) {
		assert(_rewind_active && g_vars.level_loop_counter==rewind_expected_counter);
		if(test_frames==21)handle_keyevent(SDLK_r,false,&g_sys.input,&paused);
		else handle_controllerbutton(SDL_CONTROLLER_BUTTON_LEFTSHOULDER,false,&g_sys.input,&paused);
	}
	if(test_frames==22 || test_frames==26) {
		assert(!_rewind_active && !g_sys.input.direction && !g_sys.input.space && !g_sys.input.jump);
		assert(g_vars.level_loop_counter==rewind_expected_counter);
	}
	if(test_frames==27) {
		assert(g_vars.level_loop_counter==rewind_expected_counter+1);
	}
	if(test_frames==29) {
		assert(_rewind_active && g_vars.level_loop_counter==rewind_expected_counter);
		g_sys.input.menu=true; /* Opening pause while still holding rewind. */
	}
	if(test_frames==30)assert(!_pause_menu_open);
	if(test_frames==31) {
		assert(_rewind_active && g_vars.level_loop_counter==rewind_expected_counter);
		g_sys.input.quit=true;
		puts("rewind: actual game loop R/LB hold/release, paused audio, forward resume, pause and quit while rewinding passed");
	}
	++test_frames;
}

/* Actual map pixels at camera boundaries, including Amiga's screen-fixed
 * copper colors. Covers intermediate 60 Hz frames, both axes and reversals. */
static uint8_t scroll_map_pixel(int x,int y) {
	x=MAX(0,MIN(g_vars.screen_tilemap_w*16-1,x));y=MAX(0,MIN(1999,y));
	const uint8_t tile=*lookup_sql(x/16,y/16);
	return g_res.tiles[g_vars.screen_tile_lut[tile&127]*16+(tile&128 ? 320 : 0)+(y&15)*640+(x&15)];
}
static void test_scroll_edges() {
	assert(GAME_SCREEN_W==320 && GAME_SCREEN_H==200);
	SDL_SetWindowSize(_window,320,200);
	g_sys.render_clear_sprites();
	uint32_t pixels[320*200];int mismatches_old=0,cases=0;
	const int max_x=g_vars.screen_tilemap_w*16-320,max_y=g_vars.screen_tilemap_h-160;
	for(int corner=0;corner<5;++corner) {
		g_vars.screen_tilemap_xorigin=corner==4 ? 113 : ((corner&1) ? max_x : 0);
		g_vars.screen_tilemap_yorigin=corner==4 ? 81 : ((corner&2) ? max_y : 0);
		g_vars.screen_tilemap_xoffset=-(g_vars.screen_tilemap_xorigin&15);
		g_vars.screen_tilemap_yoffset=-(g_vars.screen_tilemap_yorigin&15);
		screen_clear(0);do_level_redraw_tilemap(g_vars.screen_tilemap_xorigin,g_vars.screen_tilemap_yorigin);
		copy_level_bitmap();assert(_scroll_margin==32);
		for(int dx=-6;dx<=6;dx+=6)for(int dy=-4;dy<=4;dy+=4) {
			for(int phase=0;phase<=2;++phase) {
				const int sx=dx*(2-phase)/2,sy=dy*(2-phase)/2;
				sdl2_draw_screen(sx,sy,phase/2.f);
				SDL_RenderSetLogicalSize(_renderer,0,0);
				assert(SDL_RenderReadPixels(_renderer,NULL,SDL_PIXELFORMAT_ARGB8888,pixels,320*4)==0);
				SDL_RenderSetLogicalSize(_renderer,320,200);
				for(int y=0;y<160;++y)for(int x=0;x<320;++x) {
					const uint8_t color=scroll_map_pixel(g_vars.screen_tilemap_xorigin+x-sx,g_vars.screen_tilemap_yorigin+y-sy);
					const int line=y*COPPER_BARS_H/160;
					const uint32_t expected=color==_copper_color_key && line<COPPER_BARS_H ? _copper_palette[line] : _screen_palette[color];
					if((pixels[(y+14)*320+x]&0xffffff)!=(expected&0xffffff)) {
						fprintf(stderr,"scroll pixel mismatch corner=%d delta=%d,%d phase=%d at=%d,%d actual=%06x expected=%06x\n",corner,dx,dy,phase,x,y,pixels[(y+14)*320+x]&0xffffff,expected&0xffffff);assert(false);
					}
				}
				++cases;
				/* Recreate the former unpadded translation to prove this scene
				 * catches clear strips, HUD bleed and scrolling copper colors. */
				if(corner==4 && dx==6 && dy==4 && phase==1) {
					_scroll_margin=0;sdl2_draw_screen(sx,sy,.5f);
					uint32_t old[320*200];SDL_RenderSetLogicalSize(_renderer,0,0);
					assert(SDL_RenderReadPixels(_renderer,NULL,SDL_PIXELFORMAT_ARGB8888,old,320*4)==0);
					SDL_RenderSetLogicalSize(_renderer,320,200);_scroll_margin=32;
					for(int y=14;y<174;++y)for(int x=0;x<320;++x)mismatches_old+=(pixels[y*320+x]!=old[y*320+x]);
					sdl2_draw_screen(sx,sy,.5f);snapshot("scroll-edges-fixed");
				}
			}
		}
	}
	assert(mismatches_old>0);
	printf("scroll: %d full-frame edge/diagonal/reversal samples match map pixels and fixed copper rows; former renderer fails %d pixels passed\n",cases,mismatches_old);
	g_sys.input.quit=true;
}

static uint8_t native_sprite_pixel(int frame,int x,int y) {
	const uint8_t *src=g_res.spr_frames[frame];
	const int w=READ_LE_UINT16(src-2),h=READ_LE_UINT16(src-4),plane=w*h/8;
	uint8_t color=0;
	for(int b=0;b<4;++b)if(src[b*plane+y*w/8+x/8]&(0x80u>>(x&7)))color|=1<<b;
	return color;
}
static void read_native_test_pixels(uint32_t *pixels) {
	SDL_RenderSetLogicalSize(_renderer,0,0);
	assert(SDL_RenderReadPixels(_renderer,NULL,SDL_PIXELFORMAT_ARGB8888,pixels,320*4)==0);
	SDL_RenderSetLogicalSize(_renderer,320,200);
}
static void test_world_tile_redraw() {
	uint8_t before[320*200],expected[320*200],map[BB_STATE_MAP_SIZE];
	memcpy(map,g_res.sql,sizeof(map));
	g_vars.screen_tilemap_xorigin=77;g_vars.screen_tilemap_yorigin=93;
	g_vars.screen_tilemap_xoffset=-(77&15);g_vars.screen_tilemap_yoffset=-(93&15);
	screen_clear(0);do_level_redraw_tilemap(77,93);memcpy(before,g_res.vga,sizeof(before));
	/* A lift changes cable/shaft tiles at world coordinates while the camera
	 * moves. Incremental paint must be identical to a complete map redraw. */
	for(int ty=3;ty<19;++ty)for(int tx=2;tx<27;++tx) {
		memcpy(g_res.sql,map,sizeof(map));memcpy(g_res.vga,before,sizeof(before));
		const int tile=(*lookup_sql(tx,ty)+37)&255;
		do_level_update_tile(tx,ty,tile);memcpy(expected,g_res.vga,sizeof(expected));
		memcpy(g_res.vga,before,sizeof(before));do_level_redraw_tilemap(77,93);
		assert(!memcmp(expected,g_res.vga,sizeof(expected)));
	}
	memcpy(g_res.sql,map,sizeof(map));
	puts("tiles: 400 on/offscreen lift edits match full redraw with a non-aligned moving camera passed");
}

static void snapshot_amiga_occlusion() {
	if(!getenv("BLUES_VIEW_TEST_OUTPUT"))return;
	struct bb_state before;assert(capture_game_state(&before));
	for(int y=8;y<g_vars.screen_tilemap_h/16-8;++y)for(int x=10;x<110;++x) {
		const int tile=*lookup_sql(x,y),mask=g_res.triggers[tile].foreground_tile_num;
		if(mask>=g_res.amiga_mask_count)continue;
		bool solid=true;for(int i=0;i<32;++i)solid&=g_res.amiga_masks[mask][i]==255;
		if(!solid)continue;
		for(int i=0;i<MAX_OBJECTS;++i)g_vars.objects[i].visible_flag=false;
		struct object_t *p=&g_vars.objects[OBJECT_NUM_PLAYER1];p->visible_flag=true;
		p->xpos=x*16+8;p->ypos=y*16+24;p->anim_frame=0;
		g_vars.screen_tilemap_xorigin=MAX(0,p->xpos-160);g_vars.screen_tilemap_yorigin=MAX(0,p->ypos-96);
		g_vars.screen_tilemap_xoffset=-(g_vars.screen_tilemap_xorigin&15);g_vars.screen_tilemap_yoffset=-(g_vars.screen_tilemap_yorigin&15);
		p->screen_xpos=p->xpos-g_vars.screen_tilemap_xorigin;p->screen_ypos=p->ypos-g_vars.screen_tilemap_yorigin;
		repaint_restored_state();snapshot("amiga-occlusion-fixed");
		int keep=0;for(int i=0;i<_sprites_count;++i)if(_sprites[i].sheet!=RENDER_SPR_FG)_sprites[keep++]=_sprites[i];
		_sprites_count=keep;sdl2_draw_screen(0,0,1);snapshot("amiga-occlusion-before");
		restore_timeline_state(&before);repaint_restored_state();return;
	}
}
static void snapshot_city_lift() {
	if((g_vars.level!=4 && g_vars.level!=5) || !getenv("BLUES_VIEW_TEST_OUTPUT"))return;
	struct bb_state before;assert(capture_game_state(&before));
	int slot=-1;for(int i=0;i<MAX_OBJECTS;++i)if(g_vars.objects[i].type==(g_vars.level==5 ? 40 : 35)){slot=i;break;}
	assert(slot>=0);
	for(int i=0;i<MAX_OBJECTS;++i)g_vars.objects[i].visible_flag=false;
	struct object_t *lift=&g_vars.objects[slot],*player=&g_vars.objects[OBJECT_NUM_PLAYER1];
	const int lift_frame=g_vars.level==5 ? 160 : SPRITES_COUNT;
	lift->visible_flag=player->visible_flag=true;lift->anim_frame=lift_frame;
	player->xpos=lift->xpos;player->ypos=lift->ypos-15;player->anim_frame=0;
	g_vars.screen_tilemap_xorigin=MAX(0,MIN(player->xpos-160,g_vars.screen_tilemap_w*16-320));
	g_vars.screen_tilemap_yorigin=MAX(0,MIN(lift->ypos-80,g_vars.screen_tilemap_h-160));
	g_vars.screen_tilemap_xoffset=-(g_vars.screen_tilemap_xorigin&15);g_vars.screen_tilemap_yoffset=-(g_vars.screen_tilemap_yorigin&15);
	for(int i=0;i<MAX_OBJECTS;++i) {
		g_vars.objects[i].screen_xpos=g_vars.objects[i].xpos-g_vars.screen_tilemap_xorigin;
		g_vars.objects[i].screen_ypos=g_vars.objects[i].ypos-g_vars.screen_tilemap_yorigin;
	}
	repaint_restored_state();snapshot(g_vars.level==5 ? "concert-lift-fixed" : "city-lift-fixed");
	/* Recreate the exact old palette error in the same real scene. */
	struct spritesheet_t *sheet=&_spritesheets[RENDER_SPR_LEVEL];
	const int w=READ_LE_UINT16(g_res.spr_frames[lift_frame]-2),h=READ_LE_UINT16(g_res.spr_frames[lift_frame]-4);
	SDL_Surface *surface=SDL_CreateRGBSurface(0,w,h,8,0,0,0,0);assert(surface);
	SDL_SetSurfacePalette(surface,_palette);SDL_SetColorKey(surface,1,0);
	for(int y=0;y<h;++y)for(int x=0;x<w;++x)((uint8_t *)surface->pixels)[y*surface->pitch+x]=native_sprite_pixel(lift_frame,x,y);
	SDL_Texture *broken=SDL_CreateTextureFromSurface(_renderer,surface);assert(broken);SDL_FreeSurface(surface);
	SDL_Texture *texture=sheet->texture;const SDL_Rect rect=sheet->r[lift_frame-SPRITES_COUNT];
	sheet->texture=broken;sheet->r[lift_frame-SPRITES_COUNT]=(SDL_Rect){0,0,w,h};
	sdl2_draw_screen(0,0,1);snapshot(g_vars.level==5 ? "concert-lift-before" : "city-lift-before");
	sheet->texture=texture;sheet->r[lift_frame-SPRITES_COUNT]=rect;SDL_DestroyTexture(broken);
	restore_timeline_state(&before);repaint_restored_state();
}

static void test_native_lift_animation() {
	if(g_vars.level!=0 && g_vars.level!=4 && g_vars.level!=5)return;
	/* These references come from the real level/object assets, independent
	 * of the production frame-selection helper. Concert is the regression
	 * case omitted by the previous level-number-specific correction. */
	const int frame=g_vars.level==5 ? 160 : 146;
	const int type=g_vars.level==5 ? 40 : (g_vars.level==4 ? 35 : 7);
	int instances=0;for(int i=0;i<MAX_OBJECTS;++i)instances+=g_vars.objects[i].type==type;
	assert(instances>0);
	const uint8_t *src=g_res.spr_frames[frame];
	const int w=READ_LE_UINT16(src-2),h=READ_LE_UINT16(src-4);
	uint8_t canvas[320*200];memset(canvas,24,sizeof(canvas));
	uint32_t pixels[320*200];int different=0;
	for(int y=0;y<h;++y)for(int x=0;x<w;++x)if(native_sprite_pixel(frame,x,y) && native_sprite_pixel(frame,x,y)!=native_sprite_pixel(frame+1,x,y))++different;
	assert(different>0);
	g_sys.set_scroll_interpolation(0,0,0,false);
	for(int tick=0;tick<60;++tick) {
		const int px=40+2*(tick%30),py=60+2*(tick%8),flip=tick&1;
		const int prev_x=tick ? 40+2*((tick-1)%30) : px,prev_y=tick ? 60+2*((tick-1)%8) : py;
		g_sys.begin_sprite_frame();g_sys.render_clear_sprites();g_sys.set_sprite_owner(700);
		g_sys.render_add_sprite(RENDER_SPR_LEVEL,frame-SPRITES_COUNT,px,py,flip);
		g_sys.set_sprite_owner(-1);g_sys.set_scroll_interpolation(0,0,33,true);
		g_sys.copy_bitmap(canvas,320,200);
		for(int phase=0;phase<=2;++phase) {
			sdl2_draw_screen(0,0,phase/2.f);read_native_test_pixels(pixels);
			const int dx=prev_x+(px-prev_x)*phase/2,dy=prev_y+(py-prev_y)*phase/2;
			for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
				const int sample=flip ? w-1-x : x;
				const bool covered=native_sprite_pixel(frame,sample,y)!=0;
				const uint8_t color=covered ? (g_vars.level==4 ? g_res.tiles[y*640+(sample&15)] : native_sprite_pixel(frame+1,sample,y)) : 24;
				assert((pixels[(dy+y)*320+dx+x]&0xffffff)==(_screen_palette[color]&0xffffff));
			}
		}
	}
	g_sys.set_scroll_interpolation(0,0,0,false);
	printf("native lifts: %d real stage instances, frame %d, 60 moving/flipped ticks and all intermediate pixels passed\n",instances,frame);
}

static void test_amiga_layers() {
	assert(g_res.amiga_data);

	SDL_SetWindowSize(_window,320,200);
	g_sys.set_scroll_interpolation(0,0,0,false);
	snapshot_amiga_occlusion();
	snapshot_city_lift();
	uint8_t canvas[320*200];memset(canvas,24,sizeof(canvas));
	uint32_t pixels[320*200];int checked=0,opaque=0,holes=0;
	/* Every native .m entry is rendered over a visible marker behind it,
	 * retaining solid black and preserving unmasked holes pixel-for-pixel. */
	for(int tile=0;tile<256;++tile) {
		const int mask=g_res.triggers[tile].foreground_tile_num;
		if(mask>=g_res.amiga_mask_count)continue;
		g_sys.render_clear_sprites();g_sys.copy_bitmap(canvas,320,200);
		g_sys.render_add_sprite(RENDER_SPR_FG,tile,64,64,0);sdl2_draw_screen(0,0,1);
		read_native_test_pixels(pixels);
		for(int y=0;y<16;++y)for(int x=0;x<16;++x) {
			const bool covered=(g_res.amiga_masks[mask][y*2+x/8]&(0x80u>>(x&7)))!=0;
			const int index=tile&127;
			const uint8_t color=covered ? g_res.tiles[((index/20)*640+index%20)*16+(tile&128 ? 320 : 0)+y*640+x] : 24;
			const uint32_t expected=covered && color==_copper_color_key ? _copper_palette[(64+y-14)*80/160] : _screen_palette[color];
			assert((pixels[(64+y)*320+64+x]&0xffffff)==(expected&0xffffff));
			opaque+=covered;holes+=!covered;
		}
		++checked;
	}
	if(g_res.amiga_mask_count)assert(checked && opaque);
	/* The bottom of the playfield must use the last copper row, rather than
	 * abruptly reverting to the palette key fourteen lines above the HUD. */
	if(_copper_color_key>=0) {
		memset(g_res.vga,_copper_color_key,GAME_SCREEN_W*GAME_SCREEN_H);
		g_sys.render_clear_sprites();copy_level_bitmap();sdl2_draw_screen(0,0,1);
		read_native_test_pixels(pixels);
		for(int y=0;y<160;++y)assert((pixels[(14+y)*320+100]&0xffffff)==(_copper_palette[y*80/160]&0xffffff));
	}
	/* Native lift/level sprite frames must not borrow pixels from adjacent
	 * atlas entries, including flips and alternating frames at 60 Hz. */
	g_sys.copy_bitmap(canvas,320,200);
	for(int tick=0;tick<8;++tick) {
		const int frame=SPRITES_COUNT+(tick&1),flip=(tick>>1)&1;
		const uint8_t *src=g_res.spr_frames[frame];
		const int w=READ_LE_UINT16(src-2),h=READ_LE_UINT16(src-4);
		g_sys.begin_sprite_frame();g_sys.render_clear_sprites();
		/* Only the shopping-centre frames are alternating lift frames with
		 * a shared footprint; other levels start with unrelated NPC frames. */
		g_sys.set_sprite_owner(g_vars.level==0 ? 500 : 500+tick);
		g_sys.render_add_sprite(RENDER_SPR_LEVEL,frame-SPRITES_COUNT,50,60,flip);
		g_sys.set_sprite_owner(-1);g_sys.set_scroll_interpolation(0,0,33,true);
		sdl2_draw_screen(0,0,.5f);read_native_test_pixels(pixels);
		for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
			const int sample_x=flip ? w-1-x : x;
			uint8_t color=native_sprite_pixel(frame,sample_x,y);
			if(frame==SPRITES_COUNT && (g_vars.level==0 || g_vars.level==4) && color) {
				/* Independent city reference: the native map's first BLK tile,
				 * not the malformed sprite colors copied by the old test. */
				color=g_vars.level==4 ? g_res.tiles[y*640+(sample_x&15)] : native_sprite_pixel(frame+1,sample_x,y);
			}
			assert((pixels[(60+y)*320+50+x]&0xffffff)==(_screen_palette[color ? color : 24]&0xffffff));
		}
	}
	/* Common props/lift effects come from objet (four planes / level colors),
	 * unlike player frames (three planes / colors 16..23). */
	g_sys.set_scroll_interpolation(0,0,0,false);
	for(int frame=101;frame<115;++frame) {
		const uint8_t *src=g_res.spr_frames[frame];
		const int w=READ_LE_UINT16(src-2),h=READ_LE_UINT16(src-4);
		g_sys.render_clear_sprites();g_sys.render_add_sprite(RENDER_SPR_GAME,frame,50,60,0);
		sdl2_draw_screen(0,0,1);read_native_test_pixels(pixels);
		for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
			const uint8_t color=native_sprite_pixel(frame,x,y);
			assert((pixels[(60+y)*320+50+x]&0xffffff)==(_screen_palette[color ? color : 24]&0xffffff));
		}
	}

	test_native_lift_animation();
	printf("Amiga: %d foreground tiles (%d opaque / %d hole pixels), complete copper height, native prop palettes and alternating lift frames passed\n",checked,opaque,holes);
	g_sys.input.quit=true;
}

static void test_events() {
	sdl2_process_events();
	if (_sprites_cliprect.y != TILEMAP_OFFSET_Y) {
        if (choosing_character) {
            g_sys.input.direction=((character_events==0 && character_choice!=0) || (character_events==1 && character_choice==2)) ? INPUT_DIRECTION_RIGHT : 0;
            g_sys.input.space=character_events >= (character_choice==2 ? 2 : 1);
            ++character_events;
            if (g_sys.input.space) choosing_character=false;
            return;
        }
		g_sys.input.space = true; /* Skip title, character choice and intermission. */
		return;
	}
	g_sys.input.space = false;
	if (getenv("BLUES_TEST_AMIGA_LAYERS")) {if(test_frames++>=2){test_world_tile_redraw();test_amiga_layers();}return;}
	if (getenv("BLUES_TEST_SCROLL_EDGES")) {if(test_frames++>=2)test_scroll_edges();return;}
	if (getenv("BLUES_TEST_REWIND")) {test_rewind_events();return;}
	if (getenv("BLUES_TEST_SLOTS")) {if(test_frames++>=2)test_save_slots();return;}
	if (getenv("BLUES_TEST_STATE_MENU")) {
		if(test_frames==2) g_sys.input.menu=true;
		if(test_frames==3) {assert(!memcmp(&state_menu_before,&g_vars,sizeof(g_vars)));g_sys.input.quit=true;puts("state menu: rejected missing file, confirmed save/load, frozen world and clean restore passed");}
		++test_frames;return;
	}
	if (getenv("BLUES_TEST_STATE")) {if(test_frames++>=2)test_game_state();return;}
	if (getenv("BLUES_TEST_PREFS_EXPECT")) {test_preferences_loaded();return;}
	if (getenv("BLUES_TEST_LEVELS")) {test_level_events();return;}
	if (getenv("BLUES_TEST_MENU") || getenv("BLUES_TEST_OPTIONS") || getenv("BLUES_TEST_VOLUMES") || getenv("BLUES_TEST_ASSIST_MENU") || getenv("BLUES_TEST_RESET_PREFS")) {
		if (test_frames == 0) {
			g_sys.input.view_step = 4;
			if(getenv("BLUES_TEST_ASSIST_TWO")) {g_vars.two_players_flag=1;init_level();}
		}
		if (test_frames == 2) {
			bool paused = false;
			handle_keyevent(SDLK_ESCAPE, false, &g_sys.input, &paused);
		}
		if (test_frames == 3) {
			assert(menus_tested == 1);
			if(getenv("BLUES_TEST_ASSIST_MENU")) {
				assert(((g_options.cheats & CHEATS_INVULNERABLE)!=0) != options_before_test.invulnerable);
				assert(((g_options.cheats & CHEATS_UNLIMITED_LIFES)!=0) != options_before_test.infinite_lives);
				assert(g_vars.objects[OBJECT_NUM_PLAYER1].lifes_count==options_before_test.lives[0]+2);
				for(int i=0;i<2;++i)if(assist_player_active(i))assert(g_vars.objects[OBJECT_NUM_PLAYER1+i].data51==assist_health_before[i]);
				g_sys.input.switch_mode=true;
			} else if(getenv("BLUES_TEST_VOLUMES")) {
				assert(g_options.music_volume==40 && g_options.effects_volume==0);
				g_sys.input.switch_mode=true;
			} else if (!getenv("BLUES_TEST_OPTIONS") && !getenv("BLUES_TEST_RESET_PREFS")) g_sys.input.quit = true;
		}
        if (test_frames==4 && getenv("BLUES_TEST_RESET_PREFS")) {
            assert(g_options.camera_mode==CAMERA_SMOOTH && g_options.hud_mode==HUD_CLASSIC);
            assert(g_options.initial_lives[0]==3 && g_options.initial_lives[1]==3);
            assert(!g_options.cheats && g_options.music_volume==100 && g_options.effects_volume==100);
            assert(g_vars.objects[OBJECT_NUM_PLAYER1].data51==assist_health_before[0]);
            assert(!g_res.amiga_data && _game_keys[BIND_ACTION]==SDLK_SPACE);
            g_sys.input.quit=true;
        }
		if(test_frames==4 && getenv("BLUES_TEST_ASSIST_MENU")) {
			assert(g_res.amiga_data != options_before_test.amiga);
			assert(_assist_invulnerable != options_before_test.invulnerable && _assist_lives != options_before_test.infinite_lives);
			assert(g_vars.objects[OBJECT_NUM_PLAYER1].lifes_count==options_before_test.lives[0]+2);
			snapshot("assists-gameplay");
			g_sys.input.quit=true;
		}
		if(test_frames==4 && getenv("BLUES_TEST_VOLUMES")) {
			assert(g_options.music_volume==40 && g_options.effects_volume==0);
			test_native_volumes();
			g_sys.input.quit=true;
		}
		if (test_frames == 4 && getenv("BLUES_TEST_OPTIONS")) {
			assert(g_res.amiga_data != options_before_test.amiga);
			assert(g_options.camera_mode != options_before_test.camera && g_options.hud_mode != options_before_test.hud);
			assert(g_options.scroll_rate != options_before_test.render_rate && _scroll_rate == g_options.scroll_rate);
			assert(g_sys.input.view_step == options_before_test.view_step+1);
			assert((_hud_width > 0) == (g_options.hud_mode == HUD_FIXED));
			g_sys.input.quit=true;
		}
		++test_frames;
		return;
	}
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
		if (getenv("BLUES_TEST_TRAVERSAL")) {test_traversal();return;}
		if (getenv("BLUES_TEST_ASSISTS")) {
			test_assists();
			return;
		}
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
	if (!SDL_getenv("BLUES_CONFIG_FILE")) SDL_setenv("BLUES_CONFIG_FILE","",1);
	if (!SDL_getenv("BLUES_SAVE_DIR")) SDL_setenv("BLUES_SAVE_DIR","",1);
	if (getenv("BLUES_TEST_MODES") || getenv("BLUES_TEST_VOLUMES")) g_sys.start_audio = capture_start_audio;
	g_sys.process_events = test_events;
	if (getenv("BLUES_TEST_REWIND")) g_sys.run_pause_menu=test_rewind_pause_menu;
	if (getenv("BLUES_TEST_STATE_MENU")) g_sys.run_pause_menu=test_state_menu;
	if (getenv("BLUES_TEST_LEVELS")) g_sys.run_pause_menu=test_level_menu;
	if (getenv("BLUES_TEST_MENU") || getenv("BLUES_TEST_OPTIONS") || getenv("BLUES_TEST_VOLUMES") || getenv("BLUES_TEST_ASSIST_MENU") || getenv("BLUES_TEST_RESET_PREFS")) g_sys.run_pause_menu = test_run_pause_menu;
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
	if (getenv("BLUES_TEST_MENU") || getenv("BLUES_TEST_OPTIONS") || getenv("BLUES_TEST_VOLUMES") || getenv("BLUES_TEST_ASSIST_MENU") || getenv("BLUES_TEST_RESET_PREFS")) {
		assert(!_pause_menu_enabled);
		puts("menu: frozen game/objects/map/zoom/audio, preserved scene and clean resume through actual game loop passed");
		return result;
	}
	if (getenv("BLUES_TEST_AMIGA_LAYERS")) return result;
	if (getenv("BLUES_TEST_SCROLL_EDGES")) return result;
	if (getenv("BLUES_TEST_REWIND")) {assert(!_rewind_history && !_rewind_active);return result;}
	if (getenv("BLUES_TEST_SLOTS")) return result;
	if (getenv("BLUES_TEST_STATE_MENU")) return result;
	if (getenv("BLUES_TEST_STATE")) return result;
	if (getenv("BLUES_TEST_PREFS_EXPECT")) return result;
	if (getenv("BLUES_TEST_LEVELS")) {assert(levels_started==7);return result;}
	if (getenv("BLUES_TEST_TRAVERSAL")) return result;
	if (getenv("BLUES_TEST_ASSISTS")) return result;
	if (getenv("BLUES_TEST_HUD_SWITCH")) return result;
	if (getenv("BLUES_TEST_CAMERA")) return result;
	assert(test_frames == 176 && out_frames >= 40 && closer_frames >= 40);
	assert(GAME_SCREEN_W == base_w && GAME_SCREEN_H == base_h);
	printf("viewport: eased zoom-out/in, mid-transition reversal, reset, four map corners, fixed window, restored menus: passed\n");
	return result;
}
