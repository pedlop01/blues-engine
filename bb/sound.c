
#include "game.h"
#include "resource.h"
#include "mixer.h"
#include "util.h"
#include "dos_audio.h"
#include "sys.h"

#define PAULA_FREQ 3546897

static struct dos_audio *_dos_audio;

static const struct {
	uint16_t offset; // words
	uint16_t size;
} _sounds_amiga[] = {
	{ 0x0000, 0x0256 },
	{ 0x0000, 0x0256 },
	{ 0x0000, 0x0256 },
	{ 0x0000, 0x0256 },
	{ 0x012b, 0x051a },
	{ 0x03b8, 0x0532 },
	{ 0x0651, 0x03e8 },
	{ 0x0b51, 0x0c1c },
	{ 0x115f, 0x0e40 },
	{ 0x0000, 0x0256 },
	{ 0x0000, 0x0256 },
	{ 0x187f, 0x04c4 },
	{ 0x1ae1, 0x0c14 },
	{ 0x20eb, 0x1e1a },
	{ 0x2ff8, 0x0610 },
	{ 0x3300, 0x0420 },
	{ 0x3510, 0x032c },
	{ 0x36a6, 0x0574 }
};

// Amiga, ExoticA
static const char *_modules[] = {
	"almost", "mod.almost",
	"gunn",   "mod.bluesgunnbest",
	"every",  "mod.every",
	"shot",   "mod.shot"
};

static uint8_t *load_file(const char *filename, int *size, uint8_t *buffer) {
	FILE *fp = fopen_nocase(g_res.datapath, filename);
	if (!fp) {
		return 0;
	}
	fseek(fp, 0, SEEK_END);
	const int filesize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if (!buffer) {
		buffer = (uint8_t *)malloc(filesize);
		if (!buffer) {
			print_warning("Failed to allocate %d bytes", filesize);
			fclose(fp);
			return 0;
		}
	}
	if (fread(buffer, 1, filesize, fp) != filesize) {
		print_warning("Failed to read %d bytes from '%s'", filesize, filename);
		free(buffer);
		buffer = 0;
	} else {
		*size = filesize;
	}
	fclose(fp);
	return buffer;
}

void sound_init() {
	if (g_options.sound_mode == SOUND_AMIGA ||
		(g_options.sound_mode == SOUND_AUTO && g_res.amiga_data)) {
		return;
	}
	int size = 0;
	uint8_t *exe = load_file("1.exe", &size, 0);
	if (exe) {
		_dos_audio = dos_audio_create(exe, size, SYS_AUDIO_FREQ);
		free(exe);
	}
	if (!_dos_audio) {
		if (g_options.sound_mode == SOUND_DOS) {
			print_error("DOS audio needs a supported original 1.EXE in the data directory");
		}
		print_warning("DOS audio unavailable (missing or unsupported 1.EXE); trying Amiga audio");
	}
}

void sound_fini() {
	/* The mixer stops its audio thread before this is called. */
	dos_audio_destroy(_dos_audio);
	_dos_audio = 0;
}

void play_sound(int num) {
	if (num < 0 || num >= (int)ARRAYSIZE(_sounds_amiga)) return;
	if (_dos_audio) {
		g_sys.lock_audio();
		dos_audio_sound(_dos_audio, num);
		g_sys.unlock_audio();
		return;
	}
	if (g_res.snd) {
		g_mix.play_sound(g_res.snd + _sounds_amiga[num].offset * 2, _sounds_amiga[num].size, PAULA_FREQ / 0x358, 0);
	}
}

void play_music(int num) {
	if (num < 0 || num >= 4) return;
	if (_dos_audio) {
		g_sys.lock_audio();
		dos_audio_music(_dos_audio, num);
		g_sys.unlock_audio();
		g_mix.set_music_callback(dos_audio_render, _dos_audio, dos_audio_set_volumes);
		return;
	}
	const char *filename = _modules[num * 2]; // Amiga
	int size = 0;
	uint8_t *buf = load_file(filename, &size, 0);
	if (buf) {
		// append samples to the end of the buffer
		static const int SONG_INFO_LEN = 20;
		static const int NUM_SAMPLES = 8;
		struct {
			char name[23];
			int size;
		} samples[8];

		int samples_size = 0;
		int offset = SONG_INFO_LEN;
		for (int i = 0; i < NUM_SAMPLES; ++i, offset += 30) {
			memcpy(samples[i].name, &buf[offset], 22);
			samples[i].name[22] = 0;
			samples[i].size = READ_BE_UINT16(&buf[offset + 22]) * 2;
			samples_size += samples[i].size;
		}
		buf = (uint8_t *)realloc(buf, size + samples_size);
		if (buf) {
			memset(buf + size, 0, samples_size);
			for (int i = 0; i < NUM_SAMPLES; ++i) {
				if (samples[i].size != 0) {
					if (samples[i].name[0]) {
						int sample_size = 0;
						if (!load_file(samples[i].name, &sample_size, buf + size)) {
							print_warning("Unable to load instrument '%s'", samples[i].name);
						}
					}
					size += samples[i].size;
				}
			}
			g_mix.play_music(buf, size);
			free(buf);
		}
	} else { // ExoticA
		filename = _modules[num * 2 + 1];
		buf = load_file(filename, &size, 0);
		if (buf) {
			g_mix.play_music(buf, size);
			free(buf);
		}
	}
}
