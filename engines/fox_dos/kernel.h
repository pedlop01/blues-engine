/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "legacy/include/level.h"
#ifdef __cplusplus
extern "C" {
#endif
int fox_dos_init(const char *root);
int fox_dos_load(unsigned level, int lives);
int fox_dos_step(unsigned input, int invulnerable, int infinite);
TITUS_level *fox_dos_world(void);
int fox_dos_camera_x(void);
int fox_dos_camera_y(void);
int fox_dos_camera_locked(void);
int fox_dos_health_visible(void);
unsigned fox_dos_tile_phase(void);
const char *fox_dos_error(void);
void fox_dos_close(void);
// Field-wise signed integer visitor; pointers are stable resource/slot indices.
typedef long long (*fox_dos_visit)(void *, const char *, long long);
int fox_dos_state(fox_dos_visit, void *, int restoring);
void fox_dos_music(unsigned);
unsigned fox_dos_level_music(void);
int fox_dos_picture(unsigned, unsigned char **);
void fox_dos_audio_tick(void);
#ifdef __cplusplus
}
#endif
