#pragma once
#include "json.hpp"
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace titus {
namespace fs = std::filesystem;
using Json = nlohmann::json;
struct Error : std::runtime_error {
    std::string code;
    Error(std::string code, const std::string& message);
};
std::string sha256(const std::string& bytes);
std::string file_sha256(const fs::path& path);
std::string fingerprint(const Json& manifest);
Json parse_json(const std::string& text, std::size_t maximum_bytes=1024*1024);
std::string platform();
std::optional<std::string> environment_value(const char* name);
std::string uuid();
bool path_exists(const fs::path& path);
bool path_is_directory(const fs::path& path);
bool path_is_regular_file(const fs::path& path);
bool path_is_symlink(const fs::path& path);
bool path_is_empty(const fs::path& path);
uint64_t path_file_size(const fs::path& path);
void copy_file_exact(const fs::path& from,const fs::path& to);
void move_directory(const fs::path& from,const fs::path& to);
void remove_regular_file(const fs::path& path); // Missing is allowed; never removes a directory.
void remove_empty_directory(const fs::path& path);
void ensure_directory(const fs::path& path);
fs::path filesystem_path(const fs::path& path); // Native I/O spelling; keep normalized paths in JSON.
fs::path normalized_path(const fs::path& path);
struct Paths {
    fs::path config, data, cache;
    static Paths resolve(const fs::path& override_root = {},
                         std::function<std::string(const char*)> env = {});
    fs::path catalog() const { return config / "catalog.json"; }
    fs::path profile_directory(const std::string& profile, const std::string& game,
                               const std::string& compatibility) const;
    void protect_source(const fs::path& source) const;
};
struct Detection {
    Json installation; // v1 installation fields; id assigned on registration
    Json diagnostics;
};
class Detector {
    Json profiles_;
public:
    Detector();
    explicit Detector(Json profiles);
    const Json& profiles() const { return profiles_; }
    std::vector<Detection> scan(const fs::path& source, bool recursive = false) const;
};
void validate_catalog(const Json& catalog);
class CatalogStore {
    Paths paths_;
    Detector detector_;
    Json mutate(const std::function<bool(Json&)>& change, long long expected);
public:
    explicit CatalogStore(Paths paths, Detector detector = Detector());
    Json load() const;
    Json load_library_preferences() const;
    void save_library_preferences(const Json& preferences);
    Json init(long long expected = -1);
    Json add(const fs::path& source, bool recursive = false, long long expected = -1);
    Json relocate(const std::string& id, const fs::path& source, long long expected = -1);
    Json remove(const std::string& id, long long expected = -1);
    Json recheck(const std::string& id, long long expected = -1);
    Json recover(long long expected = -1);
};
}
