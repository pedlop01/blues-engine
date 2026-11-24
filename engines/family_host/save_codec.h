/* Pointer-free, little-endian state. Only named table/record IDs are accepted.
 */
#include "engines/blues-family/native.h"
#include "regions.h"
#include <time.h>
#define FAMILY_SAVE_MAX (1024 * 1024)
#define FAMILY_SAVE_HEADER 80
static const char *_family_save_error = "";
static uint8_t *_family_pending_save;
static size_t _family_pending_size;
struct fs_codec {
  uint8_t *data;
  size_t pos, size;
  bool read, ok;
};
static struct family_region _fs_regions[2048];
static size_t _fs_region_count;
static void fs_region(const void *data, size_t size) {
  if (_fs_region_count < ARRAYSIZE(_fs_regions))
    _fs_regions[_fs_region_count++] = (struct family_region){data, size};
}
static void fs_regions() {
  _fs_region_count = 1;
  size_t count;
  const struct family_region *regions = family_static_regions(&count);
  for (size_t i = 0; i < count; ++i)
    fs_region(regions[i].data, regions[i].size);
#if FAMILY_SAVE_GAME == 1
  fs_region(g_res.tmp, 72728);
  fs_region(g_vars.triggers_table, sizeof(g_vars.triggers_table));
  fs_region(g_vars.tilemap_lut_init, sizeof(g_vars.tilemap_lut_init));
#else
  regions = family_boss_regions(&count);
  for (size_t i = 0; i < count; ++i)
    fs_region(regions[i].data, regions[i].size);
  regions = family_monster_regions(&count);
  for (size_t i = 0; i < count; ++i)
    fs_region(regions[i].data, regions[i].size);
  fs_region(g_res.leveldat, g_res.levellen);
  fs_region(g_vars.tile_tbl1, 256);
  fs_region(g_vars.tile_tbl2, 256);
  fs_region(g_vars.tile_tbl3, 256);
  for (int i = 0; i < OBJECTS_COUNT; ++i)
    fs_region(&g_vars.objects_tbl[i], 1);
  for (int i = 0; i < MAX_LEVEL_MONSTERS; ++i) {
    fs_region(&g_res.level.monsters_tbl[i], 1);
    fs_region(&g_res.restart.monsters_tbl[i], 1);
  }
  for (int i = 0; i < MAX_LEVEL_PLATFORMS; ++i)
    fs_region(&g_res.level.platforms_tbl[i], 1);
  for (int i = 0; i < MAX_LEVEL_ITEMS; ++i)
    fs_region(&g_res.level.items_tbl[i], 1);
  for (int i = 0; i < MAX_LEVEL_BONUSES; ++i)
    fs_region(&g_res.level.bonuses_tbl[i], 1);
#endif
}
static uint64_t fs_unsigned(struct fs_codec *c, uint64_t value, int width) {
  if (!c->ok || c->pos > c->size || (size_t)width > c->size - c->pos) {
    c->ok = false;
    return 0;
  }
  if (c->read) {
    value = 0;
    for (int i = 0; i < width; ++i)
      value |= (uint64_t)c->data[c->pos + i] << (i * 8);
  } else
    for (int i = 0; i < width; ++i)
      c->data[c->pos + i] = (uint8_t)(value >> (i * 8));
  c->pos += width;
  return value;
}
#define FS_FIELD(v, w)                                                         \
  do {                                                                         \
    uint64_t n = fs_unsigned(c, (uint64_t)(v), w);                             \
    if (c->read && c->ok)                                                      \
      (v) = n;                                                                 \
  } while (0)
#define FS_BOOL(v)                                                             \
  do {                                                                         \
    uint64_t n = fs_unsigned(c, (v), 1);                                       \
    if (n > 1)                                                                 \
      c->ok = false;                                                           \
    else if (c->read && c->ok)                                                 \
      (v) = n;                                                                 \
  } while (0)
static bool fs_reference(const void *p, uint32_t *id, uint32_t *offset) {
  if (!p) {
    *id = *offset = 0;
    return true;
  }
  uintptr_t address = (uintptr_t)p;
  for (size_t i = 1; i < _fs_region_count; ++i) {
    uintptr_t base = (uintptr_t)_fs_regions[i].data;
    if (base && address >= base && address - base < _fs_regions[i].size) {
      *id = i;
      *offset = address - base;
      return true;
    }
  }
  return false;
}
static const void *fs_pointer(struct fs_codec *c, const void *pointer) {
  uint32_t id = 0, offset = 0;
  if (!c->read && !fs_reference(pointer, &id, &offset)) {
    c->ok = false;
    return NULL;
  }
  FS_FIELD(id, 4);
  FS_FIELD(offset, 4);
  if (!c->read)
    return pointer;
  if (id == 0) {
    if (offset)
      c->ok = false;
    return NULL;
  }
  if (id >= _fs_region_count || !_fs_regions[id].data ||
      offset >= _fs_regions[id].size) {
    c->ok = false;
    return NULL;
  }
  return (const uint8_t *)_fs_regions[id].data + offset;
}
#define FS_REF(v)                                                              \
  do {                                                                         \
    const void *p = fs_pointer(c, v);                                          \
    if (c->read && c->ok)                                                      \
      (v) = (void *)p;                                                         \
  } while (0)
static void fs_bytes(struct fs_codec *c, uint8_t *data, size_t size) {
  if (!c->ok || c->pos > c->size || size > c->size - c->pos) {
    c->ok = false;
    return;
  }
  if (c->read)
    memcpy(data, c->data + c->pos, size);
  else
    memcpy(c->data + c->pos, data, size);
  c->pos += size;
}
static void family_object_codec(struct fs_codec *c, struct object_t *o,
                                int index) {
  FS_FIELD(o->x_pos, 2);
  FS_FIELD(o->y_pos, 2);
  FS_FIELD(o->spr_num, 2);
#if FAMILY_SAVE_GAME == 1
  (void)index;
  for (int i = 0; i < 11; ++i) {
    const int type = object82_type(o);
    const bool pointer = (index < 0 && i == 2) ||
                         (o->spr_num != 0xffff && index >= 82 && index < 102 &&
                          (i == 1 || (i == 4 && (type == 0 || type == 7)) ||
                           (i == 7 && (type == 2 || type == 3))));
    uint8_t kind = pointer ? 1 : 0;
    FS_FIELD(kind, 1);
    if (kind != (pointer ? 1 : 0)) {
      c->ok = false;
      return;
    }
    if (pointer) {
      bool was_ok = c->ok;
      FS_REF(o->data[i].p);
      if (was_ok && !c->ok)
        print_warning("Unresolved save object %d field %d", index, i);
    } else {
      FS_FIELD(o->data[i].w, 2);
    }
  }
#else
  FS_FIELD(o->x_velocity, 2);
  FS_FIELD(o->x_friction, 1);
  if (index == 1) {
    FS_FIELD(o->data.p.hdir, 2);
    FS_FIELD(o->data.p.current_anim_num, 1);
    FS_REF(o->data.p.anim);
    FS_FIELD(o->data.p.y_velocity, 2);
    FS_FIELD(o->data.p.special_anim_num, 1);
  } else if (index >= 2 && index <= 5) {
    FS_REF(o->data.c.anim);
    FS_FIELD(o->data.c.y_velocity, 2);
  } else if (index >= 11 && index <= 22) {
    FS_FIELD(o->data.m.flags, 1);
    FS_REF(o->data.m.ref);
    FS_FIELD(o->data.m.x_velocity, 2);
    FS_FIELD(o->data.m.y_velocity, 2);
    FS_REF(o->data.m.anim);
    FS_FIELD(o->data.m.state, 1);
    FS_FIELD(o->data.m.energy, 1);
    FS_FIELD(o->data.m.hit_jump_counter, 1);
  } else {
    FS_REF(o->data.t.ref);
    FS_FIELD(o->data.t.counter, 2);
    FS_FIELD(o->data.t.y_velocity, 2);
  }
  FS_FIELD(o->hit_counter, 1);
#endif
}
#if FAMILY_SAVE_GAME == 1
#include "../../ja/state_fields.h"
#else
#include "../../p2/state_fields.h"
#endif
static uint32_t fs_crc(const uint8_t *data, size_t size) {
  uint32_t crc = ~0u;
  while (size--) {
    crc ^= *data++;
    for (int i = 0; i < 8; ++i)
      crc = (crc >> 1) ^ ((0u - (crc & 1)) & 0xedb88320u);
  }
  return ~crc;
}
static uint32_t fs_u32(const uint8_t *p) { return READ_LE_UINT32(p); }
static void fs_put32(uint8_t *p, uint32_t n) {
  for (int i = 0; i < 4; ++i)
    p[i] = n >> (8 * i);
}
static bool fs_hash_file(uint32_t *hash, const char *name) {
  FILE *fp = g_options.dos_datapath ? fopen_nocase(g_options.dos_datapath, name)
                                    : NULL;
  if (!fp)
    return false;
  for (const char *p = name; *p; ++p) {
    *hash ^= (uint8_t)*p;
    *hash *= 16777619u;
  }
  uint8_t buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), fp)))
    for (size_t i = 0; i < n; ++i) {
      *hash ^= buf[i];
      *hash *= 16777619u;
    }
  bool ok = !ferror(fp);
  fclose(fp);
  return ok;
}
static bool family_save_signature(int level, uint32_t *signature) {
  if (level < 0 || level >= FAMILY_LEVEL_COUNT ||
      !family_level_available(level))
    return false;
  uint32_t hash = 2166136261u;
#if FAMILY_SAVE_GAME == 1
  const char *files[] = {"font256.eat", "board.eat", "motif.eat", "sprites.eat",
                         "samples.eat"};
  int group = READ_BE_UINT16(level_data1p + level * 2) >> 8;
  if (group < 0 || group >= 4 || !fs_hash_file(&hash, _background[group]) ||
      !fs_hash_file(&hash, _decor[group]))
    return false;
#else
  const char *files[] = {"MAP.SQZ",     "MOTIF.SQZ", "ALLFONTS.SQZ",
                         "SPRITES.SQZ", "FRONT.SQZ", "UNION.SQZ",
                         "KEYB.SQZ",    "SAMPLE.SQZ"};
  static const uint8_t groups[] = {0, 0, 0, 1, 1, 1, 2, 3,
                                   3, 0, 4, 4, 4, 5, 0, 2};
  char name[16];
  snprintf(name, sizeof(name), "LEVEL%c.SQZ",
           level < 9 ? '1' + level : 'A' + level - 9);
  if (!fs_hash_file(&hash, name))
    return false;
  snprintf(name, sizeof(name), "BACK%d.SQZ", groups[level]);
  if (!fs_hash_file(&hash, name))
    return false;
#endif
  for (size_t i = 0; i < ARRAYSIZE(files); ++i)
    if (!fs_hash_file(&hash, files[i]))
      return false;
  *signature = hash;
  return true;
}
static bool fs_envelope(const uint8_t *data, size_t size) {
  return size >= FAMILY_SAVE_HEADER + SAVE_PREVIEW_BYTES &&
         size <= FAMILY_SAVE_MAX && !memcmp(data, "TITUSFS\n", 8) &&
         fs_u32(data + 8) == 1 && fs_u32(data + 12) == FAMILY_SAVE_GAME &&
         fs_u32(data + 16) == size &&
         fs_crc(data + 24, size - 24) == fs_u32(data + 20) &&
         fs_u32(data + 28) < FAMILY_LEVEL_COUNT && fs_u32(data + 52) <= 1 &&
         fs_u32(data + 56) <= 1 && fs_u32(data + 60) <= 8 &&
         fs_u32(data + 64) <= 255 && fs_u32(data + 68) <= 255;
}
int family_native_inspect(const uint8_t *data, size_t size,
                          struct titus_slot_info *info) {
  memset(info, 0, sizeof(*info));
  if (size >= 16 && (!memcmp(data, "TITUSFS\n", 8)) &&
      (fs_u32(data + 8) != 1 || fs_u32(data + 12) != FAMILY_SAVE_GAME)) {
    info->version = -1;
    return 0;
  }
  if (!fs_envelope(data, size))
    return 0;
  info->decoded = 1;
  info->version = 1;
  info->preview = 1;
  info->resources = fs_u32(data + 24);
  info->level = fs_u32(data + 28);
  info->timestamp = fs_u32(data + 32) | ((uint64_t)fs_u32(data + 36) << 32);
  info->x = (int32_t)fs_u32(data + 40);
  info->y = (int32_t)fs_u32(data + 44);
  info->frame = fs_u32(data + 48);
  info->camera = fs_u32(data + 52);
  info->hud = fs_u32(data + 56);
  info->view = fs_u32(data + 60);
  info->lives1 = fs_u32(data + 64);
  info->lives2 = fs_u32(data + 68);
  uint32_t signature;
  info->compatible = family_save_signature(info->level, &signature) &&
                     signature == info->resources;
  return info->decoded;
}
int family_native_preview(const uint8_t *data, size_t size, uint8_t *rgb) {
  if (!fs_envelope(data, size))
    return 0;
  memcpy(rgb, data + size - SAVE_PREVIEW_BYTES, SAVE_PREVIEW_BYTES);
  return 1;
}
int family_native_prepare(int slot) {
  uint8_t *data = malloc(FAMILY_SAVE_MAX);
  size_t size = 0;
  struct titus_slot_info info = {0};
  _family_save_error = "GUARDADO DANADO O INCOMPATIBLE";
  bool ok = data && g_sys.read_save_file &&
            g_sys.read_save_file(slot, data, FAMILY_SAVE_MAX, &size) &&
            family_native_inspect(data, size, &info) && info.compatible;
  if (!ok) {
    free(data);
    return -1;
  }
  free(_family_pending_save);
  _family_pending_save = data;
  _family_pending_size = size;
  return info.level;
}
static const char *family_save_error() { return _family_save_error; }
static bool family_prepare_load(int slot) {
  int level = family_native_prepare(slot);
  if (level < 0)
    return false;
  _family_requested_level = level;
  return true;
}
struct fs_view {
  double x, y, width, height;
  int cheats;
  bool infinite;
};
static void fs_real(struct fs_codec *c, double *v) {
  uint64_t n = 0;
  memcpy(&n, v, 8);
  n = fs_unsigned(c, n, 8);
  if (c->read) {
    memcpy(v, &n, 8);
    if (!isfinite(*v) || fabs(*v) > 100000)
      c->ok = false;
  }
}
static void fs_memory_codec(struct fs_codec *c, struct family_memory *s,
                            struct fs_view *view) {
  fs_bytes(c, s->palette, sizeof(s->palette));
  family_vars_codec(c, &s->vars);
  family_vars_codec(c, &s->render);
#if FAMILY_SAVE_GAME == 1
  fs_bytes(c, s->tmp, sizeof(s->tmp));
#else
  uint32_t length = g_res.levellen;
  FS_FIELD(length, 4);
  if (length != (uint32_t)g_res.levellen) {
    c->ok = false;
    return;
  }
  family_level_codec(c, &s->level);
  family_level_codec(c, &s->restart);
  fs_bytes(c, (uint8_t *)(s + 1), length);
#endif
  fs_real(c, &view->x);
  fs_real(c, &view->y);
  fs_real(c, &view->width);
  fs_real(c, &view->height);
  FS_FIELD(view->cheats, 4);
  FS_BOOL(view->infinite);
}
static bool fs_valid_memory(const struct family_memory *s, int level,
                            const struct fs_view *view) {
  if (view->width < 320 || view->width > 2048 || view->height < 200 ||
      view->height > 2048 || view->x < 0 || view->y < 0 || view->cheats < 0 ||
      view->cheats > 31)
    return false;
#if FAMILY_SAVE_GAME == 1
  return s->vars.level == level + 1 && s->render.level == level + 1 &&
         s->vars.player >= 0 && s->vars.player <= 1 &&
         s->vars.tilemap_w == g_vars.tilemap_w &&
         s->vars.tilemap_h == g_vars.tilemap_h &&
         s->vars.tilemap_type == g_vars.tilemap_type && s->vars.level_num < 4 &&
         s->vars.triggers_table[18] <= TRIGGERS_COUNT;
#else
  return s->vars.level_num == level && s->render.level_num == level &&
         s->vars.tilemap.size == g_vars.tilemap.size &&
         s->vars.tilemap.h == g_vars.tilemap.h &&
         s->level.monsters_count <= MAX_LEVEL_MONSTERS &&
         s->restart.monsters_count <= MAX_LEVEL_MONSTERS &&
         s->vars.player_club_type < 4;
#endif
}
static bool family_save_game(int slot) {
  _family_save_error = "NO SE PUEDE GUARDAR ESTE ESTADO";
  if (!_family_active || _family_rewinding)
    return false;
#if FAMILY_SAVE_GAME == 1
  if (g_vars.player == 2)
    return false;
#endif
  uint32_t signature;
  if (!family_save_signature(family_level(), &signature))
    return false;
  struct family_memory *state = calloc(1, family_state_size());
  uint8_t *data = calloc(1, FAMILY_SAVE_MAX);
  if (!state || !data) {
    free(state);
    free(data);
    return false;
  }
  family_capture(state);
  fs_regions();
  struct fs_view view = {_family_x,        _family_y,
                         _family_width,    _family_height,
                         g_options.cheats, g_options.family_infinite_lives};
  struct fs_codec codec = {data, FAMILY_SAVE_HEADER,
                           FAMILY_SAVE_MAX - SAVE_PREVIEW_BYTES, false, true};
  fs_memory_codec(&codec, state, &view);
  bool ok = codec.ok && fs_valid_memory(state, family_level(), &view);
  if (ok) {
    size_t size = codec.pos + SAVE_PREVIEW_BYTES;
    memcpy(data, "TITUSFS\n", 8);
    fs_put32(data + 8, 1);
    fs_put32(data + 12, FAMILY_SAVE_GAME);
    fs_put32(data + 16, size);
    fs_put32(data + 24, signature);
    fs_put32(data + 28, family_level());
    uint64_t stamp = (uint64_t)time(NULL);
    fs_put32(data + 32, stamp);
    fs_put32(data + 36, stamp >> 32);
#if FAMILY_SAVE_GAME == 1
    fs_put32(data + 40, g_vars.players_table[0].obj.x_pos);
    fs_put32(data + 44, g_vars.players_table[0].obj.y_pos);
    fs_put32(data + 48, g_vars.level_loop_counter);
#else
    fs_put32(data + 40, g_vars.objects_tbl[1].x_pos);
    fs_put32(data + 44, g_vars.objects_tbl[1].y_pos);
    fs_put32(data + 48, g_vars.level_draw_counter);
#endif
    fs_put32(data + 52, g_options.camera_mode);
    fs_put32(data + 56, g_options.hud_mode);
    fs_put32(data + 60, g_sys.input.view_step);
    fs_put32(data + 64, family_lives(0));
    fs_put32(data + 68, family_lives(1));
    ok = g_sys.capture_save_preview &&
         g_sys.capture_save_preview(data + codec.pos);
    if (ok) {
      fs_put32(data + 20, fs_crc(data + 24, size - 24));
      ok = g_sys.write_save_file && g_sys.write_save_file(slot, data, size);
    }
  }
  free(state);
  free(data);
  _family_save_error =
      ok ? "PARTIDA GUARDADA" : "NO SE PUDO GUARDAR LA PARTIDA";
  return ok;
}
static void family_inspect_save(int slot, struct sys_save_slot *out) {
  memset(out, 0, sizeof(*out));
  out->reason = "SIN PARTIDA";
  int status = g_sys.save_file_status ? g_sys.save_file_status(slot) : -1;
  if (!status) {
    out->status = SAVE_EMPTY;
    return;
  }
  out->status = SAVE_UNREADABLE;
  out->reason = "NO SE PUDO LEER";
  uint8_t *data = malloc(FAMILY_SAVE_MAX);
  size_t size = 0;
  struct titus_slot_info info = {0};
  if (data && g_sys.read_save_file &&
      g_sys.read_save_file(slot, data, FAMILY_SAVE_MAX, &size)) {
    family_native_inspect(data, size, &info);
    out->status = info.version == -1 ? SAVE_INCOMPATIBLE : SAVE_DAMAGED;
    out->reason = "GUARDADO DANADO O INCOMPATIBLE";
    if (info.decoded) {
      out->status = info.compatible ? SAVE_READY : SAVE_INCOMPATIBLE;
      out->reason = info.compatible ? "LISTA PARA CARGAR"
                                    : "DATOS DEL JUEGO INCOMPATIBLES";
      out->level = info.level;
      out->lives[0] = info.lives1;
      out->lives[1] = info.lives2;
      out->timestamp = info.timestamp;
      out->has_metadata = true;
      out->has_preview = family_native_preview(data, size, out->preview);
    }
  }
  free(data);
}
static bool family_apply_pending() {
  if (!_family_pending_save)
    return true;
  struct family_memory *state = calloc(1, family_state_size());
  struct fs_view view = {0};
  fs_regions();
  struct fs_codec codec = {_family_pending_save, FAMILY_SAVE_HEADER,
                           _family_pending_size - SAVE_PREVIEW_BYTES, true,
                           state != NULL};
  if (state)
    fs_memory_codec(&codec, state, &view);
  int level = fs_u32(_family_pending_save + 28);
  bool ok = codec.ok && codec.pos == codec.size &&
            fs_valid_memory(state, level, &view);
  if (ok) {
    family_restore(state);
    g_options.camera_mode = fs_u32(_family_pending_save + 52);
    g_options.hud_mode = fs_u32(_family_pending_save + 56);
    g_sys.input.view_step = fs_u32(_family_pending_save + 60);
    g_options.cheats = view.cheats;
    g_options.family_infinite_lives = view.infinite;
    _family_x = view.x;
    _family_y = view.y;
    _family_width = view.width;
    _family_height = view.height;
    family_resume_music();
    family_cut();
    family_timeline_clear();
    family_timeline_record();
#ifdef TITUS_MANAGED
    titus_runtime_restored(level, (int32_t)fs_u32(_family_pending_save + 40),
                           (int32_t)fs_u32(_family_pending_save + 44),
                           fs_u32(_family_pending_save + 48),
                           g_options.camera_mode, g_options.hud_mode,
                           g_sys.input.view_step);
#endif
  }
  free(state);
  free(_family_pending_save);
  _family_pending_save = NULL;
  _family_pending_size = 0;
  return ok;
}
#undef FS_FIELD
#undef FS_BOOL
#undef FS_REF
