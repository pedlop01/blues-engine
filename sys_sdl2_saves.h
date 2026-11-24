/* UTF-8 per-user save storage. Slot API is ready for the slot browser. */
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
static char *save_path(int slot) {
	if (slot<0 || slot>=SAVE_SLOT_COUNT) return NULL;
	const char *override=SDL_getenv("BLUES_SAVE_DIR");
	if (override && !*override) return NULL;
	char *directory=override ? SDL_strdup(override) : SDL_GetPrefPath("pedlop01","blues-engine");
#ifdef _WIN32
    /* CRT getenv may encode an inherited Windows path in the ANSI code page.
     * Managed paths set through SDL_setenv are already UTF-8 and stay intact. */
    if(directory && override && !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,directory,-1,NULL,0)){
        DWORD n=GetEnvironmentVariableW(L"BLUES_SAVE_DIR",NULL,0);
        wchar_t *wide=n?SDL_malloc(n*sizeof(wchar_t)):NULL;
        if(wide&&GetEnvironmentVariableW(L"BLUES_SAVE_DIR",wide,n)){
            int bytes=WideCharToMultiByte(CP_UTF8,0,wide,-1,NULL,0,NULL,NULL);
            char *utf8=bytes?SDL_malloc(bytes):NULL;
            if(utf8){WideCharToMultiByte(CP_UTF8,0,wide,-1,utf8,bytes,NULL,NULL);SDL_free(directory);directory=utf8;}
        }
        SDL_free(wide);
    }
#endif
	if (!directory) return NULL;
	const size_t capacity=strlen(directory)+40;
	char *path=SDL_malloc(capacity);
	if (path) {
		if (!override) {
			snprintf(path,capacity,"%ssaves",directory);
#ifdef _WIN32
			wchar_t *wide=(wchar_t *)SDL_iconv_string("UTF-16LE","UTF-8",path,strlen(path)+1);
			if (wide) _wmkdir(wide);
			SDL_free(wide);
#else
			mkdir(path,0700);
#endif
			snprintf(path,capacity,"%ssaves/slot%02d.bbs",directory,slot+1);
		} else snprintf(path,capacity,"%s/slot%02d.bbs",directory,slot+1);
	}
	SDL_free(directory);return path;
}
static bool sdl2_read_save_file(int slot,uint8_t *data,size_t capacity,size_t *size) {
	char *path=save_path(slot);
	FILE *fp=path ? preferences_open(path,false) : NULL;
	SDL_free(path);
	if (!fp) return false;
	const size_t count=fread(data,1,capacity,fp);
	const bool success=!ferror(fp) && fgetc(fp)==EOF;
	fclose(fp);
	if (success) *size=count;
	return success;
}
static bool sdl2_write_save_file(int slot,const uint8_t *data,size_t size) {
	char *path=save_path(slot);
	if (!path) return false;
	char *temporary=SDL_malloc(strlen(path)+32);
	if (!temporary) {SDL_free(path);return false;}
#ifdef _WIN32
	const unsigned long process_id=GetCurrentProcessId();
#else
	const unsigned long process_id=(unsigned long)getpid();
#endif
	sprintf(temporary,"%s.tmp.%lu",path,process_id);
	FILE *fp=preferences_open(temporary,true);
	bool success=false;
	if (fp) {
		success=fwrite(data,1,size,fp)==size;
		if(fclose(fp)) success=false;
		if(success) success=preferences_replace(temporary,path);
		/* On a failed write leave the original slot untouched. A stale
		 * temporary is never read as a save and can be replaced next time. */
	}
	SDL_free(temporary);SDL_free(path);return success;
}

static int sdl2_save_file_status(int slot) {
	char *path=save_path(slot);if(!path)return -1;
	errno=0;FILE *fp=preferences_open(path,false);const int error=errno;
	SDL_free(path);if(fp){fclose(fp);return 1;}return error==ENOENT ? 0 : -1;
}
static bool sdl2_capture_save_preview(uint8_t *rgb) {
	int w,h;if(SDL_GetRendererOutputSize(_renderer,&w,&h)!=0 || w<=0 || h<=0)return false;
	uint8_t *pixels=malloc((size_t)w*h*3);if(!pixels)return false;
	const bool menu=_pause_menu_open;
	_pause_menu_open=false;_save_preview_capture=true;
	sdl2_draw_screen(0,0,1); /* Read before Present; never capture the menu. */
	_save_preview_capture=false;_pause_menu_open=menu;
	SDL_RenderSetLogicalSize(_renderer,0,0);
	const bool ok=SDL_RenderReadPixels(_renderer,NULL,SDL_PIXELFORMAT_RGB24,pixels,w*3)==0;
	SDL_RenderSetLogicalSize(_renderer,_screen_w,_screen_h);
	if(ok) {
		memset(rgb,0,SAVE_PREVIEW_BYTES);
		int tw=SAVE_PREVIEW_WIDTH,th=(int)((int64_t)h*tw/w);
		if(th>SAVE_PREVIEW_HEIGHT){th=SAVE_PREVIEW_HEIGHT;tw=(int)((int64_t)w*th/h);}
		tw=MAX(1,tw);th=MAX(1,th);
		const int ox=(SAVE_PREVIEW_WIDTH-tw)/2,oy=(SAVE_PREVIEW_HEIGHT-th)/2;
		for(int y=0;y<th;++y)for(int x=0;x<tw;++x) {
			const int sx=(int)((int64_t)(2*x+1)*w/(2*tw)),sy=(int)((int64_t)(2*y+1)*h/(2*th));
			memcpy(rgb+((y+oy)*SAVE_PREVIEW_WIDTH+x+ox)*3,pixels+((size_t)sy*w+sx)*3,3);
		}
	}
	free(pixels);return ok;
}
