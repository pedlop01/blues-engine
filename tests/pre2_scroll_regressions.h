/* P2-specific pixel/raster and native-loop checks. Included only by its probe.
 */
static uint32_t p2_display_hash() {
  int w, h;
  SDL_GetRendererOutputSize(_renderer, &w, &h);
  uint8_t *pixels = calloc((size_t)w * h, 4);
  assert(pixels);
  SDL_RenderSetLogicalSize(_renderer, 0, 0);
  assert(SDL_RenderReadPixels(_renderer, NULL, SDL_PIXELFORMAT_ARGB8888, pixels,
                              w * 4) == 0);
  uint32_t value = checksum(pixels, (size_t)w * h * 4);
  free(pixels);
  SDL_RenderSetLogicalSize(_renderer, _screen_w, _screen_h);
  return value;
}
static void p2_clip_regression() {
  uint8_t tile[128];
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 16; x += 2)
      tile[y * 8 + x / 2] = (((x + y) % 16) << 4) | ((x + y + 1) % 16);
  const int ys[] = {-17, -15, -1, 0, 1, 159, 169, 175, 176, 200};
  int cases = 0;
  for (int edge = 0; edge < 2; ++edge)
    for (int dx = -17; dx <= 17; ++dx)
      for (int yi = 0; yi < 10; ++yi) {
        const int xo = (edge ? 304 : 0) + dx, yo = ys[yi];
        memset(g_res.vga, 231, 64000);
        video_draw_tile(tile, xo, yo);
        for (int y = 0; y < 200; ++y)
          for (int x = 0; x < 320; ++x) {
            uint8_t expected = 231;
            if (y < 176 && x >= xo && x < xo + 16 && y >= yo && y < yo + 16) {
              uint8_t b = tile[(y - yo) * 8 + (x - xo) / 2],
                      color = ((x - xo) & 1) ? b & 15 : b >> 4;
              if (color)
                expected = color;
            }
            if (g_res.vga[y * 320 + x] != expected) {
              fprintf(stderr,
                      "P2 tile clip mismatch: offset %d,%d pixel %d,%d got %u "
                      "expected %u\n",
                      xo, yo, x, y, g_res.vga[y * 320 + x], expected);
              abort();
            }
          }
        ++cases;
      }
  printf("PASS: %d packed-tile clipping placements, odd/even edges and "
         "transparency\n",
         cases);
}
static void p2_synthetic_scene(bool transparent) {
  g_sys.input.view_step = 0;
  g_options.hud_mode = HUD_CLASSIC;
  g_options.scroll_rate = 60;
  g_sys.set_scroll_render_rate(60);
  memset(g_res.leveldat, 0, g_vars.tilemap.size + 512);
  memset(g_res.level.tile_attributes2, 0, sizeof(g_res.level.tile_attributes2));
  for (int n = 0; n < 256; ++n)
    g_vars.tile_tbl1[n] = g_vars.tile_tbl2[n] = g_vars.tile_tbl3[n] = n;
  g_vars.level_animated_tiles_current_tbl = g_vars.tile_tbl1;
  uint8_t *tile = g_res.leveldat + g_vars.tilemap.size + 512;
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 16; x += 2)
      tile[y * 8 + x / 2] =
          transparent ? 0 : ((((x % 15) + 1) << 4) | (((x + 1) % 15) + 1));
  uint8_t palette[768];
  for (int n = 0; n < 256; ++n)
    for (int c = 0; c < 3; ++c)
      palette[n * 3 + c] = (n % 16) * 16;
  g_sys.set_screen_palette(palette, 0, 256, 8);
  for (int y = 0; y < 200; ++y)
    for (int x = 0; x < 320; ++x)
      g_res.background[y * 320 + x] = (x % 15) + 1;
  for (int n = 0; n < OBJECTS_COUNT; ++n)
    g_vars.objects_tbl[n].spr_num = 0xFFFF;
  g_vars.tilemap.x = 16;
  g_vars.tilemap.scroll_dx = 0;
  g_vars.tilemap.y = 0;
  g_vars.tilemap.scroll_dy = 0;
  family_begin();
  _family_before_draw = g_vars;
}
static void p2_backdrop_regression() {
  _save_preview_capture = true;
  p2_synthetic_scene(true);
  family_present();
  sdl2_draw_screen(0, 0, 1);
  uint32_t reference = p2_display_hash();
  for (int dx = -7; dx <= 7; ++dx) {
    sdl2_draw_screen(dx, 3, .5f);
    assert(p2_display_hash() == reference &&
           "P2 backdrop moves with tile interpolation");
  }
  _save_preview_capture = false;
}
static void p2_foreground_regression() {
  p2_synthetic_scene(true);
  g_res.level.tile_attributes2[0] = 0x40;
  g_res.level.front_tiles_lut[0] = 0;
  for (int frame = 0; frame < 24; ++frame) {
    int native = 256 + frame * 4;
    g_vars.tilemap.x = native / 16;
    g_vars.tilemap.scroll_dx = native % 16;
    _family_before_draw = g_vars;
    family_present();
    if (frame) {
      int front = 0;
      for (int n = 0; n < _sprites_count; ++n)
        if (_sprites[n].sheet == RENDER_SPR_FG) {
          assert(!_sprites[n].interpolate &&
                 "foreground grid tile interpolates as an actor");
          ++front;
        }
      assert(front > 0);
    }
  }
}
static void p2_camera_regression() {
  _save_preview_capture = true;
  p2_synthetic_scene(false);
  for (int scale = 1; scale <= 3; ++scale) {
    SDL_SetWindowSize(_window, 320 * scale, 200 * scale);
    int w, h;
    SDL_GetRendererOutputSize(_renderer, &w, &h);
    assert(w == 320 * scale);
    g_vars.tilemap.x = 16;
    g_vars.tilemap.scroll_dx = 0;
    family_cut();
    _family_before_draw = g_vars;
    family_present();
    uint32_t *pixels = malloc((size_t)w * h * 4);
    assert(pixels);
    for (int frame = 1; frame <= 34; ++frame) {
      const int native = frame <= 32 ? 256 + frame : 412 + (frame - 33);
      g_vars.tilemap.x = native / 16;
      g_vars.tilemap.scroll_dx = native % 16;
      _family_before_draw = g_vars;
      family_present();
      assert(_scroll_interpolation_active);
      sdl2_present_at(_scroll_update_timestamp + _scroll_period_counter);
      uint32_t actual = p2_display_hash();
      sdl2_draw_screen(_family_last_x - _family_x, _family_last_y - _family_y,
                       1);
      if (p2_display_hash() != actual) {
        fprintf(stderr,
                "P2 fractional camera lost: native %d camera %.4f raster %d "
                "scale %d frame %d\n",
                native, _family_x, _family_last_x, scale, frame);
        abort();
      }
      for (int part = 0; part <= 4; ++part) {
        sdl2_present_at(_scroll_update_timestamp +
                        _scroll_period_counter * part / 4);
        SDL_RenderSetLogicalSize(_renderer, 0, 0);
        assert(SDL_RenderReadPixels(_renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                                    pixels, w * 4) == 0);
        SDL_RenderSetLogicalSize(_renderer, _screen_w, _screen_h);
        const uint32_t *row = pixels + 80 * scale * w;
        for (int x = 0; x < 32 * scale; ++x)
          if (row[x] != row[x + 160 * scale] ||
              row[w - 1 - x] != row[w - 1 - x - 160 * scale]) {
            fprintf(
                stderr,
                "P2 interrupted edge: scale %d frame %d phase %d margin %d\n",
                scale, frame, part, _scroll_margin);
            abort();
          }
      }
    }
    free(pixels);
  }
  _save_preview_capture = false;
}
static uint32_t p2_route_time;
static int p2_route_frames, p2_route_calls;
static uint32_t p2_route_clock() { return p2_route_time; }
static void p2_route_fade() {}
static void p2_route_transition(enum sys_transition_e type, bool open) {
  (void)type;
  (void)open;
}
static void p2_route_events() {
  sdl2_process_events();
  g_sys.input.space = !_family_active;
  if (!_family_active)
    return;
  g_sys.input.direction =
      p2_route_frames < 180 ? INPUT_DIRECTION_RIGHT : INPUT_DIRECTION_LEFT;
  g_sys.input.jump = p2_route_frames % 60 < 8;
  if (p2_route_frames >= 360)
    g_sys.input.quit = true;
}
static void p2_route_sleep(int duration) {
  p2_route_time += MAX(1, duration);
  assert(++p2_route_calls < 2500);
  if (!_family_active || !_family_camera_valid)
    return;
  assert(_screen_w == _family_w && _screen_h == _family_h);
  int count = _sprites_count;
  for (int part = 1; part <= 4; ++part) {
    if (_scroll_interpolation_active)
      sdl2_present_at(_scroll_update_timestamp +
                      _scroll_period_counter * part / 4);
    else
      sdl2_draw_screen((float)_scroll_camera_offset_x,
                       (float)_scroll_camera_offset_y, 1);
    assert(_sprites_count == count);
  }
  const char *capture = SDL_getenv("PF_CAPTURE");
  if (capture && p2_route_frames == 90 && g_options.scroll_rate == 120 &&
      g_options.hud_mode == HUD_FIXED) {
    _save_preview_capture = true;
    if (_scroll_interpolation_active)
      sdl2_present_at(_scroll_update_timestamp + _scroll_period_counter);
    else
      sdl2_draw_screen((float)_scroll_camera_offset_x,
                       (float)_scroll_camera_offset_y, 1);
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
    _save_preview_capture = false;
    SDL_RenderPresent(_renderer);
  }
  ++p2_route_frames;
}
static void p2_motion_regression() {
  sound_init();
  g_mix.init();
  g_sys.process_events = p2_route_events;
  g_sys.sleep = p2_route_sleep;
  g_sys.get_timestamp = p2_route_clock;
  g_sys.fade_in_palette = g_sys.fade_out_palette = p2_route_fade;
  g_sys.transition_screen = p2_route_transition;
  g_options.cheats =
      CHEATS_NO_HIT | CHEATS_UNLIMITED_ENERGY | CHEATS_UNLIMITED_LIFES;
  g_options.family_infinite_lives = true;
  for (int rate = 60; rate <= 120; rate *= 2)
    for (int hud = 0; hud < 2; ++hud) {
      family_end();
      g_sys.input = (struct input_t){.space = true};
      g_options.hud_mode = hud;
      g_options.scroll_rate = rate;
      g_sys.set_scroll_render_rate(rate);
      g_vars.level_num = 0;
      g_vars.player_lifes = 3;
      p2_route_frames = p2_route_calls = 0;
      do_level();
      assert(p2_route_frames >= 360);
    }
  g_mix.fini();
  sound_fini();
  puts("PASS: four native P2 right/left/jump routes at 60/120 Hz");
}
static void p2_timeline_regression() {
  sound_init();
  g_mix.init();
  family_timeline_clear();
  family_present();
  family_timeline_record();
  const struct vars_t before = g_vars;
  const uint32_t map_hash = checksum(g_res.leveldat, g_res.levellen);
  g_vars.objects_tbl[1].x_pos += 30;
  g_res.leveldat[0] ^= 1;
  family_timeline_record();
  g_sys.input.rewind = true;
  assert(family_timeline_step());
  assert(!memcmp(&g_vars, &before, sizeof(before)));
  assert(checksum(g_res.leveldat, g_res.levellen) == map_hash);
  g_sys.input.rewind = false;
  assert(!family_timeline_step());
  assert(checksum(g_res.leveldat, g_res.levellen) == map_hash);
  g_mix.fini();
  sound_fini();
}
static void p2_scroll_regressions(const char *name) {
  if (!strcmp(name, "clip"))
    p2_clip_regression();
  else if (!strcmp(name, "backdrop"))
    p2_backdrop_regression();
  else if (!strcmp(name, "camera"))
    p2_camera_regression();
  else if (!strcmp(name, "foreground"))
    p2_foreground_regression();
  else if (!strcmp(name, "motion"))
    p2_motion_regression();
  else if (!strcmp(name, "timeline"))
    p2_timeline_regression();
  else
    abort();
}
