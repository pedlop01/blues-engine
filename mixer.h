
#ifndef MIXER_H__
#define MIXER_H__

#include "intern.h"

typedef void (*mix_repeat_sound_cb)(const uint8_t **data, uint32_t *size);
typedef void (*mix_volume_cb)(void *context, int music, int effects);
typedef void (*mix_music_cb)(void *context, int16_t *samples, int count);

struct mixer_t {
	void (*init)();
	void (*fini)();
	void (*play_sound)(const uint8_t *data, uint32_t size, int freq, mix_repeat_sound_cb repeat_cb);
	void (*play_music)(const uint8_t *data, uint32_t size);
	void (*set_music_callback)(mix_music_cb callback, void *context, mix_volume_cb volumes);
	void (*set_volumes)(int music, int effects);
};

extern struct mixer_t g_mix;

#endif
