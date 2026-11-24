// SPDX-License-Identifier: GPL-3.0-or-later
#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
extern "C" {
SDL_Surface *fox_dos_glyph(unsigned char);
}
namespace titus::fox_dos {
std::uint64_t &active_serial() {
  static std::uint64_t value = 0;
  return value;
}
std::uint64_t fresh_serial() {
  static std::uint64_t value = 0;
  return ++value;
}
void load(unsigned level, int lives) {
  active_serial() = 0;
  if (level < 1 || level > 15 || lives < 0 || lives > 99 ||
      fox_dos_load(level - 1, lives) < 0)
    throw std::runtime_error(fox_dos_error());
}
Snapshot snapshot() {
  Snapshot result;
  result.level = fox_dos_world()->levelnumber + 1;
  auto visit = [](void *p, const char *, long long v) {
    static_cast<Snapshot *>(p)->values.push_back(v);
    return v;
  };
  if (!fox_dos_values(visit, &result, 0))
    throw std::runtime_error("Unrepresentable Fox state");
  return result;
}
void restore(const Snapshot &s) {
  active_serial() = 0;
  if (s.level < 1 || s.level > 15 || s.values.size() > 100000)
    throw std::runtime_error("Invalid Fox state size");
  auto old = snapshot();
  bool changed = old.level != s.level;
  if (changed)
    load(s.level);
  struct Reader {
    const std::vector<long long> &v;
    std::size_t pos = 0;
    bool ok = true;
  };
  auto visit = [](void *p, const char *, long long v) {
    auto &r = *static_cast<Reader *>(p);
    if (r.pos >= r.v.size()) {
      r.ok = false;
      return v;
    }
    return r.v[r.pos++];
  };
  Reader r{s.values};
  if (!fox_dos_values(visit, &r, 1) || !r.ok || r.pos != s.values.size()) {
    if (changed)
      load(old.level);
    Reader rollback{old.values};
    fox_dos_values(visit, &rollback, 1);
    throw std::runtime_error("Invalid Fox snapshot fields");
  }
}
Json save(const Snapshot &s, const std::string &fp) {
  Json payload = {{"level", s.level}, {"values", s.values}};
  return {{"schema_version", 1},
          {"format_id", "fox.dos.json"},
          {"fingerprint", fp},
          {"payload_sha256", titus::sha256(payload.dump())},
          {"payload", payload}};
}
Snapshot decode(const Json &j, const std::string &fp) {
  if (j.value("schema_version", 0) != 1 ||
      j.value("format_id", std::string()) != "fox.dos.json" ||
      j.at("fingerprint") != fp)
    throw std::runtime_error("Incompatible Fox DOS save");
  auto &p = j.at("payload");
  if ((p.size() != 2 && p.size() != 3) ||
      j.at("payload_sha256") != titus::sha256(p.dump()) ||
      !p.at("values").is_array() || p.at("values").size() > 100000)
    throw std::runtime_error("Damaged Fox DOS save");
  Snapshot s;
  s.level = p.at("level");
  if (s.level < 1 || s.level > 15)
    throw std::runtime_error("Invalid Fox level");
  for (auto &v : p.at("values")) {
    if (!v.is_number_integer())
      throw std::runtime_error("Invalid Fox state value");
    s.values.push_back(v.get<long long>());
  }
  return s;
}
namespace {
std::vector<TITUS_sprite *> sprites() {
  auto *w = fox_dos_world();
  std::vector<TITUS_sprite *> r;
  for (int i = w->elevatorcount - 1; i >= 0; --i)
    r.push_back(&w->elevator[i].sprite);
  for (int i = w->trashcount - 1; i >= 0; --i)
    r.push_back(&w->trash[i]);
  for (int i = w->enemycount - 1; i >= 0; --i)
    r.push_back(&w->enemy[i].sprite);
  for (int i = w->objectcount - 1; i >= 0; --i)
    r.push_back(&w->object[i].sprite);
  r.push_back(&w->player.sprite3);
  r.push_back(&w->player.sprite2);
  r.push_back(&w->player.sprite);
  return r;
}
std::uint32_t color(SDL_Color c) {
  return 0xff000000 | (unsigned(c.r) << 16) | (unsigned(c.g) << 8) | c.b;
}
void blit(Frame &f, SDL_Surface *s, double left, double top, double scale,
          const SDL_Palette *pal, bool transparent, bool flip = false,
          bool flash = false) {
  if (!s || scale <= 0)
    return;
  int x0 = std::max(0, int(std::floor(left))),
      y0 = std::max(0, int(std::floor(top))),
      x1 = std::min(int(f.width), int(std::ceil(left + s->w * scale))),
      y1 = std::min(int(f.height), int(std::ceil(top + s->h * scale)));
  for (int y = y0; y < y1; ++y) {
    int sy = std::clamp(int((y + .5 - top) / scale), 0, s->h - 1);
    auto *row = static_cast<const Uint8 *>(s->pixels) + sy * s->pitch;
    for (int x = x0; x < x1; ++x) {
      int sx = std::clamp(int((x + .5 - left) / scale), 0, s->w - 1);
      if (flip)
        sx = s->w - 1 - sx;
      unsigned n = row[sx];
      if (transparent && !n)
        continue;
      if (n >= unsigned(pal->ncolors))
        continue;
      f.pixels[y * f.width + x] =
          flash ? (n & 1 ? 0xffffffff : 0xff000000) : color(pal->colors[n]);
    }
  }
}
} // namespace
std::vector<Pose> poses() {
  std::vector<Pose> r;
  for (auto *s : sprites())
    r.push_back({s->x, s->y});
  return r;
}
void rectangle(Frame &f, int x, int y, int w, int h, std::uint32_t c) {
  int endx = std::min(int(f.width), x + w),
      endy = std::min(int(f.height), y + h);
  for (int iy = std::max(0, y); iy < endy; ++iy)
    std::fill(f.pixels.begin() + iy * f.width + std::max(x, 0),
              f.pixels.begin() + iy * f.width + std::max(endx, 0), c);
}
void text(Frame &f, int x, int y, const std::string &str, std::uint32_t tint,
          unsigned scale) {
  for (unsigned char c : str) {
    auto *s = fox_dos_glyph(static_cast<unsigned char>(std::toupper(c)));
    if (s)
      for (int sy = 0; sy < s->h; ++sy)
        for (int sx = 0; sx < s->w; ++sx) {
          auto n = *(static_cast<Uint8 *>(s->pixels) + sy * s->pitch + sx);
          if (n != 1)
            rectangle(f, x + sx * int(scale), y + sy * int(scale), scale, scale,
                      tint);
        }
    x += 8 * scale;
  }
}
Frame render(const View &v, const std::vector<Pose> &previous) {
  Frame f{
      v.width, v.height,
      std::vector<std::uint32_t>(std::size_t(v.width) * v.height, 0xff000000)};
  auto *w = fox_dos_world();
  double sx = v.width / 320., sy = v.height / 200.,
         scale = std::min(sx, sy) * v.zoom;
  double width = v.width / scale, height = v.height / scale;
  double ox = (v.width - 320 * scale) / 2 - v.x * scale,
         oy = (v.height - 200 * scale) / 2 - v.y * scale;
  int firstx = std::max(0, int(std::floor(-ox / (16 * scale)))),
      lastx = std::min(w->width, int(std::ceil((v.width - ox) / (16 * scale))));
  int firsty = std::max(0, int(std::floor(-oy / (16 * scale)))),
      lasty =
          std::min(w->height, int(std::ceil((v.height - oy) / (16 * scale))));
  for (int y = firsty; y < lasty; ++y)
    for (int x = firstx; x < lastx; ++x) {
      auto &tile = w->tile[w->tilemap[y][x]];
      unsigned id = tile.animation[fox_dos_tile_phase() % 3];
      blit(f, w->tile[id].tiledata, ox + x * 16 * scale, oy + y * 16 * scale,
           scale, w->pixelformat->palette, false);
    }
  auto list = sprites();
  for (std::size_t i = 0; i < list.size(); ++i) {
    auto *p = list[i];
    if (!p->enabled || (p->invisible && i >= unsigned(w->elevatorcount)) ||
        !p->spritedata)
      continue;
    double x = p->x, y = p->y;
    if (i < previous.size() && std::abs(previous[i].x - p->x) < 80 &&
        std::abs(previous[i].y - p->y) < 80) {
      x = previous[i].x + (x - previous[i].x) * v.alpha;
      y = previous[i].y + (y - previous[i].y) * v.alpha;
    }
    auto *d = p->spritedata;
    double left = p->flipped ? x + d->refwidth - d->data->w : x - d->refwidth;
    double top = y + d->refheight - d->data->h + 1;
    blit(f, d->data, ox + left * scale, oy + top * scale, scale,
         w->pixelformat->palette, true, p->flipped, p->flash);
  }
  unsigned ui = std::max(1u, std::min(v.width / 320, v.height / 200));
  if (v.classic_hud &&
      fox_dos_health_visible()) { // Original DOS health-bar motif, stable under
                                  // map zoom.
    for (int n = 0; n < 16; ++n)
      rectangle(f, (96 + n * 8) * ui, (n < w->player.hp ? 9 : 15) * ui, 4 * ui,
                (n < w->player.hp ? 16 : 4) * ui, 0xffffffff);
  } else if (!v.classic_hud) {
    rectangle(f, 4 * ui, 4 * ui, 145 * ui, 18 * ui, 0xff182334);
    text(f, 8 * ui, 7 * ui,
         "VIDAS " + std::to_string(w->lives) + "  NIVEL " +
             std::to_string(w->levelnumber == 14 ? 16 : w->levelnumber + 1),
         0xffffd76a, ui);
  }
  return f;
}
Frame picture(unsigned which, unsigned scale) {
  unsigned char *raw = nullptr;
  int size = fox_dos_picture(which, &raw);
  std::unique_ptr<unsigned char, decltype(&std::free)> data(raw, std::free);
  if (size < 64768 || scale < 1 || scale > 4)
    throw std::runtime_error("Invalid original DOS intro picture");
  Frame f{320 * scale, 200 * scale,
          std::vector<std::uint32_t>(64000 * scale * scale)};
  for (unsigned y = 0; y < f.height; ++y)
    for (unsigned x = 0; x < f.width; ++x) {
      unsigned n = data.get()[768 + (y / scale) * 320 + x / scale];
      f.pixels[y * f.width + x] = 0xff000000 |
                                  (unsigned(data.get()[n * 3]) * 4 << 16) |
                                  (unsigned(data.get()[n * 3 + 1]) * 4 << 8) |
                                  unsigned(data.get()[n * 3 + 2]) * 4;
    }
  return f;
}
void bmp(const fs::path &path, const Frame &f) {
  std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> s(
      SDL_CreateRGBSurfaceWithFormatFrom(
          const_cast<std::uint32_t *>(f.pixels.data()), f.width, f.height, 32,
          f.width * 4, SDL_PIXELFORMAT_ARGB8888),
      SDL_FreeSurface);
  if (!s || SDL_SaveBMP(s.get(), filesystem_path(path).u8string().c_str()))
    throw std::runtime_error(SDL_GetError());
}
} // namespace titus::fox_dos
