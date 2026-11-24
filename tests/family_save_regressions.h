/* Invoked in independent writer/reader processes, including across platforms.
 */
static void regression_family_save(const char *mode) {
  sound_init();
  g_mix.init();
  family_present();
  struct sys_save_slot info;
  if (!strcmp(mode, "write")) {
    g_sys.input.view_step = 2;
    g_options.hud_mode = HUD_FIXED;
    g_options.cheats = 1;
    g_options.family_infinite_lives = true;
#if FAMILY_SAVE_GAME == 1
    g_vars.players_table[0].vinyls_count = 17;
    get_random_number();
#else
    g_vars.score = 123456;
    g_vars.random.e = 54321;
#endif
    _family_before_draw = g_vars;
    family_present();
    if (!family_save_game(0)) {
      fprintf(stderr, "save failed: %s\n", family_save_error());
      abort();
    }
    family_inspect_save(0, &info);
    assert(info.status == SAVE_READY && info.has_preview);
  } else if (!strcmp(mode, "read")) {
    assert(family_native_prepare(0) == family_level());
    assert(family_apply_pending());
#if FAMILY_SAVE_GAME == 1
    assert(g_vars.players_table[0].vinyls_count == 17 &&
           g_vars.random_initialized);
#else
    assert(g_vars.score == 123456 && g_vars.random.e == 54321);
#endif
    assert(g_options.hud_mode == HUD_FIXED && g_sys.input.view_step == 2 &&
           g_options.family_infinite_lives);
    assert(family_save_game(1));
    uint8_t *a = malloc(FAMILY_SAVE_MAX), *b = malloc(FAMILY_SAVE_MAX);
    size_t na, nb;
    assert(g_sys.read_save_file(0, a, FAMILY_SAVE_MAX, &na) &&
           g_sys.read_save_file(1, b, FAMILY_SAVE_MAX, &nb));
    assert(na == nb);
    assert(!memcmp(a + FAMILY_SAVE_HEADER, b + FAMILY_SAVE_HEADER,
                   na - FAMILY_SAVE_HEADER - SAVE_PREVIEW_BYTES));
    free(a);
    free(b);
  } else if (!strcmp(mode, "reject")) {
    struct vars_t before = g_vars;
    if (family_native_prepare(0) >= 0)
      assert(!family_apply_pending());
    assert(!memcmp(&before, &g_vars, sizeof(g_vars)));
  } else
    assert(!"Unknown family save test");
  g_mix.fini();
  sound_fini();
}
