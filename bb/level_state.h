/* Private integration: capture a stable simulation boundary and rebuild assets
 * before restoring references. No simulation runs while saving or loading. */
#include <time.h>
#include "dos_audio.h"
extern uint8_t *level_tilesdata_1fe8[];
static struct view_transition *_state_view;
static int *_state_view_step, *_state_frame_remainder, *_state_base_w, *_state_base_h;
static bool *_state_initial_transition;
static struct bb_state *_prepared_state;
static bool _state_load_reopen;
static const char *_state_error="";

static bool hash_resource(uint32_t *hash,const char *path,const char *name) {
	if (!name || !*name) return true;
	FILE *fp=path ? fopen_nocase(path,name) : NULL;
	if (!fp) return false;
	for (const char *p=name;*p;++p) {*hash^=(uint8_t)*p;*hash*=16777619u;}
	uint8_t data[4096];size_t count;
	while ((count=fread(data,1,sizeof(data),fp))) for (size_t i=0;i<count;++i) {*hash^=data[i];*hash*=16777619u;}
	const bool ok=!ferror(fp);fclose(fp);return ok;
}
static bool state_resource_hash(int level,bool amiga,bool demo,uint32_t *result) {
	if(level<0 || level>=MAX_LEVELS || (demo && (amiga || level!=0))) return false;
	const char *path=amiga ? g_options.amiga_datapath : g_options.dos_datapath;
	if (!path || data_file_present(path,"mag.tbl")!=amiga || (!amiga && data_file_present(path,"demomag.sql")!=demo)) return false;
	uint32_t hash=2166136261u;
	if (amiga) {
		const bool fallback=level==5 && !data_file_present(path,_levels_amiga[level].bin);
		const char *files[]={"sprite","objet",_levels_amiga[level].blk,_levels_amiga[level].tbl,
			fallback ? "concert.tbl" : _levels_amiga[level].bin,_levels_amiga[level].ennemi,_levels_amiga[level].m};
		for (unsigned i=0;i<ARRAYSIZE(files);++i) if(!hash_resource(&hash,path,files[i])) return false;
		if(fallback && !hash_resource(&hash,g_options.dos_datapath,"concert.sql")) return false;
	} else {
		const char *files[]={"sprite.sqv",demo ? "demomag.ck1" : _levels[level].ck1,
			demo ? "demomag.ck2" : _levels[level].ck2,demo ? "demomag.sql" : _levels[level].sql,
			_levels[level].bin,_levels[level].avt,_levels[level].sqv};
		for(unsigned i=0;i<ARRAYSIZE(files);++i) if(!hash_resource(&hash,path,files[i])) return false;
	}
	*result=hash;return true;
}
static bool capture_game_state(struct bb_state *s) {
	if (!_state_view || !_loaded_resources_signature) return false;
	memset(s,0,sizeof(*s));
	s->vars=g_vars;memcpy(s->triggers,g_res.triggers,sizeof(s->triggers));
	memcpy(s->map,g_res.sql,sizeof(s->map));memcpy(s->level_types,level_data,sizeof(s->level_types));
	memcpy(s->object_widths,level_obj_w,sizeof(s->object_widths));memcpy(s->object_heights,level_obj_h,sizeof(s->object_heights));
	for(int i=0;i<103;++i) s->animation_counters[i]=(i<86 ? level_tilesdata_1e8c[i] : level_tilesdata_1fe8[i-86])[0];
	s->camera=_camera;s->view=*_state_view;s->width=GAME_SCREEN_W;s->height=GAME_SCREEN_H;
	s->base_width=*_state_base_w;s->base_height=*_state_base_h;
	s->view_step=*_state_view_step;s->input_view_step=g_sys.input.view_step;s->frame_remainder=*_state_frame_remainder;
	s->amiga=g_res.amiga_data;s->demo=g_res.dos_demo;s->cga=g_options.cga_colors;
	s->amiga_colors=g_options.amiga_colors;s->copper=g_options.amiga_copper_bars;s->camera_cut=_view_camera_cut;
	s->camera_mode=g_options.camera_mode;s->hud_mode=g_options.hud_mode;s->cheats=g_options.cheats;s->sound_mode=g_options.sound_mode;
	s->start_xpos16=g_options.start_xpos16;s->start_ypos16=g_options.start_ypos16;s->dos_scrolling=g_options.dos_scrolling;
	s->resources=_loaded_resources_signature;s->timestamp=(uint64_t)time(NULL);
	return bb_state_valid(s);
}
static const char *game_save_error() {return _state_error;}
static bool save_current_game(int slot) {
	struct bb_state *state=malloc(sizeof(*state));
	uint8_t *data=malloc(BB_STATE_MAX_SIZE);size_t size=0;
	bool success=state && data && capture_game_state(state) && bb_state_encode(state,data,BB_STATE_MAX_SIZE,&size);
	_state_error="NO SE PUEDE GUARDAR ESTE ESTADO";
	if(success) {
		uint8_t *rgb=malloc(SAVE_PREVIEW_BYTES);
		success=rgb && g_sys.capture_save_preview && g_sys.capture_save_preview(rgb) && bb_state_attach_preview(data,BB_STATE_MAX_SIZE,&size,rgb);
		free(rgb);
		if(!success) {_state_error="NO SE PUDO CAPTURAR LA MINIATURA";free(state);free(data);return false;}
		success=g_sys.write_save_file && g_sys.write_save_file(slot,data,size);
		_state_error=success ? "PARTIDA GUARDADA" : "NO SE PUDO ESCRIBIR EL GUARDADO";
	}
	free(state);free(data);return success;
}
static bool state_resources_available(const struct bb_state *s) {
	uint32_t signature;
	if (!state_resource_hash(s->vars.level,s->amiga,s->demo,&signature)) {
		_state_error="FALTAN DATOS DEL JUEGO GUARDADO";return false;
	}
	if (signature!=s->resources) {_state_error="DATOS DEL JUEGO INCOMPATIBLES";return false;}
	if (s->sound_mode==SOUND_DOS) {
		FILE *fp=fopen_nocase(s->amiga ? g_options.amiga_datapath : g_options.dos_datapath,"1.exe");
		struct dos_audio *audio=NULL;
		if(fp) {
			if(fseek(fp,0,SEEK_END)==0) {
				const long size=ftell(fp);
				if(size>0 && size<=1048576 && fseek(fp,0,SEEK_SET)==0) {
					uint8_t *exe=malloc(size);
					if(exe && fread(exe,1,size,fp)==(size_t)size) audio=dos_audio_create(exe,size,SYS_AUDIO_FREQ);
					free(exe);
				}
			}
			fclose(fp);
		}
		if(!audio) {_state_error="FALTAN DATOS DE AUDIO DOS VALIDOS";return false;}
		dos_audio_destroy(audio);
	}
	return true;
}
static bool prepare_game_load(int slot) {
	free(_prepared_state);_prepared_state=NULL;
	uint8_t *data=malloc(BB_STATE_MAX_SIZE);struct bb_state *state=malloc(sizeof(*state));size_t size=0;
	_state_error="NO SE PUDO LEER EL GUARDADO";
	bool success=data && state && g_sys.read_save_file && g_sys.read_save_file(slot,data,BB_STATE_MAX_SIZE,&size);
	if(success) {
		_state_error="GUARDADO DANADO O INCOMPATIBLE";
		success=bb_state_decode(state,data,size);
		if(success) success=state_resources_available(state);
	}
	free(data);
	if(success) {_prepared_state=state;_state_error="";} else free(state);
	return success;
}
static void inspect_game_save(int slot,struct sys_save_slot *info) {
	memset(info,0,sizeof(*info));
	const int exists=g_sys.save_file_status ? g_sys.save_file_status(slot) : -1;
	if(exists==0){info->status=SAVE_EMPTY;info->reason="SLOT VACIO";return;}
	info->status=SAVE_UNREADABLE;info->reason="NO SE PUDO LEER EL GUARDADO";
	if(exists<0)return;
	uint8_t *data=malloc(BB_STATE_MAX_SIZE);struct bb_state *state=malloc(sizeof(*state));size_t size=0;
	if(!data || !state){free(data);free(state);return;}
	info->status=SAVE_DAMAGED;info->reason="GUARDADO DANADO";
	if(g_sys.read_save_file && g_sys.read_save_file(slot,data,BB_STATE_MAX_SIZE,&size)) {
		if(size>=24 && !memcmp(data,"BBSAVE\r\n",8) && ((READ_LE_UINT32(data+8)!=1 && READ_LE_UINT32(data+8)!=2) || READ_LE_UINT32(data+20)!=0x42423031)) {
			info->status=SAVE_INCOMPATIBLE;info->reason="VERSION NO COMPATIBLE";
		} else if(bb_state_decode(state,data,size)) {
			info->has_metadata=true;info->level=state->vars.level;info->timestamp=state->timestamp;info->amiga=state->amiga;
			info->two_players=state->vars.two_players_flag;info->infinite_lives=(state->cheats&CHEATS_UNLIMITED_LIFES)!=0;
			for(int i=0;i<2;++i)info->lives[i]=state->vars.objects[OBJECT_NUM_PLAYER1+i].lifes_count;
			const uint8_t *preview=bb_state_preview(data,size);
			if(preview){info->has_preview=true;memcpy(info->preview,preview,SAVE_PREVIEW_BYTES);}
			const char *previous=_state_error;
			if(state_resources_available(state)){info->status=SAVE_READY;info->reason=NULL;}
			else {info->status=SAVE_INCOMPATIBLE;info->reason=_state_error;}
			_state_error=previous;
		}
	}
	free(data);free(state);
}
static void repaint_restored_state() {
	const struct vars_t before=g_vars;
	screen_init();screen_clear_sprites(0);
	do_level_redraw_tilemap(g_vars.screen_tilemap_xorigin,g_vars.screen_tilemap_yorigin);
	for(int i=0;i<MAX_OBJECTS;++i) {
		const struct object_t *o=&g_vars.objects[i];
		if(o->type==100 || !o->visible_flag) continue;
		g_sys.set_sprite_owner(i | (o->type<<8) | (o->type==2 ? o->grab_type<<16 : 0));
		if(o->blinking_counter && (g_vars.level_loop_counter&1)) {
			if(o->facing_left) screen_add_game_sprite4(o->screen_xpos,o->screen_ypos+1,o->anim_frame,o->blinking_counter);
			else screen_add_game_sprite3(o->screen_xpos,o->screen_ypos+1,o->anim_frame,o->blinking_counter);
		} else if(o->facing_left) screen_add_game_sprite2(o->screen_xpos,o->screen_ypos+1,o->anim_frame);
		else screen_add_game_sprite1(o->screen_xpos,o->screen_ypos+1,o->anim_frame);
	}
	g_sys.set_sprite_owner(-1);
	draw_foreground_tiles();
	const int vinyls=g_vars.vinyls_count;
	_state_repainting=true;draw_level_panel();_state_repainting=false;g_vars.vinyls_count=vinyls;
	copy_level_bitmap();g_sys.update_screen();
	memset(g_res.vga,0,GAME_SCREEN_W*GAME_SCREEN_H);
	g_vars=before;
}
static void restore_timeline_state(const struct bb_state *s) {
	g_vars=s->vars;memcpy(g_res.sql,s->map,sizeof(s->map));memcpy(g_res.triggers,s->triggers,sizeof(s->triggers));
	memcpy(level_data,s->level_types,sizeof(s->level_types));memcpy(level_obj_w,s->object_widths,sizeof(s->object_widths));memcpy(level_obj_h,s->object_heights,sizeof(s->object_heights));
	for(int i=0;i<103;++i) (i<86 ? level_tilesdata_1e8c[i] : level_tilesdata_1fe8[i-86])[0]=s->animation_counters[i];
	_camera=s->camera;_view_camera_cut=s->camera_cut;*_state_view=s->view;
	*_state_view_step=s->view_step;*_state_frame_remainder=s->frame_remainder;
	*_state_base_w=_level_base_w=s->base_width;*_state_base_h=_level_base_h=s->base_height;
	*_state_initial_transition=false;
	g_sys.input.view_step=s->input_view_step;
	g_options.camera_mode=s->camera_mode;g_options.hud_mode=s->hud_mode;g_options.cheats=s->cheats;
	configure_level_hud();
	g_sys.render_set_sprites_clipping_rect(0,TILEMAP_OFFSET_Y,TILEMAP_SCREEN_W,TILEMAP_SCREEN_H);
	g_sys.set_scroll_interpolation(0,0,0,false);
	update_assist_status();
}
static bool apply_prepared_game() {
	const struct bb_state *s=_prepared_state;
	if(!s || !_state_view || !state_resources_available(s)) return false;
	/* Renderer allocation can fail without replacing the old scene. */
	if(!g_sys.resize_viewport(s->width,s->height)) {_state_error="NO HAY MEMORIA PARA CARGAR";return false;}
	g_mix.fini();sound_fini();res_fini();memset(&g_res,0,sizeof(g_res));
	g_options.screen_w=s->width;g_options.screen_h=s->height;
	g_options.cga_colors=s->cga;g_options.amiga_colors=s->amiga_colors;g_options.amiga_copper_bars=s->copper;
	g_options.camera_mode=s->camera_mode;g_options.hud_mode=s->hud_mode;g_options.cheats=s->cheats;g_options.sound_mode=s->sound_mode;
	g_options.start_xpos16=s->start_xpos16;g_options.start_ypos16=s->start_ypos16;g_options.dos_scrolling=s->dos_scrolling;
	g_vars=s->vars;
	res_init(s->amiga ? g_options.amiga_datapath : g_options.dos_datapath,MAX(64000,s->width*s->height));
	sound_init();load_common_graphics();g_mix.init();load_level_data(s->vars.level);
	restore_timeline_state(s);
	g_sys.input.direction=0;g_sys.input.space=g_sys.input.jump=g_sys.input.menu=g_sys.input.switch_mode=g_sys.input.switch_hud=g_sys.input.rewind=false;
	g_sys.input.view_step=s->input_view_step;
	g_sys.set_copper_bars(s->copper ? _copper_data+g_vars.level*18 : NULL);
	g_sys.set_scroll_render_rate(g_options.scroll_rate);
	play_music(g_vars.music_num);
	repaint_restored_state();
	rewind_clear();
	free(_prepared_state);_prepared_state=NULL;
	if(g_sys.save_preferences) g_sys.save_preferences();
	return true;
}
