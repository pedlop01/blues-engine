// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "titus/fox_amiga_rules.hpp"
namespace titus::fox_amiga {
struct PlatformBody {
    ObjectBody body;
    unsigned direction=0,speed=0,source_range=0,range=0,counter=0,source_direction=0;
    Point origin;
};
std::vector<PlatformBody> initialize_platforms(const Level&);
void step_platforms(std::vector<PlatformBody>&,Body&,Point camera,const NativeRules&,const std::optional<Viewport>& view={});
}
