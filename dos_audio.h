#ifndef DOS_AUDIO_H__
#define DOS_AUDIO_H__

#include <stddef.h>
#include <stdint.h>

struct dos_audio;

/* Reads instruments and sequences from the user's supported DOS executable.
 * Returns NULL for missing, truncated, or unsupported executable layouts. */
struct dos_audio *dos_audio_create(const uint8_t *exe, size_t size, int rate);
void dos_audio_destroy(struct dos_audio *audio);
void dos_audio_music(struct dos_audio *audio, int song);
void dos_audio_sound(struct dos_audio *audio, int sound);
void dos_audio_set_volumes(void *audio, int music, int effects);
void dos_audio_render(void *audio, int16_t *samples, int count);

#endif
