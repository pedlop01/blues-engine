#pragma once
#include "titus/session.hpp"
namespace titus {
std::string decode_pre1_executable(const std::string& packed);
fs::path pre1_prepared_directory(const Json& request);
bool pre1_prepared_valid(const Json& request);
void prepare_pre1(const Json& request,bool inject_failure=false);
}
