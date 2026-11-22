/* Deterministic music/PCM bus and ramp tests, without an audio device. */
#include "../mixer.c"
static void no_lock() {}
struct sys_t g_sys = {.lock_audio=no_lock,.unlock_audio=no_lock};
static void constant_music(void *param,int16_t *samples,int count) {
	for(int i=0;i<count;++i)samples[i]=10000;
}
int main() {
	static uint8_t effect[200000];
	memset(effect,20,sizeof(effect));
	set_music_callback(constant_music,NULL,NULL);
	play_sound(effect,sizeof(effect),SYS_AUDIO_FREQ,NULL);
	int16_t samples[2048];
	mix(NULL,(uint8_t*)samples,sizeof(samples));
	for(int i=0;i<2048;++i) assert(samples[i]==15120);
	set_volumes(0,100);
	mix(NULL,(uint8_t*)samples,sizeof(samples));
	for(int i=1;i<882;++i) assert(samples[i]<=samples[i-1] && samples[i-1]-samples[i]<=12);
	for(int i=882;i<2048;++i) assert(samples[i]==5120);
	set_volumes(100,0);
	mix(NULL,(uint8_t*)samples,sizeof(samples));
	for(int i=882;i<2048;++i) assert(samples[i]==10000);
	set_volumes(0,0);
	mix(NULL,(uint8_t*)samples,sizeof(samples));
	for(int i=882;i<2048;++i) assert(samples[i]==0);
	const uint32_t pos=_channel.pos;
	mix(NULL,(uint8_t*)samples,sizeof(samples));
	assert(_channel.pos>pos); /* Muting never pauses playback. */
	set_volumes(40,70);
	const struct mixerchannel_t channel=_channel;
	const struct audio_gain music_gain=_music_gain,effects_gain=_effects_gain;
	int16_t whole[4096],chunked[4096];
	mix(NULL,(uint8_t*)whole,sizeof(whole));
	_channel=channel; _music_gain=music_gain; _effects_gain=effects_gain;
	for(int offset=0;offset<4096;) {
		int n=MIN(13,4096-offset);
		mix(NULL,(uint8_t*)(chunked+offset),n*2); offset+=n;
	}
	assert(memcmp(whole,chunked,sizeof(whole))==0);
	for(int i=882;i<4096;++i)assert(whole[i]==audio_gain_apply(10000,40*65536/100)+audio_gain_apply(5120,70*65536/100));
	puts("mixer: independent music/PCM levels, exact mute, 20ms bounded ramps and block continuity passed");
	return 0;
}
