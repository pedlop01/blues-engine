#ifndef BB_STATE_H
#define BB_STATE_H
#include "game.h"
#include "resource.h"
#define BB_STATE_MAX_SIZE (128*1024)
#define BB_STATE_MAP_SIZE (640*25)
struct view_transition { int from_w, from_h, to_w, to_h, elapsed; };
struct bb_camera_state {
	bool valid;
	double x, y, lead;
	int origin_x, origin_y, width, height, player;
};
struct bb_state {
	struct vars_t vars;
	struct trigger_t triggers[MAX_TRIGGERS];
	uint8_t map[BB_STATE_MAP_SIZE];
	uint8_t animation_counters[103];
	uint8_t level_types[MAX_LEVELS*MAX_OBJECTS];
	uint16_t object_widths[MAX_OBJECTS], object_heights[MAX_OBJECTS];
	struct bb_camera_state camera;
	struct view_transition view;
	int width, height, base_width, base_height, view_step, input_view_step, frame_remainder;
	bool amiga, demo, cga, amiga_colors, copper, camera_cut;
	int camera_mode, hud_mode, cheats, sound_mode;
	int start_xpos16, start_ypos16;
	bool dos_scrolling;
	uint32_t resources;
	uint64_t timestamp;
};
uint32_t bb_state_crc(const uint8_t *data, size_t size);
bool bb_state_encode(const struct bb_state *state, uint8_t *data, size_t capacity, size_t *size);
bool bb_state_decode(struct bb_state *state, const uint8_t *data, size_t size);
bool bb_state_attach_preview(uint8_t *data, size_t capacity, size_t *size, const uint8_t *rgb);
const uint8_t *bb_state_preview(const uint8_t *data, size_t size);
bool bb_state_valid(const struct bb_state *state);
/* In-memory representation resolves only trusted static table identifiers. */
#endif
