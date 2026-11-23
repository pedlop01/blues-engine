#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct titus_native_options {
    const char *dos,*amiga;
    int action_button,jump_button,load_slot;
    int game,probe,edition,fullscreen,music,effects,camera,hud,view,invulnerable,infinite_lives,lives1,lives2,level,sound;
};
struct titus_slot_info {int decoded,compatible,version,amiga,preview,x,y,frame,level,lives1,lives2,camera,hud,view;uint32_t resources;uint64_t timestamp;};
int titus_native_load_slot(void);
int titus_native_prepare_load(int slot);
int titus_native_preview(const uint8_t *data,size_t size,uint8_t *rgb);
void titus_runtime_load_failed(void);
void titus_runtime_restored(int level,int x,int y,int frame,int camera,int hud,int view);
void titus_native_buttons(int *action,int *jump);
void titus_native_set_buttons(int action,int jump);
void titus_native_options_init(const struct titus_native_options *options,int preferences);
int titus_native_run(void);
void titus_native_preferences(int *fullscreen,int *music,int *effects);
void titus_runtime_actor(int x,int y);
void titus_native_capture(const char *path);
void titus_native_audio_stats(uint64_t *samples,uint64_t *nonzero,uint64_t *clipped,int *peak,int *effects);
void titus_native_inspect(const uint8_t *data,size_t size,struct titus_slot_info *info);
void titus_runtime_gameplay(void);
void titus_runtime_probe_rewind(int active);
void titus_runtime_probe_paused(int paused);
int titus_runtime_probe_save(void);
void titus_runtime_ready(const char *scene);
int titus_runtime_poll(void);
void titus_runtime_return_request(int confirmation);
void titus_runtime_return_cancelled(void);
#ifdef __cplusplus
}
#endif
