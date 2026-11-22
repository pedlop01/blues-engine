/* Presentation only: rendering noise uses its own deterministic seed, never
 * the simulation RNG. The original DOS/Amiga scene and HUD remain intact. */
static void sdl2_set_rewind(bool active,int frames) {
	const bool ending=_rewind_active && !active;
	_rewind_active=active;_rewind_frames=frames;
	if(active) SDL_PauseAudio(1);
	if(ending) {
		const bool menu=g_sys.input.menu;
		remember_held_controls();clear_pause_input();g_sys.input.menu=menu;
		if(_window_has_focus && !g_sys.input.quit) SDL_PauseAudio(0);
	}
}
static bool sdl2_begin_rewind_scene() {
	if(!_rewind_active || _pause_menu_open || _save_preview_capture)return false;
	int w,h,tw=0,th=0;
	if(SDL_GetRendererOutputSize(_renderer,&w,&h)!=0)return false;
	if(_rewind_texture)SDL_QueryTexture(_rewind_texture,NULL,NULL,&tw,&th);
	if(tw!=w || th!=h) {
		SDL_DestroyTexture(_rewind_texture);
		_rewind_texture=SDL_CreateTexture(_renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_TARGET,w,h);
	}
	if(!_rewind_texture || SDL_SetRenderTarget(_renderer,_rewind_texture)!=0)return false;
	/* Changing render targets resets SDL viewport/scale. Recalculate the
	 * logical transform even when its dimensions did not change. */
	SDL_RenderSetLogicalSize(_renderer,0,0);
	SDL_RenderSetLogicalSize(_renderer,_screen_w,_screen_h);
	return true;
}
static uint32_t rewind_noise(uint32_t *seed) {
	*seed=*seed*1664525u+1013904223u;return *seed;
}
static void sdl2_draw_rewind(bool captured) {
	if(!_rewind_active || _pause_menu_open || _save_preview_capture)return;
	if(captured)SDL_SetRenderTarget(_renderer,NULL);
	int w,h;SDL_GetRendererOutputSize(_renderer,&w,&h);
	Uint8 red,green,blue,alpha;SDL_GetRenderDrawColor(_renderer,&red,&green,&blue,&alpha);
	SDL_BlendMode blend;SDL_GetRenderDrawBlendMode(_renderer,&blend);
	SDL_RenderSetLogicalSize(_renderer,0,0);
	SDL_RenderSetViewport(_renderer,NULL);SDL_RenderSetScale(_renderer,1,1);
	const uint32_t tick=SDL_GetTicks()/33;
	uint32_t noise=0x564853u ^ tick;
	if(captured) {
		SDL_RenderCopy(_renderer,_rewind_texture,NULL,NULL);
		/* Thin displaced strips simulate tape tracking without hiding action. */
		for(int i=0;i<5;++i) {
			const int y=rewind_noise(&noise)%MAX(1,h);
			SDL_Rect src={0,y,w,MIN(MAX(1,h/100),h-y)},dst=src;
			dst.x=(int)(rewind_noise(&noise)%13)-6;
			SDL_RenderCopy(_renderer,_rewind_texture,&src,&dst);
		}
	}
	SDL_SetRenderDrawBlendMode(_renderer,SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(_renderer,8,18,32,22);SDL_RenderFillRect(_renderer,NULL);
	SDL_SetRenderDrawColor(_renderer,0,0,0,35);
	for(int y=0;y<h;y+=MAX(3,h/100))SDL_RenderDrawLine(_renderer,0,y,w,y);
	const int band=(int)((tick*7u)%(uint32_t)MAX(1,h));
	SDL_SetRenderDrawColor(_renderer,210,232,238,38);
	SDL_Rect r={0,band,w,MAX(2,h/45)};SDL_RenderFillRect(_renderer,&r);
	for(int i=0;i<100;++i) {
		const int x=rewind_noise(&noise)%MAX(1,w),y=rewind_noise(&noise)%MAX(1,h);
		SDL_SetRenderDrawColor(_renderer,220,236,244,50+rewind_noise(&noise)%65);
		SDL_RenderDrawLine(_renderer,x,y,MIN(w-1,x+1+(int)(rewind_noise(&noise)%7)),y);
	}
	/* Same bitmap font as the pause UI, at the original logical resolution. */
	SDL_RenderSetLogicalSize(_renderer,320,200);
	SDL_SetRenderDrawColor(_renderer,4,12,20,190);
	r=(SDL_Rect){9,23,195,21};SDL_RenderFillRect(_renderer,&r);
	char label[40];snprintf(label,sizeof(label),"REBOBINANDO %d.%d S",_rewind_frames/30,(_rewind_frames%30)/3);
	SDL_SetRenderDrawColor(_renderer,230,242,248,255);
	for(int x=0;x<5;++x)for(int arrow=0;arrow<2;++arrow)
		SDL_RenderDrawLine(_renderer,14+arrow*7+x,32-x,14+arrow*7+x,32+x);
	pause_text_color(label,32,29,1,PAUSE_TEXT);
	SDL_SetRenderDrawColor(_renderer,69,189,213,210);
	r=(SDL_Rect){14,39,MAX(1,180*MIN(240,_rewind_frames)/240),2};SDL_RenderFillRect(_renderer,&r);
	SDL_RenderSetLogicalSize(_renderer,_screen_w,_screen_h);
	SDL_SetRenderDrawBlendMode(_renderer,blend);
	SDL_SetRenderDrawColor(_renderer,red,green,blue,alpha);
}
