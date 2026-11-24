/* Process-local snapshots. No pointers are written to disk. Cleared before
 * any asset reload; a forward tick after rewind replaces the old future. */
#define FAMILY_HISTORY 241
struct family_presentation_state {
  double x, y, width, height;
  int last_x, last_y, native_x, native_y, view, camera, hud, cheats;
  bool infinite, valid;
};
static struct family_presentation_state _family_views[FAMILY_HISTORY];
static uint8_t *_family_history;
static int _family_head, _family_count;
static bool _family_rewinding;
static void family_timeline_clear() { _family_head = _family_count = 0; }
static void family_timeline_init() {
  free(_family_history);
  _family_history = calloc(FAMILY_HISTORY, family_state_size());
  family_timeline_clear();
  _family_rewinding = false;
}
static void family_timeline_record() {
  if (!_family_history)
    return;
  _family_views[_family_head] =
      (struct family_presentation_state){_family_x,
                                         _family_y,
                                         _family_width,
                                         _family_height,
                                         _family_last_x,
                                         _family_last_y,
                                         _family_native_x,
                                         _family_native_y,
                                         g_sys.input.view_step,
                                         g_options.camera_mode,
                                         g_options.hud_mode,
                                         g_options.cheats,
                                         g_options.family_infinite_lives,
                                         _family_camera_valid};
  family_capture(_family_history + (size_t)_family_head * family_state_size());
  _family_head = (_family_head + 1) % FAMILY_HISTORY;
  if (_family_count < FAMILY_HISTORY)
    ++_family_count;
}
static bool family_timeline_step() {
  if (!g_sys.input.rewind) {
    if (_family_rewinding) {
      _family_rewinding = false;
      g_sys.set_rewind(false, 0);
      g_mix.play_sound(NULL, 0, 0, NULL);
      family_resume_music();
      g_mix.set_volumes(g_options.music_volume, g_options.effects_volume);
      family_cut();
    }
    return false;
  }
  _family_rewinding = true;
  g_mix.set_volumes(0, 0);
  if (_family_count > 1) {
    _family_head = (_family_head + FAMILY_HISTORY - 1) % FAMILY_HISTORY;
    --_family_count;
    const int index = (_family_head + FAMILY_HISTORY - 1) % FAMILY_HISTORY;
    family_restore(_family_history + (size_t)index * family_state_size());
    const struct family_presentation_state *v = &_family_views[index];
    _family_x = v->x;
    _family_y = v->y;
    _family_width = v->width;
    _family_height = v->height;
    _family_last_x = v->last_x;
    _family_last_y = v->last_y;
    _family_native_x = v->native_x;
    _family_native_y = v->native_y;
    g_sys.input.view_step = v->view;
    g_options.camera_mode = v->camera;
    g_options.hud_mode = v->hud;
    g_options.cheats = v->cheats;
    g_options.family_infinite_lives = v->infinite;
    _family_camera_valid = v->valid;
  }
  g_sys.set_rewind(true, MAX(0, _family_count - 1));
  g_sys.set_scroll_interpolation(0, 0, 0, false);
  family_present();
  g_sys.sleep(33);
  return true;
}
