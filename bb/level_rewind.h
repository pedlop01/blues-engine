/* Private, bounded timeline. No disk I/O, pointers to freed assets or audio
 * callbacks are stored. A new forward frame replaces the abandoned future. */
#define REWIND_CAPACITY (8*30+1)
static struct bb_state *_rewind_history;
static int _rewind_head, _rewind_count;
static bool _rewind_running;
static void rewind_clear() { _rewind_head=_rewind_count=0; }
static void rewind_init() {
	free(_rewind_history);
	_rewind_history=calloc(REWIND_CAPACITY,sizeof(*_rewind_history));
	rewind_clear();_rewind_running=false;
	if(!_rewind_history) print_warning("Rewind history unavailable (out of memory)");
}
static bool rewind_same_context(const struct bb_state *s) {
	return s->resources==_loaded_resources_signature && s->amiga==g_res.amiga_data &&
		s->vars.level==g_vars.level && s->camera_mode==g_options.camera_mode &&
		s->hud_mode==g_options.hud_mode && s->cheats==g_options.cheats;
}
static void rewind_record() {
	if(!_rewind_history)return;
	if(_rewind_count && !rewind_same_context(&_rewind_history[(_rewind_head+REWIND_CAPACITY-1)%REWIND_CAPACITY])) rewind_clear();
	if(!capture_game_state(&_rewind_history[_rewind_head])) {rewind_clear();return;}
	_rewind_head=(_rewind_head+1)%REWIND_CAPACITY;
	_rewind_count=MIN(REWIND_CAPACITY,_rewind_count+1);
}
static void rewind_finish() {
	if(!_rewind_running)return;
	_rewind_running=false;
	/* Discard effects from the abandoned future, restart the saved song and
	 * rebuild the audio backend once, never for every reverse frame. */
	if(!g_sys.input.quit) {
		g_mix.fini();sound_fini();sound_init();g_mix.init();play_music(g_vars.music_num);
	}
	g_sys.set_rewind(false,MAX(0,_rewind_count-1));
}
static void rewind_step() {
	if(!_rewind_history || !_rewind_count)return;
	const struct bb_state *latest=&_rewind_history[(_rewind_head+REWIND_CAPACITY-1)%REWIND_CAPACITY];
	if(!rewind_same_context(latest)) {rewind_clear();return;}
	_rewind_running=true;
	/* Consume two 30 Hz snapshots per display tick, stopping at the oldest. */
	const int steps=MIN(2,_rewind_count-1);
	_rewind_head=(_rewind_head+REWIND_CAPACITY-steps)%REWIND_CAPACITY;
	_rewind_count-=steps;
	const struct bb_state *s=&_rewind_history[(_rewind_head+REWIND_CAPACITY-1)%REWIND_CAPACITY];
	if((s->width!=GAME_SCREEN_W || s->height!=GAME_SCREEN_H) && !resize_level_view(s->width,s->height,false)) {
		rewind_clear();rewind_finish();return;
	}
	g_options.screen_w=s->width;g_options.screen_h=s->height;
	restore_timeline_state(s);
	g_sys.set_rewind(true,_rewind_count-1);
	repaint_restored_state();
}
