
#ifndef SYS_H__
#define SYS_H__

#include "intern.h"

#define INPUT_DIRECTION_LEFT  (1 << 0)
#define INPUT_DIRECTION_RIGHT (1 << 1)
#define INPUT_DIRECTION_UP    (1 << 2)
#define INPUT_DIRECTION_DOWN  (1 << 3)

#define SYS_AUDIO_FREQ 44100

#define RENDER_SPR_GAME  0 /* player sprites */
#define RENDER_SPR_LEVEL 1 /* level sprites */
#define RENDER_SPR_FG    2 /* foreground tiles */
#define RENDER_SPR_COUNT 3

struct input_t {
	uint8_t direction;
	bool quit;
	bool space;
	bool jump;
	bool digit1, digit2, digit3;
	int view_step; /* 0 = initial framing; higher values reveal more map. */
	bool switch_mode;
	bool switch_hud;
	bool menu;
	bool rewind;
};

typedef void (*sys_audio_cb)(void *, uint8_t *data, int len);

struct sys_rect_t {
	int x, y;
	int w, h;
};

enum sys_transition_e {
	TRANSITION_SQUARE,
	TRANSITION_CURTAIN
};

enum sys_save_status { SAVE_EMPTY, SAVE_READY, SAVE_INCOMPATIBLE, SAVE_DAMAGED, SAVE_UNREADABLE };
struct sys_save_slot {
	enum sys_save_status status;
	int level, lives[2];
	bool amiga, two_players, infinite_lives, has_preview, has_metadata;
	uint64_t timestamp;
	const char *reason;
	uint8_t preview[SAVE_PREVIEW_BYTES];
};

struct sys_pause_options {
	bool amiga, can_switch;
	bool invulnerable, infinite_lives, two_players, player_alive[2];
	int lives[2];
	bool select_character, reset_defaults, load_game;
	bool (*save_game)(int slot);
	bool (*prepare_load)(int slot);
	void (*inspect_save)(int slot, struct sys_save_slot *info);
	const char *(*save_error)();
	const char *load_notice;
	int current_level, requested_level; /* -1 means continue. */
	bool available_levels[2][6]; /* DOS, Amiga; six stages each. */
	enum camera_mode camera;
	enum hud_mode hud;
	int render_rate, view_step;
	int music_volume, effects_volume;
	void (*apply_volumes)(int music, int effects);
};

struct sys_t {
	struct input_t	input;
	void	(*init)();
	void	(*load_preferences)();
	void	(*save_preferences)();
	bool	(*read_save_file)(int slot, uint8_t *data, size_t capacity, size_t *size);
	bool	(*write_save_file)(int slot, const uint8_t *data, size_t size);
	int	(*save_file_status)(int slot);
	bool	(*capture_save_preview)(uint8_t *rgb);
	void	(*fini)();
	void	(*set_screen_size)(int w, int h, const char *caption, int scale, const char *filter, bool fullscreen);
	void	(*set_screen_palette)(const uint8_t *colors, int offset, int count, int depth);
	void	(*set_palette_amiga)(const uint16_t *colors, int offset);
	void	(*set_copper_bars)(const uint16_t *data);
	void	(*set_palette_color)(int i, const uint8_t *colors);
	void	(*fade_in_palette)();
	void	(*fade_out_palette)();
	void	(*copy_bitmap)(const uint8_t *p, int w, int h);
	void	(*update_screen)();
	void	(*shake_screen)(int dx, int dy);
	void	(*transition_screen)(enum sys_transition_e type, bool open);
	void	(*process_events)();
	void	(*set_pause_menu_enabled)(bool enabled);
	void	(*set_rewind)(bool active, int frames);
	void	(*set_assist_status)(bool invulnerable, bool infinite_lives);
	void	(*run_pause_menu)();
	void	(*set_pause_options)(const struct sys_pause_options *options);
	void	(*get_pause_options)(struct sys_pause_options *options);
	void	(*sleep)(int duration);
	uint32_t	(*get_timestamp)();
	void	(*start_audio)(sys_audio_cb callback, void *param);
	void	(*stop_audio)();
	void	(*lock_audio)();
	void	(*unlock_audio)();
	void	(*render_load_sprites)(int spr_type, int count, const struct sys_rect_t *r, const uint8_t *data, int w, int h, uint8_t color_key, bool update_pal);
	void	(*render_unload_sprites)(int spr_type);
	void	(*render_add_sprite)(int spr_type, int frame, int x, int y, int xflip);
	void	(*render_clear_sprites)();
	void	(*render_set_sprites_clipping_rect)(int x, int y, int w, int h);
	void	(*print_log)(FILE *fp, const char *s);
	void	(*set_scroll_interpolation)(int dx, int dy, int simulation_period, bool enabled);
	void	(*set_scroll_render_rate)(int rate);
	bool	(*resize_viewport)(int w, int h);
	/* Overlay strips sampled from the centered native HUD in copy_bitmap. */
	void	(*set_hud_layout)(int width, int height, int top, int bottom);
	void	(*begin_sprite_frame)();
	void	(*set_sprite_owner)(int owner);
};

extern struct sys_t g_sys;

#endif /* SYS_H__ */
