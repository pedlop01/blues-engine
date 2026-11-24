// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <vector>
namespace titus::pre1_amiga {
using Bytes=std::vector<std::uint8_t>;
// Strict resource readers only: no runtime, simulation or palette assignment.
Bytes lzss(const Bytes&);
struct Sprite {unsigned width=0,height=0;Bytes colors,opaque;};
std::vector<Sprite> sprites(const Bytes& decoded,bool monochrome=false);
struct Sample {unsigned rate=0,one_shot=0,repeat=0,cycle=0,volume=0;Bytes pcm;};
Sample sample_8svx(const Bytes&);
}
