/* Production intermission, independent native/DOS brush decoding and SDL
 * texture output. No game data are included in this repository. */
#include "../sys_sdl2.c"
#include "../bb/game.c"
#include "../bb/resource.h"
#include "../bb/state.h"
#include "../mixer.h"
struct options_t g_options;
static uint32_t clock_ms;
static int presents,animated,changes,event_count,skip_at,quit_at;
static uint32_t last_crc;
static void test_sleep(int duration) {clock_ms+=duration;if(duration==20)++animated;}
static uint32_t test_clock() {return clock_ms;}
static void test_events() {
	++event_count;
	if(skip_at && event_count>=skip_at)g_sys.input.space=true;
	if(quit_at && event_count>=quit_at)g_sys.input.quit=true;
}
static void no_fade() {}
static void test_present() {
	++presents;
	const uint32_t crc=bb_state_crc(g_res.vga,320*200);
	if(crc!=last_crc)++changes;
	last_crc=crc;sdl2_update_screen();
}
static void reference_stamp(uint8_t *dst,int x,int y,int frame) {
	extern const uint8_t spr71a6_amiga[],spr71da_amiga[];
	const uint8_t *src=g_res.amiga_data ? (frame==125 ? spr71a6_amiga : spr71da_amiga) : g_res.spr_frames[frame];
	const int w=g_res.amiga_data ? (frame==125 ? 16 : 48) : READ_LE_UINT16(src-2);
	const int h=g_res.amiga_data ? (frame==125 ? 6 : 34) : READ_LE_UINT16(src-4),plane=w*h/8;
	for(int yy=0;yy<h;++yy)for(int xx=0;xx<w;++xx) {
		const int dx=x-w/2+xx,dy=y-h+yy;uint8_t color=0;
		for(int bit=0;bit<4;++bit)if(src[bit*plane+yy*w/8+xx/8]&(0x80u>>(xx&7)))color|=1<<(g_res.amiga_data ? bit : 3-bit);
		if(dx>=0 && dx<320 && dy>=0 && dy<200 && color)dst[dy*320+dx]=g_options.cga_colors ? g_res.cga_lut_sqv[(yy&1)*16+color] : color;
	}
}
static void reference_cross(uint8_t *dst,int x,int y) {
	for(int i=0;i<40;++i)reference_stamp(dst,x+19-i,y-19+i,125);
	for(int i=0;i<40;++i)reference_stamp(dst,x-19+i,y-19+i,125);
}
static void check_texture(const uint8_t *expected) {
	/* The stored bitmap being correct alone did not catch the original bug:
	 * check the actual uploaded texture, centered within the logical view. */
	const int ox=(_screen_w-320)/2,oy=(_screen_h-200)/2;
	for(int y=0;y<200;++y)for(int x=0;x<320;++x)
		assert((_screen_buffer[(y+oy)*_screen_w+x+ox]&0xffffff)==(_screen_palette[expected[y*320+x]]&0xffffff));
}
static void capture(const char *name) {
	const char *dir=getenv("BLUES_VIEW_TEST_OUTPUT");if(!dir)return;
	int w,h;SDL_GetRendererOutputSize(_renderer,&w,&h);
	SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);assert(s);
	SDL_RenderSetLogicalSize(_renderer,0,0);
	assert(SDL_RenderReadPixels(_renderer,NULL,s->format->format,s->pixels,s->pitch)==0);
	SDL_RenderSetLogicalSize(_renderer,_screen_w,_screen_h);
	char file[1024];snprintf(file,sizeof(file),"%s/%s.bmp",dir,name);assert(SDL_SaveBMP(s,file)==0);SDL_FreeSurface(s);
}
int main(int argc,char **argv) {
	assert(argc>=2 && argc<=4);
	SDL_setenv("SDL_VIDEODRIVER","dummy",1);SDL_setenv("SDL_AUDIODRIVER","dummy",1);
	SDL_setenv("BLUES_CONFIG_FILE","",1);SDL_setenv("BLUES_SAVE_DIR","",1);
	const bool wide=argc>2 && !strcmp(argv[2],"wide");
	g_options.screen_w=wide ? 640 : 320;g_options.screen_h=wide ? 360 : 200;
	for(int i=2;i<argc;++i)if(!strcmp(argv[i],"cga"))g_options.cga_colors=true;
	g_options.scroll_rate=60;g_options.music_volume=g_options.effects_volume=100;
	g_options.sound_mode=SOUND_AUTO;g_options.dos_datapath=argv[1];g_options.amiga_datapath=argv[1];
	sdl2_init();sdl2_set_screen_size(GAME_SCREEN_W,GAME_SCREEN_H,"Intermission test",2,"nearest",false);
	res_init(argv[1],MAX(64000,GAME_SCREEN_W*GAME_SCREEN_H));
	g_options.amiga_colors=g_res.amiga_data;load_common_graphics();g_mix.init();sound_init();
	g_sys.sleep=test_sleep;g_sys.get_timestamp=test_clock;g_sys.process_events=test_events;
	g_sys.update_screen=test_present;g_sys.fade_in_palette=no_fade;
	static const uint8_t xs[]={250,80,240,200,80,80},ys[]={170,55,40,95,165,170};
	uint8_t base[320*200],expected[320*200];
	for(int level=0;level<6;++level) {
		g_vars.level=level;
		load_img(g_res.amiga_data ? "inter.lbm" : "inter.sqz",GAME_SCREEN_W,g_options.cga_colors ? 9 : -1);
		memcpy(base,g_res.vga,sizeof(base));memcpy(expected,base,sizeof(base));
		for(int i=0;i<level-1;++i)reference_cross(expected,xs[i],ys[i]);
		if(level==5)reference_cross(expected,xs[5],ys[5]);
		if(level>0 && level<5)reference_cross(expected,xs[level-1],ys[level-1]);
		if(level<5)reference_stamp(expected,xs[level],ys[level],126);
		assert(memcmp(expected,base,sizeof(base))!=0);
		for(int fast=0;fast<2;++fast) {
			clock_ms=0;presents=animated=changes=event_count=0;skip_at=fast ? 3 : 0;quit_at=0;
			g_sys.input.space=g_sys.input.quit=false;last_crc=bb_state_crc(base,sizeof(base));
			do_inter_screen();
			assert(!memcmp(expected,g_res.vga,sizeof(expected)));check_texture(expected);
			if(level>0 && level<5) {assert(fast ? animated<=3 : animated==80);if(!fast)assert(changes>=60);}
			else assert(animated==0);
			assert(changes>0 && presents>=3);
			char name[48];snprintf(name,sizeof(name),"intermission-%s-%d",g_res.amiga_data ? "amiga" : "dos",level+1);if(!fast)capture(name);
		}
	}
	/* Quit must interrupt the animated brush immediately. */
	g_vars.level=2;g_sys.input.space=g_sys.input.quit=false;clock_ms=0;event_count=animated=0;skip_at=0;quit_at=4;
	do_inter_screen();assert(g_sys.input.quit && animated==4);
	/* Clipped stamps must not write outside the native canvas. */
	for(int x=-30;x<=350;x+=380)for(int y=-30;y<=230;y+=260)screen_stamp_inter_sprite(x,y,126);
	g_mix.fini();sound_fini();res_fini();sdl2_fini();
	puts("intermission: six levels, persistent completed marks, animated cross, current note, real texture upload, skip/quit and clipped native canvas passed");
	return 0;
}
