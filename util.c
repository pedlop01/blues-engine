
#include <stdarg.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <dirent.h>
#include <strings.h>
#include "sys.h"
#include "util.h"
#ifdef _WIN32
#include <windows.h>
#endif

int g_debug_mask = 0;

static void string_lower(char *p) {
	for (; *p; ++p) {
		if (*p >= 'A' && *p <= 'Z') {
			*p += 'a' - 'A';
		}
	}
}

static void string_upper(char *p) {
	for (; *p; ++p) {
		if (*p >= 'a' && *p <= 'z') {
			*p += 'A' - 'a';
		}
	}
}

#ifndef NDEBUG
void print_debug(int debug_channel, const char *msg, ...) {
	if (g_debug_mask & debug_channel) {
		char buf[256];
		va_list va;
		va_start(va, msg);
		vsprintf(buf, msg, va);
		va_end(va);
		fprintf(stdout, "%s\n", buf);
		g_sys.print_log(stdout, buf);
	}
}
#endif

void print_warning(const char *msg, ...) {
	char buf[256];
	va_list va;
	va_start(va, msg);
	vsprintf(buf, msg, va);
	va_end(va);
	fprintf(stderr, "WARNING: %s\n", buf);
	g_sys.print_log(stderr, buf);
}

void print_error(const char *msg, ...) {
	char buf[256];
	va_list va;
	va_start(va, msg);
	vsprintf(buf, msg, va);
	va_end(va);
	fprintf(stderr, "ERROR: %s!\n", buf);
	g_sys.print_log(stderr, buf);
	exit(-1);
}

void print_info(const char *msg, ...) {
	char buf[256];
	va_list va;
	va_start(va, msg);
	vsprintf(buf, msg, va);
	va_end(va);
	fprintf(stdout, "%s\n", buf);
	g_sys.print_log(stdout, buf);
}

#ifdef _WIN32
wchar_t *path_utf16(const char *path) {
    int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0);
    if(!size)return NULL;
    wchar_t *wide=malloc(size*sizeof(wchar_t));if(!wide)return NULL;
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,size);
    for(wchar_t *p=wide;*p;++p)if(*p==L'/')*p=L'\\';
    if(wcsncmp(wide,L"\\\\?\\",4)==0)return wide;
    DWORD count=GetFullPathNameW(wide,0,NULL,NULL);
    wchar_t *full=count?malloc(count*sizeof(wchar_t)):NULL;
    if(!full||!GetFullPathNameW(wide,count,full,NULL)){free(wide);free(full);return NULL;}free(wide);
    wchar_t *extended=malloc((count+8)*sizeof(wchar_t));if(!extended){free(full);return NULL;}
    if(full[0]==L'\\'&&full[1]==L'\\'){wcscpy(extended,L"\\\\?\\UNC\\");wcscat(extended,full+2);}
    else{wcscpy(extended,L"\\\\?\\");wcscat(extended,full);}free(full);return extended;
}
#endif
FILE *fopen_utf8(const char *path,const char *mode) {
#ifdef _WIN32
    wchar_t *wide=path_utf16(path);wchar_t *wm=malloc((strlen(mode)+1)*sizeof(wchar_t));
    if(wm)for(size_t i=0;i<=strlen(mode);++i)wm[i]=(unsigned char)mode[i];
    FILE *fp=wide&&wm?_wfopen(wide,wm):NULL;free(wide);free(wm);return fp;
#else
    return fopen(path,mode);
#endif
}
FILE *fopen_nocase(const char *path,const char *filename) {
    if(!path||!filename)return NULL;
    const size_t size=strlen(path)+strlen(filename)+2;char *buf=malloc(size);if(!buf)return NULL;
    snprintf(buf,size,"%s/%s",path,filename);FILE *fp=fopen_utf8(buf,"rb");
    if(!fp){void (*const convert[])(char *)={string_upper,string_lower};
        for(unsigned i=0;i<2&&!fp;++i){strcpy(buf+strlen(path)+1,filename);convert[i](buf+strlen(path)+1);fp=fopen_utf8(buf,"rb");}}
    if(!fp){
#ifdef _WIN32
        size_t capacity=strlen(path)+3;char *pattern=malloc(capacity);
        if(pattern){snprintf(pattern,capacity,"%s/*",path);wchar_t *wide=path_utf16(pattern);wchar_t *name=NULL;
            int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,filename,-1,NULL,0);if(n){name=malloc(n*sizeof(wchar_t));if(name)MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,filename,-1,name,n);}
            WIN32_FIND_DATAW entry;HANDLE search=wide?FindFirstFileW(wide,&entry):INVALID_HANDLE_VALUE;
            if(search!=INVALID_HANDLE_VALUE){do{if(name&&_wcsicmp(name,entry.cFileName)==0){int bytes=WideCharToMultiByte(CP_UTF8,0,entry.cFileName,-1,NULL,0,NULL,NULL);char *actual=malloc(bytes);
                    if(actual){WideCharToMultiByte(CP_UTF8,0,entry.cFileName,-1,actual,bytes,NULL,NULL);size_t len=strlen(path)+bytes+2;char *found=malloc(len);if(found){snprintf(found,len,"%s/%s",path,actual);fp=fopen_utf8(found,"rb");free(found);}free(actual);}break;}}while(FindNextFileW(search,&entry));FindClose(search);}
            free(wide);free(name);free(pattern);}
#else
        DIR *directory=opendir(path);if(directory){struct dirent *entry;
            while((entry=readdir(directory))){if(strcasecmp(entry->d_name,filename)==0){size_t len=strlen(path)+strlen(entry->d_name)+2;char *actual=malloc(len);if(actual){snprintf(actual,len,"%s/%s",path,entry->d_name);fp=fopen_utf8(actual,"rb");free(actual);}break;}}closedir(directory);}
#endif
    }
    free(buf);return fp;
}

uint16_t fread_le16(FILE *fp) {
	const uint16_t val = fgetc(fp);
	return val | (fgetc(fp) << 8);
}
