/* Uses private production routines to verify that presentation never mutates
 * either native simulation. Requires the user's original data directory. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../sys_sdl2.c"
#ifdef PROBE_JA
#include "../ja/level.c"
#else
#include "../p2/level.c"
#endif
#include "family_save_regressions.h"
struct options_t g_options;
static uint32_t frozen() { return 1234; }
static uint32_t checksum(const uint8_t *p, size_t n) {
  uint32_t h = 2166136261u;
  while (n--)
    h = (h ^ *p++) * 16777619u;
  return h;
}

#ifdef PROBE_JA
static uint32_t display_hash() {
  int w, h;
  SDL_GetRendererOutputSize(_renderer, &w, &h);
  uint8_t *pixels = calloc((size_t)w * h, 4);
  assert(pixels);
  SDL_RenderSetLogicalSize(_renderer, 0, 0);
  assert(SDL_RenderReadPixels(_renderer, NULL, SDL_PIXELFORMAT_ARGB8888, pixels,
                              w * 4) == 0);
  const uint32_t value = checksum(pixels, (size_t)w * h * 4);
  free(pixels);
  SDL_RenderSetLogicalSize(_renderer, _screen_w, _screen_h);
  return value;
}
static void visible_sprite_sleep(int duration) {
  (void)duration;
  /* Every intermediate presentation frame must retain the drawn actors. */
  assert(_sprites_count > 0 && "actors were cleared before 60/120 Hz frames");
  const int count = _sprites_count;
  for (int i = 1; i <= 4; ++i) {
    sdl2_draw_screen(0, 0, i / 4.f);
    assert(_sprites_count == count);
  }
}
static void regression_sprites() {
  for (int rate = 60; rate <= 120; rate *= 2) {
    g_options.scroll_rate = rate;
    g_sys.set_scroll_render_rate(rate);
    for (int i = 0; i < OBJECTS_COUNT; ++i)
      g_vars.objects_table[i].spr_num = 0xFFFF;
    g_vars.objects_table[0].spr_num = 19;
    g_vars.objects_table[0].x_pos = 100;
    g_vars.objects_table[0].y_pos = 100;
    _family_before_draw = g_vars;
    family_present();
    assert(_sprites_count > 0);
    g_sys.sleep = visible_sprite_sleep;
    level_sync();
    assert(_sprites_count > 0);
  }
}
static void regression_resize() {
  for (int hud = 0; hud < 2; ++hud)
    for (int zoom = 0; zoom <= 8; ++zoom) {
      g_options.hud_mode = hud;
      g_sys.input.view_step = zoom;
      family_end();
      init_level(get_level());
      family_begin();
      family_present();
      family_end();
      assert(_screen_w == 320 && _screen_h == 200);
      init_level(get_level());
      family_begin();
      assert((_family_w == 0 ||
              (_family_w == _screen_w && _family_h == _screen_h)) &&
             "stale viewport after death/reload");
      family_present();
      assert(_family_w == _screen_w && _family_h == _screen_h);
      assert(GAME_SCREEN_W == 320 && GAME_SCREEN_H == 200);
      family_end();
      uint8_t *large = calloc(400 * 240, 1);
      assert(large);
      sdl2_copy_bitmap(large, 400, 240);
      free(large);
      family_begin();
    }
}

static uint32_t route_time;
static int route_sleeps, route_after_restart, route_x, route_draws,
    route_powered;
static bool route_killed, route_restarted, route_in_powerdown, route_scroll;
static int motion_frames, motion_player_x;
static double motion_camera_x;
static uint32_t route_clock() { return route_time; }
static void route_fade() {}
static void route_sleep(int duration) {
  route_time += MAX(1, duration);
  assert(++route_sleeps < 2000);
  if (_family_active && _family_camera_valid) {
    assert(_screen_w == _family_w && _screen_h == _family_h);
    if (route_scroll) {
      const int px = g_vars.players_table[0].obj.x_pos;
      if (motion_frames > 30 && motion_frames < 160 && px >= motion_player_x &&
          px - motion_player_x < 32 && _family_x < motion_camera_x - .001) {
        fprintf(stderr,
                "camera reversed while moving right: frame %d player %d -> %d "
                "camera %.3f -> %.3f native %d\n",
                motion_frames, motion_player_x, px, motion_camera_x, _family_x,
                family_native_x());
        abort();
      }
      motion_player_x = px;
      motion_camera_x = _family_x;
      ++motion_frames;
    }
    const int sprites = _sprites_count;
    if (route_in_powerdown) {
      bool player_drawn = false;
      for (int i = 0; i < _sprites_count; ++i)
        if (_sprites[i].owner == 0) {
          assert(_sprites[i].num == (g_vars.objects_table[0].spr_num & 0x1FFF));
          player_drawn = true;
        }
      assert(player_drawn);
    }
    for (int i = 1; i <= 4; ++i) {
      const float t = i / 4.f;
      if (_scroll_interpolation_active)
        sdl2_present_at(_scroll_update_timestamp +
                        (uint64_t)(_scroll_period_counter * t));
      else
        sdl2_draw_screen((float)_scroll_camera_offset_x,
                         (float)_scroll_camera_offset_y, 1);
      assert(_sprites_count == sprites);
    }
    const char *capture = SDL_getenv("PF_CAPTURE");
    if (capture && route_after_restart == 19 && route_powered &&
        g_options.scroll_rate == 120 && g_options.hud_mode == HUD_FIXED &&
        g_sys.input.view_step == 4) {
      int w, h;
      SDL_GetRendererOutputSize(_renderer, &w, &h);
      SDL_Surface *shot =
          SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
      assert(shot);
      SDL_RenderSetLogicalSize(_renderer, 0, 0);
      assert(SDL_RenderReadPixels(_renderer, NULL, shot->format->format,
                                  shot->pixels, shot->pitch) == 0);
      assert(SDL_SaveBMP(shot, capture) == 0);
      SDL_FreeSurface(shot);
      SDL_RenderSetLogicalSize(_renderer, _screen_w, _screen_h);
    }
    ++route_draws;
    if (route_restarted)
      ++route_after_restart;
  }
}
static void route_events() {
  sdl2_process_events();
  g_sys.input.space = !_family_active;
  if (!_family_active)
    return;
  if (route_scroll) {
    g_sys.input.direction =
        motion_frames < 180 ? INPUT_DIRECTION_RIGHT : INPUT_DIRECTION_LEFT;
    g_sys.input.jump = motion_frames % 60 < 8;
    if (motion_frames >= 360)
      g_sys.input.quit = true;
    return;
  }
  struct player_t *player = &g_vars.players_table[0];
  if (!route_killed && g_vars.level_loop_counter >= 12) {
    route_killed = true;
    if (route_powered)
      player_flags(&player->obj) |= PLAYER_FLAGS_POWERUP;
    route_in_powerdown = route_powered;
    level_player_die(player);
    route_in_powerdown = false;
  }
  if (route_killed && !route_restarted && !(player_flags2(&player->obj) & 8) &&
      player->lifes_count == 2) {
    route_restarted = true;
    route_x = player->obj.x_pos;
  }
  g_sys.input.direction =
      (!route_killed || route_restarted) ? INPUT_DIRECTION_RIGHT : 0;
  if (route_after_restart >= 20)
    g_sys.input.quit = true;
}
static void regression_death() {
  sound_init();
  g_mix.init();
  g_sys.process_events = route_events;
  g_sys.sleep = route_sleep;
  g_sys.get_timestamp = route_clock;
  g_sys.fade_in_palette = g_sys.fade_out_palette = route_fade;
  const int zooms[] = {0, 1, 4, 8};
  int routes = 0;
  for (int rate = 60; rate <= 120; rate *= 2)
    for (int hud = 0; hud < 2; ++hud)
      for (int z = 0; z < 4; ++z)
        for (int powered = 0; powered < 2; ++powered) {
          family_end();
          g_sys.input = (struct input_t){.view_step = zooms[z], .space = true};
          g_options.camera_mode = CAMERA_SMOOTH;
          g_options.hud_mode = hud;
          g_options.scroll_rate = rate;
          g_sys.set_scroll_render_rate(rate);
          g_vars.level = 1;
          g_vars.player = 0;
          g_vars.players_table[0].lifes_count = 3;
          route_killed = route_restarted = false;
          route_sleeps = route_after_restart = route_draws = 0;
          route_powered = powered;
          do_level();
          assert(route_killed && route_restarted && route_draws > 20 &&
                 route_after_restart >= 20);
          assert(g_vars.players_table[0].obj.x_pos > route_x + 10);
          assert(g_vars.players_table[0].lifes_count == 2);
          ++routes;
        }
  g_mix.fini();
  sound_fini();
  printf("PASS: %d real death/respawn/movement routes, normal and powered "
         "player\n",
         routes);
}
static void regression_native_scroll() {
  sound_init();
  g_mix.init();
  route_scroll = true;
  g_sys.process_events = route_events;
  g_sys.sleep = route_sleep;
  g_sys.get_timestamp = route_clock;
  g_sys.fade_in_palette = g_sys.fade_out_palette = route_fade;
  g_options.cheats = CHEATS_OBJECT_NO_HIT | CHEATS_DECOR_NO_HIT;
  g_options.family_infinite_lives = true;
  for (int rate = 60; rate <= 120; rate *= 2)
    for (int hud = 0; hud < 2; ++hud) {
      family_end();
      g_sys.input = (struct input_t){.space = true};
      g_options.scroll_rate = rate;
      g_options.hud_mode = hud;
      g_sys.set_scroll_render_rate(rate);
      g_vars.level = 1;
      g_vars.player = 0;
      g_vars.players_table[0].lifes_count = 3;
      motion_frames = route_sleeps = 0;
      motion_camera_x = 0;
      motion_player_x = 0;
      do_level();
      assert(motion_frames >= 360);
    }
  g_mix.fini();
  sound_fini();
  puts("PASS: native right/left scrolling with camera direction checks");
}
static void regression_music() {
  sound_init();
  g_mix.init();
  const uint32_t before = checksum(g_res.tmp, 72728);
  _family_rewinding = true;
  g_sys.input.rewind = false;
  assert(!family_timeline_step());
  assert(checksum(g_res.tmp, 72728) == before &&
         "music reload overwrote live level resources");
  g_mix.fini();
  sound_fini();
}

static void regression_scroll_edges() {
  /* Read back before Present; GPU swap-chain backbuffers are undefined after
   * it. */
  _save_preview_capture = true;
  g_options.scroll_rate = 60;
  g_sys.set_scroll_render_rate(60);
  g_sys.input.view_step = 0;
  g_options.hud_mode = HUD_CLASSIC;
  memset(g_vars.tilemap_data, 0, g_vars.tilemap_w * g_vars.tilemap_h);
  memset(g_vars.tilemap_lut_type, 0, sizeof(g_vars.tilemap_lut_type));
  memset(g_vars.tile_palette_table, 0, sizeof(g_vars.tile_palette_table));
  for (int page = 0; page < 6; ++page)
    for (int n = 0; n < 256; ++n)
      g_vars.tilemap_lut_init[page * 256 + n] = n;
  g_vars.tilemap_current_lut = g_vars.tilemap_lut_init;
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 16; x += 2)
      g_res.tmp[y * 8 + x / 2] = (((x % 15) + 1) << 4) | (((x + 1) % 15) + 1);
  uint8_t palette[768];
  for (int n = 0; n < 256; ++n)
    for (int c = 0; c < 3; ++c)
      palette[n * 3 + c] = (n % 16) * 16;
  g_sys.set_screen_palette(palette, 0, 256, 8);
  for (int n = 0; n < OBJECTS_COUNT; ++n)
    g_vars.objects_table[n].spr_num = 0xFFFF;
  g_vars.tilemap_x = 16;
  g_vars.tilemap_scroll_dx = 0;
  g_vars.tilemap_y = 0;
  g_vars.tilemap_scroll_dy = 0;
  family_begin();
  _family_before_draw = g_vars;
  family_present();
  const bool motion = SDL_getenv("PF_CAMERA_MOTION") != NULL;
  for (int scale = 1; scale <= 3; ++scale) {
    SDL_SetWindowSize(_window, 320 * scale, 200 * scale);
    int w, h;
    SDL_GetRendererOutputSize(_renderer, &w, &h);
    assert(w == 320 * scale);
    uint32_t *pixels = malloc((size_t)w * h * 4);
    assert(pixels);
    for (int phase = -24; phase <= 24; ++phase) {
      sdl2_draw_screen(phase / 8.f, 0, 1);
      SDL_RenderSetLogicalSize(_renderer, 0, 0);
      assert(SDL_RenderReadPixels(_renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                                  pixels, w * 4) == 0);
      SDL_RenderSetLogicalSize(_renderer, _screen_w, _screen_h);
      const uint32_t *row = pixels + (80 * scale) * w;
      for (int x = 0; x < 32 * scale; ++x) {
        if (row[x] != row[x + 160 * scale]) {
          fprintf(stderr,
                  "left-edge discontinuity: scale %d offset %.3f pixel %d: "
                  "%08x vs %08x\n",
                  scale, phase / 8.f, x, row[x], row[x + 160 * scale]);
          abort();
        }
      }
    }
    free(pixels);
    if (motion) {
      /* Each output scale is an independent route, rather than a teleport
       * back from the previous route (which intentionally cuts the camera). */
      g_vars.tilemap_x = 16;
      g_vars.tilemap_scroll_dx = 0;
      family_cut();
      _family_before_draw = g_vars;
      family_present();
      for (int frame = 1; frame <= 34; ++frame) {
        const int native =
            frame <= 32 ? 256 + frame * 4 : 508 + (frame - 33) * 4;
        g_vars.tilemap_x = native / 16;
        g_vars.tilemap_scroll_dx = (native % 16) / 4;
        _family_before_draw = g_vars;
        family_present();
        assert(_scroll_interpolation_active);
        uint32_t *moving = malloc((size_t)w * h * 4);
        assert(moving);
        for (int part = 0; part <= 4; ++part) {
          sdl2_present_at(_scroll_update_timestamp +
                          _scroll_period_counter * part / 4);
          SDL_RenderSetLogicalSize(_renderer, 0, 0);
          assert(SDL_RenderReadPixels(_renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                                      moving, w * 4) == 0);
          SDL_RenderSetLogicalSize(_renderer, _screen_w, _screen_h);
          const uint32_t *row = moving + 80 * scale * w;
          for (int x = 0; x < 32 * scale; ++x)
            if (row[x] != row[x + 160 * scale] ||
                row[w - 1 - x] != row[w - 1 - x - 160 * scale]) {
              fprintf(stderr,
                      "missing map edge: scale %d frame %d phase %d margin %d "
                      "camera %.3f raster %d\n",
                      scale, frame, part, _scroll_margin, _family_x,
                      _family_last_x);
              abort();
            }
        }
        free(moving);
        const uint32_t actual = display_hash();
        sdl2_draw_screen(_family_last_x - _family_x, _family_last_y - _family_y,
                         1);
        if (display_hash() != actual) {
          fprintf(stderr,
                  "smooth camera lost precision: native %d smooth %.4f raster "
                  "%d scale %d frame %d\n",
                  family_native_x(), _family_x, _family_last_x, scale, frame);
          abort();
        }
      }
    }
  }
  if (motion) {
    family_cut();
    g_vars.tilemap_x = 16;
    g_vars.tilemap_scroll_dx = 0;
    g_vars.objects_table[0].spr_num = 19;
    g_vars.objects_table[0].x_pos = 500;
    g_vars.objects_table[0].y_pos = 100;
    _family_before_draw = g_vars;
    family_present();
    const double old_camera = _family_x;
    g_vars.objects_table[0].x_pos = 503;
    g_vars.tilemap_scroll_dx = 1;
    _family_before_draw = g_vars;
    family_present();
    const struct sprite_t *actor = NULL;
    for (int n = 0; n < _sprites_count; ++n)
      if (_sprites[n].owner == 0)
        actor = &_sprites[n];
    assert(actor && actor->interpolate);
    const double half = _spritesheets[actor->sheet].r[actor->num].w / 2.;
    const double old_anchor =
        500 - (sprite_sizes[19] & 255) + half - old_camera;
    const double new_anchor = 503 - (sprite_sizes[19] & 255) + half - _family_x;
    for (int part = 0; part <= 4; ++part) {
      const float t = part / 4.f;
      SDL_FRect r = sdl2_sprite_rect(actor, 0, 0, t);
      assert(fabs(r.x + half - (old_anchor + (new_anchor - old_anchor) * t)) <
             .001);
    }
  }
  _save_preview_capture = false;
}
static void regression_backdrop() {
  _save_preview_capture = true;
  /* Transparent tiles reveal a striped original backdrop. It is screen
   * anchored: camera interpolation must never move it between logic ticks. */
  memset(g_res.tmp, 0, 32768);
  for (int y = 0; y < 200; ++y)
    for (int x = 0; x < 320; ++x)
      g_res.background[y * 320 + x] = (x % 15) + 1;
  for (int i = 0; i < OBJECTS_COUNT; ++i)
    g_vars.objects_table[i].spr_num = 0xFFFF;
  _family_before_draw = g_vars;
  family_present();
  sdl2_draw_screen(0, 0, 1);
  const uint32_t reference = display_hash();
  for (int offset = -7; offset <= 7; ++offset) {
    sdl2_draw_screen(offset, 3, .5f);
    assert(display_hash() == reference &&
           "backdrop oscillates with tile interpolation");
  }
  _save_preview_capture = false;
}
#else
#include "pre2_scroll_regressions.h"
#endif
int main(int argc, char **argv) {
  assert(argc >= 2);
  g_options.screen_w = 320;
  g_options.screen_h = 200;
  g_options.scroll_rate = 60;
  g_options.music_volume = g_options.effects_volume = 100;
  g_options.initial_lives[0] = g_options.initial_lives[1] = 3;
  g_options.camera_mode = CAMERA_SMOOTH;
  g_sys.init();
  g_sys.set_screen_size(320, 200, "Family presentation probe", 2, NULL, false);
  g_sys.get_timestamp = frozen;
  g_options.dos_datapath=argv[1];
  res_init(argv[1], 64000);
#ifdef PROBE_JA
  g_vars.level = argc > 2 ? atoi(argv[2]) : 1;
  g_vars.player = 0;
  g_sys.set_screen_palette(common_palette_data, 0, 128, 6);
  video_load_sprites();
  init_level(get_level());
  g_vars.players_table[0].lifes_count = 3;
  level_draw_tilemap();
  draw_panel();
  level_reset_palette();
  level_draw_objects();
  const uint8_t *map = g_res.tmp;
  size_t map_size = 72728;
#else
  video_init();
  video_convert_tiles(g_res.uniondat, g_res.unionlen);
  g_vars.level_num = argc > 2 ? atoi(argv[2]) : 0;
  g_vars.player_lifes = 3;
  load_level_data(g_vars.level_num);
  set_level_palette();
  video_load_sprites();
  video_load_front_tiles();
  video_clear();
  level_reset();
  level_reset_objects();
  level_init_tilemap();
  level_clear_panel();
  level_update_tilemap();
  level_draw_panel();
  level_draw_objects();
  level_draw_front_tiles();
  const uint8_t *map = g_res.leveldat;
  size_t map_size = g_res.levellen;
#endif
  g_vars.timestamp = 1234;
  family_begin();
  _family_before_draw = g_vars;
  if(SDL_getenv("PF_SAVE_TEST")){
    regression_family_save(SDL_getenv("PF_SAVE_TEST"));family_end();res_fini();g_sys.fini();puts("PASS: family persistent save");return 0;
  }
#ifdef PROBE_JA
  const char *regression = SDL_getenv("PF_REGRESSION");
  if (regression) {
    if (!strcmp(regression, "camera")) {
      SDL_setenv("PF_CAMERA_MOTION", "1", 1);
      regression_scroll_edges();
    } else if (!strcmp(regression, "motion"))
      regression_native_scroll();
    else if (!strcmp(regression, "scroll"))
      regression_scroll_edges();
    else if (!strcmp(regression, "death"))
      regression_death();
    else if (!strcmp(regression, "music"))
      regression_music();
    else if (!strcmp(regression, "sprites"))
      regression_sprites();
    else if (!strcmp(regression, "resize"))
      regression_resize();
    else if (!strcmp(regression, "backdrop"))
      regression_backdrop();
    else
      abort();
    family_end();
    res_fini();
    g_sys.fini();
    puts("PASS: Jukebox lifecycle regression");
    return 0;
  }
#else
  const char *regression=SDL_getenv("PF_REGRESSION");
  if(regression){p2_scroll_regressions(regression);family_end();res_fini();g_sys.fini();puts("PASS: Prehistorik 2 scroll regression");return 0;}
#endif
  const struct vars_t before = g_vars;
  const uint32_t hash = checksum(map, map_size);
  for (int rate = 30; rate <= 120; rate *= 2) {
    g_options.scroll_rate = rate;
    g_sys.set_scroll_render_rate(rate);
    for (int hud = 0; hud < 2; ++hud) {
      g_options.hud_mode = hud;
      for (int step = 0; step <= 8; ++step) {
        g_sys.input.view_step = step;
        for (int frame = 0; frame < 24; ++frame) {
          family_present();
          assert(!memcmp(&g_vars, &before, sizeof(before)));
          assert(checksum(map, map_size) == hash);
          assert(GAME_SCREEN_W == 320 && GAME_SCREEN_H == 200);
        }
        assert(_family_w == MIN(320 + step * 64, MAX(320, family_map_w())));
        assert(_family_h ==
               MIN(200 + step * 32, MAX(200, family_map_h() + 24)));
      }
    }
  }
  if (argc > 3) {
    int w, h;
    SDL_GetRendererOutputSize(_renderer, &w, &h);
    SDL_Surface *shot =
        SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    assert(shot);
    SDL_RenderSetLogicalSize(_renderer, 0, 0);
    assert(SDL_RenderReadPixels(_renderer, NULL, shot->format->format,
                                shot->pixels, shot->pitch) == 0);
    assert(SDL_SaveBMP(shot, argv[3]) == 0);
    SDL_FreeSurface(shot);
    SDL_RenderSetLogicalSize(_renderer, _screen_w, _screen_h);
  }
  /* A multi-page selector must expose the actual final page, independently
   * of the original six-stage Blues menu. */
  struct sys_pause_options menu = {0};
  menu.level_count = FAMILY_LEVEL_COUNT;
  menu.level_names = family_level_names;
  menu.current_level = FAMILY_LEVEL_COUNT - 2;
  for (int i = 0; i < FAMILY_LEVEL_COUNT; ++i)
    menu.available_levels[0][i] = family_level_available(i);
  sdl2_set_pause_options(&menu);
  _pause_page = PAUSE_LEVELS;
  _pause_selection = FAMILY_LEVEL_COUNT - 2;
  assert(pause_item_count() == FAMILY_LEVEL_COUNT + 1);
  pause_draw_settings();
  assert(!strcmp(pause_level_name(FAMILY_LEVEL_COUNT - 2),
                 family_level_names[FAMILY_LEVEL_COUNT - 2]));
  pause_activate();
  assert(_pause_page == PAUSE_LEVEL_CONFIRM);
  _pause_selection = 1;
  assert(pause_activate());
  assert(_pause_options.requested_level == FAMILY_LEVEL_COUNT - 2);
  /* Restore full mutable state; future snapshots are discarded. */
  family_timeline_clear();
  family_timeline_record();

  g_vars.timestamp = 999;
#ifdef PROBE_JA
  g_vars.players_table[0].obj.x_pos += 30;
  g_res.tmp[0] ^= 1;
#else
  g_vars.objects_tbl[1].x_pos += 30;
  g_res.leveldat[0] ^= 1;
  g_res.level.boss_energy -= 5;
#endif
  family_timeline_record();
  g_sys.input.rewind = true;
  assert(family_timeline_step());
  assert(_family_count == 1 && !memcmp(&g_vars, &before, sizeof(before)));
  assert(checksum(map, map_size) == hash);
  g_sys.input.rewind = false;
  /* No music subsystem is started by this resource-only test. */
  _family_rewinding = false;
  g_sys.set_rewind(false, 0);
  family_end();
  res_fini();
  g_sys.fini();
  puts("PASS: 30/60/120 Hz, both HUDs, all zoom steps preserve native state "
       "and map; selector and full-state rewind");
  return 0;
}
