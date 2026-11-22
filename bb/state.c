/* Portable, pointer-free little-endian save format, version 1. */
#include "state.h"
#include "state_fields.h"
#include "util.h"
#include <math.h>
extern const int bb_animframes_count;
extern const uint8_t *level_triggersdata_3356[], *level_triggersdata_2030[];
extern uint8_t *level_tilesdata_1fe8[];
extern const uint16_t level_tilesdata_1e8c_sizes[], level_tilesdata_1fe8_sizes[];

uint32_t bb_state_crc(const uint8_t *data, size_t size) {
	uint32_t crc=~0u;
	for (size_t i=0;i<size;++i) {
		crc^=data[i];
		for (int bit=0;bit<8;++bit) crc=(crc>>1)^((0u-(crc&1))&0xEDB88320u);
	}
	return ~crc;
}
static int table_id(const uint8_t *pointer, int kind) {
	if (!pointer) return 255;
	const int count=kind==1 ? 4 : (kind==2 ? 103 : 61);
	for (int i=0;i<count;++i) {
		const uint8_t *p=kind==1 ? level_triggersdata_3356[i] : (kind==3 ? level_triggersdata_2030[i] : (i<86 ? level_tilesdata_1e8c[i] : level_tilesdata_1fe8[i-86]));
		if (p==pointer) return i;
	}
	return -1;
}
static const uint8_t *table_pointer(int id, int kind, bool *ok) {
	if (id==255) return NULL;
	if (id<0 || id>=(kind==1 ? 4 : (kind==2 ? 103 : 61))) { *ok=false; return NULL; }
	return kind==1 ? level_triggersdata_3356[id] : (kind==3 ? level_triggersdata_2030[id] : (id<86 ? level_tilesdata_1e8c[id] : level_tilesdata_1fe8[id-86]));
}
static int animation_id(const uint8_t **pointer) {
	if (!pointer) return -1;
	for (int i=0;i<bb_animframes_count;++i) if (pointer==animframes_059d+i) return i;
	return -2;
}
static bool dimension(int w,int h) { return w>=320 && w<=2048 && h>=200 && h<=2040; }
static bool invalid_state(int line) { print_debug(DBG_GAME,"Save validation failed (%d)",line); return false; }
bool bb_state_valid(const struct bb_state *s) {
	const struct vars_t *v=&s->vars;
	if (v->level<0 || v->level>=MAX_LEVELS || v->player<0 || v->player>1 || v->start_level<0 || v->start_level>=MAX_LEVELS) return invalid_state(__LINE__);
	if (v->screen_tilemap_w!=level_dim[v->level*2] || v->screen_tilemap_h!=level_dim[v->level*2+1]) return invalid_state(__LINE__);
	if (!dimension(s->width,s->height) || !dimension(s->base_width,s->base_height)) return invalid_state(__LINE__);
	if (s->width>v->screen_tilemap_w*16 || s->height-40>2000) return invalid_state(__LINE__);
	if (v->screen_tilemap_xorigin<0 || v->screen_tilemap_xorigin+s->width>v->screen_tilemap_w*16 || v->screen_tilemap_yorigin<0 || v->screen_tilemap_yorigin+s->height-40>2000) return invalid_state(__LINE__);
	if (v->screen_tilemap_xoffset < -15 || v->screen_tilemap_xoffset>0 || v->screen_tilemap_yoffset < -15 || v->screen_tilemap_yoffset>0) return invalid_state(__LINE__);
	if (s->input_view_step<0 || s->input_view_step>8 || s->view_step<0 || s->view_step>8 || s->frame_remainder<0 || s->frame_remainder>=30) return invalid_state(__LINE__);
	if (s->view.elapsed<0 || s->view.elapsed>300 || (s->view.elapsed<300 && (!dimension(s->view.from_w,s->view.from_h) || !dimension(s->view.to_w,s->view.to_h)))) return invalid_state(__LINE__);
	if (s->start_xpos16 < -1 || s->start_xpos16>127 || s->start_ypos16 < -1 || s->start_ypos16>124) return invalid_state(__LINE__);
	if (s->camera_mode<0 || s->camera_mode>1 || s->hud_mode<0 || s->hud_mode>1 || s->cheats<0 || s->cheats>7 || s->sound_mode<0 || s->sound_mode>2) return invalid_state(__LINE__);
	if (!isfinite(s->camera.x) || !isfinite(s->camera.y) || !isfinite(s->camera.lead) || fabs(s->camera.x)>100000 || fabs(s->camera.y)>100000 || fabs(s->camera.lead)>100000) return invalid_state(__LINE__);
	if (v->music_num<0 || v->music_num>3 || v->triggers_counter<0 || v->triggers_counter>65535 || v->update_objects_counter<0 || v->ending_counter<0 || v->ending_counter>1000000) return invalid_state(__LINE__);
	if (v->play_demo_flag || !v->play_level_flag || v->quit_level_flag || v->requested_level!=-1 || v->requested_character || v->menu_level_jump) return invalid_state(__LINE__);
	for (int i=0;i<128;++i) if (v->screen_tile_lut[i]!=(i/20)*640+i%20) return invalid_state(__LINE__);
	for (int i=0;i<MAX_OBJECTS;++i) {
		const struct object_t *o=&v->objects[i];
		if ((o->type>40 && o->type!=100) || o->special_anim>28 || (o->type==2 && o->grab_type>28) || o->sprite_type>6 || o->facing_left>1 || o->anim_frame>=MAX_SPR_FRAMES) return invalid_state(__LINE__);
		const int anim=animation_id(o->animframes_ptr);
		if (anim < -1 || (anim>=0 && anim+28>=bb_animframes_count) || (o->type!=100 && anim!=o->type*29)) return invalid_state(__LINE__);
		if (o->unk5D>=MAX_OBJECTS || o->anim_num>128 || o->xpos < -256 || o->xpos>2304 || o->ypos < -512 || o->ypos>4096) return invalid_state(__LINE__);
		if (table_id(o->trigger3,3)<0 || (o->trigger3 && o->trigger3_num>o->trigger3[0])) return invalid_state(__LINE__);
		if (i>=OBJECT_NUM_PLAYER1 && (o->data51>5 || o->lifes_count>99 || (o->type<2 && !o->lifes_count) || o->vinyls_count<0 || o->data5F>6)) return invalid_state(__LINE__);
		if (s->object_widths[i]>128 || s->object_heights[i]>128) return invalid_state(__LINE__);
	}
	for (int i=0;i<MAX_DOORS;++i) if (v->doors[i].src_x16>127 || v->doors[i].dst_x16>127 || v->doors[i].src_y16>124 || v->doors[i].dst_y16>124) return invalid_state(__LINE__);
	bool used[MAX_TRIGGERS]={0};
	for(size_t i=0;i<sizeof(s->map);++i) used[s->map[i]]=true;
	for (int i=0;i<MAX_TRIGGERS;++i) {
		const struct trigger_t *t=&s->triggers[i];
		const int id=table_id(t->op_table2,2);
		if (table_id(t->op_table1,1)<0 || table_id(t->op_table3,3)<0 || id<0 || t->op_func>40) return invalid_state(__LINE__);
		if ((t->tile_type==2 || t->tile_type==3 || t->tile_type==14 || t->tile_type==15) && !t->op_table1) return invalid_state(__LINE__);
		/* Original BINs contain out-of-range indices in unused entries. */
		if (used[i] && id!=255 && t->tile_index>=(id<86 ? level_tilesdata_1e8c_sizes[id] : level_tilesdata_1fe8_sizes[id-86])) return invalid_state(__LINE__);
	}
	for (int i=0;i<MAX_LEVELS*MAX_OBJECTS;++i) if (s->level_types[i]>40 && s->level_types[i]!=100) return invalid_state(__LINE__);
	for (int i=0;i<103;++i) {
		const uint8_t *p=i<86 ? level_tilesdata_1e8c[i] : level_tilesdata_1fe8[i-86];
		if (!s->animation_counters[i] || s->animation_counters[i]>p[1]) return invalid_state(__LINE__);
		for (int j=0;j<i;++j) if ((j<86 ? level_tilesdata_1e8c[j] : level_tilesdata_1fe8[j-86])==p && s->animation_counters[j]!=s->animation_counters[i]) return invalid_state(__LINE__);
	}
	return true;
}
struct codec { uint8_t *data; size_t capacity, pos; bool read, ok; };
static void scalar(struct codec *c,int64_t *value,int64_t low,int64_t high) {
	if (!c->ok || c->pos+4>c->capacity) { c->ok=false; return; }
	if (c->read) {
		uint32_t n=0; for (int i=0;i<4;++i) n|=(uint32_t)c->data[c->pos+i]<<(8*i);
		*value=low<0 && n>INT32_MAX ? (int64_t)n-4294967296LL : n;
	}
	if (*value<low || *value>high) { c->ok=false; return; }
	if (!c->read) for (int i=0;i<4;++i) c->data[c->pos+i]=(uint32_t)*value>>(8*i);
	c->pos+=4;
}
#define FIELD(value,low,high) do { int64_t n=(value); scalar(c,&n,(low),(high)); if(c->read && c->ok) (value)=n; } while(0)
#define F(value) FIELD(value,INT32_MIN,INT32_MAX)
#define BOOL(value) FIELD(value,0,1)
static void bytes(struct codec *c,uint8_t *p,size_t size) {
	if (!c->ok || size>c->capacity-c->pos) {c->ok=false;return;}
	if (c->read) memcpy(p,c->data+c->pos,size); else memcpy(c->data+c->pos,p,size);
	c->pos+=size;
}
static void real(struct codec *c,double *value) {
	uint64_t bits=0;
	if (!c->read) memcpy(&bits,value,8);
	uint32_t low=bits,high=bits>>32;
	FIELD(low,0,UINT32_MAX); FIELD(high,0,UINT32_MAX);
	if (c->read) {bits=(uint64_t)high<<32|low;memcpy(value,&bits,8);}
}
static void payload(struct codec *c,struct bb_state *s) {
	struct vars_t *v=&s->vars;
#define VAR(name,low,high) FIELD(v->name,low,high);
	BB_STATE_VARS_FIELDS(VAR)
#undef VAR
	for (int i=0;i<MAX_OBJECTS;++i) {FIELD(v->level_xpos[i],-32768,32767);FIELD(v->level_ypos[i],-32768,32767);}
	for (int i=0;i<128;++i) FIELD(v->screen_tile_lut[i],0,65535);
	for (int i=0;i<MAX_DOORS;++i) {FIELD(v->doors[i].src_x16,0,65535);FIELD(v->doors[i].src_y16,0,65535);FIELD(v->doors[i].dst_x16,0,65535);FIELD(v->doors[i].dst_y16,0,65535);}
	for (int i=0;i<MAX_OBJECTS;++i) {
		struct object_t *o=&v->objects[i];
#define OBJ(name,low,high) FIELD(o->name,low,high);
		BB_STATE_OBJECT_FIELDS(OBJ)
#undef OBJ
		int id=c->read ? -1 : animation_id(o->animframes_ptr); F(id);
		if (c->read) {if(id < -1 || id>=bb_animframes_count) c->ok=false;else o->animframes_ptr=id<0 ? NULL : animframes_059d+id;}
		id=c->read ? 255 : table_id(o->trigger3,3); F(id);
		if (c->read) o->trigger3=table_pointer(id,3,&c->ok);
		FIELD(s->object_widths[i],0,65535);FIELD(s->object_heights[i],0,65535);
	}
	for (int i=0;i<MAX_TRIGGERS;++i) {
		struct trigger_t *t=&s->triggers[i];
		FIELD(t->tile_type,0,255);FIELD(t->tile_flags,0,255);FIELD(t->op_func,0,255);FIELD(t->unk10,0,255);
		FIELD(t->unk16,0,255);FIELD(t->tile_index,0,255);FIELD(t->foreground_tile_num,0,255);
		for(int kind=1;kind<=3;++kind) {
			const uint8_t **p=kind==1 ? &t->op_table1 : (kind==2 ? &t->op_table2 : &t->op_table3);
			int id=c->read ? 255 : table_id(*p,kind); F(id);
			if(c->read) *p=table_pointer(id,kind,&c->ok);
		}
	}
	bytes(c,s->map,sizeof(s->map));bytes(c,s->animation_counters,sizeof(s->animation_counters));bytes(c,s->level_types,sizeof(s->level_types));
	F(s->width);F(s->height);F(s->base_width);F(s->base_height);F(s->view_step);F(s->input_view_step);F(s->frame_remainder);
	BOOL(s->amiga);BOOL(s->demo);BOOL(s->cga);BOOL(s->amiga_colors);BOOL(s->copper);BOOL(s->camera_cut);
	F(s->camera_mode);F(s->hud_mode);F(s->cheats);F(s->sound_mode);
	F(s->start_xpos16);F(s->start_ypos16);BOOL(s->dos_scrolling);
	F(s->view.from_w);F(s->view.from_h);F(s->view.to_w);F(s->view.to_h);F(s->view.elapsed);
	BOOL(s->camera.valid);real(c,&s->camera.x);real(c,&s->camera.y);real(c,&s->camera.lead);
	F(s->camera.origin_x);F(s->camera.origin_y);F(s->camera.width);F(s->camera.height);F(s->camera.player);
	FIELD(s->resources,0,UINT32_MAX);
	uint32_t low=s->timestamp,high=s->timestamp>>32;FIELD(low,0,UINT32_MAX);FIELD(high,0,UINT32_MAX);
	if (c->read) s->timestamp=(uint64_t)high<<32|low;
}
#undef FIELD
#undef F
#undef BOOL
bool bb_state_encode(const struct bb_state *state,uint8_t *data,size_t capacity,size_t *size) {
	if (capacity<24 || !bb_state_valid(state)) return false;
	struct bb_state copy=*state;
	struct codec c={data,capacity,24,false,true}; payload(&c,&copy);
	if (!c.ok) return false;
	memcpy(data,"BBSAVE\r\n",8);
	uint32_t fields[]={1,(uint32_t)(c.pos-24),bb_state_crc(data+24,c.pos-24),0x42423031};
	for (int i=0;i<4;++i) for(int byte=0;byte<4;++byte) data[8+i*4+byte]=fields[i]>>(8*byte);
	*size=c.pos;return true;
}
bool bb_state_decode(struct bb_state *state,const uint8_t *data,size_t size) {
	if (size<24 || size>BB_STATE_MAX_SIZE || memcmp(data,"BBSAVE\r\n",8)) return false;
	uint32_t fields[4]={0};
	for(int i=0;i<4;++i) for(int byte=0;byte<4;++byte) fields[i]|=(uint32_t)data[8+i*4+byte]<<(8*byte);
	if(fields[0]!=1 || fields[1]!=size-24 || fields[2]!=bb_state_crc(data+24,size-24) || fields[3]!=0x42423031) return false;
	struct bb_state copy={0}; struct codec c={(uint8_t *)data,size,24,true,true};payload(&c,&copy);
	if (!c.ok || c.pos!=size || !bb_state_valid(&copy)) return false;
	*state=copy;return true;
}
