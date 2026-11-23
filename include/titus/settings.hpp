#pragma once
#include "titus/process.hpp"
namespace titus {
struct Setting {std::string group,key,label,feature;Json values,initial;};
std::vector<Setting> game_settings(const std::string& game);
Json filter_settings(const Json& values,const std::string& game);
void validate_settings_values(const Json& values,const std::string& game);
Json native_settings(const Json& request);
// Native preferences keep engine-specific keys. Only temporary overrides are restored.
void restore_native_settings(const Json& request,const Json& values);
class SettingsStore {
    Paths paths_;
public:
    explicit SettingsStore(Paths paths):paths_(std::move(paths)){}
    Json load() const;
    Json scope(const Json& doc,const Json& identity,const std::string& scope) const;
    Json resolve(const Json& doc,const Json& identity,const Json& session=Json::object()) const;
    Json preview(const Json& doc,const Json& identity,const std::string& scope,const Json& values) const;
    Json edit(const Json& doc,const Json& identity,const std::string& scope,const Json& values);
    void recover(const Json& request);
    void begin(const Json& request);
    void seed(const Json& identity,const Json& native);
    void collect(const Json& request,const Json& overrides);
};
}
