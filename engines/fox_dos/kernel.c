/* SPDX-License-Identifier: GPL-3.0-or-later
 * OpenTitus bootstrap/one-step integration; original simulation order retained.
 */
#include "kernel.h"
#include "audio.h"
#include "backbuffer.h"
#include "draw.h"
#include "elevators.h"
#include "enemies.h"
#include "fonts.h"
#include "gates.h"
#include "globals.h"
#include "objects.h"
#include "original.h"
#include "player.h"
#include "reset.h"
#include "scroll.h"
#include "settings.h"
#include "sprites.h"
#include "sqz.h"
#include "tituserror.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <wchar.h>
#include <windows.h>
#endif
static TITUS_level world;
static TITUS_spritedata **actors;
static TITUS_objectdata **objects;
static TITUS_spritecache cache;
static uint16 actor_count, object_count;
static int loaded = 0;
int fox_ending_phase = 0, fox_ending_ticks = 0, fox_heart_index = 0,
    fox_death_ticks = 0;
static Uint8 keys[512];
static char *data_root;
FILE *__real_fopen(const char *, const char *);
FILE *__wrap_fopen(const char *name, const char *mode) {
  char path[4096];
  if (!data_root)
    return __real_fopen(name, mode);
  if (strchr(name, '/') || strchr(name, '\\') || strchr(name, ':') ||
      strstr(name, "..") || mode[0] != 'r')
    return NULL;
  if (snprintf(path, sizeof(path), "%s/%s", data_root, name) >=
      (int)sizeof(path))
    return NULL;
#ifdef _WIN32
  int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
  if (!n)
    return NULL;
  wchar_t *input = malloc(n * sizeof(wchar_t));
  wchar_t *wide = malloc((n + 8) * sizeof(wchar_t));
  FILE *file = NULL;
  wchar_t wmode[16];
  if (input && wide && strlen(mode) < 15) {
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, input, n);
    for (int i = 0; input[i]; ++i)
      if (input[i] == L'/')
        input[i] = L'\\';
    if (input[0] == L'\\' && input[1] == L'\\') {
      wcscpy(wide, L"\\\\?\\UNC\\");
      wcscat(wide, input + 2);
    } else {
      wcscpy(wide, L"\\\\?\\");
      wcscat(wide, input);
    }
    for (unsigned i = 0; i <= strlen(mode); ++i)
      wmode[i] = (unsigned char)mode[i];
    file = _wfopen(wide, wmode);
  }
  free(wide);
  free(input);
  return file;
#else
  return __real_fopen(path, mode);
#endif
}
Uint8 *fox_legacy_keys(int *n) {
  if (n)
    *n = 512;
  return keys;
}
int fox_legacy_event(SDL_Event *event) { return 0; }
void fox_tick(void) {}
void fox_ready(void) {}
void fox_actor(int x, int y) {}
void fox_playing(void) {}
void fox_paused(int p) {}
void fox_pause_begin(void) {}
int fox_pause_end(int p) { return p; }
int waitforbutton(void) { return 0; }
int waitforbutton_notpressed(void) { return 0; }
int fox_dos_init(const char *root) {
  unsigned char *raw;
  int size;
  data_root = strdup(root);
  levelcount = 15;
  game = 0;
  devmode = 0;
  strcpy(spritefile, "SPREXP.SQZ");
  strcpy(fontfile, "FONTS.SQZ");
  const char *names[] = {"LEVEL0.SQZ", "LEVELJ.SQZ", "LEVEL1.SQZ", "LEVEL2.SQZ",
                         "LEVEL3.SQZ", "LEVEL4.SQZ", "LEVEL5.SQZ", "LEVEL6.SQZ",
                         "LEVEL7.SQZ", "LEVEL8.SQZ", "LEVEL9.SQZ", "LEVELB.SQZ",
                         "LEVELC.SQZ", "LEVELE.SQZ", "LEVELG.SQZ"};
  for (int i = 0; i < 15; ++i)
    strcpy(levelfiles[i], names[i]);
  ingamewidth = 320;
  ingameheight = 200;
  reswidth = 320;
  resheight = 200;
  initoriginal();
  if (loadpixelformat(&world.pixelformat) < 0)
    return -1;
  screen = SDL_CreateRGBSurface(0, 320, 200, 8, 0, 0, 0, 0);
  tilescreen = SDL_CreateRGBSurface(0, 320, 192, 8, 0, 0, 0, 0);
  copypixelformat(screen->format, world.pixelformat);
  copypixelformat(tilescreen->format, world.pixelformat);
  size = unSQZ(spritefile, &raw);
  if (size < 0)
    return -1;
  int result =
      loadsprites(&actors, (char *)raw, size, world.pixelformat, &actor_count);
  free(raw);
  if (result < 0 || initspritecache(&cache, 100, 3) < 0 ||
      loadobjects(&objects, &object_count) < 0)
    return -1;
  loadfonts();
  return initaudio();
}
int fox_dos_load(unsigned n, int lives) {
  unsigned char *raw;
  int size;
  if (n >= (unsigned)levelcount)
    return -1;
  if (loaded)
    freelevel(&world);
  world.levelnumber = n;
  world.levelid = getlevelid(n);
  world.lives = lives;
  world.extrabonus = 0;
  size = unSQZ(levelfiles[n], &raw);
  if (size < 0)
    return -1;
  int result = loadlevel(&world, raw, size, actors, &cache, objects);
  free(raw);
  if (result < 0)
    return result;
  loaded = 1;
  fox_ending_phase = fox_ending_ticks = fox_heart_index = fox_death_ticks = 0;
  CLEAR_DATA(&world);
  GODMODE = NOCLIP = DISPLAYLOOPTIME = fox_false;
  INIT_SCREENM(&world);
  TFR_SCREENM();
  DISPLAY_SPRITES(&world);
  SELECT_MUSIC(LEVEL_MUSIC[world.levelid]);
  return 0;
}

static int ending_tick(unsigned input) {
  TITUS_player *p = &world.player;
  NOSCROLL_FLAG = fox_true;
  if (!fox_ending_phase) {
    updatesprite(&world, &p->sprite, 343, fox_true);
    p->sprite.x = -100;
    p->sprite.y = 180;
    p->sprite.animation = anim_moktar;
    updatesprite(&world, &p->sprite2, 337, fox_true);
    p->sprite2.x = 420;
    p->sprite2.y = 180;
    p->sprite2.animation = anim_zoubida;
    fox_ending_phase = 1;
  }
  if (fox_ending_phase == 1) {
    p->sprite.x += 3;
    MOVE_HIM(&world, &p->sprite);
    p->sprite2.x -= 3;
    MOVE_HIM(&world, &p->sprite2);
    if (p->sprite2.x <= p->sprite.x + 28) {
      updatesprite(&world, &p->sprite2, 346, fox_true);
      p->sprite2.flipped = fox_true;
      p->sprite2.x -= 24;
      p->sprite.animation = anim_smoke;
      p->sprite.y -= 16;
      fox_ending_phase = 2;
      fox_ending_ticks = 0;
    }
  } else if (fox_ending_phase == 2) {
    MOVE_HIM(&world, &p->sprite);
    ++p->sprite.y;
    if (++fox_ending_ticks == 16) {
      updatesprite(&world, &p->sprite, 355, fox_true);
      fox_ending_phase = 3;
    }
  } else if (fox_ending_phase == 3) {
    if (fox_heart_index < 0 || fox_heart_index >= ORIG_ANIM_MAX * 2 - 1)
      fox_heart_index = 0;
    if (COEUR_POS[fox_heart_index] < 0)
      fox_heart_index += COEUR_POS[fox_heart_index];
    if (fox_heart_index < 0 || fox_heart_index >= ORIG_ANIM_MAX * 2 - 1)
      fox_heart_index = 0;
    p->sprite.x = COEUR_POS[fox_heart_index++];
    p->sprite.y = COEUR_POS[fox_heart_index++];
    if (input & 16) {
      updatesprite(&world, &p->sprite, 335, fox_true);
      p->sprite.x = (BITMAP_X << 4) - 118;
      p->sprite.y = (BITMAP_Y << 4) + 100;
      updatesprite(&world, &p->sprite3, 336, fox_true);
      p->sprite3.x = (BITMAP_X << 4) + 438;
      p->sprite3.y = (BITMAP_Y << 4) + 100;
      fox_ending_ticks = 0;
      fox_ending_phase = 4;
    }
  } else if (fox_ending_phase == 4) {
    p->sprite.x += 8;
    p->sprite3.x -= 8;
    if (++fox_ending_ticks == 31)
      fox_ending_phase = 5;
  }
  scroll(&world);
  BLOC_ANIMATION(&world);
  TFR_SCREENM();
  DISPLAY_SPRITES(&world);
  return fox_ending_phase == 5 ? 4 : 0;
}
int fox_dos_step(unsigned input, int god, int infinite) {
  if (world.levelid == 16)
    return ending_tick(input);
  if (fox_death_ticks) {
    world.player.sprite.speedY--;
    if (world.player.sprite.speedY < -16)
      world.player.sprite.speedY = -16;
    world.player.sprite.y -= world.player.sprite.speedY;
    if (--fox_death_ticks == 0) {
      CLEAR_DATA(&world);
      INIT_SCREENM(&world);
      SELECT_MUSIC(LEVEL_MUSIC[world.levelid]);
    }
    TFR_SCREENM();
    DISPLAY_SPRITES(&world);
    return 0;
  }
  memset(keys, 0, sizeof(keys));
  keys[KEY_LEFT] = !!(input & 1);
  keys[KEY_RIGHT] = !!(input & 2);
  keys[KEY_UP] = !!(input & 4);
  keys[KEY_DOWN] = !!(input & 8);
  keys[KEY_SPACE] = !!(input & 16);
  GODMODE = god;
  int lives = world.lives;
  DISPLAY_COUNT(&world);
  IMAGE_COUNTER = (IMAGE_COUNTER + 1) & 0xfff;
  MOVE_TRP(&world);
  move_objects(&world);
  move_player(&world);
  MOVE_NMI(&world);
  MOVE_TRASH(&world);
  SET_NMI(&world);
  CROSSING_GATE(&world);
  SPRITES_ANIMATION(&world);
  scroll(&world);
  BLOC_ANIMATION(&world);
  TFR_SCREENM();
  DISPLAY_SPRITES(&world);
  if (infinite)
    world.lives = lives;
  if (NEWLEVEL_FLAG)
    return 1;
  if (GAMEOVER_FLAG)
    return 2;
  if (RESETLEVEL_FLAG == 1) {
    if (world.lives <= 0 && !infinite)
      return 2;
    if (!infinite)
      --world.lives;
    FORCE_POSE(&world);
    updatesprite(&world, &world.player.sprite, 13, fox_true);
    world.player.sprite.speedY = 15;
    fox_death_ticks = 60;
    SELECT_MUSIC(1);
    return 3;
  }
  return 0;
}
TITUS_level *fox_dos_world(void) { return &world; }
int fox_dos_health_visible(void) { return BAR_FLAG > 0; }
unsigned fox_dos_tile_phase(void) { return tile_anim; }
int fox_dos_camera_x(void) { return BITMAP_X * 16; }
int fox_dos_camera_y(void) { return BITMAP_Y * 16; }
const char *fox_dos_error(void) { return lasterror; }
void fox_dos_music(unsigned n) { SELECT_MUSIC(n); }
void fox_dos_close(void) {
  if (loaded)
    freelevel(&world);
  freeaudio();
  freefonts();
  freespritecache(&cache);
  freesprites(&actors, actor_count);
  freeobjects(&objects, object_count);
  freepixelformat(&world.pixelformat);
  SDL_FreeSurface(screen);
  SDL_FreeSurface(tilescreen);
  free(data_root);
}

SDL_Surface *fox_dos_glyph(unsigned char ch) {
  return font && (font->type[ch] == 2 || font->type[ch] == 3)
             ? (SDL_Surface *)font->sub[ch]
             : font_undefined;
}

unsigned fox_dos_level_music(void) { return LEVEL_MUSIC[world.levelid]; }
int fox_dos_picture(unsigned n, unsigned char **out) {
  char *names[] = {"TITUS.SQZ", "TITRE.SQZ", "MENU.SQZ"};
  return n < 3 ? unSQZ(names[n], out) : -1;
}

int fox_dos_camera_locked(void) { return NOSCROLL_FLAG != 0; }
