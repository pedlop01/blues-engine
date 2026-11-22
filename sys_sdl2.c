
#include <SDL.h>
#include <limits.h>
#include "sys.h"
#include "util.h"

#define COPPER_BARS_H 80
#define MAX_SPRITES 256

static const int FADE_STEPS = 16;

struct spritesheet_t {
	int count;
	SDL_Rect *r;
	SDL_Surface *surface;
	SDL_Texture *texture;
};

static struct spritesheet_t _spritesheets[RENDER_SPR_COUNT];

struct sprite_t {
	int sheet;
	int num;
	int x, y;
	bool xflip;
};

static struct sprite_t *_sprites;
static int _sprites_count, _sprites_capacity;
static SDL_Rect _sprites_cliprect;

static int _screen_w, _screen_h;
static int _shake_dx, _shake_dy;
static int _scroll_rate = 30;
static double _scroll_target_x, _scroll_target_y;
static double _scroll_previous_x, _scroll_previous_y;
static uint64_t _scroll_update_timestamp, _scroll_period_counter;
static uint64_t _render_schedule_origin, _next_render_counter;
static bool _scroll_interpolation_active, _render_schedule_initialized;
static SDL_Window *_window;
static SDL_Renderer *_renderer;
static SDL_Texture *_texture;
static SDL_PixelFormat *_fmt;
static SDL_Palette *_palette;
static uint32_t _screen_palette[256];
static uint32_t *_screen_buffer;
static int _copper_color_key;
static uint32_t _copper_palette[COPPER_BARS_H];

static SDL_GameController *_controller;
static SDL_Joystick *_joystick;

static void sdl2_init() {
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
	SDL_ShowCursor(SDL_DISABLE);
	_screen_w = _screen_h = 0;
	memset(_screen_palette, 0, sizeof(_screen_palette));
	_palette = SDL_AllocPalette(256);
	_screen_buffer = 0;
	_copper_color_key = -1;
	SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt");
	_controller = 0;
	const int count = SDL_NumJoysticks();
	if (count > 0) {
		for (int i = 0; i < count; ++i) {
			if (SDL_IsGameController(i)) {
				_controller = SDL_GameControllerOpen(i);
				if (_controller) {
					fprintf(stdout, "Using controller '%s'\n", SDL_GameControllerName(_controller));
					break;
				}
			}
		}
		if (!_controller) {
			_joystick = SDL_JoystickOpen(0);
			if (_joystick) {
				fprintf(stdout, "Using joystick '%s'\n", SDL_JoystickName(_joystick));
			}
		}
	}
}

static void sdl2_fini() {
	if (_fmt) {
		SDL_FreeFormat(_fmt);
		_fmt = 0;
	}
	if (_palette) {
		SDL_FreePalette(_palette);
		_palette = 0;
	}
	if (_texture) {
		SDL_DestroyTexture(_texture);
		_texture = 0;
	}
	if (_renderer) {
		SDL_DestroyRenderer(_renderer);
		_renderer = 0;
	}
	if (_window) {
		SDL_DestroyWindow(_window);
		_window = 0;
	}
	free(_screen_buffer);
	free(_sprites);
	_sprites = 0;
	_sprites_count = _sprites_capacity = 0;
	if (_controller) {
		SDL_GameControllerClose(_controller);
		_controller = 0;
	}
	if (_joystick) {
		SDL_JoystickClose(_joystick);
		_joystick = 0;
	}
	SDL_Quit();
}

static void sdl2_set_screen_size(int w, int h, const char *caption, int scale, const char *filter, bool fullscreen) {
	assert(_screen_w == 0 && _screen_h == 0); // abort if called more than once
	_screen_w = w;
	_screen_h = h;
	if (!filter || strcmp(filter, "nearest") == 0) {
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
	} else if (strcmp(filter, "linear") == 0) {
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
	} else {
		print_warning("Unhandled filter '%s'", filter);
	}
	const int window_w = w * scale;
	const int window_h = h * scale;
	const int flags = fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_RESIZABLE;
	_window = SDL_CreateWindow(caption, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_w, window_h, flags);
	if (!_window) {
		print_error("Failed to create SDL window: %s", SDL_GetError());
	}
	_renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_PRESENTVSYNC);
	if (!_renderer) {
		print_warning("VSync renderer unavailable: %s", SDL_GetError());
		_renderer = SDL_CreateRenderer(_window, -1, 0);
	}
	if (!_renderer) {
		print_error("Failed to create SDL renderer: %s", SDL_GetError());
	}
	SDL_RenderSetLogicalSize(_renderer, w, h);
	print_debug(DBG_SYSTEM, "set_screen_size %d,%d", _screen_w, _screen_h);
	_screen_buffer = (uint32_t *)calloc(_screen_w * _screen_h, sizeof(uint32_t));
	if (!_screen_buffer) {
		print_error("Failed to allocate screen buffer");
	}
	static const uint32_t pfmt = SDL_PIXELFORMAT_RGB888;
	_texture = SDL_CreateTexture(_renderer, pfmt, SDL_TEXTUREACCESS_STREAMING, _screen_w, _screen_h);
	_fmt = SDL_AllocFormat(pfmt);
	_sprites_cliprect.x = 0;
	_sprites_cliprect.y = 0;
	_sprites_cliprect.w = w;
	_sprites_cliprect.h = h;
}

static bool sdl2_resize_viewport(int w, int h) {
	uint32_t *buffer = calloc((size_t)w * h, sizeof(*buffer));
	SDL_Texture *texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_RGB888,
		SDL_TEXTUREACCESS_STREAMING, w, h);
	if (!buffer || !texture) {
		free(buffer);
		if (texture) SDL_DestroyTexture(texture);
		print_warning("Unable to resize viewport to %dx%d: %s", w, h, SDL_GetError());
		return false;
	}
	if (SDL_RenderSetLogicalSize(_renderer, w, h) != 0) {
		free(buffer);
		SDL_DestroyTexture(texture);
		return false;
	}
	SDL_DestroyTexture(_texture);
	free(_screen_buffer);
	_texture = texture;
	_screen_buffer = buffer;
	_screen_w = w;
	_screen_h = h;
	_scroll_interpolation_active = false;
	_render_schedule_initialized = false;
	return true;
}

static void sdl2_set_scroll_render_rate(int rate) {
	_scroll_rate = rate;
	_render_schedule_initialized = false;
}

static void sdl2_set_scroll_interpolation(int dx, int dy, int simulation_period, bool enabled) {
	const bool active = enabled && _scroll_rate > 30 && simulation_period > 0;
	const uint64_t now = active ? SDL_GetPerformanceCounter() : 0;
	if (active && !_scroll_interpolation_active) {
		_scroll_target_x = _scroll_target_y = 0.;
		_scroll_previous_x = _scroll_previous_y = 0.;
		_render_schedule_origin = now;
		_next_render_counter = _render_schedule_origin;
		_render_schedule_initialized = true;
	}
	if (active) {
		_scroll_previous_x = _scroll_target_x;
		_scroll_previous_y = _scroll_target_y;
		_scroll_target_x += dx;
		_scroll_target_y += dy;
		_scroll_update_timestamp = now;
		_scroll_period_counter = SDL_GetPerformanceFrequency() * (uint64_t)simulation_period / 1000;
		if (_scroll_period_counter == 0) {
			_scroll_period_counter = 1;
		}
	} else {
		_render_schedule_initialized = false;
	}
	_scroll_interpolation_active = active;
}

static uint32_t convert_amiga_color(uint16_t color) {
	uint8_t r = (color >> 8) & 15;
	r |= r << 4;
	uint8_t g = (color >> 4) & 15;
	g |= g << 4;
	uint8_t b =  color       & 15;
	b |= b << 4;
	return SDL_MapRGB(_fmt, r, g, b);
}

static void set_amiga_color(uint16_t color, SDL_Color *p) {
	const uint8_t r = (color >> 8) & 15;
	p->r = (r << 4) | r;
	const uint8_t g = (color >> 4) & 15;
	p->g = (g << 4) | g;
	const uint8_t b =  color       & 15;
	p->b = (b << 4) | b;
}

static void sdl2_set_palette_amiga(const uint16_t *colors, int offset) {
	SDL_Color *palette_colors = &_palette->colors[offset];
	for (int i = 0; i < 16; ++i) {
		_screen_palette[offset + i] = convert_amiga_color(colors[i]);
		set_amiga_color(colors[i], &palette_colors[i]);
	}
}

static void sdl2_set_copper_bars(const uint16_t *data) {
	if (!data) {
		_copper_color_key = -1;
	} else {
		_copper_color_key = (data[0] - 0x180) / 2;
		const uint16_t *src = data + 1;
		uint32_t *dst = _copper_palette;
		for (int i = 0; i < COPPER_BARS_H / 5; ++i) {
			const int j = i + 1;
			*dst++ = convert_amiga_color(src[j]);
			*dst++ = convert_amiga_color(src[i]);
			*dst++ = convert_amiga_color(src[j]);
			*dst++ = convert_amiga_color(src[i]);
			*dst++ = convert_amiga_color(src[j]);
		}
        }
}

static void sdl2_set_screen_palette(const uint8_t *colors, int offset, int count, int depth) {
	SDL_Color *palette_colors = &_palette->colors[offset];
	const int shift = 8 - depth;
	for (int i = 0; i < count; ++i) {
		int r = *colors++;
		int g = *colors++;
		int b = *colors++;
		if (depth != 8) {
			r = (r << shift) | (r >> (depth - shift));
			g = (g << shift) | (g >> (depth - shift));
			b = (b << shift) | (b >> (depth - shift));
		}
		_screen_palette[offset + i] = SDL_MapRGB(_fmt, r, g, b);
		palette_colors[i].r = r;
		palette_colors[i].g = g;
		palette_colors[i].b = b;
	}
	for (int i = 0; i < ARRAYSIZE(_spritesheets); ++i) {
		struct spritesheet_t *sheet = &_spritesheets[i];
		if (sheet->surface) {
			SDL_DestroyTexture(sheet->texture);
			sheet->texture = SDL_CreateTextureFromSurface(_renderer, sheet->surface);
		}
	}
}

static void sdl2_set_palette_color(int i, const uint8_t *colors) {
	int r = colors[0];
	r = (r << 2) | (r >> 4);
	int g = colors[1];
	g = (g << 2) | (g >> 4);
	int b = colors[2];
	b = (b << 2) | (b >> 4);
	_screen_palette[i] = SDL_MapRGB(_fmt, r, g, b);
}

static void fade_palette_helper(int in) {
	SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_BLEND);
	SDL_Rect r;
	r.x = r.y = 0;
	SDL_GetRendererOutputSize(_renderer, &r.w, &r.h);
	for (int i = 0; i <= FADE_STEPS; ++i) {
		int alpha = 255 * i / FADE_STEPS;
		if (in) {
			alpha = 255 - alpha;
		}
		SDL_SetRenderDrawColor(_renderer, 0, 0, 0, alpha);
		SDL_RenderClear(_renderer);
		SDL_RenderCopy(_renderer, _texture, 0, 0);
		SDL_RenderFillRect(_renderer, &r);
		SDL_RenderPresent(_renderer);
		SDL_Delay(30);
	}
}

static void sdl2_fade_in_palette() {
	if (!g_sys.input.quit) {
		fade_palette_helper(1);
	}
}

static void sdl2_fade_out_palette() {
	if (!g_sys.input.quit) {
		fade_palette_helper(0);
	}
}

static void sdl2_transition_screen(enum sys_transition_e type, bool open) {
	const int step_w = _screen_w / FADE_STEPS;
	const int step_h = _screen_h / FADE_STEPS;
	SDL_Rect r;
	r.x = 0;
	r.w = 0;
	r.y = 0;
	r.h = (type == TRANSITION_CURTAIN) ? _screen_h : 0;
	do {
		r.x = (_screen_w - r.w) / 2;
		if (r.x < 0) {
			r.x = 0;
		}
		r.w += step_w;
		if (r.x + r.w > _screen_w) {
			r.w = _screen_w - r.x;
		}
		if (type == TRANSITION_SQUARE) {
			r.y = (_screen_h - r.h) / 2;
			if (r.y < 0) {
				r.y = 0;
			}
			r.h += step_h;
			if (r.y + r.h > _screen_h) {
				r.h = _screen_h - r.y;
			}
		}
		SDL_RenderClear(_renderer);
		SDL_RenderCopy(_renderer, _texture, &r, &r);
		SDL_RenderPresent(_renderer);
		SDL_Delay(30);
	} while (r.x > 0 && (type == TRANSITION_CURTAIN || r.y > 0));
}

static void sdl2_copy_bitmap(const uint8_t *p, int w, int h) {
	if (w != _screen_w || h != _screen_h) {
		memset(_screen_buffer, 0, _screen_w * _screen_h * sizeof(uint32_t));
		const int offset_x = (_screen_w - w) / 2;
		const int offset_y = (_screen_h - h) / 2;
		for (int j = 0; j < h; ++j) {
			for (int i = 0; i < w; ++i) {
				_screen_buffer[(offset_y + j) * _screen_w + (offset_x + i)] = _screen_palette[*p++];
			}
		}
	} else if (_copper_color_key != -1) {
		for (int j = 0; j < _screen_h; ++j) {
			const int color = (j * 200 / _screen_h) / 2;
			if (color < COPPER_BARS_H) {
				const uint32_t line_color = _copper_palette[color];
				for (int i = 0; i < _screen_w; ++i) {
					_screen_buffer[j * _screen_w + i] = (p[i] == _copper_color_key) ? line_color : _screen_palette[p[i]];
				}
			} else {
				for (int i = 0; i < _screen_w; ++i) {
					_screen_buffer[j * _screen_w + i] = _screen_palette[p[i]];
				}
			}
			p += _screen_w;
		}
	} else {
		for (int i = 0; i < _screen_w * _screen_h; ++i) {
			_screen_buffer[i] = _screen_palette[p[i]];
		}
	}
	SDL_UpdateTexture(_texture, 0, _screen_buffer, _screen_w * sizeof(uint32_t));
}

static void sdl2_render_background(float scroll_x, float scroll_y, int clip_x, int clip_y, int clip_w, int clip_h) {
	SDL_Rect clip = { clip_x, clip_y, clip_w, clip_h };
	SDL_RenderSetClipRect(_renderer, &clip);
	SDL_FRect dest = { _shake_dx + scroll_x, _shake_dy + scroll_y, _screen_w, _screen_h };
	SDL_RenderCopyF(_renderer, _texture, 0, &dest);
}

static void sdl2_draw_screen(float scroll_x, float scroll_y) {
	const bool smooth_scroll = scroll_x != 0.f || scroll_y != 0.f;
	SDL_RenderClear(_renderer);
	if (!smooth_scroll) {
		SDL_Rect r = { _shake_dx, _shake_dy, _screen_w, _screen_h };
		SDL_RenderCopy(_renderer, _texture, 0, &r);
	} else {
		const int world_bottom = _sprites_cliprect.y + _sprites_cliprect.h;
		if (_sprites_cliprect.y > 0) {
			sdl2_render_background(0.f, 0.f, 0, 0, _screen_w, _sprites_cliprect.y);
		}
		if (world_bottom < _screen_h) {
			sdl2_render_background(0.f, 0.f, 0, world_bottom, _screen_w, _screen_h - world_bottom);
		}
		sdl2_render_background(scroll_x, scroll_y,
			_sprites_cliprect.x, _sprites_cliprect.y, _sprites_cliprect.w, _sprites_cliprect.h);
	}

	// sprites
	SDL_Rect clip = _sprites_cliprect;
	SDL_RenderSetClipRect(_renderer, &clip);
	for (int i = 0; i < _sprites_count; ++i) {
		const struct sprite_t *spr = &_sprites[i];
		struct spritesheet_t *sheet = &_spritesheets[spr->sheet];
		if (spr->num >= sheet->count) {
			continue;
		}
		if (!smooth_scroll) {
			SDL_Rect r = { spr->x + _shake_dx, spr->y + _shake_dy,
				sheet->r[spr->num].w, sheet->r[spr->num].h };
			if (!spr->xflip) {
				SDL_RenderCopy(_renderer, sheet->texture, &sheet->r[spr->num], &r);
			} else {
				SDL_RenderCopyEx(_renderer, sheet->texture, &sheet->r[spr->num], &r, 0., 0, SDL_FLIP_HORIZONTAL);
			}
		} else {
			SDL_FRect r = { spr->x + _shake_dx + scroll_x, spr->y + _shake_dy + scroll_y,
				sheet->r[spr->num].w, sheet->r[spr->num].h };
			if (!spr->xflip) {
				SDL_RenderCopyF(_renderer, sheet->texture, &sheet->r[spr->num], &r);
			} else {
				SDL_RenderCopyExF(_renderer, sheet->texture, &sheet->r[spr->num], &r, 0., 0, SDL_FLIP_HORIZONTAL);
			}
		}
	}
	SDL_RenderSetClipRect(_renderer, 0);

	SDL_RenderPresent(_renderer);
}

static void sdl2_present_at(uint64_t timestamp) {
	double progress = (double)(timestamp - _scroll_update_timestamp) / _scroll_period_counter;
	if (progress > 1.) {
		progress = 1.;
	}
	const double display_x = _scroll_previous_x + (_scroll_target_x - _scroll_previous_x) * progress;
	const double display_y = _scroll_previous_y + (_scroll_target_y - _scroll_previous_y) * progress;
	sdl2_draw_screen(
		(float)(_scroll_target_x - display_x),
		(float)(_scroll_target_y - display_y));
}

static void sdl2_update_screen() {
	if (!_scroll_interpolation_active) {
		sdl2_draw_screen(0.f, 0.f);
		return;
	}
	const uint64_t now = SDL_GetPerformanceCounter();
	if (now < _next_render_counter) {
		return;
	}
	const uint64_t frequency = SDL_GetPerformanceFrequency();
	const uint64_t elapsed = now - _render_schedule_origin;
	const uint64_t elapsed_seconds = elapsed / frequency;
	const uint64_t elapsed_remainder = elapsed % frequency;
	const uint64_t frames_elapsed = elapsed_seconds * _scroll_rate
		+ (elapsed_remainder * _scroll_rate) / frequency;
	const uint64_t next_frame = frames_elapsed + 1;
	_next_render_counter = _render_schedule_origin
		+ (next_frame / _scroll_rate) * frequency
		+ ((next_frame % _scroll_rate) * frequency) / _scroll_rate;
	sdl2_present_at(now);
}

static void sdl2_shake_screen(int dx, int dy) {
	_shake_dx = dx;
	_shake_dy = dy;
}

static void handle_keyevent(int keysym, bool keydown, struct input_t *input, bool *paused) {
	switch (keysym) {
	case SDLK_LEFT:
		if (keydown) {
			input->direction |= INPUT_DIRECTION_LEFT;
		} else {
			input->direction &= ~INPUT_DIRECTION_LEFT;
		}
		break;
	case SDLK_RIGHT:
		if (keydown) {
			input->direction |= INPUT_DIRECTION_RIGHT;
		} else {
			input->direction &= ~INPUT_DIRECTION_RIGHT;
		}
		break;
	case SDLK_UP:
		if (keydown) {
			input->direction |= INPUT_DIRECTION_UP;
		} else {
			input->direction &= ~INPUT_DIRECTION_UP;
		}
		break;
	case SDLK_DOWN:
		if (keydown) {
			input->direction |= INPUT_DIRECTION_DOWN;
		} else {
			input->direction &= ~INPUT_DIRECTION_DOWN;
		}
		break;
	case SDLK_RETURN:
	case SDLK_SPACE:
		input->space = keydown;
		break;
	case SDLK_LSHIFT:
	case SDLK_RSHIFT:
		input->jump = keydown;
		break;
	case SDLK_ESCAPE:
		if (keydown) {
			g_sys.input.quit = true;
		}
		break;
	case SDLK_1:
		input->digit1 = keydown;
		break;
	case SDLK_2:
		input->digit2 = keydown;
		break;
	case SDLK_3:
		input->digit3 = keydown;
		break;
	case SDLK_MINUS:
	case SDLK_KP_MINUS:
		if (keydown) {
			input->view_step = MIN(8, input->view_step + 1);
		}
		break;
	case SDLK_EQUALS:
	case SDLK_PLUS:
	case SDLK_KP_PLUS:
		if (keydown) {
			input->view_step = MAX(0, input->view_step - 1);
		}
		break;
	case SDLK_0:
		if (keydown) {
			input->view_step = 0;
		}
		break;
	case SDLK_F6:
		if (!keydown) input->switch_mode = true;
		break;
	case SDLK_p:
		if (!keydown) {
			*paused = !*paused;
		}
		break;
	}
}

static void handle_controlleraxis(int axis, int value, struct input_t *input) {
	static const int THRESHOLD = 3200;
	switch (axis) {
	case SDL_CONTROLLER_AXIS_LEFTX:
	case SDL_CONTROLLER_AXIS_RIGHTX:
		if (value < -THRESHOLD) {
			input->direction |= INPUT_DIRECTION_LEFT;
		} else {
			input->direction &= ~INPUT_DIRECTION_LEFT;
		}
		if (value > THRESHOLD) {
			input->direction |= INPUT_DIRECTION_RIGHT;
		} else {
			input->direction &= ~INPUT_DIRECTION_RIGHT;
		}
		break;
	case SDL_CONTROLLER_AXIS_LEFTY:
	case SDL_CONTROLLER_AXIS_RIGHTY:
		if (value < -THRESHOLD) {
			input->direction |= INPUT_DIRECTION_UP;
		} else {
			input->direction &= ~INPUT_DIRECTION_UP;
		}
		if (value > THRESHOLD) {
			input->direction |= INPUT_DIRECTION_DOWN;
		} else {
			input->direction &= ~INPUT_DIRECTION_DOWN;
		}
		break;
	}
}

static void handle_controllerbutton(int button, bool pressed, struct input_t *input, bool *paused) {
	switch (button) {
	case SDL_CONTROLLER_BUTTON_A:
		input->space = pressed;
		break;
	case SDL_CONTROLLER_BUTTON_B:
		input->jump = pressed;
		break;
	case SDL_CONTROLLER_BUTTON_START:
		if (!pressed) {
			*paused = !*paused;
		}
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_UP:
		if (pressed) {
			input->direction |= INPUT_DIRECTION_UP;
		} else {
			input->direction &= ~INPUT_DIRECTION_UP;
		}
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
		if (pressed) {
			input->direction |= INPUT_DIRECTION_DOWN;
		} else {
			input->direction &= ~INPUT_DIRECTION_DOWN;
		}
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
		if (pressed) {
			input->direction |= INPUT_DIRECTION_LEFT;
		} else {
			input->direction &= ~INPUT_DIRECTION_LEFT;
		}
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
		if (pressed) {
			input->direction |= INPUT_DIRECTION_RIGHT;
		} else {
			input->direction &= ~INPUT_DIRECTION_RIGHT;
		}
		break;
	}
}

static void handle_joystickhatmotion(int value, struct input_t *input) {
	input->direction = 0;
	if (value & SDL_HAT_UP) {
		input->direction |= INPUT_DIRECTION_UP;
	}
	if (value & SDL_HAT_DOWN) {
		input->direction |= INPUT_DIRECTION_DOWN;
	}
	if (value & SDL_HAT_LEFT) {
		input->direction |= INPUT_DIRECTION_LEFT;
	}
	if (value & SDL_HAT_RIGHT) {
		input->direction |= INPUT_DIRECTION_RIGHT;
	}
}

static void handle_joystickaxismotion(int axis, int value, struct input_t *input) {
	static const int THRESHOLD = 3200;
	switch (axis) {
	case 0:
		input->direction &= ~(INPUT_DIRECTION_RIGHT | INPUT_DIRECTION_LEFT);
		if (value > THRESHOLD) {
			input->direction |= INPUT_DIRECTION_RIGHT;
		} else if (value < -THRESHOLD) {
			input->direction |= INPUT_DIRECTION_LEFT;
		}
		break;
	case 1:
		input->direction &= ~(INPUT_DIRECTION_UP | INPUT_DIRECTION_DOWN);
		if (value > THRESHOLD) {
			input->direction |= INPUT_DIRECTION_DOWN;
		} else if (value < -THRESHOLD) {
			input->direction |= INPUT_DIRECTION_UP;
		}
		break;
	}
}

static void handle_joystickbutton(int button, int pressed, struct input_t *input) {
	switch (button) {
	case 0:
		input->space = pressed;
		break;
	case 1:
		input->jump = pressed;
		break;
	}
}

static int handle_event(const SDL_Event *ev, bool *paused) {
	switch (ev->type) {
	case SDL_QUIT:
		g_sys.input.quit = true;
		break;
	case SDL_WINDOWEVENT:
		switch (ev->window.event) {
		case SDL_WINDOWEVENT_FOCUS_GAINED:
		case SDL_WINDOWEVENT_FOCUS_LOST:
			*paused = (ev->window.event == SDL_WINDOWEVENT_FOCUS_LOST);
			break;
		}
		break;
	case SDL_KEYUP:
		handle_keyevent(ev->key.keysym.sym, 0, &g_sys.input, paused);
		break;
	case SDL_KEYDOWN:
		handle_keyevent(ev->key.keysym.sym, 1, &g_sys.input, paused);
		break;
	case SDL_CONTROLLERDEVICEADDED:
		if (!_controller) {
			_controller = SDL_GameControllerOpen(ev->cdevice.which);
			if (_controller) {
				fprintf(stdout, "Using controller '%s'\n", SDL_GameControllerName(_controller));
			}
		}
		break;
	case SDL_CONTROLLERDEVICEREMOVED:
		if (_controller == SDL_GameControllerFromInstanceID(ev->cdevice.which)) {
			fprintf(stdout, "Removed controller '%s'\n", SDL_GameControllerName(_controller));
			SDL_GameControllerClose(_controller);
			_controller = 0;
		}
		break;
	case SDL_CONTROLLERBUTTONUP:
		if (_controller) {
			handle_controllerbutton(ev->cbutton.button, 0, &g_sys.input, paused);
		}
		break;
	case SDL_CONTROLLERBUTTONDOWN:
		if (_controller) {
			handle_controllerbutton(ev->cbutton.button, 1, &g_sys.input, paused);
		}
		break;
	case SDL_CONTROLLERAXISMOTION:
		if (_controller) {
			handle_controlleraxis(ev->caxis.axis, ev->caxis.value, &g_sys.input);
		}
		break;
	case SDL_JOYHATMOTION:
		if (_joystick) {
			handle_joystickhatmotion(ev->jhat.value, &g_sys.input);
		}
		break;
	case SDL_JOYAXISMOTION:
		if (_joystick) {
			handle_joystickaxismotion(ev->jaxis.axis, ev->jaxis.value, &g_sys.input);
		}
		break;
	case SDL_JOYBUTTONUP:
		if (_joystick) {
			handle_joystickbutton(ev->jbutton.button, 0, &g_sys.input);
		}
		break;
	case SDL_JOYBUTTONDOWN:
		if (_joystick) {
			handle_joystickbutton(ev->jbutton.button, 1, &g_sys.input);
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static void sdl2_process_events() {
	bool paused = false;
	while (1) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			const bool prev = paused;
			handle_event(&ev, &paused);
			if (g_sys.input.quit) {
				break;
			}
			if (prev != paused) {
				SDL_PauseAudio(paused);
			}
		}
		if (!paused) {
			break;
		}
		SDL_Delay(100);
	}
}

static void sdl2_sleep(int duration) {
	if (!_scroll_interpolation_active) {
		SDL_Delay(duration);
		return;
	}
	const uint32_t end = SDL_GetTicks() + duration;
	while ((int32_t)(end - SDL_GetTicks()) > 0) {
		const uint64_t now = SDL_GetPerformanceCounter();
		if (now >= _next_render_counter) {
			sdl2_update_screen();
			continue;
		}
		const int until_end = (int32_t)(end - SDL_GetTicks());
		const uint64_t until_render_counter = _next_render_counter - now;
		const uint64_t frequency = SDL_GetPerformanceFrequency();
		const int until_render = (int)((until_render_counter * 1000) / frequency);
		const int delay = MIN(until_end, MAX(1, until_render));
		if (delay <= 0) {
			break;
		}
		SDL_Delay(delay);
	}
}

static uint32_t sdl2_get_timestamp() {
	return SDL_GetTicks();
}

static void sdl2_start_audio(sys_audio_cb callback, void *param) {
	SDL_AudioSpec desired;
	memset(&desired, 0, sizeof(desired));
	desired.freq = SYS_AUDIO_FREQ;
	desired.format = AUDIO_S16;
	desired.channels = 1;
	/* Keep roughly 93ms of scheduling headroom at 44.1kHz, matching the
	 * previous 2048/22050 buffer while avoiding low-rate OPL downsampling. */
	desired.samples = 4096;
	desired.callback = callback;
	desired.userdata = param;
	if (SDL_OpenAudio(&desired, 0) == 0) {
		SDL_PauseAudio(0);
	} else {
		print_warning("Failed to open SDL audio device: %s", SDL_GetError());
	}
}

static void sdl2_stop_audio() {
	SDL_CloseAudio();
}

static void sdl2_lock_audio() {
	SDL_LockAudio();
}

static void sdl2_unlock_audio() {
	SDL_UnlockAudio();
}

static void render_load_sprites(int spr_type, int count, const struct sys_rect_t *r, const uint8_t *data, int w, int h, uint8_t color_key, bool update_pal) {
	assert(spr_type < ARRAYSIZE(_spritesheets));
	struct spritesheet_t *sheet = &_spritesheets[spr_type];
	sheet->count = count;
	sheet->r = (SDL_Rect *)malloc(count * sizeof(SDL_Rect));
	for (int i = 0; i < count; ++i) {
		SDL_Rect *rect = &sheet->r[i];
		rect->x = r[i].x;
		rect->y = r[i].y;
		rect->w = r[i].w;
		rect->h = r[i].h;
	}
	SDL_Surface *surface = SDL_CreateRGBSurface(0, w, h, 8, 0x0, 0x0, 0x0, 0x0);
	SDL_SetSurfacePalette(surface, _palette);
	SDL_SetColorKey(surface, 1, color_key);
	SDL_LockSurface(surface);
	for (int y = 0; y < h; ++y) {
		memcpy(((uint8_t *)surface->pixels) + y * surface->pitch, data + y * w, w);
	}
	SDL_UnlockSurface(surface);
	sheet->texture = SDL_CreateTextureFromSurface(_renderer, surface);
	if (update_pal) { /* update texture on palette change */
		sheet->surface = surface;
	} else  {
		SDL_FreeSurface(surface);
	}
}

static void render_unload_sprites(int spr_type) {
	struct spritesheet_t *sheet = &_spritesheets[spr_type];
	free(sheet->r);
	if (sheet->surface) {
		SDL_FreeSurface(sheet->surface);
	}
	if (sheet->texture) {
		SDL_DestroyTexture(sheet->texture);
	}
	memset(sheet, 0, sizeof(struct spritesheet_t));
}

static void render_add_sprite(int spr_type, int frame, int x, int y, int xflip) {
	if (_sprites_count == _sprites_capacity) {
		if (_sprites_capacity > INT_MAX / 2) {
			print_error("Too many sprites");
		}
		const int capacity = _sprites_capacity ? _sprites_capacity * 2 : MAX_SPRITES;
		struct sprite_t *sprites = (struct sprite_t *)realloc(_sprites, (size_t)capacity * sizeof(*sprites));
		if (!sprites) {
			print_error("Failed to allocate sprite list (%d entries)", capacity);
		}
		_sprites = sprites;
		_sprites_capacity = capacity;
	}
	struct sprite_t *spr = &_sprites[_sprites_count];
	spr->sheet = spr_type;
	spr->num = frame;
	spr->x = x;
	spr->y = y;
	spr->xflip = xflip;
	++_sprites_count;
}

static void render_clear_sprites() {
	_sprites_count = 0;
}

static void render_set_sprites_clipping_rect(int x, int y, int w, int h) {
	_sprites_cliprect.x = x;
	_sprites_cliprect.y = y;
	_sprites_cliprect.w = w;
	_sprites_cliprect.h = h;
}

static void print_log(FILE *fp, const char *s) {
}

struct sys_t g_sys = {
	.init = sdl2_init,
	.fini = sdl2_fini,
	.set_screen_size = sdl2_set_screen_size,
	.set_screen_palette = sdl2_set_screen_palette,
	.set_palette_amiga = sdl2_set_palette_amiga,
	.set_copper_bars = sdl2_set_copper_bars,
	.set_palette_color = sdl2_set_palette_color,
	.fade_in_palette = sdl2_fade_in_palette,
	.fade_out_palette = sdl2_fade_out_palette,
	.copy_bitmap = sdl2_copy_bitmap,
	.update_screen = sdl2_update_screen,
	.shake_screen = sdl2_shake_screen,
	.transition_screen = sdl2_transition_screen,
	.process_events = sdl2_process_events,
	.sleep = sdl2_sleep,
	.get_timestamp = sdl2_get_timestamp,
	.start_audio = sdl2_start_audio,
	.stop_audio = sdl2_stop_audio,
	.lock_audio = sdl2_lock_audio,
	.unlock_audio = sdl2_unlock_audio,
	.render_load_sprites = render_load_sprites,
	.render_unload_sprites = render_unload_sprites,
	.render_add_sprite = render_add_sprite,
	.render_clear_sprites = render_clear_sprites,
	.render_set_sprites_clipping_rect = render_set_sprites_clipping_rect,
	.print_log = print_log,
	.set_scroll_interpolation = sdl2_set_scroll_interpolation,
	.set_scroll_render_rate = sdl2_set_scroll_render_rate,
	.resize_viewport = sdl2_resize_viewport,
};
