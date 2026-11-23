/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TITUS_MANAGED_FOX_H
#define TITUS_MANAGED_FOX_H
#ifdef __cplusplus
extern "C" {
#endif
void fox_tick(void);
void fox_ready(void);
void fox_actor(int x,int y);
void fox_pause_begin(void);
int fox_pause_end(int code);
int fox_run(void);
void fox_set_data_root(const char *root);
void fox_request_quit(void);
void fox_playing(void);
void fox_paused(int paused);
void fox_key(int key,int down);
int fox_audio_active(void);
void fox_audio_stats(unsigned long long *count,unsigned long long *nonzero,unsigned long long *clipped,int *peak);
void fox_capture(const char *path);
#ifdef __cplusplus
}
#endif
#endif
