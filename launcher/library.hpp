#pragma once
#include "titus/catalog.hpp"

namespace titus {
struct GameInfo { const char* id; const char* title; const char* initials; const char* description; unsigned color; };
extern const std::vector<GameInfo> game_info;
std::string friendly_error(const std::exception& error);
std::string source_key(const Json& source);
struct Library {
    CatalogStore store;
    Json catalog, preferences;
    std::vector<std::string> games;
    std::string error, preference_error;
    bool dirty=false, healthy=false;
    explicit Library(Paths paths, Detector detector=Detector());
    void reload();
    void reconcile();
    const GameInfo& info() const;
    Json installations() const;
    const Json* selected() const;
    const Json* source() const;
    void select_game(int delta);
    void choose_game(const std::string& game);
    void select_installation(int delta);
    void select_edition(const std::string& edition);
    void select_source(int delta);
    std::string data_status() const;
    std::string launch_reason() const;
    bool source_available() const;
    void persist();
};
}
