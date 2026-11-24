// SPDX-License-Identifier: GPL-3.0-or-later
#include "managed.hpp"
#include <SDL.h>
#include <iomanip>
#include <sstream>
namespace titus::pre1_amiga {
ManagedHost::ManagedHost(const std::vector<std::string> &args) {
  if (args.size() != 3 ||
      (args[1] != "--inspect-request" && args[1] != "--session-request"))
    throw Error("E_ARGUMENT", "Use an inspection or session request");
  auto file = normalized_path(fs::u8path(args[2]));
  request_ = parse_json(read_document(file, 128 * 1024));
  if (request_.at("game_id") != "prehistorik" ||
      request_.at("edition_id") != "amiga" ||
      request_.at("engine_id") != "pre1-amiga-native")
    throw Error(
        "E_GAME",
        "Native Prehistorik runtime requires the audited Amiga edition");
  if (request_.at("kind") !=
      (args[1] == "--inspect-request" ? "inspect_request" : "launch"))
    throw Error("E_REQUEST", "Wrong managed request kind");
  validate_engine_request(request_, file);
  verify_profile_lock(request_);
  directory_ = file.parent_path();
  work_ = fs::u8path(
      request_["paths"]["work_directory"]["path"].get<std::string>());
  root_ =
      fs::u8path(request_["sources"][0]["location"]["path"].get<std::string>());
  ensure_directory(work_);
  if (!inspection()) {
    events_.open(filesystem_path(directory_ / "events.jsonl"),
                 std::ios::binary | std::ios::app);
    emit("hello", {{"engine_id", "pre1-amiga-native"},
                   {"build_revision", TITUS_PRE1_AMIGA_BUILD_REV},
                   {"protocol", 1}});
    emit("progress", {{"phase", "initialize"}, {"percent", 30}});
  }
}
void ManagedHost::emit(const char *name, const Json &body) {
  events_ << Json({{"schema_version", 1},
                   {"kind", "event"},
                   {"session_id", request_["session_id"]},
                   {"seq", ++sequence_},
                   {"event", name},
                   {"body", body}})
                 .dump()
          << '\n';
  events_.flush();
  if (!events_)
    throw Error("E_PROTOCOL", "Cannot write native Amiga event");
}
std::vector<std::string> ManagedHost::arguments() const {
  std::vector<std::string> args = {"titus-pre1-amiga", "--datapath",
                                   root_.u8string(), "--workdir",
                                   work_.u8string()};
  auto test = environment_value("TITUS_TEST_RUNTIME_MS").value_or("0");
  if (test != "0") {
    args.insert(args.end(), {"--test-ms", test, "--capture",
                             (directory_ / "gameplay.bmp").u8string()});
    auto script = environment_value("TITUS_TEST_SCRIPT");
    if (script && !script->empty())
      args.insert(args.end(), {"--test-script", *script});
  }
  return args;
}
void ManagedHost::inspect() {
  Assets assets(root_);
  const auto original = assets.files.read("main");
  std::uint32_t crc = 0xffffffff;
  for (auto byte : original) {
    crc ^= byte;
    for (unsigned n = 0; n < 8; ++n)
      crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
  }
  std::ostringstream crc_text;
  crc_text << std::hex << std::setw(8) << std::setfill('0')
           << (crc ^ 0xffffffff);
  Json slots = Json::array(), details = Json::array();
  for (unsigned slot = 1; slot <= 8; ++slot) {
    auto file = work_ / ("slot-" + std::to_string(slot) + ".json");
    if (!path_exists(file))
      continue;
    Json entry = {{"slot", slot},
                  {"file", file.filename().u8string()},
                  {"format_id", "prehistorik.amiga.json"},
                  {"format_version", nullptr},
                  {"edition_id", nullptr},
                  {"resource_fingerprint", nullptr},
                  {"native_resource_crc32", nullptr},
                  {"payload_sha256", nullptr},
                  {"has_preview", false},
                  {"status", "damaged"}};
    try {
      const auto raw = read_document(file, 2 * 1024 * 1024);
      auto doc = parse_json(raw, 2 * 1024 * 1024);
      if (!doc.at("fingerprint").is_string() ||
          doc.at("fingerprint").get<std::string>().size() != 64)
        throw Error("E_SAVE_INCOMPATIBLE", "Invalid save fingerprint");
      entry["format_version"] = 1;
      entry["edition_id"] = "amiga";
      entry["resource_fingerprint"] = doc.at("fingerprint");
      entry["native_resource_crc32"] = crc_text.str();
      entry["payload_sha256"] = sha256(raw);
      if (doc.at("fingerprint") != fingerprint())
        entry["status"] = "incompatible";
      else {
        auto game = restore_game(assets, doc, fingerprint());
        entry["status"] = "ready";
        Json detail = {{"slot", slot},
                       {"payload_sha256", entry["payload_sha256"]},
                       {"level", display_level(game.state.current_level) - 1},
                       {"lives1", game.state.player.lives},
                       {"preview_rgb", nullptr}};
        auto preview = work_ / ("slot-" + std::to_string(slot) + ".bmp");
        if (path_exists(preview) && path_file_size(preview) < 1024 * 1024 &&
            doc.contains("preview_sha256") &&
            doc["preview_sha256"] == file_sha256(preview)) {
          std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surface(
              SDL_LoadBMP(filesystem_path(preview).u8string().c_str()),
              SDL_FreeSurface);
          if (surface && surface->w == 320 && surface->h == 200) {
            std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> converted(
                SDL_ConvertSurfaceFormat(surface.get(),
                                         SDL_PIXELFORMAT_ARGB8888, 0),
                SDL_FreeSurface);
            if (converted) {
              const char *hex = "0123456789abcdef";
              std::string pixels;
              pixels.reserve(128 * 80 * 6);
              for (unsigned y = 0; y < 80; ++y)
                for (unsigned x = 0; x < 128; ++x) {
                  auto row = reinterpret_cast<const std::uint32_t *>(
                      static_cast<const Uint8 *>(converted->pixels) +
                      (y * 200 / 80) * converted->pitch);
                  auto color = row[x * 320 / 128];
                  for (int shift : {16, 8, 0}) {
                    unsigned byte = (color >> shift) & 255;
                    pixels += hex[byte >> 4];
                    pixels += hex[byte & 15];
                  }
                }
              detail["preview_rgb"] = pixels;
              entry["has_preview"] = true;
            }
          }
        }
        details.push_back(detail);
      }
    } catch (const std::exception &) {
      entry["status"] = "damaged";
      entry["resource_fingerprint"] = nullptr;
      entry["native_resource_crc32"] = nullptr;
      entry["payload_sha256"] = nullptr;
    }
    slots.push_back(entry);
  }
  Json index = {{"schema_version", 1},
                {"kind", "save_index"},
                {"profile_id", request_["profile_id"]},
                {"game_id", "prehistorik"},
                {"engine_id", "pre1-amiga-native"},
                {"save_compatibility_id", request_["save_compatibility_id"]},
                {"slot_count", 8},
                {"slots", slots}};
  write_document(directory_ / "save-details.json",
                 {{"schema_version", 1}, {"slots", details}});
  write_document(directory_ / "inspection.json",
                 {{"schema_version", 1},
                  {"kind", "inspection"},
                  {"inspection_id", request_["inspection_id"]},
                  {"profile_id", request_["profile_id"]},
                  {"game_id", "prehistorik"},
                  {"engine_id", "pre1-amiga-native"},
                  {"save_compatibility_id", request_["save_compatibility_id"]},
                  {"status", "ok"},
                  {"features", runtime_features(request_)},
                  {"save_slot_count", 8},
                  {"preparation_ids", Json::array()},
                  {"save_index", index},
                  {"error", nullptr}});
}

void ManagedHost::ready(const char *scene) {
  if (environment_value("TITUS_TEST_RUNTIME_MS").value_or("0") != "0" &&
      environment_value("TITUS_TEST_FAULT").value_or("") == "before_ready")
    throw Error("E_ENGINE_INIT", "Deliberate native Amiga load failure");
  ready_ = true;
  emit("ready", {{"features", runtime_features(request_)},
                 {"scene", scene},
                 {"save_slot_count", 8},
                 {"audio_active", true}});
}
bool ManagedHost::poll() {
  auto bytes = read_document(directory_ / "commands.jsonl");
  if (command_offset_ > bytes.size())
    throw Error("E_PROTOCOL", "Command channel truncated");
  while (true) {
    auto end = bytes.find('\n', command_offset_);
    if (end == std::string::npos) {
      if (bytes.size() - command_offset_ > 16384)
        throw Error("E_PROTOCOL", "Oversized native Amiga command");
      break;
    }
    if (end - command_offset_ > 16384)
      throw Error("E_PROTOCOL", "Oversized native Amiga command");
    auto command =
        parse_json(bytes.substr(command_offset_, end - command_offset_));
    command_offset_ = end + 1;
    if (command.size() != 5 || command["schema_version"] != 1 ||
        command["kind"] != "command" ||
        command["session_id"] != request_["session_id"] ||
        command["seq"] != ++command_sequence_ ||
        command["command"] != "return_to_library")
      throw Error("E_PROTOCOL", "Invalid native Amiga command");
    if (!returning_) {
      returning_ = true;
      emit("return_requested", {{"confirmation_required", false}});
    }
  }
  return returning_;
}
void ManagedHost::observe(const Json &values) {
  write_document(directory_ / "probe-observation.json", values);
}
void ManagedHost::finish() {
  if (finished_)
    return;
  emit("closing");
  if (environment_value("TITUS_TEST_RUNTIME_MS").value_or("0") != "0" &&
      environment_value("TITUS_TEST_FAULT").value_or("") == "missing_result") {
    finished_ = true;
    return;
  }
  write_document(directory_ / "result.json",
                 {{"schema_version", 1},
                  {"kind", "result"},
                  {"session_id", request_["session_id"]},
                  {"game_id", "prehistorik"},
                  {"engine_id", "pre1-amiga-native"},
                  {"outcome", ready_ ? "returned" : "cancelled"},
                  {"ready", ready_},
                  {"native_exit_code", 0},
                  {"error", nullptr}});
  finished_ = true;
}
void ManagedHost::fail(const std::exception &error) {
  if (inspection() || finished_ || !events_.is_open())
    return;
  auto detail = runtime_error(error, ready_);
  emit("error", detail);
  emit("closing");
  write_document(directory_ / "result.json",
                 {{"schema_version", 1},
                  {"kind", "result"},
                  {"session_id", request_["session_id"]},
                  {"game_id", "prehistorik"},
                  {"engine_id", "pre1-amiga-native"},
                  {"outcome", ready_ ? "runtime_failed" : "load_failed"},
                  {"ready", ready_},
                  {"native_exit_code", 1},
                  {"error", detail}});
  finished_ = true;
}
} // namespace titus::pre1_amiga
