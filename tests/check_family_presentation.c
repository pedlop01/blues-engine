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
struct options_t g_options;
static uint32_t frozen() { return 1234; }
static uint32_t checksum(const uint8_t *p, size_t n) {
  uint32_t h = 2166136261u;
  while (n--)
    h = (h ^ *p++) * 16777619u;
  return h;
}
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
