// SPDX-License-Identifier: GPL-3.0-or-later
// Native DOS host; common SDL audio/controls/menu conventions match Fox.
#include "../dos_host/controls.hpp"
#include "audio.hpp"
#include "host_game.hpp"
#include "managed.hpp"

#include "titus/process.hpp"
#include <SDL.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <deque>
#include <iostream>
#include <memory>
using namespace titus::fox_dos;
namespace fs = std::filesystem;
namespace {
std::unique_ptr<ManagedHost> managed;
int run(const std::vector<std::string> &input_args) {
  auto args = input_args;
  if (args.size() == 3 &&
      (args[1] == "--session-request" || args[1] == "--inspect-request" ||
       args[1] == "--prepare-request")) {
    managed = std::make_unique<ManagedHost>(args);
    if (managed->inspection()) {
      managed->inspect();
      return 0;
    }
    args = managed->arguments();
  }
  fs::path root, work, capture, script;
  unsigned level = 1, test_ms = 0, render_hz = 60, probe_ticks = 0;
  bool level_argument = false;
  for (unsigned i = 1; i < args.size(); i += 2) {
    if (i + 1 >= args.size())
      throw std::runtime_error("Missing argument value");
    const auto &key = args[i];
    const auto &v = args[i + 1];
    if (key == "--datapath")
      root = fs::u8path(v);
    else if (key == "--probe-ticks")
      probe_ticks = unsigned(std::stoul(v));
    else if (key == "--render-hz")
      render_hz = unsigned(std::stoul(v));
    else if (key == "--workdir")
      work = fs::u8path(v);
    else if (key == "--capture")
      capture = fs::u8path(v);
    else if (key == "--test-script")
      script = fs::u8path(v);
    else if (key == "--level") {
      level = std::stoul(v);
      level_argument = true;
    } else if (key == "--test-ms")
      test_ms = std::stoul(v);
    else
      throw std::runtime_error("Unknown argument: " + key);
  }
  if (probe_ticks) {
    if (probe_ticks > 2000)
      throw std::runtime_error("Probe tick limit");
    test_ms = 120000;
  }
  if (root.empty() || level < 1 || level > 16 || level == 15 ||
      test_ms > 120000)
    throw std::runtime_error("Use --datapath DIRECTORY [--level 1..14/16]");
  if (render_hz != 30 && render_hz != 60 && render_hz != 120)
    throw std::runtime_error("Use render Hz 30/60/120");
  root = titus::normalized_path(root);
  if (work.empty())
    work = titus::Paths::resolve().data / "standalone/fox-dos";
  work = titus::normalized_path(work);
  auto inside = [](const fs::path &x, const fs::path &y) {
    auto r = x.lexically_relative(y);
    return !r.empty() && *r.begin() != "..";
  };
  if (!managed && (inside(work, root) || inside(root, work)))
    throw std::runtime_error("Storage overlaps original resources");
  titus::ensure_directory(work);
  Assets assets(root);
  Game game;
  game.start(assets, internal_level(level));
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER |
               SDL_INIT_TIMER))
    throw std::runtime_error(SDL_GetError());
  struct Quit {
    ~Quit() { SDL_Quit(); }
  } quit;
  std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
      SDL_CreateWindow("Titus the Fox DOS — Titus Engine",
                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 960, 600,
                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI),
      SDL_DestroyWindow);
  if (!window)
    throw std::runtime_error(SDL_GetError());
  std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(
      SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED),
      SDL_DestroyRenderer);
  if (!renderer)
    renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE));
  if (!renderer)
    throw std::runtime_error(SDL_GetError());
  std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture(
      nullptr, SDL_DestroyTexture);
  std::unique_ptr<SDL_GameController, decltype(&SDL_GameControllerClose)>
      controller(nullptr, SDL_GameControllerClose);
  auto connect = [&] {
    if (controller && SDL_GameControllerGetAttached(controller.get()))
      return;
    controller.reset();
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
      if (SDL_IsGameController(i)) {
        controller.reset(SDL_GameControllerOpen(i));
        if (controller)
          break;
      }
  };
  connect();
  Audio audio;
  audio.open();
  unsigned playing = 0;
  auto music_for = [&] {
    game.bind();
    unsigned which = game.state.game_over  ? 99
                     : game.level_complete ? 100 + game.state.current_level
                                           : game.state.current_level;
    if (which != playing) {
      audio.clear_effects();
      fox_dos_music(game.state.game_over  ? 1
                    : game.level_complete ? 0
                                          : fox_dos_level_music());
      playing = which;
    }
  };
  bool running = true, menu = false, controls = false, smooth = true,
       classic_hud = true, fullscreen = false, invulnerable = false,
       infinite = false, rewinding = false;
  unsigned selected = 0, page = 0, music = 80, effects = 80, initial_lives = 3,
           action_button = 0, jump_button = 1, intro = test_ms ? 0 : 1,
           frames = 0, rewound = 0;
  double zoom = 1, target_zoom = 1, camera = 0, accumulator = 0,
         intro_elapsed = 0;
  std::string message;
  const auto fingerprint = managed
                               ? managed->fingerprint()
                               : titus::sha256("fox.dos:" + assets.fingerprint);
  titus::dos_host::Keyboard keyboard;
  unsigned control_selected = 0;
  bool capture_key = false;
  const auto preferences = work / "preferences.json";
  if (titus::path_exists(preferences)) {
    const auto p = titus::parse_json(titus::read_document(preferences, 32768));
    if (p.value("schema_version", 0) != 1)
      throw std::runtime_error("Unsupported native preferences");
    if (!level_argument)
      level = p.value("start_level", level);
    if (level < 1 || level > 16 || level == 15)
      throw std::runtime_error("Invalid saved starting level");
    if (p.contains("keyboard"))
      keyboard.read(p["keyboard"]);
    smooth = p.value("camera", std::string("smooth")) == "smooth";
    classic_hud = p.value("hud", std::string("classic")) == "classic";
    invulnerable = p.value("invulnerable", false);
    infinite = p.value("infinite_lives", false);
    initial_lives = p.value("lives", 3u);
    render_hz = p.value("render_hz", render_hz);
    if (render_hz != 30 && render_hz != 60 && render_hz != 120)
      render_hz = 60;
    music = p.value("music_volume", 80u);
    effects = p.value("effects_volume", 80u);
    target_zoom = p.value("zoom", 1.);
    action_button = p.value("action_button", 0u);
    jump_button = p.value("jump_button", 1u);
    fullscreen = p.value("fullscreen", false);
    if (initial_lives > 99 || music > 100 || effects > 100 ||
        target_zoom < 1. / 3 || target_zoom > 2 || action_button > 14 ||
        jump_button > 14 || action_button == jump_button)
      throw std::runtime_error("Invalid native preferences");
    game.start(assets, internal_level(level), initial_lives);
    zoom = target_zoom;
  }
  if (managed) {
    const auto &settings = managed->settings();
    if (settings.contains("video"))
      fullscreen = settings["video"].value("fullscreen", fullscreen);
    if (settings.contains("audio")) {
      music = settings["audio"].value("music_volume", music);
      effects = settings["audio"].value("effects_volume", effects);
    }
    if (settings.contains("input")) {
      action_button = settings["input"].value("action_button", action_button);
      jump_button = settings["input"].value("jump_button", jump_button);
    }
    if (settings.contains("fox")) {
      const auto &p = settings["fox"];
      classic_hud = p.value("hud", std::string("classic")) == "classic";
      smooth = p.value("camera", std::string("smooth")) == "smooth";
      invulnerable = p.value("invulnerable", false);
      infinite = p.value("infinite_lives", false);
      initial_lives = p.value("lives", 3u);
      level = p.value("start_level", level);
      target_zoom = zoom = 1. / (1. + p.value("view_step", 0u) * .25);
      game.start(assets, internal_level(level), initial_lives);
    }
  }
  SDL_SetWindowFullscreen(window.get(),
                          fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
  std::deque<Game> history;
  std::array<Frame, 8> previews;
  std::array<unsigned, 8> slot_levels{};
  auto slot_path = [&](unsigned s) {
    return work / ("slot-" + std::to_string(s + 1) + ".json");
  };
  auto refresh = [&] {
    for (unsigned s = 0; s < 8; ++s) {
      slot_levels[s] = 0;
      previews[s] = {};
      try {
        if (!titus::path_exists(slot_path(s)))
          continue;
        auto doc = titus::parse_json(
            titus::read_document(slot_path(s), 2 * 1024 * 1024),
            2 * 1024 * 1024);
        auto saved = restore_game(assets, doc, fingerprint);
        slot_levels[s] = display_level(saved.state.current_level);
        auto path = work / ("slot-" + std::to_string(s + 1) + ".bmp");
        if (!titus::path_exists(path) ||
            doc.value("preview_sha256", std::string()) !=
                titus::file_sha256(path))
          continue;
        std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> image(
            SDL_LoadBMP(titus::filesystem_path(path).u8string().c_str()),
            SDL_FreeSurface);
        if (!image || image->w != 320 || image->h != 200)
          continue;
        std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> converted(
            SDL_ConvertSurfaceFormat(image.get(), SDL_PIXELFORMAT_ARGB8888, 0),
            SDL_FreeSurface);
        if (!converted)
          continue;
        auto &f = previews[s];
        f = {320, 200, std::vector<std::uint32_t>(64000)};
        for (unsigned y = 0; y < 200; ++y)
          std::copy_n(reinterpret_cast<std::uint32_t *>(
                          static_cast<Uint8 *>(converted->pixels) +
                          y * converted->pitch),
                      320, f.pixels.data() + y * 320);
      } catch (const std::exception &) {
        slot_levels[s] = 99;
      }
    }
  };
  refresh();
  double camera_y = 0;
  auto reset_view = [&] {
    game.bind();
    camera = fox_dos_camera_x();
    camera_y = fox_dos_camera_y();
    accumulator = 0;
    history.clear();
  };
  reset_view();
  auto select_level = [&](unsigned n) {
    level = display_level(n);
    game.start(assets, n, game.state.player.lives, game.state.score);
    reset_view();
    playing = 0;
    music_for();
  };
  titus::Json scripted = titus::Json::array();
  unsigned script_index = 0;
  bool test_rewind = false, pause_seen = false;
  unsigned moves_right = 0, moves_left = 0, jump_up = 0;
  if (!script.empty()) {
    if (!test_ms)
      throw std::runtime_error("Scripts require a bounded test runtime");
    scripted = titus::parse_json(titus::read_document(script, 65536));
    if (!scripted.is_array() || scripted.size() > 256)
      throw std::runtime_error("Invalid test script");
  }
  if (intro) {
    fox_dos_music(0);
    playing = 0;
  }
  if (managed && managed->action().at("type") == "load_slot") {
    unsigned slot = managed->action().at("slot");
    game =
        restore_game(assets,
                     titus::parse_json(titus::read_document(slot_path(slot - 1),
                                                            2 * 1024 * 1024),
                                       2 * 1024 * 1024),
                     fingerprint);
    reset_view();
    intro = 0;
    playing = 0;
    music_for();
  }
  if (managed)
    managed->ready(intro ? "title" : "gameplay");
  auto started = std::chrono::steady_clock::now(), last = started;
  Frame last_frame;
  bool audio_reopen = false, was_rewinding = false;
  while (running) {
    if (audio_reopen) {
      audio_reopen = false;
      try {
        audio.reopen();
      } catch (const std::exception &) {
        message = "AUDIO DESCONECTADO";
      }
    }
    struct AudioGuard {
      Audio &a;
      bool active = true;
      explicit AudioGuard(Audio &x) : a(x) { a.lock(); }
      void release() {
        if (active) {
          a.unlock();
          active = false;
        }
      }
      ~AudioGuard() { release(); }
    } guard(audio);
    if (managed && managed->poll())
      running = false;
    auto now = std::chrono::steady_clock::now();
    double elapsed =
        std::min(.1, std::chrono::duration<double>(now - last).count());
    last = now;
    unsigned ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - started)
            .count();
    if (test_ms && ms >= test_ms)
      running = false;
    while (script_index < scripted.size() &&
           scripted[script_index].at("ms").get<unsigned>() <= ms) {
      const auto &step = scripted[script_index++];
      if (step.value("audio_reset", false))
        audio_reopen = true;
      if (step.contains("rewind"))
        test_rewind = step.at("rewind");
      if (step.contains("key")) {
        SDL_Event e{};
        e.type = SDL_KEYDOWN;
        e.key.keysym.sym =
            SDL_GetKeyFromName(step.at("key").get<std::string>().c_str());
        SDL_PushEvent(&e);
      }
    }
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if ((e.type == SDL_AUDIODEVICEREMOVED &&
           e.adevice.which == audio.device) ||
          (e.type == SDL_AUDIODEVICEADDED && !audio.device)) {
        audio_reopen = true;
        continue;
      }
      if (e.type == SDL_QUIT)
        running = false;
      if (e.type == SDL_CONTROLLERDEVICEADDED ||
          e.type == SDL_CONTROLLERDEVICEREMOVED)
        connect();
      if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        auto b = e.cbutton.button;
        SDL_Keycode key = b == SDL_CONTROLLER_BUTTON_START        ? SDLK_ESCAPE
                          : b == SDL_CONTROLLER_BUTTON_DPAD_UP    ? SDLK_UP
                          : b == SDL_CONTROLLER_BUTTON_DPAD_DOWN  ? SDLK_DOWN
                          : b == SDL_CONTROLLER_BUTTON_DPAD_LEFT  ? SDLK_LEFT
                          : b == SDL_CONTROLLER_BUTTON_DPAD_RIGHT ? SDLK_RIGHT
                          : b == SDL_CONTROLLER_BUTTON_A ? SDLK_RETURN
                                                         : SDLK_UNKNOWN;
        if (key) {
          e.type = SDL_KEYDOWN;
          e.key.keysym.sym = key;
          e.key.repeat = 0;
        }
      }
      if (e.type != SDL_KEYDOWN || e.key.repeat)
        continue;
      auto key = e.key.keysym.sym;
      if (controls && capture_key) {
        if (key == SDLK_ESCAPE)
          capture_key = false;
        else if (keyboard.bind(control_selected,
                               e.key.keysym.scancode == SDL_SCANCODE_UNKNOWN
                                   ? SDL_GetScancodeFromKey(key)
                                   : e.key.keysym.scancode))
          capture_key = false;
        continue;
      }
      if (key == SDLK_F11) {
        fullscreen = !fullscreen;
        SDL_SetWindowFullscreen(window.get(),
                                fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        continue;
      }
      if (key == SDLK_ESCAPE || key == SDLK_p) {
        if (controls)
          controls = false;
        else if (menu && page) {
          page = selected = 0;
        } else {
          menu = !menu;
          page = selected = 0;
        }
        continue;
      }
      if (!menu &&
          (key == SDLK_EQUALS || key == SDLK_PLUS || key == SDLK_KP_PLUS))
        target_zoom = std::min(2., target_zoom + .125);
      if (!menu && (key == SDLK_0 || key == SDLK_KP_0))
        target_zoom = 1;
      if (!menu && (key == SDLK_MINUS || key == SDLK_KP_MINUS))
        target_zoom = std::max(1. / 3, target_zoom - .125);
      if (intro && key == SDLK_RETURN) {
        if (intro < 3)
          ++intro;
        else {
          intro = 0;
          playing = 0;
          music_for();
        }
        intro_elapsed = 0;
        continue;
      }
      if (controls) {
        if (key == SDLK_UP)
          control_selected = (control_selected + 7) % 8;
        if (key == SDLK_DOWN)
          control_selected = (control_selected + 1) % 8;
        if (key == SDLK_RETURN && control_selected < 6)
          capture_key = true;
        if (control_selected >= 6 &&
            (key == SDLK_RETURN || key == SDLK_LEFT || key == SDLK_RIGHT)) {
          const unsigned buttons[] = {0, 1, 2, 3, 7, 8, 10};
          auto &b = control_selected == 6 ? action_button : jump_button;
          unsigned other = control_selected == 6 ? jump_button : action_button;
          unsigned i = std::find(std::begin(buttons), std::end(buttons), b) -
                       std::begin(buttons);
          int dir = key == SDLK_LEFT ? -1 : 1;
          do {
            i = (i + 7 + dir) % 7;
          } while (buttons[i] == other);
          b = buttons[i];
        }
        continue;
      }
      if (menu) {
        unsigned count = page == 0 ? 6 : page == 1 ? 11 : 8;
        if (key == SDLK_UP)
          selected = (selected + count - (page >= 2 ? 2 : 1)) % count;
        if (key == SDLK_DOWN)
          selected = (selected + (page >= 2 ? 2 : 1)) % count;
        if (page >= 2 && (key == SDLK_LEFT || key == SDLK_RIGHT))
          selected = (selected + (key == SDLK_LEFT ? 7 : 1)) % 8;
        bool activate = key == SDLK_RETURN ||
                        (page == 1 && (key == SDLK_LEFT || key == SDLK_RIGHT));
        int dir = key == SDLK_LEFT ? -1 : 1;
        if (activate && page == 0) {
          switch (selected) {
          case 0:
            menu = false;
            break;
          case 1:
            page = 1;
            selected = 0;
            break;
          case 2:
          case 3:
            page = selected == 2 ? 2 : 3;
            selected = 0;
            refresh();
            break;
          case 4:
            controls = !controls;
            break;
          case 5:
            running = false;
            break;
          }
        } else if (activate && page == 1) {
          switch (selected) {
          case 0:
            smooth = !smooth;
            break;
          case 1:
            classic_hud = !classic_hud;
            break;
          case 2:
            invulnerable = !invulnerable;
            break;
          case 3:
            infinite = !infinite;
            break;
          case 4:
            initial_lives = std::clamp(game.state.player.lives + dir, 0, 99);
            game.state.player.lives = initial_lives;
            break;
          case 5: {
            const unsigned order[] = {1, 2,  3,  4,  5,  6,  7, 8,
                                      9, 10, 11, 12, 13, 14, 15};
            auto it = std::find(std::begin(order), std::end(order),
                                game.state.current_level);
            unsigned i = it - std::begin(order);
            select_level(order[(i + 15 + dir) % 15]);
            break;
          }
          case 6:
            music = std::clamp(int(music) + dir * 10, 0, 100);
            break;
          case 7:
            effects = std::clamp(int(effects) + dir * 10, 0, 100);
            break;
          case 8:
            render_hz = render_hz == 30   ? (dir > 0 ? 60 : 120)
                        : render_hz == 60 ? (dir > 0 ? 120 : 30)
                                          : (dir > 0 ? 30 : 60);
            history.clear();
            accumulator = 0;
            break;
          case 9:
            select_level(game.state.current_level);
            menu = false;
            break;
          case 10:
            page = selected = 0;
            break;
          }
        } else if (activate && page >= 2) {
          try {
            if (page == 2) {
              auto doc = save_game(game, fingerprint);
              View v;
              v.width = 320;
              v.height = 200;
              game.bind();
              v.x = fox_dos_camera_x();
              v.y = fox_dos_camera_y();
              auto preview = render(assets, game, v);
              auto path =
                  work / ("slot-" + std::to_string(selected + 1) + ".bmp");
              bmp(path, preview);
              doc["preview_sha256"] = titus::file_sha256(path);
              titus::write_document(slot_path(selected), doc);
              message = "PARTIDA GUARDADA";
              refresh();
            } else {
              game = restore_game(
                  assets,
                  titus::parse_json(titus::read_document(slot_path(selected),
                                                         2 * 1024 * 1024),
                                    2 * 1024 * 1024),
                  fingerprint);
              reset_view();
              playing = 0;
              music_for();
              menu = false;
              intro = 0;
            }
          } catch (const std::exception &) {
            message = "NO SE PUDO COMPLETAR";
          }
        }
      } else if (game.level_complete && key == SDLK_RETURN) {
        game.advance(assets);
        reset_view();
        playing = 0;
        music_for();
      } else if ((game.state.game_over || game.ending) && key == SDLK_RETURN) {
        game.start(assets, 1, initial_lives);
        reset_view();
        playing = 0;
        music_for();
      }
    }
    auto button = [&](unsigned b) {
      return controller && SDL_GameControllerGetButton(
                               controller.get(), SDL_GameControllerButton(b));
    };
    auto axis = [&](SDL_GameControllerAxis a) {
      return controller ? SDL_GameControllerGetAxis(controller.get(), a) : 0;
    };
    const Uint8 *keys = SDL_GetKeyboardState(nullptr);
    rewinding = !menu && !intro &&
                (keys[keyboard.codes[5]] || test_rewind ||
                 button(SDL_CONTROLLER_BUTTON_BACK)) &&
                !history.empty();
    if (was_rewinding && !rewinding)
      audio.clear_effects();
    was_rewinding = rewinding;
    audio.volume(music, effects);
    audio.muted = menu || rewinding;
    if (intro && !menu) {
      intro_elapsed += elapsed;
      if (intro < 3 && intro_elapsed >= 2) {
        ++intro;
        intro_elapsed = 0;
      }
    }
    accumulator += elapsed;
    const double tick_seconds = .029;
    while (accumulator >= tick_seconds && !menu && !intro &&
           (!game.level_complete || rewinding)) {
      accumulator -= tick_seconds;
      if (rewinding) {
        game = std::move(history.back());
        history.pop_back();
        ++rewound;
        if (history.empty())
          break;
      } else {
        history.push_back(game);
        if (history.size() > unsigned(std::lround(10 / tick_seconds)))
          history.pop_front();
        Input in;
        in.left = keys[keyboard.codes[0]] ||
                  button(SDL_CONTROLLER_BUTTON_DPAD_LEFT) ||
                  axis(SDL_CONTROLLER_AXIS_LEFTX) < -12000;
        in.right = keys[keyboard.codes[1]] ||
                   button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) ||
                   axis(SDL_CONTROLLER_AXIS_LEFTX) > 12000;
        in.up = keys[keyboard.codes[2]] || button(jump_button) ||
                button(SDL_CONTROLLER_BUTTON_DPAD_UP) ||
                axis(SDL_CONTROLLER_AXIS_LEFTY) < -12000;
        in.down = keys[keyboard.codes[3]] ||
                  button(SDL_CONTROLLER_BUTTON_DPAD_DOWN) ||
                  axis(SDL_CONTROLLER_AXIS_LEFTY) > 12000;
        in.attack = keys[keyboard.codes[4]] || button(action_button);
        if (test_ms) {
          if (probe_ticks) {
            in.right = game.ticks < 140;
            in.left = game.ticks >= 140;
            in.up = game.ticks % 60 < 15;
            in.attack = game.ticks % 20 < 5;
          } else {
            in.right = ms < 1800;
            in.left = ms >= 1800 && ms < 3200;
            in.up = ms >= 3200 && ms < 4800;
            in.attack = ms % 1000 < 300;
          }
        }
        int x = game.state.player.x, y = game.state.player.y,
            oldlevel = game.state.current_level, screen = 0;
        game.tick(assets, in, probe_ticks ? true : invulnerable,
                  probe_ticks ? true : infinite);
        int dx = game.state.player.x - x, dy = game.state.player.y - y;
        if (std::abs(dx) > 80 || std::abs(dy) > 80) {
          camera = fox_dos_camera_x();
          camera_y = fox_dos_camera_y();
          game.previous = poses();
        }
        if (in.right && dx > 0 && dx < 32)
          moves_right += dx;
        if (in.left && dx < 0 && dx > -32)
          moves_left -= dx;
        if (in.up && dy < 0 && dy > -32)
          jump_up -= dy;
        if (oldlevel != int(game.state.current_level)) {
          history.clear();
          camera = fox_dos_camera_x();
          camera_y = fox_dos_camera_y();
        }
      }
      music_for();
      if (probe_ticks && game.ticks >= probe_ticks) {
        running = false;
        break;
      }
    }
    if (menu) {
      pause_seen = true;
      accumulator = 0;
    }
    if (intro)
      accumulator = 0;
    zoom += (target_zoom - zoom) * (1 - std::exp(-elapsed * 14));
    double alpha = menu ? 1 : std::clamp(accumulator / .029, 0., 1.);
    game.bind();
    auto *w = fox_dos_world();
    if (w->levelid == 16)
      zoom = 1;
    double width = 320 / zoom, height = 200 / zoom;
    auto clamp_axis = [](double value, double world, double field) {
      if (world <= field)
        return (world - 320) / 2;
      return std::clamp(value, (field - 320) / 2, world - (field + 320) / 2);
    };
    double desired = smooth && !fox_dos_camera_locked()
                         ? w->player.sprite.x - 160
                         : fox_dos_camera_x();
    double desired_y = smooth && !fox_dos_camera_locked()
                           ? w->player.sprite.y - 100
                           : fox_dos_camera_y();
    desired = clamp_axis(desired, w->width * 16, width);
    // Vertical framing has the same 320-independent centre convention as
    // render().
    auto clamp_y = [&](double value) {
      if (w->height * 16 <= height)
        return (w->height * 16 - 200) / 2.;
      return std::clamp(value, (height - 200) / 2,
                        w->height * 16 - (height + 200) / 2);
    };
    desired_y = clamp_y(desired_y);
    double follow = 1 - std::exp(-elapsed * 12);
    camera = smooth ? camera + (desired - camera) * follow : desired;
    camera_y = smooth ? camera_y + (desired_y - camera_y) * follow : desired_y;
    camera = clamp_axis(camera, w->width * 16, width);
    camera_y = clamp_y(camera_y);
    int rw, rh;
    SDL_GetRendererOutputSize(renderer.get(), &rw, &rh);
    unsigned resolution = std::clamp(std::min(rw / 320, rh / 200), 1, 4);
    View v;
    v.width = 320 * resolution;
    v.height = 200 * resolution;
    v.x = camera;
    v.y = camera_y;
    v.zoom = zoom;
    v.alpha = smooth ? alpha : 1;
    v.classic_hud = classic_hud;
    Frame frame = render(assets, game, v);
    auto ui = [&](int x, int y, const std::string &s,
                  std::uint32_t c = 0xffd0d7e0) {
      text(frame, assets, x * resolution, y * resolution, s, c, resolution);
    };
    if (rewinding) {
      auto copy = frame.pixels;
      for (unsigned y = 0; y < frame.height; ++y) {
        int shift =
            ((y / resolution + frames * 7) % 43) < 3 ? int(resolution * 4) : 0;
        for (unsigned x = 0; x < frame.width; ++x) {
          auto px =
              copy[std::size_t(y) * frame.width + (x + shift) % frame.width];
          if (y % (resolution * 3) == 0)
            px = ((px & 0xfefefe) >> 1) | 0xff000000;
          if (shift)
            px = ((px & 0xfefefe) >> 1) | 0xff606060;
          frame.pixels[std::size_t(y) * frame.width + x] = px;
        }
      }
      ui(12, 28, "<< REBOBINANDO");
    }
    if (intro) {
      frame = picture(intro - 1, resolution);
      if (intro == 3)
        ui(72, 184, "ENTER PARA JUGAR", 0xffffdf63);
    }
    if (menu) {
      rectangle(frame, 16 * resolution, 12 * resolution, 288 * resolution,
                174 * resolution, 0xff101828);
      rectangle(frame, 16 * resolution, 12 * resolution, 288 * resolution,
                resolution, 0xffd4b970);
      ui(40, 18, "TITUS THE FOX DOS", 0xffffdf63);
      std::vector<std::string> rows =
          page == 0
              ? std::vector<std::string>{"CONTINUAR",
                                         "OPCIONES",
                                         "GUARDAR PARTIDA",
                                         "CARGAR PARTIDA",
                                         "CONTROLES",
                                         "VOLVER A BIBLIOTECA"}
              : std::vector<std::string>{
                    std::string("CAMARA ") + (smooth ? "SUAVE" : "ORIGINAL"),
                    std::string("HUD ") + (classic_hud ? "ORIGINAL" : "FIJO"),
                    std::string("INVULNERABLE ") + (invulnerable ? "SI" : "NO"),
                    std::string("VIDAS INFINITAS ") + (infinite ? "SI" : "NO"),
                    "VIDAS " + std::to_string(game.state.player.lives),
                    "NIVEL " +
                        std::to_string(display_level(game.state.current_level)),
                    "MUSICA " + std::to_string(music),
                    "EFECTOS " + std::to_string(effects),
                    "PRESENTACION " + std::to_string(render_hz) + " FPS",
                    "REINICIAR NIVEL",
                    "VOLVER"};
      if (page < 2)
        for (unsigned i = 0; i < rows.size(); ++i)
          ui(32, (page == 1 ? 33 : 38) + i * (page == 1 ? 11 : 19), rows[i],
             selected == i ? 0xffffdf63 : 0xffd0d7e0);
      else
        for (unsigned i = 0; i < 8; ++i) {
          unsigned left = 24 + i % 2 * 140, top = 34 + i / 2 * 35;
          rectangle(frame, left * resolution, top * resolution,
                    136 * resolution, 33 * resolution,
                    i == selected ? 0xffd4b970 : 0xff344155);
          rectangle(frame, (left + 1) * resolution, (top + 1) * resolution,
                    134 * resolution, 31 * resolution, 0xff101828);
          const auto &preview = previews[i];
          if (!preview.pixels.empty())
            for (unsigned y = 0; y < 29 * resolution; ++y)
              for (unsigned x = 0; x < 52 * resolution; ++x)
                frame.pixels[std::size_t((top + 2) * resolution + y) *
                                 frame.width +
                             (left + 2) * resolution + x] =
                    preview
                        .pixels[std::size_t(y * 200 / (29 * resolution)) * 320 +
                                x * 320 / (52 * resolution)];
          ui(left + 60, top + 3, std::to_string(i + 1));
          ui(left + 60, top + 17,
             slot_levels[i] == 99 ? "ERROR"
             : slot_levels[i]     ? "N " + std::to_string(slot_levels[i])
                                  : "VACIO");
        }
      if (page >= 2)
        ui(24, 160, "+/- ZOOM  0 RESET  F11 PANTALLA", 0xffd0d7e0);
      ui(24, 178, message.empty() ? "ENTER ELEGIR   ESC VOLVER" : message,
         0xffffdf63);
      if (controls) {
        rectangle(frame, 0, 0, frame.width, frame.height, 0xff101828);
        ui(24, 12, "CONTROLES", 0xffffdf63);
        for (unsigned i = 0; i < 8; ++i) {
          auto label = i < 6 ? keyboard.label(i)
                       : i == 6
                           ? "MANDO ACCION " + std::to_string(action_button)
                           : "MANDO SALTO " + std::to_string(jump_button);
          ui(24, 34 + i * 16, label,
             i == control_selected ? 0xffffdf63 : 0xffd0d7e0);
        }
        ui(24, 160, "+/- ZOOM  0 RESET  F11 PANTALLA", 0xffd0d7e0);
        ui(24, 178,
           capture_key ? "PULSA UNA TECLA - ESC CANCELA"
                       : "ENTER CAMBIAR - ESC VOLVER",
           0xffffdf63);
      }
    } else if (game.level_complete) {
      ui(64, 60, "NIVEL COMPLETADO", 0xffffdf63);
      ui(64, 84, "BONUS " + std::to_string(game.state.score));
      ui(64, 108, "VIDAS " + std::to_string(game.state.player.lives));
      ui(64, 140, "ENTER PARA CONTINUAR", 0xffffdf63);
    } else if (game.state.game_over) {
      ui(112, 82, "GAME OVER", 0xffffdf63);
      ui(68, 112, "ENTER PARA REINICIAR");
    } else if (game.ending)
      ui(80, 184, "ENTER PARA REINICIAR", 0xffffdf63);
    int tw = 0, th = 0;
    if (texture)
      SDL_QueryTexture(texture.get(), nullptr, nullptr, &tw, &th);
    if (tw != int(frame.width) || th != int(frame.height)) {
      texture.reset(SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_STREAMING, frame.width,
                                      frame.height));
      if (!texture)
        throw std::runtime_error(SDL_GetError());
    }
    SDL_UpdateTexture(texture.get(), nullptr, frame.pixels.data(),
                      frame.width * 4);
    SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255);
    SDL_RenderClear(renderer.get());
    double size = std::min(rw / 320., rh / 200.);
    SDL_Rect dst{int((rw - 320 * size) / 2), int((rh - 200 * size) / 2),
                 int(320 * size), int(200 * size)};
    guard.release();
    SDL_RenderCopy(renderer.get(), texture.get(), nullptr, &dst);
    SDL_RenderPresent(renderer.get());
    last_frame = std::move(frame);
    ++frames;
    auto spent =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - now)
            .count();
    if (spent < 1. / render_hz)
      SDL_Delay(unsigned((1. / render_hz - spent) * 1000));
  }
  titus::Json native_preferences = titus::Json::object();
  if (titus::path_exists(preferences))
    native_preferences =
        titus::parse_json(titus::read_document(preferences, 32768));
  titus::Json owned_preferences = {
      {"schema_version", 1},
      {"keyboard", keyboard.json()},
      {"camera", smooth ? "smooth" : "classic"},
      {"hud", classic_hud ? "classic" : "fixed"},
      {"invulnerable", invulnerable},
      {"infinite_lives", infinite},
      {"render_hz", render_hz},
      {"lives", initial_lives},
      {"zoom", target_zoom},
      {"view_step", unsigned(std::clamp(
                        int(std::lround((1. / target_zoom - 1.) * 4)), 0, 8))},
      {"start_level", level},
      {"fullscreen", fullscreen},
      {"music_volume", music},
      {"effects_volume", effects},
      {"action_button", action_button},
      {"jump_button", jump_button}};
  for (auto &entry : owned_preferences.items())
    native_preferences[entry.key()] = entry.value();
  titus::write_document(preferences, native_preferences);
  if (!capture.empty() && !last_frame.pixels.empty())
    bmp(capture, last_frame);
  if (managed && test_ms)
    managed->observe({{"frames", frames},
                      {"logic_hz", 1000.0 / 29.0},
                      {"ticks", game.ticks},
                      {"level", game.state.current_level},
                      {"right", moves_right},
                      {"left", moves_left},
                      {"jump", jump_up},
                      {"pause", pause_seen},
                      {"rewind_steps", rewound},
                      {"samples", audio.samples.load()},
                      {"nonzero", audio.nonzero.load()},
                      {"clipped", audio.clipped.load()},
                      {"peak", audio.peak.load()}});
  if (managed)
    managed->finish();
  std::cout << titus::Json(
                   {{"state_sha256",
                     titus::sha256(save_game(game, fingerprint).dump())},
                    {"render_hz", render_hz},
                    {"frames", frames},
                    {"logic_hz", 1000.0 / 29.0},
                    {"ticks", game.ticks},
                    {"level", game.state.current_level},
                    {"right", moves_right},
                    {"left", moves_left},
                    {"jump", jump_up},
                    {"pause", pause_seen},
                    {"rewind_steps", rewound},
                    {"samples", audio.samples.load()},
                    {"nonzero", audio.nonzero.load()},
                    {"clipped", audio.clipped.load()},
                    {"peak", audio.peak.load()}})
                   .dump()
            << '\n';
  return 0;
}
} // namespace
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) try {
  std::vector<std::string> args;
  for (int i = 0; i < argc; ++i)
    args.push_back(fs::path(argv[i]).u8string());
  return run(args);
} catch (const std::exception &e) {
  if (managed)
    try {
      managed->fail(e);
    } catch (...) {
    }
  std::cerr << e.what() << '\n';
  return 1;
}
#else
int main(int argc, char **argv) try {
  return run(std::vector<std::string>(argv, argv + argc));
} catch (const std::exception &e) {
  if (managed)
    try {
      managed->fail(e);
    } catch (...) {
    }
  std::cerr << e.what() << '\n';
  return 1;
}
#endif
