/* Native reimplementation of the Blues Brothers DOS AdLib sequencer.
 * Offsets below refer to the data segment of the supported 1991 executable.
 * Game instruments and songs are loaded at runtime, never embedded here. */
#include "dos_audio.h"
#include "audio_gain.h"
#include "third_party/nuked-opl3/opl3.h"
#include <stdlib.h>
#include <string.h>

#define DOS_DATA_OFFSET 0x11e00
#define DOS_DATA_SIZE 0x878c
#define PIT_CLOCK 1193182
#define PIT_DIVISOR 16384
#define TRACK_COUNT 10

struct dos_track {
	uint8_t duration, volume, unused, drum_duration, gate, channel, instrument;
	uint8_t remaining, note, octave, loop, previous, held, mode;
	uint16_t position, return_position;
	int ended;
};

struct dos_audio {
	uint8_t data[65536];
	opl3_chip opl;
	struct dos_track tracks[TRACK_COUNT];
	uint16_t instruments;
	uint8_t rhythm, divider, countdown;
	int rate, playing, failed, track_count, pending_sound;
	uint64_t phase, rendered;
	int music_volume, effects_volume;
	int writing_effect, effect_channel;
	uint8_t rhythm_written;
	struct audio_gain music_gain, effects_gain;
};

static uint16_t word(const struct dos_audio *a, uint16_t p) {
	return a->data[p] | (a->data[(uint16_t)(p + 1)] << 8);
}

static void write_reg(struct dos_audio *a, uint8_t reg, uint8_t value) {
	/* Channel 6 is shared by music's bass drum and DOS effects. Its owner
	 * changes only when the original sequencer actually programs that voice. */
	const int base = reg & 0xe0;
	const int slot = reg & 0x1f;
	const int op6 = (base == 0x20 || base == 0x40 || base == 0x60 || base == 0x80 || base == 0xe0) &&
		(slot == a->data[0x8405+13] || slot == a->data[0x8405+16]);
	if (op6 || reg == 0xa6 || reg == 0xb6 || reg == 0xc6 ||
		(reg == 0xbd && ((value ^ a->rhythm_written) & 0x10))) {
		a->effect_channel = a->writing_effect;
	}
	if (reg == 0xbd) a->rhythm_written = value;
	OPL3_WriteRegBuffered(&a->opl, reg, value);
}

static int operator_offset(struct dos_audio *a, int channel) {
	if (channel < 0 || channel > 10) {
		a->failed = 1;
		return 0;
	}
	return a->data[0x8418 + channel];
}

static void write_operator(struct dos_audio *a, uint16_t p, int slot) {
	static const uint8_t regs[] = { 0x60, 0x80, 0x40, 0x20, 0xe0 };
	if (slot < 0 || slot > 18 || p >= DOS_DATA_SIZE - 5) {
		a->failed = 1;
		return;
	}
	const int offset = a->data[0x8405 + slot];
	for (int i = 0; i < 5; ++i) {
		write_reg(a, regs[i] + offset, a->data[p + i]);
	}
}

static uint16_t instrument_data(struct dos_audio *a, int instrument) {
	if (instrument < 0 || instrument >= 18) {
		a->failed = 1;
		return 0;
	}
	const uint16_t p = word(a, a->instruments + instrument * 2);
	if (p < 0x5b48 || p >= DOS_DATA_SIZE - 12) {
		a->failed = 1;
		return 0;
	}
	return p;
}

static void set_volume(struct dos_audio *a, struct dos_track *t, uint8_t volume) {
	t->volume = volume;
	uint16_t p = instrument_data(a, t->instrument);
	if (t->instrument >= 15) {
		++p;
	}
	const int level = (63 - (a->data[p + 2] & 63)) * 16 * (volume + 1);
	int slot = operator_offset(a, t->channel);
	if (slot <= 13) {
		slot += 3;
	}
	if (slot > 18) {
		a->failed = 1;
		return;
	}
	write_reg(a, 0x40 + a->data[0x8405 + slot], (uint8_t)(63 - (level >> 8)));
}

static void set_instrument(struct dos_audio *a, struct dos_track *t, uint8_t mode) {
	t->mode = mode;
	if (mode == 1) {
		t->instrument = t->octave + 15;
		const uint16_t p = instrument_data(a, t->instrument);
		t->channel = a->data[p];
		if (t->channel < 6 || t->channel > 10) {
			a->failed = 1;
			return;
		}
		a->rhythm |= 0x10 >> (t->channel - 6);
	} else {
		t->instrument = mode > 1 ? mode - 1 : mode;
		if (t->channel >= 6 && t->channel <= 10) {
			a->rhythm &= ~(0x10 >> (t->channel - 6));
			write_reg(a, 0xbd, a->rhythm);
		}
	}
	int slot = operator_offset(a, t->channel);
	if (t->channel <= 6) {
		slot += 3;
	}
	uint16_t p = instrument_data(a, t->instrument);
	if (t->instrument >= 15) {
		++p;
	}
	write_operator(a, p, slot);
	if (t->channel < 7) {
		write_operator(a, p + 5, slot - 3);
		write_reg(a, 0xc0 + t->channel, a->data[p + 10]);
	}
}

static void play_note(struct dos_audio *a, struct dos_track *t, uint16_t frequency) {
	if (frequency != 0) {
		if (t->instrument >= 15) {
			if (t->instrument != t->octave + 15) {
				set_instrument(a, t, 1);
				set_volume(a, t, t->volume);
			}
			if (t->channel < 6 || t->channel > 10) {
				a->failed = 1;
				return;
			}
			write_reg(a, 0xbd, a->rhythm & ~(0x10 >> (t->channel - 6)));
			if (t->channel == 6) {
				write_reg(a, 0xa6, 0x57);
				write_reg(a, 0xb6, 0);
				write_reg(a, 0xb6, 5);
			}
			write_reg(a, 0xbd, a->rhythm);
			return;
		}
		write_reg(a, 0xa0 + t->channel, frequency);
		if (t->previous != t->note || !t->held) {
			write_reg(a, 0xb0 + t->channel, 0);
		}
		write_reg(a, 0xb0 + t->channel, (frequency >> 8) | ((t->octave + 2) << 2) | 0x20);
	}
	t->held = t->gate;
}

static void tick_track(struct dos_audio *a, struct dos_track *t) {
	if (t->ended) {
		return;
	}
	const uint8_t previous_remaining = t->remaining--;
	if (previous_remaining > 1) {
		return;
	}
	/* A corrupt jump must never hang the real-time audio thread. */
	for (int budget = 0; budget < 4096; ++budget) {
		if (t->position < 0x5b48 || t->position >= 0x77c5) {
			a->failed = 1;
			return;
		}
		const uint8_t command = a->data[t->position++];
		const int value = command & 15;
		if (!(command & 0x80)) {
			t->previous = t->note;
			t->note = value;
			t->octave = command >> 4;
			if (t->note > 14) {
				a->failed = 1;
				return;
			}
			play_note(a, t, word(a, 0x8423 + t->note * 2));
			int shift = t->duration == 7 ? t->drum_duration : t->duration;
			t->remaining = (shift < 8) ? ((t->duration == 7 ? 0x40 : 0x60) >> shift) : 0;
			return;
		}
		switch ((command >> 4) & 7) {
		case 0: t->duration = value; break;
		case 1: set_volume(a, t, value); break;
		case 2: t->unused = value; break;
		case 3: t->drum_duration = value; break;
		case 4: t->gate = value; break;
		case 5: t->channel = value; if (value > 10) a->failed = 1; break;
		case 6: set_instrument(a, t, value); break;
		case 7:
			switch (value) {
			case 0:
				t->return_position = t->position + 2;
				t->position = word(a, t->position);
				break;
			case 1: t->loop = a->data[t->position++]; break;
			case 2:
				if (--t->loop) t->position = word(a, t->position);
				else t->position += 2;
				break;
			case 3: t->position = t->return_position; break;
			case 4: t->position = word(a, t->position); break;
			case 15: t->ended = 1; return;
			default: a->failed = 1; return;
			}
			break;
		}
		if (a->failed) return;
	}
	a->failed = 1;
}

static void tick(struct dos_audio *a) {
	if (--a->countdown == 0) {
		a->countdown = a->divider;
		return;
	}
	for (int i = 0; i < a->track_count && !a->failed; ++i) {
		tick_track(a, &a->tracks[i]);
	}
	if (a->pending_sound >= 0 && !a->failed) {
		uint16_t p = word(a, 0x82e5 + a->pending_sound * 2) + 1;
		a->pending_sound = -1;
		if (p < 0x5b48 || p >= DOS_DATA_SIZE - 11) {
			a->failed = 1;
			return;
		}
		a->writing_effect = 1;
		a->rhythm &= ~0x10;
		write_reg(a, 0xbd, a->rhythm);
		write_operator(a, p, 16);
		write_operator(a, p + 5, 13);
		write_reg(a, 0xc6, a->data[p + 10]);
		write_reg(a, 0xa6, 0x57);
		write_reg(a, 0xb6, 0);
		write_reg(a, 0xb6, 0x29);
		a->writing_effect = 0;
	}
}

struct dos_audio *dos_audio_create(const uint8_t *exe, size_t size, int rate) {
	/* Reject other releases instead of interpreting unrelated EXE bytes as music. */
	static const uint8_t entry[] = { 0xba, 0xc0, 0x0f, 0x2e, 0x89, 0x16 };
	static const uint8_t writer[] = { 0xba, 0x88, 0x03, 0xee, 0xb9, 0x05, 0x00 };
	static const uint8_t songs[] = { 0x4f, 0x83, 0x2b, 0x83, 4, 0, 0, 0,
		0x65, 0x83, 0x07, 0x83, 50, 0, 0, 0,
		0x7b, 0x83, 0x07, 0x83, 4, 0, 0, 0,
		0x91, 0x83, 0x07, 0x83, 3, 0, 0, 0 };
	if (!exe || size < DOS_DATA_OFFSET + DOS_DATA_SIZE || rate < 8000 || rate > 192000 ||
		memcmp(exe, "MZ", 2) || exe[8] != 0x20 || exe[9] != 2 ||
		memcmp(exe + 0x2200, entry, sizeof(entry)) ||
		memcmp(exe + 0x112c1, writer, sizeof(writer)) ||
		memcmp(exe + DOS_DATA_OFFSET + 0x82c3, songs, sizeof(songs))) {
		return NULL;
	}
	struct dos_audio *a = calloc(1, sizeof(*a));
	if (!a) return NULL;
	memcpy(a->data, exe + DOS_DATA_OFFSET, DOS_DATA_SIZE);
	a->rate = rate;
	a->music_volume = a->effects_volume = 100;
	audio_gain_init(&a->music_gain,100);
	audio_gain_init(&a->effects_gain,100);
	a->rhythm = 0x20;
	a->pending_sound = -1;
	OPL3_Reset(&a->opl, rate);
	static const uint16_t init[] = { 0x0001, 0x0008, 0x00bd, 0x00b6,
		0x00a6, 0x1135, 0x57a8, 0x09b8, 0x03a7, 0x0ab7 };
	for (unsigned i = 0; i < sizeof(init) / sizeof(init[0]); ++i) {
		write_reg(a, init[i] & 255, init[i] >> 8);
	}
	for (int reg = 0xb0; reg < 0xb9; ++reg) write_reg(a, reg, 0);
	for (int reg = 0xa0; reg < 0xb9; ++reg) write_reg(a, reg, 0);
	return a;
}

void dos_audio_destroy(struct dos_audio *a) {
	free(a);
}

void dos_audio_music(struct dos_audio *a, int song) {
	if (!a || song < 0 || song > 3) return;
	const uint16_t info = 0x82c3 + song * 8;
	uint16_t p = word(a, info);
	a->instruments = word(a, info + 2);
	a->divider = a->countdown = a->data[info + 4];
	a->track_count = 0;
	a->failed = 0;
	memset(a->tracks, 0, sizeof(a->tracks));
	while (word(a, p) != 0xffff && a->track_count < TRACK_COUNT) {
		struct dos_track *t = &a->tracks[a->track_count];
		t->channel = a->track_count;
		t->position = word(a, p);
		++a->track_count;
		p += 2;
	}
	a->playing = 1;
}

void dos_audio_sound(struct dos_audio *a, int sound) {
	if (a && sound >= 0 && sound < 18) a->pending_sound = sound;
}

void dos_audio_set_volumes(void *context, int music, int effects) {
	struct dos_audio *a = context;
	if (!a) return;
	a->music_volume = audio_volume(music);
	a->effects_volume = audio_volume(effects);
	if (!a->rendered) {
		/* New backends inherit mute/levels before their first sample. */
		audio_gain_init(&a->music_gain,a->music_volume);
		audio_gain_init(&a->effects_gain,a->effects_volume);
	} else {
		audio_gain_set(&a->music_gain,a->music_volume,a->rate);
		audio_gain_set(&a->effects_gain,a->effects_volume,a->rate);
	}
}

void dos_audio_render(void *context, int16_t *samples, int count) {
	struct dos_audio *a = context;
	for (int i = 0; i < count; ++i) {
		if (!a || a->failed) {
			samples[i] = 0;
			continue;
		}
		a->phase += PIT_CLOCK;
		if (a->phase >= (uint64_t)a->rate * PIT_DIVISOR) {
			a->phase -= (uint64_t)a->rate * PIT_DIVISOR;
			if (a->playing) tick(a);
		}
		const int music_gain = audio_gain_next(&a->music_gain);
		for (int ch = 0; ch < 18; ++ch) a->opl.channel_gain[ch] = music_gain;
		const int effects_gain = audio_gain_next(&a->effects_gain);
		a->opl.channel_gain[6] = a->effect_channel ? effects_gain : music_gain;
		++a->rendered;
		int16_t stereo[2];
		OPL3_GenerateResampled(&a->opl, stereo);
		samples[i] = ((int)stereo[0] + stereo[1]) / 2;
	}
}
