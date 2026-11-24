// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "titus/fox_amiga_audio.hpp"
#include "titus/pre1_amiga.hpp"
namespace titus::pre1_amiga {
fox_amiga::PatSong
song(const Bytes &,
     const std::function<Bytes(const std::string &)> &read_sample);
}
