// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <vector>
#include <array>
namespace titus::fox_amiga {
using Bytes = std::vector<std::uint8_t>;
// Research asset reader only. Does not expose a playable engine capability.
Bytes unpack(const Bytes& source);
struct Image { unsigned width, height; Bytes pixels; };
std::vector<Image> sprites(const Bytes& raw);
std::vector<Image> tiles(const Bytes& raw);
struct Picture { Image image; std::array<std::uint32_t, 64> palette{}; };
Picture picture(const Bytes& raw, unsigned planes, bool repeat_encoded);
Image map_view(const Bytes& map, const std::vector<Image>& tiles,
               unsigned x, unsigned y, unsigned width, unsigned height);
}
