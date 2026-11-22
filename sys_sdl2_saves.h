/* UTF-8 per-user save storage. Slot API is ready for the slot browser. */
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
static char *save_path(int slot) {
	if (slot<0 || slot>=8) return NULL;
	const char *override=SDL_getenv("BLUES_SAVE_DIR");
	if (override && !*override) return NULL;
	char *directory=override ? SDL_strdup(override) : SDL_GetPrefPath("pedlop01","blues-engine");
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
