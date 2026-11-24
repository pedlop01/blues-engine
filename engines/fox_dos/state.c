/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "kernel.h"
#include "original.h"
#include <stdint.h>
#include <stdio.h>
extern int fox_ending_phase, fox_ending_ticks, fox_heart_index, fox_death_ticks;
extern int fox_dos_scalars(fox_dos_visit, void *, int);
static int actor_id(TITUS_level *w, TITUS_spritedata *p) {
  if (!p)
    return -1;
  for (int i = 0; i < SPRITECOUNT; ++i)
    if (w->spritedata[i] == p)
      return i;
  return -2;
}
static TITUS_sprite *sprite_at(TITUS_level *w, int id) {
  if (id < 0)
    return NULL;
  TITUS_sprite *player[] = {&w->player.sprite, &w->player.sprite2,
                            &w->player.sprite3};
  if (id < 3)
    return player[id];
  id -= 3;
  if (id < w->objectcount)
    return &w->object[id].sprite;
  id -= w->objectcount;
  if (id < w->enemycount)
    return &w->enemy[id].sprite;
  id -= w->enemycount;
  if (id < w->elevatorcount)
    return &w->elevator[id].sprite;
  id -= w->elevatorcount;
  if (id < w->trashcount)
    return &w->trash[id];
  return NULL;
}
static int sprite_id(TITUS_level *w, TITUS_sprite *p) {
  if (!p)
    return -1;
  for (int i = 0; i < 3 + w->objectcount + w->enemycount + w->elevatorcount +
                          w->trashcount;
       ++i)
    if (sprite_at(w, i) == p)
      return i;
  return -2;
}
static int16 *animation_at(int id) {
  if (id < 0)
    return NULL;
  int16 *base[] = {&anim_player[0][0], anim_enemy, anim_zoubida, anim_moktar,
                   anim_smoke};
  unsigned count[] = {sizeof(anim_player) / 2, sizeof(anim_enemy) / 2,
                      sizeof(anim_zoubida) / 2, sizeof(anim_moktar) / 2,
                      sizeof(anim_smoke) / 2};
  unsigned group = (unsigned)id >> 16, offset = (unsigned)id & 65535;
  return group < 5 && offset < count[group] ? base[group] + offset : NULL;
}
static int animation_id(int16 *p) {
  if (!p)
    return -1;
  int16 *base[] = {&anim_player[0][0], anim_enemy, anim_zoubida, anim_moktar,
                   anim_smoke};
  unsigned count[] = {sizeof(anim_player) / 2, sizeof(anim_enemy) / 2,
                      sizeof(anim_zoubida) / 2, sizeof(anim_moktar) / 2,
                      sizeof(anim_smoke) / 2};
  for (int i = 0; i < 5; ++i) {
    uintptr_t n = (uintptr_t)p, b = (uintptr_t)base[i];
    if (n >= b && n - b < count[i] * 2 && (n - b) % 2 == 0)
      return (i << 16) + (n - b) / 2;
  }
  return -2;
}
int fox_dos_state(fox_dos_visit fn, void *ctx, int restore) {
  TITUS_level *w = fox_dos_world();
  if (!fox_dos_scalars(fn, ctx, restore))
    return 0;
  char key[80];
  for (int i = 0; i < 3 + w->objectcount + w->enemycount + w->elevatorcount +
                          w->trashcount;
       ++i) {
    TITUS_sprite *p = sprite_at(w, i);
    snprintf(key, sizeof key, "spriteptr[%d].actor", i);
    long long n = fn(ctx, key, actor_id(w, p->spritedata));
    if (n < -1 || n >= SPRITECOUNT)
      return 0;
    if (restore)
      p->spritedata = n < 0 ? NULL : w->spritedata[n];
    snprintf(key, sizeof key, "spriteptr[%d].ontop", i);
    n = fn(ctx, key, sprite_id(w, p->ONTOP));
    if (n < -1 || (n >= 0 && !sprite_at(w, n)))
      return 0;
    if (restore)
      p->ONTOP = sprite_at(w, n);
    snprintf(key, sizeof key, "spriteptr[%d].animation", i);
    n = fn(ctx, key, animation_id(p->animation));
    if (n < -1 || n > 0x4ffff || (n >= 0 && !animation_at(n)))
      return 0;
    if (restore)
      p->animation = animation_at(n);
  }
  for (int i = 0; i < w->objectcount; ++i) {
    int id = -1;
    for (int j = 0; j < ORIG_OBJECT_COUNT; ++j)
      if (w->object[i].objectdata == w->objectdata[j])
        id = j;
    snprintf(key, sizeof key, "objectptr[%d]", i);
    long long n = fn(ctx, key, id);
    if (n < -1 || n >= ORIG_OBJECT_COUNT)
      return 0;
    if (restore)
      w->object[i].objectdata = n < 0 ? NULL : w->objectdata[n];
  }
  int *extra[] = {&fox_ending_phase, &fox_ending_ticks, &fox_heart_index,
                  &fox_death_ticks};
  const char *names[] = {"ending.phase", "ending.ticks", "ending.heart",
                         "death.ticks"};
  int limits[] = {5, 31, ORIG_ANIM_MAX * 2 - 1, 60};
  for (int i = 0; i < 4; ++i) {
    long long n = fn(ctx, names[i], *extra[i]);
    if (n < 0 || n > limits[i])
      return 0;
    if (restore)
      *extra[i] = n;
  }
  return 1;
}
