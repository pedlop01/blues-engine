/* Deterministic tests of the production renderer's actor placement. */
#include "../sys_sdl2.c"
#include <math.h>

static void next_frame(void) {
	sdl2_begin_sprite_frame();
	render_clear_sprites();
}
static void actor(int owner, int frame, int x, int y) {
	sdl2_set_sprite_owner(owner);
	render_add_sprite(RENDER_SPR_GAME, frame, x, y, 0);
}
static void position(int index, float camera, float progress, float x, float y) {
	SDL_FRect r = sdl2_sprite_rect(&_sprites[index], camera, 0, progress);
	assert(fabsf(r.x - x) < .001f && fabsf(r.y - y) < .001f);
}
int main(void) {
	SDL_Rect frames[] = {{0,0,16,16}, {0,0,32,20}};
	_spritesheets[RENDER_SPR_GAME].r = malloc(sizeof(frames));
	memcpy(_spritesheets[RENDER_SPR_GAME].r, frames, sizeof(frames));
	_spritesheets[RENDER_SPR_GAME].count = 2;
	_scroll_interpolation_active = true;
	actor(1,0,100,50); actor(2,0,200,70);
	next_frame();
	/* Draw order changes; camera shifts 6px, actor 1 moves 6px in world. */
	actor(2,0,194,70); actor(1,0,100,50);
	position(0,3,.5f,197,70); position(1,3,.5f,100,50);
	next_frame();
	/* Wider/taller animation retains the same center and feet. */
	actor(1,1,92,46);
	position(0,0,0,92,46); position(0,0,.5f,92,46);
	next_frame(); actor(1,1,104,54);
	for (int rate = 60; rate <= 120; rate *= 2) {
	 for (int sample = 0; sample <= rate / 30; ++sample) {
	  float t = (float)sample / (rate / 30);
	  position(0,0,t,92+12*t,46+8*t);
	 }
	}
	/* Multiple parts of the same object keep independent positions. */
	next_frame(); actor(3,0,10,10);
	render_add_sprite(RENDER_SPR_GAME,0,30,10,0);
	next_frame(); actor(3,0,20,10);
	render_add_sprite(RENDER_SPR_GAME,0,40,10,0);
	position(0,0,.5f,15,10); position(1,0,.5f,35,10);
	/* Reused slots of another type and absent actors have no history. */
	next_frame(); actor(259,0,80,10); assert(!_sprites[0].interpolate);
	next_frame(); next_frame(); actor(3,0,20,10); assert(!_sprites[0].interpolate);
	next_frame(); actor(3,0,40,10);
	_scroll_interpolation_active = false; position(0,0,0,40,10);
	sdl2_set_scroll_interpolation(0,0,33,false);
	assert(_previous_sprites_count == 0);
	next_frame(); render_unload_sprites(RENDER_SPR_GAME);
	assert(_previous_sprites_count == 0);
	free(_sprites); free(_previous_sprites);
	puts("actors: 60/120 Hz, camera motion, animation anchors, ordering, multipart sprites, reused slots and cuts passed");
	return 0;
}
