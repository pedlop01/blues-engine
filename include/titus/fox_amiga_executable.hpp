// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "titus/fox_amiga.hpp"
namespace titus::fox_amiga {
// Bounded data unpacking only. No instruction from the game is executed.
// Virtual relocation bases match the independent reference audit.
struct ExecutableSections { std::array<Bytes,4> sections; };
ExecutableSections executable_sections(const Bytes& hunk_file);
}
