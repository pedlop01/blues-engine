
#include <SDL.h>
#include <limits.h>
#include "sys.h"
#include "util.h"

#define COPPER_BARS_H 80
#define MAX_SPRITES 256

static const int FADE_STEPS = 16;

struct spritesheet_t {
	int count;
	uint32_t *color_usage;
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
	int owner, part;
	float anchor_x, anchor_y;
	float previous_anchor_x, previous_anchor_y;
	bool interpolate;
};

static struct sprite_t *_sprites;
static int _sprites_count, _sprites_capacity;
static struct sprite_t *_previous_sprites;
static int _previous_sprites_count, _previous_sprites_capacity;
static int _sprite_owner = -1, _sprite_part;
static SDL_Rect _sprites_cliprect;
static int _hud_width, _hud_height, _hud_top, _hud_bottom;
static SDL_Texture *_hud_texture_top, *_hud_texture_bottom;
static void sdl2_draw_hud();
static void sdl2_begin_scene();
static void sdl2_end_scene();
static int _world_offset_x, _world_offset_y;
static bool _pause_menu_enabled, _pause_menu_open;
static bool _rewind_active, _rewind_key, _rewind_button;
static int _rewind_frames;
static SDL_Texture *_rewind_texture;
static bool sdl2_begin_rewind_scene();
static void sdl2_draw_rewind(bool captured);
static void sdl2_set_rewind(bool active, int frames);
static bool _window_has_focus = true;
static bool _blocked_keys[SDL_NUM_SCANCODES];
static bool _blocked_buttons[SDL_CONTROLLER_BUTTON_MAX];
static bool _blocked_axes[SDL_CONTROLLER_AXIS_MAX];
static bool _blocked_joy_buttons[32], _blocked_joy_axes[2], _blocked_hat;
static void sdl2_draw_pause_menu();
static void sdl2_draw_assist_status();
static void allow_preferences_reset();
static bool handle_bound_key(int key, bool down, struct input_t *input);
static bool handle_bound_button(int button, bool down, struct input_t *input);

static int _screen_w, _screen_h;
static SDL_Texture *_scroll_texture, *_scroll_copper_texture, *_scroll_backdrop_texture;
static uint32_t *_scroll_pixels;
static int _scroll_margin, _scroll_width, _scroll_height;
static void sdl2_clear_scroll_background() {
	SDL_DestroyTexture(_scroll_backdrop_texture);_scroll_backdrop_texture=NULL;
	SDL_DestroyTexture(_scroll_texture);_scroll_texture=NULL;
	SDL_DestroyTexture(_scroll_copper_texture);_scroll_copper_texture=NULL;
	free(_scroll_pixels);_scroll_pixels=NULL;
	_scroll_margin=_scroll_width=_scroll_height=0;
}
static int _shake_dx, _shake_dy;
static int _scroll_rate = 30;
static double _scroll_target_x, _scroll_target_y;
static double _scroll_previous_x, _scroll_previous_y;
static uint64_t _scroll_update_timestamp, _scroll_period_counter;
static uint64_t _render_schedule_origin, _next_render_counter;
static bool _scroll_interpolation_active, _render_schedule_initialized;
static SDL_Window *_window;
static SDL_Renderer *_renderer;
static SDL_Texture *_texture, *_save_preview_texture;
static bool _save_preview_capture;
static SDL_PixelFormat *_fmt;
static SDL_Palette *_palette;
static uint32_t _screen_palette[256];
static uint32_t *_screen_buffer;
static int _copper_color_key;
static uint32_t _copper_palette[COPPER_BARS_H];

static SDL_GameController *_controller;
static SDL_Joystick *_joystick;

#ifdef TITUS_MANAGED
#include "engines/blues-family/native.h"
static bool _managed_return_pending;
static void remember_held_controls();
#endif
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
	sdl2_clear_scroll_background();
	SDL_DestroyTexture(_rewind_texture);_rewind_texture=NULL;
	SDL_DestroyTexture(_save_preview_texture);_save_preview_texture=NULL;
	SDL_DestroyTexture(_hud_texture_top);
	SDL_DestroyTexture(_hud_texture_bottom);
	_hud_texture_top = _hud_texture_bottom = 0;
	_hud_width = _hud_height = 0;
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
	free(_previous_sprites);
	_previous_sprites = 0;
	_previous_sprites_count = _previous_sprites_capacity = 0;
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
#ifdef TITUS_MANAGED
    SDL_PumpEvents();remember_held_controls();
#endif
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
	_scroll_margin=0;
	_scroll_interpolation_active = false;
	_render_schedule_initialized = false;
	_previous_sprites_count = 0;
	return true;
}

static void sdl2_set_scroll_render_rate(int rate) {
	_scroll_rate = rate;
	_render_schedule_initialized = false;
	_previous_sprites_count = 0;
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
		_previous_sprites_count = 0;
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
		if (_hud_width > 0) {
			sdl2_begin_scene();
			SDL_Rect dst = { -_world_offset_x, -_world_offset_y, _screen_w, _screen_h };
			SDL_RenderCopy(_renderer, _texture, NULL, &dst);
			sdl2_end_scene();
			SDL_RenderSetLogicalSize(_renderer, 0, 0);
			SDL_RenderFillRect(_renderer, NULL);
			SDL_RenderSetLogicalSize(_renderer, _screen_w, _screen_h);
		} else {
			SDL_RenderClear(_renderer);
			SDL_RenderCopy(_renderer, _texture, 0, 0);
			SDL_RenderFillRect(_renderer, &r);
		}
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
		sdl2_begin_scene();
		SDL_Rect dst = r;
		dst.x -= _world_offset_x;
		dst.y -= _world_offset_y;
		SDL_RenderCopy(_renderer, _texture, &r, &dst);
		sdl2_end_scene();
		SDL_RenderPresent(_renderer);
		SDL_Delay(30);
	} while (r.x > 0 && (type == TRANSITION_CURTAIN || r.y > 0));
}

static void sdl2_copy_bitmap(const uint8_t *p, int w, int h) {
	_scroll_margin=0; /* A new frame must explicitly supply its own border. */
	if (w != _screen_w || h != _screen_h) {
		memset(_screen_buffer, 0, _screen_w * _screen_h * sizeof(uint32_t));
        /* Center/crop safely even during native-resolution transitions. */
        const int src_x=MAX(0,(w-_screen_w)/2),src_y=MAX(0,(h-_screen_h)/2);
        const int dst_x=MAX(0,(_screen_w-w)/2),dst_y=MAX(0,(_screen_h-h)/2);
        const int copy_w=MIN(w,_screen_w),copy_h=MIN(h,_screen_h);
        for(int y=0;y<copy_h;++y)for(int x=0;x<copy_w;++x)
            _screen_buffer[(dst_y+y)*_screen_w+dst_x+x]=_screen_palette[p[(src_y+y)*w+src_x+x]];
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
	if (_hud_width > 0 && _screen_w >= _hud_width) {
		const int x = (_screen_w - _hud_width) / 2;
		if(_hud_top>0)SDL_UpdateTexture(_hud_texture_top, 0, _screen_buffer + x,
			_screen_w * sizeof(uint32_t));
		SDL_UpdateTexture(_hud_texture_bottom, 0,
			_screen_buffer + (_screen_h - _hud_bottom) * _screen_w + x,
			_screen_w * sizeof(uint32_t));
	}
}

static void sdl2_upload_scroll_background(const uint8_t *pixels,int margin,bool transparent_zero) {
	if(!pixels || margin<=0){sdl2_clear_scroll_background();return;}
	const int w=_screen_w+margin*2,h=_sprites_cliprect.h+margin*2;
	if(w!=_scroll_width || h!=_scroll_height) {
		sdl2_clear_scroll_background();
		_scroll_texture=SDL_CreateTexture(_renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,w,h);
		_scroll_copper_texture=SDL_CreateTexture(_renderer,SDL_PIXELFORMAT_RGB888,SDL_TEXTUREACCESS_STREAMING,1,_screen_h);
		_scroll_pixels=malloc((size_t)w*h*4);
		if(!_scroll_texture || !_scroll_copper_texture || !_scroll_pixels){sdl2_clear_scroll_background();return;}
		SDL_SetTextureBlendMode(_scroll_texture,SDL_BLENDMODE_BLEND);
		_scroll_width=w;_scroll_height=h;
	}
	for(int i=0;i<w*h;++i)_scroll_pixels[i]=(pixels[i]==_copper_color_key || (transparent_zero && pixels[i]==0)) ? 0 : (0xff000000u|_screen_palette[pixels[i]]);
	SDL_UpdateTexture(_scroll_texture,NULL,_scroll_pixels,w*4);
	for(int y=0;y<_screen_h;++y) {
		const int line=MAX(0,MIN(COPPER_BARS_H-1,(y-_sprites_cliprect.y)*COPPER_BARS_H/MAX(1,_sprites_cliprect.h)));
		_scroll_pixels[y]=_copper_color_key<0 ? 0 : (line<COPPER_BARS_H ? _copper_palette[line] : _screen_palette[_copper_color_key]);
	}
	SDL_UpdateTexture(_scroll_copper_texture,NULL,_scroll_pixels,4);
	_scroll_margin=margin;
}

static void sdl2_set_scroll_background(const uint8_t *pixels,int margin) {
    SDL_DestroyTexture(_scroll_backdrop_texture);_scroll_backdrop_texture=NULL;
    sdl2_upload_scroll_background(pixels,margin,false);
}
static void sdl2_set_scroll_layers(const uint8_t *tiles,const uint8_t *backdrop,int margin){
    sdl2_upload_scroll_background(tiles,margin,true);
    if(!_scroll_margin)return;
    if(!_scroll_backdrop_texture)_scroll_backdrop_texture=SDL_CreateTexture(_renderer,SDL_PIXELFORMAT_RGB888,SDL_TEXTUREACCESS_STREAMING,_screen_w,_sprites_cliprect.h);
    if(!_scroll_backdrop_texture)print_error("Unable to allocate stationary backdrop texture");
    for(int i=0;i<_screen_w*_sprites_cliprect.h;++i)_scroll_pixels[i]=_screen_palette[backdrop[i]];
    SDL_UpdateTexture(_scroll_backdrop_texture,NULL,_scroll_pixels,_screen_w*4);
}

static void sdl2_render_background(float scroll_x, float scroll_y, int clip_x, int clip_y, int clip_w, int clip_h) {
	SDL_Rect clip = { clip_x - _world_offset_x, clip_y - _world_offset_y, clip_w, clip_h };
	SDL_RenderSetClipRect(_renderer, &clip);
	if(_scroll_margin && clip_y==_sprites_cliprect.y && clip_h==_sprites_cliprect.h) {
		const SDL_Rect src={0,clip_y,1,clip_h};
		const SDL_FRect base={clip_x-_world_offset_x,clip_y-_world_offset_y,clip_w,clip_h};
		if(_scroll_backdrop_texture)SDL_RenderCopyF(_renderer,_scroll_backdrop_texture,NULL,&base);
		else SDL_RenderCopyF(_renderer,_scroll_copper_texture,&src,&base);
		const SDL_FRect world={_shake_dx+scroll_x-_scroll_margin-_world_offset_x,
			_shake_dy+scroll_y+_sprites_cliprect.y-_scroll_margin-_world_offset_y,_scroll_width,_scroll_height};
		SDL_RenderCopyF(_renderer,_scroll_texture,NULL,&world);
		return;
	}
	SDL_FRect dest = { _shake_dx + scroll_x - _world_offset_x, _shake_dy + scroll_y - _world_offset_y, _screen_w, _screen_h };
	SDL_RenderCopyF(_renderer, _texture, 0, &dest);
}

static SDL_FRect sdl2_sprite_rect(const struct sprite_t *spr, float scroll_x, float scroll_y, float progress) {
	const SDL_Rect *frame = &_spritesheets[spr->sheet].r[spr->num];
	SDL_FRect r = { spr->x + _shake_dx, spr->y + _shake_dy, frame->w, frame->h };
	if (_scroll_interpolation_active && spr->interpolate) {
		/* Anchors are screen positions from their respective simulation
		 * frames: their delta already includes camera motion. Do not add
		 * scroll_x/y again. Keep the current animation's foot/center anchor. */
		r.x += (spr->previous_anchor_x - spr->anchor_x) * (1.f - progress);
		r.y += (spr->previous_anchor_y - spr->anchor_y) * (1.f - progress);
	} else {
		r.x += scroll_x;
		r.y += scroll_y;
	}
	return r;
}

static void sdl2_get_screen_palette(uint8_t *rgb) {
    for(int i=0;i<256;++i)SDL_GetRGB(_screen_palette[i],_fmt,rgb+i*3,rgb+i*3+1,rgb+i*3+2);
}

static void sdl2_set_hud_layout(int width, int height, int top, int bottom) {
	if (width > 0 && (width != _hud_width || top != _hud_top || bottom != _hud_bottom)) {
		SDL_DestroyTexture(_hud_texture_top);
		SDL_DestroyTexture(_hud_texture_bottom);
		_hud_texture_top = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_RGB888,
			SDL_TEXTUREACCESS_STREAMING, width, MAX(1,top));
		_hud_texture_bottom = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_RGB888,
			SDL_TEXTUREACCESS_STREAMING, width, bottom);
		if (!_hud_texture_top || !_hud_texture_bottom) print_error("Failed to allocate HUD textures");
	}
	_hud_width = width;
	_hud_height = height;
	_hud_top = top;
	_hud_bottom = bottom;
}

static void sdl2_draw_hud() {
	if (_hud_width <= 0 || _hud_height <= 0 || _screen_w < _hud_width ||
		_screen_h < _hud_top + _hud_bottom) return;
	/* Reuse the original release's pixels, palette and layout. Only the
	 * presentation scale changes: the HUD keeps its native logical size
	 * while the world has its own zoom. No artwork is replaced or decoded
	 * a second time, and menus keep their normal rendering path. */
	int output_w, output_h;
	SDL_GetRendererOutputSize(_renderer, &output_w, &output_h);
	const float scale_x = (float)output_w / _hud_width;
	const float scale_y = (float)output_h / _hud_height;
	const float scale = scale_x < scale_y ? scale_x : scale_y;
	const float x = (output_w - _hud_width * scale) / 2.f;
	const float y = (output_h - _hud_height * scale) / 2.f;
	SDL_RenderSetLogicalSize(_renderer, 0, 0);
	SDL_FRect dst = { x, y, _hud_width * scale, _hud_top * scale };
	if(_hud_top>0)SDL_RenderCopyF(_renderer, _hud_texture_top, 0, &dst);
	dst.y = y + (_hud_height - _hud_bottom) * scale;
	dst.h = _hud_bottom * scale;
	SDL_RenderCopyF(_renderer, _hud_texture_bottom, 0, &dst);
	SDL_RenderSetLogicalSize(_renderer, _screen_w, _screen_h);
}

/* Fit the complete map view between the native HUD bands. Drawing the HUD
 * over the world used to hide its edges and broke zoom framing. */
static void sdl2_begin_scene() {
	_world_offset_x = _world_offset_y = 0;
	if (_hud_width <= 0) {
		SDL_RenderClear(_renderer);
		return;
	}
	int w, h;
	SDL_GetRendererOutputSize(_renderer, &w, &h);
	const float sx = (float)w / _hud_width, sy = (float)h / _hud_height;
	const float hud_scale = sx < sy ? sx : sy;
	const float available_w = _hud_width * hud_scale;
	const float available_h = (_hud_height - _hud_top - _hud_bottom) * hud_scale;
	const float world_sx = available_w / _sprites_cliprect.w;
	const float world_sy = available_h / _sprites_cliprect.h;
	const float scale = world_sx < world_sy ? world_sx : world_sy;
	SDL_Rect viewport = {
		(int)((w - _sprites_cliprect.w * scale) / 2.f + .5f),
		(int)((h - _hud_height * hud_scale) / 2.f + _hud_top * hud_scale +
			(available_h - _sprites_cliprect.h * scale) / 2.f + .5f),
		(int)(_sprites_cliprect.w * scale + .5f),
		(int)(_sprites_cliprect.h * scale + .5f)
	};
	SDL_RenderSetLogicalSize(_renderer, 0, 0);
	SDL_RenderClear(_renderer);
	SDL_RenderSetViewport(_renderer, &viewport);
	SDL_RenderSetScale(_renderer, scale, scale);
	_world_offset_x = _sprites_cliprect.x;
	_world_offset_y = _sprites_cliprect.y;
}

static void sdl2_end_scene() {
	if (_hud_width <= 0) return;
	SDL_RenderSetScale(_renderer, 1.f, 1.f);
	SDL_RenderSetViewport(_renderer, NULL);
	sdl2_draw_hud();
	_world_offset_x = _world_offset_y = 0;
}

/* Masked foreground pixels still use the live Amiga color register: a
 * copper-key pixel must hide an actor with the gradient, not a flat color. */
static void sdl2_foreground_copper(const struct sprite_t *spr,float scroll_x,float scroll_y,float progress) {
	const struct spritesheet_t *sheet=&_spritesheets[spr->sheet];
	if(spr->sheet!=RENDER_SPR_FG || !sheet->surface || _copper_color_key<0 || _copper_color_key>=32 ||
		!sheet->color_usage || !(sheet->color_usage[spr->num]&(1u<<_copper_color_key)))return;
	const SDL_Rect *source=&sheet->r[spr->num];
	SDL_FRect dest=sdl2_sprite_rect(spr,scroll_x,scroll_y,progress);
	dest.x-=_world_offset_x;dest.y-=_world_offset_y;
	Uint8 r,g,b,a;SDL_GetRenderDrawColor(_renderer,&r,&g,&b,&a);
	for(int y=0;y<source->h;++y) {
		const uint8_t *row=(const uint8_t *)sheet->surface->pixels+(source->y+y)*sheet->surface->pitch+source->x;
		for(int x=0;x<source->w;++x)if(row[x]==_copper_color_key) {
			const int line=MAX(0,MIN(COPPER_BARS_H-1,(int)((dest.y+_world_offset_y+y-_sprites_cliprect.y)*COPPER_BARS_H/MAX(1,_sprites_cliprect.h))));
			const uint32_t color=_copper_palette[line];
			SDL_SetRenderDrawColor(_renderer,(color>>16)&255,(color>>8)&255,color&255,255);
			const SDL_FRect pixel={dest.x+(spr->xflip ? source->w-1-x : x),dest.y+y,1,1};
			SDL_RenderFillRectF(_renderer,&pixel);
		}
	}
	SDL_SetRenderDrawColor(_renderer,r,g,b,a);
}

static void sdl2_draw_screen(float scroll_x, float scroll_y, float progress) {
	const bool rewind_scene = sdl2_begin_rewind_scene();
	const bool smooth_scroll = scroll_x != 0.f || scroll_y != 0.f;
	sdl2_begin_scene();
	if (_hud_width > 0) {
		sdl2_render_background(scroll_x, scroll_y, _sprites_cliprect.x,
			_sprites_cliprect.y, _sprites_cliprect.w, _sprites_cliprect.h);
	} else if (_scroll_margin && !smooth_scroll) {
		SDL_Rect all={0,0,_screen_w,_screen_h};SDL_RenderCopy(_renderer,_texture,NULL,&all);
		sdl2_render_background(0,0,_sprites_cliprect.x,_sprites_cliprect.y,_sprites_cliprect.w,_sprites_cliprect.h);
	} else if (!smooth_scroll) {
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
	clip.x -= _world_offset_x;
	clip.y -= _world_offset_y;
	SDL_RenderSetClipRect(_renderer, &clip);
	for (int i = 0; i < _sprites_count; ++i) {
		const struct sprite_t *spr = &_sprites[i];
		struct spritesheet_t *sheet = &_spritesheets[spr->sheet];
		if (spr->num >= sheet->count) {
			continue;
		}
		const bool smooth_actor = _scroll_interpolation_active && spr->interpolate;
		if (!smooth_scroll && !smooth_actor) {
			SDL_Rect r = { spr->x + _shake_dx - _world_offset_x, spr->y + _shake_dy - _world_offset_y,
				sheet->r[spr->num].w, sheet->r[spr->num].h };
			if (!spr->xflip) {
				SDL_RenderCopy(_renderer, sheet->texture, &sheet->r[spr->num], &r);
			} else {
				SDL_RenderCopyEx(_renderer, sheet->texture, &sheet->r[spr->num], &r, 0., 0, SDL_FLIP_HORIZONTAL);
			}
		} else {
			SDL_FRect r = sdl2_sprite_rect(spr, scroll_x, scroll_y, progress);
			r.x -= _world_offset_x;
			r.y -= _world_offset_y;
			if (!spr->xflip) {
				SDL_RenderCopyF(_renderer, sheet->texture, &sheet->r[spr->num], &r);
			} else {
				SDL_RenderCopyExF(_renderer, sheet->texture, &sheet->r[spr->num], &r, 0., 0, SDL_FLIP_HORIZONTAL);
			}
		}
		sdl2_foreground_copper(spr,scroll_x,scroll_y,progress);
	}
	SDL_RenderSetClipRect(_renderer, 0);
	sdl2_end_scene();
	sdl2_draw_assist_status();
	sdl2_draw_rewind(rewind_scene);
	if (_pause_menu_open) sdl2_draw_pause_menu();

	if (!_save_preview_capture) SDL_RenderPresent(_renderer);
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
		(float)(_scroll_target_y - display_y), (float)progress);
}

static void sdl2_update_screen() {
	if (!_scroll_interpolation_active) {
		sdl2_draw_screen(0.f, 0.f, 1.f);
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
	if (_pause_menu_enabled && keysym == SDLK_r) {
		_rewind_key=keydown;input->rewind=_rewind_key || _rewind_button;return;
	}
	if (_pause_menu_enabled && handle_bound_key(keysym, keydown, input)) return;
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
		if (_pause_menu_enabled) {
			if (!keydown) input->menu = true;
			break;
		}
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
	case SDLK_F7:
		if (!keydown) input->switch_hud = true;
		break;
	case SDLK_F6:
		if (!keydown) input->switch_mode = true;
		break;
	case SDLK_p:
		if (_pause_menu_enabled) {
			if (!keydown) input->menu = true;
			break;
		}
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
	if (_pause_menu_enabled && button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
		_rewind_button=pressed;input->rewind=_rewind_key || _rewind_button;return;
	}
	if (
#ifdef TITUS_MANAGED
        true
#else
        _pause_menu_enabled
#endif
        ) {if(handle_bound_button(button,pressed,input))return;if(button==SDL_CONTROLLER_BUTTON_A||button==SDL_CONTROLLER_BUTTON_B)return;}
	switch (button) {
	case SDL_CONTROLLER_BUTTON_A:
		input->space = pressed;
		break;
	case SDL_CONTROLLER_BUTTON_B:
		input->jump = pressed;
		break;
	case SDL_CONTROLLER_BUTTON_START:
		if (_pause_menu_enabled) {
			if (!pressed) input->menu = true;
			break;
		}
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
#ifdef TITUS_MANAGED
        if(_pause_menu_enabled){_managed_return_pending=true;g_sys.input.menu=true;break;}
#endif
		g_sys.input.quit = true;
		break;
	case SDL_WINDOWEVENT:
		switch (ev->window.event) {
		case SDL_WINDOWEVENT_FOCUS_GAINED:
		case SDL_WINDOWEVENT_FOCUS_LOST:
			*paused = (ev->window.event == SDL_WINDOWEVENT_FOCUS_LOST);
			_window_has_focus = !*paused;
			if (*paused) {_rewind_key=_rewind_button=false;g_sys.input.rewind=false;}
			break;
		}
		break;
	case SDL_KEYUP:
		if (ev->key.keysym.scancode > SDL_SCANCODE_UNKNOWN && _blocked_keys[ev->key.keysym.scancode]) {
			_blocked_keys[ev->key.keysym.scancode] = false;
			break;
		}
		handle_keyevent(ev->key.keysym.sym, 0, &g_sys.input, paused);
		break;
	case SDL_KEYDOWN:
		if (ev->key.keysym.scancode > SDL_SCANCODE_UNKNOWN && _blocked_keys[ev->key.keysym.scancode]) break;
		if (ev->key.repeat && (ev->key.keysym.sym == SDLK_RETURN || ev->key.keysym.sym == SDLK_SPACE ||
			ev->key.keysym.sym == SDLK_LSHIFT || ev->key.keysym.sym == SDLK_RSHIFT)) break;
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
			_rewind_button=false;g_sys.input.rewind=_rewind_key;
		}
		break;
	case SDL_CONTROLLERBUTTONUP:
		if (ev->cbutton.button >= SDL_CONTROLLER_BUTTON_MAX) break;
		if (_blocked_buttons[ev->cbutton.button]) {
			_blocked_buttons[ev->cbutton.button] = false;
			break;
		}
		if (_controller) {
			handle_controllerbutton(ev->cbutton.button, 0, &g_sys.input, paused);
		}
		break;
	case SDL_CONTROLLERBUTTONDOWN:
		if (ev->cbutton.button >= SDL_CONTROLLER_BUTTON_MAX) break;
		if (_blocked_buttons[ev->cbutton.button]) break;
		if (_controller) {
			handle_controllerbutton(ev->cbutton.button, 1, &g_sys.input, paused);
		}
		break;
	case SDL_CONTROLLERAXISMOTION:
		if (ev->caxis.axis >= SDL_CONTROLLER_AXIS_MAX) break;
		if (_blocked_axes[ev->caxis.axis]) {
			if (abs(ev->caxis.value) <= 3200) _blocked_axes[ev->caxis.axis] = false;
			break;
		}
		if (_controller) {
			handle_controlleraxis(ev->caxis.axis, ev->caxis.value, &g_sys.input);
		}
		break;
	case SDL_JOYHATMOTION:
		if (_blocked_hat) {
			if (ev->jhat.value == SDL_HAT_CENTERED) _blocked_hat = false;
			break;
		}
		if (_joystick) {
			handle_joystickhatmotion(ev->jhat.value, &g_sys.input);
		}
		break;
	case SDL_JOYAXISMOTION:
		if (ev->jaxis.axis < 2 && _blocked_joy_axes[ev->jaxis.axis]) {
			if (abs(ev->jaxis.value) <= 3200) _blocked_joy_axes[ev->jaxis.axis] = false;
			break;
		}
		if (_joystick) {
			handle_joystickaxismotion(ev->jaxis.axis, ev->jaxis.value, &g_sys.input);
		}
		break;
	case SDL_JOYBUTTONUP:
		if (ev->jbutton.button < 32 && _blocked_joy_buttons[ev->jbutton.button]) {
			_blocked_joy_buttons[ev->jbutton.button] = false;
			break;
		}
		if (_pause_menu_enabled && ev->jbutton.button == 7 && _joystick) {
			g_sys.input.menu = true;
			break;
		}
		if (_joystick) {
			handle_joystickbutton(ev->jbutton.button, 0, &g_sys.input);
		}
		break;
	case SDL_JOYBUTTONDOWN:
		if (ev->jbutton.button < 32 && _blocked_joy_buttons[ev->jbutton.button]) break;
		if (_joystick) {
			handle_joystickbutton(ev->jbutton.button, 1, &g_sys.input);
		}
		break;
	default:
		return -1;
	}
	return 0;
}

#include "sys_sdl2_menu.h"
#include "sys_sdl2_preferences.h"
#include "sys_sdl2_rewind.h"
#include "sys_sdl2_saves.h"

static void sdl2_process_events() {
#ifdef TITUS_MANAGED
    if(titus_runtime_poll()) {
        if(_pause_menu_enabled){_managed_return_pending=true;g_sys.input.menu=true;}
        else {titus_runtime_return_request(0);g_sys.input.quit=true;}
    }
#endif
	bool paused = false;
	while (1) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			const bool prev = paused;
			handle_event(&ev, &paused);
			if (g_sys.input.quit || g_sys.input.menu) {
				break;
			}
			if (prev != paused) {
				SDL_PauseAudio(paused || _rewind_active);
#ifdef TITUS_MANAGED
                titus_runtime_probe_paused(paused&&ev.type==SDL_KEYUP&&ev.key.keysym.sym==SDLK_p);
#endif
			}
		}
		if (!paused || g_sys.input.quit || g_sys.input.menu) {
			break;
		}
#ifdef TITUS_MANAGED
        if(titus_runtime_poll()) {
            if(_pause_menu_enabled){_managed_return_pending=true;g_sys.input.menu=true;}
            else {titus_runtime_return_request(0);g_sys.input.quit=true;}
        }
#endif
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
	sheet->color_usage=NULL;
	if(spr_type==RENDER_SPR_FG && color_key==0x20) {
		sheet->color_usage=calloc(count,sizeof(*sheet->color_usage));
		if(sheet->color_usage)for(int i=0;i<count;++i)for(int y=0;y<r[i].h;++y)for(int x=0;x<r[i].w;++x) {
			const uint8_t color=data[(r[i].y+y)*w+r[i].x+x];
			if(color<32)sheet->color_usage[i]|=1u<<color;
		}
	}
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
	if (update_pal || (spr_type==RENDER_SPR_FG && color_key==0x20)) { /* retain native mask indices / update palette */
		sheet->surface = surface;
	} else  {
		SDL_FreeSurface(surface);
	}
}

static void render_unload_sprites(int spr_type) {
	/* A replacement atlas may belong to a different DOS/Amiga release. */
	_previous_sprites_count = 0;
	struct spritesheet_t *sheet = &_spritesheets[spr_type];
	free(sheet->color_usage);sheet->color_usage=NULL;
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
	spr->owner = _sprite_owner;
	spr->part = _sprite_part++;
	spr->interpolate = false;
	spr->anchor_x = x;
	spr->anchor_y = y;
	const struct spritesheet_t *sheet = &_spritesheets[spr_type];
	if (frame >= 0 && frame < sheet->count) {
		spr->anchor_x += sheet->r[frame].w / 2.f;
		spr->anchor_y += sheet->r[frame].h;
	}
	if (spr->owner >= 0) {
		for (int i = 0; i < _previous_sprites_count; ++i) {
			const struct sprite_t *previous = &_previous_sprites[i];
			if (previous->owner == spr->owner && previous->part == spr->part && previous->sheet == spr->sheet) {
				spr->previous_anchor_x = previous->anchor_x;
				spr->previous_anchor_y = previous->anchor_y;
				spr->interpolate = true;
				break;
			}
		}
	}
	++_sprites_count;
}

static void sdl2_begin_sprite_frame() {
	if (_sprites_count > _previous_sprites_capacity) {
		struct sprite_t *sprites = realloc(_previous_sprites, (size_t)_sprites_count * sizeof(*sprites));
		if (!sprites) print_error("Failed to allocate sprite interpolation history");
		_previous_sprites = sprites;
		_previous_sprites_capacity = _sprites_count;
	}
	if (_sprites_count) memcpy(_previous_sprites, _sprites, (size_t)_sprites_count * sizeof(*_sprites));
	_previous_sprites_count = _sprites_count;
}

static void sdl2_set_sprite_owner(int owner) {
	_sprite_owner = owner;
	_sprite_part = 0;
}

static void render_clear_sprites() {
	_sprites_count = 0;
	sdl2_set_sprite_owner(-1);
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
	.load_preferences = sdl2_load_preferences,
	.save_preferences = sdl2_save_preferences,
	.read_save_file = sdl2_read_save_file,
	.write_save_file = sdl2_write_save_file,
	.save_file_status = sdl2_save_file_status,
	.capture_save_preview = sdl2_capture_save_preview,
	.fini = sdl2_fini,
	.set_screen_size = sdl2_set_screen_size,
	.set_screen_palette = sdl2_set_screen_palette,
	.set_palette_amiga = sdl2_set_palette_amiga,
	.set_copper_bars = sdl2_set_copper_bars,
	.set_palette_color = sdl2_set_palette_color,
	.fade_in_palette = sdl2_fade_in_palette,
	.fade_out_palette = sdl2_fade_out_palette,
	.copy_bitmap = sdl2_copy_bitmap,
	.set_scroll_background = sdl2_set_scroll_background,
	.update_screen = sdl2_update_screen,
	.shake_screen = sdl2_shake_screen,
	.transition_screen = sdl2_transition_screen,
	.process_events = sdl2_process_events,
	.set_pause_menu_enabled = sdl2_set_pause_menu_enabled,
	.set_assist_status = sdl2_set_assist_status,
	.run_pause_menu = sdl2_run_pause_menu,
	.set_pause_options = sdl2_set_pause_options,
	.set_rewind = sdl2_set_rewind,
	.get_pause_options = sdl2_get_pause_options,
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
	.set_hud_layout = sdl2_set_hud_layout,
	.begin_sprite_frame = sdl2_begin_sprite_frame,
	.set_sprite_owner = sdl2_set_sprite_owner,
	.get_screen_palette = sdl2_get_screen_palette,
	.set_scroll_layers = sdl2_set_scroll_layers,
};

#ifdef TITUS_MANAGED
void titus_native_capture(const char *path){
    int w,h;SDL_GetRendererOutputSize(_renderer,&w,&h);
    SDL_Surface *image=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);
    if(!image)print_error("Cannot allocate probe screenshot");
    if(SDL_RenderReadPixels(_renderer,0,image->format->format,image->pixels,image->pitch)!=0||SDL_SaveBMP(image,path)!=0)print_error("Cannot capture native gameplay");
    SDL_FreeSurface(image);
}
#endif

#ifdef TITUS_MANAGED
void titus_native_set_buttons(int action,int jump){if(action>=0)_game_buttons[0]=action;if(jump>=0)_game_buttons[1]=jump;}
void titus_native_buttons(int *action,int *jump){*action=_game_buttons[0];*jump=_game_buttons[1];}
#endif
