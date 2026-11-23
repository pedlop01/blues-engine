
#include "mixer.h"
#include "audio_gain.h"
#include "sys.h"
#include "util.h"

#include <libmodplug/modplug.h>

struct mixerchannel_t {
	const uint8_t *data;
	uint32_t pos;
	uint32_t step;
	uint32_t size;
	mix_repeat_sound_cb repeat_cb;
};

static const int _rate = SYS_AUDIO_FREQ;
static struct mixerchannel_t _channel;
static ModPlugFile *_mpf;
static mix_music_cb _music_callback;
static void *_music_context;
static mix_volume_cb _volume_callback;
static int _headroom = 65536;
static void set_headroom(int percent) { _headroom = audio_volume(percent) * 65536 / 100; }
static int _music_volume = 100, _effects_volume = 100;
static struct audio_gain _music_gain = {65536,65536,0}, _effects_gain = {65536,65536,0};

static void mix(void *param, uint8_t *buf, int len) {
	memset(buf, 0, len);
	if (_music_callback) {
		_music_callback(_music_context, (int16_t *)buf, len / sizeof(int16_t));
	}
	if (_mpf) {
		const int count = ModPlug_Read(_mpf, buf, len);
		if (count == 0) {
			ModPlug_SeekOrder(_mpf, 0);
		}
	}
	for (int i = 0; i + 1 < len; i += sizeof(int16_t)) {
		const int music_gain = audio_gain_next(&_music_gain);
		const int effects_gain = audio_gain_next(&_effects_gain);
		int sample = *(int16_t *)(buf + i);
		/* A combined native callback owns its two buses before summation. */
		if (!_volume_callback) sample = audio_gain_apply(sample,music_gain);
		sample = audio_gain_apply(sample,_headroom);
		if (_channel.data) {
			int pos = _channel.pos >> 16;
			if (pos >= _channel.size) {
				_channel.data = 0;
				if (_channel.repeat_cb) _channel.repeat_cb(&_channel.data,&_channel.size);
				if (_channel.data) _channel.pos = pos = 0;
			}
			if (_channel.data) {
				sample += audio_gain_apply(audio_gain_apply(((int8_t)_channel.data[pos])*256,effects_gain),_headroom);
				_channel.pos += _channel.step;
			}
		}
		*(int16_t *)(buf + i) = sample < -32768 ? -32768 : (sample > 32767 ? 32767 : sample);
	}
}

static void mixer_init() {
	ModPlug_Settings mp_settings;
	memset(&mp_settings, 0, sizeof(mp_settings));
	ModPlug_GetSettings(&mp_settings);
	mp_settings.mFlags = MODPLUG_ENABLE_OVERSAMPLING | MODPLUG_ENABLE_NOISE_REDUCTION;
	mp_settings.mChannels = 1;
	mp_settings.mBits = 16;
	mp_settings.mFrequency = _rate;
	mp_settings.mResamplingMode = MODPLUG_RESAMPLE_FIR;
	mp_settings.mLoopCount = -1;
	ModPlug_SetSettings(&mp_settings);
	g_sys.start_audio(mix, 0);
}

static void mixer_fini() {
	g_sys.stop_audio();
	if (_mpf) {
		ModPlug_Unload(_mpf);
		_mpf = 0;
	}
	_music_callback = 0;
	_music_context = 0;
	_volume_callback = 0;
	memset(&_channel, 0, sizeof(_channel));
	_headroom = 65536;
}

static void play_sound(const uint8_t *data, uint32_t size, int freq, mix_repeat_sound_cb repeat_cb) {
	g_sys.lock_audio();
	_channel.data = data;
	_channel.pos = 0;
	_channel.step = ((uint64_t)freq << 16) / _rate;
	_channel.size = size;
	_channel.repeat_cb = repeat_cb;
	g_sys.unlock_audio();
}

static void play_music(const uint8_t *data, uint32_t size) {
	g_sys.lock_audio();
	_music_callback = 0;
	_music_context = 0;
	_volume_callback = 0;
	if (_mpf) {
		ModPlug_Unload(_mpf);
		_mpf = 0;
	}
	_mpf = ModPlug_Load(data, size);
	if (_mpf) {
		print_debug(DBG_MIXER, "Loaded module '%s'", ModPlug_GetName(_mpf));
	}
	g_sys.unlock_audio();
}

static void set_music_callback(mix_music_cb callback, void *context, mix_volume_cb volumes) {
	g_sys.lock_audio();
	if (_mpf) {
		ModPlug_Unload(_mpf);
		_mpf = 0;
	}
	_music_callback = callback;
	_music_context = context;
	_volume_callback = volumes;
	if (volumes) volumes(context,_music_volume,_effects_volume);
	g_sys.unlock_audio();
}

static void set_volumes(int music, int effects) {
	g_sys.lock_audio();
	_music_volume = audio_volume(music); _effects_volume = audio_volume(effects);
	audio_gain_set(&_music_gain,_music_volume,_rate);
	audio_gain_set(&_effects_gain,_effects_volume,_rate);
	if (_volume_callback) _volume_callback(_music_context,_music_volume,_effects_volume);
	g_sys.unlock_audio();
}

struct mixer_t g_mix = {
	.init = mixer_init,
	.fini = mixer_fini,
	.play_sound = play_sound,
	.play_music = play_music,
	.set_music_callback = set_music_callback,
	.set_volumes = set_volumes,
	.set_headroom = set_headroom,
};
