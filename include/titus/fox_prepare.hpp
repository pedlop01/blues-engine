#pragma once
#include "titus/session.hpp"
namespace titus {
std::string decode_fox_audio(const std::string& packed);
fs::path fox_prepared_directory(const Json& request);
bool fox_prepared_valid(const Json& request);
void prepare_fox(const Json& request,bool inject_failure=false);
}
