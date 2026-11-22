#ifndef AUDIO_GAIN_H__
#define AUDIO_GAIN_H__
#include <stdint.h>

/* Q16 linear gain; a 20ms ramp keeps volume changes continuous across blocks. */
struct audio_gain { int32_t value, target; int remaining; };
static inline int audio_volume(int percent) { return percent < 0 ? 0 : (percent > 100 ? 100 : percent); }
static inline void audio_gain_init(struct audio_gain *gain, int percent) {
	gain->value = gain->target = audio_volume(percent) * 65536 / 100;
	gain->remaining = 0;
}
static inline void audio_gain_set(struct audio_gain *gain, int percent, int rate) {
	const int target = audio_volume(percent) * 65536 / 100;
	if (gain->target == target) return;
	gain->target = target;
	gain->remaining = rate / 50;
	if (gain->remaining < 1) gain->remaining = 1;
}
static inline int audio_gain_next(struct audio_gain *gain) {
	if (gain->remaining) {
		gain->value += (gain->target - gain->value) / gain->remaining;
		--gain->remaining;
	}
	return gain->value;
}
static inline int audio_gain_apply(int sample, int gain) { return (int32_t)sample * gain / 65536; }
#endif
