/* Shared host for Jukebox/P2. The simulation ALWAYS uses its native 320x200
 * viewport. Rendering runs on a disposable vars copy with a separate camera;
 * visible bounds therefore cannot change collisions, AI or progression. */
#include "mixer.h"
#include <math.h>
static bool _family_active, _family_camera_valid;
static int _family_w = 320, _family_h = 200, _family_last_x, _family_last_y;
static int _family_native_x, _family_native_y;
#ifdef FAMILY_FRACTIONAL_CAMERA
static int _family_scroll_margin = 32;
#endif
static double _family_width = 320, _family_height = 200, _family_x, _family_y;
static uint8_t *_family_pixels, *_family_border;
#ifdef FAMILY_SEPARATE_BACKDROP
static uint8_t *_family_backdrop;
#endif
static size_t _family_capacity;
static struct vars_t _family_before_draw;
static int _family_requested_level = -1;
static bool _family_character;
static void family_present();
static void family_cut();
#include "timeline.h"
static void family_apply_volumes(int music, int effects) {
  g_options.music_volume = music;
  g_options.effects_volume = effects;
  g_mix.set_volumes(music, effects);
}
static void family_cut() {
  _family_camera_valid = false;
  g_sys.set_scroll_interpolation(0, 0, 0, false);
}
static void family_begin() {
  _family_active = true;
  /* Reloads return SDL to 320x200: never trust the last game view cache. */
  _family_w = _family_h = 0;
#ifdef FAMILY_FRACTIONAL_CAMERA
  _family_scroll_margin = 32;
#endif
  _family_character = false;
  _family_requested_level = -1;
  _family_width = MIN(320 + MIN(8, MAX(0, g_sys.input.view_step)) * 64,
                      MAX(320, family_map_w()));
  _family_height = MIN(200 + MIN(8, MAX(0, g_sys.input.view_step)) * 32,
                       MAX(200, family_map_h() + 24));
  _family_before_draw = g_vars;
  family_timeline_init();
  family_timeline_record();
  family_cut();
  g_sys.set_pause_menu_enabled(true);
}
static void family_end() {
  _family_active = false;
  g_sys.set_pause_menu_enabled(false);
  if (_family_rewinding) {
    g_mix.set_volumes(g_options.music_volume, g_options.effects_volume);
    g_sys.set_rewind(false, 0);
  }
  _family_rewinding = false;
  free(_family_history);
  _family_history = NULL;
  family_timeline_clear();
  family_cut();
  g_sys.set_scroll_background(NULL, 0);
  g_sys.set_hud_layout(0, 0, 0, 0);
  g_sys.resize_viewport(320, 200);
  _family_w = 320;
  _family_h = 200;
  g_sys.render_set_sprites_clipping_rect(0, 0, 320, 176);
  g_sys.render_clear_sprites();
  free(_family_pixels);
  _family_pixels = NULL;
  _family_capacity = 0;
#ifdef FAMILY_SEPARATE_BACKDROP
  free(_family_backdrop);
  _family_backdrop = NULL;
#endif
  free(_family_border);
  _family_border = NULL;
}
static bool family_before_tick() {
  g_sys.process_events();
  if (g_sys.input.menu) {
    struct sys_pause_options options = {0};
    options.camera = g_options.camera_mode;
    options.hud = g_options.hud_mode;
    options.render_rate = g_options.scroll_rate;
    options.view_step = g_sys.input.view_step;
    options.music_volume = g_options.music_volume;
    options.effects_volume = g_options.effects_volume;
    options.apply_volumes = family_apply_volumes;
    options.invulnerable = family_invulnerable();
    options.infinite_lives = g_options.family_infinite_lives;
    const int lives_before = family_lives(0);
    options.lives[0] = lives_before;
    options.lives[1] = family_lives(1);
    options.player_alive[0] = true;
    options.player_alive[1] = false;
    options.current_level = family_level();
    options.requested_level = -1;
    options.level_count = FAMILY_LEVEL_COUNT;
    options.level_names = family_level_names;
    options.no_character_select = FAMILY_NO_CHARACTER;
    for (int i = 0; i < FAMILY_LEVEL_COUNT; ++i)
      options.available_levels[0][i] = family_level_available(i);
    g_sys.set_pause_options(&options);
    const uint32_t paused_at = g_sys.get_timestamp();
    g_sys.run_pause_menu();
    family_pause_elapsed(g_sys.get_timestamp() - paused_at);
    g_sys.get_pause_options(&options);
    g_options.camera_mode = options.camera;
    g_options.hud_mode = options.hud;
    g_options.scroll_rate = options.render_rate;
    g_sys.set_scroll_render_rate(options.render_rate);
    g_sys.input.view_step = options.view_step;
    g_options.family_infinite_lives = options.infinite_lives;
    family_set_assists(options.invulnerable, options.infinite_lives);
    family_set_lives(0, options.lives[0]);
    if (options.reset_defaults)
      g_options.initial_lives[0] = g_options.initial_lives[1] = 3;
    else if (options.lives[0] != lives_before)
      g_options.initial_lives[0] = options.lives[0];
    _family_requested_level = options.requested_level;
    _family_character = options.select_character;
    family_cut();
    if (g_sys.save_preferences)
      g_sys.save_preferences();
  }
  g_sys.set_assist_status(family_invulnerable(),
                          g_options.family_infinite_lives);
  if (!g_sys.input.quit && !_family_character && _family_requested_level < 0 &&
      family_timeline_step())
    return false;
  const bool advance =
      !g_sys.input.quit && !_family_character && _family_requested_level < 0;
  if (advance)
    g_sys.render_clear_sprites();
  return advance;
}
static void family_present() {
#ifdef FAMILY_FRACTIONAL_CAMERA
  const double previous_camera_x = _family_x, previous_camera_y = _family_y;
#endif
  const int view = MIN(8, MAX(0, g_sys.input.view_step));
  const int target_w = MIN(320 + view * 64, MAX(320, family_map_w()));
  const int target_h = MIN(200 + view * 32, MAX(200, family_map_h() + 24));
  if (!_family_rewinding) {
    _family_width += (target_w - _family_width) * .25;
    _family_height += (target_h - _family_height) * .25;
    if (fabs(_family_width - target_w) < 1)
      _family_width = target_w;
    if (fabs(_family_height - target_h) < 1)
      _family_height = target_h;
  }
  const int w = ((int)lrint(_family_width) + 1) & ~1,
            h = ((int)lrint(_family_height) + 1) & ~1;
  bool cut = !_family_camera_valid;
  const int nx = family_native_x(), ny = family_native_y();
  if (abs(nx - _family_native_x) > 128 || abs(ny - _family_native_y) > 96)
    cut = true;
  _family_native_x = nx;
  _family_native_y = ny;
  const double target_x =
      MAX(0, MIN(MAX(0, family_map_w() - w), nx - (w - 320) / 2));
  const double target_y =
      MAX(0, MIN(MAX(0, family_map_h() - (h - 24)), ny - (h - 200) / 2));
  if (!_family_rewinding || !_family_camera_valid) {
    if (cut || g_options.camera_mode == CAMERA_CLASSIC) {
      _family_x = target_x;
      _family_y = target_y;
    } else {
      _family_x += (target_x - _family_x) * .3;
      _family_y += (target_y - _family_y) * .3;
    }
  }
  const int x = family_quantize_x((int)lrint(_family_x)),
            y = (int)lrint(_family_y);
  if (w != _family_w || h != _family_h) {
    if (!g_sys.resize_viewport(w, h))
      return;
    _family_w = w;
    _family_h = h;
    cut = true;
  }
  const size_t size = (size_t)w * h;
  if (size > _family_capacity) {
    uint8_t *pixels = realloc(_family_pixels, size);
    if (!pixels)
      print_error("Unable to allocate family viewport");
    _family_pixels = pixels;
    _family_capacity = size;
  }
  g_sys.render_set_sprites_clipping_rect(0, 0, w, h - 24);
  g_sys.set_hud_layout(g_options.hud_mode == HUD_FIXED ? 320 : 0, 200, 0, 24);
#ifdef FAMILY_FRACTIONAL_CAMERA
  if (g_sys.set_scroll_camera)
    g_sys.set_scroll_camera(x, y, _family_x, _family_y, 33,
                            !cut && g_options.camera_mode == CAMERA_SMOOTH);
  else
#endif
    g_sys.set_scroll_interpolation(x - _family_last_x, y - _family_last_y, 33,
                                   !cut &&
                                       g_options.camera_mode == CAMERA_SMOOTH);
  _family_last_x = x;
  _family_last_y = y;
  _family_camera_valid = true;
  g_sys.render_clear_sprites();
  family_draw_view(x, y, w, h, _family_pixels);
  g_sys.copy_bitmap(_family_pixels, w, h);
  int margin = 32;
#ifdef FAMILY_FRACTIONAL_CAMERA
  if (!cut && g_options.camera_mode == CAMERA_SMOOTH &&
      g_options.scroll_rate > 30) {
    /* Cover the entire interpolation interval, including the raster's
     * fractional displacement. Retain the largest margin until reload. */
    double required =
        fmax(fabs(x - previous_camera_x), fabs(y - previous_camera_y));
    required = fmax(required, fmax(fabs(x - _family_x), fabs(y - _family_y)));
    const int needed = ((int)ceil(required) + 2 + 15) / 16 * 16;
    _family_scroll_margin = MAX(_family_scroll_margin, needed);
  }
  margin = _family_scroll_margin;
#endif
  const int pitch = w + 2 * margin;
  uint8_t *border =
      realloc(_family_border, (size_t)pitch * (h - 24 + 2 * margin));
  if (border) {
    _family_border = border;
#ifdef FAMILY_SEPARATE_BACKDROP
    if (g_sys.set_scroll_layers) {
      uint8_t *backdrop = realloc(_family_backdrop, (size_t)w * (h - 24));
      if (!backdrop)
        print_error("Unable to allocate stationary backdrop");
      _family_backdrop = backdrop;
      for (int by = 0; by < h - 24; ++by)
        for (int bx = 0; bx < w; ++bx)
          backdrop[by * w + bx] = family_backdrop_pixel(bx, by);
      for (int by = -margin; by < h - 24 + margin; ++by)
        for (int bx = -margin; bx < w + margin; ++bx)
          border[(by + margin) * pitch + bx + margin] =
              family_map_layer_pixel(x + bx, y + by);
      g_sys.set_scroll_layers(border, backdrop, margin);
    } else
#endif
    {
      for (int by = -margin; by < h - 24 + margin; ++by)
        for (int bx = -margin; bx < w + margin; ++bx)
          border[(by + margin) * pitch + bx + margin] =
              (by >= 0 && by < h - 24 && bx >= 0 && bx < w)
                  ? _family_pixels[by * w + bx]
                  : family_map_pixel(x + bx, y + by, bx, by);
      g_sys.set_scroll_background(border, margin);
    }
  }
  g_sys.update_screen();
  g_sys.begin_sprite_frame();
}
